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
#include "config.h"

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
#include <poll.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>

/*
 * This is used by the "variable pool" implementation to keep track of
 * children, their input descriptor, and their active work.
 */
struct	worker {
	int	 fd; /* active request fd or -1 */
	int	 ctrl; /* control socket */
	pid_t	 pid; /* process */
	time_t	 last; /* last deschedule or 0 if never */
	uint64_t cookie;
};

/*
 * Whether we're supposed to stop or whether we've had a child exit.
 */
static	volatile sig_atomic_t stop = 0;
static	volatile sig_atomic_t chld = 0;
static	volatile sig_atomic_t hup = 0;

static	int verbose = 0;

static 	void dbg(const char *fmt, ...) 
		__attribute__((format(printf, 1, 2)));

static void
sighandlehup(int sig)
{

	hup = 1;
}

static void
sighandlestop(int sig)
{

	stop = 1;
}

static void
sighandlechld(int sig)
{

	chld = 1;
}

static void
dbg(const char *fmt, ...) 
{
	va_list	 ap;

	if (0 == verbose)
		return;
	va_start(ap, fmt);
	vsyslog(LOG_DEBUG, fmt, ap);
	va_end(ap);
}

/*
 * Fully write a file descriptor and the non-optional buffer.
 * Return 0 on failure, 1 on success.
 */
static int
fullwritefd(int fd, int sendfd, void *b, size_t bsz)
{
	struct msghdr	 msg;
	int		 rc;
	char		 buf[CMSG_SPACE(sizeof(fd))];
	struct iovec 	 io;
	struct cmsghdr	*cmsg;
	struct pollfd	 pfd;

	assert(bsz && bsz <= 256);

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
		syslog(LOG_ERR, "poll: passing "
			"connection to worker: %m");
		return(0);
	} else if (0 == rc) {
		syslog(LOG_WARNING, "poll: passing "
			"connection to worker: timeout!?");
		goto again;
	} else if ( ! (POLLOUT & pfd.revents)) {
		syslog(LOG_ERR, "poll: passing "
			"connection to worker: disconnect");
		return(0);
	} else if (sendmsg(fd, &msg, 0) < 0) {
		syslog(LOG_ERR, "sendmsg: passing "
			"connection to worker: %m");
		return(0);
	}
	return(1);
}

/*
 * Fully read a buffer of size "bufsz".
 * Returns 0 on failure (soft and hard), 1 on success.
 */
static int
fullread(int fd, void *buf, size_t bufsz)
{
	ssize_t	 	 ssz;
	size_t	 	 sz;
	struct pollfd	 pfd;
	int		 rc;

	pfd.fd = fd;
	pfd.events = POLLIN;

	for (sz = 0; sz < bufsz; sz += (size_t)ssz) {
		if ((rc = poll(&pfd, 1, -1)) < 0) {
			syslog(LOG_ERR, "poll: receiving "
				"ack from worker: %m");
			return(0);
		} else if (0 == rc) {
			syslog(LOG_WARNING, "poll: receiving "
				"ack from worker: timeout");
			ssz = 0;
			continue;
		} else if ( ! (POLLIN & pfd.revents)) {
			syslog(LOG_ERR, "poll: receiving "
				"ack from worker: disconnect");
			return(0);
		} else if ((ssz = read(fd, buf + sz, bufsz - sz)) < 0) {
			syslog(LOG_ERR, "read: receiving "
				"ack from worker: %m");
			return(0);
		} else if (0 == ssz && sz > 0) {
			syslog(LOG_ERR, "read: receiving "
				"ack from worker: short read");
			return(0);
		} else if (0 == ssz && sz == 0) {
			syslog(LOG_ERR, "read: receiving "
				"ack from worker: EOF");
			return(0);
		} else if (sz > SIZE_MAX - (size_t)ssz) {
			syslog(LOG_ERR, "read: receiving "
				"ack from worker: overflow");
			return(0);
		}
	}

	return(1);
}

/*
 * Create a non-blocking socket pair.
 * Remove 0 on failure, 1 on success.
 */
static int
xsocketpair(int *sock)
{
	int	 rc, fl1, fl2;

	sock[0] = sock[1] = -1;
	rc = socketpair(AF_UNIX, SOCK_STREAM, 0, sock);
	if (-1 == rc)
		syslog(LOG_ERR, "socketpair: "
			"connection to worker: %m");
	else if (-1 == (fl1 = fcntl(sock[0], F_GETFL, 0)))
		syslog(LOG_ERR, "fcntl: F_GETFL: "
			"connection to worker: %m");
	else if (-1 == (fl2 = fcntl(sock[1], F_GETFL, 0)))
		syslog(LOG_ERR, "fcntl: F_GETFL: "
			"connection to worker: %m");
	else if (-1 == fcntl(sock[0], F_SETFL, fl1 | O_NONBLOCK))
		syslog(LOG_ERR, "fcntl: F_SETFL: "
			"connection to worker: %m");
	else if (-1 == fcntl(sock[1], F_SETFL, fl2 | O_NONBLOCK))
		syslog(LOG_ERR, "fcntl: F_SETFL: "
			"connection to worker: %m");
	else
		return(1);

	if (-1 != sock[0])
		close(sock[0]);
	if (-1 != sock[1])
		close(sock[1]);

	return(0);
}

/*
 * Start a worker for the variable pool.
 */
static int
varpool_start(struct worker *w, const struct worker *ws, 
	size_t wsz, int fd, char **nargv)
{
	int	 pair[2];
	char	 buf[64];
	size_t	 i;

	w->ctrl = w->fd = -1;
	w->pid = -1;

	if ( ! xsocketpair(pair))
		return(0);
	w->ctrl = pair[0];

	if (-1 == (w->pid = fork())) {
		syslog(LOG_ERR, "fork: worker: %m");
		close(pair[0]);
		close(pair[1]);
		w->ctrl = -1;
		return(0);
	} else if (0 == w->pid) {
		/* 
		 * Close out all current descriptors.
		 * Note that we're a "new-mode" FastCGI manager to the
		 * child via our environment variable.
		 */
		for (i = 0; i < wsz; i++)
			if (-1 != ws[i].ctrl && 
			    -1 == close(ws[i].ctrl))
				syslog(LOG_ERR, "close: "
					"worker cleanup: %m");

		close(fd);
		snprintf(buf, sizeof(buf), "%d", pair[1]);
		setenv("FCGI_LISTENSOCK_DESCRIPTORS", buf, 1);
		execv(nargv[0], nargv);
		syslog(LOG_ERR, "execv: %s: %m", nargv[0]);
		_exit(EXIT_FAILURE);
	}

	/* Close the child descriptor. */
	if (-1 == close(pair[1])) {
		syslog(LOG_ERR, "close: worker-%u pipe: %m", w->pid);
		return(0);
	}

	dbg("worker-%u: started", w->pid);
	return(1);
}

/*
 * A variable-sized pool of web application clients.
 * A minimum of "wsz" applications are always running, and will grow to
 * "maxwsz" at which point no new connections are accepted.
 * This is an EXPERIMENTAL extension.
 */
static int
varpool(size_t wsz, size_t maxwsz, time_t waittime,
	int fd, const char *sockpath, char *argv[])
{
	struct worker	*ws;
	struct worker	*slough;
	size_t		 pfdsz, opfdsz, pfdmaxsz, i, j, minwsz,
			 sloughsz, sloughmaxsz;
	int		 rc, exitcode, afd, accepting;
	struct pollfd	*pfd, *opfd;
	struct sockaddr_storage ss;
	socklen_t	 sslen;
	void		*pp;
	time_t		 t;
	sigset_t	 set;
	uint64_t	 cookie;

	dbg("YOU ARE RUNNING VARIABLE MODE AT YOUR OWN RISK.");

	signal(SIGCHLD, sighandlechld);
	signal(SIGTERM, sighandlestop);
	signal(SIGHUP, sighandlehup);
	sigemptyset(&set);
	sigaddset(&set, SIGCHLD);
	sigaddset(&set, SIGTERM);
	sigaddset(&set, SIGHUP);
	sigprocmask(SIG_BLOCK, &set, NULL);

again:
	stop = chld = hup = 0;

	/* 
	 * Allocate worker array, polling descriptor array, and slough
	 * array (exiting workers).
	 * Set our initial exit code and our acceptance status.
	 */
	minwsz = wsz;
	ws = calloc(wsz, sizeof(struct worker));
	sloughmaxsz = (maxwsz - minwsz) * 2;
	slough = calloc(sloughmaxsz, sizeof(struct worker));
	pfdmaxsz = wsz + 1;
	pfd = calloc(pfdmaxsz, sizeof(struct pollfd));
	exitcode = 0;

	if (NULL == ws || NULL == pfd || NULL == slough) {
		/* Serious problems. */
		syslog(LOG_ERR, "calloc: initialisation: %m");
		close(fd);
		unlink(sockpath);
		free(ws);
		free(slough);
		free(pfd);
		return(0);
	}

	/* Zeroth pfd is always the domain socket. */
	pfd[0].fd = fd;
	pfd[0].events = POLLIN;

	/* Guard against bad de-allocation. */
	for (i = 0; i < wsz; i++) {
		ws[i].ctrl = ws[i].fd = -1;
		ws[i].pid = -1;
	}

	sloughsz = 0;
	accepting = 1;
	pfdsz = 1;

	/*
	 * Start up the [initial] worker processes.
	 * We'll later spin up workers when we need them.
	 * If these die immediately, we'll find out below when we
	 * unblock our signals.
	 */
	for (i = 0; i < wsz; i++)
		if ( ! varpool_start(&ws[i], ws, wsz, fd, argv))
			goto out;
pollagain:
	/*
	 * Main part.
	 * Poll on our control socket (pfd[0]) and the children that
	 * have active connections.
	 * If we're not accepting new connections, avoid polling on the
	 * first entry by offsetting.
	 * We have a timeout if we get a termination signal whilst
	 * waiting.
	 */
	opfd = accepting ? pfd : pfd + 1;
	opfdsz = accepting ? pfdsz : pfdsz - 1;

	sigprocmask(SIG_UNBLOCK, &set, NULL);
	rc = poll(opfd, opfdsz, 1000);
	sigprocmask(SIG_BLOCK, &set, NULL);

	/* Did an error occur? */
	if (rc < 0 && EINTR != errno) {
		syslog(LOG_ERR, "poll: main event: %m");
		goto out;
	} 

	if (stop) {
		/* 
		 * If we're being requested to stop, go to exit now. 
		 * We'll immediately kill off our children and wait.
		 */
		dbg("servicing exit request");
		exitcode = 1;
		goto out;
	} else if (chld) {
		/*
		 * A child has exited.
		 * This can mean one of two things: either a worker has
		 * exited abnormally or one of the "sloughed" workers
		 * has finished its exit.
		 */
		if (0 == sloughsz) {
			syslog(LOG_ERR, "worker unexpectedly exited");
			goto out;
		}

		/* Look at the running children. */
		for (i = 0; i < wsz; i++) {
			assert(-1 != ws[i].pid);
			rc = waitpid(ws[i].pid, NULL, WNOHANG);
			if (rc < 0) {
				syslog(LOG_ERR, "wait: worker-%u "
					"check: %m", ws[i].pid);
				goto out;
			} else if (0 == rc)
				continue;
			/*
			 * We found a process that has already exited.
			 * Close its fd as well so that we know that the
			 * pid of -1 means it's completely finished.
			 */
			syslog(LOG_ERR, "worker-%u "
				"unexpectedly exited", ws[i].pid);
			if (-1 == close(ws[i].ctrl))
				syslog(LOG_ERR, "close: worker-%u "
					"control socket: %m",
					ws[wsz - 1].pid);
			ws[i].pid = -1;
			goto out;
		}

		/* Ok... an exiting child can be reaped. */
		chld = 0;
		for (i = 0; i < sloughsz; ) {
			rc = waitpid(slough[i].pid, NULL, WNOHANG);
			if (0 == rc) {
				i++;
				continue;
			} else if (rc < 0) {
				syslog(LOG_ERR, "wait: sloughed "
					"worker-%u check: %m", 
					slough[i].pid);
				goto out;
			}
			dbg("slough: releasing worker-%u\n", 
				slough[i].pid);
			if (i < sloughsz - 1)
				slough[i] = slough[sloughsz - 1];
			sloughsz--;
		}
	} else if (hup) {
		dbg("servicing restart request");
		goto out;
	}

	/*
	 * See if we should reduce our pool size.
	 * We only do this if we've grown beyond the minimum.
	 */
	if (wsz > minwsz) {
		t = time(NULL);
		/*
		 * We only reduce if the last worker is (1) not running
		 * and (2) was last descheduled a while ago.
		 */
		assert(-1 != ws[wsz - 1].fd || ws[wsz - 1].last);
		assert(wsz > 1);

		if (-1 == ws[wsz - 1].fd &&
		    t - ws[wsz - 1].last > waittime) {
			assert(-1 != ws[wsz - 1].ctrl);
			assert(-1 != ws[wsz - 1].pid);
			assert(-1 == ws[wsz - 1].fd);
			assert(-1 == pfd[pfdmaxsz - 1].fd);

			if (sloughsz >= sloughmaxsz) {
				syslog(LOG_ERR, "slough pool "
					"maximum size reached");
				goto out;
			}
			
			/* Close down the worker in the usual way. */
			if (-1 == close(ws[wsz - 1].ctrl)) {
				syslog(LOG_ERR, "close: worker-%u "
					"control socket: %m",
					ws[wsz - 1].pid);
				goto out;
			} 
			if (-1 == kill(ws[wsz - 1].pid, SIGTERM)) {
				syslog(LOG_ERR, "kill: worker-%u: %m",
					ws[wsz - 1].pid);
				goto out;
			}

			/* 
			 * Append the dying client to the slough array,
			 * since workers may take time to die.
			 */
			dbg("slough: acquiring worker-%u\n", 
				ws[wsz - 1].pid);
			slough[sloughsz++] = ws[wsz - 1];

			/* Reallocations. */
			pp = reallocarray(ws, wsz - 1, 
				sizeof(struct worker));
			if (NULL == pp) {
				syslog(LOG_ERR, "reallocarray: "
					"worker array: %m");
				goto out;
			}
			ws = pp;
			wsz--;
			pp = reallocarray(pfd, pfdmaxsz - 1,
				sizeof(struct pollfd));
			if (NULL == pp) {
				syslog(LOG_ERR, "reallocarray: "
					"descriptor array: %m");
				goto out;
			}
			pfd = pp;
			pfdmaxsz--;
		}
	}
	
	if (0 == rc)
		goto pollagain;

	/*
	 * Now we see which of the workers has exited.
	 * We do this until we've processed all of them.
	 */
	assert(rc > 0);
	for (i = 1; i < pfdsz && rc > 0; ) {
		if (POLLHUP & pfd[i].revents ||
		    POLLERR & pfd[i].revents) {
			syslog(LOG_ERR, "poll: worker disconnect");
			goto out;
		} else if ( ! (POLLIN & pfd[i].revents)) {
			i++;
			continue;
		} 

		/* 
		 * Read the "identifier" that the child process gives
		 * to us.
		 */
		if ( ! fullread(pfd[i].fd, &cookie, sizeof(uint64_t)))
			goto out;

		/*
		 * First, look up the worker that's now free.
		 * Mark its active descriptor as free.
		 * TODO: use a queue of "working" processes to prevent
		 * this from being so expensive.
		 */
		for (j = 0; j < wsz; j++)
			if (ws[j].cookie == cookie)
				break;

		if (j == wsz) {
			syslog(LOG_ERR, "poll: bad worker response");
			goto out;
		}

		dbg("worker-%u: release %d", ws[j].pid, ws[j].fd);

		/*
		 * Close the descriptor (that we still hold) and mark
		 * this worker as no longer working.
		 */
		rc--;
		close(ws[j].fd);
		if (0 == accepting) {
			accepting = 1;
			dbg("rate-limiting: disabled");
		}
		ws[j].fd = -1;
		ws[j].last = time(NULL);

		/*
		 * Now, clear the active descriptor from the file
		 * descriptor array.
		 * Do so by flipping it into the last slot then
		 * truncating the array size.
		 * Obviously, we only do this if we're not the current
		 * end of array...
		 */
		if (pfdsz - 1 != i)
			pfd[i] = pfd[pfdsz - 1];
		pfd[pfdsz - 1].fd = -1;
		pfdsz--;
	}

	if (0 == accepting)
		goto pollagain;

	if (POLLHUP & pfd[0].revents) {
		syslog(LOG_ERR, "poll: control hangup");
		goto out;
	} else if (POLLERR & pfd[0].revents) {
		syslog(LOG_ERR, "poll: control error?");
		goto out;
	} else if ( ! (POLLIN & pfd[0].revents))
		goto pollagain;

	/* 
	 * We have a new request.
	 * First, see if we need to allocate more workers.
	 */
	if (pfdsz == pfdmaxsz) {
		if (wsz + 1 > maxwsz) {
			accepting = 0;
			dbg("rate-limiting: enabled");
			goto pollagain;
		}
		pp = reallocarray(pfd, pfdmaxsz + 1,
			sizeof(struct pollfd));
		if (NULL == pp) {
			syslog(LOG_ERR, "reallocarray: workers: %m");
			goto out;
		}
		pfd = pp;
		memset(&pfd[pfdmaxsz], 0, sizeof(struct pollfd));
		pfd[pfdmaxsz].fd = -1;

		pp = reallocarray(ws, wsz + 1,
			sizeof(struct worker));
		if (NULL == pp) {
			syslog(LOG_ERR, "reallocarray: descriptors: %m");
			goto out;
		}
		ws = pp;
		memset(&ws[wsz], 0, sizeof(struct worker));

		if ( ! varpool_start(&ws[wsz], ws, wsz + 1, fd, argv))
			goto out;
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
		if (EAGAIN == errno || 
		    EWOULDBLOCK == errno)
			goto pollagain;
		syslog(LOG_ERR, "accept: new connection: %m");
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
#if HAVE_ARC4RANDOM
	ws[i].cookie = arc4random();
#else
	ws[i].cookie = random();
#endif
	dbg("worker-%u: acquire %d "
		"(pollers %zu/%zu: workers %zu/%zu)", 
		ws[i].pid, afd, pfdsz, pfdmaxsz, wsz, maxwsz);
	pfd[pfdsz].events = POLLIN;
	pfd[pfdsz].fd = ws[i].ctrl;
	pfdsz++;

	if (fullwritefd(ws[i].ctrl, ws[i].fd, &ws[i].cookie, sizeof(uint64_t)))
		goto pollagain;

out:
	if ( ! hup) {
		/*
		 * If we're not going to restart, then close the FastCGI
		 * file descriptor as soon as possible.
		 */
		dbg("closing control socket");
		if (-1 == close(fd))
			syslog(LOG_ERR, "close: control: %m");
		fd = -1;
	}

	/*
	 * Close the application's control socket; then, if that doesn't
	 * make it exit (kcgi(3) will, but other applications may not),
	 * we also deliver a SIGTERM.
	 */
	for (i = 0; i < wsz; i++) {
		if (-1 == ws[i].pid)
			continue;
		dbg("worker-%u: terminating", ws[i].pid);
		if (-1 == close(ws[i].ctrl))
			syslog(LOG_ERR, "close: "
				"worker-%u control: %m", ws[i].pid);
		if (-1 == kill(ws[i].pid, SIGTERM))
			syslog(LOG_ERR, "kill: "
				"worker-%u: %m", ws[i].pid);
	}

	/*
	 * Now wait for the children and pending children.
	 */
	for (i = 0; i < wsz; i++) {
		if (-1 == ws[i].pid)
			continue;
		dbg("worker-%u: reaping", ws[i].pid);
		if (-1 == waitpid(ws[i].pid, NULL, 0))
			syslog(LOG_ERR, "wait: "
				"worker-%u: %m", ws[i].pid);
	}

	for (i = 0; i < sloughsz; i++) {
		dbg("sloughed worker-%u: reaping", slough[i].pid);
		if (-1 == waitpid(slough[i].pid, NULL, 0))
			syslog(LOG_ERR, "wait: sloughed "
				"worker-%u: %m", slough[i].pid);
	}

	free(ws);
	free(slough);
	free(pfd);

	if (hup)
		goto again;

	/* 
	 * Now we're really exiting.
	 * Do our final cleanup if we didn't already...
	 */
	if (-1 != fd) {
		dbg("closing control socket");
		if (-1 == close(fd))
			syslog(LOG_ERR, "close: control: %m");
	}
	return(exitcode);
}

static int
fixedpool(size_t wsz, int fd, const char *sockpath, char *argv[])
{
	pid_t		 *ws;
	size_t		  i;
	sigset_t	  set, oset;
	void 		(*sigfp)(int);

	/*
	 * Dying children should notify us that something is horribly
	 * wrong and we should exit.
	 * Also handle SIGTERM in the same way.
	 */
	signal(SIGTERM, sighandlestop);
	signal(SIGCHLD, sighandlechld);
	signal(SIGHUP, sighandlehup);
	sigemptyset(&set);
	sigaddset(&set, SIGCHLD);
	sigaddset(&set, SIGTERM);
	sigaddset(&set, SIGHUP);
	sigprocmask(SIG_BLOCK, &set, &oset);

again:
	hup = stop = chld = 0;

	/* Allocate worker array. */
	if (NULL == (ws = calloc(wsz, sizeof(pid_t)))) {
		syslog(LOG_ERR, "calloc: initialisation: %m");
		close(fd);
		unlink(sockpath);
		return(0);
	}

	/* 
	 * "Zero" the workers.
	 * This is in case the initialisation fails.
	 */
	for (i = 0; i < wsz; i++)
		ws[i] = -1;

	for (i = 0; i < wsz; i++) {
		if (-1 == (ws[i] = fork())) {
			syslog(LOG_ERR, "fork: worker: %m");
			goto out;
		} else if (0 == ws[i]) {
			/*
			 * Assign stdin to be the socket over which
			 * we're going to transfer request descriptors
			 * when we get them.
			 */
			if (-1 == dup2(fd, STDIN_FILENO)) {
				syslog(LOG_ERR, "dup2: worker: %m");
				_exit(EXIT_FAILURE);
			}
			close(fd);
			execv(argv[0], argv);
			syslog(LOG_ERR, "execve: %s: %m", argv[0]);
			_exit(EXIT_FAILURE);
		}
	}

	sigsuspend(&oset);

	if (stop)
		dbg("servicing exit request");
	else if (chld)
		syslog(LOG_ERR, "worker unexpectedly exited");
	else 
		dbg("servicing restart request");
out:
	if ( ! hup) {
		/*
		 * If we're not going to restart, then close the FastCGI
		 * file descriptor as soon as possible.
		 */
		if (-1 == close(fd))
			syslog(LOG_ERR, "close: control: %m");
		fd = -1;
	}

	/* Suppress child exit signals whilst we kill them. */
	sigfp = signal(SIGCHLD, SIG_DFL);

	/*
	 * Now wait on the children.
	 * This can take forever, but properly-written children will
	 * exit when receiving SIGTERM.
	 */
	for (i = 0; i < wsz; i++)
		if (-1 != ws[i] && -1 == kill(ws[i], SIGTERM))
			syslog(LOG_ERR, "kill: worker-%u: %m", ws[i]);

	for (i = 0; i < wsz; i++)
		if (-1 != ws[i] && -1 == waitpid(ws[i], NULL, 0))
			syslog(LOG_ERR, "wait: worker-%u: %m", ws[i]);

	signal(SIGCHLD, sigfp);

	free(ws);

	if (hup)
		goto again;

	/* 
	 * Now we're really exiting.
	 * Do our final cleanup if we didn't already...
	 */
	if (-1 != fd && -1 == close(fd))
		syslog(LOG_ERR, "close: control: %m");
	return(1);
}

int
main(int argc, char *argv[])
{
	int			  c, fd, varp, usemax, useq, nod;
	struct passwd		 *pw;
	size_t			  i, wsz, sz, lsz, maxwsz;
	time_t			  waittime;
	const char		 *pname, *sockpath, *chpath,
	      			 *sockuser, *procuser, *errstr;
	struct sockaddr_un	  sun;
	mode_t			  old_umask;
	uid_t		 	  sockuid, procuid;
	gid_t			  sockgid, procgid;
	char			**nargv;

	if ((pname = strrchr(argv[0], '/')) == NULL)
		pname = argv[0];
	else
		++pname;

	if (0 != geteuid()) {
		fprintf(stderr, "%s: need root privileges\n", pname);
		return(EXIT_FAILURE);
	}

	sockuid = procuid = sockgid = procgid = -1;
	wsz = 5;
	usemax = useq = 0;
	sockpath = "/var/www/run/httpd.sock";
	chpath = "/var/www";
	sockuser = procuser = NULL;
	varp = 0;
	nod = 0;
	maxwsz = lsz = 0;
	waittime = 60 * 5;

	while (-1 != (c = getopt(argc, argv, "l:p:n:N:s:u:U:rvdw:")))
		switch (c) {
		case ('l'):
			useq = 1;
			lsz = strtonum(optarg, 1, INT_MAX, &errstr);
			if (NULL == errstr)
				break;
			fprintf(stderr, "-l must be "
				"between 1 and %d\n", INT_MAX);
			return(EXIT_FAILURE);	
		case ('n'):
			wsz = strtonum(optarg, 0, INT_MAX, &errstr);
			if (NULL == errstr)
				break;
			fprintf(stderr, "-n must be "
				"between 0 and %d\n", INT_MAX);
			return(EXIT_FAILURE);	
		case ('N'):
			usemax = 1;
			maxwsz = strtonum(optarg, 0, INT_MAX, &errstr);
			if (NULL == errstr)
				break;
			fprintf(stderr, "-N must be "
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
		case ('r'):
			varp = 1;
			break;	
		case ('v'):
			verbose = 1;
			break;	
		case ('d'):
			nod = 1;
			break;	
		case ('w'):
			waittime = strtonum(optarg, 0, INT_MAX, &errstr);
			if (NULL == errstr)
				break;
			fprintf(stderr, "-w must be "
				"between 0 and %d\n", INT_MAX);
			return(EXIT_FAILURE);	
		default:
			goto usage;
		}

	argc -= optind;
	argv += optind;

	if (0 == argc) 
		goto usage;

	if (usemax && varp) {
		if (maxwsz == wsz)
			varp = 0;
		else if (maxwsz < wsz)
			goto usage;
	} else
		maxwsz = wsz * 2;

	if (0 == useq)
		lsz = (varp ? maxwsz : wsz) * 2;

	assert(lsz);

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

	if (-1 == listen(fd, lsz)) {
		perror(sockpath);
		close(fd);
		return(EXIT_FAILURE);
	}

	/* 
	 * Jail our file-system.
	 */
	if (-1 == chroot(chpath)) {
		perror("chroot");
		close(fd);
		return(EXIT_FAILURE);
	} else if (-1 == chdir("/")) {
		perror("chdir");
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

	nargv = calloc(argc + 1, sizeof(char *));
	if (NULL == nargv) {
		perror(NULL);
		close(fd);
		return(EXIT_FAILURE);
	}

	for (i = 0; i < (size_t)argc; i++)
		nargv[i] = argv[i];

	if ( ! nod && -1 == daemon(1, 0)) {
		perror("daemon");
		close(fd);
		unlink(sockpath);
		free(nargv);
		return(EXIT_FAILURE);
	} 
	
	if (nod)
		openlog(pname, LOG_PERROR | LOG_PID, LOG_DAEMON);
	else
		openlog(pname, LOG_PID, LOG_DAEMON);

	c = varp ?
		varpool(wsz, maxwsz, waittime, fd, sockpath, nargv) :
		fixedpool(wsz, fd, sockpath, nargv);

	free(nargv);
	return(c ? EXIT_SUCCESS : EXIT_FAILURE);
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
