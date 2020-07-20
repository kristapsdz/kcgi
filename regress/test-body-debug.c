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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <curl/curl.h>

#include "../kcgi.h"
#include "regress.h"

static char log[32] = "/tmp/test-body-debug.XXXXXXXXXX";

static int
parent(CURL *curl)
{
	const char	*data = "foo=ba\t\rr";

	curl_easy_setopt(curl, CURLOPT_URL, 
		"http://localhost:17123/");
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data);
	return curl_easy_perform(curl) == CURLE_OK;
}

static int
child(void)
{
	struct kreq	 r;
	const char 	*page = "index";
	enum kcgi_err	 er;

	if (!kutil_openlog(log))
		return 0;

	er = khttp_parsex
		(&r, ksuffixmap, kmimetypes, KMIME__MAX, 
		 NULL, 0, &page, 1, KMIME_TEXT_HTML,
		 0, NULL, NULL, KREQ_DEBUG_READ_BODY, NULL);
	if (er != KCGI_OK)
		return 0;

	khttp_head(&r, kresps[KRESP_STATUS], 
		"%s", khttps[KHTTP_200]);
	khttp_head(&r, kresps[KRESP_CONTENT_TYPE], 
		"%s", kmimetypes[KMIME_TEXT_HTML]);
	khttp_body(&r);
	khttp_free(&r);
	return 1;
}

int
main(int argc, char *argv[])
{
	int		 fd = -1, rc = 1;
	FILE		*f = NULL;
	char		*line = NULL;
	size_t		 linesize = 0, lineno = 0;
	ssize_t		 linelen;
	struct log_line	 log_line;

	if ((fd = mkstemp(log)) == -1)
		err(1, "%s", log);

	if (!regress_cgi(parent, child))
		goto out;

	if ((f = fdopen(fd, "r")) == NULL) {
		warn("%s", log);
		goto out;
	}

	for (;;) {
		if ((linelen = getline(&line, &linesize, f)) == -1)
			break;
		if (!log_line_parse(line, &log_line))
			goto out;
		if (strcmp(log_line.level, "INFO"))
			continue;
		switch (lineno) {
		case 0:
			if (strcmp(log_line.errmsg, "foo=ba\\t\\rr\n"))
				goto out;
			break;
		case 1:
			break;
		default:
			goto out;
		}
		lineno++;
	}

	if (ferror(f))
		goto out;

	rc = 0;
out:
	if (f != NULL)
		fclose(f);
	else if (fd != -1)
		close(fd);

	free(line);
	unlink(log);
	return rc;
}
