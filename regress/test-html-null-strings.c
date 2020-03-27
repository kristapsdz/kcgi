/*	$Id$ */
/*
 * Copyright (c) 2020 Kristaps Dzonsons <kristaps@bsd.lv>
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
#include "../kcgihtml.h"
#include "regress.h"

#define	EXPECT \
	"<!DOCTYPE html><html><body></body></html>"

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
		"http://localhost:17123/index.html");
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, bufcb);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buf);
	if (curl_easy_perform(curl) != CURLE_OK) {
		warnx("curl_easy_perform");
		return 0;
	}
	if (!(rc = strcmp(buf.buf, EXPECT) == 0))
		warnx("content test failure: %s", buf.buf);
	free(buf.buf);
	return rc;
}

static int
child(void)
{
	struct kreq	 r;
	struct khtmlreq	 req;
	const char 	*page[] = { "index" };
	int		 rc = 0;

	if (khttp_parse(&r, NULL, 0, page, 1, 0) != KCGI_OK) {
		warnx("khttp_parse");
		return 0;
	} else if (r.page != 0 || r.mime != KMIME_TEXT_HTML)
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

	if (khtml_open(&req, &r, 0) != KCGI_OK) {
		warnx("khtml_open");
		goto out;
	}

	kcgi_writer_disable(&r);
	
	if (khtml_elem(&req, KELEM_DOCTYPE) != KCGI_OK ||
	    khtml_elem(&req, KELEM_HTML) != KCGI_OK ||
	    khtml_elem(&req, KELEM_BODY) != KCGI_OK) {
		warnx("khtml_elem");
		goto out;
	}

	if (khtml_puts(&req, NULL) != KCGI_OK ||
	    khtml_printf(&req, NULL) != KCGI_OK ||
	    khtml_write(NULL, 9, &req) != KCGI_OK ||
	    khtml_write("abcdef", 0, &req) != KCGI_OK) {
		warnx("khtml_puts/write");
		goto out;
	}
	
	if (khtml_close(&req) != KCGI_OK) {
		warnx("khtml_close");
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
