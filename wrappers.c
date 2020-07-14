/*	$Id$ */
/*
 * Copyright (c) 2012, 2014--2017 Kristaps Dzonsons <kristaps@bsd.lv>
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

#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <sys/wait.h>

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "kcgi.h"
#include "extern.h"

int
kxvasprintf(const char *file, int line, 
	char **p, const char *fmt, va_list ap)
{
	int	 len;

	if ((len = vasprintf(p, fmt, ap)) >= 0)
		return len;

	kutil_warn(NULL, NULL, "vasprintf");
	*p = NULL;
	return -1;
}

int
kxasprintf(const char *file, int line, char **p, const char *fmt, ...)
{
	va_list	 ap;
	int	 ret;

	va_start(ap, fmt);
	ret = kxvasprintf(file, line, p, fmt, ap);
	va_end(ap);
	return ret;
}

void *
kxcalloc(const char *file, int line, size_t nm, size_t sz)
{
	void	 *p;

	if (nm == 0 || sz == 0)  {
		kutil_warnx(NULL, NULL, "calloc: zero length");
		return NULL;
	} else if ((p = calloc(nm, sz)) != NULL)
		return p;

	kutil_warn(NULL, NULL, "calloc: %zu, %zu", nm, sz);
	return NULL;
}

void *
kxmalloc(const char *file, int line, size_t sz)
{
	void	 *p;

	if (sz == 0) {
		kutil_warnx(NULL, NULL, "malloc: zero length");
		return NULL;
	} else if ((p = malloc(sz)) != NULL)
		return p;

	kutil_warn(NULL, NULL, "malloc: %zu", sz);
	return NULL;
}

void *
kxrealloc(const char *file, int line, void *pp, size_t sz)
{
	void	 *p;

	if (sz == 0) {
		kutil_warnx(NULL, NULL, "realloc: zero length");
		return NULL;
	} else if ((p = realloc(pp, sz)) != NULL)
		return p;

	kutil_warn(NULL, NULL, "realloc: %zu", sz);
	return NULL;
}

void *
kxreallocarray(const char *file, 
	int line, void *pp, size_t nm, size_t sz)
{
	void	 *p;

	if (sz == 0 || nm == 0) {
		kutil_warnx(NULL, NULL, "reallocarray: zero length");
		return NULL;
	} else if ((p = reallocarray(pp, nm, sz)) != NULL)
		return p;

	kutil_warn(NULL, NULL, "reallocarray: %zu, %zu", nm, sz);
	return NULL;
}

char *
kxstrdup(const char *file, int line, const char *cp)
{
	char	*p;

	if (cp == NULL) {
		kutil_warnx(NULL, NULL, "strdup: NULL string");
		return NULL;
	} else if ((p = strdup(cp)) != NULL)
		return p;

	kutil_warn(NULL, NULL, "strdup");
	return NULL;
}

/*
 * waitpid() and logging anything but a return with EXIT_SUCCESS.
 * Returns KCGI_OK on EXIT_SUCCESS, KCGI_SYSTEM on waitpid() error,
 * KCGI_FORM on child process failure.
 */
enum kcgi_err
kxwaitpid(pid_t pid)
{
	int	st;

	if (waitpid(pid, &st, 0) == -1) {
		kutil_warn(NULL, NULL, "waitpid");
		return KCGI_SYSTEM;
	} else if (WIFEXITED(st) && WEXITSTATUS(st) == EXIT_SUCCESS)
		return KCGI_OK;

	if (WIFEXITED(st))
		kutil_warnx(NULL, NULL, "waitpid: child failure");
	else 
		kutil_warnx(NULL, NULL, "waitpid: child signal");

	return KCGI_FORM;
}

/*
 * Set a file-descriptor as being non-blocking.
 * Returns KCGI_SYSTEM on error, KCGI_OK on success.
 */
enum kcgi_err
kxsocketprep(int sock)
{
	int	 fl;

	if ((fl = fcntl(sock, F_GETFL, 0)) == -1) {
		kutil_warn(NULL, NULL, "fcntl");
		return KCGI_SYSTEM;
	} else if (fcntl(sock, F_SETFL, fl | O_NONBLOCK) == -1) {
		kutil_warn(NULL, NULL, "fcntl");
		return KCGI_SYSTEM;
	}

	return KCGI_OK;
}

/*
 * Create a non-blockin gsocketpair.
 * Return KCGI_ENFILE on temporary failure, KCGI_SYSTEM on fatal error,
 * KCGI_OK on success.
 * Only sets "sock" on success.
 */
enum kcgi_err
kxsocketpair(int domain, int type, int protocol, int *sock)
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

/*
 * Given a string "buf", write first the length of the string (w/o NUL
 * terminator), then the string itself (again, w/o NUL terminator).
 * If "buf" is NULL, then write a zero-length string.
 * See fullreadword().
 */
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
 * This is like fullwrite() but it does not bail out on errors.
 * We need this for writing our response to a socket that may be closed
 * at any time, like the FastCGI one.
 * Returns KCGI_SYSTEM, KCGI_HUP, or KCGI_OK.
 * Ignores SIGPIPE, restoring it on exit.
 */
enum kcgi_err
fullwritenoerr(int fd, const void *buf, size_t bufsz)
{
	ssize_t	 	  ssz;
	size_t	 	  sz;
	struct pollfd	  pfd;
	int		  rc;
	enum kcgi_err	  er = KCGI_OK;
	void		(*sig)(int);

	pfd.fd = fd;
	pfd.events = POLLOUT;

	sig = signal(SIGPIPE, SIG_IGN);

	for (sz = 0; sz < bufsz; sz += (size_t)ssz) {
		if ((rc = poll(&pfd, 1, -1)) < 0) {
			XWARN("poll: %d, POLLOUT", fd);
			er = KCGI_SYSTEM;
			break;
		} else if (0 == rc) {
			XWARNX("poll: timeout!?");
			ssz = 0;
			continue;
		} else if (POLLHUP & pfd.revents) {
			XWARNX("poll: POLLHUP");
			er = KCGI_HUP;
			break;
		} else if (POLLERR & pfd.revents) {
			XWARNX("poll: POLLER");
			er = KCGI_SYSTEM;
			break;
#ifdef __APPLE__
		} else if ( ! (POLLOUT & pfd.revents) && 
			    ! (POLLNVAL & pfd.revents)) {
			XWARNX("poll: not POLLOUT");
			er = KCGI_SYSTEM;
			break;
#else
		} else if ( ! (POLLOUT & pfd.revents)) {
			XWARNX("poll: not POLLOUT");
			er = KCGI_SYSTEM;
			break;
#endif
		} 
		
		if ((ssz = write(fd, buf + sz, bufsz - sz)) < 0) {
			if (EPIPE == errno) {
				XWARNX("write: HUP");
				er = KCGI_HUP;
				break;
			}
			XWARN("write: %d, %zu", fd, bufsz - sz);
			er = KCGI_SYSTEM;
			break;
		} else if (sz > SIZE_MAX - (size_t)ssz) {
			XWARNX("write: overflow: %zu, %zd", sz, ssz);
			er = KCGI_SYSTEM;
			break;
		} 
	}

	signal(SIGPIPE, sig);
	return er;
}

/*
 * Write the full contents of "buf", which can be NULL so long as bufsz
 * is zero, to the non-blocking stream.
 * This will loop on "fd" until all data has been sent.
 * On error (which shouldn't happen), this will kill the process.
 */
void
fullwrite(int fd, const void *buf, size_t bufsz)
{
	ssize_t	 	 ssz;
	size_t	 	 sz;
	struct pollfd	 pfd;
	int		 rc;

	/* Make coverity be silent. */
	if (0 == bufsz)
		return;

	assert(NULL != buf);

	pfd.fd = fd;
	pfd.events = POLLOUT;

	for (sz = 0; sz < bufsz; sz += (size_t)ssz) {
		if ((rc = poll(&pfd, 1, -1)) < 0) 
			XWARN("poll: %d, POLLOUT", fd);
		else if (0 == rc) {
			XWARNX("poll: timeout!?");
			ssz = 0;
			continue;
		} else if (POLLHUP & pfd.revents)
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
	int		 rc;

	pfd.fd = fd;
	pfd.events = POLLIN;

	for (sz = 0; sz < bufsz; sz += (size_t)ssz) {
		if ((rc = poll(&pfd, 1, -1)) < 0) {
			XWARN("poll: %d, POLLIN", fd);
			*er = KCGI_SYSTEM;
		} else if (0 == rc) {
			XWARNX("poll: timeout!?");
			ssz = 0;
			continue;
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
	int		 rc;

	pfd.fd = fd;
	pfd.events = POLLIN;

	for (sz = 0; sz < bufsz; sz += (size_t)ssz) {
		if ((rc = poll(&pfd, 1, -1)) < 0) {
			XWARN("poll");
			*er = KCGI_SYSTEM;
			return(-1);
		} else if (0 == rc) {
			XWARNX("poll: timeout!?");
			ssz = 0;
			continue;
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
			*er = KCGI_SYSTEM;
			return(-1);
		} else if (0 == ssz && sz > 0) {
			XWARN("read: short read");
			*er = KCGI_FORM;
			return(-1);
		} else if (0 == ssz && sz == 0 && ! eofok) {
			XWARNX("read: unexpected eof: read "
				"%zu of %zu bytes", sz, bufsz);
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
 * by the word itself, not including the NUL terminator.
 * Return KCGI_OK on success or another error, otherwise.
 * This will initially set cp to NULL and sz to zero, and only allocate
 * on success (on failure, cp will be NULL, sz be zero).
 * The cp array is always NUL-terminated, although the buffer it reads
 * is opaque.
 */
enum kcgi_err
fullreadwordsz(int fd, char **cp, size_t *sz)
{
	enum kcgi_err	 ke = KCGI_SYSTEM;

	*cp = NULL;
	*sz = 0;

	if (fullread(fd, sz, sizeof(size_t), 0, &ke) < 0)
		return(ke);

	/* TODO: check additive overflow of "sz + 1". */

	if (NULL == (*cp = XMALLOC(*sz + 1))) {
		*sz = 0;
		return(KCGI_ENOMEM);
	}

	(*cp)[*sz] = '\0';
	if (0 == *sz)
		return(KCGI_OK);

	/* 
	 * Silence Coverity warning.
	 * This will set "ke" regardless of the return value, and we
	 * want to pass that back to the caller.
	 */

	/* coverity[check_return] */
	(void)fullread(fd, *cp, *sz, 0, &ke);

	if (KCGI_OK != ke) {
		free(*cp);
		*cp = NULL;
		*sz = 0;
	}
	return(ke);
}

/*
 * See fullreadwordsz() with a discarded size.
 */
enum kcgi_err
fullreadword(int fd, char **cp)
{
	size_t sz;

	return fullreadwordsz(fd, cp, &sz);
}

/*
 * Write a file-descriptor "sendfd" and a buffer "b" of length "bsz",
 * which must be 256 bytes or fewer, but not zero.
 * See fullwritefd().
 * Returns zero on failure (any kind), non-zero on success.
 */
int
fullwritefd(int fd, int sendfd, void *b, size_t bsz)
{
	struct msghdr	 msg;
	int		 rc;
	char		 buf[CMSG_SPACE(sizeof(fd))];
	struct iovec 	 io;
	struct cmsghdr	*cmsg;
	struct pollfd	 pfd;

	assert(bsz <= 256 && bsz > 0);

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
		kutil_warn(NULL, NULL, "poll");
		return 0;
	} else if (rc == 0) {
		kutil_warnx(NULL, NULL, "poll: timeout!?");
		goto again;
	}
	
	if (!(pfd.revents & POLLOUT)) {
		kutil_warnx(NULL, NULL, "poll: no output");
		return 0;
	} 
	
	if ((rc = sendmsg(fd, &msg, 0)) < 0) {
		kutil_warn(NULL, NULL, "sendmsg");
		return 0;
	} else if (rc == 0) {
		kutil_warnx(NULL, NULL, "sendmsg: short write");
		return 0;
	}

	return 1;
}

/*
 * Read a file-descriptor into "recvfd" and a buffer "b" of length
 * "bsz", which must be 256 bytes or fewer, but not zero.
 * See fullwritefd().
 * Returns <0 on system failure, 0 on hangup (remote end closed), and >0
 * on success.
 * The output pointers are only set on success.
 */
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

	assert(bsz <= 256 && bsz > 0);

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

again:
	if ((rc = poll(&pfd, 1, INFTIM)) < 0) {
		kutil_warn(NULL, NULL, "poll");
		return (-1);
	} else if (rc == 0) {
		kutil_warnx(NULL, NULL, "poll timeout!?!?");
		goto again;
	}
	
	if (!(pfd.revents & POLLIN)) {
		kutil_warnx(NULL, NULL, "poll: no input");
		return 0;
	} 
	
	if ((rc = recvmsg(fd, &msg, 0)) < 0) {
		kutil_warn(NULL, NULL, "recvmsg");
		return (-1);
	} else if (rc == 0) {
		kutil_warnx(NULL, NULL, "recvmsg: short read");
		return 0;
	}

	memcpy(b, m_buffer, bsz);
	for (cmsg = CMSG_FIRSTHDR(&msg); cmsg != NULL;
		 cmsg = CMSG_NXTHDR(&msg, cmsg)) {
		if (cmsg->cmsg_len == CMSG_LEN(sizeof(int)) &&
		    cmsg->cmsg_level == SOL_SOCKET &&
		    cmsg->cmsg_type == SCM_RIGHTS) {
			*recvfd = *(int *)CMSG_DATA(cmsg);
			return 1;
		}
	}

	kutil_warnx(NULL, NULL, "recvmsg: no SCM_RIGHTS");
	return (-1);
}
