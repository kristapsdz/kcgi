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

	expect = "/foo/";
	url = khttp_urlpart("/foo", "html", NULL, NULL);
	if (url == NULL)
		errx(EXIT_FAILURE, "failed expect");
	if (strcmp(url, expect))
		errx(EXIT_FAILURE, "%s: failed expect: %s", expect, url);
	free(url);

	expect = "/foo/";
	url = khttp_urlpart("/foo", "html", "", NULL);
	if (url == NULL)
		errx(EXIT_FAILURE, "failed expect");
	if (strcmp(url, expect))
		errx(EXIT_FAILURE, "%s: failed expect: %s", expect, url);
	free(url);

	expect = "/";
	url = khttp_urlpart("", "html", "", NULL);
	if (url == NULL)
		errx(EXIT_FAILURE, "failed expect");
	if (strcmp(url, expect))
		errx(EXIT_FAILURE, "%s: failed expect: %s", expect, url);
	free(url);

	expect = "";
	url = khttp_urlpart(NULL, "html", "", NULL);
	if (url == NULL)
		errx(EXIT_FAILURE, "failed expect");
	if (strcmp(url, expect))
		errx(EXIT_FAILURE, "%s: failed expect: %s", expect, url);
	free(url);

	expect = "/foo/?bar=baz";
	url = khttp_urlpart("/foo", "html", "", "bar", "baz", NULL);
	if (url == NULL)
		errx(EXIT_FAILURE, "failed expect");
	if (strcmp(url, expect))
		errx(EXIT_FAILURE, "%s: failed expect: %s", expect, url);
	free(url);

	expect = "foo/?bar=baz";
	url = khttp_urlpart("foo", "html", "", "bar", "baz", NULL);
	if (url == NULL)
		errx(EXIT_FAILURE, "failed expect");
	if (strcmp(url, expect))
		errx(EXIT_FAILURE, "%s: failed expect: %s", expect, url);
	free(url);

	expect = "foo/?bar=&baz=foo";
	url = khttp_urlpart("foo", "html", "", "bar", NULL, "baz", "foo", NULL);
	if (url == NULL)
		errx(EXIT_FAILURE, "failed expect");
	if (strcmp(url, expect))
		errx(EXIT_FAILURE, "%s: failed expect: %s", expect, url);
	free(url);

	expect = "foo/?bar=&baz=foo";
	url = khttp_urlpart("foo", "html", "", "bar", "", "baz", "foo", NULL);
	if (url == NULL)
		errx(EXIT_FAILURE, "failed expect");
	if (strcmp(url, expect))
		errx(EXIT_FAILURE, "%s: failed expect: %s", expect, url);
	free(url);

	expect = "?bar=baz";
	url = khttp_urlpart(NULL, "html", "", "bar", "baz", NULL);
	if (url == NULL)
		errx(EXIT_FAILURE, "failed expect");
	if (strcmp(url, expect))
		errx(EXIT_FAILURE, "%s: failed expect: %s", expect, url);
	free(url);

	expect = "/?bar=baz";
	url = khttp_urlpart("", "", "", "bar", "baz", NULL);
	if (url == NULL)
		errx(EXIT_FAILURE, "failed expect");
	if (strcmp(url, expect))
		errx(EXIT_FAILURE, "%s: failed expect: %s", expect, url);
	free(url);

	expect = "/?bar=baz";
	url = khttp_urlpart("", "html", "", "bar", "baz", NULL);
	if (url == NULL)
		errx(EXIT_FAILURE, "failed expect");
	if (strcmp(url, expect))
		errx(EXIT_FAILURE, "%s: failed expect: %s", expect, url);
	free(url);

	expect = "/path/to/foo.html";
	url = khttp_urlpart("/path/to", "html", "foo", NULL);
	if (url == NULL)
		errx(EXIT_FAILURE, "failed expect");
	if (strcmp(url, expect))
		errx(EXIT_FAILURE, "%s: failed expect: %s", expect, url);
	free(url);

	expect = "relpath/to/foo";
	url = khttp_urlpart("relpath/to", NULL, "foo", NULL);
	if (url == NULL)
		errx(EXIT_FAILURE, "failed expect");
	if (strcmp(url, expect))
		errx(EXIT_FAILURE, "%s: failed expect: %s", expect, url);
	free(url);

	expect = "/path/to/foo";
	url = khttp_urlpart("/path/to", NULL, "foo", NULL);
	if (url == NULL)
		errx(EXIT_FAILURE, "failed expect");
	if (strcmp(url, expect))
		errx(EXIT_FAILURE, "%s: failed expect: %s", expect, url);
	free(url);

	expect = "/path/to/foo";
	url = khttp_urlpart("/path/to", "", "foo", NULL);
	if (url == NULL)
		errx(EXIT_FAILURE, "failed expect");
	if (strcmp(url, expect))
		errx(EXIT_FAILURE, "%s: failed expect: %s", expect, url);
	free(url);

	expect = "/foo.html";
	url = khttp_urlpart("", "html", "foo", NULL);
	if (url == NULL)
		errx(EXIT_FAILURE, "failed expect");
	if (strcmp(url, expect))
		errx(EXIT_FAILURE, "%s: failed expect: %s", expect, url);
	free(url);

	expect = "foo.html";
	url = khttp_urlpart(NULL, "html", "foo", NULL);
	if (url == NULL)
		errx(EXIT_FAILURE, "failed expect");
	if (strcmp(url, expect))
		errx(EXIT_FAILURE, "%s: failed expect: %s", expect, url);
	free(url);

	expect = "foo";
	url = khttp_urlpart(NULL, NULL, "foo", NULL);
	if (url == NULL)
		errx(EXIT_FAILURE, "failed expect");
	if (strcmp(url, expect))
		errx(EXIT_FAILURE, "%s: failed expect: %s", expect, url);
	free(url);

	expect = "/pat h/to/foo.html?foo=bar";
	url = khttp_urlpart("/pat h/to", "html", "foo", "foo", "bar", NULL);
	if (url == NULL)
		errx(EXIT_FAILURE, "failed expect");
	if (strcmp(url, expect))
		errx(EXIT_FAILURE, "%s: failed expect: %s", expect, url);
	free(url);

	expect = "/path/to/fo+o.html?foo=bar";
	url = khttp_urlpart("/path/to", "html", "fo o", "foo", "bar", NULL);
	if (url == NULL)
		errx(EXIT_FAILURE, "failed expect");
	if (strcmp(url, expect))
		errx(EXIT_FAILURE, "%s: failed expect: %s", expect, url);
	free(url);

	expect = "/path/to/foo.html?foo=bar";
	url = khttp_urlpart("/path/to", "html", "foo", "foo", "bar", NULL);
	if (url == NULL)
		errx(EXIT_FAILURE, "failed expect");
	if (strcmp(url, expect))
		errx(EXIT_FAILURE, "%s: failed expect: %s", expect, url);
	free(url);

	expect = "/path/to/foo.html?fo+o=bar";
	url = khttp_urlpart("/path/to", "html", "foo", "fo o", "bar", NULL);
	if (url == NULL)
		errx(EXIT_FAILURE, "failed expect");
	if (strcmp(url, expect))
		errx(EXIT_FAILURE, "%s: failed expect: %s", expect, url);
	free(url);

	expect = "/path/to/foo.html?fo+o=bar+o";
	url = khttp_urlpart("/path/to", "html", "foo", "fo o", "bar o", NULL);
	if (url == NULL)
		errx(EXIT_FAILURE, "failed expect");
	if (strcmp(url, expect))
		errx(EXIT_FAILURE, "%s: failed expect: %s", expect, url);
	free(url);

	return(EXIT_SUCCESS);
}
