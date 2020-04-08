/*	$Id$ */
/*
 * Copyright (c) 2018, 2020 Kristaps Dzonsons <kristaps@bsd.lv>
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

#include "../kcgi.h"

int
main(int argc, char *argv[])
{
	char		*url;
	const char	*expect;

	/* Should fail: no filename. */

	url = kutil_urlpart(NULL, "/foo", "html", NULL, NULL);
	if (NULL != url)
		errx(EXIT_FAILURE, "failed expect");

	/* All of these should succeed... */

	expect = "/path/to/foo.html";
	url = kutil_urlpart(NULL, "/path/to", "html", "foo", NULL);
	if (NULL == url)
		errx(EXIT_FAILURE, "failed expect");
	if (strcmp(url, expect))
		errx(EXIT_FAILURE, "%s: failed expect: %s", expect, url);
	free(url);

	expect = "relpath/to/foo";
	url = kutil_urlpart(NULL, "relpath/to", NULL, "foo", NULL);
	if (NULL == url)
		errx(EXIT_FAILURE, "failed expect");
	if (strcmp(url, expect))
		errx(EXIT_FAILURE, "%s: failed expect: %s", expect, url);
	free(url);

	expect = "/path/to/foo";
	url = kutil_urlpart(NULL, "/path/to", NULL, "foo", NULL);
	if (NULL == url)
		errx(EXIT_FAILURE, "failed expect");
	if (strcmp(url, expect))
		errx(EXIT_FAILURE, "%s: failed expect: %s", expect, url);
	free(url);

	/* 
	 * This behaviour has been changed, but was never explicitly
	 * documented.
	 */
#if 0
	expect = "/path/to/foo.";
	url = kutil_urlpart(NULL, "/path/to", "", "foo", NULL);
	if (NULL == url)
		errx(EXIT_FAILURE, "failed expect");
	if (strcmp(url, expect))
		errx(EXIT_FAILURE, "%s: failed expect: %s", expect, url);
	free(url);
#endif

	expect = "/foo.html";
	url = kutil_urlpart(NULL, "", "html", "foo", NULL);
	if (NULL == url)
		errx(EXIT_FAILURE, "failed expect");
	if (strcmp(url, expect))
		errx(EXIT_FAILURE, "%s: failed expect: %s", expect, url);
	free(url);

	expect = "foo.html";
	url = kutil_urlpart(NULL, NULL, "html", "foo", NULL);
	if (NULL == url)
		errx(EXIT_FAILURE, "failed expect");
	if (strcmp(url, expect))
		errx(EXIT_FAILURE, "%s: failed expect: %s", expect, url);
	free(url);

	expect = "foo";
	url = kutil_urlpart(NULL, NULL, NULL, "foo", NULL);
	if (NULL == url)
		errx(EXIT_FAILURE, "failed expect");
	if (strcmp(url, expect))
		errx(EXIT_FAILURE, "%s: failed expect: %s", expect, url);
	free(url);

	/* Now encodings. */
	/* This should fail. */
	/* It's the only failure case here. */

	url = kutil_urlpart(NULL, "", "html", "foo", "fail", NULL);
	if (NULL != url)
		errx(EXIT_FAILURE, "failed expect");

	/* These should all succeed. */

	expect = "/pat h/to/foo.html?foo=bar";
	url = kutil_urlpart(NULL, "/pat h/to", "html", "foo", "foo", "bar", NULL);
	if (NULL == url)
		errx(EXIT_FAILURE, "failed expect");
	if (strcmp(url, expect))
		errx(EXIT_FAILURE, "%s: failed expect: %s", expect, url);
	free(url);

	expect = "/path/to/fo+o.html?foo=bar";
	url = kutil_urlpart(NULL, "/path/to", "html", "fo o", "foo", "bar", NULL);
	if (NULL == url)
		errx(EXIT_FAILURE, "failed expect");
	if (strcmp(url, expect))
		errx(EXIT_FAILURE, "%s: failed expect: %s", expect, url);
	free(url);

	expect = "/path/to/foo.html?foo=bar";
	url = kutil_urlpart(NULL, "/path/to", "html", "foo", "foo", "bar", NULL);
	if (NULL == url)
		errx(EXIT_FAILURE, "failed expect");
	if (strcmp(url, expect))
		errx(EXIT_FAILURE, "%s: failed expect: %s", expect, url);
	free(url);

	expect = "/path/to/foo.html?fo+o=bar";
	url = kutil_urlpart(NULL, "/path/to", "html", "foo", "fo o", "bar", NULL);
	if (NULL == url)
		errx(EXIT_FAILURE, "failed expect");
	if (strcmp(url, expect))
		errx(EXIT_FAILURE, "%s: failed expect: %s", expect, url);
	free(url);

	expect = "/path/to/foo.html?fo+o=bar+o";
	url = kutil_urlpart(NULL, "/path/to", "html", "foo", "fo o", "bar o", NULL);
	if (NULL == url)
		errx(EXIT_FAILURE, "failed expect");
	if (strcmp(url, expect))
		errx(EXIT_FAILURE, "%s: failed expect: %s", expect, url);
	free(url);

	return(EXIT_SUCCESS);
}
