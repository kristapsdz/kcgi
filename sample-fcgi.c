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
#include <getopt.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "kcgi.h"

int
main(int argc, char *argv[])
{
	struct kreq	 req;
	struct kfcgi	*fcgi;
	enum kcgi_err	 er;
	const char	*pname;
	int		 rc, c, debug;

	if ((pname = strrchr(argv[0], '/')) == NULL)
		pname = argv[0];
	else
		++pname;

	debug = 0;
	while (-1 != (c = getopt(argc, argv, "d:")))
		switch (c) {
		case ('d'):
			debug = atoi(optarg);
			break;
		default:
			return(EXIT_FAILURE);
		}

	if (KCGI_OK != khttp_fcgi_init(&fcgi, NULL, 0))
		return(EXIT_FAILURE);

	for (rc = 0;;) {
		er = khttp_fcgi_parse(fcgi, &req, NULL, 0, 0);
		if (KCGI_HUP == er) {
			rc = 1;
			fprintf(stderr, "Hangup\n");
			khttp_free(&req);
			break;
		} else if (KCGI_OK != er) {
			khttp_free(&req);
			break;
		}
		khttp_head(&req, kresps[KRESP_STATUS], 
			"%s", khttps[KHTTP_200]);
		khttp_body(&req);
		khttp_puts(&req, "Hello, world!\n");
		khttp_free(&req);
		if (debug > 0 && 0 == --debug)
			break;
	}

	fprintf(stderr, "Exiting\n");
	khttp_fcgi_free(fcgi);
	return(rc ? EXIT_SUCCESS : EXIT_FAILURE);
}
