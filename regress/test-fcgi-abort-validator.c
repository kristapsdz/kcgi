/*	$Id$ */
/*
 * Copyright (c) 2016, 2018 Kristaps Dzonsons <kristaps@bsd.lv>
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
parent(CURL *curl)
{

	curl_easy_setopt(curl, CURLOPT_HTTP09_ALLOWED, 1L);
	curl_easy_setopt(curl, CURLOPT_URL, 
		"http://localhost:17123/index.html?foo=bar");
	return CURLE_GOT_NOTHING == curl_easy_perform(curl);
}

static int
kvalid_abort(struct kpair *kp)
{

	abort();
	/* NOTREACHED */
	return(0);
}

static int
child(void)
{
	struct kreq	 r;
	const char 	*page = "index";
	struct kfcgi	*fcgi;
	struct kvalid	 key = { kvalid_abort, "foo" };
	enum kcgi_err	 er;

	if ( ! khttp_fcgi_test())
		return 0;
	if (KCGI_OK != khttp_fcgi_init(&fcgi, &key, 1, &page, 1, 0))
		return 0;
	if (KCGI_OK == (er = khttp_fcgi_parse(fcgi, &r)))
		khttp_free(&r);
	khttp_fcgi_free(fcgi);
	return KCGI_FORM == er ? 1 : 0;
}

int
main(int argc, char *argv[])
{

	return regress_fcgi(parent, child) ? 
		EXIT_SUCCESS : EXIT_FAILURE;
}
