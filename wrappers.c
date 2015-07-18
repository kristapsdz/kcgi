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
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/wait.h>

#include <errno.h>
#include <fcntl.h>
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
	} else if (-1 == (fl1 = fcntl(sock[0], F_GETFL, 0))) {
		XWARN("fcntl");
	} else if (-1 == (fl2 = fcntl(sock[1], F_GETFL, 0))) {
		XWARN("fcntl");
	} else if (-1 == fcntl(sock[0], F_SETFL, fl1 | O_NONBLOCK)) {
		XWARN("fcntl");
	} else if (-1 == fcntl(sock[1], F_SETFL, fl2 | O_NONBLOCK)) {
		XWARN("fcntl");
	} else
		return(KCGI_OK);

	close(sock[0]);
	close(sock[1]);
	return(KCGI_SYSTEM);
}
