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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <sys/wait.h>

#include <errno.h>
#include <getopt.h>
#include <pwd.h>
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
	struct passwd		*pw;
	size_t			 wsz, i, sz;
	const char		*pname, *sockpath, *chpath,
	      			*sockuser, *procuser;
	struct sockaddr_un	 sun;
	mode_t			 old_umask;
	uid_t		 	 sockuid, procuid;
	gid_t			 sockgid, procgid;

	if ((pname = strrchr(argv[0], '/')) == NULL)
		pname = argv[0];
	else
		++pname;

	if (0 != geteuid()) {
		fprintf(stderr, "%s: need root privileges", pname);
		return(EXIT_FAILURE);
	}

	rc = EXIT_FAILURE;
	sockuid = procuid = sockgid = procgid = -1;
	wsz = 5;
	sockpath = "/var/www/run/httpd.sock";
	chpath = "/var/www";
	ws = NULL;
	sockuser = procuser = NULL;

	while (-1 != (c = getopt(argc, argv, "p:n:s:u:U:")))
		switch (c) {
		case ('n'):
			wsz = atoi(optarg);
			break;	
		case ('p'):
			chpath = optarg;
			break;	
		case ('s'):
			sockpath = optarg;
			break;	
		case ('u'):
			sockuser = optarg;
			break;	
		case ('U'):
			procuser = optarg;
			break;	
		default:
			goto usage;
		}

	argc -= optind;
	argv += optind;

	pw = NULL;
	if (NULL != procuser && NULL == (pw = getpwnam(procuser))) { 
		fprintf(stderr, "%s: no such user\n", procuser);
		return(EXIT_FAILURE);
	} else if (NULL != pw) {
		procuid = pw->pw_uid;
		procgid = pw->pw_gid;
	}

	pw = NULL;
	if (NULL != sockuser && NULL == (pw = getpwnam(sockuser))) { 
		fprintf(stderr, "%s: no such user\n", sockuser);
		return(EXIT_FAILURE);
	} else if (NULL != pw) {
		sockuid = pw->pw_uid;
		sockgid = pw->pw_gid;
	}

	/* Do the usual dance to set up UNIX sockets. */
	memset(&sun, 0, sizeof(sun));
	sun.sun_family = AF_UNIX;
	sz = strlcpy(sun.sun_path, sockpath, sizeof(sun.sun_path));
	if (sz >= sizeof(sun.sun_path)) {
		fprintf(stderr, "socket path to long\n");
		return(EXIT_FAILURE);
	}
#ifndef __linux__
	sun.sun_len = sz;
#endif

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

	old_umask = umask(S_IXUSR|S_IXGRP|S_IWOTH|S_IROTH|S_IXOTH);

	/* 
	 * Now actually bind to the FastCGI socket set up our
	 * listeners, and make sure that we're not blocking.
	 * If necessary, change the file's ownership.
	 * We buffer up to the number of available workers.
	 */
	if (-1 == bind(fd, (struct sockaddr *)&sun, sizeof(sun))) {
		perror("bind");
		close(fd);
		return(EXIT_FAILURE);
	}
	umask(old_umask);

	if (NULL != sockuser) 
		if (chown(sockpath, sockuid, sockgid) == -1) {
			perror(sockpath);
			close(fd);
			return(EXIT_FAILURE);
		}

	if (-1 == listen(fd, wsz)) {
		perror(sockpath);
		close(fd);
		return(EXIT_FAILURE);
	}

	/* 
	 * Jail our file-system.
	 */
	if (-1 == chroot(chpath)) {
		perror(chpath);
		close(fd);
		return(EXIT_FAILURE);
	} else if (-1 == chdir("/")) {
		perror("/");
		close(fd);
		unlink(sockpath);
		return(EXIT_FAILURE);
	}

	if (NULL != procuser)  {
		if (0 != setgid(procgid)) {
			perror(procuser);
			close(fd);
			return(EXIT_FAILURE);
		} else if (0 != setuid(procuid)) {
			perror(procuser);
			close(fd);
			return(EXIT_FAILURE);
		} else if (-1 != setuid(0)) {
			fprintf(stderr, "%s: managed to regain "
				"root privileges: aborting\n", pname);
			close(fd);
			return(EXIT_FAILURE);
		}
	}

	/* Allocate worker array. */
	if (NULL == (ws = calloc(sizeof(pid_t), wsz))) {
		perror(NULL);
		close(fd);
		unlink(sockpath);
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
		if (0 != sleep(2))
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
	 */
	for (i = 0; i < wsz; i++) {
		if (-1 == ws[i])
			continue;
		if (-1 == kill(ws[i], SIGTERM))
			perror("kill");
	}

	sleep(1);

	for (i = 0; i < wsz; i++) {
		if (-1 == ws[i])
			continue;
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
		"[-p chroot] "
		"[-s sockpath] "
		"[-u sockuser] "
		"[-U procuser] "
		"-- prog [arg1...]\n", pname);
	return(EXIT_FAILURE);
}
