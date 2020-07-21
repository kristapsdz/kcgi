/*	$Id$ */
/*
 * Copyright (c) 2014, 2020 Kristaps Dzonsons <kristaps@bsd.lv>
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
#ifndef REGRESS_H
#define REGRESS_H

struct	log_line {
	const char	*addr; /* remote address */
	const char	*ident; /* opaque identifier */
	const char	*date; /* date (HTTP format) */
	const char	*level; /* log level (INFO, etc.) */
	const char	*umsg; /* user message */
	const char	*errmsg; /* error message (in umsg) */
};

typedef int	(*cb_child)(void);
typedef int	(*cb_parent)(CURL *);

int		  log_line_parse(char *, struct log_line *);

int		  regress_cgi(cb_parent, cb_child);
int		  regress_fcgi(cb_parent, cb_child);

#endif
