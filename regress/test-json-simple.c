/*	$Id$ */
/*
 * Copyright (c) 2018 Kristaps Dzonsons <kristaps@bsd.lv>
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

#include <curl/curl.h>

#include "../kcgi.h"
#include "../kcgijson.h"
#include "regress.h"

static size_t
bufcb(void *contents, size_t sz, size_t nm, void *dat)
{
	struct kcgi_buf	*buf = dat;

	if (KCGI_OK != kcgi_buf_write(contents, nm * sz, buf))
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
	if (CURLE_OK != curl_easy_perform(curl))
		return 0;

	rc = 0 == strcmp(buf.buf, 
		"{\"hello\": [\"world\", [\"world\"]], "
		"\"hello\": \"world\"}");
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

	if (KCGI_OK != khttp_parse(&r, NULL, 0, page, 1, 0))
		return(0);
	if (r.page)
		goto out;
	if (KMIME_APP_JSON != r.mime)
		goto out;

	rc = 1;
	khttp_head(&r, kresps[KRESP_STATUS], 
		"%s", khttps[KHTTP_200]);
	khttp_head(&r, kresps[KRESP_CONTENT_TYPE], 
		"%s", kmimetypes[r.mime]);
	khttp_body(&r);

	kjson_open(&req, &r);
	kjson_obj_open(&req);
	kjson_arrayp_open(&req, "hello");
	kjson_putstring(&req, "world");
	kjson_array_open(&req);
	kjson_putstring(&req, "world");
	kjson_array_close(&req);
	kjson_array_close(&req);
	kjson_putstringp(&req, "hello", "world");
	kjson_obj_close(&req);
	kjson_close(&req);
out:
	khttp_free(&r);
	return(rc);
}

int
main(int argc, char *argv[])
{

	return(regress_cgi(parent, child) ? 
		EXIT_SUCCESS : EXIT_FAILURE);
}
