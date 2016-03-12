/*	$Id$ */
/*
 * Copyright (c) 2015--2016 Kristaps Dzonsons <kristaps@bsd.lv>
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

#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <sys/wait.h>

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <limits.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

/*
 * This is used by the "variable pool" implementation to keep track of
 * children, their input descriptor, and their active work.
 */
struct	worker {
	int	 fd; /* active request fd or -1 */
	int	 ctrl; /* control socket */
	pid_t	 pid; /* process */
};

/*
 * Whether we're supposed to stop.
 */
static	volatile sig_atomic_t stop = 0;

static void
sighandle(int sig)
{

	stop = 1;
}

static int
fullwritefd(int fd, int sendfd, void *b, size_t bsz)
{
	struct msghdr	 msg;
	int		 rc;
	char		 buf[CMSG_SPACE(sizeof(fd))];
	struct iovec 	 io;
	struct cmsghdr	*cmsg;
	struct pollfd	 pfd;

	assert(bsz <= 256);

	memset(buf, 0, sizeof(buf));
	memset(&msg, 0, sizeof(struct msghdr));
	memset(&io, 0, sizeof(struct iovec));

	io.iov_base = b;
	io.iov_len = bsz;

	msg.msg_iov = &io;
	msg.msg_iovlen = 1;
	msg.msg_control = buf;
	msg.msg_controllen = sizeof(buf);

	cmsg = CMSG_FIRSTHDR(&msg);
	cmsg->cmsg_level = SOL_SOCKET;
	cmsg->cmsg_type = SCM_RIGHTS;
	cmsg->cmsg_len = CMSG_LEN(sizeof(fd));

	*((int *)CMSG_DATA(cmsg)) = sendfd;

	msg.msg_controllen = cmsg->cmsg_len;

	pfd.fd = fd;
	pfd.events = POLLOUT;
again:
	if ((rc = poll(&pfd, 1, -1)) < 0) {
		perror("poll");
		return(-1);
	} else if (0 == rc) {
		fprintf(stderr, "poll: timeout!?\n");
		goto again;
	} else if ( ! (POLLOUT & pfd.revents)) {
		fprintf(stderr, "poll: hangup\n");
		return(-1);
	} else if (sendmsg(fd, &msg, 0) < 0) {
		perror("sendmsg");
		return(0);
	}
	return(1);
}

static int
fullread(int fd, void *buf, size_t bufsz)
{
	ssize_t	 	 ssz;
	size_t	 	 sz;
	struct pollfd	 pfd;
	int		 rc, er;

	pfd.fd = fd;
	pfd.events = POLLIN;

	for (sz = 0; sz < bufsz; sz += (size_t)ssz) {
		if ((rc = poll(&pfd, 1, -1)) < 0) {
			perror("poll");
			return(0);
		} else if (0 == rc) {
			fprintf(stderr, "poll: timeout!?\n");
			ssz = 0;
			continue;
		} else if ( ! (POLLIN & pfd.revents)) {
			fprintf(stderr, "poll: hangup\n");
			return(0);
		} else if ((ssz = read(fd, buf + sz, bufsz - sz)) < 0) {
			er = errno;
			fprintf(stderr, "read: %d, %zu: %s\n", fd, bufsz - sz, strerror(er));
			return(0);
		} else if (0 == ssz && sz > 0) {
			fprintf(stderr, "read: short read\n");
			return(0);
		} else if (0 == ssz && sz == 0) {
			fprintf(stderr, "read: unexpected eof\n");
			return(0);
		} else if (sz > SIZE_MAX - (size_t)ssz) {
			fprintf(stderr, "read: overflow: %zu, %zd\n", sz, ssz);
			return(0);
		}
	}

	return(1);
}

static int
xsocketpair(int *sock)
{
	int	 rc, fl1, fl2;

	sock[0] = sock[1] = -1;
	rc = socketpair(AF_UNIX, SOCK_STREAM, 0, sock);
	if (-1 == rc)
		perror("socketpair");
	else if (-1 == (fl1 = fcntl(sock[0], F_GETFL, 0)))
		perror("fcntl");
	else if (-1 == (fl2 = fcntl(sock[1], F_GETFL, 0)))
		perror("fcntl");
	else if (-1 == fcntl(sock[0], F_SETFL, fl1 | O_NONBLOCK))
		perror("fcntl");
	else if (-1 == fcntl(sock[1], F_SETFL, fl2 | O_NONBLOCK))
		perror("fcntl");
	else
		return(1);

	if (-1 != sock[0])
		close(sock[0]);
	if (-1 != sock[1])
		close(sock[1]);

	return(0);
}

static int
varpool(size_t wsz, int fd, const char *sockpath, char *argv[])
{
	struct worker	*ws;
	size_t		 pfdsz, pfdmaxsz, i, j;
	int		 rc, exitcode, afd, pair[2];
	struct pollfd	*pfd;
	struct sockaddr_storage ss;
	socklen_t	 sslen;
	char		 buf[64];
	void		*pp;

	/* 
	 * Allocate worker array.
	 * We'll only grow after this point.
	 */
	ws = calloc(wsz, sizeof(struct worker));
	pfdmaxsz = wsz + 1;
	pfd = calloc(pfdmaxsz, sizeof(struct pollfd));
	exitcode = 0;

	if (NULL == ws || NULL == pfd) {
		/* Serious problems. */
		perror(NULL);
		close(fd);
		unlink(sockpath);
		free(ws);
		free(pfd);
		return(0);
	}

	/* Guard against bad de-allocation. */
	for (i = 0; i < wsz; i++) {
		ws[i].ctrl = ws[i].fd = -1;
		ws[i].pid = -1;
	}

	/*
	 * Dying children should notify us that something is horribly
	 * wrong and we should exit.
	 */
	signal(SIGTERM, sighandle);
	signal(SIGCHLD, sighandle);
	signal(SIGINT, sighandle);

	/*
	 * Zeroth pfd is always the domain socket.
	 */
	pfd[0].fd = fd;
	pfd[0].events = POLLIN;
	pfdsz = 1;

	/*
	 * Start up the [initial] worker processes.
	 * We'll later spin up workers when we need them.
	 */
	for (i = 0; i < wsz; i++) {
		/* 
		 * Make our control pair. 
		 * This makes a non-blocking socket that we'll use to
		 * write the file descriptor into.
		 */
		if ( ! xsocketpair(pair))
			goto out;
		ws[i].ctrl = pair[0];

		if (-1 == (ws[i].pid = fork())) {
			perror("fork");
			goto out;
		} else if (0 == ws[i].pid) {
			/* 
			 * Close out all current descriptors.
			 * Note that we're a "new-mode" FastCGI manager
			 * to the child via our environment variable.
			 */
			for (i = 0; i < wsz; i++)
				if (-1 != ws[i].ctrl)
					close(ws[i].ctrl);
			close(fd);
			snprintf(buf, sizeof(buf), "%d", pair[1]);
			setenv("FCGI_LISTENSOCK_DESCRIPTORS", buf, 1);
			/* coverity[tainted_string] */
			execv(argv[0], argv);
			perror(argv[0]);
			_exit(EXIT_FAILURE);
		}
		/* Close the child descriptor. */
		close(pair[1]);
	}

pollagain:
	/*
	 * Main part.
	 * Poll on our control socket (pfd[0]) and the children that
	 * have active connections.
	 * We have a timeout if we get a termination signal whilst
	 * waiting.
	 */
	if ((rc = poll(pfd, pfdsz, 1000)) < 0) {
		if (EINTR == errno && stop) {
			exitcode = 1;
			goto out;
		}
		perror("poll");
		goto out;
	} else if (stop) {
		exitcode = 1;
		goto out;
	} else if (0 == rc)
		goto pollagain;

	/*
	 * Now we see which of the workers has exited.
	 * We do this until we've processed all of them.
	 */
	assert(rc > 0);
	for (i = 1; i < pfdsz && rc > 0; ) {
		if (POLLHUP & pfd[i].revents ||
		    POLLERR & pfd[i].revents) {
			fprintf(stderr, "poll: HUP/error\n");
			goto out;
		} else if ( ! (POLLIN & pfd[i].revents)) {
			i++;
			continue;
		} 

		/* 
		 * Read the "identifier" that the child process gives
		 * to us.
		 */
		if (fullread(pfd[i].fd, &afd, sizeof(int)) < 0)
			goto out;

		/*
		 * First, look up the worker that's now free.
		 * Mark its active descriptor as free.
		 * TODO: use a queue of "working" processes to prevent
		 * this from being so expensive.
		 */
		for (j = 0; j < wsz; j++)
			if (ws[j].fd == afd)
				break;

		if (j == wsz) {
			fprintf(stderr, "poll: spurrious event\n");
			goto out;
		}

		/*
		 * Close the descriptor (that we still hold) and mark
		 * this worker as no longer working.
		 */
		rc--;
		close(ws[j].fd);
		ws[j].fd = -1;

		/*
		 * Now, clear the active descriptor from the file
		 * descriptor array.
		 * Do so by flipping it into the last slot then
		 * truncating the array size.
		 * Obviously, we only do this if we're not the current
		 * end of array...
		 */
		if (pfdsz - 1 != i)
			memcpy(&pfd[i], &pfd[pfdsz - 1], sizeof(struct pollfd));

		pfdsz--;
	}

	if (POLLHUP & pfd[0].revents) {
		fprintf(stderr, "poll: control HUP!?\n");
		goto out;
	} else if (POLLERR & pfd[0].revents) {
		fprintf(stderr, "poll: control error!?\n");
		goto out;
	} else if ( ! (POLLIN & pfd[0].revents))
		goto pollagain;

	fprintf(stderr, "Allocating new...\n");

	/* 
	 * We have a new request.
	 * First, see if we need to allocate more workers.
	 */
	if (pfdsz == pfdmaxsz) {
		pp = realloc(pfd, (pfdmaxsz + 1) * 
			sizeof(struct pollfd));
		if (NULL == pp) {
			perror("realloc");
			goto out;
		}
		pfd = pp;
		pp = realloc(ws, (wsz + 1) * sizeof(struct worker));
		if (NULL == pp) {
			perror("realloc");
			goto out;
		}
		ws = pp;

		memset(&pfd[pfdmaxsz], 0, sizeof(struct pollfd));
		ws[wsz].pid = -1;
		ws[wsz].fd = -1;

		/* Make our control pair. */
		if ( ! xsocketpair(pair))
			goto out;
		ws[wsz].ctrl = pair[0];
		if (-1 == (ws[wsz].pid = fork())) {
			perror("fork");
			goto out;
		} else if (0 == ws[wsz].pid) {
			for (i = 0; i < wsz; i++)
				if (-1 != ws[i].ctrl)
					close(ws[i].ctrl);
			close(fd);
			snprintf(buf, sizeof(buf), "%d", pair[1]);
			setenv("FCGI_LISTENSOCK_DESCRIPTORS", buf, 1);
			/* coverity[tainted_string] */
			execv(argv[0], argv);
			perror(argv[0]);
			_exit(EXIT_FAILURE);
		}
		close(pair[1]);
		pfdmaxsz++;
		wsz++;
	} 

	/*
	 * Actually accept the socket.
	 * Don't do anything with it, however.
	 */
	sslen = sizeof(ss);
	afd = accept(fd, (struct sockaddr *)&ss, &sslen);
	if (afd < 0) {
		perror("accept");
		goto out;
	} 

	/*
	 * Look up the next unavailable worker.
	 */
	for (i = 0; i < wsz; i++)
		if (-1 == ws[i].fd) 
			break;
	assert(i < wsz);
	ws[i].fd = afd;
	fprintf(stderr, "Child[%zu] <- %d\n", i, afd);
	pfd[pfdsz].events = POLLIN;
	pfd[pfdsz].fd = ws[i].ctrl;
	pfdsz++;
	if (0 == fullwritefd(ws[i].ctrl, ws[i].fd, &ws[i].fd, sizeof(int)))
		goto out;

	goto pollagain;
out:

	for (i = 0; i < wsz; i++) {
		if (-1 != ws[i].ctrl)
			close(ws[i].ctrl);
		if (-1 != ws[i].pid)
			kill(ws[i].pid, SIGTERM);
	}

	for (i = 0; i < wsz; i++) {
		if (-1 != ws[i].pid) {
			if (waitpid(ws[i].pid, NULL, 0) < 0)
				perror("waitpid");
		}
	}

	free(ws);
	free(pfd);
	return(exitcode);
}

static int
fixedpool(size_t wsz, int fd, const char *sockpath, char *argv[])
{
	pid_t		*ws;
	size_t		 i;
	int		 c;

	fprintf(stderr, "Fixed pool: %zu\n", wsz);

	/* Allocate worker array. */
	if (NULL == (ws = calloc(wsz, sizeof(pid_t)))) {
		perror(NULL);
		close(fd);
		unlink(sockpath);
		return(0);
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
			/* coverity[tainted_string] */
			execv(argv[0], argv);
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
	return(1);
}

int
main(int argc, char *argv[])
{
	int			 c, fd, rc, varp;
	pid_t			*ws;
	struct passwd		*pw;
	size_t			 wsz, sz, lsz;
	const char		*pname, *sockpath, *chpath,
	      			*sockuser, *procuser, *errstr;
	struct sockaddr_un	 sun;
	mode_t			 old_umask;
	uid_t		 	 sockuid, procuid;
	gid_t			 sockgid, procgid;

	if ((pname = strrchr(argv[0], '/')) == NULL)
		pname = argv[0];
	else
		++pname;

	if (0 != geteuid()) {
		fprintf(stderr, "%s: need root privileges\n", pname);
		return(EXIT_FAILURE);
	}

	lsz = 0;
	rc = EXIT_FAILURE;
	sockuid = procuid = sockgid = procgid = -1;
	wsz = 5;
	sockpath = "/var/www/run/httpd.sock";
	chpath = "/var/www";
	ws = NULL;
	sockuser = procuser = NULL;
	varp = 0;

	while (-1 != (c = getopt(argc, argv, "l:p:n:s:u:U:v")))
		switch (c) {
		case ('l'):
			lsz = strtonum(optarg, 0, INT_MAX, &errstr);
			if (NULL == errstr)
				break;
			fprintf(stderr, "-l must be "
				"between 0 and %d\n", INT_MAX);
			return(EXIT_FAILURE);	
		case ('n'):
			wsz = strtonum(optarg, 0, INT_MAX, &errstr);
			if (NULL == errstr)
				break;
			fprintf(stderr, "-n must be "
				"between 0 and %d\n", INT_MAX);
			return(EXIT_FAILURE);	
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
		case ('v'):
			varp = 1;
			break;	
		default:
			goto usage;
		}

	argc -= optind;
	argv += optind;

	if (0 == argc) 
		goto usage;

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

	if (-1 == listen(fd, 0 == lsz ? wsz : lsz)) {
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

	if (varp) {
		if ( ! varpool(wsz, fd, sockpath, argv))
			return(EXIT_FAILURE);
		return(EXIT_SUCCESS);
	}

	if ( ! fixedpool(wsz, fd, sockpath, argv))
		return(EXIT_FAILURE);
	return(EXIT_SUCCESS);
usage:
	fprintf(stderr, "usage: %s "
		"[-l backlog] "
		"[-n workers] "
		"[-p chroot] "
		"[-s sockpath] "
		"[-u sockuser] "
		"[-U procuser] "
		"-- prog [arg1...]\n", pname);
	return(EXIT_FAILURE);
}
