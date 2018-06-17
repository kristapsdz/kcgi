/*	$Id$ */
/*
 * Copyright (c) 2018 Charles Collicutt <charles@collicutt.co.uk>
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
	struct curl_slist *list = NULL;
	const char *body = "PLAIN TEXT";
	int c;

	curl_easy_setopt(curl, CURLOPT_URL,
		"http://localhost:17123/plain.txt");
	curl_easy_setopt(curl, CURLOPT_POST, 1);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body);
	list = curl_slist_append(list,
		"Authorization: Digest username=\"admin\","
		"realm=\"AuthInt Example\","
		"nonce=\"367sj3265s5\","
		"uri=\"/plain.txt\","
		"qop=auth-int,"
		"nc=000001ff,"
		"cnonce=\"hxk1lu63b6c7vhk\","
		"response=\"517a8a2617faed1847835f3ec271ca38\","
		"opaque=\"87aaxcval4gba36\"");
	list = curl_slist_append(list,
		"Content-Type: application/octet-stream");
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, list);
	c = curl_easy_perform(curl);
	curl_slist_free_all(list);
	return(CURLE_OK == c);
}

static int
child(void)
{
	struct kreq	 r;
	const char 	*page = "index";
	int		 rc;

	rc = 0;
	if (khttp_fcgi_test())
		return(0);
	if (KCGI_OK != khttp_parse(&r, NULL, 0, &page, 1, 0))
		return(0);
	if (KAUTH_DIGEST != r.rawauth.type)
		goto out;
	else if (0 == r.rawauth.authorised)
		goto out;
	else if (strcmp(r.rawauth.d.digest.user, "admin"))
		goto out;
	else if (strcmp(r.rawauth.d.digest.realm, "AuthInt Example"))
		goto out;
	else if (strcmp(r.rawauth.d.digest.uri, "/plain.txt"))
		goto out;
	else if (khttpdigest_validate(&r, "12435") <= 0)
		goto out;

	khttp_head(&r, kresps[KRESP_STATUS],
		"%s", khttps[KHTTP_200]);
	khttp_head(&r, kresps[KRESP_CONTENT_TYPE],
		"%s", kmimetypes[KMIME_TEXT_HTML]);
	khttp_body(&r);
	rc = 1;
out:
	khttp_free(&r);
	return(rc);
}

int
main(int argc, char *argv[])
{

	return(regress_cgi(parent, child) ? EXIT_SUCCESS : EXIT_FAILURE);
}
