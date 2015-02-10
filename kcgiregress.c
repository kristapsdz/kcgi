/*	$Id$ */
/*
 * Copyright (c) 2014 Kristaps Dzonsons <kristaps@bsd.lv>
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
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/un.h>
#include <netinet/in.h>

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>

#include "kcgiregress.h"

enum	cgimode {
	MODE_CGI,
	MODE_FASTCGI
};

/* FastCGI header types. */
enum	fcgi {
	FCGI_BEGIN_REQUEST = 1,
	FCGI_END_REQUEST = 3,
	FCGI_PARAMS = 4,
	FCGI_STDIN = 5,
	FCGI_STDOUT = 6,
	FCGI_STDERR = 7
};

/* FastCGI role types. */
enum	fcgi_role {
	FCGI_RESPONDER = 1,
	FCGI_AUTHORIZER = 2,
	FCGI_FILTER = 3 
};

/* 
 * 8-byte FastCGI header.
 * Each frame to and from the application has this.
 * All fields >8 bytes are in network-byte order.
 */
struct 	fcgi_req {
	uint8_t	 ver; /* version (always 1) */
	uint8_t	 type; /* type (see enum fcgi) */
	uint16_t id; /* identifier (always 1) */
	uint16_t len; /* length of subsequent */
	uint8_t	 pad; /* zero */
	uint8_t	 reserved; /* zero */
};

/* Begin the FastCGI request. */
struct	fcgi_begin_req {
	uint16_t role;
	uint8_t	 flags;
	uint8_t	 reserved[5];
};

/* Name-value parameter. */
struct	fcgi_param_11 {
	uint8_t	 namelen;
	uint8_t	 valuelen;
};

/* Name-value parameter. */
struct	fcgi_param_14 {
	uint8_t	 namelen;
	uint32_t valuelen;
};

/* Name-value parameter. */
struct	fcgi_param_41 {
	uint32_t namelen;
	uint8_t	 valuelen;
};

/* Name-value parameter. */
struct	fcgi_param_44 {
	uint32_t namelen;
	uint32_t valuelen;
};

/* End FastCGI request. */
struct	fcgi_end_req {
	uint32_t status;
	uint8_t	 protocolStatus;
	uint8_t	 reserved[3];
};

enum	fcgi_err {
	FCGI_REQUEST_COMPLETE = 0,
	FCGI_CANT_MPX_CONN = 1,
	FCGI_OVERLOADED = 2,
	FCGI_UNKNOWN_ROLE = 3
};

#define	FCGI_KEEP_CONN  1

static int
send_buf(int fd, const unsigned char *buf, size_t bsz)
{
	ssize_t	 ssz;

	if (0 == bsz)
		return(1);

	if (-1 == (ssz = write(fd, buf, bsz))) {
		perror(NULL);
		return(0);
	} else if ((size_t)ssz != bsz) {
		fprintf(stderr, "short write\n");
		return(0);
	}

	return(1);
}

static int
send_header(int fd, enum fcgi type, uint16_t sz)
{
	struct fcgi_req		 req;
	unsigned char		*buf;
	size_t			 bsz;

	bsz = sizeof(struct fcgi_begin_req);
	buf = (unsigned char *)&req;
	memset(&req, 0, bsz);

	req.ver = 1;
	req.type = (unsigned int)type;
	req.id = htons(1);
	req.len = htons(sz);
	
	return(send_buf(fd, buf, bsz));
}

static int
send_begin_request(int fd)
{
	struct fcgi_begin_req 	 req;
	unsigned char		*buf;
	size_t		 	 bsz;

	bsz = sizeof(struct fcgi_begin_req);
	assert(bsz < UINT16_MAX);
	buf = (unsigned char *)&req;
	memset(&req, 0, bsz);

	req.role = FCGI_RESPONDER;
	req.flags = FCGI_KEEP_CONN;

	if ( ! send_header(fd, FCGI_BEGIN_REQUEST, bsz))
		return(0);

	return(send_buf(fd, buf, bsz));
}

static int
send_params(int fd, const char *name, const char *value)
{
	struct fcgi_param_11	 p11;
	struct fcgi_param_41	 p41;
	struct fcgi_param_14	 p14;
	struct fcgi_param_44	 p44;
	unsigned char		*buf;
	size_t		 	 bsz, namelen, valuelen;

	namelen = strlen(name);
	valuelen = strlen(value);

	if (namelen > UINT8_MAX) {
		if (valuelen > UINT8_MAX) {
			p44.namelen = htonl(namelen);
			p44.valuelen = htonl(valuelen);
			buf = (unsigned char *)&p44;
			bsz = sizeof(struct fcgi_param_44);
		} else {
			p41.namelen = htonl(namelen);
			p41.valuelen = valuelen;
			buf = (unsigned char *)&p41;
			bsz = sizeof(struct fcgi_param_41);
		}
	} else {
		if (valuelen > UINT8_MAX) {
			p14.namelen = namelen;
			p14.valuelen = htonl(valuelen);
			buf = (unsigned char *)&p14;
			bsz = sizeof(struct fcgi_param_14);
		} else {
			p11.namelen = namelen;
			p11.valuelen = valuelen;
			buf = (unsigned char *)&p11;
			bsz = sizeof(struct fcgi_param_11);
		}
	}

	if ( ! send_header(fd, FCGI_PARAMS, bsz))
		return(0);
	if ( ! send_buf(fd, buf, bsz))
		return(0);
	if ( ! send_buf(fd, (const unsigned char *)name, namelen))
		return(0);
	if ( ! send_buf(fd, (const unsigned char *)value, valuelen))
		return(0);

	return(1);
}

static int
send_stdin(int fd, const char *buf, size_t bsz)
{

	if ( ! send_header(fd, FCGI_STDIN, bsz))
		return(0);
	if ( ! send_buf(fd, (const unsigned char *)buf, bsz))
		return(0);

	return(1);
}

static int
read_end_request(int fd, char *buf, size_t bsz)
{
	struct fcgi_end_req	*req;
	ssize_t		   	 ssz;

	assert(bsz > sizeof(struct fcgi_end_req));
	ssz = read(fd, buf, sizeof(struct fcgi_end_req));
	if (ssz < 0) {
		perror(NULL);
		return(-1);
	} else if (ssz != sizeof(struct fcgi_end_req)) {
		fprintf(stderr, "input size mismatch\n");
		return(-1);
	}

	req = (struct fcgi_end_req *)buf;

	if (FCGI_REQUEST_COMPLETE != req->protocolStatus) {
		fprintf(stderr, "bad protocol status\n");
		return(-1);
	} else if (EXIT_SUCCESS != ntohl(req->status)) {
		fprintf(stderr, "bad exit status\n");
		return(-1);
	}

	return(0);
}

static int
read_stdout(int fd, char *buf, size_t bsz)
{
	ssize_t	 	 ssz;
	struct fcgi_req	*req;
	enum fcgi	 type;
	size_t		 len, slen;

	assert(bsz > 8);
	ssz = read(fd, buf, sizeof(struct fcgi_req));
	if (ssz < 0) {
		perror(NULL);
		return(-1);
	} else if (ssz != 8) {
		fprintf(stderr, "input size mismatch\n");
		return(-1);
	}

	req = (struct fcgi_req *)buf;
	if (1 != req->ver) {
		fprintf(stderr, "bad fcgi version\n");
		return(-1);
	} else if (1 != ntohs(req->id)) {
		fprintf(stderr, "bad fcgi id\n");
		return(-1);
	}

	if (FCGI_END_REQUEST == (type = req->type))
		return(read_end_request(fd, buf, bsz));

	if (FCGI_STDERR != type && FCGI_STDOUT != type) {
		fprintf(stderr, "bad fcgi type\n");
		return(-1);
	}

	len = ntohs(req->len);

	while (len > 0) {
		slen = bsz < len ? bsz : len;
		if (-1 == (ssz = read(fd, buf, slen))) {
			perror(NULL);
			return(-1);
		} else if (0 == ssz) {
			fprintf(stderr, "unexpected eof\n");
			return(-1);
		}
		if (FCGI_STDERR == type)
			fwrite(buf, 1, slen, stderr);
		else
			fwrite(buf, 1, slen, stdout);
		len -= slen;
	}

	return(1);
}

static int
bindport(short port)
{
	int	 	 s, opt;
	struct sockaddr_in ad;

	opt = 1;

	/* Create our reusable socket. */
	if (-1 == (s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP))) {
		perror("socket");
		return(-1);
	} else if (-1 == setsockopt(s, SOL_SOCKET, 
		 SO_REUSEADDR, &opt, sizeof(opt))) {
		perror("setsockopt");
		close(s);
		return(-1);
	}

	/* 
	 * Bind and set our listen codes. 
	 * Note that we do not set this to be non-blocking, so the
	 * subsequent accept(2) will be blocking.
	 */
	memset(&ad, 0, sizeof(struct sockaddr_in));
	ad.sin_family = AF_INET;
	ad.sin_port = htons(port);
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

	return(s);
}

static int
bindpath(const char *path)
{
	int	 	 fd;
	size_t		 len, count;
	struct sockaddr_un sun;

	/* Wait til the path is open for writing. */
	for (count = 0; count < 10; count++) {
		if (-1 != access(path, W_OK))
			break;
		if (ENOENT != errno) {
			fprintf(stderr, "%s: access\n", path);
			return(-1);
		} else
			sleep(1);
	}

	/* Wait at most ten seconds. */
	if (10 == count) {
		fprintf(stderr, "%s: couldn't access\n", path);
		return(-1);
	}

	/* Usual unix(4) socket stuff to prepare. */
	memset(&sun, 0, sizeof(sun));
	sun.sun_family = AF_UNIX;
	len = strlcpy(sun.sun_path, path, sizeof(sun.sun_path));
	if (len >= sizeof(sun.sun_path)) {
		fprintf(stderr, "%s: too long\n", path);
		return(-1);
	}
	sun.sun_len = len;

	if (-1 == (fd = socket(AF_UNIX, SOCK_STREAM, 0))) {
		perror("socket (AF_UNIX)");
		return(-1);
	}

	/* Bind and set to non-blocking. */
	if (-1 == bind(fd, (struct sockaddr *)&sun, sizeof(sun))) {
		perror(path);
		close(fd);
		return(-1);
	} else if (-1 == fcntl(fd, F_SETFL, O_NONBLOCK)) {
		perror(path);
		close(fd);
		return(-1);
	}

	return(fd);
}

static int
dochild_fcgi(kcgi_regress_server child, void *carg)
{
	int	 	 s, fd, in, rc, opt, first, st;
	struct sockaddr_in ad, rem;
	socklen_t	 len;
	char		 head[1024], *cp, *path, *query, *key, *val;
	size_t		 sz;
	pid_t		 pid;
	extern char	*__progname;

	rc = 0;
	opt = 1;
	s = in = fd = -1;
	memset(&ad, 0, sizeof(struct sockaddr_in));

	fprintf(stderr, "%s: starting child...\n", __func__);

	/* Fork the child that will run the server test. */
	if (-1 == (pid = fork())) {
		perror("fork");
		goto out;
	} else if (0 == pid) {
		/*close(STDIN_FILENO);
		close(STDOUT_FILENO);*/
		exit(child(carg) ? EXIT_SUCCESS : EXIT_FAILURE);
	}

	fprintf(stderr, "%s: binding socket: %s\n", __func__, "/tmp/fastcgi.sock");

	/* Wait til the child has started up, binding to it. */
	if (-1 == (fd = bindpath("/tmp/fastcgi.sock"))) 
		goto out;

	fprintf(stderr, "%s: binding port: %d\n", __func__, 17123);

	/* Bind to (blocking) testing socket. */
	if (-1 == (s = bindport(17123)))
		goto out;

	fprintf(stderr, "%s: sleeping...\n", __func__);

	/*
	 * Tell our parent that we've bound to our socket and are ready
	 * to receive data.
	 * They'll do a waitpid() to see when we're sleeping, then wake
	 * us up when we're already ready to go.
	 */
	kill(getpid(), SIGSTOP);

	fprintf(stderr, "%s: awake, accepting\n", __func__);

	/* Wait for a single testing connection. */
	len = sizeof(struct sockaddr_in);
	if (-1 == (in = accept(s, (struct sockaddr *)&rem, &len))) {
		perror("accept");
		goto out;
	} 

	/* Assign the socket as our stdin and stdout. */
	if (STDIN_FILENO != dup2(in, STDIN_FILENO)) {
		perror("dup2");
		goto out;
	} else if (STDOUT_FILENO != dup2(in, STDOUT_FILENO)) {
		perror("dup2");
		goto out;
	} 

	/* Here we go... */
	send_begin_request(fd);
	send_params(fd, "SCRIPT_NAME", __progname);

	/*
	 * Read header lines.
	 * We'll do this in the simplest possible way so that there are
	 * no surprises.
	 */
	first = 1;
	while (NULL != fgets(head, sizeof(head), stdin)) {
		/* Strip off the CRLF terminator. */
		if ((sz = strlen(head)) < 2) {
			fprintf(stderr, "Bad HTTP header\n");
			goto out;
		}
		if ('\n' != head[sz - 1] || '\r' != head[sz - 2]) {
			fprintf(stderr, "Unterminated HTTP header\n");
			goto out;
		}
		head[sz - 2] = '\0';

		/* Empty line: now we're at the CGI document. */
		if ('\0' == head[0])
			break;

		/* Process our header. */
		if (first) {
			/* Snarf the first GET/POST line. */
			if (0 == strncmp(head, "GET ", 4)) {
				send_params(fd, "REQUEST_METHOD", "GET");
				path = head + 4;
			} else if (0 == strncmp(head, "POST ", 5)) {
				send_params(fd, "REQUEST_METHOD", "POST");
				path = head + 5;
			} else {
				fprintf(stderr, "Unknown HTTP "
					"first line: %s\n", head);
				goto out;
			}

			/* Split this into the path and query. */
			cp = path;
			while ('\0' != *cp && ! isspace(*cp))
				cp++;
			*cp = '\0';
			if (NULL != (query = strchr(path, '?')))
				*query++ = '\0';

			first = 0;
			send_params(fd, "PATH_INFO", path);
			if (NULL != query)
				send_params(fd, "QUERY_STRING", query);
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
		while ('\0' != *val && isspace(*val))
			val++;

		/* Recognise some attributes... */
		if (0 == strcmp(key, "Content-Length"))
			send_params(fd, "CONTENT_LENGTH", val);
		else if (0 == strcmp(key, "Content-Type"))
			send_params(fd, "CONTENT_TYPE", val);
		else if (0 == strcmp(key, "Accept"))
			send_params(fd, "HTTP_ACCEPT", val);
		else if (0 == strcmp(key, "Host"))
			send_params(fd, "HTTP_HOST", val);
		else if (0 != strcmp(key, "Expect"))
			fprintf(stderr, "Skipping header: %s\n", key);
	}

	if (NULL == head)
		goto out;

	while ((sz = fread(head, 1, sizeof(head), stdin)) > 0) {
		if (ferror(stdin)) {
			perror(NULL);
			goto out;
		} else if ( ! send_stdin(fd, head, sz))
			goto out;
	}

	if (ferror(stdin)) {
		perror(NULL);
		goto out;
	} else if ( ! feof(stdin)) {
		fprintf(stderr, "not eof!?\n");
		goto out;
	} else if ( ! send_stdin(fd, NULL, 0))
		goto out;

	rc = 1;

	do
		rc = read_stdout(fd, head, sizeof(head));
	while (1 == rc);

	if (rc < 0) {
		rc = 0;
		goto out;
	}

	rc = 1;
out:
	if (-1 != pid) {
		if (-1 == kill(pid, SIGHUP))
			perror(NULL);

		if (-1 == waitpid(pid, &st, 0)) {
			perror(NULL);
			rc = 0;
		} else if ( ! WIFEXITED(st)) {
			fprintf(stderr, "Regression application "
				"didn't exit properly\n");
			rc = 0;
		} else if (EXIT_SUCCESS != WEXITSTATUS(st)) {
			fprintf(stderr, "Regression application "
				"didn't exit with proper code\n");
			rc = 0;
		}
	}
	if (-1 != in)
		close(in);
	if (-1 != s)
		close(s);
	if (-1 != fd)
		close(fd);

	return(rc);
}

static int
dochild_cgi(kcgi_regress_server child, void *carg)
{
	int	 	 s, in, rc, opt, first;
	struct sockaddr_in ad, rem;
	socklen_t	 len;
	char		 head[1024], *cp, *path, *query, *key, *val;
	size_t		 sz;
	extern char	*__progname;

	rc = 0;
	opt = 1;
	s = in = -1;
	memset(&ad, 0, sizeof(struct sockaddr_in));

	/*
	 * Bind and listen to our reusable testing socket.
	 * We pretty much just choose a random port for this.
	 */
	if (-1 == (s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP))) {
		perror("socket");
		exit(EXIT_FAILURE);
	} else if (-1 == setsockopt(s, SOL_SOCKET, 
		 SO_REUSEADDR, &opt, sizeof(opt))) {
		perror("setsockopt");
		goto out;
	}

	ad.sin_family = AF_INET;
	ad.sin_port = htons(17123);
	ad.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

	if (-1 == bind(s, (struct sockaddr *)&ad, sizeof(ad))) {
		perror("bind");
		goto out;
	} else if (-1 == listen(s, 1)) {
		perror("listen");
		goto out;
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
		goto out;
	} 

	/*
	 * Assign the socket as our stdin and stdout.
	 * This will facilitate the CGI script in reading and write its
	 * response directly into the stream.
	 */
	if (STDIN_FILENO != dup2(in, STDIN_FILENO)) {
		perror("dup2");
		goto out;
	} else if (STDOUT_FILENO != dup2(in, STDOUT_FILENO)) {
		perror("dup2");
		goto out;
	} 

	setenv("SCRIPT_NAME", __progname, 1);
	
	/*
	 * Read header lines.
	 * We'll do this in the simplest possible way so that there are
	 * no surprises.
	 */
	first = 1;
	while (NULL != fgets(head, sizeof(head), stdin)) {
		/* Strip off the CRLF terminator. */
		if ((sz = strlen(head)) < 2) {
			fprintf(stderr, "Bad HTTP header\n");
			goto out;
		}
		if ('\n' != head[sz - 1] || '\r' != head[sz - 2]) {
			fprintf(stderr, "Unterminated HTTP header\n");
			goto out;
		}
		head[sz - 2] = '\0';

		/* Empty line: now we're at the CGI document. */
		if ('\0' == head[0])
			break;

		/* Process our header. */
		if (first) {
			/* Snarf the first GET/POST line. */
			if (0 == strncmp(head, "GET ", 4)) {
				setenv("REQUEST_METHOD", "GET", 1);
				path = head + 4;
			} else if (0 == strncmp(head, "POST ", 5)) {
				setenv("REQUEST_METHOD", "POST", 1);
				path = head + 5;
			} else {
				fprintf(stderr, "Unknown HTTP "
					"first line: %s\n", head);
				goto out;
			}

			/* Split this into the path and query. */
			cp = path;
			while ('\0' != *cp && ! isspace(*cp))
				cp++;
			*cp = '\0';
			if (NULL != (query = strchr(path, '?')))
				*query++ = '\0';

			first = 0;
			setenv("PATH_INFO", path, 1);
			if (NULL != query)
				setenv("QUERY_STRING", query, 1);
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
		while ('\0' != *val && isspace(*val))
			val++;

		/* Recognise some attributes... */
		if (0 == strcmp(key, "Content-Length"))
			setenv("CONTENT_LENGTH", val, 1);
		else if (0 == strcmp(key, "Content-Type"))
			setenv("CONTENT_TYPE", val, 1);
		else if (0 == strcmp(key, "Accept"))
			setenv("HTTP_ACCEPT", val, 1);
		else if (0 == strcmp(key, "Host"))
			setenv("HTTP_HOST", val, 1);
		else if (0 != strcmp(key, "Expect"))
			fprintf(stderr, "Skipping header: %s\n", key);
	}

	if (NULL == head)
		goto out;

	rc = child(carg);
out:
	if (-1 != in)
		close(in);
	if (-1 != s)
		close(s);

	return(rc);
}

static int
regress(enum cgimode mode, kcgi_regress_client parent, 
	void *parg, kcgi_regress_server child, void *carg)
{
	pid_t	 chld, pid;
	int	 rc, st;

	/* Create our "test framework" child. */
	if (-1 == (chld = fork())) {
		perror(NULL);
		exit(EXIT_FAILURE);
	} else if (0 == chld) {
		if (MODE_CGI == mode)
			rc = dochild_cgi(child, carg);
		else
			rc = dochild_fcgi(child, carg);
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
		fprintf(stderr, "Child not sleeping!?\n");
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
kcgi_regress_fastcgi(kcgi_regress_client parent, void *parg,
	kcgi_regress_server child, void *carg)
{

	return(regress(MODE_FASTCGI, parent, parg, child, carg));
}

int
kcgi_regress_cgi(kcgi_regress_client parent, void *parg,
	kcgi_regress_server child, void *carg)
{

	return(regress(MODE_CGI, parent, parg, child, carg));
}
