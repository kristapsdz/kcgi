/*	$Id$ */
/*
 * Copyright (c) 2018, 2020 Kristaps Dzonsons <kristaps@bsd.lv>
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

#if HAVE_ERR
# include <err.h>
#endif
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <curl/curl.h>

#include "../kcgi.h"
#include "regress.h"

static size_t
bufcb(void *contents, size_t sz, size_t nm, void *dat)
{
	struct kcgi_buf	*buf = dat;

	if (kcgi_buf_write(contents, nm * sz, buf) != KCGI_OK)
		return 0;
	return nm * sz;
}

static int
parent(CURL *curl)
{
	struct kcgi_buf	 buf;
	int		 rc;
	unsigned char	 want[] = {
		'a', 'b', 'c',
		'-', '1', '2',
		'a', 'b', 'c',
		0, 10, 
		/* We'll lose bits, but that's the point. */
		(char)256, (char)257,
		0, 1, 2
	};

	memset(&buf, 0, sizeof(struct kcgi_buf));

	curl_easy_setopt(curl, CURLOPT_URL, 
		"http://localhost:17123/index");
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, bufcb);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buf);
	if (curl_easy_perform(curl) != CURLE_OK)
		return 0;

	/* Check same string and NUL terminated. */

	rc = buf.sz == sizeof(want) &&
   	     memcmp(buf.buf, want, buf.sz) == 0 &&
	     buf.buf[buf.sz] == '\0';

	free(buf.buf);
	return rc;
}

static int
child(void)
{
	struct kreq	 r;
	const char 	*page[] = { "index" };
	int		 rc = 0;
	char		 bbuf[3] = { 0, 1, 2 };
	struct kcgi_buf	 buf;

	if (khttp_parse(&r, NULL, 0, page, 1, 0) != KCGI_OK) {
		warnx("khttp_parse");
		return 0;
	} else if (r.page != 0)
		goto out;

	/* Setup */

	if (khttp_head(&r, kresps[KRESP_STATUS],
	    "%s", khttps[KHTTP_200]) != KCGI_OK) {
		warnx("khttp_head");
		goto out;
	} else if (khttp_body(&r) != KCGI_OK) {
		warnx("khttp_body");
		goto out;
	}

	/* Tests. */

	memset(&buf, 0, sizeof(struct kcgi_buf));

	if (kcgi_buf_puts(&buf, "abc") != KCGI_OK) {
		warnx("kcgi_buf_puts");
		goto out;
	} else if (kcgi_buf_puts(&buf, NULL) != KCGI_OK) {
		warnx("kcgi_buf_puts");
		goto out;
	} else if (kcgi_buf_puts(&buf, "") != KCGI_OK) {
		warnx("kcgi_buf_puts");
		goto out;
	} else if (kcgi_buf_printf(&buf, "%d", -12) != KCGI_OK) {
		warnx("kcgi_buf_printf");
		goto out;
	} else if (kcgi_buf_printf(&buf, NULL) != KCGI_OK) {
		warnx("kcgi_buf_printf");
		goto out;
	} else if (kcgi_buf_printf(&buf, "%s", "abc") != KCGI_OK) {
		warnx("kcgi_buf_printf");
		goto out;
	} else if (kcgi_buf_putc(&buf, 0) != KCGI_OK) {
		warnx("kcgi_buf_putc");
		goto out;
	} else if (kcgi_buf_putc(&buf, 10) != KCGI_OK) {
		warnx("kcgi_buf_putc");
		goto out;
	} else if (kcgi_buf_putc(&buf, (char)256) != KCGI_OK) {
		warnx("kcgi_buf_putc");
		goto out;
	} else if (kcgi_buf_putc(&buf, (char)257) != KCGI_OK) {
		warnx("kcgi_buf_putc");
		goto out;
	} else if (kcgi_buf_write(bbuf, sizeof(bbuf), &buf) != KCGI_OK) {
		warnx("khttp_putc");
		goto out;
	} else if (kcgi_buf_write(NULL, sizeof(bbuf), &buf) != KCGI_OK) {
		warnx("khttp_putc");
		goto out;
	} else if (kcgi_buf_write(bbuf, 0, &buf) != KCGI_OK) {
		warnx("khttp_putc");
		goto out;
	}

	khttp_write(&r, buf.buf, buf.sz);
	free(buf.buf);
	rc = 1;
out:
	khttp_free(&r);
	return rc;
}

int
main(int argc, char *argv[])
{

	return regress_cgi(parent, child) ? 
		EXIT_SUCCESS : EXIT_FAILURE;
}
