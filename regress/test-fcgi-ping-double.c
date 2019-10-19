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
#include <unistd.h>

#include <curl/curl.h>

#include "../kcgi.h"
#include "regress.h"

static int
parent1(CURL *curl)
{

	curl_easy_setopt(curl, CURLOPT_HTTP09_ALLOWED, 1L);
	curl_easy_setopt(curl, CURLOPT_URL, 
		"http://localhost:17123/index1.html");
	return CURLE_OK == curl_easy_perform(curl);
}

static int
parent2(CURL *curl)
{

	curl_easy_setopt(curl, CURLOPT_HTTP09_ALLOWED, 1L);
	curl_easy_setopt(curl, CURLOPT_URL, 
		"http://localhost:17123/index2.html");
	return CURLE_OK == curl_easy_perform(curl);
}

static int
child1(void)
{
	struct kreq	 r;
	const char 	*page[] = { "index1", "index2" };
	struct kfcgi	*fcgi;
	enum kcgi_err	 er;

	if ( ! khttp_fcgi_test())
		return 0;
	if (KCGI_OK != khttp_fcgi_init(&fcgi, NULL, 0, page, 2, 1))
		return 0;

	while (KCGI_OK == (er = khttp_fcgi_parse(fcgi, &r))) {
		if (0 != r.page) {
			khttp_free(&r);
			khttp_fcgi_free(fcgi);
			return 0;
		}
		khttp_head(&r, kresps[KRESP_STATUS], 
			"%s", khttps[KHTTP_200]);
		khttp_head(&r, kresps[KRESP_CONTENT_TYPE], 
			"%s", kmimetypes[KMIME_TEXT_HTML]);
		khttp_body(&r);
		khttp_free(&r);
	}

	khttp_free(&r);
	khttp_fcgi_free(fcgi);
	return KCGI_HUP == er ? 1 : 0;
}

static int
child2(void)
{
	struct kreq	 r;
	const char 	*page[] = { "index1", "index2" };
	struct kfcgi	*fcgi;
	enum kcgi_err	 er;

	if ( ! khttp_fcgi_test())
		return 0;
	if (KCGI_OK != khttp_fcgi_init(&fcgi, NULL, 0, page, 2, 0))
		return 0;

	while (KCGI_OK == (er = khttp_fcgi_parse(fcgi, &r))) {
		if (1 != r.page) {
			khttp_free(&r);
			khttp_fcgi_free(fcgi);
			return 0;
		}
		khttp_head(&r, kresps[KRESP_STATUS], 
			"%s", khttps[KHTTP_200]);
		khttp_head(&r, kresps[KRESP_CONTENT_TYPE], 
			"%s", kmimetypes[KMIME_TEXT_HTML]);
		khttp_body(&r);
		khttp_free(&r);
	}

	khttp_free(&r);
	khttp_fcgi_free(fcgi);
	return KCGI_HUP == er ? 1 : 0;
}

int
main(int argc, char *argv[])
{

	if ( ! regress_fcgi(parent1, child1))
		return EXIT_FAILURE;
	return regress_fcgi(parent2, child2) ? 
		EXIT_SUCCESS : EXIT_FAILURE;
}
