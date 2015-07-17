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
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>

#include "kcgiregress.h"

static int
dochild_cgi(kcgi_regress_server child, void *carg)
{
	int	 	 s, in, rc, opt, first;
	struct sockaddr_in ad, rem;
	socklen_t	 len;
	char		 head[1024], headbuf[1024];
	char		*cp, *path, *query, *key, *val;
	size_t		 sz;
	extern char	*__progname;

	rc = 0;
	opt = 1;
	in = -1;
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
		if (0 == strcmp(key, "Content-Length")) {
			setenv("CONTENT_LENGTH", val, 1);
			continue;
		} else if (0 == strcmp(key, "Content-Type")) {
			setenv("CONTENT_TYPE", val, 1);
			continue;
		}

		/*
		 * Now we have "regular" attributes that we want to cast
		 * directly as HTTP attributes in the CGI environment.
		 */
		strlcpy(headbuf, "HTTP_", sizeof(headbuf));
		sz = strlcat(headbuf, key, sizeof(headbuf));
		assert(sz < sizeof(headbuf));
		for (cp = headbuf; '\0' != *cp; cp++) 
			if ('-' == *cp)
				*cp = '_';
			else if (isalpha((int)*cp))
				*cp = toupper((int)*cp);

		setenv(headbuf, val, 1);
	}

	if (-1 != in)
		close(in);
	if (-1 != s)
		close(s);
	return(child(carg));
out:
	if (-1 != in)
		close(in);
	if (-1 != s)
		close(s);
	return(rc);
}

static int
regress(kcgi_regress_client parent, void *parg, 
	kcgi_regress_server child, void *carg)
{
	pid_t	 chld, pid;
	int	 rc, st;

	/* Create our "test framework" child. */
	if (-1 == (chld = fork())) {
		perror(NULL);
		exit(EXIT_FAILURE);
	} else if (0 == chld) {
		rc = dochild_cgi(child, carg);
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
kcgi_regress_cgi(kcgi_regress_client parent, void *parg,
	kcgi_regress_server child, void *carg)
{

	return(regress(parent, parg, child, carg));
}
