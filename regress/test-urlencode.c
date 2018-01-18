/*	$Id$ */
/*
 * Copyright (c) 2018 Kristaps Dzonsons <kristaps@bsd.lv>
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

#include <err.h>

#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "../kcgi.h"

struct	test {
	const char	*input;
	const char	*output;
};

static	const struct test tests[] = {
	{ "", "" },
	{ "foobar", "foobar" },
	{ "foo bar", "foo+bar" },
	{ "foo~bar", "foo~bar" },
	{ "foo-bar", "foo-bar" },
	{ "foo_bar", "foo_bar" },
	{ "foo.bar", "foo.bar" },
	{ "foo.bar.", "foo.bar." },
	{ "foo.bar.-", "foo.bar.-" },
	{ "-_foo.bar.-", "-_foo.bar.-" },
	{ "-_foo+bar.-", "-_foo%2bbar.-" },
	{ "-_foo\tbar.-", "-_foo%09bar.-" },
	{ "\t-_foo\tbar.-", "%09-_foo%09bar.-" },
	{ "\t-_foo\tbar.-\t", "%09-_foo%09bar.-%09" },
	{ "\t-_foo%\tbar.-\t", "%09-_foo%25%09bar.-%09" },
	{ "-_foo%09}bar.-", "-_foo%2509%7dbar.-" },
	{ NULL, NULL }
};

int
main(int argc, char *argv[])
{
	const struct test *t;
	char	*url;

	for (t = tests; NULL != t->input; t++) {
		if (NULL == (url = kutil_urlencode(t->input)))
			return(EXIT_FAILURE);
		if (strcmp(url, t->output))
			errx(EXIT_FAILURE, "%s: fail (have %s, "
				"want %s)", t->input, url, t->output);
		free(url);
	}

	return(EXIT_SUCCESS);
}
