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
#include "../config.h"

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
		/*
		 * arc4random_uniform is of course fine on BSD systems,
		 * but isn't implemented properly on all systems, e.g.,
		 * Alpine Linux, so we don't pull in <bsd/stdlib.h> and
		 * this might cause compile problems.
		 * We would do more values, but Linux sometimes has
		 * 32-bit time.
		 */
		v = arc4random_uniform(50 * 365 * 24 * 60 * 60);
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
