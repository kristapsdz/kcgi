/*	$Id$ */
/*
 * Copyright (c) 2016, 2017, 2020 Kristaps Dzonsons <kristaps@bsd.lv>
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
#include "config.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "kcgi.h"
#include "extern.h"

char *
khttp_epoch2str(int64_t tt, char *buf, size_t sz)
{
	struct tm	*tm;
	char		 rbuf[64];
	time_t		 t = (time_t)tt;

	if (buf == NULL || sz == 0)
		return NULL;

	/* 
	 * It's not clear when gmtime() would actually return NULL.
	 * strftime shouldn't return zero because 64 bytes will always
	 * be enough for this function.
	 */

	if ((tm = gmtime(&t)) == NULL) {
		XWARNX("gmtime");
		return NULL;
	} 

	if (strftime(rbuf, sizeof(rbuf),
	    "%a, %d %b %Y %T GMT", tm) == 0) {
		XWARNX("strftime");
		return NULL;
	}

	strlcpy(buf, rbuf, sz);
	return buf;
}

/*
 * Deprecated interface simply zeroes the time structure if it's
 * negative.  The original implementation did not do this (it set the
 * "struct tm" to zero), but the documentation still said otherwise.
 * This uses the original documented behaviour.
 */
char *
kutil_epoch2str(int64_t tt, char *buf, size_t sz)
{

	return khttp_epoch2str(tt < 0 ? 0 : tt, buf, sz);
}

char *
khttp_epoch2ustr(int64_t tt, char *buf, size_t sz)
{
	struct tm	*tm;
	time_t		 t = (time_t)tt;
	char		 rbuf[64];

	if (buf == NULL || sz == 0)
		return NULL;

	/* 
	 * It's not clear when gmtime() would actually return NULL.
	 * snprintf() shouldn't fail.
	 */

	if ((tm = gmtime(&t)) == NULL) {
		XWARNX("gmtime");
		return NULL;
	}

	if (snprintf(rbuf, sizeof(rbuf), 
	    "%.4d-%.2d-%.2dT%.2d:%.2d:%.2dZ",
	    tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday, 
	    tm->tm_hour, tm->tm_min, tm->tm_sec) == -1) {
		XWARNX("snprintf");
		return NULL;
	}

	strlcpy(buf, rbuf, sz);
	return buf;
}

/*
 * Deprecated.
 * See kutil_epoch2str() for behaviour notes.
 */
char *
kutil_epoch2utcstr(int64_t tt, char *buf, size_t sz)
{

	return khttp_epoch2ustr(tt < 0 ? 0 : tt, buf, sz);
}

int
khttp_epoch2tms(int64_t tt, int *tm_sec, int *tm_min, 
	int *tm_hour, int *tm_mday, int *tm_mon, 
	int *tm_year, int *tm_wday, int *tm_yday)
{
	struct tm	*tm;
	time_t		 t = tt;

	if ((tm = gmtime(&t)) == NULL) {
		XWARNX("gmtime");
		return 0;
	}

	if (tm_sec != NULL)
		*tm_sec = tm->tm_sec;
	if (tm_min != NULL)
		*tm_min = tm->tm_min;
	if (tm_hour != NULL)
		*tm_hour = tm->tm_hour;
	if (tm_mday != NULL)
		*tm_mday = tm->tm_mday;
	if (tm_mon != NULL)
		*tm_mon = tm->tm_mon;
	if (tm_year != NULL)
		*tm_year = tm->tm_year;
	if (tm_wday != NULL)
		*tm_wday = tm->tm_wday;
	if (tm_yday != NULL)
		*tm_yday = tm->tm_yday;

	return 1;
}

/*
 * Deprecated.
 * This has a bad corner case where gmtime() returns NULL and we just
 * assume the epoch---this wasn't present in the earlier version of this
 * because it was using a hand-rolled gmtime() that didn't fail.  This
 * is suitably unlikely that it's ok, as it's deprecated.
 */
void
kutil_epoch2tmvals(int64_t tt, int *tm_sec, int *tm_min, 
	int *tm_hour, int *tm_mday, int *tm_mon, 
	int *tm_year, int *tm_wday, int *tm_yday)
{
	struct tm	*tm;
	struct tm	 ttm;
	time_t		 t = tt < 0 ? 0 : tt;

	/* This shouldn't return NULL, but be certain. */

	if ((tm = gmtime(&t)) == NULL) {
		XWARNX("gmtime");
		memset(&ttm, 0, sizeof(struct tm));
		tm = &ttm;
	}

	if (tm_sec != NULL)
		*tm_sec = tm->tm_sec;
	if (tm_min != NULL)
		*tm_min = tm->tm_min;
	if (tm_hour != NULL)
		*tm_hour = tm->tm_hour;
	if (tm_mday != NULL)
		*tm_mday = tm->tm_mday;
	if (tm_mon != NULL)
		*tm_mon = tm->tm_mon;
	if (tm_year != NULL)
		*tm_year = tm->tm_year;
	if (tm_wday != NULL)
		*tm_wday = tm->tm_wday;
	if (tm_yday != NULL)
		*tm_yday = tm->tm_yday;
}

/*
 * http://pubs.opengroup.org/onlinepubs/9699919799/basedefs/V1_chap04.html#tag_04_15
 */
static int64_t
mkdate(int64_t d, int64_t m, int64_t y)
{
	int64_t	 v;

	m = (m + 9) % 12;
	y = y - m / 10;
	v = 365 * y + y / 4 - y / 100 + 
		y / 400 + (m * 306 + 5) / 10 + (d - 1);
	return(v * 86400);
}

int
kutil_date_check(int64_t mday, int64_t mon, int64_t year)
{
	int	 leap = 0;

	/* 
	 * Basic boundary checks.
	 * The 1582 check is for the simple Gregorian calendar rules of
	 * leap date calculation.
	 * This can be lifted to account for even more times, but it
	 * seems unlikely that pre-1582 date input will be required.
	 */

	if (year < 1582 || year > 9999 || 
	    mon < 1 || mon > 12 || 
	    mday < 1 || mday > 31)
		return(0);

	/* Check for 30 days. */

	if ((4 == mon || 6 == mon || 9 == mon || 11 == mon) &&
	    mday > 30)
		return(0);

	/* Check for leap year. */

	if (year % 400 == 0 || (year % 100 != 0 && year % 4 == 0))
		leap = 1;

	if (leap && 2 == mon && mday > 29)
		return(0);
	if ( ! leap && 2 == mon && mday > 28)
		return(0);

	return(1);
}

int64_t
kutil_date2epoch(int64_t day, int64_t mon, int64_t year)
{

	return(mkdate(day, mon, year) -
	       mkdate(1, 1, 1970));
}

int
kutil_datetime_check(int64_t mday, int64_t mon, int64_t year,
	int64_t hour, int64_t minute, int64_t sec)
{

	if ( ! kutil_date_check(mday, mon, year))
		return(0);
	return(hour >= 0 && hour < 24 &&
		minute >= 0 && minute < 60 &&
		sec >= 0 && sec < 60);
}

int64_t
kutil_datetime2epoch(int64_t day, int64_t mon, int64_t year,
	int64_t hour, int64_t minute, int64_t sec)
{

	return(kutil_date2epoch(day, mon, year) +
		hour * (60 * 60) + minute * 60 + sec);
}
