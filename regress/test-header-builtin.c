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
#include "regress.h"

#define	HTTP_ACCEPT		"text/html"
#define	HTTP_CHARSET		"utf-8"
#define	HTTP_ENCODING		"identity"
#define	HTTP_LANGUAGE		"en-UK"
#define	HTTP_DEPTH		"0"
#define	HTTP_FROM		"webmaster@example.org"
#define	HTTP_HOST		"example.org"
#define HTTP_IF			"\"1234567890\""
#define HTTP_IFMODSINCE		"Wed, 21 Oct 2015 07:28:00 GMT"
#define	HTTP_IFMATCH		"\"1234567890\""
#define	HTTP_IFNMATCH		"\"1234567890\""
#define	HTTP_IFRANGE		"\"1234567890\""
#define HTTP_IFUMODSINCE	"Wed, 21 Oct 2015 07:28:00 GMT"
#define HTTP_MAX_FORWARDS	"10"
#define HTTP_RANGE		"bytes=500-999"
#define HTTP_REFERRER		"https://www.bsd.lv"
#define HTTP_USER_AGENT		"foobarbaz"

static int
parent(CURL *curl)
{
	struct curl_slist	*slist = NULL;
	int		   	 ret;

	slist = curl_slist_append(slist, "Accept: " HTTP_ACCEPT);
	slist = curl_slist_append(slist, "Accept-Charset: " HTTP_CHARSET);
	slist = curl_slist_append(slist, "Accept-Encoding: " HTTP_ENCODING);
	slist = curl_slist_append(slist, "Accept-Language: " HTTP_LANGUAGE);
	/* Don't test for Authorization. */
	slist = curl_slist_append(slist, "Depth: " HTTP_DEPTH);
	slist = curl_slist_append(slist, "From: " HTTP_FROM);
	slist = curl_slist_append(slist, "Host: " HTTP_HOST);
	slist = curl_slist_append(slist, "If: " HTTP_IF);
	slist = curl_slist_append(slist, "If-Match: " HTTP_IFMATCH);
	slist = curl_slist_append(slist, "If-Modified-Since: " HTTP_IFMODSINCE);
	slist = curl_slist_append(slist, "If-None-Match: " HTTP_IFNMATCH);
	slist = curl_slist_append(slist, "If-Range: " HTTP_IFRANGE);
	slist = curl_slist_append(slist, "If-Unmodified-Since: " HTTP_IFUMODSINCE);
	slist = curl_slist_append(slist, "Max-Forwards: " HTTP_MAX_FORWARDS);
	/* Don't test for Proxy Authorization. */
	slist = curl_slist_append(slist, "Range: " HTTP_RANGE);
	slist = curl_slist_append(slist, "Referer: " HTTP_REFERRER);
	slist = curl_slist_append(slist, "User-Agent: " HTTP_USER_AGENT);

	curl_easy_setopt(curl, CURLOPT_URL, "http://localhost:17123/");
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, slist);
	ret = curl_easy_perform(curl);
	curl_slist_free_all(slist);

	return ret == CURLE_OK;
}

static int
test(const struct kreq *r, enum krequ kr, const char *val)
{

	if (r->reqmap[kr] != NULL ||
	    strcmp(r->reqmap[kr]->val, val) == 0)
		return 1;
	warnx("missing header: %s", val);
	return 0;
}

static int
child(void)
{
	struct kreq	 r;
	const char 	*page = "index";

	if (khttp_parse(&r, NULL, 0, &page, 1, 0) != KCGI_OK) {
		warnx("khttp_parse");
		return 0;
	}

	test(&r, KREQU_ACCEPT, HTTP_ACCEPT);
	test(&r, KREQU_ACCEPT_CHARSET, HTTP_CHARSET);
	test(&r, KREQU_ACCEPT_ENCODING, HTTP_ENCODING);
	test(&r, KREQU_ACCEPT_LANGUAGE, HTTP_LANGUAGE);
	test(&r, KREQU_DEPTH, HTTP_DEPTH);
	test(&r, KREQU_FROM, HTTP_FROM);
	test(&r, KREQU_HOST, HTTP_HOST);
	test(&r, KREQU_IF, HTTP_IF);
	test(&r, KREQU_IF_MODIFIED_SINCE, HTTP_IFMODSINCE);
	test(&r, KREQU_IF_MATCH, HTTP_IFMATCH);
	test(&r, KREQU_IF_NONE_MATCH, HTTP_IFNMATCH);
	test(&r, KREQU_IF_RANGE, HTTP_IFRANGE);
	test(&r, KREQU_IF_UNMODIFIED_SINCE, HTTP_IFUMODSINCE);
	test(&r, KREQU_MAX_FORWARDS, HTTP_MAX_FORWARDS);
	test(&r, KREQU_RANGE, HTTP_RANGE);
	test(&r, KREQU_REFERER, HTTP_REFERRER);
	test(&r, KREQU_USER_AGENT, HTTP_USER_AGENT);

	khttp_head(&r, kresps[KRESP_STATUS], 
		"%s", khttps[KHTTP_201]);
	khttp_body(&r);
	khttp_free(&r);
	return 1;
}

int
main(int argc, char *argv[])
{

	return regress_cgi(parent, child) ? EXIT_SUCCESS : EXIT_FAILURE;
}
