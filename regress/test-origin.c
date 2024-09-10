/*
 * Copyright (c) Kristaps Dzonsons <kristaps@bsd.lv>
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
	struct curl_slist	*list = NULL;
	CURLcode		 c;

	curl_easy_setopt(curl, CURLOPT_URL, "http://localhost:17123/");
	list = curl_slist_append(list, "Origin: http://localhost");
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, list);
	c = curl_easy_perform(curl);
	curl_slist_free_all(list); 
	return c == CURLE_OK;
}

static int
child(void)
{
	struct kreq	 r;
	const char 	*page = "index";
	int		 rc = 0;

	if (khttp_fcgi_test())
		return 0;
	if (khttp_parse(&r, NULL, 0, &page, 1, 0) != KCGI_OK)
		return 0;
	if (r.reqmap[KREQU_ORIGIN] == NULL)
		goto out;
	if (strcasecmp(r.reqmap[KREQU_ORIGIN]->val, "http://localhost"))
		goto out;

	khttp_head(&r, kresps[KRESP_STATUS], 
		"%s", khttps[KHTTP_200]);
	khttp_head(&r, kresps[KRESP_CONTENT_TYPE], 
		"%s", kmimetypes[KMIME_TEXT_HTML]);
	khttp_head(&r, kresps[KRESP_ACCESS_CONTROL_ALLOW_ORIGIN], 
		"%s", r.reqmap[KREQU_ORIGIN]->val);
	khttp_body(&r);
	rc = 1;
out:
	khttp_free(&r);
	return rc;
}

int
main(int argc, char *argv[])
{

	return !regress_cgi(parent, child);
}
