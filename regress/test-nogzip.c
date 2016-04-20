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

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <curl/curl.h>

#include "../kcgi.h"
#include "regress.h"

struct	buf {
	char	  buf[BUFSIZ];
	size_t	  sz;
};

static int
parentwrite(void *ptr, size_t sz, size_t nm, void *dat)
{
	struct buf	*buf = dat;

	if (buf->sz + (sz * nm) + 1 > BUFSIZ)
		return(-1);
	memcpy(buf->buf + buf->sz, ptr, sz * nm);
	buf->sz += sz * nm;
	buf->buf[buf->sz] = '\0';
	return(sz * nm);
}

static int
parent(CURL *curl)
{
	struct buf	 buf;
	char		*cp;

	memset(&buf, 0, sizeof(struct buf));
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, parentwrite);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buf);
	curl_easy_setopt(curl, CURLOPT_URL, "http://localhost:17123/");
	/*curl_easy_setopt(curl, CURLOPT_ENCODING, "");*/
	if (CURLE_OK != curl_easy_perform(curl))
		return(0);
	if (buf.sz != strlen(buf.buf)) {
		fprintf(stderr, "buffer size mismatch\n");
		return(0);
	} else if (NULL == (cp = strstr(buf.buf, "\r\n\r\n"))) {
		fprintf(stderr, "no crlf\n");
		return(0);
	}
	cp += 4;
	return(0 == strcmp(cp, "1234567890"));
}

static int
child(void)
{
	struct kreq	 r;
	const char 	*page = "index";

	if (KCGI_OK != khttp_parse(&r, NULL, 0, &page, 1, 0))
		return(0);

	khttp_head(&r, kresps[KRESP_STATUS],
		"%s", khttps[KHTTP_200]);
	khttp_head(&r, kresps[KRESP_CONTENT_TYPE],
		"%s", kmimetypes[KMIME_TEXT_HTML]);
	if (khttp_body(&r))
		return(0);
	khttp_puts(&r, "1234567890");
	khttp_free(&r);
	return(1);
}

int
main(int argc, char *argv[])
{

	return(regress_cgi(parent, child) ? EXIT_SUCCESS : EXIT_FAILURE);
}
