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

#include <inttypes.h>
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
	struct tm	 have, test;
	int		 c;

	/* 
	 * Test a lot of positive and negative numbers.
	 * We want the full range of time_t to work.
	 */

	for (i = 0; i < 100000; i++) {
#if HAVE_ARC4RANDOM
		v = (time_t)arc4random() * (time_t)arc4random();
#else
		v = (time_t)random() * (time_t)random();
#endif
		if ((tm = gmtime(&v)) == NULL) {
			warnx("gmtime: %" PRId64, (int64_t)v);
			continue;
		}
		have = *tm;
		c = khttp_epoch2tms(v,
			&test.tm_sec,
			&test.tm_min,
			&test.tm_hour,
			&test.tm_mday,
			&test.tm_mon,
			&test.tm_year,
			&test.tm_wday,
			&test.tm_yday);
		if (!c)
			errx(1, "khttp_epoch2tms");
		if (have.tm_sec != test.tm_sec ||
		    have.tm_min != test.tm_min ||
		    have.tm_hour != test.tm_hour ||
		    have.tm_mday != test.tm_mday ||
		    have.tm_mon != test.tm_mon ||
		    have.tm_year != test.tm_year ||
		    have.tm_wday != test.tm_wday ||
		    have.tm_yday != test.tm_yday)
			errx(1, "khttp_epoch2tms: have "
			    "{%d, %d, %d, %d, %d, %d, %d, %d}, want "
			    "{%d, %d, %d, %d, %d, %d, %d, %d}",
			    have.tm_sec,
			    have.tm_min,
			    have.tm_hour,
			    have.tm_mday,
			    have.tm_mon,
			    have.tm_year,
			    have.tm_wday,
			    have.tm_yday,
			    test.tm_sec,
			    test.tm_min,
			    test.tm_hour,
			    test.tm_mday,
			    test.tm_mon,
			    test.tm_year,
			    test.tm_wday,
			    test.tm_yday);
	}

	/* Handle NULL values. */

	c = khttp_epoch2tms(0, NULL, NULL, 
		NULL, NULL, NULL, NULL, NULL, NULL);
	if (!c)
		errx(1, "khttp_epoch2tms");

	return 0;
}
