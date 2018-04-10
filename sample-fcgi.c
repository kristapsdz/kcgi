/*	$Id$ */
/*
 * Copyright (c) 2015, 2018 Kristaps Dzonsons <kristaps@bsd.lv>
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
#include <getopt.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "kcgi.h"

int
main(void)
{
	struct kreq	 req;
	struct kfcgi	*fcgi;
	enum kcgi_err	 er;

	if (KCGI_OK != khttp_fcgi_init(&fcgi, NULL, 0, NULL, 0, 0))
		return 0;

	for (;;) {
		er = khttp_fcgi_parse(fcgi, &req);
		if (KCGI_EXIT == er) {
			fprintf(stderr, "khttp_fcgi_parse: terminate\n");
			break;
		} else if (KCGI_OK != er) {
			fprintf(stderr, "khttp_fcgi_parse: error: %d\n", er);
			break;
		}

		er = khttp_head(&req, kresps[KRESP_STATUS], 
			"%s", khttps[KHTTP_200]);
		if (KCGI_HUP == er) {
			fprintf(stderr, "khttp_head: interrupt\n");
			khttp_free(&req);
			continue;
		} else if (KCGI_OK != er) {
			fprintf(stderr, "khttp_head: error: %d\n", er);
			khttp_free(&req);
			break;
		}

		er = khttp_head(&req, kresps[KRESP_CONTENT_TYPE], 
			"%s", kmimetypes[req.mime]);
		if (KCGI_HUP == er) {
			fprintf(stderr, "khttp_head: interrupt\n");
			khttp_free(&req);
			continue;
		} else if (KCGI_OK != er) {
			fprintf(stderr, "khttp_head: error: %d\n", er);
			khttp_free(&req);
			break;
		}

		er = khttp_body(&req);
		if (KCGI_HUP == er) {
			fprintf(stderr, "khttp_body: interrupt\n");
			khttp_free(&req);
			continue;
		} else if (KCGI_OK != er) {
			fprintf(stderr, "khttp_body: error: %d\n", er);
			khttp_free(&req);
			break;
		}

		er = khttp_puts(&req, "Hello, world!\n");
		if (KCGI_HUP == er) {
			fprintf(stderr, "khttp_puts: interrupt\n");
			khttp_free(&req);
			continue;
		} else if (KCGI_OK != er) {
			fprintf(stderr, "khttp_puts: error: %d\n", er);
			khttp_free(&req);
			break;
		}

		khttp_free(&req);
	}

	khttp_fcgi_free(fcgi);
	return 0;
}
