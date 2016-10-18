/*	$Id$ */
/*
 * Copyright (c) 2014 Kristaps Dzonsons <kristaps@bsd.lv>
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
#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

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
		"http://localhost:17123/test/foo/bar/baz.xml");
	return(CURLE_OK == curl_easy_perform(curl));
}

static int
child(void)
{
	struct kreq	 r;
	const char 	*page[] = { "index", "test" };
	int		 rc = 0;
	struct kfcgi	*fcgi;
	enum kcgi_err	 er;

	if (KCGI_OK != khttp_fcgi_init(&fcgi, NULL, 0, page, 2, 0))
		return(0);

	while (KCGI_OK == (er = khttp_fcgi_parse(fcgi, &r))) {
		if (1 != r.page)
			goto out;
		if (strcmp(r.fullpath, "/test/foo/bar/baz.xml"))
			goto out;
		if (strcmp(r.path, "foo/bar/baz"))
			goto out;
		if (strcmp(r.suffix, "xml"))
			goto out;
		if (KMIME_TEXT_XML != r.mime)
			goto out;
		khttp_head(&r, kresps[KRESP_STATUS], 
			"%s", khttps[KHTTP_200]);
		khttp_head(&r, kresps[KRESP_CONTENT_TYPE], 
			"%s", kmimetypes[KMIME_TEXT_HTML]);
		khttp_body(&r);
		khttp_free(&r);
	}
	rc = 1;
out:
	khttp_free(&r);
	khttp_fcgi_free(fcgi);
	return(rc);
}

int
main(int argc, char *argv[])
{

	return(regress_fcgi(parent, child) ? EXIT_SUCCESS : EXIT_FAILURE);
}
