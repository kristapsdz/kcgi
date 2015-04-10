/*	$Id$ */
/*
 * Copyright (c) 2015 Kristaps Dzonsons <kristaps@bsd.lv>
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

#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "kcgi.h"
#include "extern.h"

/*
 * A sequence of bytes parsed from the input stream.
 */
struct	pdigbuf {
	const char	*pos;
	size_t		 sz;
};

/*
 * Pointers to the components of an HTTP digest scheme in the input
 * stream.
 * These are empty strings (sz == 0) if undescribed.
 */
struct	pdigest {
	enum khttpalg	 alg;
	enum khttpqop	 qop;
	struct pdigbuf	 user;
	struct pdigbuf	 uri;
	struct pdigbuf	 realm;
	struct pdigbuf	 nonce;
	struct pdigbuf	 cnonce;
	struct pdigbuf	 response;
	struct pdigbuf	 count;
	struct pdigbuf	 opaque;
};

/*
 * Hash algorithm identifiers.
 */
static	const char *const httpalgs[KHTTPALG__MAX] = {
	"MD5", /* KHTTPALG_MD5 */
	"MD5-sess" /* KHTTPALG_MD5_SESS */
};

/*
 * Quality-of-protection identifiers.
 */
static	const char *const httpqops[KHTTPQOP__MAX] = {
	NULL, /* KHTTPQOP_NONE */
	"auth", /* KHTTPQOP_AUTH */
	"auth-int" /* KHTTPQOP_AUTH_INT */
};

/*
 * Parses the next token part.
 * Returns the start of the token and its size.
 * Positions "next" to be at the next token.
 */
static const char *
kauth_nexttok(const char **next, char delim, size_t *sz)
{
	const char	*cp;

	while (isspace((int)**next))
		(*next)++;

	for (*sz = 0, cp = *next; '\0' != **next; (*next)++, (*sz)++)
		if ('\0' != delim && delim == **next)
			break;
		else if (isspace((int)**next))
			break;

	if ('\0' != delim && delim == **next) 
		(*next)++;

	while (isspace((int)**next))
		(*next)++;

	return(cp);
}

/*
 * Parse a quoted-pair (or non-quoted pair) from the string "cp".
 * Puts the location of the next token back into "cp" and fills the
 * pointer in "val" (if non-NULL) and its size in "sz".
 */
static void
kauth_nextvalue(struct pdigbuf *val, const char **cp)
{
	int	 quot;

	if ('\0' == **cp)
		return;

	if ((quot = ('"' == **cp)))
		(*cp)++;

	if (NULL != val) {
		val->pos = *cp;
		val->sz = 0;
	}

	for ( ; '\0' != **cp; (*cp)++) {
		if (quot && '"' == **cp && '\\' != (*cp)[-1])
			break;
		else if ( ! quot && isspace((int)**cp))
			break;
		else if ( ! quot && ',' == **cp)
			break;
		if (NULL != val)
			val->sz++;
	}

	if (quot && '"' == **cp)
		(*cp)++;
	while (isspace((int)**cp))
		(*cp)++;
	if (',' == **cp)
		(*cp)++;
	while (isspace((int)**cp))
		(*cp)++;
}

/*
 * Parse a token.
 * We don't strictly follow RFC 2615's TOKEN specification, which says
 * that tokens can be followed by any separator.
 * We only use commas as separators.
 */
static void
kauth_nexttoken(size_t *val, const char **cp,
	const char *const *vals, size_t valsz)
{
	struct pdigbuf	 buf;

	kauth_nextvalue(&buf, cp);

	for (*val = 0; *val < valsz; (*val)++) {
		if (NULL == vals[*val])
			continue;
		if (buf.sz != strlen(vals[*val]))
			continue;
		if (0 == strncasecmp(buf.pos, vals[*val], buf.sz)) 
			return;
	}
}

static void
kauth_alg(enum khttpalg *val, const char **cp)
{
	size_t	 i;

	kauth_nexttoken(&i, cp, httpalgs, KHTTPALG__MAX);
	*val = i;
}

static void
kauth_qop(enum khttpqop *val, const char **cp)
{
	size_t	 i;

	kauth_nexttoken(&i, cp, httpqops, KHTTPQOP__MAX);
	*val = i;
}

static int
kauth_eq(const char *p, const char *start, size_t sz, size_t want)
{

	return(want == sz && 0 == strncasecmp(start, p, want));
}

static void
khttpbasic_input(int fd, const char *cp)
{
	enum kauth	 auth;
	int		 authorised;

	auth = KAUTH_BASIC;
	fullwrite(fd, &auth, sizeof(enum kauth));
	while (isspace((int)*cp))
		cp++;

	if ('\0' == cp) {
		authorised = 0;
		fullwrite(fd, &authorised, sizeof(int));
		return;
	}

	authorised = 1;
	fullwrite(fd, &authorised, sizeof(int));
	fullwriteword(fd, cp);
}

/*
 * Parse HTTP ``Digest'' authentication tokens from the nil-terminated
 * string, which can be NULL or malformed.
 */
static void
khttpdigest_input(int fd, const char *cp)
{
	enum kauth	 auth;
	const char	*start;
	int		 rc, authorised;
	size_t		 sz;
	struct pdigest	 d;

	auth = KAUTH_DIGEST;
	fullwrite(fd, &auth, sizeof(enum kauth));
	memset(&d, 0, sizeof(struct khttpdigest));

	for (rc = 1; 1 == rc && '\0' != *cp; ) {
		start = kauth_nexttok(&cp,  '=', &sz);
		if (kauth_eq("username", start, sz, 8))
			kauth_nextvalue(&d.user, &cp);
		else if (kauth_eq("realm", start, sz, 5))
			kauth_nextvalue(&d.realm, &cp);
		else if (kauth_eq("nonce", start, sz, 5))
			kauth_nextvalue(&d.nonce, &cp);
		else if (kauth_eq("cnonce", start, sz, 6))
			kauth_nextvalue(&d.cnonce, &cp);
		else if (kauth_eq("response", start, sz, 8))
			kauth_nextvalue(&d.response, &cp);
		else if (kauth_eq("uri", start, sz, 3))
			kauth_nextvalue(&d.uri, &cp);
		else if (kauth_eq("algorithm", start, sz, 9))
			kauth_alg(&d.alg, &cp);
		else if (kauth_eq("qop", start, sz, 3))
			kauth_qop(&d.qop, &cp);
		else if (kauth_eq("nc", start, sz, 2))
			kauth_nextvalue(&d.count, &cp);
		else if (kauth_eq("opaque", start, sz, 6))
			kauth_nextvalue(&d.opaque, &cp);
		else
			kauth_nextvalue(NULL, &cp);
	}

	/* Minimum requirements. */
	authorised = 
		0 != d.user.sz &&
		0 != d.realm.sz &&
		0 != d.nonce.sz &&
		0 != d.response.sz &&
		0 != d.uri.sz;

	/* Additional requirements: MD5-sess. */
	if (authorised && KHTTPALG_MD5_SESS == d.alg) 
		authorised = 0 != d.cnonce.sz;

	/* Additional requirements: qop. */
	if (authorised && 
		(KHTTPQOP_AUTH == d.qop ||
		 KHTTPQOP_AUTH_INT == d.qop))
		authorised = 
			0 != d.count.sz &&
			0 != d.cnonce.sz;

	fullwrite(fd, &authorised, sizeof(int));

	if ( ! authorised)
		return;

	fullwrite(fd, &d.alg, sizeof(enum khttpalg));
	fullwrite(fd, &d.qop, sizeof(enum khttpqop));
	fullwrite(fd, &d.user.sz, sizeof(size_t));
	fullwrite(fd, d.user.pos, d.user.sz);
	fullwrite(fd, &d.uri.sz, sizeof(size_t));
	fullwrite(fd, d.uri.pos, d.uri.sz);
	fullwrite(fd, &d.realm.sz, sizeof(size_t));
	fullwrite(fd, d.realm.pos, d.realm.sz);
	fullwrite(fd, &d.nonce.sz, sizeof(size_t));
	fullwrite(fd, d.nonce.pos, d.nonce.sz);
	fullwrite(fd, &d.cnonce.sz, sizeof(size_t));
	fullwrite(fd, d.cnonce.pos, d.cnonce.sz);
	fullwrite(fd, &d.response.sz, sizeof(size_t));
	fullwrite(fd, d.response.pos, d.response.sz);
	fullwrite(fd, &d.count.sz, sizeof(size_t));
	fullwrite(fd, d.count.pos, d.count.sz);
	fullwrite(fd, &d.opaque.sz, sizeof(size_t));
	fullwrite(fd, d.opaque.pos, d.opaque.sz);
}

enum kcgi_err
kworker_auth_parent(int fd, struct khttpauth *auth)
{
	enum kcgi_err	 ke;

	if (fullread(fd, &auth->type, sizeof(enum kauth), 0, &ke) < 0)
		return(ke);

	switch (auth->type) {
	case (KAUTH_DIGEST):
		if (fullread(fd, &auth->authorised, sizeof(int), 0, &ke) < 0)
			return(ke);
		if ( ! auth->authorised)
			break;
		if (fullread(fd, &auth->d.digest.alg, sizeof(enum khttpalg), 0, &ke) < 0)
			return(ke);
		if (fullread(fd, &auth->d.digest.qop, sizeof(enum khttpqop), 0, &ke) < 0)
			return(ke);
		if (KCGI_OK != (ke = fullreadword(fd, &auth->d.digest.user)))
			return(ke);
		if (KCGI_OK != (ke = fullreadword(fd, &auth->d.digest.uri)))
			return(ke);
		if (KCGI_OK != (ke = fullreadword(fd, &auth->d.digest.realm)))
			return(ke);
		if (KCGI_OK != (ke = fullreadword(fd, &auth->d.digest.nonce)))
			return(ke);
		if (KCGI_OK != (ke = fullreadword(fd, &auth->d.digest.cnonce)))
			return(ke);
		if (KCGI_OK != (ke = fullreadword(fd, &auth->d.digest.response)))
			return(ke);
		if (KCGI_OK != (ke = fullreadword(fd, &auth->d.digest.count)))
			return(ke);
		if (KCGI_OK != (ke = fullreadword(fd, &auth->d.digest.opaque)))
			return(ke);
		break;
	case (KAUTH_BASIC):
		if (fullread(fd, &auth->authorised, sizeof(int), 0, &ke) < 0)
			return(ke);
		if ( ! auth->authorised)
			break;
		if (KCGI_OK != (ke = fullreadword(fd, &auth->d.basic.response)))
			return(ke);
		break;
	default:
		break;
	}

	return(KCGI_OK);
}

void
kworker_auth_child(int fd, const char *cp)
{
	const char	*start;
	size_t	 	 sz;
	enum kauth	 auth;

	if (NULL == cp || '\0' == *cp) {
		auth = KAUTH_NONE;
		fullwrite(fd, &auth, sizeof(enum kauth));
		return;
	}

	start = kauth_nexttok(&cp, '\0', &sz);
	if (sz == 6 && 0 == strncasecmp(start, "digest", sz)) {
		khttpdigest_input(fd, cp);
		return;
	} else if (sz == 5 && 0 == strncasecmp(start, "basic", sz)) {
		khttpbasic_input(fd, cp);
		return;
	}

	auth = KAUTH_UNKNOWN;
	fullwrite(fd, &auth, sizeof(enum kauth));
}
