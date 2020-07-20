/*	$Id$ */
/*
 * Copyright (c) 2014 Kristaps Dzonsons <kristaps@bsd.lv>
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

#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>

#include <ctype.h>
#if HAVE_ERR
# include <err.h>
#endif
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <curl/curl.h>

#include "../kcgiregress.h"
#include "regress.h"

static int
dochild(void *arg)
{
	cb_child	child = arg;

	return child();
}

static int
doparent(void *arg)
{
	CURL		*curl;
	cb_parent	 parent = arg;
	int		 rc;

	if ((curl = curl_easy_init()) == NULL)
		errx(1, "curl_easy_init");

	rc = parent(curl);
	curl_easy_cleanup(curl);
	curl_global_cleanup();
	return rc;
}

/*
 * Parse to the end of the current word from a log message and NUL
 * terminate it.
 * The word either ends on a white-space (or NUL) boundary or the given
 * endchar if not NUL.
 * Returns the next word position, which may be the end of the buffer,
 * or NULL if the buffer could not be terminated.
 */
static char *
log_line_parse_word(char *msg, char endchar)
{

	if (endchar != '\0')
		while (*msg != '\0' && *msg != endchar)
			msg++;
	else
		while (*msg != '\0' && !isspace((unsigned char)*msg))
			msg++;
	if (*msg == '\0')
		return NULL;
	*msg++ = '\0';
	while (*msg != '\0' && isspace((unsigned char)*msg))
		msg++;
	return msg;
}

/*
 * Parse all components of a log line, including newline.
 * The error message may be NULL.
 * Return zero on failure, non-zero on success.
 */
int
log_line_parse(char *msg, struct log_line *p)
{
	char	*tmp;

	p->addr = msg;
	if ((msg = log_line_parse_word(msg, '\0')) == NULL)
		return 0;
	p->ident = msg;
	if ((msg = log_line_parse_word(msg, '\0')) == NULL)
		return 0;
	if ('[' != *msg)
		return 0;
	msg++;
	p->date = msg;
	if ((msg = log_line_parse_word(msg, ']')) == NULL)
		return 0;
	p->level = msg;
	if ((msg = log_line_parse_word(msg, '\0')) == NULL)
		return 0;
	p->umsg = msg;
	if ((tmp = strrchr(msg, ':')) != NULL) {
		*tmp++ = '\0';
		p->errmsg = ++tmp;
	} else {
		log_line_parse_word(msg, '\0');
		p->errmsg = NULL;
	}

	return 1;
}

int
regress_cgi(cb_parent parent, cb_child child)
{

	return kcgi_regress_cgi(doparent, parent, dochild, child);
}

int
regress_fcgi(cb_parent parent, cb_child child)
{

	return kcgi_regress_fcgi(doparent, parent, dochild, child);
}
