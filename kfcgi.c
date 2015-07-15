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
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <sys/wait.h>

#include <errno.h>
#include <getopt.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/*
 * The status of each FastCGI worker.
 * We don't really have anything here right now.
 */
struct	fcgi {
	pid_t	 pid; /* pid */
	int	 control[2]; /* socketpair */
};

static	volatile sig_atomic_t stop = 0;

/*
 * Handle child termination.
 */
static void
sighandle(int sig)
{

	stop = 1;
}

/*
 * Transfer file descriptor `fd' to the process connected to `socket'.
 * This is almost verbatim from Chapter 15 of Stevens' UNIX Network
 * Programming book.
 */
static int 
sendfd(int socket, int fd)
{
	struct msghdr	 msg;
	char 		 buf[CMSG_SPACE(sizeof(fd))];
	struct iovec 	 io;
	struct cmsghdr 	*cmsg;
	unsigned char 	 value;

	memset(buf, 0, sizeof(buf));
	memset(&msg, 0, sizeof(struct msghdr));
	memset(&io, 0, sizeof(struct iovec));

	value = '\0';
	io.iov_base = &value;
	io.iov_len = 0;
	msg.msg_iov = &io;
	msg.msg_iovlen = 1;
	msg.msg_control = buf;
	msg.msg_controllen = sizeof(buf);

	cmsg = CMSG_FIRSTHDR(&msg);
	cmsg->cmsg_level = SOL_SOCKET;
	cmsg->cmsg_type = SCM_RIGHTS;
	cmsg->cmsg_len = CMSG_LEN(sizeof(fd));

	*((int *)CMSG_DATA(cmsg)) = fd;
	msg.msg_controllen = cmsg->cmsg_len;
	return(-1 != sendmsg(socket, &msg, 0));
}

int
main(int argc, char *argv[])
{
	int			 c, fd, rc, nfd;
	const int		 on = 1;
	struct fcgi		*ws;
	size_t			 wsz, i, sz, total;
	const char		*pname, *sockpath;
	struct sockaddr_un	 sun;
	struct pollfd		 pfd;
	socklen_t		 slen;
	struct sockaddr_storage	 ss;
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

	/* Allocate worker array. */
	if (NULL == (ws = calloc(sizeof(struct fcgi), wsz))) {
		fprintf(stderr, "%s: memory failure\n", pname);
		return(EXIT_FAILURE);
	}

	/*
	 * Dying children should notify us that something is horribly
	 * wrong and we should exit.
	 * TODO: catch SIGINT and so on and handle them accordingly.
	 */
	signal(SIGCHLD, sighandle);

	for (i = 0; i < wsz; i++) {
		c = socketpair(AF_UNIX, SOCK_STREAM, 0, ws[i].control);
		if (-1 == c) {
			perror("socketpair");
			break;
		} else if (-1 == (ws[i].pid = fork())) {
			perror("fork");
			break;
		} else if (0 == ws[i].pid) {
			/*
			 * Assign stdin to be the socket over which
			 * we're going to transfer request descriptors
			 * when we get them.
			 */
			close(ws[i].control[0]);
			ws[i].control[0] = -1;
			if (-1 == dup2(ws[i].control[1], STDIN_FILENO))
				_exit(EXIT_FAILURE);
			execvp(argv[0], argv);
			_exit(EXIT_FAILURE);
		}
		close(ws[i].control[1]);
		ws[i].control[1] = -1;
	}

	/*
	 * Prepare the socket then unlink any dead existing ones.
	 * This is because we want to control the socket.
	 */
	if (-1 == (fd = socket(AF_UNIX, SOCK_STREAM, 0))) {
		perror("socket");
		goto out;
	} else if (-1 == unlink(sockpath)) {
		perror(sockpath);
		goto out;
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
		goto out;
	}
	umask(old_umask);
	if (-1 == ioctl(fd, FIONBIO, &on)) {
		perror("ioctl");
		goto out;
	} else if (-1 == listen(fd, wsz)) {
		perror("listen");
		goto out;
	}

	pfd.fd = fd;
	pfd.events = POLLIN;
	total = 0;

	/*
	 * While we have data coming over the wire, pass file
	 * descriptors immediately to our children.
	 * The algorithm for distributing over children is just
	 * round-robin because we don't know anything about their
	 * status.
	 */
	while ( ! stop) {
		c = poll(&pfd, 1, -1);
		if (c < 0 && EINTR == errno) {
			fprintf(stderr, "%s: interrupt caught\n", pname);
			break;
		} else if (c < 0) {
			perror("poll");
			goto out;
		} else if (POLLIN != pfd.events) {
			fprintf(stderr, "%s: bad poll events\n", pname);
			goto out;
		}

		nfd = accept(fd, (struct sockaddr *)&ss, &slen);
		if ( ! sendfd(ws[total % wsz].control[0], nfd)) {
			fprintf(stderr, "%s: dead child\n", pname);
			goto out;
		}
		close(nfd);
		total++;
	}

	rc = EXIT_SUCCESS;
out:
	/* 
	 * First, close the sockets.
	 * This will make any children that are waiting exit properly.
	 */
	for (i = 0; i < wsz; i++) {
		close(ws[i].control[0]);
		ws[i].control[0] = -1;
	}

	/*
	 * Now wait on the children.
	 * TODO: kill children if we wait too long.
	 */
	for (i = 0; i < wsz; i++) {
		if (-1 == waitpid(ws[i].pid, &c, 0))
			perror("waitpid");
		else if ( ! WIFEXITED(c))
			fprintf(stderr, "%s: child signalled!?\n", pname);
		else if (EXIT_SUCCESS != WEXITSTATUS(c))
			fprintf(stderr, "%s: child had bad exit\n", pname);
	}

	if (-1 != fd)
		close(fd);

	free(ws);
	return(rc);
usage:
	fprintf(stderr, "usage: %s "
		"[-n workers] "
		"[-s sockpath] "
		"-- prog [arg1...]\n", pname);
	return(EXIT_FAILURE);
}
