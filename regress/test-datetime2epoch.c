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
	struct tm	 test;
	int64_t		 v, res;

	/* 
	 * Test a lot of positive and negative numbers.
	 * We want the full range of time_t to work.
	 */

	for (i = 0; i < 100000; i++) {
		v = arc4random();
		if (!KHTTP_EPOCH2TM(v, &test))
			errx(1, "KHTTP_EPOCH2TM");
		if (!khttp_datetime2epoch(&res,
		    test.tm_mday,
		    test.tm_mon + 1,
		    test.tm_year + 1900,
		    test.tm_hour,
		    test.tm_min,
		    test.tm_sec))
			errx(1, "khttp_datetime2epoch: %" PRId64, v);
		if (res != v) 
			errx(1, "date cross-check: have "
				"%" PRId64 ", want %" PRId64, res, v);
	}

	for (i = 0; i < 100000; i++) {
		v = (int64_t)arc4random() * -1;
		if (!KHTTP_EPOCH2TM(v, &test))
			errx(1, "KHTTP_EPOCH2TM");
		if (!khttp_datetime2epoch(&res,
		    test.tm_mday,
		    test.tm_mon + 1,
		    test.tm_year + 1900,
		    test.tm_hour,
		    test.tm_min,
		    test.tm_sec))
			errx(1, "khttp_datetime2epoch: %" PRId64, v);
		if (res != v) 
			errx(1, "date cross-check: have "
				"%" PRId64 ", want %" PRId64, res, v);
	}

	/* Test specifically for -1 and 0. */

	v = -1;
	if (!KHTTP_EPOCH2TM(v, &test))
		errx(1, "KHTTP_EPOCH2TM");
	if (!khttp_datetime2epoch(&res,
	    test.tm_mday,
	    test.tm_mon + 1,
	    test.tm_year + 1900,
	    test.tm_hour,
	    test.tm_min,
	    test.tm_sec))
		errx(1, "khttp_datetime2epoch: %" PRId64, v);
	if (res != v) 
		errx(1, "date cross-check: have "
			"%" PRId64 ", want %" PRId64, res, v);

	v = 0;
	if (!KHTTP_EPOCH2TM(v, &test))
		errx(1, "KHTTP_EPOCH2TM");
	if (!khttp_datetime2epoch(&res,
	    test.tm_mday,
	    test.tm_mon + 1,
	    test.tm_year + 1900,
	    test.tm_hour,
	    test.tm_min,
	    test.tm_sec))
		errx(1, "khttp_datetime2epoch: %" PRId64, v);
	if (res != v) 
		errx(1, "date cross-check: have "
			"%" PRId64 ", want %" PRId64, res, v);

	/* Leap year is not ok. */

	if (khttp_datetime2epoch(&res,
	    29,
	    2,
	    2019,
	    0,
	    0,
	    0))
		errx(1, "khttp_datetime2epoch should fail");

	/* Leap year is ok. */

	if (!khttp_datetime2epoch(&res,
	    29,
	    2,
	    2020,
	    0,
	    0,
	    0))
		errx(1, "khttp_datetime2epoch");

	/* Bad day of month. */

	if (khttp_datetime2epoch(&res,
	    31,
	    4,
	    2020,
	    0,
	    0,
	    0))
		errx(1, "khttp_datetime2epoch should fail");

	/* Zeroes everywhere fails (month, day). */

	if (khttp_datetime2epoch(&res,
	    0,
	    0,
	    0,
	    0,
	    0,
	    0))
		errx(1, "khttp_datetime2epoch should fail");

	/* Zero hour. */

	if (!khttp_datetime2epoch(&res,
	    1,
	    1,
	    0,
	    0,
	    0,
	    0))
		errx(1, "khttp_datetime2epoch");

	return 0;
}
