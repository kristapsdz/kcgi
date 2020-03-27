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
#include "../kcgijson.h"
#include "regress.h"

#define	REGRESS \
	"{\"hello\": [1234, [\"world\", 1.2, null, true]], " \
	"\"hello\": \"world\"}"

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

	memset(&buf, 0, sizeof(struct kcgi_buf));

	curl_easy_setopt(curl, CURLOPT_URL, 
		"http://localhost:17123/index.json");
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, bufcb);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buf);
	if (curl_easy_perform(curl) != CURLE_OK) {
		warnx("curl_easy_perform");
		return 0;
	}

	if (!(rc = strcmp(buf.buf, REGRESS) == 0)) 
		warnx("content test failure: %s", buf.buf);
	free(buf.buf);
	return rc;
}

static int
child(void)
{
	struct kreq	 r;
	struct kjsonreq	 req;
	const char 	*page[] = { "index" };
	int		 rc = 0;

	if (khttp_parse(&r, NULL, 0, page, 1, 0) != KCGI_OK) {
		warnx("khttp_parse");
		return 0;
	} else if (r.page != 0 || r.mime != KMIME_APP_JSON)
		goto out;

	/* Setup. */

	if (khttp_head(&r, kresps[KRESP_STATUS],
	    "%s", khttps[KHTTP_200]) != KCGI_OK) {
		warnx("khttp_head");
		goto out;
	} else if (khttp_head(&r, kresps[KRESP_CONTENT_TYPE],
	    "%s", kmimetypes[r.mime]) != KCGI_OK) {
		warnx("khttp_head");
		goto out;
	} else if (khttp_body(&r) != KCGI_OK) {
		warnx("khttp_body");
		goto out;
	}

	if (kjson_open(&req, &r) != KCGI_OK) {
		warnx("kjson_open");
		goto out;
	} else if (kjson_objp_open(&req, "foo") != KCGI_WRITER) {
		warnx("kjson_obj_open should fail");
		goto out;
	} else if (kjson_obj_open(&req) != KCGI_OK) {
		warnx("kjson_obj_open");
		goto out;
	} else if (kjson_arrayp_open(&req, "hello") != KCGI_OK) {
		warnx("kjson_arrayp_open");
		goto out;
	} else if (kjson_putint(&req, 1234) != KCGI_OK) {
		warnx("kjson_putstring");
		goto out;
	} else if (kjson_putintp(&req, "foo", 1234) != KCGI_WRITER) {
		warnx("kjson_putstring should fail");
		goto out;
	} else if (kjson_array_open(&req) != KCGI_OK) {
		warnx("kjson_array_open");
		goto out;
	} else if (kjson_putstring(&req, "world") != KCGI_OK) {
		warnx("kjson_putstring");
		goto out;
	} else if (kjson_putstring(&req, NULL) != KCGI_OK) {
		warnx("kjson_putstring");
		goto out;
	} else if (kjson_putstringp(&req, "foo", "bar") != KCGI_WRITER) {
		warnx("kjson_putstring"); /* should fail */
		goto out;
	} else if (kjson_putdouble(&req, 1.2) != KCGI_OK) {
		warnx("kjson_double");
		goto out;
	} else if (kjson_putdouble(&req, 1.0 / 0.0) != KCGI_OK) {
		warnx("kjson_double");
		goto out;
	} else if (kjson_putbool(&req, 1) != KCGI_OK) {
		warnx("kjson_double");
		goto out;
	} else if (kjson_array_close(&req) != KCGI_OK ||
	           kjson_array_close(&req) != KCGI_OK) {
		warnx("kjson_array_close");
		goto out;
	} else if (kjson_putstringp(&req, "hello", "world") != KCGI_OK) {
		warnx("kjson_putstringp");
		goto out;
	} else if (kjson_close(&req) != KCGI_OK) {
		warnx("kjson_close");
		goto out;
	}

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
