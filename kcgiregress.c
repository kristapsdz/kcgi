/*	$Id$ */
/*
 * Copyright (c) 2014--2016, 2019 Kristaps Dzonsons <kristaps@bsd.lv>
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
#include "config.h"

#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <netinet/in.h>

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "kcgiregress.h"

#define	FCGI_HDR_SIZE		8

/*
 * The FastCGI header.
 * This is duplicated elsewhere in the code, but is harmless as it isn't
 * really going to change.
 */
struct 	fcgi_hdr {
	uint8_t	 version;
	uint8_t	 type;
#define	FCGI_HDR_END		3
#define	FCGI_HDR_HEAD		4
#define	FCGI_HDR_DATA_IN	5
#define	FCGI_HDR_DATA_OUT	6
	uint16_t requestId;
	uint16_t contentLength;
	uint8_t	 paddingLength;
	uint8_t	 reserved;
};

/*
 * Non-blocking read.
 * Returns <0 on failure (system error) or bytes read, which may be
 * zero in the event of EOF.
 */
static ssize_t
nb_read(int fd, void *buf, size_t bufsz)
{
	struct pollfd	 pfd;
	int		 rc;

	assert(buf != NULL && bufsz);

	pfd.fd = fd;
	pfd.events = POLLIN;

	if ((rc = poll(&pfd, 1, -1)) < 0) {
		perror("poll");
		return (-1);
	} else if (rc == 0) {
		fprintf(stderr, "poll: timeout!?\n");
		return (-1);
	} 
	
	return read(fd, buf, bufsz);
}

/*
 * Non-blocking write of "bufsz" bytes in "buf".
 * Return TRUE on success, FALSE on failure (system error).
 */
static int
nb_write(int fd, const void *buf, size_t bufsz)
{
	ssize_t	 	 ssz;
	size_t	 	 sz;
	struct pollfd	 pfd;
	int		 rc;

	if (buf == NULL || bufsz == 0)
		return 1;

	pfd.fd = fd;
	pfd.events = POLLOUT;

	for (sz = 0; sz < bufsz; sz += (size_t)ssz) {
		if ((rc = poll(&pfd, 1, -1)) < 0)
			perror("poll");
		else if (rc == 0)
			fprintf(stderr, "poll: timeout!?\n");
		else if ((ssz = write(fd, buf + sz, bufsz - sz)) < 0)
			perror("write");
		else if (sz > SIZE_MAX - (size_t)ssz)
			fprintf(stderr, "write: overflow: "
				"%zu, %zd", sz, ssz);
		else
			continue;
		return 0;
	}

	return 1;
}

/*
 * Blocking write of "sz" bytes in "buf".
 * Return TRUE on success, FALSE on failure (system error).
 */
static int
b_write(int fd, const void *buf, size_t sz)
{
	ssize_t	 ssz;
	size_t	 wsz = 0;

	while (sz > 0) {
		if ((ssz = write(fd, buf + wsz, sz)) == -1) {
			perror("write");
			return 0;
		}
		sz -= (size_t)ssz;
		wsz += (size_t)ssz;
	}

	return 1;
}

/*
 * Blocking read (discarding bytes).
 * Discards "sz" bytes and returns TRUE on success, FALSE on failure
 * (system error or EOF).
 */
static int
b_ignore(int fd, size_t sz)
{
	ssize_t	 ssz;
	char	 buf;

	while (sz > 0) {
		if ((ssz = read(fd, &buf, 1)) == -1) {
			perror("read");
			return 0;
		} else if (ssz == 0) {
			fputs("read: unexpected EOF\n", stderr);
			return 0;
		}
		sz--;
	}

	return 1;
}

/*
 * Blocking read.
 * Reads "sz" bytes into buf and returns TRUE on success, FALSE on
 * failure (system error or EOF).
 */
static int
b_read(int fd, void *buf, size_t sz)
{
	ssize_t	 ssz;
	size_t	 rsz = 0;

	while (sz > 0) {
		if ((ssz = read(fd, buf + rsz, sz)) == -1) {
			perror("read");
			return 0;
		} else if (ssz == 0) {
			fputs("read: unexpected EOF\n", stderr);
			return 0;
		}
		sz -= (size_t)ssz;
		rsz += (size_t)ssz;
	}

	return 1;
}

/*
 * Write a entire FastCGI header to the child.
 * Note that the members need to be in network-byte order, so perform
 * your htons and so on before invoking this function.
 */
static int
fcgi_hdr_write(int fd, const struct fcgi_hdr *hdr)
{

	if (!b_write(fd, &hdr->version, 1))
		fprintf(stderr, "%s: version\n", __func__);
	else if (!b_write(fd, &hdr->type, 1))
		fprintf(stderr, "%s: type\n", __func__);
	else if (!b_write(fd, &hdr->requestId, 2))
		fprintf(stderr, "%s: requestId\n", __func__);
	else if (!b_write(fd, &hdr->contentLength, 2))
		fprintf(stderr, "%s: data length\n", __func__);
	else if (!b_write(fd, &hdr->paddingLength, 1))
		fprintf(stderr, "%s: pad length\n", __func__);
	else if (!b_write(fd, &hdr->reserved, 1))
		fprintf(stderr, "%s: reserved\n", __func__);
	else
		return 1;

	return 0;
}

static int
fcgi_hdr_read(int fd, struct fcgi_hdr *hdr)
{
	char		 buf[FCGI_HDR_SIZE];

	if (!b_read(fd, buf, sizeof(buf))) {
		fprintf(stderr, "%s: header\n", __func__);
		return 0;
	}

	hdr->version = buf[0];
	hdr->type = buf[1];
	hdr->requestId = ntohs(*(uint16_t *)&buf[2]);
	hdr->contentLength = ntohs(*(uint16_t *)&buf[4]);
	hdr->paddingLength = buf[6];
	hdr->reserved = buf[7];

	return 1;
}

static int
fcgi_data_write(int fd, const void *buf, size_t sz)
{
	struct fcgi_hdr	 hdr;

	hdr.version = 1;
	hdr.type = FCGI_HDR_DATA_IN;
	hdr.requestId = htons(1);
	hdr.contentLength = htons(sz);
	hdr.paddingLength = 0;
	hdr.reserved = 0;

	if (!fcgi_hdr_write(fd, &hdr))
		fprintf(stderr, "%s: header\n", __func__);
	else if (!b_write(fd, buf, sz))
		fprintf(stderr, "%s: data\n", __func__);
	else
		return 1;

	return 0;
}

static int
fcgi_end_read(int fd, int *status)
{
	uint32_t	 st;
	uint8_t		 pst;
	uint8_t		 res[3];

	if (!b_read(fd, &st, 4)) {
		fprintf(stderr, "%s: status\n", __func__);
		return 0;
	} else if (!b_read(fd, &pst, 1)) {
		fprintf(stderr, "%s: flags\n", __func__);
		return 0;
	} else if (!b_read(fd, res, sizeof(res))) {
		fprintf(stderr, "%s: reserved\n", __func__);
		return 0;
	}

	/*
	 * We use EXIT_SUCCESS for our status message.
	 * See output.c for where this is set.
	 */

	*status = ntohl(st) == EXIT_SUCCESS ? 1 : 0;
	return 1;
}

static int
fcgi_begin_write(int fd)
{
	struct fcgi_hdr	 hdr;
	uint16_t	 role = htons(1);
	uint8_t		 flags = 0;
	uint8_t		 res[5];

	hdr.version = 1;
	hdr.type = 1;
	hdr.requestId = htons(1);
	hdr.contentLength = htons(8);
	hdr.paddingLength = 0;
	hdr.reserved = 0;

	memset(res, 0, sizeof(res));

	if (!fcgi_hdr_write(fd, &hdr))
		fprintf(stderr, "%s: header\n", __func__);
	else if (!b_write(fd, &role, 2))
		fprintf(stderr, "%s: role\n", __func__);
	else if (!b_write(fd, &flags, 1))
		fprintf(stderr, "%s: flags\n", __func__);
	else if (!b_write(fd, res, sizeof(res)))
		fprintf(stderr, "%s: reserved\n", __func__);
	else
		return 1;

	return 0;
}

/*
 * Set the environment variable for a CGI script.
 * This involves actually setting the environment variable.
 */
static int
dochild_params_cgi(const char *key, const char *val, void *arg)
{

	setenv(key, val, 1);
	return 1;
}

/*
 * Set the environment variable for a FastCGI script.
 * We'll need to bundle it into a FastCGI request and write that to the
 * child.
 */
static int
dochild_params_fcgi(const char *key, const char *val, void *arg)
{
	int	 	 fd = *(int *)arg;
	struct fcgi_hdr	 hdr;
	uint32_t	 lenl;
	uint8_t		 lens;
	size_t		 sz;

	sz = strlen(key) + (strlen(key) > 127 ? 4 : 1) +
		strlen(val) + (strlen(val) > 127 ? 4 : 1);

	/* Start with the FastCGI header. */

	hdr.version = 1;
	hdr.type = FCGI_HDR_HEAD;
	hdr.requestId = htons(1);
	hdr.contentLength = htons(sz);
	hdr.paddingLength = 0;
	hdr.reserved = 0;

	if (!fcgi_hdr_write(fd, &hdr)) {
		fprintf(stderr, "%s: header\n", __func__);
		return(0);
	}

	/* Key and value lengths. */

	if ((sz = strlen(key)) > 127) {
		lenl = htonl(sz);
		if (!b_write(fd, &lenl, 4)) {
			fprintf(stderr, "%s: key length", __func__);
			return 0;
		}
	} else {
		lens = sz;
		if (!b_write(fd, &lens, 1)) {
			fprintf(stderr, "%s: key length", __func__);
			return 0;
		}
	}
	if ((sz = strlen(val)) > 127) {
		lenl = htonl(sz);
		if (!b_write(fd, &lenl, 4)) {
			fprintf(stderr, "%s: val length", __func__);
			return 0;
		}
	} else {
		lens = sz;
		if (!b_write(fd, &lens, 1)) {
			fprintf(stderr, "%s: val length", __func__);
			return 0;
		}
	}

	/* Key and value data. */
	if (!b_write(fd, key, strlen(key))) {
		fprintf(stderr, "%s: key", __func__);
		return 0;
	} else if (!b_write(fd, val, strlen(val))) {
		fprintf(stderr, "%s: val", __func__);
		return 0;
	}

	return 1;
}

/*
 * Parse HTTP header lines from input.
 * This obviously isn't the best way of doing things, but it's simple
 * and easy to fix for the purposes of this regression suite.
 * This will continually parse lines from `fd' until it reaches a blank
 * line, at which point it will return control.
 * It returns zero on failure (read failure, or possibly write failure
 * when serialising to the CGI context) or non-zero on success.
 */
static int
dochild_params(int fd, void *arg, size_t *length,
	int (*fp)(const char *, const char *, void *)) 
{
	int		 first;
	char		 head[BUFSIZ], buf[BUFSIZ];
	ssize_t		 ssz;
	size_t		 sz;
	char		*cp, *path, *query, *key, *val;
	char		 c;
	extern char	*__progname;

	if (length != NULL)
		*length = 0;

	if (!fp("SCRIPT_NAME", __progname, arg))
		return 0;

	/*
	 * Read header lines without buffering and clobbering the data
	 * yet to be read on the wire.
	 * Process them as the environment input to our CGI.
	 */

	for (first = 1; ; ) {
		/*
		 * Start by reading the header itself into a
		 * fixed-length buffer, making sure to respect that the
		 * descriptor is non-blocking.
		 */

		for (sz = 0; sz < BUFSIZ; ) {
			ssz = nb_read(fd, &c, 1);
			if (ssz < 0) {
				perror("read");
				return 0;
			} else if (ssz == 0 || (head[sz++] = c) == '\n')
				break;
		}

		/* Strip CRLF. */

		if (sz < 2 || sz == BUFSIZ) {
			fprintf(stderr, "Bad HTTP header\n");
			return 0;
		} else if (head[sz - 2] != '\r') {
			fprintf(stderr, "Bad HTTP header CRLF\n");
			return 0;
		}
		head[sz - 2] = '\0';

		/* Empty line: now we're at the CGI document. */

		if (head[0] == '\0')
			break;

		/* Process our header. */

		if (first) {
			/* Snarf the first GET/POST line. */

			if (strncmp(head, "GET ", 4) == 0) {
				if (!fp("REQUEST_METHOD", "GET", arg))
					return 0;
				path = head + 4;
			} else if (strncmp(head, "POST ", 5) == 0) {
				if (!fp("REQUEST_METHOD", "POST", arg))
					return(0);
				path = head + 5;
			} else {
				fprintf(stderr, "Unknown HTTP "
					"first line: %s\n", head);
				return 0;
			}

			/* Split this into the path and query. */

			cp = path;
			while (*cp != '\0' && !isspace((unsigned char)*cp))
				cp++;
			*cp = '\0';
			if ((query = strchr(path, '?')) != NULL)
				*query++ = '\0';

			first = 0;
			if (!fp("PATH_INFO", path, arg))
				return 0;
			if (query != NULL)
				if (!fp("QUERY_STRING", query, arg))
					return 0;
			continue;
		}

		/* 
		 * Split headers into key/value parts.
		 * Strip the leading spaces on the latter. 
		 * Let baddies (no value) just go by.
		 */

		key = head;
		if ((val = strchr(key, ':')) == NULL)
			continue;
		*val++ = '\0';
		while (*val != '\0' && isspace((unsigned char)*val))
			val++;

		/* Recognise some attributes... */

		if (strcmp(key, "Content-Length") == 0) {
			if (length != NULL)
				*length = atoi(val);
			if (!fp("CONTENT_LENGTH", val, arg))
				return 0;
			continue;
		} else if (strcmp(key, "Content-Type") == 0) {
			if (!fp("CONTENT_TYPE", val, arg))
				return 0;
			continue;
		}

		/*
		 * Now we have "regular" attributes that we want to cast
		 * directly as HTTP attributes in the CGI environment.
		 */

		strlcpy(buf, "HTTP_", sizeof(buf));
		sz = strlcat(buf, key, sizeof(buf));
		assert(sz < sizeof(buf));
		for (cp = buf;  *cp != '\0'; cp++) 
			if (*cp == '-')
				*cp = '_';
			else if (isalpha((unsigned char)*cp))
				*cp = toupper((unsigned char)*cp);

		if (!fp(buf, val, arg))
			return 0;
	}

	return 1;
}

/*
 * First, read regress().
 * This is the "child" portion of that description.
 * Bind to the local port over which we'll directly accept test requests
 * from our regression suite.
 * This port is acting as an ad hoc web server.
 * Return the file descriptor on success or -1 on failure.
 */
static int
dochild_prepare(void)
{
	int	 	 s, in, opt = 1;
	struct sockaddr_in ad, rem;
	socklen_t	 len;

	memset(&ad, 0, sizeof(struct sockaddr_in));

	/*
	 * Bind and listen to our reusable testing socket.
	 * We pretty much just choose a random port for this.
	 */

	if ((s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) {
		perror("socket");
		return (-1);
	} else if (setsockopt(s, SOL_SOCKET, 
		   SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
		perror("setsockopt");
		close(s);
		return (-1);
	}

	ad.sin_family = AF_INET;
	ad.sin_port = htons(KCGI_REGRESS_PORT);
	ad.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

	if (bind(s, (struct sockaddr *)&ad, sizeof(ad)) == -1) {
		perror("bind");
		close(s);
		return (-1);
	} else if (listen(s, 1) == -1) {
		perror("listen");
		close(s);
		return (-1);
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
	 * When we accept it, immediately note as non-blocking: kcgi(3)
	 * uses polling, so it will need a non-blocking descriptor.
	 */

	len = sizeof(struct sockaddr_in);
	if ((in = accept(s, (struct sockaddr *)&rem, &len)) == -1) {
		perror("accept");
		close(s);
		return (-1);
	} else if (fcntl(in, F_SETFL, O_NONBLOCK) == -1) {
		perror("fcntl: O_NONBLOCK");
		close(s);
		close(in);
		return 0;
	}

	close(s);
	return in;
}

/*
 * Broker a FastCGI process child.
 * Return zero on failure and non-zero on success.
 */
static int
dochild_fcgi(kcgi_regress_server child, void *carg)
{
	int			 in, rc, fd;
	size_t			 sz, len;
	pid_t			 pid;
	struct sockaddr_un	 sun;
	struct sockaddr		*ss;
	ssize_t			 ssz;
	extern char		*__progname;
	char			 sfn[22];
	char			 buf[BUFSIZ];
	struct fcgi_hdr		 hdr;
	mode_t		  	 mode;

	/* 
	 * Create a temporary file, close it, then unlink it.
	 * The child will recreate this as a socket.
	 */

	strlcpy(sfn, "/tmp/kfcgi.XXXXXXXXXX", sizeof(sfn));

	/* This shuts up Coverity. */

	mode = umask(S_IXUSR | S_IRWXG | S_IRWXO);
	if ((fd = mkstemp(sfn)) == -1) {
		perror(sfn);
		return 0;
	} else if (close(fd) == -1 || unlink(sfn) == -1) {
		perror(sfn);
		return 0;
	}
	umask(mode);

	/* Do the usual dance to set up UNIX sockets. */

	ss = (struct sockaddr *)&sun;
	memset(&sun, 0, sizeof(sun));
	sun.sun_family = AF_UNIX;
	sz = strlcpy(sun.sun_path, sfn, sizeof(sun.sun_path));
	if (sz >= sizeof(sun.sun_path)) {
		fprintf(stderr, "socket path to long\n");
		return 0;
	}
#ifndef __linux__
	sun.sun_len = sz;
#endif

	if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
		perror("socket");
		return 0;
	} else if (bind(fd, ss, sizeof(sun)) == -1) {
		perror(sfn);
		close(fd);
		return 0;
	} else if (listen(fd, 5) == -1) {
		perror(sfn);
		close(fd);
		return 0;
	}

	/*
	 * Now fork the FastCGI process.
	 * We need to use a separate process because we're going to
	 * package the request as a FastCGI request and ship it into the
	 * UNIX socket.
	 */

	if ((pid = fork()) == -1) {
		perror("fork");
		unlink(sfn);
		close(fd);
		return 0;
	} else if (pid == 0) {
		if (dup2(fd, STDIN_FILENO) == -1)
			_exit(EXIT_FAILURE);
		close(fd);
		return child(carg);
	}

	/* 
	 * Close the socket, as we're going to connect to it.
	 * The child has a reference to the object (via its dup2), so
	 * we're not going to totally remove the file.
	 */

	close(fd);
	fd = in = -1;
	rc = 0;

	/* Get the next incoming connection and FILE-ise it. */

	if ((in = dochild_prepare()) == -1) 
		goto out;

	/*
	 * Open a new socket to the FastCGI object and connect to it,
	 * reusing the prior socket address.
	 * Then remove the object, as nobody needs it any more.
	 */

	if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
		perror(sfn);
		goto out;
	} else if (connect(fd, ss, sizeof(sun)) == -1) {
		perror(sfn);
		goto out;
	} else if (unlink(sfn) == -1) {
		perror(sfn);
		goto out;
	} 
	sfn[0] = '\0';

	/* Write the request, its parameters, and all data. */

	if (!fcgi_begin_write(fd))
		goto out;
	if (!dochild_params(in, &fd, &len, dochild_params_fcgi))
		goto out;

	/*
	 * We might not have any data to read from the server, so
	 * remember whether a Content-Type was stipulated and only
	 * download data if it was.
	 * This is required because we'll just sit here waiting for data
	 * otherwise.
	 * Note: "in" is non-blocking, so be respectful.
	 */

	while (len > 0) {
		ssz = nb_read(in, buf, 
			len < sizeof(buf) ? len : sizeof(buf));
		if (ssz < 0) {
			perror("read");
			goto out;
		} else if (ssz == 0)
			break;

		if (!fcgi_data_write(fd, buf, ssz)) {
			fprintf(stderr, "%s: stdout\n", __func__);
			goto out;
		}
		len -= ssz;
	}

	/* Indicate end of input. */

	if (!fcgi_data_write(fd, NULL, 0)) {
		fprintf(stderr, "%s: stdout (FIN)\n", __func__);
		goto out;
	}

	/*
	 * Now we read the response, funneling it to the output.
	 * Stop reading on error or when we receive the end of data
	 * token from the FastCGI client.
	 * Start with the return code as non-zero, because getting no
	 * FastCGI response is valid (failed connection) while getting
	 * some is an error.
	 */

	rc = 1;
	while (fcgi_hdr_read(fd, &hdr)) {
		rc = 0;
		if (hdr.type == FCGI_HDR_END) {
			/* End of message. */
			if (!fcgi_end_read(fd, &rc)) {
				fprintf(stderr, "%s: bad fin\n", __func__);
				goto out;
			}
			break;
		} else if (hdr.type != FCGI_HDR_DATA_OUT) {
			fprintf(stderr, "%s: bad type: %" 
				PRIu8 "\n", __func__, hdr.type);
			goto out;
		}

		/* Echo using a temporary buffer. */

		while (hdr.contentLength > 0) {
			sz = hdr.contentLength > BUFSIZ ? 
				BUFSIZ : hdr.contentLength;
			if (b_read(fd, buf, sz)) {
				if (!nb_write(in, buf, sz))
					goto out;
				hdr.contentLength -= sz;
				continue;
			} else
				fprintf(stderr, "%s: bad read\n", __func__);
			goto out;
		}
		if (b_ignore(fd, hdr.paddingLength) == 0) {
			fprintf(stderr, "%s: bad ignore\n", __func__);
			goto out;
		} 
	}
out:
	/* 
	 * Begin by asking the child to exit.
	 * Then close all of our comm channels.
	 */

	kill(pid, SIGTERM);
	if (in != -1)
		close(in);
	if (sfn[0] != '\0')
		unlink(sfn);
	if (fd != -1)
		close(fd);

	/*
	 * Now mandate that the child dies and reap its resources.
	 * FIXME: we might kill the process before it's done actually
	 * terminating, which is unfair and will raise spurious
	 * warnings elsewhere.
	 */

	if (waitpid(pid, NULL, 0) == -1)
		perror("waitpid");
	return rc;
}

/*
 * Broker a CGI process child.
 * Return zero on failure and non-zero on success.
 */
static int
dochild_cgi(kcgi_regress_server child, void *carg)
{
	int		 in, fd[2], rc;
	const char	*msg;
	pid_t		 pid;
	char		*vec, *end, *start, *cp, *ovec;
	size_t		 vecsz, headsz;
	void		*pp;
	char		 buf[BUFSIZ];
	ssize_t		 ssz;

	if ((in = dochild_prepare()) == -1) 
		return 0;

	/*
	 * We need to do some filtering from the CGI script's output
	 * (just its Status message), so create a socketpair which we'll
	 * use to scrub its output.
	 * This is because the CGI protocol is stupid: it would have
	 * been a lot easier to just require an HTTP status message, but
	 * I guess the intent was to make the web server worry about
	 * formatting for various versions of HTTP.
	 * Whatever.
	 */

	if (socketpair(PF_LOCAL, SOCK_STREAM, 0, fd) == -1) {
		perror("socketpair");
		close(in);
		return 0;
	} 

	/* Launch the actual CGI process. */

	if ((pid = fork()) == -1) {
		perror("fork");
		close(fd[0]);
		close(fd[1]);
		close(in);
		return 0;
	} else if (pid == 0) {
		close(fd[1]);
		/*
		 * First, we suck down the HTTP headeres into our CGI
		 * environment and run the child.
		 * Next, re-assign our stdin to be the server's file
		 * descriptor "in".
		 * Then assign our stdout to be fd[0].
		 */

		if (!dochild_params
		    (in, NULL, NULL, dochild_params_cgi)) {
			close(in);
			close(fd[0]);
			return(0);
		}

		if (dup2(in, STDIN_FILENO) != STDIN_FILENO) {
			perror("dup2");
			close(in);
			close(fd[0]);
			_exit(EXIT_FAILURE);
		} 
		close(in);

		if (dup2(fd[0], STDOUT_FILENO) != STDOUT_FILENO) {
			perror("dup2");
			close(fd[0]);
			return 0;
		} 
		close(fd[0]);

		child(carg);
		_exit(in ? EXIT_SUCCESS : EXIT_FAILURE);
	}

	/*
	 * We're in the parent.
	 * Read and buffer the output of the CGI process until we get
	 * its Status value.
	 * Do so by copying through a static buffer into a growable
	 * vector that we'll scan for the Status message to re-write.
	 * Of course, if the CGI process is ill-designed, we'll consume
	 * all of our memory and die.
	 */

	close(fd[0]);
	vecsz = 0;
	vec = end = NULL;
	rc = 0;

	while ((ssz = read(fd[1], buf, sizeof(buf))) > 0) {
		pp = realloc(vec, vecsz + ssz);
		if (pp == NULL) {
			perror("realloc");
			goto out;
		}
		vec = pp;
		memcpy(vec + vecsz, buf, ssz);
		vecsz += ssz;
		end = memmem(vec, vecsz, "\r\n\r\n", 4);
		if (end != NULL)
			break;
	}

	if (ssz < 0) {
		perror("read");
		goto out;
	}

	/*
	 * Now all our headers are in vec plus extra; of if it's NULL,
	 * we reached the end of input without getting headers.
	 * Now we start to scan for the status line or dump.
	 */

	if (end != NULL) {
		/* Look for the status field. */

		headsz = (size_t)(end - vec);
		start = memmem(vec, headsz, "Status:", 7);
		if (start == NULL) {
			/*
			 * No status field.
			 * This is Ok (according to CGI).
			 * However, we do not provide a valid status, so
			 * do what others do and 200 it.
			 */

			msg = "HTTP/1.1 200 OK\r\n";
			if (!nb_write(in, msg, strlen(msg)))
				goto out;
			fprintf(stderr, "CGI script did "
				"not specify status\n");
			ovec = vec;
		} else {
			/*
			 * We found the status.
			 * Print it out now, then everything that came
			 * before it.
			 */

			msg = "HTTP/1.1";
			if (!nb_write(in, msg, strlen(msg)))
				goto out;
			cp = start + 7;
			while (cp < end) {
				if (!nb_write(in, cp, 1))
					goto out;
				cp++;
				if (cp[-1] == '\n')
					break;
			}
			if (!nb_write(in, vec, (size_t)(start - vec)))
				goto out;
			vecsz -= (cp - vec);
			ovec = cp;
		}

		/*
		 * Print everything else in our vector array, then poll
		 * on the CGI script til its dry.
		 */

		if (!nb_write(in, ovec, vecsz))
			goto out;

		while ((ssz = read(fd[1], buf, sizeof(buf))) > 0)
			if ( ! nb_write(in, buf, ssz))
				goto out;

		if (ssz < 0) {
			perror("read");
			goto out;
		}
	} else {
		if (!nb_write(in, vec, vecsz))
			goto out;
		fprintf(stderr, "CGI script did "
			"not terminate headers\n");
	} 

	rc = 1;
out:
	if (waitpid(pid, NULL, 0) == -1)
		perror("waitpid");

	free(vec);
	close(in);
	close(fd[1]);
	return rc;
}

/*
 * This is the real beginning of the regression system.
 * Create a child process first, and wait for it to go to sleep.
 * When the child has gone to sleep, we know that it has started up and
 * bound to a socket, so we can wake it up (SIGCONT) and test against
 * the open socket.
 */
static int
regress(int fastcgi,
	kcgi_regress_client parent, void *parg, 
	kcgi_regress_server child, void *carg)
{
	pid_t	 chld, pid;
	int	 rc, st;

	/* 
	 * Create our "test framework" child.
	 * The child will return EXIT_SUCCESS or EXIT_FAILURE in the
	 * usual way.
	 */

	if ((chld = fork()) == -1) {
		perror(NULL);
		exit(EXIT_FAILURE);
	} else if (chld == 0) {
		rc = fastcgi ?
			dochild_fcgi(child, carg) :
			dochild_cgi(child, carg);
		exit(rc ? EXIT_SUCCESS : EXIT_FAILURE);
	}

	/* 
	 * Wait for it to sleep.
	 * We do this to prevent the "parent" from trying to access the
	 * web application over a socket that hasn't been opened yet.
	 */

	do {
		pid = waitpid(chld, &st, WUNTRACED);
	} while (pid == -1 && errno == EINTR);

	/*
	 * Once the child sleeps, we know that it has bound itself to
	 * the listening socket. 
	 * So simply wake it up and continue our work.
	 */

	if (pid == -1) {
		perror(NULL);
		exit(EXIT_FAILURE);
	} else if (!WIFSTOPPED(st)) {
		fprintf(stderr, "child not sleeping\n");
		exit(EXIT_FAILURE);
	} else if (kill(chld, SIGCONT) == -1) {
		perror(NULL);
		exit(EXIT_FAILURE);
	}

	rc = parent(parg);

	if (waitpid(pid, &st, 0) == -1) {
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

	return regress(0, parent, parg, child, carg);
}

int
kcgi_regress_fcgi(kcgi_regress_client parent, void *parg,
	kcgi_regress_server child, void *carg)
{

	return regress(1, parent, parg, child, carg);
}
