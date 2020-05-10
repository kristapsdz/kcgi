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
	int64_t 	 v, vv, tm_sec, tm_min, tm_hour, tm_mday, 
			 tm_mon, tm_year, tm_wday, tm_yday;

	/* 
	 * FIXME: test truly across all int64_t.
	 * Right now the implementation is too slow for that.
	 */

	for (i = 0; i < 100000; i++) {
#if HAVE_ARC4RANDOM
		v = arc4random();
#else
		v = random();
#endif
		khttp_epoch2datetime(v,
			&tm_sec,
			&tm_min,
			&tm_hour,
			&tm_mday,
			&tm_mon,
			&tm_year,
			&tm_wday,
			&tm_yday);
		if (!khttp_datetime2epoch(&vv, tm_mday,
		    tm_mon, tm_year, tm_hour, tm_min, tm_sec))
			errx(1, "khttp_datetime2epoch");
		if (v != vv)
			errx(1, "khttp_datetime2epoch: mismatch");
	}

	for (i = 0; i < 100000; i++) {
#if HAVE_ARC4RANDOM
		v = (int64_t)arc4random() * -1.0;
#else
		v = (int64_t)random() * -1.0;
#endif
		khttp_epoch2datetime(v,
			&tm_sec,
			&tm_min,
			&tm_hour,
			&tm_mday,
			&tm_mon,
			&tm_year,
			&tm_wday,
			&tm_yday);
		if (!khttp_datetime2epoch(&vv, tm_mday,
		    tm_mon, tm_year, tm_hour, tm_min, tm_sec))
			errx(1, "khttp_datetime2epoch");
		if (v != vv)
			errx(1, "khttp_datetime2epoch: mismatch");
	}

	return 0;
}
