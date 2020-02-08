/*	$Id$ */
/*
 * Copyright (c) 2016--2018 Kristaps Dzonsons <kristaps@bsd.lv>
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

#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "kcgi.h"

enum	llevel {
	LLEVEL_INFO,
	LLEVEL_WARN,
	LLEVEL_ERROR,
	LLEVEL__MAX
};

static	const char *llevels[LLEVEL__MAX] = {
	"INFO", /* LLEVEL_INFO */
	"WARN", /* LLEVEL_WARN */
	"ERROR", /* LLEVEL_ERROR */
};

/*
 * Actual logging function.
 * Only the optional error string is trusted to have "good" characters;
 * all others may contain anything and are filtered.
 */
static void
logmsg(const struct kreq *r, const char *err, const char *lvl, 
	const char *ident, const char *fmt, va_list ap)
{
	int		 i, cmpsz, sz;
	char		 date[64];
	char		*msg, *var, *cmp, *p;

	/* 
	 * Convert to GMT.
	 * We can't use localtime because we're probably going to be
	 * chrooted, and maybe sandboxed, and touching our timezone
	 * files will crash us (or at least not be applicable).
	 */

	kutil_epoch2str(time(NULL), date, sizeof(date));

	/*
	 * Format the variable args, then compose with the log prefix
	 * to form the basic message.
	 */

	kvasprintf(&var, fmt, ap);
	cmpsz = kasprintf(&cmp, "%s %s [%s] %s %s",
			  NULL == r ? "-" : r->remote, 
			  NULL == ident ? "-" : ident, date, 
			  NULL == lvl ? "-" : lvl,
			  var);
	free(var);

	/*
	 * Allocate the required memory for the message, leaving room
	 * for whitespace character expansion and optional error message.
	 */

	sz = cmpsz + 2; /* for \n\0 */
	for (i = 0; i < cmpsz; i++) {
		switch (cmp[i]) {
		case ('\n'):
		case ('\r'):
		case ('\t'):
			sz++;
			break;
		}
	}

	if (NULL != err) { 
		sz += 2;	/* for ": " */
		sz += strlen(err);
	}

	p = msg = kmalloc(sz);

	/*
	 * Copy message into final buffer, filtering unprintables
	 * and whitespace.
	 */

	for (i = 0; i < cmpsz; i++) {
		switch (cmp[i]) {
		case ('\n'):
			*p++ = '\\';
			*p++ = 'n';
			break;
		case ('\r'):
			*p++ = '\\';
			*p++ = 'r';
			break;
		case ('\t'):
			*p++ = '\\';
			*p++ = 't';
			break;
		default:
			if (isprint((unsigned char)cmp[i]))
				*p++ = cmp[i];
			else
				*p++ = '?';
			break;
		}
	}
	*p = '\0';
	free(cmp);

	/* Append optional error message, and newline */

	if (NULL != err) {
		(void)strlcat(msg, ": ", sz);
		(void)strlcat(msg, err, sz);
	}
	(void)strlcat(msg, "\n", sz);

	fputs(msg, stderr);

	free(msg);
}

int
kutil_openlog(const char *file)
{

	if (NULL != file && NULL == freopen(file, "a", stderr))
		return(0);
	return(EOF != setvbuf(stderr, NULL, _IOLBF, 0));
}

void
kutil_vlog(const struct kreq *r, const char *lvl,
	const char *ident, const char *fmt, va_list ap)
{

	logmsg(r, strerror(errno), lvl, ident, fmt, ap);
}

void
kutil_vlogx(const struct kreq *r, const char *lvl,
	const char *ident, const char *fmt, va_list ap)
{

	logmsg(r, NULL, lvl, ident, fmt, ap);
}

void
kutil_warnx(const struct kreq *r, 
	const char *ident, const char *fmt, ...)
{
	va_list	 ap;

	va_start(ap, fmt);
	kutil_vlogx(r, llevels[LLEVEL_WARN], ident, fmt, ap);
	va_end(ap);
}

void
kutil_errx(const struct kreq *r, 
	const char *ident, const char *fmt, ...)
{
	va_list	 ap;

	va_start(ap, fmt);
	kutil_vlogx(r, llevels[LLEVEL_ERROR], ident, fmt, ap);
	va_end(ap);
	exit(EXIT_FAILURE);
}

void
kutil_verrx(const struct kreq *r, 
	const char *ident, const char *fmt, va_list ap)
{

	kutil_vlogx(r, llevels[LLEVEL_ERROR], ident, fmt, ap);
	exit(EXIT_FAILURE);
}

void
kutil_vwarnx(const struct kreq *r, 
	const char *ident, const char *fmt, va_list ap)
{

	kutil_vlogx(r, llevels[LLEVEL_WARN], ident, fmt, ap);
}

void
kutil_err(const struct kreq *r, 
	const char *ident, const char *fmt, ...)
{
	va_list	 ap;

	va_start(ap, fmt);
	kutil_vlog(r, llevels[LLEVEL_ERROR], ident, fmt, ap);
	va_end(ap);
	exit(EXIT_FAILURE);
}

void
kutil_warn(const struct kreq *r, 
	const char *ident, const char *fmt, ...)
{
	va_list	 ap;

	va_start(ap, fmt);
	kutil_vlog(r, llevels[LLEVEL_WARN], ident, fmt, ap);
	va_end(ap);
}

void
kutil_verr(const struct kreq *r, 
	const char *ident, const char *fmt, va_list ap)
{

	kutil_vlog(r, llevels[LLEVEL_ERROR], ident, fmt, ap);
	exit(EXIT_FAILURE);
}

void
kutil_vwarn(const struct kreq *r, 
	const char *ident, const char *fmt, va_list ap)
{

	kutil_vlog(r, llevels[LLEVEL_WARN], ident, fmt, ap);
}

void
kutil_info(const struct kreq *r, 
	const char *ident, const char *fmt, ...)
{
	va_list	 ap;

	va_start(ap, fmt);
	kutil_vlogx(r, llevels[LLEVEL_INFO], ident, fmt, ap);
	va_end(ap);
}

void
kutil_vinfo(const struct kreq *r, 
	const char *ident, const char *fmt, va_list ap)
{

	kutil_vlogx(r, llevels[LLEVEL_INFO], ident, fmt, ap);
}

void
kutil_logx(const struct kreq *r, const char *lvl,
	const char *ident, const char *fmt, ...)
{
	va_list	 ap;

	va_start(ap, fmt);
	kutil_vlogx(r, lvl, ident, fmt, ap);
	va_end(ap);
}

void
kutil_log(const struct kreq *r, const char *lvl,
	const char *ident, const char *fmt, ...)
{
	va_list	 ap;

	va_start(ap, fmt);
	kutil_vlog(r, lvl, ident, fmt, ap);
	va_end(ap);
}
