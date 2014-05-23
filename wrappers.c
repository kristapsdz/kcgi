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
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

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
