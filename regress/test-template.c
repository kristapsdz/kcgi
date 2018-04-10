/*	$Id$ */
/*
 * Copyright (c) 2017 Kristaps Dzonsons <kristaps@bsd.lv>
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
#include "../config.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../kcgi.h"

static int
test1_cmp(size_t idx, void *arg)
{
	const char *res = "foo";

	if (idx)
		return(0);

	kcgi_buf_puts(arg, res);
	return(1);
}

static int
test1_fallthrough(const char *k, size_t ksz, void *arg)
{
	const char *val = "foo", *key = "bar";
	size_t	 keysz;

	keysz = strlen(key);
	if (keysz == ksz && 0 == memcmp(key, k, keysz))
		kcgi_buf_puts(arg, val);
	return(1);
}

static int
test1(void)
{
	struct ktemplate t;
	struct ktemplatex tkx;
	const char	*keys[] = { "foobar" };
	const char	*test;
	const char	*r;
	size_t		 testsz, rsz;
	struct kcgi_buf	 b;
	int		 rc, c = 0;

	test = "abc@@foobar@@def";
	testsz = strlen(test);

	memset(&t, 0, sizeof(struct ktemplate));
	memset(&tkx, 0, sizeof(struct ktemplatex));
	memset(&b, 0, sizeof(struct kcgi_buf));

	t.key = NULL;
	t.keysz = 0;
	t.arg = &b;
	t.cb = test1_cmp;
	tkx.writer = kcgi_buf_write;
	tkx.fbk = NULL;

	rc = khttp_templatex_buf(&t, test, testsz, &tkx, &b);
	if (0 != rc)
		goto out;
	if (b.sz != testsz || memcmp(test, b.buf, testsz))
		goto out;

	free(b.buf);
	memset(&b, 0, sizeof(struct kcgi_buf));

	t.key = keys;
	t.keysz = 1;
	t.arg = &b;
	t.cb = test1_cmp;
	rc = khttp_templatex_buf(&t, test, testsz, &tkx, &b);
	r = "abcfoodef";
	rsz = strlen(r);
	if (b.sz != rsz || memcmp(r, b.buf, rsz))
		goto out;

	free(b.buf);
	memset(&b, 0, sizeof(struct kcgi_buf));

	test = "abc@@bar@@def";
	testsz = strlen(test);

	t.key = keys;
	t.keysz = 1;
	t.arg = &b;
	t.cb = test1_cmp;
	rc = khttp_templatex_buf(&t, test, testsz, &tkx, &b);
	r = "abc@@bar@@def";
	rsz = strlen(r);
	if (b.sz != rsz || memcmp(r, b.buf, rsz))
		goto out;

	free(b.buf);
	memset(&b, 0, sizeof(struct kcgi_buf));

	test = "abc@@foobar@@def";
	testsz = strlen(test);

	t.key = NULL;
	t.keysz = 0;
	t.arg = &b;
	t.cb = test1_cmp;
	tkx.fbk = test1_fallthrough;
	rc = khttp_templatex_buf(&t, test, testsz, &tkx, &b);
	r = "abc@@foobar@@def";
	rsz = strlen(r);
	if (b.sz != rsz || memcmp(r, b.buf, rsz))
		goto out;

	free(b.buf);
	memset(&b, 0, sizeof(struct kcgi_buf));

	test = "abc@@bar@@def";
	testsz = strlen(test);

	t.key = NULL;
	t.keysz = 0;
	t.arg = &b;
	t.cb = test1_cmp;
	tkx.fbk = test1_fallthrough;
	rc = khttp_templatex_buf(&t, test, testsz, &tkx, &b);
	r = "abcfoodef";
	rsz = strlen(r);
	if (b.sz != rsz || memcmp(r, b.buf, rsz))
		goto out;

	free(b.buf);
	memset(&b, 0, sizeof(struct kcgi_buf));

	test = "abc@@bar@@def@@moobar@@";
	testsz = strlen(test);

	t.key = NULL;
	t.keysz = 0;
	t.arg = &b;
	t.cb = test1_cmp;
	tkx.fbk = test1_fallthrough;
	rc = khttp_templatex_buf(&t, test, testsz, &tkx, &b);
	r = "abcfoodef@@moobar@@";
	rsz = strlen(r);
	if (b.sz != rsz || memcmp(r, b.buf, rsz))
		goto out;

	c = 1;
out:
	free(b.buf);
	return(c ? EXIT_SUCCESS : EXIT_FAILURE);
}

int
main(void)
{

	if ( ! test1())
		return(EXIT_FAILURE);

	return(EXIT_SUCCESS);
}
