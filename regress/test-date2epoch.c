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
	struct tm	 test, ntest;
	time_t		 vv, v;
	int64_t		 res;

	/* 
	 * This is a wanky test, but test-datetime2epoch uses a much
	 * fuller approach, so this is mostly instructional.
	 * To create a date we can check, create a random time, break it
	 * down, set its time to zero, re-create the time from that,
	 * then check against it.
	 * We use the native time_t for this, so we're going to be
	 * restricted to the operating system's time_t width.
	 * For Solaris, for example, this is limited.
	 */

	for (i = 0; i < 100000; i++) {
#if HAVE_ARC4RANDOM
		v = (time_t)arc4random();
#else
		v = (time_t)random();
#endif
		if (!KHTTP_EPOCH2TM(v, &test))
			errx(1, "KHTTP_EPOCH2TM");

		memset(&ntest, 0, sizeof(struct tm));
		ntest.tm_mday = test.tm_mday;
		ntest.tm_mon = test.tm_mon;
		ntest.tm_year = test.tm_year;
		if ((vv = timegm(&ntest)) == -1)
			errx(1, "timegm");

		if (!khttp_date2epoch(&res,
		    test.tm_mday,
		    test.tm_mon + 1,
		    test.tm_year + 1900))
			errx(1, "khttp_date2epoch: %lld", 
				(long long)vv);

		if (res != vv) 
			errx(1, "date cross-check: have "
				"%" PRId64 ", want %lld", res, 
				(long long)vv);
	}

	for (i = 0; i < 100000; i++) {
#if HAVE_ARC4RANDOM
		v = (time_t)arc4random() * -1;
#else
		v = (time_t)random() * -1;
#endif
		if (!KHTTP_EPOCH2TM(v, &test))
			errx(1, "KHTTP_EPOCH2TM");

		memset(&ntest, 0, sizeof(struct tm));
		ntest.tm_mday = test.tm_mday;
		ntest.tm_mon = test.tm_mon;
		ntest.tm_year = test.tm_year;
		if ((vv = timegm(&ntest)) == -1)
			errx(1, "timegm");

		if (!khttp_date2epoch(&res,
		    test.tm_mday,
		    test.tm_mon + 1,
		    test.tm_year + 1900))
			errx(1, "khttp_date2epoch: %lld",
				(long long)vv);

		if (res != vv) 
			errx(1, "date cross-check: have "
				"%" PRId64 ", want %lld", res, 
				(long long)vv);
	}

	/* Leap year is not ok. */

	if (khttp_date2epoch(&res,
	    29,
	    2,
	    2019))
		errx(1, "khttp_date2epoch should fail");

	/* Leap year is ok. */

	if (!khttp_date2epoch(&res,
	    29,
	    2,
	    2020))
		errx(1, "khttp_date2epoch");

	/* Bad day of month. */

	if (khttp_date2epoch(&res,
	    31,
	    4,
	    2020))
		errx(1, "khttp_date2epoch should fail");

	return 0;
}
