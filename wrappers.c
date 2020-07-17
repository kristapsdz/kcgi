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
kxvasprintf(char **p, const char *fmt, va_list ap)
{
	int	 len;

	if ((len = vasprintf(p, fmt, ap)) >= 0)
		return len;

	kutil_warn(NULL, NULL, "vasprintf");
	*p = NULL;
	return -1;
}

int
kxasprintf(char **p, const char *fmt, ...)
{
	va_list	 ap;
	int	 ret;

	va_start(ap, fmt);
	ret = kxvasprintf(p, fmt, ap);
	va_end(ap);
	return ret;
}

void *
kxcalloc(size_t nm, size_t sz)
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
kxmalloc(size_t sz)
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
kxrealloc(void *pp, size_t sz)
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
kxreallocarray(void *pp, size_t nm, size_t sz)
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
kxstrdup(const char *cp)
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
 * Create a non-blocking socketpair.
 * Return KCGI_ENFILE on temporary failure, KCGI_SYSTEM on fatal error,
 * KCGI_OK on success.
 * The input socket pair is only valid on success.
 */
enum kcgi_err
kxsocketpair(int domain, int type, int protocol, int sock[2])
{
	int	 	 rc;
	enum kcgi_err	 er;

	rc = socketpair(domain, type, protocol, sock);
	if (rc == -1 && (EMFILE == errno || ENFILE == errno)) {
		kutil_warn(NULL, NULL, "socketpair");
		return KCGI_ENFILE;
	} else if (rc == -1) {
		kutil_warn(NULL, NULL, "socketpair");
		return KCGI_SYSTEM;
	}

	if ((er = kxsocketprep(sock[0])) == KCGI_OK &&
	   ((er = kxsocketprep(sock[1])) == KCGI_OK))
		return KCGI_OK;
	
	close(sock[0]);
	close(sock[1]);
	return er;
}

/*
 * Write a string "buf".
 * If "buf" is NULL, then write a zero-length string.
 * See fullreadword().
 * On error (which shouldn't happen), this will exit the process with
 * the exit code EXIT_FAILURE.
 */
void
fullwriteword(int fd, const char *buf)
{
	size_t	 sz;

	if (buf != NULL) {
		sz = strlen(buf);
		fullwrite(fd, &sz, sizeof(size_t));
		fullwrite(fd, buf, sz);
	} else {
		sz = 0;
		fullwrite(fd, &sz, sizeof(size_t));
	}
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

	if ((sig = signal(SIGPIPE, SIG_IGN)) == SIG_ERR) {
		kutil_warn(NULL, NULL, "signal");
		return KCGI_SYSTEM;
	}

	for (sz = 0; sz < bufsz; sz += (size_t)ssz) {
		if ((rc = poll(&pfd, 1, -1)) < 0) {
			kutil_warn(NULL, NULL, "poll");
			er = KCGI_SYSTEM;
			break;
		} else if (rc == 0) {
			kutil_warnx(NULL, NULL, "poll: timeout!?");
			ssz = 0;
			continue;
		}

		if (pfd.revents & POLLHUP) {
			kutil_warnx(NULL, NULL, "poll: hangup");
			er = KCGI_HUP;
			break;
		} else if (pfd.revents & POLLERR) {
			kutil_warnx(NULL, NULL, "poll: error");
			er = KCGI_SYSTEM;
			break;
		}

		/* See note in fullwrite(). */

#ifdef __APPLE__
		if (!(pfd.revents & POLLOUT) && 
		    !(pfd.revents & POLLNVAL)) {
			kutil_warnx(NULL, NULL, "poll: no output");
			er = KCGI_SYSTEM;
			break;
		}
#else
		if (!(pfd.revents & POLLOUT)) {
			kutil_warnx(NULL, NULL, "poll: no output");
			er = KCGI_SYSTEM;
			break;
		}
#endif

		if ((ssz = write(fd, buf + sz, bufsz - sz)) < 0) {
			er = errno == EPIPE ? KCGI_HUP : KCGI_SYSTEM;
			kutil_warn(NULL, NULL, "write");
			break;
		} else if (sz > SIZE_MAX - (size_t)ssz) {
			kutil_warnx(NULL, NULL, "write: overflow");
			er = KCGI_SYSTEM;
			break;
		} 
	}

	if (signal(SIGPIPE, sig) == SIG_ERR) {
		kutil_warn(NULL, NULL, "signal");
		er = KCGI_SYSTEM;
	}

	return er;
}

/*
 * Write "buf", which can be NULL so long as bufsz is zero in which case
 * it's a noop.
 * On error (which shouldn't happen), this will exit the process with
 * the exit code EXIT_FAILURE.
 */
void
fullwrite(int fd, const void *buf, size_t bufsz)
{
	ssize_t	 	 ssz;
	size_t	 	 sz;
	struct pollfd	 pfd;
	int		 rc;

	if (bufsz == 0)
		return;

	assert(buf != NULL);
	pfd.fd = fd;
	pfd.events = POLLOUT;

	for (sz = 0; sz < bufsz; sz += (size_t)ssz) {
		if ((rc = poll(&pfd, 1, INFTIM)) == 0) {
			kutil_warnx(NULL, NULL, "poll: timeout!?");
			ssz = 0;
			continue;
		} else if (rc < 0)
			kutil_err(NULL, NULL, "poll");

		if (pfd.revents & POLLHUP)
			kutil_errx(NULL, NULL, "poll: hangup");
		else if (pfd.revents & POLLERR)
			kutil_errx(NULL, NULL, "poll: error");

		/*
		 * This CPP exists because testing on Mac OS X will have
		 * "fd" point to a device and poll(2) returns POLLNVAL
		 * if the descriptor is to a device.
		 * This is documented in the BUGS section of poll(2).
		 */
#ifdef __APPLE__
		if (!(pfd.revents & POLLOUT) && 
		    !(pfd.revents & POLLNVAL))
			kutil_errx(NULL, NULL, "poll: no output");
#else
		if (!(pfd.revents & POLLOUT))
			kutil_errx(NULL, NULL, "poll: no output");
#endif

		if ((ssz = write(fd, buf + sz, bufsz - sz)) < 0)
			kutil_err(NULL, NULL, "write");
		else if (sz > SIZE_MAX - (size_t)ssz)
			kutil_errx(NULL, NULL, "write: overflow");
	}
}

/*
 * Read the contents of "buf" of size "bufsz".
 * If "eofok" is set, return zero if there is no data to read.
 * If it's not set, this condition returns <0.
 * Returns <0 on errors and >0 on success.
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
		if ((rc = poll(&pfd, 1, INFTIM)) < 0) {
			kutil_warn(NULL, NULL, "poll");
			*er = KCGI_SYSTEM;
			return (-1);
		} else if (rc == 0) {
			kutil_warnx(NULL, NULL, "poll: timeout!?");
			ssz = 0;
			continue;
		} 
		
		if (!(pfd.revents & POLLIN)) {
			if (eofok && sz == 0) {
				*er = KCGI_OK;
				return 0;
			}
			kutil_warnx(NULL, NULL, "poll: no input");
			*er = KCGI_FORM;
			return (-1);
		} 
		
		if ((ssz = read(fd, buf + sz, bufsz - sz)) < 0) {
			kutil_warn(NULL, NULL, "read");
			*er = KCGI_SYSTEM;
			return (-1);
		} else if (ssz == 0 && sz > 0) {
			kutil_warnx(NULL, NULL, "read: short read");
			*er = KCGI_FORM;
			return (-1);
		} else if (ssz == 0 && sz == 0 && !eofok) {
			kutil_warnx(NULL, NULL, "read: end of file");
			*er = KCGI_FORM;
			return (-1);
		} else if (ssz == 0 && sz == 0 && eofok) {
			*er = KCGI_OK;
			return 0;
		} else if (sz > SIZE_MAX - (size_t)ssz) {
			kutil_warnx(NULL, NULL, "read: overflow");
			*er = KCGI_FORM;
			return (-1);
		}
	}

	*er = KCGI_OK;
	return 1;
}

/*
 * Read a string from the stream.
 * Return KCGI_OK on success or another on error.
 * This will set cp to NULL and sz to zero on failure.
 * The cp array (on success) is always NUL-terminated, although the
 * buffer it reads is opaque.
 * On success, "sz" may legit be zero.
 */
enum kcgi_err
fullreadwordsz(int fd, char **cp, size_t *sz)
{
	enum kcgi_err	 ke;
	int		 rc;

	*cp = NULL;
	*sz = 0;

	if (fullread(fd, sz, sizeof(size_t), 0, &ke) < 0)
		return ke;

	/* TODO: check additive overflow of "sz + 1". */

	if ((*cp = kxmalloc(*sz + 1)) == NULL) {
		*sz = 0;
		return KCGI_ENOMEM;
	}
	(*cp)[*sz] = '\0';

	if (*sz == 0)
		return KCGI_OK;

	/* Because we don't set "eofok", never returns zero. */

	if ((rc = fullread(fd, *cp, *sz, 0, &ke)) > 0) {
		assert(ke == KCGI_OK);
		return ke;
	}

	assert(rc < 0);
	assert(ke != KCGI_OK);
	free(*cp);
	*cp = NULL;
	*sz = 0;
	return ke;
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
	if ((rc = poll(&pfd, 1, INFTIM)) < 0) {
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
