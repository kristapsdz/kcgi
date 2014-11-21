/*	$Id$ */
/*
 * Copyright (c) 2012, 2014 Kristaps Dzonsons <kristaps@bsd.lv>
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

#include <fcntl.h>
#include <errno.h>
#include <poll.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "kcgi.h"
#include "extern.h"

/*
 * Structure for a running sandbox.
 */
struct	ksandbox {
	int	 fd; /* communicator within sandbox */
	pid_t	 pid; /* pid of sandbox */
};

#if 0
union cmsgbuf {
	struct cmsghdr	 hdr;
	unsigned char	 buf[CMSG_SPACE(sizeof(int))];
};
#endif

/*
 * Perform system-specific initialisation for the parent.
 * This is only used by systrace(4), which requires the parent to
 * register the child's systrace hooks.
 */
void
ksandbox_init_parent(void *arg, pid_t child)
{

#if defined(HAVE_SYSTRACE)
	if ( ! ksandbox_systrace_init_parent(arg, child))
		XWARNX("systrace sandbox failed (parent)");
#endif
}

/*
 * Allocate system-specific data for the sandbox.
 * This is only used by systrace, which requires extra accounting
 * information for the child.
 */
void *
ksandbox_alloc(void)
{
	void	*p = NULL;

#ifdef HAVE_SYSTRACE
	if (NULL == (p = (ksandbox_systrace_alloc())))
		XWARNX("systrace alloc failed");
#endif
	return(p);
}

void
ksandbox_free(void *arg)
{
	
	/* This is either NULL of something allocated. */
	free(arg);
}

/*
 * Close the sandbox from the parent.
 * This consists of waiting for the child to exit, then running any
 * system-specific exit routines (right now only systrace).
 */
void
ksandbox_close(void *arg, pid_t pid)
{
	pid_t	 rp;
	int	 st;

	do
		rp = waitpid(pid, &st, 0);
	while (rp == -1 && errno == EINTR);

	if (-1 == rp)
		XWARN("waiting for child");
	else if (WIFEXITED(st) && EXIT_SUCCESS != WEXITSTATUS(st))
		XWARNX("child status %d", WEXITSTATUS(st));
	else if (WIFSIGNALED(st))
		XWARNX("child signal %d", WTERMSIG(st));

	/* Now run system-specific closure stuff. */
#ifdef HAVE_SYSTRACE
	ksandbox_systrace_close(arg);
#endif
}

/*
 * Initialise the child context of a sandbox.
 * Each sandbox will want to do something here to make sure that the
 * child context is sandboxed properly.
 */
void
ksandbox_init_child(void *arg, int fd __unused)
{

#if defined(HAVE_CAPSICUM)
	if ( ! ksandbox_capsicum_init_child(arg, fd))
		XWARNX("capsicum sandbox failed (child)");
#elif defined(HAVE_SANDBOX_INIT)
	if ( ! ksandbox_darwin_init_child(arg))
		XWARNX("darwin sandbox failed (child)");
#elif defined(HAVE_SYSTRACE)
	if ( ! ksandbox_systrace_init_child(arg))
		XWARNX("systrace sandbox failed (child)");
#endif
}

/*===================================================================*/
/* From here on we have our experimental stuff.                      */
/*===================================================================*/

#if 0
/*
 * Notify the sandbox monitor that we want to invoke a sandboxed
 * command.
 * Then wait for the response which consists of the transferred file
 * descriptor for communicating with the child.
 */
int
ksandbox_notify(const struct ksandbox *p, size_t pos)
{
	ssize_t		 ssz;
	struct msghdr	 msg;
	struct cmsghdr	*cmsg;
	union cmsgbuf	 buf;

	/* 
	 * Write the notification to the child.
	 */
	ssz = write(p->fd, &pos, sizeof(size_t));
	if (-1 == ssz) {
		XWARN("write");
		return(-1);
	} else if (ssz != sizeof(size_t)) {
		XWARNX("write: short write");
		return(-1);
	}

	/*
	 * Set up our message control buffer and fill the buffer from
	 * the child, checking for truncation.
	 */
	memset(&msg, 0, sizeof(struct msghdr));
	msg.msg_control = &buf.buf;
	msg.msg_controllen = sizeof(buf.buf);

	if (-1 == recvmsg(p->fd, &msg, 0)) {
		XWARN("recvmsg");
		return(-1);
	} else if ((msg.msg_flags & MSG_TRUNC) || 
		(msg.msg_flags & MSG_CTRUNC)) {
		XWARNX("control message truncation");
		return(-1);
	}

	/*
	 * Loop through message headers.
	 * Respond with the first one that's a transfer of rights.
	 * FIXME: we should only take the first one.
	 */
	for (cmsg = CMSG_FIRSTHDR(&msg); 
			cmsg != NULL;
			cmsg = CMSG_NXTHDR(&msg, cmsg)) {
		if (cmsg->cmsg_len != CMSG_LEN(sizeof(int)))
			continue;
		if (cmsg->cmsg_level != SOL_SOCKET)
			continue;
		if (cmsg->cmsg_type != SCM_RIGHTS)
			continue;
		return(*(int *)CMSG_DATA(cmsg));
	}

	XWARNX("no descriptor in control message");
	return(-1);
}

void
ksandbox_destroy(struct ksandbox *p)
{
	pid_t	 pid;
	int	 st;

	close(p->fd);

	do
		pid = waitpid(p->pid, &st, 0);
	while (pid == -1 && errno == EINTR);

	if (-1 == pid)
		XWARN("waiting for child");
	else if (WIFEXITED(st) && EXIT_SUCCESS != WEXITSTATUS(st))
		XWARNX("child status %d", WEXITSTATUS(st));
	else if (WIFSIGNALED(st))
		XWARNX("child signal %d", WTERMSIG(st));
}

/*
 * Accept an array of function pointers that can be invoked in a
 * sandboxed child.
 * Fork a monitor (child) process and return parent control to the
 * caller.
 * The monitor will wait for notification (see ksandbox_notify()) of a
 * function index at which point it will fork a sandboxed child to
 * perform the given function.
 * A socketpair will be transferred from the monitor to the caller
 * allowing for interprocess communication.
 */
struct ksandbox * 
ksandbox_monitor(void (**fps)(int fd), size_t sz)
{
	int	 	 fds[2], cfds[2];
	struct ksandbox	*p;
	int	 	 rc = EXIT_FAILURE;
	struct pollfd	 pfd;
	pid_t	 	 pid;
	size_t	 	 pos;
	ssize_t	 	 ssz;
	struct msghdr	 msg;
	struct cmsghdr	*cmsg;
	union cmsgbuf	 buf;

	if (NULL == (p = XMALLOC(sizeof(struct ksandbox))))
		return(NULL);

	/*
	 * Set up the communication between us and our monitor.
	 * The child descriptor must be non-blocking to allow the poll
	 * on parent input.
	 */
	if (-1 == socketpair(AF_UNIX, SOCK_STREAM, 0, fds)) {
		XWARN("socketpair");
		return(NULL);
	} else if (-1 == fcntl(fds[1], F_SETFL, O_NONBLOCK)) {
		XWARN("fcntl: O_NONBLOCK");
		close(fds[0]);
		close(fds[1]);
		return(NULL);
	} else if (-1 == (pid = fork())) {
		XWARN("fork");
		close(fds[0]);
		close(fds[1]);
		return(NULL);
	} else if (pid > 0) {
		close(fds[1]);
		p->fd = fds[0];
		p->pid = pid;
		return(p);
	}

	free(p);

	/*
	 * We're in the child process, which monitors for input from the
	 * parent and responds in forking off workers.
	 */
	close(fds[0]);
	pfd.events = POLLIN;
	pfd.fd = fds[1];

	while (-1 != poll(&pfd, 1, -1)) {
		/*
		 * Check first for whether the parent is waking us up
		 * with a function we must invoke.
		 * If so, set us as non-blocking, read the function
		 * identifier, then continue preparing the worker.
		 */
		if (POLLIN & pfd.revents) {
			if (-1 == fcntl(pfd.fd, F_SETFL, 0)) {
				XWARN("fcntl: 0");
				goto out;
			}
			ssz = read(pfd.fd, &pos, sizeof(size_t));
			if (-1 == ssz) {
				XWARN("read");
				goto out;
			} else if (pos >= sz) {
				XWARNX("function out of bounds");
				goto out;
			}

			if (-1 == socketpair(AF_UNIX, SOCK_STREAM, 0, cfds)) {
				XWARN("socketpair");
				goto out;
			} else if (-1 == (pid = fork())) {
				XWARN("fork");
				goto out;
			} else if (0 == pid) {
				close(fds[1]);
				(*fps[pos])(fds[0]);
				_exit(EXIT_SUCCESS);
			}

			close(fds[0]);

			memset(&msg, 0, sizeof(msg));
			msg.msg_control = &buf.buf;
			msg.msg_controllen = sizeof(buf.buf);

			cmsg = CMSG_FIRSTHDR(&msg);
			cmsg->cmsg_len = CMSG_LEN(sizeof(int));
			cmsg->cmsg_level = SOL_SOCKET;
			cmsg->cmsg_type = SCM_RIGHTS;
			*(int *)CMSG_DATA(cmsg) = fds[1];

			if (-1 == sendmsg(pfd.fd, &msg, 0)) {
				XWARN("sendmsg");
				goto out;
			}
			if (-1 == fcntl(pfd.fd, F_SETFL, O_NONBLOCK)) {
				XWARN("fcntl: O_NONBLOCK");
				goto out;
			}
		}
		/*
		 * Exit conditions: POLLHUP means that it's time for us
		 * to exit.
		 * Errors cause us to exit regardless.
		 */
		if (POLLHUP & pfd.revents) {
			rc = EXIT_SUCCESS;
			goto out;
		}
		if (POLLERR & pfd.revents) {
			XWARNX("poll: POLLERR");
			goto out;
		}
		if (POLLNVAL & pfd.revents) {
			XWARNX("poll: POLLNVAL");
			goto out;
		}
	}
	XWARN("poll");
out:
	close(pfd.fd);
	_exit(rc);
}
#endif
