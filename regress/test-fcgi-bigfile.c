/*	$Id$ */
/*
 * Copyright (c) 2015 Kristaps Dzonsons <kristaps@bsd.lv>
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

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <curl/curl.h>

#include "../kcgi.h"
#include "regress.h"

static size_t
doign(void *ptr, size_t sz, size_t nm, void *arg)
{

	*(size_t *)arg += sz * nm;
	return(sz * nm);
}

static int
parent(CURL *curl)
{
	char		*p;
	size_t		 i;

	p = malloc(1024 * 1024 + 5);
	strncpy(p, "tag=", 5);
	for (i = 0; i < 1024 * 1024; i++)
		p[i + 4] = (i % 10) + 65;
	p[1024 * 1024 + 4] = '\0';

	i = 0;
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, doign);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &i);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, p);
	curl_easy_setopt(curl, CURLOPT_URL, 
		"http://localhost:17123/");
	if (CURLE_OK != curl_easy_perform(curl))
		return(0);
	free(p);
	return(i >= 1024 * 1024);
}

static int
child(void)
{
	struct kreq	 r;
	const char 	*page = "index";
	struct kvalid	 valid = { NULL, "tag" };
	size_t		 i;
	struct kfcgi	*fcgi;
	enum kcgi_err	 er;

	if (KCGI_OK != khttp_fcgi_init(&fcgi, &valid, 1, &page, 1, 0))
		return(0);

	while (KCGI_OK == (er = khttp_fcgi_parse(fcgi, &r))) {
		if (NULL == r.fieldmap[0]) {
			khttp_free(&r);
			khttp_fcgi_free(fcgi);
			return(0);
		} else if (1024 * 1024 != r.fieldmap[0]->valsz) {
			khttp_free(&r);
			khttp_fcgi_free(fcgi);
			return(0);
		}

		for (i = 0; i < 1024 * 1024; i++)
			if (r.fieldmap[0]->val[i] != (i % 10) + 65) {
				khttp_free(&r);
				khttp_fcgi_free(fcgi);
				return(0);
			}

		khttp_head(&r, kresps[KRESP_STATUS], 
			"%s", khttps[KHTTP_200]);
		khttp_head(&r, kresps[KRESP_CONTENT_TYPE], 
			"%s", kmimetypes[KMIME_TEXT_HTML]);
		khttp_body(&r);
		khttp_write(&r, r.fieldmap[0]->val, r.fieldmap[0]->valsz);
		khttp_free(&r);
	}

	khttp_fcgi_free(fcgi);
	return(KCGI_HUP == er ? 1 : 0);
}

int
main(int argc, char *argv[])
{

	return(regress_fcgi(parent, child) ? EXIT_SUCCESS : EXIT_FAILURE);
}
