/*	$Id$ */
/*
 * Copyright (c) 2012, 2014, 2015, 2017 Kristaps Dzonsons <kristaps@bsd.lv>
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
#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "kcgi.h"
#include "kcgijson.h"
#include "extern.h"

enum kcgi_err
kjson_open(struct kjsonreq *r, struct kreq *req)
{

	memset(r, 0, sizeof(struct kjsonreq));
	if (NULL == (r->arg = kcgi_writer_get(req, 0)))
		return(KCGI_ENOMEM);
	r->stack[0].type = KJSON_ROOT;
	return(KCGI_OK);
}

enum kcgi_err
kjson_close(struct kjsonreq *r)
{
	enum kcgi_err	 er = KCGI_OK;

	while (r->stackpos) {
		switch (r->stack[r->stackpos].type) {
		case (KJSON_ARRAY):
			er = kcgi_writer_putc(r->arg, ']');
			break;
		case (KJSON_STRING):
			er = kcgi_writer_putc(r->arg, '"');
			break;
		case (KJSON_OBJECT):
			er = kcgi_writer_putc(r->arg, '}');
			break;
		default:
			abort();
		}
		if (KCGI_OK != er)
			break;
		r->stackpos--;
	}

	kcgi_writer_free(r->arg);
	r->arg = NULL;
	return(er);
}

/*
 * Put a quoted JSON string into the output stream.
 * See RFC 7159, sec 7.
 */
static enum kcgi_err
kjson_write(struct kjsonreq *r, const char *cp, size_t sz, int quot)
{
	enum kcgi_err	e;
	char		enc[7];
	size_t		 i;

	if (quot && KCGI_OK != (e = kcgi_writer_putc(r->arg, '"')))
		return(e);

	for (i = 0; i < sz; i++) {
		/* Encode control characters. */
		if (cp[i] <= 0x1f) {
			snprintf(enc, sizeof(enc),
				"\\u%.4X", cp[i]);
			e = kcgi_writer_puts(r->arg, enc);
			if (KCGI_OK != e)
				return(e);
			continue;
		}
		/* Quote, solidus, reverse solidus. */
		switch (cp[i]) {
		case ('"'):
		case ('\\'):
		case ('/'):
			e = kcgi_writer_putc(r->arg, '\\');
			if (KCGI_OK != e)
				return(e);
			e = kcgi_writer_putc(r->arg, cp[i]);
			break;
		default:
			e = kcgi_writer_putc(r->arg, cp[i]);
			break;
		}
		if (KCGI_OK != e)
			return(e);
	}

	if (quot && KCGI_OK != (e = kcgi_writer_putc(r->arg, '"')))
		return(e);

	return(KCGI_OK);
}

static enum kcgi_err
kjson_puts(struct kjsonreq *r, const char *cp)
{

	return(kjson_write(r, cp, strlen(cp), 1));
}

/*
 * Check the parent of a new JSON element, which may or may not have a
 * key, which implies key-value instead of a value-only element.
 * An example of a key-value would be the inside of {foo: "bar"}, while
 * the value-only would be the surrounding object.
 * In short, we cannot have a key if our parent is the root
 * (KJSON_ROOT) or an array (KJSON_ARRAY), both of which accept
 * only values, e.g., [1, 2, 3] but not [a: b, c: d].
 * We must have a key, however, if our parent is an object, that is,
 * don't accept {4} and so on.
 * Moreover, we should never be in a string context.
 * Returns the value of kcgi_writer_puts() or KCGI_FORM on check violation.
 */
static enum kcgi_err
kjson_check(struct kjsonreq *r, const char *key)
{
	enum kcgi_err	 er;

	switch (r->stack[r->stackpos].type) {
	case (KJSON_STRING):
		return(KCGI_FORM);
	case (KJSON_OBJECT):
		if (NULL == key)
			return(KCGI_FORM);
		break;
	case (KJSON_ROOT):
		/* FALLTHROUGH */
	case (KJSON_ARRAY):
		if (NULL != key)
			return(KCGI_FORM);
		break;
	}

	if (r->stack[r->stackpos].elements++ > 0) 
		if (KCGI_OK != (er = kcgi_writer_puts(r->arg, ", ")))
			return(er);

	if (NULL != key) {
		if (KCGI_OK != (er = kjson_puts(r, key)))
			return(er);
		if (KCGI_OK != (er = kcgi_writer_puts(r->arg, ": ")))
			return(er);
	}
	return(KCGI_OK);
}

/* 
 * Only allow normal, subnormal (real small), or zero numbers.
 * The rest don't have a consistent JSON encoding.
 */
static int
kjson_check_fp(double val)
{

	switch (fpclassify(val)) {
	case (FP_ZERO):
		/* FALLTHROUGH */
	case (FP_SUBNORMAL):
		/* FALLTHROUGH */
	case (FP_NORMAL):
		break;
	default:
		return(0);
	}

	return(1);
}

static enum kcgi_err
kjson_putnumberp(struct kjsonreq *r, const char *key, const char *val)
{
	enum kcgi_err	 er;

	if (KCGI_OK != (er = kjson_check(r, key)))
		return(er);
	return(kcgi_writer_puts(r->arg, val));
}

enum kcgi_err
kjson_putstring(struct kjsonreq *r, const char *val)
{

	return(kjson_putstringp(r, NULL, val));
}

enum kcgi_err
kjson_putstringp(struct kjsonreq *r, const char *key, const char *val)
{
	enum kcgi_err	 er;

	if (KCGI_OK != (er = kjson_check(r, key)))
		return(er);
	return(kjson_puts(r, val));
}

enum kcgi_err
kjson_putdouble(struct kjsonreq *r, double val)
{

	return(kjson_putdoublep(r, NULL, val));
}

enum kcgi_err
kjson_putdoublep(struct kjsonreq *r, const char *key, double val)
{
	char		 buf[256];

	if ( ! kjson_check_fp(val))
		return(KCGI_FORM);
	(void)snprintf(buf, sizeof(buf), "%g", val);
	return(kjson_putnumberp(r, key, buf));
}

enum kcgi_err
kjson_putint(struct kjsonreq *r, int64_t val)
{

	return(kjson_putintp(r, NULL, val));
}

enum kcgi_err
kjson_putintstr(struct kjsonreq *r, int64_t val)
{

	return(kjson_putintstrp(r, NULL, val));
}

enum kcgi_err
kjson_putnullp(struct kjsonreq *r, const char *key)
{
	enum kcgi_err 	 er;

	if (KCGI_OK != (er = kjson_check(r, key)))
		return(er);
	return(kcgi_writer_puts(r->arg, "null"));
}

enum kcgi_err
kjson_putboolp(struct kjsonreq *r, const char *key, int val)
{
	enum kcgi_err	 er;

	if (KCGI_OK != (er = kjson_check(r, key)))
		return(er);
	return(kcgi_writer_puts(r->arg, val ? "true" : "false"));
}

enum kcgi_err
kjson_putnull(struct kjsonreq *r)
{

	return(kjson_putnullp(r, NULL));
}

enum kcgi_err
kjson_putbool(struct kjsonreq *r, int val)
{

	return(kjson_putboolp(r, NULL, val));
}

enum kcgi_err
kjson_putintstrp(struct kjsonreq *r, const char *key, int64_t val)
{
	char	buf[22];

	(void)snprintf(buf, sizeof(buf), "%" PRId64, val);
	return(kjson_putstringp(r, key, buf));
}


enum kcgi_err
kjson_putintp(struct kjsonreq *r, const char *key, int64_t val)
{
	char	buf[22];

	(void)snprintf(buf, sizeof(buf), "%" PRId64, val);
	return(kjson_putnumberp(r, key, buf));
}

enum kcgi_err
kjson_array_open(struct kjsonreq *r)
{

	return(kjson_arrayp_open(r, NULL));
}

enum kcgi_err
kjson_arrayp_open(struct kjsonreq *r, const char *key)
{
	enum kcgi_err	 er;

	if (KCGI_OK != (er = kjson_check(r, key)))
		return(er);

	assert(r->stackpos < KJSON_STACK_MAX - 1);
	r->stack[++r->stackpos].elements = 0;
	r->stack[r->stackpos].type = KJSON_ARRAY;
	return(kcgi_writer_putc(r->arg, '['));
}

enum kcgi_err
kjson_array_close(struct kjsonreq *r)
{

	if (0 == r->stackpos)
		return(KCGI_FORM);
	if (KJSON_ARRAY != r->stack[r->stackpos].type)
		return(KCGI_FORM);
	r->stackpos--;
	return(kcgi_writer_putc(r->arg, ']'));
}

enum kcgi_err
kjson_string_write(const char *p, size_t sz, void *arg)
{
	struct kjsonreq	*r = arg;

	if (KJSON_STRING != r->stack[r->stackpos].type)
		return(KCGI_FORM);

	return(kjson_write(r, p, sz, 0));
}

enum kcgi_err
kjson_string_puts(struct kjsonreq *r, const char *cp)
{

	return(kjson_string_write(cp, strlen(cp), r));
}

enum kcgi_err
kjson_string_putdouble(struct kjsonreq *r, double val)
{
	char	buf[256];

	if ( ! kjson_check_fp(val))
		return(KCGI_FORM);
	(void)snprintf(buf, sizeof(buf), "%g", val);
	return(kjson_string_write(buf, strlen(buf), r));
}

enum kcgi_err
kjson_string_putint(struct kjsonreq *r, int64_t val)
{
	char	buf[22];

	(void)snprintf(buf, sizeof(buf), "%" PRId64, val);
	return(kjson_string_write(buf, strlen(buf), r));
}

enum kcgi_err
kjson_string_open(struct kjsonreq *r)
{

	return(kjson_stringp_open(r, NULL));
}

enum kcgi_err
kjson_stringp_open(struct kjsonreq *r, const char *key)
{
	enum kcgi_err	 er;

	if (KCGI_OK != (er = kjson_check(r, key)))
		return(er);

	assert(r->stackpos < KJSON_STACK_MAX - 1);
	r->stack[++r->stackpos].elements = 0;
	r->stack[r->stackpos].type = KJSON_STRING;
	return(kcgi_writer_putc(r->arg, '"'));
}

enum kcgi_err
kjson_string_close(struct kjsonreq *r)
{

	if (0 == r->stackpos)
		return(KCGI_FORM);
	if (KJSON_STRING != r->stack[r->stackpos].type)
		return(KCGI_FORM);
	r->stackpos--;
	return(kcgi_writer_putc(r->arg, '"'));
}

enum kcgi_err
kjson_obj_open(struct kjsonreq *r)
{

	return(kjson_objp_open(r, NULL));
}

enum kcgi_err
kjson_objp_open(struct kjsonreq *r, const char *key)
{
	enum kcgi_err	 er;

	if (KCGI_OK != (er = kjson_check(r, key)))
		return(er);

	assert(r->stackpos < KJSON_STACK_MAX - 1);
	r->stack[++r->stackpos].elements = 0;
	r->stack[r->stackpos].type = KJSON_OBJECT;
	return(kcgi_writer_putc(r->arg, '{'));
}

enum kcgi_err
kjson_obj_close(struct kjsonreq *r)
{
	enum kcgi_err	 er;

	if (0 == r->stackpos)
		return(KCGI_FORM);
	if (KJSON_OBJECT != r->stack[r->stackpos].type)
		return(KCGI_FORM);
	if (KCGI_OK != (er = kcgi_writer_putc(r->arg, '}')))
		return(er);
	r->stackpos--;
	return(KCGI_OK);
}
