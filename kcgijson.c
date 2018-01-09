/*	$Id$ */
/*
 * Copyright (c) 2012, 2014, 2015 Kristaps Dzonsons <kristaps@bsd.lv>
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
#include "kcgijson.h"

void
kjson_open(struct kjsonreq *r, struct kreq *req)
{

	memset(r, 0, sizeof(struct kjsonreq));
	r->req = req;
	r->stack[0].type = KJSON_ROOT;
}

int
kjson_close(struct kjsonreq *r)
{
	int	 i;

	i = r->stackpos > 0;
	while (r->stackpos) {
		switch (r->stack[r->stackpos].type) {
		case (KJSON_ARRAY):
			khttp_putc(r->req, ']');
			break;
		case (KJSON_STRING):
			khttp_putc(r->req, '"');
			break;
		case (KJSON_OBJECT):
			khttp_putc(r->req, '}');
			break;
		default:
			abort();
		}
		r->stackpos--;
	}

	return(i);
}

/*
 * Put a quoted JSON string into the output stream.
 */
static enum kcgi_err
kjson_puts(struct kjsonreq *r, const char *cp)
{
	int	 	c;
	enum kcgi_err	e;

	if (KCGI_OK != (e = khttp_putc(r->req, '"')))
		return(e);

	while ('\0' != (c = *cp++)) {
		switch (c) {
		case ('"'):
		case ('\\'):
		case ('/'):
			if (KCGI_OK != (e = khttp_putc(r->req, '\\')))
				return(e);
			e = khttp_putc(r->req, c);
			break;
		case ('\b'):
			e = khttp_puts(r->req, "\\b");
			break;
		case ('\f'):
			e = khttp_puts(r->req, "\\f");
			break;
		case ('\n'):
			e = khttp_puts(r->req, "\\n");
			break;
		case ('\r'):
			e = khttp_puts(r->req, "\\r");
			break;
		case ('\t'):
			e = khttp_puts(r->req, "\\t");
			break;
		default:
			e = khttp_putc(r->req, c);
			break;
		}
		if (KCGI_OK != e)
			return(e);
	}

	return(khttp_putc(r->req, '"'));
}

static enum kcgi_err
kjson_check(struct kjsonreq *r, const char *key)
{
	enum kcgi_err	 er;

	/* We should never be in a string context. */

	if (KJSON_STRING == r->stack[r->stackpos].type)
		goto out;

	/*
	 * Check the parent of a new JSON value.
	 * In short, we cannot have a key if our parent is the root
	 * (KJSON_ROOT) or an array (KJSON_ARRAY), both of which accept
	 * only values.
	 */

	if (NULL != key && KJSON_OBJECT == r->stack[r->stackpos].type)
		goto out;
	if (NULL == key && KJSON_ARRAY == r->stack[r->stackpos].type)
		goto out;
	if (NULL == key && KJSON_ROOT == r->stack[r->stackpos].type)
		goto out;

	return(KCGI_FORM);
out:
	if (r->stack[r->stackpos].elements++ > 0) 
		if (KCGI_OK != (er = khttp_puts(r->req, ", ")))
			return(er);

	if (NULL != key) {
		if (KCGI_OK != (er = kjson_puts(r, key)))
			return(er);
		if (KCGI_OK != (er = khttp_puts(r->req, ": ")))
			return(er);
	}
	return(KCGI_OK);
}

static enum kcgi_err
kjson_putnumberp(struct kjsonreq *r, const char *key, const char *val)
{

	if ( ! kjson_check(r, key))
		return(KCGI_FORM);
	return(khttp_puts(r->req, val));
}

enum kcgi_err
kjson_putstring(struct kjsonreq *r, const char *val)
{

	return(kjson_putstringp(r, NULL, val));
}

enum kcgi_err
kjson_putstringp(struct kjsonreq *r, const char *key, const char *val)
{

	if ( ! kjson_check(r, key))
		return(KCGI_FORM);
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
	char	buf[256];

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

	if ( ! kjson_check(r, key))
		return(KCGI_FORM);
	return(khttp_puts(r->req, "null"));
}

enum kcgi_err
kjson_putboolp(struct kjsonreq *r, const char *key, int val)
{

	if ( ! kjson_check(r, key))
		return(KCGI_FORM);
	return(khttp_puts(r->req, val ? "true" : "false"));
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

	if ( ! kjson_check(r, key))
		return(KCGI_FORM);

	r->stack[r->stackpos].elements++;
	r->stack[++r->stackpos].elements = 0;
	r->stack[r->stackpos].type = KJSON_ARRAY;
	assert(r->stackpos < 128);
	return(khttp_putc(r->req, '['));
}

enum kcgi_err
kjson_array_close(struct kjsonreq *r)
{

	if (0 == r->stackpos)
		return(KCGI_FORM);
	if (KJSON_ARRAY != r->stack[r->stackpos].type)
		return(KCGI_FORM);
	r->stackpos--;
	return(khttp_putc(r->req, ']'));
}

enum kcgi_err
kjson_string_write(const char *p, size_t sz, void *arg)
{
	struct kjsonreq	*r = arg;
	enum kcgi_err	 er;
	size_t		 i;

	if (KJSON_STRING != r->stack[r->stackpos].type)
		return(KCGI_FORM);

	for (i = 0; i < sz; i++) {
		switch (p[i]) {
		case ('"'):
		case ('\\'):
		case ('/'):
			er = khttp_putc(r->req, '\\');
			if (KCGI_OK != er)
				return(er);
			er = khttp_putc(r->req, p[i]);
			break;
		case ('\b'):
			er = khttp_puts(r->req, "\\b");
			break;
		case ('\f'):
			er = khttp_puts(r->req, "\\f");
			break;
		case ('\n'):
			er = khttp_puts(r->req, "\\n");
			break;
		case ('\r'):
			er = khttp_puts(r->req, "\\r");
			break;
		case ('\t'):
			er = khttp_puts(r->req, "\\t");
			break;
		default:
			er = khttp_putc(r->req, p[i]);
			break;
		}
		if (KCGI_OK != er)
			return(er);
	}

	return(KCGI_OK);
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

	if ( ! kjson_check(r, key))
		return(KCGI_FORM);

	r->stack[r->stackpos].elements++;
	r->stack[++r->stackpos].elements = 0;
	r->stack[r->stackpos].type = KJSON_STRING;
	assert(r->stackpos < 128);
	return(khttp_putc(r->req, '"'));
}

enum kcgi_err
kjson_string_close(struct kjsonreq *r)
{

	if (0 == r->stackpos)
		return(KCGI_FORM);
	if (KJSON_STRING != r->stack[r->stackpos].type)
		return(KCGI_FORM);
	r->stackpos--;
	return(khttp_putc(r->req, '"'));
}

enum kcgi_err
kjson_obj_open(struct kjsonreq *r)
{

	return(kjson_objp_open(r, NULL));
}

enum kcgi_err
kjson_objp_open(struct kjsonreq *r, const char *key)
{

	if ( ! kjson_check(r, key))
		return(KCGI_FORM);

	r->stack[r->stackpos].elements++;
	r->stack[++r->stackpos].elements = 0;
	r->stack[r->stackpos].type = KJSON_OBJECT;
	assert(r->stackpos < 128);
	return(khttp_putc(r->req, '{'));
}

enum kcgi_err
kjson_obj_close(struct kjsonreq *r)
{
	enum kcgi_err	 er;

	if (0 == r->stackpos)
		return(KCGI_FORM);
	if (KJSON_OBJECT != r->stack[r->stackpos].type)
		return(KCGI_FORM);
	if (KCGI_OK != (er = khttp_putc(r->req, '}')))
		return(er);
	r->stackpos--;
	return(KCGI_OK);
}
