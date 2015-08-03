/*	$Id$ */
/*
 * Copyright (c) 2014, 2015 Kristaps Dzonsons <kristaps@bsd.lv>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/un.h>
#include <netinet/in.h>

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>

#include "kcgiregress.h"

/*
 * The FastCGI header.
 * This is duplicated elsewhere in the code, but is harmless as it isn't
 * really going to change.
 */
struct 	fcgi_hdr {
	uint8_t	 version;
	uint8_t	 type;
	uint16_t requestId;
	uint16_t contentLength;
	uint8_t	 paddingLength;
	uint8_t	 reserved;
};

/*
 * Perform a non-blocking write to our child FastCGI process.
 * Return non-zero on success, zero on failure.
 */
static int
fcgi_write(int fd, const void *buf, size_t sz)
{
	ssize_t	 ssz;
	size_t	 wsz;

	wsz = 0;
	while (sz > 0) {
		if (-1 == (ssz = write(fd, buf + wsz, sz))) {
			perror("write");
			return(0);
		}
		sz -= (size_t)ssz;
		wsz += (size_t)ssz;
	}
	return(1);
}

static int
fcgi_read(int fd, void *buf, size_t sz)
{
	ssize_t	 ssz;
	size_t	 rsz;

	rsz = 0;
	while (sz > 0) {
		if (-1 == (ssz = read(fd, buf + rsz, sz))) {
			perror("read");
			return(0);
		} else if (0 == ssz) {
			fputs("read: unexpected EOF\n", stderr);
			return(0);
		}
		sz -= (size_t)ssz;
		rsz += (size_t)ssz;
	}

	return(1);
}

/*
 * Write a entire FastCGI header to the child.
 * Note that the members need to be in network-byte order, so perform
 * your htons and so on before invoking this function.
 */
static int
fcgi_hdr_write(int fd, const struct fcgi_hdr *hdr)
{

	if ( ! fcgi_write(fd, &hdr->version, 1))
		fprintf(stderr, "%s: version\n", __func__);
	else if ( ! fcgi_write(fd, &hdr->type, 1))
		fprintf(stderr, "%s: type\n", __func__);
	else if ( ! fcgi_write(fd, &hdr->requestId, 2))
		fprintf(stderr, "%s: requestId\n", __func__);
	else if ( ! fcgi_write(fd, &hdr->contentLength, 2))
		fprintf(stderr, "%s: data length\n", __func__);
	else if ( ! fcgi_write(fd, &hdr->paddingLength, 1))
		fprintf(stderr, "%s: pad length\n", __func__);
	else if ( ! fcgi_write(fd, &hdr->reserved, 1))
		fprintf(stderr, "%s: reserved\n", __func__);
	else
		return(1);

	return(0);
}

static int
fcgi_hdr_read(int fd, struct fcgi_hdr *hdr)
{
	char		 buf[8];

	if ( ! fcgi_read(fd, buf, 8)) {
		fprintf(stderr, "%s: header\n", __func__);
		return(0);
	}

	hdr->version = buf[0];
	hdr->type = buf[1];
	hdr->requestId = ntohs(*(uint16_t *)&buf[2]);
	hdr->contentLength = ntohs(*(uint16_t *)&buf[4]);
	hdr->paddingLength = buf[6];
	hdr->reserved = buf[7];
	return(1);
}

static int
fcgi_data_write(int fd, const void *buf, size_t sz)
{
	struct fcgi_hdr	 hdr;

	hdr.version = 1;
	hdr.type = 5;
	hdr.requestId = htons(1);
	hdr.contentLength = htons(sz);
	hdr.paddingLength = 0;
	hdr.reserved = 0;

	if ( ! fcgi_hdr_write(fd, &hdr))
		fprintf(stderr, "%s: header\n", __func__);
	else if ( ! fcgi_write(fd, buf, sz))
		fprintf(stderr, "%s: data\n", __func__);
	else
		return(1);

	return(0);
}

static int
fcgi_end_read(int fd, int *status)
{
	uint32_t	 st;
	uint8_t		 pst;
	uint8_t		 res[3];

	if ( ! fcgi_read(fd, &st, 4)) {
		fprintf(stderr, "%s: status\n", __func__);
		return(0);
	} else if ( ! fcgi_read(fd, &pst, 1)) {
		fprintf(stderr, "%s: flags\n", __func__);
		return(0);
	} else if ( ! fcgi_read(fd, res, sizeof(res))) {
		fprintf(stderr, "%s: reserved\n", __func__);
		return(0);
	}

	*status = ntohl(st) == EXIT_SUCCESS ? 1 : 0;
	return(1);
}

static int
fcgi_begin_write(int fd)
{
	struct fcgi_hdr	 hdr;
	uint16_t	 role;
	uint8_t		 flags;
	uint8_t		 res[5];

	hdr.version = 1;
	hdr.type = 1;
	hdr.requestId = htons(1);
	hdr.contentLength = htons(8);
	hdr.paddingLength = 0;
	hdr.reserved = 0;
	role = htons(1);
	flags = 0;
	memset(res, 0, sizeof(res));

	if ( ! fcgi_hdr_write(fd, &hdr))
		fprintf(stderr, "%s: header\n", __func__);
	else if ( ! fcgi_write(fd, &role, 2))
		fprintf(stderr, "%s: role\n", __func__);
	else if ( ! fcgi_write(fd, &flags, 1))
		fprintf(stderr, "%s: flags\n", __func__);
	else if ( ! fcgi_write(fd, res, sizeof(res)))
		fprintf(stderr, "%s: reserved\n", __func__);
	else
		return(1);

	return(0);
}

/*
 * Set the environment variable for a CGI script.
 * This involves actually setting the environment variable.
 */
static int
dochild_params_cgi(const char *key, const char *val, void *arg)
{

	setenv(key, val, 1);
	return(1);
}

/*
 * Set the environment variable for a FastCGI script.
 * We'll need to bundle it into a FastCGI request and write that to the
 * child.
 */
static int
dochild_params_fcgi(const char *key, const char *val, void *arg)
{
	int	 	 fd;
	struct fcgi_hdr	 hdr;
	uint32_t	 lenl;
	uint8_t		 lens;
	size_t		 sz;

	fd = *(int *)arg;
	sz = strlen(key) + (strlen(key) > 127 ? 4 : 1) +
		strlen(val) + (strlen(val) > 127 ? 4 : 1);

	/* Start with the FastCGI header. */
	hdr.version = 1;
	hdr.type = 4;
	hdr.requestId = htons(1);
	hdr.contentLength = htons(sz);
	hdr.paddingLength = 0;
	hdr.reserved = 0;
	if ( ! fcgi_hdr_write(fd, &hdr)) {
		fprintf(stderr, "%s: header\n", __func__);
		return(0);
	}

	/* Key and value lengths. */
	if ((sz = strlen(key)) > 127) {
		lenl = htonl(sz);
		if ( ! fcgi_write(fd, &lenl, 4)) {
			fprintf(stderr, "%s: key length", __func__);
			return(0);
		}
	} else {
		lens = sz;
		if ( ! fcgi_write(fd, &lens, 1)) {
			fprintf(stderr, "%s: key length", __func__);
			return(0);
		}
	}
	if ((sz = strlen(val)) > 127) {
		lenl = htonl(sz);
		if ( ! fcgi_write(fd, &lenl, 4)) {
			fprintf(stderr, "%s: val length", __func__);
			return(0);
		}
	} else {
		lens = sz;
		if ( ! fcgi_write(fd, &lens, 1)) {
			fprintf(stderr, "%s: val length", __func__);
			return(0);
		}
	}

	/* Key and value data. */
	if ( ! fcgi_write(fd, key, strlen(key))) {
		fprintf(stderr, "%s: key", __func__);
		return(0);
	} else if ( ! fcgi_write(fd, val, strlen(val))) {
		fprintf(stderr, "%s: val", __func__);
		return(0);
	}

	return(1);
}

/*
 * Parse HTTP header lines from input.
 * This obviously isn't the best way of doing things, but it's simple
 * and easy to fix for the purposes of this regression suite.
 * This will continually parse lines from `f' until it reaches a blank
 * line, at which point it will return control.
 * It returns zero on failure (read failure, or possibly write failure
 * when serialising to the CGI context) or non-zero on success.
 */
static int
dochild_params(FILE *f, void *arg, size_t *length,
	int (*fp)(const char *, const char *, void *)) 
{
	int		 first;
	char		 head[BUFSIZ], buf[BUFSIZ];
	size_t		 sz;
	char		*cp, *path, *query, *key, *val;
	extern char	*__progname;

	if (NULL != length)
		*length = 0;

	if ( ! fp("SCRIPT_NAME", __progname, arg))
		return(0);

	/*
	 * Read header lines.
	 * We'll do this in the simplest possible way so that there are
	 * no surprises.
	 */
	first = 1;
	while (NULL != fgets(head, sizeof(head), f)) {
		/* Strip off the CRLF terminator. */
		if ((sz = strlen(head)) < 2) {
			fprintf(stderr, "Bad HTTP header\n");
			return(0);
		}
		if ('\n' != head[sz - 1] || '\r' != head[sz - 2]) {
			fprintf(stderr, "Unterminated HTTP header\n");
			return(0);
		}
		head[sz - 2] = '\0';

		/* Empty line: now we're at the CGI document. */
		if ('\0' == head[0])
			break;

		/* Process our header. */
		if (first) {
			/* Snarf the first GET/POST line. */
			if (0 == strncmp(head, "GET ", 4)) {
				if ( ! fp("REQUEST_METHOD", "GET", arg))
					return(0);
				path = head + 4;
			} else if (0 == strncmp(head, "POST ", 5)) {
				if ( ! fp("REQUEST_METHOD", "POST", arg))
					return(0);
				path = head + 5;
			} else {
				fprintf(stderr, "Unknown HTTP "
					"first line: %s\n", head);
				return(0);
			}

			/* Split this into the path and query. */
			cp = path;
			while ('\0' != *cp && ! isspace((int)*cp))
				cp++;
			*cp = '\0';
			if (NULL != (query = strchr(path, '?')))
				*query++ = '\0';

			first = 0;
			if ( ! fp("PATH_INFO", path, arg))
				return(0);
			if (NULL != query)
				if ( ! fp("QUERY_STRING", query, arg))
					return(0);
			continue;
		}

		/* 
		 * Split headers into key/value parts.
		 * Strip the leading spaces on the latter. 
		 * Let baddies (no value) just go by.
		 */
		key = head;
		if (NULL == (val = strchr(key, ':')))
			continue;
		*val++ = '\0';
		while ('\0' != *val && isspace((int)*val))
			val++;

		/* Recognise some attributes... */
		if (0 == strcmp(key, "Content-Length")) {
			if (NULL != length)
				*length = atoi(val);
			if ( ! fp("CONTENT_LENGTH", val, arg))
				return(0);
			continue;
		} else if (0 == strcmp(key, "Content-Type")) {
			if ( ! fp("CONTENT_TYPE", val, arg))
				return(0);
			continue;
		}

		/*
		 * Now we have "regular" attributes that we want to cast
		 * directly as HTTP attributes in the CGI environment.
		 */
		strlcpy(buf, "HTTP_", sizeof(buf));
		sz = strlcat(buf, key, sizeof(buf));
		assert(sz < sizeof(buf));
		for (cp = buf; '\0' != *cp; cp++) 
			if ('-' == *cp)
				*cp = '_';
			else if (isalpha((int)*cp))
				*cp = toupper((int)*cp);

		if ( ! fp(buf, val, arg))
			return(0);
	}

	return(1);
}

/*
 * Bind to the local port over which we'll directly accept test requests
 * from our regression suite.
 * This port is acting as an ad hoc web server.
 * Return the file descriptor on success or -1 on failure.
 */
static int
dochild_prepare(void)
{
	int	 	 s, in, opt;
	struct sockaddr_in ad, rem;
	socklen_t	 len;

	opt = 1;
	memset(&ad, 0, sizeof(struct sockaddr_in));

	/*
	 * Bind and listen to our reusable testing socket.
	 * We pretty much just choose a random port for this.
	 */
	if (-1 == (s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP))) {
		perror("socket");
		return(-1);
	} else if (-1 == setsockopt(s, SOL_SOCKET, 
		 SO_REUSEADDR, &opt, sizeof(opt))) {
		perror("setsockopt");
		return(-1);
	}

	ad.sin_family = AF_INET;
	ad.sin_port = htons(17123);
	ad.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

	if (-1 == bind(s, (struct sockaddr *)&ad, sizeof(ad))) {
		perror("bind");
		close(s);
		return(-1);
	} else if (-1 == listen(s, 1)) {
		perror("listen");
		close(s);
		return(-1);
	}

	/*
	 * Tell our parent that we've bound to our socket and are ready
	 * to receive data.
	 * They'll do a waitpid() to see when we're sleeping, then wake
	 * us up when we're already ready to go.
	 */
	kill(getpid(), SIGSTOP);

	/*
	 * Wait for a single testing connection.
	 */
	len = sizeof(struct sockaddr_in);
	if (-1 == (in = accept(s, (struct sockaddr *)&rem, &len))) {
		perror("accept");
		close(s);
		return(-1);
	} 

	close(s);
	return(in);
}

static int
dochild_fcgi(kcgi_regress_server child, void *carg)
{
	int			 in, rc, fd;
	size_t			 sz, len, pos;
	pid_t			 pid;
	struct sockaddr_un	 sun;
	struct sockaddr		*ss;
	extern char		*__progname;
	char			 sfn[22];
	char			 buf[BUFSIZ];
	FILE			*f;
	struct fcgi_hdr		 hdr;

	/* 
	 * Create a temporary file, close it, then unlink it.
	 * The child will recreate this as a socket.
	 */
	strlcpy(sfn, "/tmp/kfcgi.XXXXXXXXXX", sizeof(sfn));
	if (-1 == (fd = mkstemp(sfn))) {
		perror(sfn);
		return(EXIT_FAILURE);
	} else if (-1 == close(fd) || -1 == unlink(sfn)) {
		perror(sfn);
		return(EXIT_FAILURE);
	}

	/* Do the usual dance to set up UNIX sockets. */
	ss = (struct sockaddr *)&sun;
	memset(&sun, 0, sizeof(sun));
	sun.sun_family = AF_UNIX;
	sz = strlcpy(sun.sun_path, sfn, sizeof(sun.sun_path));
	if (sz >= sizeof(sun.sun_path)) {
		fprintf(stderr, "socket path to long\n");
		return(EXIT_FAILURE);
	}
#ifndef __linux__
	sun.sun_len = sz;
#endif

	if (-1 == (fd = socket(AF_UNIX, SOCK_STREAM, 0))) {
		perror("socket");
		return(EXIT_FAILURE);
	} else if (-1 == bind(fd, ss, sizeof(sun))) {
		perror(sfn);
		close(fd);
		return(EXIT_FAILURE);
	} else if (-1 == listen(fd, 5)) {
		perror(sfn);
		close(fd);
		return(EXIT_FAILURE);
	}

	/*
	 * Now fork the FastCGI process.
	 * We need to use a separate process because we're going to
	 * package the request as a FastCGI request and ship it into the
	 * UNIX socket.
	 */
	if (-1 == (pid = fork())) {
		perror("fork");
		unlink(sfn);
		close(fd);
		return(EXIT_FAILURE);
	} else if (0 == pid) {
		if (-1 == dup2(fd, STDIN_FILENO))
			_exit(EXIT_FAILURE);
		close(fd);
		return(child(carg));
	}

	/* 
	 * Close the socket, as we're going to connect to it.
	 * The child has a reference to the object (via its dup2), so
	 * we're not going to totally remove the file.
	 */
	close(fd);
	fd = in = -1;
	rc = EXIT_FAILURE;
	f = NULL;

	/* Get the next incoming connection and FILE-ise it. */
	if (-1 == (in = dochild_prepare())) 
		goto out;
	if (NULL == (f = fdopen(in, "r+"))) {
		perror("fdopen");
		goto out;
	} 

	/*
	 * Open a new socket to the FastCGI object and connect to it,
	 * reusing the prior socket address.
	 * Then remove the object, as nobody needs it any more.
	 */
	if (-1 == (fd = socket(AF_UNIX, SOCK_STREAM, 0))) {
		perror(sfn);
		goto out;
	} else if (-1 == connect(fd, ss, sizeof(sun))) {
		perror(sfn);
		goto out;
	} else if (-1 == unlink(sfn)) {
		perror(sfn);
		goto out;
	} 
	sfn[0] = '\0';

	/* Write the request, its parameters, and all data. */
	if ( ! fcgi_begin_write(fd))
		goto out;
	else if ( ! dochild_params(f, &fd, &len, dochild_params_fcgi))
		goto out;

	/*
	 * We might not have any data to read from the server, so
	 * remember whether a Content-Type was stipulated and only
	 * download data if it was.
	 * This is required because we'll just sit here waiting for data
	 * otherwise.
	 */
	while (len > 0) {
		sz = fread(buf, 1, len < sizeof(buf) ? 
			len : sizeof(buf), f);
		if (0 == sz)
			break;
		if ( ! fcgi_data_write(fd, buf, sz))
			goto out;
		len -= sz;
	}
	
	/* Indicate end of input. */
	if ( ! fcgi_data_write(fd, NULL, 0))
		goto out;

	/*
	 * Now we read the response, funneling it to the output.
	 * Stop reading on error or when we receive the end of data
	 * token from the FastCGI client.
	 */
	while (fcgi_hdr_read(fd, &hdr)) {
		if (3 == hdr.type) {
			/* End of message. */
			if ( ! fcgi_end_read(fd, &rc)) 
				goto out;
			break;
		} else if (6 != hdr.type) {
			fprintf(stderr, "%s: bad type: %" PRIu8 "\n", __func__, hdr.type);
			goto out;
		}

		/* Echo using a temporary buffer. */
		while (hdr.contentLength > 0) {
			sz = hdr.contentLength > BUFSIZ ? 
				BUFSIZ : hdr.contentLength;
			if (fcgi_read(fd, buf, sz)) {
				pos = 0;
				while (sz > 0) {
					len = fwrite(buf + pos, 1, sz, f);
					sz -= len;
					pos += len;
					hdr.contentLength -= len;
				}
				continue;
			} else
				fprintf(stderr, "%s: bad read\n", __func__);
			goto out;
		}
	}
out:
	/* 
	 * Begin by asking the child to exit.
	 * Then close all of our comm channels.
	 */
	if (-1 == kill(pid, SIGTERM))
		perror("kill");
	if (NULL != f)
		fclose(f);
	else if (-1 != in)
		close(in);
	if ('\0' != sfn[0])
		unlink(sfn);
	if (-1 != fd)
		close(fd);

	/*
	 * Now mandate that the child dies and reap its resources.
	 */
	if (-1 == kill(pid, SIGKILL))
		perror("kill");
	if (-1 == waitpid(pid, NULL, 0))
		perror("waitpid");
	return(rc);
}

static int
dochild_cgi(kcgi_regress_server child, void *carg)
{
	int	 in;

	if (-1 == (in = dochild_prepare())) 
		return(0);

	/*
	 * Assign the socket as our stdin and stdout.
	 * This will facilitate the CGI script in reading and write its
	 * response directly into the stream.
	 */
	if (STDIN_FILENO != dup2(in, STDIN_FILENO)) {
		perror("dup2");
		close(in);
		return(0);
	} else if (STDOUT_FILENO != dup2(in, STDOUT_FILENO)) {
		perror("dup2");
		close(in);
		close(STDOUT_FILENO);
		return(0);
	} 

	/* Now close the channel. */
	close(in);
	
	in = dochild_params(stdin, NULL, NULL,
		dochild_params_cgi) ? child(carg) : 0;

	close(STDIN_FILENO);
	close(STDOUT_FILENO);
	return(in);
}

static int
regress(int fastcgi,
	kcgi_regress_client parent, void *parg, 
	kcgi_regress_server child, void *carg)
{
	pid_t	 chld, pid;
	int	 rc, st;

	/* Create our "test framework" child. */
	if (-1 == (chld = fork())) {
		perror(NULL);
		exit(EXIT_FAILURE);
	} else if (0 == chld) {
		rc = fastcgi ?
			dochild_fcgi(child, carg) :
			dochild_cgi(child, carg);
		exit(rc ? EXIT_SUCCESS : EXIT_FAILURE);
	}

	/* Wait for it to sleep... */
	do {
		pid = waitpid(chld, &st, WUNTRACED);
	} while (pid == -1 && errno == EINTR);

	/*
	 * Once the child sleeps, we know that it has bound itself to
	 * the listening socket. 
	 * So simply wake it up and continue our work.
	 */
	if (-1 == pid) {
		perror(NULL);
		exit(EXIT_FAILURE);
	} else if ( ! WIFSTOPPED(st)) {
		fprintf(stderr, "child not sleeping\n");
		exit(EXIT_FAILURE);
	} else if (-1 == kill(chld, SIGCONT)) {
		perror(NULL);
		exit(EXIT_FAILURE);
	}

	rc = parent(parg);

	if (-1 == waitpid(pid, &st, 0)) {
		perror(NULL);
		exit(EXIT_FAILURE);
	} 

	return(rc && WIFEXITED(st) && 
		EXIT_SUCCESS == WEXITSTATUS(st));
}

int
kcgi_regress_cgi(kcgi_regress_client parent, void *parg,
	kcgi_regress_server child, void *carg)
{

	return(regress(0, parent, parg, child, carg));
}

int
kcgi_regress_fcgi(kcgi_regress_client parent, void *parg,
	kcgi_regress_server child, void *carg)
{

	return(regress(1, parent, parg, child, carg));
}
