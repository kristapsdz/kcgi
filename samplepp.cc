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
#include <sys/types.h> /* size_t, ssize_t */
#include <stdarg.h> /* va_list */
#include <stddef.h> /* NULL */
#include <stdint.h> /* int64_t */

#include "kcgi.h"

#include <iostream>

int 
main(int argc, char *argv[]) 
{
	enum kcgi_err	  er;
	struct kreq	  r;
	const char *const pages[1] = { "index" };

	/* Set up our main HTTP context. */

	er = khttp_parse(&r, NULL, 0, pages, 1, 0);
	if (KCGI_OK != er)
		return 0;

	khttp_head(&r, kresps[KRESP_STATUS], 
	  "%s", khttps[KHTTP_200]);
	khttp_head(&r, kresps[KRESP_CONTENT_TYPE], 
	  "%s", kmimetypes[r.mime]);
	khttp_body(&r);
	khttp_puts(&r, "Hello, world!\n");

	std::cerr << "Said hello!";

	khttp_free(&r);
	return 0;
}
