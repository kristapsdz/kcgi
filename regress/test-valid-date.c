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

#include <curl/curl.h>

#include "../kcgi.h"
#include "regress.h"

static int
parent(CURL *curl)
{

	curl_easy_setopt(curl, CURLOPT_URL, 
		"http://localhost:17123/index.html?"
		"ok1=2017-01-01&"
		"ok2=1970-01-01&"
		"ok3=2016-02-29&"
		"ok4=1969-12-31&"
		"fail1=017-01-01&"
		"fail2=2017-1-01&"
		"fail3=2017-01-1&"
		"fail4=2017-13-01&"
		"fail5=2017-12-32&"
		"fail6=2018-02-29");
	return(CURLE_OK == curl_easy_perform(curl));
}

static int
child(void)
{
	struct kreq	 r;
	struct kvalid	 key[] = { 
		{ kvalid_date, "ok1" },
		{ kvalid_date, "ok2" },
		{ kvalid_date, "ok3" },
		{ kvalid_date, "ok4" },
		{ kvalid_date, "fail1" },
		{ kvalid_date, "fail2" },
		{ kvalid_date, "fail3" },
		{ kvalid_date, "fail4" },
		{ kvalid_date, "fail5" },
		{ kvalid_date, "fail6" }};
	const char 	*page[] = { "index" };

	if (KCGI_OK != khttp_parse(&r, key, sizeof(key), page, 1, 0))
		return(0);

	if (NULL == r.fieldmap[0] ||
	    NULL == r.fieldmap[1] ||
	    NULL == r.fieldmap[2] ||
	    NULL == r.fieldmap[3])
		return(0);

	if (NULL != r.fieldmap[4] ||
	    NULL != r.fieldmap[5] ||
	    NULL != r.fieldmap[6] ||
	    NULL != r.fieldmap[7] ||
	    NULL != r.fieldmap[8] ||
	    NULL != r.fieldmap[9])
		return(0);

	khttp_head(&r, kresps[KRESP_STATUS], 
		"%s", khttps[KHTTP_200]);
	khttp_head(&r, kresps[KRESP_CONTENT_TYPE], 
		"%s", kmimetypes[KMIME_TEXT_HTML]);
	khttp_body(&r);
	khttp_free(&r);
	return(1);
}

int
main(int argc, char *argv[])
{

	return(regress_cgi(parent, child) ? 
		EXIT_SUCCESS : EXIT_FAILURE);
}
