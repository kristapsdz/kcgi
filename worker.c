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

#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "kcgi.h"
#include "extern.h"

enum kcgi_err
kworker_fcgi_init(struct kworker *p)
{
	enum kcgi_err	 er;

	if (KCGI_OK != (er = kworker_init(p)))
		return(er);
	p->input = -1;
	return(xsocketpair(AF_UNIX, SOCK_STREAM, 0, p->control));
}

enum kcgi_err
kworker_init(struct kworker *p)
{
	size_t		 i;
	int 	 	 sndbuf;
	socklen_t	 socksz;
	enum kcgi_err	 er;

	memset(p, 0, sizeof(struct kworker));
	p->pid = -1;
	p->sock[0] = p->sock[1] = -1;
	p->control[0] = p->control[1] = -1;
	p->input = STDIN_FILENO;

	/* Allocate the communication sockets. */
	er = xsocketpair(AF_UNIX, SOCK_STREAM, 0, p->sock);
	if (KCGI_OK != er)
		return(er);

	/* Allocate the sandbox. (FIXME: ENOMEM?) */
	p->sand = ksandbox_alloc();

	/* Enlarge the transfer buffer size. */
	/* FIXME: is this a good idea? */
	socksz = sizeof(sndbuf);
	for (i = 200; i > 0; i--) {
		sndbuf = (i + 1) * 1024;
		if (-1 != setsockopt(p->sock[KWORKER_READ], 
			SOL_SOCKET, SO_RCVBUF, &sndbuf, socksz) &&
			-1 != setsockopt(p->sock[KWORKER_WRITE], 
			SOL_SOCKET, SO_SNDBUF, &sndbuf, socksz))
			break;
		XWARN("sockopt");
	}

	return(KCGI_OK);
}

void
kworker_free(struct kworker *p)
{

	if (-1 != p->sock[0])
		close(p->sock[0]);
	if (-1 != p->sock[1])
		close(p->sock[1]);
	if (-1 != p->input)
		close(p->input);
	if (-1 != p->control[0])
		close(p->control[0]);
	if (-1 != p->control[1])
		close(p->control[1]);
	ksandbox_free(p->sand);
}

void
kworker_prep_child(struct kworker *p)
{

	if (-1 != p->sock[KWORKER_READ]) {
		close(p->sock[KWORKER_READ]);
		p->sock[KWORKER_READ] = -1;
	}
	if (-1 != p->control[KWORKER_WRITE]) {
		close(p->control[KWORKER_WRITE]);
		p->control[KWORKER_WRITE] = -1;
	}
	ksandbox_init_child(p->sand, p->sock[KWORKER_WRITE]);
}

void
kworker_prep_parent(struct kworker *p)
{

	close(p->sock[KWORKER_WRITE]);
	p->sock[KWORKER_WRITE] = -1;
	if (-1 != p->control[KWORKER_READ]) {
		close(p->control[KWORKER_READ]);
		p->control[KWORKER_READ] = -1;
	}
	ksandbox_init_parent(p->sand, p->pid);
	close(p->input);
	p->input = -1;
}

void
kworker_kill(struct kworker *p)
{

	if (-1 != p->pid)
		(void)kill(p->pid, SIGKILL);
}

enum kcgi_err
kworker_close(struct kworker *p)
{
	pid_t	 	 rp;
	int	 	 st;
	enum kcgi_err	 ke;

	ke = KCGI_OK;

	if (-1 == p->pid) {
		ksandbox_close(p->sand);
		return(ke);
	}

	do
		rp = waitpid(p->pid, &st, 0);
	while (rp == -1 && errno == EINTR);

	if (-1 == rp) {
		ke = KCGI_SYSTEM;
		XWARN("waiting for child");
	} else if (WIFEXITED(st) && EXIT_SUCCESS != WEXITSTATUS(st)) {
		ke = KCGI_FORM;
		XWARNX("child status %d", WEXITSTATUS(st));
	} else if (WIFSIGNALED(st)) {
		ke = KCGI_FORM;
		XWARNX("child signal %d", WTERMSIG(st));
	}

	ksandbox_close(p->sand);
	return(ke);
}

void
fullwriteword(int fd, const char *buf)
{
	size_t	 sz;

	if (NULL == buf) {
		sz = 0;
		fullwrite(fd, &sz, sizeof(size_t));
		return;
	}

	sz = strlen(buf);
	fullwrite(fd, &sz, sizeof(size_t));
	fullwrite(fd, buf, sz);
}

void
fullwrite(int fd, const void *buf, size_t bufsz)
{
	ssize_t	 	 ssz;
	size_t	 	 sz;
	struct pollfd	 pfd;

	pfd.fd = fd;
	pfd.events = POLLOUT;

	for (sz = 0; sz < bufsz; sz += (size_t)ssz) {
		if (-1 == poll(&pfd, 1, -1)) {
			XWARN("poll: %d, POLLOUT", fd);
			_exit(EXIT_FAILURE);
		}

		ssz = write(fd, buf + sz, bufsz - sz);
		if (ssz < 0 && EAGAIN == errno) {
			XWARN("write: trying again");
			ssz = 0;
			continue;
		} else if (ssz < 0) {
			XWARN("write: %d, %zu", fd, bufsz - sz);
			_exit(EXIT_FAILURE);
		}
		/* Additive overflow check. */
		if (sz > SIZE_MAX - (size_t)ssz) {
			XWARNX("write: overflow: %zu, %zd", sz, ssz);
			_exit(EXIT_FAILURE);
		}
	}
}

int
fulldiscard(int fd, size_t bufsz, enum kcgi_err *er)
{
	ssize_t	 	 ssz;
	size_t	 	 sz;
	struct pollfd	 pfd;
	char		 buf;

	pfd.fd = fd;
	pfd.events = POLLIN;
	*er = KCGI_SYSTEM;

	for (sz = 0; sz < bufsz; sz += (size_t)ssz) {
		if (-1 == poll(&pfd, 1, -1)) {
			XWARN("poll: %d, POLLIN", fd);
			return(-1);
		}
		ssz = read(fd, &buf, 1);
		if (ssz < 0 && EAGAIN == errno) {
			XWARN("read: trying again");
			ssz = 0;
			continue;
		} else if (ssz < 0) {
			XWARN("read: %d, %zu", fd, bufsz - sz);
			return(-1);
		} else if (0 == ssz && sz > 0) {
			XWARN("read: short read");
			*er = KCGI_FORM;
			return(-1);
		} else if (0 == ssz && sz == 0) {
			XWARNX("read: unexpected eof");
			*er = KCGI_FORM;
			return(-1);
		}

		/* Additive overflow check. */
		if (sz > SIZE_MAX - (size_t)ssz) {
			XWARNX("read: overflow: %zu, %zd", sz, ssz);
			*er = KCGI_FORM;
			return(-1);
		}
	}

	*er = KCGI_OK;
	return(1);
}

/*
 * Read the contents of buf, size bufsz, entirely, using non-blocking
 * reads (i.e., poll(2) then read(2)).
 * This will exit with -1 on fatal errors (the child didn't return
 * enough data or we received an unexpected EOF) or 0 on EOF (only if
 * it's allowed), otherwise 1.
 */
int
fullread(int fd, void *buf, size_t bufsz, int eofok, enum kcgi_err *er)
{
	ssize_t	 	 ssz;
	size_t	 	 sz;
	struct pollfd	 pfd;

	pfd.fd = fd;
	pfd.events = POLLIN;
	*er = KCGI_SYSTEM;

	for (sz = 0; sz < bufsz; sz += (size_t)ssz) {
		if (-1 == poll(&pfd, 1, -1)) {
			XWARN("poll: %d, POLLIN", fd);
			return(-1);
		}
		ssz = read(fd, buf + sz, bufsz - sz);
		if (ssz < 0 && EAGAIN == errno) {
			XWARN("read: trying again");
			ssz = 0;
			continue;
		} else if (ssz < 0) {
			XWARN("read: %d, %zu", fd, bufsz - sz);
			return(-1);
		} else if (0 == ssz && sz > 0) {
			XWARN("read: short read");
			*er = KCGI_FORM;
			return(-1);
		} else if (0 == ssz && sz == 0 && ! eofok) {
			XWARNX("read: unexpected eof");
			*er = KCGI_FORM;
			return(-1);
		} else if (0 == ssz && sz == 0 && eofok) {
			*er = KCGI_OK;
			return(0);
		}

		/* Additive overflow check. */
		if (sz > SIZE_MAX - (size_t)ssz) {
			XWARNX("read: overflow: %zu, %zd", sz, ssz);
			*er = KCGI_FORM;
			return(-1);
		}
	}

	*er = KCGI_OK;
	return(1);
}

/*
 * Read a word from the stream, which consists of the word size followed
 * by the word itself, not including the nil terminator.
 */
enum kcgi_err
fullreadword(int fd, char **cp)
{
	size_t	 	 sz;
	enum kcgi_err	 ke;

	if (fullread(fd, &sz, sizeof(size_t), 0, &ke) < 0)
		return(ke);

	if (NULL == (*cp = XMALLOC(sz + 1)))
		return(KCGI_ENOMEM);

	(*cp)[sz] = '\0';
	if (0 == sz)
		return(KCGI_OK);

	fullread(fd, *cp, sz, 0, &ke);
	return(ke);
}

