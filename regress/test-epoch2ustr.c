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
	int64_t		 vv;
	struct tm	*tm;
	char	 	 buf[64], testbuf[64];

	/* 
	 * Test a lot of positive and negative numbers.
	 * We want the full range of time_t to work.
	 */

	for (i = 0; i < 100000; i++) {
		v = (int32_t)arc4random();
		tm = gmtime(&v);
		strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", tm);
		khttp_epoch2ustr(v, testbuf, sizeof(testbuf));
		if (strcmp(buf, testbuf))
			errx(1, "khttp_epoch2ustr: "
				"have %s, want %s", testbuf, buf);
	}

	/* Truncate test. */

	v = 10000; /* whatever */
	tm = gmtime(&v);
	strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", tm);
	khttp_epoch2ustr(v, testbuf, 10);
	if (strlen(testbuf) != 9)
		errx(1, "khttp_epoch2ustr: bad string length");
	if (strncmp(buf, testbuf, 9))
		errx(1, "khttp_epoch2ustr: have %s, "
			"want %s", testbuf, buf);

	/* Now test for time_t > int32_t. */

	vv = 100000000000;
	strlcpy(buf, "5138-11-16T09:46:40Z", sizeof(buf));
	khttp_epoch2ustr(vv, testbuf, sizeof(testbuf));
	if (strcmp(buf, testbuf))
		errx(1, "khttp_epoch2ustr: "
			"have %s, want %s", testbuf, buf);

	/* Similarly, but for >4 digit years. */

	vv = INT64_MAX;
	strlcpy(buf, "292277026596-12-04T15:30:07Z", sizeof(buf));
	khttp_epoch2ustr(vv, testbuf, sizeof(testbuf));
	if (strcmp(buf, testbuf))
		errx(1, "khttp_epoch2ustr: "
			"have %s, want %s", testbuf, buf);

	/* And time_t < int32_t (also tests for negative year). */

	vv = INT64_MIN;
	strlcpy(buf, "-292277022657-01-27T08:29:52Z", sizeof(buf));
	khttp_epoch2ustr(vv, testbuf, sizeof(testbuf));
	if (strcmp(buf, testbuf))
		errx(1, "khttp_epoch2ustr: "
			"have %s, want %s", testbuf, buf);

	/* Truncate to zero test. */

	tm = gmtime(&v);
	strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", tm);
	khttp_epoch2ustr(v, testbuf, 1);
	if (strlen(testbuf) != 0)
		errx(1, "khttp_epoch2ustr: bad string length");

	/* No NULL or zero-length values. */

	if (khttp_epoch2ustr(v, NULL, 1) != NULL)
		errx(1, "khttp_epoch2ustr: should return NULL");
	if (khttp_epoch2ustr(v, testbuf, 0) != NULL)
		errx(1, "khttp_epoch2ustr: should return NULL");

	return 0;
}
