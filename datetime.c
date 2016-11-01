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
#include "config.h"
#endif

#include <stdarg.h>
#include <stdint.h>
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

int64_t
kutil_date2epoch(int64_t day, int64_t mon, int64_t year)
{

	if (year < 1970 || mon < 0 || year < 0)
		return(0);
	return(mkdate(day, mon, year) -
	       mkdate(1, 1, 1970));
}

int64_t
kutil_datetime2epoch(int64_t day, int64_t mon, int64_t year,
	int64_t hour, int64_t minute, int64_t sec)
{

	if (year < 1970 || mon < 0 || year < 0 ||
	    hour < 0 || minute < 0 || sec < 0)
		return(0);
	return(kutil_date2epoch(day, mon, year) +
		hour * (60 * 60) + minute * 60 + sec);
}
