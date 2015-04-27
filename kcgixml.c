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
#include <assert.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "kcgi.h"
#include "kcgixml.h"

void
kxml_open(struct kxmlreq *r, struct kreq *req,
	const char *const *elems, size_t elemsz)
{

	memset(r, 0, sizeof(struct kxmlreq));
	r->req = req;
	r->elems = elems;
	r->elemsz = elemsz;
	khttp_puts(r->req, 
		"<?xml version=\"1.0\" "
		"encoding=\"utf-8\" ?>");
}

int
kxml_close(struct kxmlreq *r)
{
	int	 i;

	i = r->stackpos > 0;
	kxml_popall(r);
	return(i);
}

int
kxml_push(struct kxmlreq *r, size_t elem)
{

	if (r->stackpos >= 128) 
		return(0);

	khttp_putc(r->req, '<');
	khttp_puts(r->req, r->elems[elem]);
	khttp_putc(r->req, '>');
	r->stack[r->stackpos++] = elem;
	return(1);
}

void
kxml_pushnull(struct kxmlreq *r, size_t elem)
{

	khttp_putc(r->req, '<');
	khttp_puts(r->req, r->elems[elem]);
	khttp_puts(r->req, " />");
}

void
kxml_putc(struct kxmlreq *r, char c)
{

	switch (c) {
	case ('<'):
		khttp_puts(r->req, "&lt;");
		break;
	case ('>'):
		khttp_puts(r->req, "&gt;");
		break;
	case ('"'):
		khttp_puts(r->req, "&quot;");
		break;
	case ('&'):
		khttp_puts(r->req, "&amp;");
		break;
	default:
		khttp_putc(r->req, c);
		break;
	}
}

int
kxml_write(const char *p, size_t sz, void *arg)
{
	struct kxmlreq 	*r = arg;
	size_t	 	 i;

	for (i = 0; i < sz; i++)
		kxml_putc(r, p[i]);

	return(1);
}

void
kxml_puts(struct kxmlreq *r, const char *p)
{

	for ( ; '\0' != *p; p++)
		kxml_putc(r, *p);
}

int
kxml_pushattrs(struct kxmlreq *r, size_t elem, ...)
{
	va_list	 	 ap;
	const char	*key, *val;

	if (r->stackpos >= 128) 
		return(0);

	khttp_putc(r->req, '<');
	khttp_puts(r->req, r->elems[elem]);
	va_start(ap, elem);
	for (;;) {
		if (NULL == (key = va_arg(ap, char *)))
			break;
		val = va_arg(ap, char *);
		khttp_putc(r->req, ' ');
		khttp_puts(r->req, key);
		khttp_putc(r->req, '=');
		khttp_putc(r->req, '"');
		kxml_puts(r, val);
		khttp_putc(r->req, '"');
	}
	va_end(ap);
	khttp_putc(r->req, '>');
	r->stack[r->stackpos++] = elem;
	return(1);
}

void
kxml_pushnullattrs(struct kxmlreq *r, size_t elem, ...)
{
	va_list	 	 ap;
	const char	*key, *val;

	khttp_putc(r->req, '<');
	khttp_puts(r->req, r->elems[elem]);
	va_start(ap, elem);
	for (;;) {
		if (NULL == (key = va_arg(ap, char *)))
			break;
		val = va_arg(ap, char *);
		khttp_putc(r->req, ' ');
		khttp_puts(r->req, key);
		khttp_putc(r->req, '=');
		khttp_putc(r->req, '"');
		kxml_puts(r, val);
		khttp_putc(r->req, '"');
	}
	va_end(ap);
	khttp_puts(r->req, " />");
}

void
kxml_popall(struct kxmlreq *r)
{

	while (kxml_pop(r))
		/* Spin. */ ;
}

int
kxml_pop(struct kxmlreq *r)
{

	if (0 == r->stackpos)
		return(0);

	khttp_puts(r->req, "</");
	khttp_puts(r->req, r->elems[r->stack[--r->stackpos]]);
	khttp_putc(r->req, '>');
	return(1);

}
