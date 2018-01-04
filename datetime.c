/*	$Id$ */
/*
 * Copyright (c) 2016, 2017 Kristaps Dzonsons <kristaps@bsd.lv>
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

/*
 * Number of days per month in a non-leap year and leap year,
 * respectively to the array index.
 */
static const uint64_t monthdays[2][12] = {
	{ 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 },
	{ 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 }
};

#define LEAPYR(_yr) 	( ! ((_yr) % 4) && \
			(((_yr) % 100) || ! ((_yr) % 400)))
#define YEARDAY(_yr) 	(LEAPYR(_yr) ? 366 : 365)

/*
 * This algorithm is simple and cribbed from NetBSD.
 * It sets the values of "tm" according to "tt", an epoch value.
 * It's truncated below at the zero epoch.
 */
static void
kutil_epoch2time(int64_t tt, struct tm *tm)
{
        time_t 	 	 time = (time_t)tt;
        uint64_t	 dayclock, dayno, year;

	memset(tm, 0, sizeof(struct tm));
	
	/* Bound below. */
	if (tt < 0) 
		return;
        
	/* 
	 * This is easy: time of day is the number of seconds within a
	 * 24-hour period and the day of week (the epoch was on
	 * Thursday, seven days reliably per week).
	 */

        dayclock = time % (24 * 60 * 60);
        dayno = time / (24 * 60 * 60);

	/*
	 * Compute the time of day quite easily.
	 * Same for the weekday.
	 */

        tm->tm_sec = dayclock % 60;
        tm->tm_min = (dayclock % 3600) / 60;
        tm->tm_hour = dayclock / 3600;
        tm->tm_wday = (dayno + 4) % 7;

	/*
	 * More complicated: slough away the number of years.
	 * XXX this takes time proportionate to the year, but is easier
	 * to audit (and understand).
	 */

	year = 1970;
        while (dayno >= YEARDAY(year)) {
                dayno -= YEARDAY(year);
                year++;
        }

        tm->tm_year = year - 1900;
        tm->tm_yday = dayno;
        tm->tm_mon = 0;

	/*
	 * Similar computation as the year: increment ahead the day of
	 * the month depending on our month value.
	 */

        while (dayno >= monthdays[LEAPYR(year)][tm->tm_mon]) {
                dayno -= monthdays[LEAPYR(year)][tm->tm_mon];
                tm->tm_mon++;
        }

        tm->tm_mday = dayno + 1;
        tm->tm_isdst = 0;
}

/*
 * Format an epoch time as RFC 822, bounding below at the zero epoch.
 * Returns "buf".
 */
char *
kutil_epoch2str(int64_t tt, char *buf, size_t sz)
{
	struct tm	 tm;

	kutil_epoch2time(tt, &tm);

	/* FIXME: replace with numeric values and table lookup */

	strftime(buf, sz, "%a, %d %b %Y %T GMT", &tm);
	return(buf);
}

char *
kutil_epoch2utcstr(int64_t tt, char *buf, size_t sz)
{
	struct tm	 tm;

	kutil_epoch2time(tt, &tm);
	snprintf(buf, sz, "%.4d-%.2d-%.2dT%.2d:%.2d:%.2dZ",
		tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, 
		tm.tm_hour, tm.tm_min, tm.tm_sec);
	return(buf);
}

/*
 * Breaks down the given epoch time as a series of values.
 * Each of these can be NULL.
 */
void
kutil_epoch2tmvals(int64_t tt, int *tm_sec, int *tm_min, 
	int *tm_hour, int *tm_mday, int *tm_mon, 
	int *tm_year, int *tm_wday, int *tm_yday)
{
	struct tm	 tm;

	kutil_epoch2time(tt, &tm);
	if (NULL != tm_sec)
		*tm_sec = tm.tm_sec;
	if (NULL != tm_min)
		*tm_min = tm.tm_min;
	if (NULL != tm_hour)
		*tm_hour = tm.tm_hour;
	if (NULL != tm_mday)
		*tm_mday = tm.tm_mday;
	if (NULL != tm_mon)
		*tm_mon = tm.tm_mon;
	if (NULL != tm_year)
		*tm_year = tm.tm_year;
	if (NULL != tm_wday)
		*tm_wday = tm.tm_wday;
	if (NULL != tm_yday)
		*tm_yday = tm.tm_yday;
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
