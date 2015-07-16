/*	$Id$ */
/*
 * Copyright (c) 2015 Kristaps Dzonsons <kristaps@bsd.lv>
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
#include <sys/stat.h>
#include <sys/un.h>
#include <sys/wait.h>

#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static	volatile sig_atomic_t stop = 0;

static void
sighandle(int sig)
{

	stop = 1;
}

int
main(int argc, char *argv[])
{
	int			 c, fd, rc;
	pid_t			*ws;
	size_t			 wsz, i, sz;
	const char		*pname, *sockpath;
	struct sockaddr_un	 sun;
	mode_t			 old_umask;

	rc = EXIT_FAILURE;

	if ((pname = strrchr(argv[0], '/')) == NULL)
		pname = argv[0];
	else
		++pname;

	wsz = 5;
	sockpath = "/var/www/run/httpd.sock";
	ws = NULL;

	while (-1 != (c = getopt(argc, argv, "n:s:")))
		switch (c) {
		case ('n'):
			wsz = atoi(optarg);
			break;	
		case ('s'):
			sockpath = optarg;
			break;	
		default:
			goto usage;
		}

	argc -= optind;
	argv += optind;

	/* Do the usual dance to set up UNIX sockets. */
	memset(&sun, 0, sizeof(sun));
	sun.sun_family = AF_UNIX;
	sz = strlcpy(sun.sun_path, sockpath, sizeof(sun.sun_path));
	if (sz >= sizeof(sun.sun_path)) {
		fprintf(stderr, "socket path to long\n");
		return(EXIT_FAILURE);
	}
	sun.sun_len = sz;

	/*
	 * Prepare the socket then unlink any dead existing ones.
	 * This is because we want to control the socket.
	 */
	if (-1 == (fd = socket(AF_UNIX, SOCK_STREAM, 0))) {
		perror("socket");
		return(EXIT_FAILURE);
	} else if (-1 == unlink(sockpath)) {
		if (ENOENT != errno) {
			perror(sockpath);
			close(fd);
			return(EXIT_FAILURE);
		}
	}

	/* 
	 * Set permissions for 0666.
	 * FIXME: use user/group identifier and 0660. 
	 */
	old_umask = umask(S_IXUSR|S_IXGRP|S_IXOTH|S_IXOTH);

	/* 
	 * Now actually bind to the FastCGI socket, set up our
	 * listeners, and make sure that we're not blocking.
	 * We buffer up to the number of available workers.
	 */
	if (-1 == bind(fd, (struct sockaddr *)&sun, sizeof(sun))) {
		perror("bind");
		close(fd);
		return(EXIT_FAILURE);
	}
	umask(old_umask);
	if (-1 == listen(fd, wsz)) {
		perror("listen");
		close(fd);
		return(EXIT_FAILURE);
	}

	/* Allocate worker array. */
	if (NULL == (ws = calloc(sizeof(pid_t), wsz))) {
		fprintf(stderr, "%s: memory failure\n", pname);
		return(EXIT_FAILURE);
	}

	/*
	 * Dying children should notify us that something is horribly
	 * wrong and we should exit.
	 */
	signal(SIGTERM, sighandle);

	for (i = 0; i < wsz; i++) {
		if (-1 == (ws[i] = fork())) {
			perror("fork");
			break;
		} else if (0 == ws[i]) {
			/*
			 * Assign stdin to be the socket over which
			 * we're going to transfer request descriptors
			 * when we get them.
			 */
			if (-1 == dup2(fd, STDIN_FILENO))
				_exit(EXIT_FAILURE);
			close(fd);
			execvp(argv[0], argv);
			perror(argv[0]);
			_exit(EXIT_FAILURE);
		}
	}

	/* Close local reference to server. */
	close(fd);
	while (0 == stop) {
		if (0 != sleep(10))
			break;
		/*
		 * XXX: this is entirely for the benefit of
		 * valgrind(1), and will be disabled in later releases.
		 * valgrind(1) doesn't receive the SIGCHLD, so it needs
		 * to manually check whether the PIDs exist.
		 */
		for (i = 0; i < wsz; i++) {
			if (0 == waitpid(ws[i], NULL, WNOHANG))
				continue;
			fprintf(stderr, "%s: process has died "
				"(pid %d)\n", argv[0], ws[i]);
			ws[i] = -1;
			stop = 1;
			break;
		}
	}

	/*
	 * Now wait on the children.
	 * TODO: kill children if we wait too long.
	 */
	for (i = 0; i < wsz; i++) {
		if (-1 == ws[i])
			continue;
		kill(ws[i], SIGTERM);
		if (-1 == waitpid(ws[i], &c, 0))
			perror("waitpid");
		else if ( ! WIFEXITED(c))
			fprintf(stderr, "%s: process did not "
				"exit (pid %d)\n", argv[0], ws[i]);
		else if (EXIT_SUCCESS != WEXITSTATUS(c))
			fprintf(stderr, "%s: process exited with "
				"error (pid %d)\n", argv[0], ws[i]);
	}

	free(ws);
	return(rc);
usage:
	fprintf(stderr, "usage: %s "
		"[-n workers] "
		"[-s sockpath] "
		"-- prog [arg1...]\n", pname);
	return(EXIT_FAILURE);
}
