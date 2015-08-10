/*	$Id$ */
/*
 * Copyright (c) 2012, 2014, 2015 Kristaps Dzonsons <kristaps@bsd.lv>
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
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <sys/wait.h>

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "kcgi.h"
#include "extern.h"

int
xvasprintf(const char *file, int line, 
	char **p, const char *fmt, va_list ap)
{
	int	 len;

	if (-1 != (len = vasprintf(p, fmt, ap)))
		return(len);

	xwarn(file, line, "vasprintf");
	return(-1);
}

int
xasprintf(const char *file, int line, char **p, const char *fmt, ...)
{
	va_list	 ap;
	int	 len;

	va_start(ap, fmt);
	len = vasprintf(p, fmt, ap);
	va_end(ap);
	if (len != -1)
		return(len);

	xwarn(file, line, "vasprintf");
	return(-1);
}

void *
xcalloc(const char *file, int line, size_t nm, size_t sz)
{
	void	 *p;

	if (NULL != (p = calloc(nm, sz)))
		return(p);
	xwarn(file, line, "calloc(%zu, %zu)", nm, sz);
	return(p);
}

void *
xmalloc(const char *file, int line, size_t sz)
{
	void	 *p;

	if (NULL != (p = malloc(sz)))
		return(p);
	xwarn(file, line, "malloc(%zu)", sz);
	return(p);
}

void *
xrealloc(const char *file, int line, void *pp, size_t sz)
{
	void	 *p;

	if (NULL != (p = realloc(pp, sz)))
		return(p);
	xwarn(file, line, "realloc(%p, %zu)", pp, sz);
	return(p);
}

void *
xreallocarray(const char *file, 
	int line, void *pp, size_t nm, size_t sz)
{
	void	 *p;

	if (NULL != (p = reallocarray(pp, nm, sz)))
		return(p);
	xwarn(file, line, "reallocarray(%p, %zu, %zu)", pp, nm, sz);
	return(p);
}

char *
xstrdup(const char *file, int line, const char *cp)
{
	char	*p;

	if (NULL != (p = strdup(cp)))
		return(p);
	xwarn(file, line, "strdup(%p)", cp);
	return(p);
}

void
xwarnx(const char *file, int line, const char *fmt, ...)
{
	char		buf[1024];
	va_list		ap;

	va_start(ap, fmt);
	(void)vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	fprintf(stderr, "%s:%d: %s\n", file, line, buf);
}

void
xwarn(const char *file, int line, const char *fmt, ...)
{
	int		e = errno;
	char		buf[1024];
	va_list		ap;

	va_start(ap, fmt);
	(void)vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	fprintf(stderr, "%s:%d: %s: %s\n", 
		file, line, buf, strerror(e));
}

enum kcgi_err
xwaitpid(pid_t pid)
{
	int	 	 st;
	enum kcgi_err	 ke;

	ke = KCGI_OK;

	if (-1 == waitpid(pid, &st, 0)) {
		ke = KCGI_SYSTEM;
		XWARN("waiting for child");
	} else if (WIFEXITED(st) && EXIT_SUCCESS != WEXITSTATUS(st)) {
		ke = KCGI_FORM;
		XWARNX("child status %d", WEXITSTATUS(st));
	} else if (WIFSIGNALED(st)) {
		ke = KCGI_FORM;
		XWARNX("child signal %d", WTERMSIG(st));
	}

	return(ke);
}

enum kcgi_err
xsocketprep(int sock)
{
	int	 fl;

	if (-1 == (fl = fcntl(sock, F_GETFL, 0))) {
		XWARN("fcntl");
		return(KCGI_SYSTEM);
	} else if (-1 == fcntl(sock, F_SETFL, fl | O_NONBLOCK)) {
		XWARN("fcntl");
		return(KCGI_SYSTEM);
	}

	return(KCGI_OK);
}

enum kcgi_err
xsocketpair(int domain, int type, int protocol, int *sock)
{
	int	 rc, fl1, fl2;

	rc = socketpair(domain, type, protocol, sock);
	if (-1 == rc && (EMFILE == errno || ENFILE == errno)) {
		XWARN("socketpair");
		return(KCGI_ENFILE);
	} else if (-1 == rc) {
		XWARN("socketpair");
		return(KCGI_SYSTEM);
	} else if (-1 == (fl1 = fcntl(sock[0], F_GETFL, 0)))
		XWARN("fcntl");
	else if (-1 == (fl2 = fcntl(sock[1], F_GETFL, 0)))
		XWARN("fcntl");
	else if (-1 == fcntl(sock[0], F_SETFL, fl1 | O_NONBLOCK))
		XWARN("fcntl");
	else if (-1 == fcntl(sock[1], F_SETFL, fl2 | O_NONBLOCK))
		XWARN("fcntl");
	else
		return(KCGI_OK);

	close(sock[0]);
	close(sock[1]);
	return(KCGI_SYSTEM);
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

/*
 * Write the full contents of "buf" to the non-blocking stream.
 * This will loop on "fd" until all data has been sent.
 * On error (which shouldn't happen), this will kill the process.
 */
void
fullwrite(int fd, const void *buf, size_t bufsz)
{
	ssize_t	 	 ssz;
	size_t	 	 sz;
	struct pollfd	 pfd;

	pfd.fd = fd;
	pfd.events = POLLOUT;

	for (sz = 0; sz < bufsz; sz += (size_t)ssz) {
		if (-1 == poll(&pfd, 1, -1))
			XWARN("poll: %d, POLLOUT", fd);
		else if (POLLHUP & pfd.revents)
			XWARNX("poll: POLLHUP");
		else if (POLLERR & pfd.revents)
			XWARNX("poll: POLLER");
		/*
		 * This CPP exists because testing on Mac OS X will have
		 * `fd' point to a device and poll(2) returns POLLNVAL if
		 * the descriptor is to a device.
		 * This is documented in the BUGS section of poll(2).
		 */
#ifdef __APPLE__
		else if ( ! (POLLOUT & pfd.revents) && ! (POLLNVAL & pfd.revents))
			XWARNX("poll: not POLLOUT");
#else
		else if ( ! (POLLOUT & pfd.revents))
			XWARNX("poll: not POLLOUT");
#endif
		else if ((ssz = write(fd, buf + sz, bufsz - sz)) < 0)
			XWARN("write: %d, %zu", fd, bufsz - sz);
		else if (sz > SIZE_MAX - (size_t)ssz)
			XWARNX("write: overflow: %zu, %zd", sz, ssz);
		else
			continue;
		_exit(EXIT_FAILURE);
	}
}

/*
 * Read in "bufsz" bytes and just discard them.
 * This will do so one byte at a time, so this shouldn't be used for any
 * large buffers.
 * Returns zero on failure, non-zero on success.
 */
int
fulldiscard(int fd, size_t bufsz, enum kcgi_err *er)
{
	ssize_t	 	 ssz;
	size_t	 	 sz;
	struct pollfd	 pfd;
	char		 buf;

	pfd.fd = fd;
	pfd.events = POLLIN;

	for (sz = 0; sz < bufsz; sz += (size_t)ssz) {
		if (-1 == poll(&pfd, 1, -1)) {
			XWARN("poll: %d, POLLIN", fd);
			*er = KCGI_SYSTEM;
		} else if ( ! (POLLIN & pfd.revents)) {
			XWARNX("poll: unexpected hup");
			*er = KCGI_FORM;
		} else if ((ssz = read(fd, &buf, 1)) < 0) {
			XWARN("read: %d, %zu", fd, bufsz - sz);
			*er = KCGI_SYSTEM;
		} else if (0 == ssz && sz > 0) {
			XWARN("read: short read");
			*er = KCGI_FORM;
		} else if (0 == ssz && sz == 0) {
			XWARNX("read: unexpected eof");
			*er = KCGI_FORM;
		} else if (sz > SIZE_MAX - (size_t)ssz) {
			XWARNX("read: overflow: %zu, %zd", sz, ssz);
			*er = KCGI_FORM;
		} else
			continue;

		return(0);
	}

	*er = KCGI_OK;
	return(1);
}

/*
 * Read the contents of buf, size bufsz, entirely, using non-blocking
 * reads.
 * If "eofok" is set, we return zero if there is no data to read (HUP on
 * descriptor or zero from read function).
 * If not (and moreover), this will exit with -1 on fatal errors (the
 * child didn't return enough data or we received an unexpected EOF),
 * otherwise 1.
 */
int
fullread(int fd, void *buf, size_t bufsz, int eofok, enum kcgi_err *er)
{
	ssize_t	 	 ssz;
	size_t	 	 sz;
	struct pollfd	 pfd;

	pfd.fd = fd;
	pfd.events = POLLIN;

	for (sz = 0; sz < bufsz; sz += (size_t)ssz) {
		if (-1 == poll(&pfd, 1, -1)) {
			XWARN("poll");
			*er = KCGI_SYSTEM;
			return(-1);
		} else if ( ! (POLLIN & pfd.revents)) {
			if (eofok && 0 == sz) {
				*er = KCGI_OK;
				return(0);
			}
			XWARNX("poll: hangup");
			*er = KCGI_FORM;
			return(-1);
		} else if ((ssz = read(fd, buf + sz, bufsz - sz)) < 0) {
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
		} else if (sz > SIZE_MAX - (size_t)ssz) {
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

int
fullwritefd(int fd, int sendfd, void *b, size_t bsz)
{
	struct msghdr	 msg;
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
	if (poll(&pfd, 1, -1) < 0) {
		XWARN("poll");
		return(-1);
	} else if ( ! (POLLOUT & pfd.revents)) {
		XWARNX("poll: hangup");
		return(-1);
	} else if (sendmsg(fd, &msg, 0) < 0) {
		XWARN("sendmsg");
		return(0);
	}
	return(1);
}

int 
fullreadfd(int fd, int *recvfd, void *b, size_t bsz)
{
	struct msghdr	 msg;
	char		 m_buffer[256];
	char 		 c_buffer[256];
	struct iovec	 io;
	struct cmsghdr	*cmsg;
	int		 rc;
	struct pollfd	 pfd;

	assert(bsz <= 256);

	memset(&msg, 0, sizeof(struct msghdr));
	memset(&io, 0, sizeof(struct iovec));

	io.iov_base = m_buffer;
	io.iov_len = sizeof(m_buffer);
	msg.msg_iov = &io;
	msg.msg_iovlen = 1;

	msg.msg_control = c_buffer;
	msg.msg_controllen = sizeof(c_buffer);

	pfd.fd = fd;
	pfd.events = POLLIN;
	if (poll(&pfd, 1, -1) < 0) {
		XWARN("poll");
		return(-1);
	} else if ( ! (POLLIN & pfd.revents)) {
		XWARNX("poll: hangup");
		return(0);
	} else if ((rc = recvmsg(fd, &msg, 0)) < 0) {
		XWARN("recvmsg");
		return(-1);
	} else if (0 == rc)
		return(0);


	memcpy(b, m_buffer, bsz);
	for (cmsg = CMSG_FIRSTHDR(&msg); NULL != cmsg;
		 cmsg = CMSG_NXTHDR(&msg, cmsg)) {
		if (cmsg->cmsg_len == CMSG_LEN(sizeof(int)) &&
		    cmsg->cmsg_level == SOL_SOCKET &&
		    cmsg->cmsg_type == SCM_RIGHTS) {
			*recvfd = *(int *)CMSG_DATA(cmsg);
			return(1);
		}
	}
	XWARNX("recvmsg: no SCM_RIGHTS!?");
	return(-1);
}
