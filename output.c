/*	$Id$ */
/*
 * Copyright (c) 2015 Kristaps Dzonsons <kristaps@bsd.lv>
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

#include <assert.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_ZLIB
#include <zlib.h>
#endif

#include "kcgi.h"
#include "extern.h"

/*
 * The state of our HTTP response.
 * We can be in KSTATE_HEAD, where we're printing HTTP headers; or
 * KSTATE_BODY, where we're printing the body parts.
 * So if we try to print a header when we're supposed to be in the body,
 * this will be caught.
 */
enum	kstate {
	KSTATE_HEAD = 0,
	KSTATE_BODY
};

/*
 * Interior data.
 * This is used for managing HTTP compression.
 */
struct	kdata {
	int		 fcgi;
	enum kstate	 state;
#ifdef	HAVE_ZLIB
	gzFile		 gz;
#endif
};

void
khttp_write(struct kreq *req, const char *buf, size_t sz)
{

	assert(NULL != req->kdata);
	assert(KSTATE_BODY == req->kdata->state);
#ifdef HAVE_ZLIB
	if (NULL != req->kdata->gz)
		gzwrite(req->kdata->gz, buf, sz);
	else 
#endif
		fwrite(buf, 1, sz, stdout);
}

void
khttp_puts(struct kreq *req, const char *cp)
{

	assert(NULL != req->kdata);
	assert(KSTATE_BODY == req->kdata->state);
#ifdef HAVE_ZLIB
	if (NULL != req->kdata->gz)
		gzputs(req->kdata->gz, cp);
	else 
#endif
		fputs(cp, stdout);
}

void
khttp_putc(struct kreq *req, int c)
{

	assert(NULL != req->kdata);
	assert(KSTATE_BODY == req->kdata->state);
#ifdef HAVE_ZLIB
	if (NULL != req->kdata->gz)
		gzputc(req->kdata->gz, c);
	else 
#endif
		putchar(c);
}

void
khttp_head(struct kreq *req, const char *key, const char *fmt, ...)
{
	va_list	 ap;

	assert(NULL != req->kdata);
	assert(KSTATE_HEAD == req->kdata->state);

	printf("%s: ", key);
	va_start(ap, fmt);
	vprintf(fmt, ap);
	printf("\r\n");
	va_end(ap);
}

/*
 * Allocate our output data.
 * We accept the file descriptor for the FastCGI stream, if there's any.
 */
struct kdata *
kdata_alloc(int fcgi)
{
	struct kdata	*p;

	if (NULL != (p = XCALLOC(1, sizeof(struct kdata))))
		p->fcgi = fcgi;
	return(p);
}

void
kdata_free(struct kdata *p, int flush)
{

#ifdef HAVE_ZLIB
	if (NULL != p->gz)
		gzclose(p->gz);
#endif
	if (flush)
		fflush(stdout);
	free(p);
}

/*
 * Try to enable compression on the output stream itself.
 * We disallow compression on FastCGI streams because I don't yet have
 * the structure in place to copy the compressed data into a buffer then
 * write that out.
 * Return whether we enabled compression.
 */
int
kdata_compress(struct kdata *p)
{

	assert(KSTATE_HEAD == p->state);
#ifdef	HAVE_ZLIB
	if (-1 != p->fcgi)
		return(0);
	assert(NULL == p->gz);
	p->gz = gzdopen(STDOUT_FILENO, "w");
	if (NULL == p->gz)
		XWARN("gzdopen");
	return(NULL != p->gz);
#else
	return(0);
#endif
}

/*
 * Begin the body sequence.
 */
void
kdata_body(struct kdata *p)
{

	assert(p->state == KSTATE_HEAD);
	fputs("\r\n", stdout);
	fflush(stdout);
	p->state = KSTATE_BODY;
}
