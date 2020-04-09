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

#include "../kcgi.h"

int
main(int argc, char *argv[])
{
	char		*url;
	const char	*expect;

	expect = "https://a:8080/b?c=d&e=f";
	url = khttp_urlabs(KSCHEME_HTTPS, "a", 8080, "b", "c", "d", "e", "f", NULL);
	if (url == NULL)
		errx(EXIT_FAILURE, "failed expect");
	if (strcmp(url, expect))
		errx(EXIT_FAILURE, "%s: failed expect: %s", expect, url);
	free(url);

	expect = "https://a:8080/b";
	url = khttp_urlabs(KSCHEME_HTTPS, "a", 8080, "b", NULL);
	if (url == NULL)
		errx(EXIT_FAILURE, "failed expect");
	if (strcmp(url, expect))
		errx(EXIT_FAILURE, "%s: failed expect: %s", expect, url);
	free(url);

	expect = "https://a:8080/b";
	url = khttp_urlabs(KSCHEME_HTTPS, "a", 8080, "/b", NULL);
	if (url == NULL)
		errx(EXIT_FAILURE, "failed expect");
	if (strcmp(url, expect))
		errx(EXIT_FAILURE, "%s: failed expect: %s", expect, url);
	free(url);

	expect = "https://a:8080";
	url = khttp_urlabs(KSCHEME_HTTPS, "a", 8080, "", NULL);
	if (url == NULL)
		errx(EXIT_FAILURE, "failed expect");
	if (strcmp(url, expect))
		errx(EXIT_FAILURE, "%s: failed expect: %s", expect, url);
	free(url);

	expect = "https://a:8080";
	url = khttp_urlabs(KSCHEME_HTTPS, "a", 8080, NULL, NULL);
	if (url == NULL)
		errx(EXIT_FAILURE, "failed expect");
	if (strcmp(url, expect))
		errx(EXIT_FAILURE, "%s: failed expect: %s", expect, url);
	free(url);

	expect = "https://a";
	url = khttp_urlabs(KSCHEME_HTTPS, "a", 0, NULL, NULL);
	if (url == NULL)
		errx(EXIT_FAILURE, "failed expect");
	if (strcmp(url, expect))
		errx(EXIT_FAILURE, "%s: failed expect: %s", expect, url);
	free(url);

	expect = "https://a/b";
	url = khttp_urlabs(KSCHEME_HTTPS, "a", 0, "b", NULL);
	if (url == NULL)
		errx(EXIT_FAILURE, "failed expect");
	if (strcmp(url, expect))
		errx(EXIT_FAILURE, "%s: failed expect: %s", expect, url);
	free(url);

	expect = "mailto:b";
	url = khttp_urlabs(KSCHEME_MAILTO, "", 0, "b", NULL);
	if (url == NULL)
		errx(EXIT_FAILURE, "failed expect");
	if (strcmp(url, expect))
		errx(EXIT_FAILURE, "%s: failed expect: %s", expect, url);
	free(url);

	expect = "mailto:b";
	url = khttp_urlabs(KSCHEME_MAILTO, "", 8080, "b", NULL);
	if (url == NULL)
		errx(EXIT_FAILURE, "failed expect");
	if (strcmp(url, expect))
		errx(EXIT_FAILURE, "%s: failed expect: %s", expect, url);
	free(url);

	expect = "mailto:b?c=d";
	url = khttp_urlabs(KSCHEME_MAILTO, "", 8080, "b", "c", "d", NULL);
	if (url == NULL)
		errx(EXIT_FAILURE, "failed expect");
	if (strcmp(url, expect))
		errx(EXIT_FAILURE, "%s: failed expect: %s", expect, url);
	free(url);

	return EXIT_SUCCESS;
}
