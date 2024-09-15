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
#include <getopt.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "kcgi.h"

int
main(void)
{
	struct kreq	 r;
	struct kfcgi	*fcgi;
	enum kcgi_err	 er;

	/* Set up our FastCGI context. */

	if (khttp_fcgi_init(&fcgi, NULL, 0, NULL, 0, 0) != KCGI_OK)
		return 0;

	for (;;) {
		/* Set up the current HTTP context. */

		er = khttp_fcgi_parse(fcgi, &r);
		if (er == KCGI_EXIT)
			fprintf(stderr, "khttp_fcgi_parse: terminate\n");
		else if (er != KCGI_OK)
			fprintf(stderr, "khttp_fcgi_parse: error: %d\n", er);
		if (er != KCGI_OK)
			break;

		/* 
		 * Accept only GET and OPTIONS.  Start with CORS handling, which
		 * we'll allow to any origin.  XXX: this ignores errors in
		 * handling, which is probably not what you want.
		 */

		if (r.reqmap[KREQU_ORIGIN] != NULL)
			khttp_head(&r,
				kresps[KRESP_ACCESS_CONTROL_ALLOW_ORIGIN],
				"%s", r.reqmap[KREQU_ORIGIN]->val);
		if (r.method == KMETHOD_OPTIONS) {
			khttp_head(&r, kresps[KRESP_ALLOW], "OPTIONS, GET");
			khttp_head(&r, kresps[KRESP_STATUS], "%s",
				khttps[KHTTP_204]);
			khttp_body(&r);
		} else if (r.method == KMETHOD_GET) {
			/* Inherit the MIME type. */
			khttp_head(&r, kresps[KRESP_CONTENT_TYPE], "%s",
				kmimetypes[r.mime]);
			khttp_body(&r);
			khttp_puts(&r, "Hello, world!\n");
		}

		khttp_free(&r);
	}

	khttp_fcgi_free(fcgi);
	return 0;
}
