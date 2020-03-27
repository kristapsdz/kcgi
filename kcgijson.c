/*	$Id$ */
/*
 * Copyright (c) 2012, 2014, 2015, 2017, 2020 Kristaps Dzonsons <kristaps@bsd.lv>
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
	if ((r->arg = kcgi_writer_get(req, 0)) == NULL)
		return KCGI_ENOMEM;

	r->stack[0].type = KJSON_ROOT;
	return KCGI_OK;
}

enum kcgi_err
kjson_close(struct kjsonreq *r)
{
	enum kcgi_err	 er = KCGI_OK;

	while (r->stackpos) {
		switch (r->stack[r->stackpos].type) {
		case KJSON_ARRAY:
			er = kcgi_writer_putc(r->arg, ']');
			break;
		case KJSON_STRING:
			er = kcgi_writer_putc(r->arg, '"');
			break;
		case KJSON_OBJECT:
			er = kcgi_writer_putc(r->arg, '}');
			break;
		default:
			abort();
		}
		if (er != KCGI_OK)
			break;
		r->stackpos--;
	}

	kcgi_writer_free(r->arg);
	r->arg = NULL;
	return er;
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
	unsigned char	c;
	size_t		i;

	if (quot && (e = kcgi_writer_putc(r->arg, '"')) != KCGI_OK)
		return e;

	for (i = 0; i < sz; i++) {
		/* Make sure we're looking at the unsigned value. */
		c = cp[i];
		/* Encode control characters. */
		if (c <= 0x1f) {
			snprintf(enc, sizeof(enc), "\\u%.4X", c);
			e = kcgi_writer_puts(r->arg, enc);
			if (e != KCGI_OK)
				return e;
			continue;
		}
		/* Quote, solidus, reverse solidus. */
		switch (c) {
		case '"':
		case '\\':
		case '/':
			e = kcgi_writer_putc(r->arg, '\\');
			if (e != KCGI_OK)
				return e;
			break;
		default:
			break;
		}
		if ((e = kcgi_writer_putc(r->arg, cp[i])) != KCGI_OK)
			return e;
	}

	if (quot && (e = kcgi_writer_putc(r->arg, '"')) != KCGI_OK)
		return e;

	return KCGI_OK;
}

static enum kcgi_err
kjson_puts(struct kjsonreq *r, const char *cp)
{

	if (cp == NULL)
		return KCGI_OK;
	return kjson_write(r, cp, strlen(cp), 1);
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
 * Returns the value of kcgi_writer_puts() or KCGI_WRITER on check
 * violation.
 */
static enum kcgi_err
kjson_check(struct kjsonreq *r, const char *key)
{
	enum kcgi_err	 er;

	switch (r->stack[r->stackpos].type) {
	case KJSON_STRING:
		return KCGI_WRITER;
	case KJSON_OBJECT:
		if (key == NULL)
			return KCGI_WRITER;
		break;
	case KJSON_ROOT:
	case KJSON_ARRAY:
		if (key != NULL)
			return KCGI_WRITER;
		break;
	}

	if (r->stack[r->stackpos].elements++ > 0) 
		if ((er = kcgi_writer_puts(r->arg, ", ")) != KCGI_OK)
			return er;

	if (key != NULL) {
		if ((er = kjson_puts(r, key)) != KCGI_OK)
			return er;
		if ((er = kcgi_writer_puts(r->arg, ": ")) != KCGI_OK)
			return er;
	}
	return KCGI_OK;
}

/* 
 * Only allow normal, subnormal (real small), or zero numbers.
 * The rest don't have a consistent JSON encoding.
 */
static int
kjson_check_fp(double val)
{

	switch (fpclassify(val)) {
	case FP_ZERO:
	case FP_SUBNORMAL:
	case FP_NORMAL:
		break;
	default:
		return 0;
	}

	return 1;
}

/*
 * Emit a key-pair "key" with an unquoted value "val".
 * This should only be used for trusted values: numbers, boolean, null,
 * and the like.
 * It should never be passed anything untrusted.
 * Returns from kcgi_writer_puts and kjson_check.
 */
static enum kcgi_err
kjson_puttrustedp(struct kjsonreq *r, const char *key, const char *val)
{
	enum kcgi_err	 er;

	if ((er = kjson_check(r, key)) != KCGI_OK)
		return er;
	return kcgi_writer_puts(r->arg, val);
}

enum kcgi_err
kjson_putstring(struct kjsonreq *r, const char *val)
{

	return kjson_putstringp(r, NULL, val);
}

enum kcgi_err
kjson_putstringp(struct kjsonreq *r, const char *key, const char *val)
{
	enum kcgi_err	 er;

	if (val == NULL)
		return KCGI_OK;
	if ((er = kjson_check(r, key)) != KCGI_OK)
		return er;
	return kjson_puts(r, val);
}

enum kcgi_err
kjson_putdouble(struct kjsonreq *r, double val)
{

	return kjson_putdoublep(r, NULL, val);
}

enum kcgi_err
kjson_putdoublep(struct kjsonreq *r, const char *key, double val)
{
	char		 buf[256];

	if (!kjson_check_fp(val))
		return kjson_puttrustedp(r, key, "null");

	(void)snprintf(buf, sizeof(buf), "%g", val);
	return kjson_puttrustedp(r, key, buf);
}

enum kcgi_err
kjson_putint(struct kjsonreq *r, int64_t val)
{

	return kjson_putintp(r, NULL, val);
}

enum kcgi_err
kjson_putintstr(struct kjsonreq *r, int64_t val)
{

	return kjson_putintstrp(r, NULL, val);
}

enum kcgi_err
kjson_putnullp(struct kjsonreq *r, const char *key)
{

	return kjson_puttrustedp(r, key, "null");
}

enum kcgi_err
kjson_putboolp(struct kjsonreq *r, const char *key, int val)
{

	return kjson_puttrustedp(r, key, val ? "true" : "false");
}

enum kcgi_err
kjson_putnull(struct kjsonreq *r)
{

	return kjson_putnullp(r, NULL);
}

enum kcgi_err
kjson_putbool(struct kjsonreq *r, int val)
{

	return kjson_putboolp(r, NULL, val);
}

enum kcgi_err
kjson_putintstrp(struct kjsonreq *r, const char *key, int64_t val)
{
	char	buf[22];

	(void)snprintf(buf, sizeof(buf), "%" PRId64, val);
	return kjson_putstringp(r, key, buf);
}


enum kcgi_err
kjson_putintp(struct kjsonreq *r, const char *key, int64_t val)
{
	char	buf[22];

	(void)snprintf(buf, sizeof(buf), "%" PRId64, val);
	return kjson_puttrustedp(r, key, buf);
}

enum kcgi_err
kjson_array_open(struct kjsonreq *r)
{

	return kjson_arrayp_open(r, NULL);
}

enum kcgi_err
kjson_arrayp_open(struct kjsonreq *r, const char *key)
{
	enum kcgi_err	 er;

	if ((er = kjson_check(r, key)) != KCGI_OK)
		return er;
	if ((er = kcgi_writer_putc(r->arg, '[')) != KCGI_OK)
		return er;

	if (r->stackpos >= KJSON_STACK_MAX - 1) {
		XWARNX("maximum json stack size exceeded");
		abort();
	}
	r->stack[++r->stackpos].elements = 0;
	r->stack[r->stackpos].type = KJSON_ARRAY;
	return KCGI_OK;
}

enum kcgi_err
kjson_array_close(struct kjsonreq *r)
{
	enum kcgi_err	 er;

	if (r->stackpos == 0)
		return KCGI_WRITER;
	if (r->stack[r->stackpos].type != KJSON_ARRAY)
		return KCGI_WRITER;
	if ((er = kcgi_writer_putc(r->arg, ']')) != KCGI_OK)
		return er;
	r->stackpos--;
	return KCGI_OK;
}

enum kcgi_err
kjson_string_write(const char *p, size_t sz, void *arg)
{
	struct kjsonreq	*r = arg;

	if (r->stack[r->stackpos].type != KJSON_STRING)
		return KCGI_WRITER;

	if (p == NULL || sz == 0)
		return KCGI_OK;

	return kjson_write(r, p, sz, 0);
}

enum kcgi_err
kjson_string_puts(struct kjsonreq *r, const char *cp)
{

	if (cp == NULL)
		return KCGI_OK;

	return kjson_string_write(cp, strlen(cp), r);
}

enum kcgi_err
kjson_string_putdouble(struct kjsonreq *r, double val)
{
	char	buf[256];

	if (!kjson_check_fp(val))
		return kjson_string_write("null", 4, r);

	(void)snprintf(buf, sizeof(buf), "%g", val);
	return kjson_string_write(buf, strlen(buf), r);
}

enum kcgi_err
kjson_string_putint(struct kjsonreq *r, int64_t val)
{
	char	buf[22];

	(void)snprintf(buf, sizeof(buf), "%" PRId64, val);
	return kjson_string_write(buf, strlen(buf), r);
}

enum kcgi_err
kjson_string_open(struct kjsonreq *r)
{

	return kjson_stringp_open(r, NULL);
}

enum kcgi_err
kjson_stringp_open(struct kjsonreq *r, const char *key)
{
	enum kcgi_err	 er;

	if ((er = kjson_check(r, key)) != KCGI_OK)
		return er;
	if ((er = kcgi_writer_putc(r->arg, '"')) != KCGI_OK)
		return er;

	if (r->stackpos >= KJSON_STACK_MAX - 1) {
		XWARNX("maximum json stack size exceeded");
		abort();
	}
	r->stack[++r->stackpos].elements = 0;
	r->stack[r->stackpos].type = KJSON_STRING;
	return KCGI_OK;
}

enum kcgi_err
kjson_string_close(struct kjsonreq *r)
{
	enum kcgi_err	 er;

	if (r->stackpos == 0 ||
	    r->stack[r->stackpos].type != KJSON_STRING)
		return KCGI_WRITER;
	if ((er = kcgi_writer_putc(r->arg, '"')) != KCGI_OK)
		return(er);
	r->stackpos--;
	return KCGI_OK;
}

enum kcgi_err
kjson_obj_open(struct kjsonreq *r)
{

	return kjson_objp_open(r, NULL);
}

enum kcgi_err
kjson_objp_open(struct kjsonreq *r, const char *key)
{
	enum kcgi_err	 er;

	if ((er = kjson_check(r, key)) != KCGI_OK)
		return er;
	if ((er = kcgi_writer_putc(r->arg, '{')) != KCGI_OK)
		return er;

	if (r->stackpos >= KJSON_STACK_MAX - 1) {
		XWARNX("maximum json stack size exceeded");
		abort();
	}
	r->stack[++r->stackpos].elements = 0;
	r->stack[r->stackpos].type = KJSON_OBJECT;
	return KCGI_OK;
}

enum kcgi_err
kjson_obj_close(struct kjsonreq *r)
{
	enum kcgi_err	 er;

	if (r->stackpos == 0 ||
	    r->stack[r->stackpos].type != KJSON_OBJECT)
		return KCGI_WRITER;

	if ((er = kcgi_writer_putc(r->arg, '}')) != KCGI_OK)
		return er;

	r->stackpos--;
	return KCGI_OK;
}
