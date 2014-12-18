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
#include <netinet/in.h>

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>

#include <curl/curl.h>

#include "regress.h"

static int
dochild(cb_child child)
{
	int	 	 s, in, rc, opt, first;
	struct sockaddr_in ad, rem;
	socklen_t	 len;
	char		 head[1024], *cp, *cpp;
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

	if (NULL == (cp = strdup(__progname))) {
		perror(NULL);
		exit(EXIT_FAILURE);
	}
	setenv("SCRIPT_NAME", cp, 1);
	
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
				cp = strdup(head + 4);
			} else if (0 == strncmp(head, "POST ", 5)) {
				setenv("REQUEST_METHOD", "POST", 1);
				cp = strdup(head + 5);
			} else {
				fprintf(stderr, "Unknown HTTP "
					"first line: %s\n", head);
				goto out;
			}

			if (NULL == cp) {
				perror(NULL);
				exit(EXIT_FAILURE);
			}

			cpp = cp;
			while ('\0' != *cpp && ! isspace(*cpp))
				cpp++;
			*cpp = '\0';
			first = 0;
			setenv("PATH_INFO", cp, 1);
			continue;
		}

		/* Process header lines. */
		/*fprintf(stderr, "Skipping header: %s\n", head);*/
	}

	if (NULL == head)
		goto out;

	rc = child();
out:
	if (-1 != in)
		close(in);
	if (-1 != s)
		close(s);
	return(rc);
}

int
regress(cb_parent parent, cb_child child)
{
	pid_t	 chld, pid;
	int	 rc, st;
	CURL	*curl;

	/* Create our "test framework" child. */
	if (-1 == (chld = fork())) {
		perror(NULL);
		exit(EXIT_FAILURE);
	} else if (0 == chld)
		exit(dochild(child) ? 
			EXIT_SUCCESS : EXIT_FAILURE);

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

	/* 
	 * We're now ready to run our parent context test.
	 * Initialise the CURL context and get ready to go.
	 */
	if (NULL == (curl = curl_easy_init())) {
		perror(NULL);
		exit(EXIT_FAILURE);
	}

	rc = parent(curl);

	/*
	 * Make sure that the child has exited properly.
	 */
	curl_easy_cleanup(curl);
	curl_global_cleanup();

	if (-1 == waitpid(pid, &st, 0)) {
		perror(NULL);
		exit(EXIT_FAILURE);
	} 

	return(rc && WIFEXITED(st) && 
		EXIT_SUCCESS == WEXITSTATUS(st));
}
