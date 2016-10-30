/*	$Id$ */
/*
 * Copyright (c) 2016 Kristaps Dzonsons <kristaps@bsd.lv>
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

#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "kcgi.h"

/*
 * Actual logging function.
 * This works best if stderr is line buffered, otherwise we may clobber
 * other message who are writing at the same time.
 * All strings are trusted to have "good" characters except for the
 * variable array, which may contain anything and is filtered.
 */
static void
logmsg(const struct kreq *r, const char *err, const char *lvl, 
	const char *ident, const char *fmt, va_list ap)
{
	int		 i, sz;
	char		 date[64];
	char		 msg[1024];

	/* 
	 * Convert to GMT.
	 * We can't use localtime because we're probably going to be
	 * chrooted, and maybe pledged, and touching our timezone files
	 * will crash us (or at least not be applicable).
	 */

	kutil_epoch2str(time(NULL), date, sizeof(date));

	/* Everything up to the message itself. */

	fprintf(stderr, "%s %s [%s] %s ", r->remote, 
		NULL == ident ? "-" : ident, date, 
		NULL == lvl ? "-" : lvl);

	/*
	 * Now format the message itself.
	 * Assume that the message is UNTRUSTED, and filter the contents
	 * of each character to see if it's a valid printable.
	 */

	sz = vsnprintf(msg, sizeof(msg), fmt, ap);
	for (i = 0; i < sz; i++)
		switch (msg[i]) {
		case ('\n'):
			fputs("\\n", stderr);
			break;
		case ('\r'):
			fputs("\\r", stderr);
			break;
		case ('\t'):
			fputs("\\t", stderr);
			break;
		default:
			if (isprint((int)msg[i]))
				fputc(msg[i], stderr);
			else
				fputc('?', stderr);
			break;
		}

	/*
	 * Emit the system error message, if applicable.
	 */

	if (NULL != err) {
		fputs(": ", stderr);
		fputs(err, stderr);
	}

	fputc('\n', stderr);
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
	kutil_vlogx(r, "WARN", ident, fmt, ap);
	va_end(ap);
}

void
kutil_warn(const struct kreq *r, 
	const char *ident, const char *fmt, ...)
{
	va_list	 ap;

	va_start(ap, fmt);
	kutil_vlog(r, "WARN", ident, fmt, ap);
	va_end(ap);
}

void
kutil_info(const struct kreq *r, 
	const char *ident, const char *fmt, ...)
{
	va_list	 ap;

	va_start(ap, fmt);
	kutil_vlogx(r, "INFO", ident, fmt, ap);
	va_end(ap);
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
