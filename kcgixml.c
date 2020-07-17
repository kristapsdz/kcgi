/*	$Id$ */
/*
 * Copyright (c) 2015, 2017, 2020 Kristaps Dzonsons <kristaps@bsd.lv>
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
#include "extern.h"

enum kcgi_err
kxml_open(struct kxmlreq *r, struct kreq *req,
	const char *const *elems, size_t elemsz)
{

	memset(r, 0, sizeof(struct kxmlreq));
	if (NULL == (r->arg = kcgi_writer_get(req, 0)))
		return(KCGI_ENOMEM);
	r->elems = elems;
	r->elemsz = elemsz;
	return(KCGI_OK);
}

enum kcgi_err
kxml_prologue(struct kxmlreq *r)
{

	return kcgi_writer_puts(r->arg, 
		"<?xml version=\"1.0\" "
		"encoding=\"utf-8\" ?>");
}

enum kcgi_err
kxml_close(struct kxmlreq *r)
{
	enum kcgi_err	 er;

	er = kxml_popall(r);
	kcgi_writer_free(r->arg);
	r->arg = NULL;
	return er;
}

enum kcgi_err
kxml_push(struct kxmlreq *r, size_t elem)
{
	enum kcgi_err	 er;

	if (r->stackpos >= KXML_STACK_MAX) {
		kutil_warnx(NULL, NULL, 
			"maximum xml stack size exceeded");
		return KCGI_ENOMEM;
	} else if (elem >= r->elemsz)
		return KCGI_WRITER;

	if ((er = kcgi_writer_putc(r->arg, '<')) != KCGI_OK)
		return er;
	if ((er = kcgi_writer_puts(r->arg, r->elems[elem])) != KCGI_OK)
		return er;
	if ((er = kcgi_writer_putc(r->arg, '>')) != KCGI_OK)
		return er;

	r->stack[r->stackpos++] = elem;
	return KCGI_OK;
}

enum kcgi_err
kxml_pushnull(struct kxmlreq *r, size_t elem)
{
	enum kcgi_err	 er;

	if (r->stackpos >= KXML_STACK_MAX) {
		kutil_warnx(NULL, NULL, 
			"maximum xml stack size exceeded");
		return KCGI_ENOMEM;
	} else if (elem >= r->elemsz)
		return KCGI_WRITER;

	if ((er = kcgi_writer_putc(r->arg, '<')) != KCGI_OK)
		return er;
	if ((er = kcgi_writer_puts(r->arg, r->elems[elem])) != KCGI_OK)
		return er;

	return kcgi_writer_puts(r->arg, " />");
}

enum kcgi_err
kxml_putc(struct kxmlreq *r, char c)
{
	enum kcgi_err	 er;

	switch (c) {
	case ('<'):
		er = kcgi_writer_puts(r->arg, "&lt;");
		break;
	case ('>'):
		er = kcgi_writer_puts(r->arg, "&gt;");
		break;
	case ('"'):
		er = kcgi_writer_puts(r->arg, "&quot;");
		break;
	case ('&'):
		er = kcgi_writer_puts(r->arg, "&amp;");
		break;
	default:
		er = kcgi_writer_putc(r->arg, c);
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

	if (p == NULL || sz == 0)
		return KCGI_OK;

	for (i = 0; i < sz; i++)
		if ((er = kxml_putc(r, p[i])) != KCGI_OK)
			return er;

	return KCGI_OK;
}

enum kcgi_err
kxml_puts(struct kxmlreq *r, const char *p)
{

	if (p == NULL)
		return KCGI_OK;
	return kxml_write(p, strlen(p), r);
}

enum kcgi_err
kxml_pushattrs(struct kxmlreq *r, size_t elem, ...)
{
	va_list	 	 ap;
	const char	*key, *val;
	enum kcgi_err	 er = KCGI_OK;

	if (r->stackpos >= KXML_STACK_MAX) {
		kutil_warnx(NULL, NULL, 
			"maximum xml stack size exceeded");
		return KCGI_ENOMEM;
	} else if (elem >= r->elemsz)
		return KCGI_WRITER;

	if ((er = kcgi_writer_putc(r->arg, '<')) != KCGI_OK)
		return er;
	if ((er = kcgi_writer_puts(r->arg, r->elems[elem])) != KCGI_OK)
		return er;
	va_start(ap, elem);
	for (;;) {
		if ((key = va_arg(ap, char *)) == NULL)
			break;
		val = va_arg(ap, char *);
		if ((er = kcgi_writer_putc(r->arg, ' ')) != KCGI_OK)
			goto out;
		if ((er = kcgi_writer_puts(r->arg, key)) != KCGI_OK)
			goto out;
		if ((er = kcgi_writer_puts(r->arg, "=\"")) != KCGI_OK)
			goto out;
		if ((er = kxml_puts(r, val)) != KCGI_OK)
			goto out;
		if ((er = kcgi_writer_putc(r->arg, '"')) != KCGI_OK)
			goto out;
	}
	va_end(ap);
	r->stack[r->stackpos++] = elem;
	return kcgi_writer_putc(r->arg, '>');
out:
	va_end(ap);
	return er;
}

enum kcgi_err
kxml_pushnullattrs(struct kxmlreq *r, size_t elem, ...)
{
	va_list	 	 ap;
	const char	*key, *val;
	enum kcgi_err	 er;

	if (r->stackpos >= KXML_STACK_MAX) {
		XWARNX("maximum json stack size exceeded");
		return KCGI_ENOMEM;
	} else if (elem >= r->elemsz)
		return KCGI_WRITER;

	if ((er = kcgi_writer_putc(r->arg, '<')) != KCGI_OK)
		return er;
	if ((er = kcgi_writer_puts(r->arg, r->elems[elem])) != KCGI_OK)
		return er;
	va_start(ap, elem);
	for (;;) {
		if ((key = va_arg(ap, char *)) == NULL)
			break;
		val = va_arg(ap, char *);
		if ((er = kcgi_writer_putc(r->arg, ' ')) != KCGI_OK)
			goto out;
		if ((er = kcgi_writer_puts(r->arg, key)) != KCGI_OK)
			goto out;
		if ((er = kcgi_writer_puts(r->arg, "=\"")) != KCGI_OK)
			goto out;
		if ((er = kxml_puts(r, val)) != KCGI_OK)
			goto out;
		if ((er = kcgi_writer_putc(r->arg, '"')) != KCGI_OK)
			goto out;
	}
	va_end(ap);
	return kcgi_writer_puts(r->arg, " />");
out:
	va_end(ap);
	return er;
}

static int
kxml_pop_inner(struct kxmlreq *r, enum kcgi_err *er)
{

	if (r->stackpos == 0)
		return 0;

	*er = kcgi_writer_puts(r->arg, "</");
	if (*er != KCGI_OK)
		return (-1);

	*er = kcgi_writer_puts(r->arg,
		r->elems[r->stack[--r->stackpos]]);
	if (*er != KCGI_OK)
		return (-1);

	*er = kcgi_writer_putc(r->arg, '>');
	if (*er != KCGI_OK)
		return (-1);

	return 1;
}

enum kcgi_err
kxml_popall(struct kxmlreq *r)
{
	enum kcgi_err	 er = KCGI_OK;

	while (kxml_pop_inner(r, &er) > 0)
		/* Spin. */ ;

	return er;
}

enum kcgi_err
kxml_pop(struct kxmlreq *r)
{
	enum kcgi_err	 er;
	int		 c;

	if ((c = kxml_pop_inner(r, &er)) < 0)
		return er;
	else if (c == 0)
		return KCGI_WRITER;

	return KCGI_OK;
}
