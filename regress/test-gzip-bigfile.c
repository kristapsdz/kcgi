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
#include "../config.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <curl/curl.h>

#include "../kcgi.h"
#include "regress.h"

#if HAVE_ZLIB

#define	BUFSZ	(1024 * 1024)
#ifdef __linux__
#define arc4random random
#endif

static size_t
doign(void *ptr, size_t sz, size_t nm, void *arg)
{

	return(sz * nm);
}

static int
parent(CURL *curl)
{
	char		*p;
	size_t		 i;

	if (NULL == (p = malloc(BUFSZ + 5))) {
		perror(NULL);
		return(0);
	}
	strncpy(p, "tag=", 5);
	for (i = 0; i < BUFSZ ; i++)
		p[i + 4] = 
			(0 == i % 2) ? (arc4random() % 26) + 65 :
			((0 == i % 2) ? (arc4random() % 26) + 97 :
			 (arc4random() % 9) + 48);
	p[BUFSZ + 4] = '\0';

	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, doign);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, p);
	curl_easy_setopt(curl, CURLOPT_URL, 
		"http://localhost:17123/");
	curl_easy_setopt(curl, CURLOPT_ENCODING, "gzip");
	if (CURLE_OK != curl_easy_perform(curl))
		return(0);
	free(p);
	return(1);
}

static int
child(void)
{
	struct kreq	 r;
	const char 	*page = "index";
	struct kvalid	 valid = { NULL, "tag" };

	if (KCGI_OK != khttp_parse(&r, &valid, 1, &page, 1, 0))
		return(0);

	if (NULL == r.fieldmap[0]) {
		khttp_free(&r);
		return(0);
	} else if (BUFSZ != r.fieldmap[0]->valsz) {
		khttp_free(&r);
		return(0);
	}

	khttp_head(&r, kresps[KRESP_STATUS], 
		"%s", khttps[KHTTP_200]);
	khttp_head(&r, kresps[KRESP_CONTENT_TYPE], 
		"%s", kmimetypes[KMIME_TEXT_HTML]);
	khttp_body(&r);
	khttp_write(&r, r.fieldmap[0]->val, r.fieldmap[0]->valsz);
	khttp_free(&r);
	return(1);
}

int
main(int argc, char *argv[])
{

	return(regress_cgi(parent, child) ? EXIT_SUCCESS : EXIT_FAILURE);
}

#else
int
main(int argc, char *argv[])
{

	return(EXIT_SUCCESS);
}
#endif
