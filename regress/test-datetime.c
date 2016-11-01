/*	$Id$ */
/*
 * Copyright (c) 2016 Kristaps Dzonsons <kristaps@bsd.lv>
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

#include <err.h>

#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <curl/curl.h>

#include "../kcgi.h"
#include "regress.h"

int
main(int argc, char *argv[])
{
	size_t	 	 i;
	time_t	 	 v;
	struct tm	*tm;
	char	 	 inbuf[64], testbuf[64];

	for (i = 0; i < 100000; i++) {
		v = arc4random_uniform(300 * 365 * 24 * 60 * 60);
		tm = gmtime(&v);
		strftime(inbuf, sizeof(inbuf), "%a, %d %b %Y %T GMT", tm);
		kutil_epoch2str(v, testbuf, sizeof(testbuf));
		if (strcmp(inbuf, testbuf)) {
			warnx("System: %s", inbuf);
			warnx("KCGI: %s", testbuf);
			return(EXIT_FAILURE);
		}
	}

	return(EXIT_SUCCESS);
}
