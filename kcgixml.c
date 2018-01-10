/*	$Id$ */
/*
 * Copyright (c) 2015, 2017 Kristaps Dzonsons <kristaps@bsd.lv>
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

#include <assert.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "kcgi.h"
#include "kcgixml.h"

enum kcgi_err
kxml_open(struct kxmlreq *r, struct kreq *req,
	const char *const *elems, size_t elemsz)
{

	memset(r, 0, sizeof(struct kxmlreq));
	r->req = req;
	r->elems = elems;
	r->elemsz = elemsz;
	return(khttp_puts(r->req, 
		"<?xml version=\"1.0\" "
		"encoding=\"utf-8\" ?>"));
}

enum kcgi_err
kxml_close(struct kxmlreq *r)
{

	return(kxml_popall(r));
}

enum kcgi_err
kxml_push(struct kxmlreq *r, size_t elem)
{
	enum kcgi_err	 er;

	if (r->stackpos >= 128) 
		return(KCGI_FORM);

	if (KCGI_OK != (er = khttp_putc(r->req, '<')))
		return(er);
	if (KCGI_OK != (er = khttp_puts(r->req, r->elems[elem])))
		return(er);
	if (KCGI_OK != (er = khttp_putc(r->req, '>')))
		return(er);
	r->stack[r->stackpos++] = elem;
	return(KCGI_OK);
}

enum kcgi_err
kxml_pushnull(struct kxmlreq *r, size_t elem)
{
	enum kcgi_err	 er;

	if (KCGI_OK != (er = khttp_putc(r->req, '<')))
		return(er);
	if (KCGI_OK != (er = khttp_puts(r->req, r->elems[elem])))
		return(er);
	return(khttp_puts(r->req, " />"));
}

enum kcgi_err
kxml_putc(struct kxmlreq *r, char c)
{
	enum kcgi_err	 er;

	switch (c) {
	case ('<'):
		er = khttp_puts(r->req, "&lt;");
		break;
	case ('>'):
		er = khttp_puts(r->req, "&gt;");
		break;
	case ('"'):
		er = khttp_puts(r->req, "&quot;");
		break;
	case ('&'):
		er = khttp_puts(r->req, "&amp;");
		break;
	default:
		er = khttp_putc(r->req, c);
		break;
	}

	return(er);
}

enum kcgi_err
kxml_write(const char *p, size_t sz, void *arg)
{
	struct kxmlreq 	*r = arg;
	size_t	 	 i;
	enum kcgi_err	 er;

	for (i = 0; i < sz; i++)
		if (KCGI_OK != (er = kxml_putc(r, p[i])))
			return(er);

	return(1);
}

enum kcgi_err
kxml_puts(struct kxmlreq *r, const char *p)
{

	return(kxml_write(p, strlen(p), r));
}

enum kcgi_err
kxml_pushattrs(struct kxmlreq *r, size_t elem, ...)
{
	va_list	 	 ap;
	const char	*key, *val;
	enum kcgi_err	 er;

	if (r->stackpos >= 128) 
		return(KCGI_FORM);

	if (KCGI_OK != (er = khttp_putc(r->req, '<')))
		return(er);
	if (KCGI_OK != (er = khttp_puts(r->req, r->elems[elem])))
		return(er);
	va_start(ap, elem);
	for (;;) {
		if (NULL == (key = va_arg(ap, char *)))
			break;
		val = va_arg(ap, char *);
		if (KCGI_OK != (er = khttp_putc(r->req, ' ')))
			return(er);
		if (KCGI_OK != (er = khttp_puts(r->req, key)))
			return(er);
		if (KCGI_OK != (er = khttp_puts(r->req, "=\"")))
			return(er);
		if (KCGI_OK != (er = kxml_puts(r, val)))
			return(er);
		if (KCGI_OK != (er = khttp_putc(r->req, '"')))
			return(er);
	}
	va_end(ap);
	r->stack[r->stackpos++] = elem;
	return(khttp_putc(r->req, '>'));
}

enum kcgi_err
kxml_pushnullattrs(struct kxmlreq *r, size_t elem, ...)
{
	va_list	 	 ap;
	const char	*key, *val;
	enum kcgi_err	 er;

	if (KCGI_OK != (er = khttp_putc(r->req, '<')))
		return(er);
	if (KCGI_OK != (er = khttp_puts(r->req, r->elems[elem])))
		return(er);

	va_start(ap, elem);
	for (;;) {
		if (NULL == (key = va_arg(ap, char *)))
			break;
		val = va_arg(ap, char *);
		if (KCGI_OK != (er = khttp_putc(r->req, ' ')))
			return(er);
		if (KCGI_OK != (er = khttp_puts(r->req, key)))
			return(er);
		if (KCGI_OK != (er = khttp_puts(r->req, "=\"")))
			return(er);
		if (KCGI_OK != (er = kxml_puts(r, val)))
			return(er);
		if (KCGI_OK != (er = khttp_putc(r->req, '"')))
			return(er);
	}
	va_end(ap);
	return(khttp_puts(r->req, " />"));
}

enum kcgi_err
kxml_popall(struct kxmlreq *r)
{
	enum kcgi_err	 er;

	while (KCGI_OK == (er = kxml_pop(r)))
		/* Spin. */ ;
	return(KCGI_FORM == er ? KCGI_OK : er);
}

enum kcgi_err
kxml_pop(struct kxmlreq *r)
{
	enum kcgi_err	 er;

	if (0 == r->stackpos)
		return(KCGI_FORM);

	if (KCGI_OK != (er = khttp_puts(r->req, "</")))
		return(er);
	if (KCGI_OK != (er = khttp_puts
	    (r->req, r->elems[r->stack[--r->stackpos]])))
		return(er);
	return(khttp_putc(r->req, '>'));
}
