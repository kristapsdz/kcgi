/*	$Id$ */
/*
 * Copyright (c) 2012, 2014 Kristaps Dzonsons <kristaps@bsd.lv>
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
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "kcgi.h"

/* * 10 MB upload limit. */
#define	UPLOAD_LIMIT	 10485760

/* Maximum length of a URI (plus nil). */
#define	URISZ 		 2049

/*
 * For handling HTTP multipart forms.
 * This consists of data for a single multipart form entry.
 */
struct	hmime {
	char	 *disp; /* content disposition */
	char	 *name; /* name of form entry */
	size_t	  namesz; /* size of "name" string */
	char	 *file; /* whether a file was specified */
	char	 *ctype; /* content type */
	char	 *xcode; /* encoding type */
	char	 *bound; /* form entry boundary */
};

/*
 * A tag describes an HTML element.
 */
struct	tag {
	int		 flags; /* bit-field of flags */
#define	TAG_BLOCK	 1 /* block element (vs. inline) */
#define	TAG_ACLOSE	 2 /* auto-closing (e.g., INPUT) */
	const char	*name; /* name of element */
};

static	const char * const entities[ENTITY__MAX] = {
	"#xE9", /* ENTITY_ACUTE */
	"gt", /* ENTITY_GT */
	"#x2190", /* ENTITY_LARR */
	"lt", /* ENTITY_LT */
	"#x2014", /* ENTITY_MDASH */
	"#x2013", /* ENTITY_NDASH */
	"#x2192", /* ENTITY_RARR */
};

static	const struct tag tags[ELEM__MAX] = {
	{ 1, "html" }, /* ELEM_HTML */
	{ 1, "head" }, /* ELEM_HEAD */
	{ 1, "body" }, /* ELEM_BODY */
	{ 0, "title" }, /* ELEM_TITLE */
	{ 3, "meta" }, /* ELEM_META */
	{ 3, "link" }, /* ELEM_LINK */
	{ 1, "form" }, /* ELEM_FORM */
	{ 2, "input" }, /* ELEM_INPUT */
	{ 1, "textarea" }, /* ELEM_TEXTAREA */
	{ 1, "p" }, /* ELEM_P */
	{ 1, "blockquote" }, /* ELEM_BLOCKQUOTE */
	{ 3, "br" }, /* ELEM_BR */
	{ 1, "fieldset" }, /* ELEM_FIELDSET */
	{ 0, "legend" }, /* ELEM_LEGEND */
	{ 0, "label" }, /* ELEM_LABEL */
	{ 0, "a" }, /* ELEM_A */
	{ 0, "code" }, /* ELEM_CODE */
	{ 1, "div" }, /* ELEM_DIV */
	{ 0, "span" }, /* ELEM_SPAN */
	{ 1, "ul" }, /* ELEM_UL */
	{ 1, "li" }, /* ELEM_LI */
	{ 0, "strong" }, /* ELEM_STRONG */
	{ 1, "table" }, /* ELEM_TABLE */
	{ 1, "caption" }, /* ELEM_CAPTION */
	{ 1, "tr" }, /* ELEM_TR */
	{ 1, "td" }, /* ELEM_TD */
	{ 1, "th" }, /* ELEM_TH */
	{ 1, "select" }, /* ELEM_SELECT */
	{ 0, "option" }, /* ELEM_OPTION */
	{ 2, "img" }, /* ELEM_IMG */
	{ 0, "i" }, /* ELEM_I */
	{ 0, "q" }, /* ELEM_Q */
	{ 1, "dl" }, /* ELEM_DL */
	{ 1, "dt" }, /* ELEM_DT */
	{ 1, "dd" }, /* ELEM_DD */
	{ 3, "col" }, /* ELEM_COL */
	{ 0, "var" }, /* ELEM_VAR */
};

const char * const mimes[MIME__MAX] = {
	"html", /* MIME_HTML */
	"csv", /* MIME_CSV */
	"png", /* MIME_PNG */
};

const char * const mimetypes[MIME__MAX] = {
	"text/html; charset=utf-8", /* MIME_HTML */
	"text/csv", /* MIME_CSV */
	"image/png", /* MIME_PNG */
};

static	const char * const attrs[ATTR__MAX] = {
	"http-equiv", /* ATTR_HTTP_EQUIV */
	"content", /* ATTR_CONTENT */
	"rel", /* ATTR_REL */
	"href", /* ATTR_HREF */
	"type", /* ATTR_TYPE */
	"action", /* ATTR_ACTION */
	"method", /* ATTR_METHOD */
	"name", /* ATTR_NAME */
	"value", /* ATTR_VALUE */
	"onclick", /* ATTR_ONCLICK */
	"id", /* ATTR_ID */
	"for", /* ATTR_FOR */
	"class", /* ATTR_CLASS */
	"colspan", /* ATTR_COLSPAN */
	"disabled", /* ATTR_DISABLED */
	"selected", /* ATTR_SELECTED */
	"src", /* ATTR_SRC */
	"clear", /* ATTR_CLEAR */
	"checked", /* ATTR_CHECKED */
	"style", /* ATTR_STYLE */
	"target", /* ATTR_TARGET */
	"step", /* ATTR_STEP */
	"min", /* ATTR_MIN */
	"max", /* ATTR_MAX */
	"width", /* ATTR_WIDTH */
	"span", /* ATTR_SPAN */
	"rowspan", /* ATTR_ROWSPAN */
};

/* 
 * Name of executing CGI script.
 */
const char	*pname = NULL;

/* 
 * Safe strdup(): don't return on memory failure.
 */
char *
xstrdup(const char *cp)
{
	char	*p;

	if (NULL != (p = strdup(cp)))
		return(p);

	perror(NULL);
	exit(EXIT_FAILURE);
}

/*
 * Safe realloc(): don't return on memory failure.
 */
void *
xxrealloc(void *pp, size_t sz)
{
	char	*p;

	if (NULL != (p = realloc(pp, sz)))
		return(p);

	perror(NULL);
	exit(EXIT_FAILURE);
}

/*
 * Safe realloc() with overflow-checking.
 */
void *
xrealloc(void *pp, size_t nm, size_t sz)
{
	char	*p;

	if (nm && sz && SIZE_T_MAX / nm < sz) {
		errno = ENOMEM;
		perror(NULL);
		exit(EXIT_FAILURE);
	}

	if (NULL != (p = realloc(pp, nm * sz)))
		return(p);

	perror(NULL);
	exit(EXIT_FAILURE);
}

/*
 * Safe calloc(): don't return on exhaustion.
 */
void *
xcalloc(size_t nm, size_t sz)
{
	char	*p;

	if (NULL != (p = calloc(nm, sz)))
		return(p);

	perror(NULL);
	exit(EXIT_FAILURE);
}

/*
 * Safe malloc(): don't return on exhaustion.
 */
void *
xmalloc(size_t sz)
{
	char	*p;

	if (NULL != (p = malloc(sz)))
		return(p);

	perror(NULL);
	exit(EXIT_FAILURE);
}

#ifndef __OpenBSD__
#define INVALID 	1
#define TOOSMALL 	2
#define TOOLARGE 	3
static long long
strtonum(const char *numstr, long long minval, 
		long long maxval, const char **errstrp)
{
	long long ll = 0;
	char *ep;
	int error = 0;
	struct errval {
		const char *errstr;
		int err;
	} ev[4] = {
		{ NULL,		0 },
		{ "invalid",	EINVAL },
		{ "too small",	ERANGE },
		{ "too large",	ERANGE },
	};

	ev[0].err = errno;
	errno = 0;
	if (minval > maxval)
		error = INVALID;
	else {
		ll = strtoll(numstr, &ep, 10);
		if (numstr == ep || *ep != '\0')
			error = INVALID;
		else if ((ll == LLONG_MIN && errno == ERANGE) || 
			ll < minval)
			error = TOOSMALL;
		else if ((ll == LLONG_MAX && errno == ERANGE) || 
			ll > maxval)
			error = TOOLARGE;
	}
	if (errstrp != NULL)
		*errstrp = ev[error].errstr;
	errno = ev[error].err;
	if (error)
		ll = 0;

	return (ll);
}
#endif /*!__OpenBSD__*/

void
elem(struct req *req, enum elem elem)
{

	attr(req, elem, ATTR__MAX);
}

#if 0
void
input(struct req *req, enum key key)
{
	const char	*cp, *type;
	char		 buf[URISZ];

	if (NULL != keys[key].label && 
			KFIELD_SUBMIT != keys[key].field) {
		attr(req, ELEM_LABEL,
			ATTR_FOR, keys[key].name,
			ATTR__MAX);
		text(keys[key].label);
		closure(req, 1);
		text(":");
	}

	cp = buf;
	buf[0] = '\0';
	type = "text";

	switch (keys[key].field) {
	case (KFIELD_SUBMIT):
		cp = keys[key].label;
		type = "submit";
		break;
	case (KFIELD_PASSWORD):
		type = "password";
		break;
	case (KFIELD_EMAIL):
		type = "email";
		break;
	default:
		break;
	}

	if (NULL != req->fieldmap[key])
		switch (keys[key].field) {
		case (KFIELD_DOUBLE):
			snprintf(buf, URISZ, "%.2f",
				req->fieldmap[key]->parsed.d);
			break;
		case (KFIELD_INT):
			snprintf(buf, URISZ, "%lld",
				req->fieldmap[key]->parsed.i);
			break;
		case (KFIELD_EMAIL):
			/* FALLTHROUGH */
		case (KFIELD_STRING):
			cp = req->fieldmap[key]->parsed.s;
			break;
		default:
			break;
		}
	else if (NULL != keys[key].def) 
		cp = keys[key].def;

	attr(req, ELEM_INPUT,
		ATTR_TYPE, type,
		ATTR_NAME, keys[key].name,
		ATTR_ID, keys[key].name,
		ATTR_VALUE, cp,
		ATTR__MAX);
}
#endif

void
attr(struct req *req, enum elem elem, ...)
{
	va_list		 ap;
	enum attr	 at;
	const char	*cp;

	printf("<%s", tags[elem].name);
	va_start(ap, elem);
	while (ATTR__MAX != (at = va_arg(ap, enum attr))) {
		cp = va_arg(ap, char *);
		assert(NULL != cp);
		printf(" %s=\"%s\"", attrs[at], cp);

	}
	va_end(ap);

	if (TAG_ACLOSE & tags[elem].flags)
		putchar('/');
	putchar('>');

	if (TAG_BLOCK & tags[elem].flags)
		putchar('\n');
	if ( ! (TAG_ACLOSE & tags[elem].flags))
		req->elems[req->elemsz++] = elem;

	assert(req->elemsz < 128);
}

void
closure(struct req *req, size_t sz)
{
	size_t		 i;

	for (i = 0; i < sz; i++) {
		assert(req->elemsz > 0);
		req->elemsz--;
		printf("</%s>", tags[req->elems[req->elemsz]].name);
		if (TAG_BLOCK & tags[req->elems[req->elemsz]].flags)
			putchar('\n');
	}
}

void
decl(void)
{

	puts("<!DOCTYPE html>");
}

/*
 * In-place HTTP-decode a string.  The standard explanation is that this
 * turns "%4e+foo" into "n foo" in the regular way.  This is done
 * in-place over the allocated string.
 */
static int
urldecode(char *p)
{
	char             hex[3];
	unsigned int	 c;

	hex[2] = '\0';

	for ( ; '\0' != *p; p++) {
		if ('%' == *p) {
			if ('\0' == (hex[0] = *(p + 1)))
				return(0);
			if ('\0' == (hex[1] = *(p + 2)))
				return(0);
			if (1 != sscanf(hex, "%x", &c))
				return(0);
			if ('\0' == c)
				return(0);

			*p = (char)c;
			memmove(p + 1, p + 3, strlen(p + 3) + 1);
		} else
			*p = /* LINTED */
				'+' == *p ? ' ' : *p;
	}

	*p = '\0';
	return(1);
}

/*
 * Parse out key-value pairs from an HTTP request variable.
 * This can be either a cookie or a POST/GET string.
 */
static void
parse_pairs(struct kpair **kv, size_t *kvsz, char *p)
{
	char            *key, *val;
	size_t           sz;

	while (p && '\0' != *p) {
		while (' ' == *p)
			p++;

		key = p;
		val = NULL;

		if (NULL != (p = strchr(p, '='))) {
			*p++ = '\0';
			val = p;

			sz = strcspn(p, ";&");
			/* LINTED */
			p += sz;

			if ('\0' != *p)
				*p++ = '\0';
		} else {
			p = key;
			sz = strcspn(p, ";&");
			/* LINTED */
			p += sz;

			if ('\0' != *p)
				p++;
			continue;
		}

		if ('\0' == *key || '\0' == *val)
			continue;

		if ( ! urldecode(key))
			break;
		if ( ! urldecode(val))
			break;

		*kv = xrealloc(*kv, *kvsz + 1, sizeof(struct kpair));
		(*kv)[*kvsz].key = xstrdup(key);
		(*kv)[*kvsz].val = xstrdup(val);
		(*kvsz)++;
	}
}

static void
parse_urlenc(struct kpair **kv, size_t *kvsz)
{
	char		*p;
	ssize_t		 ssz;
	size_t		 sz;

	p = xmalloc(4096 + 1);
	sz = 0;
	do {
		ssz = read(STDIN_FILENO, p + sz, 4096);
		if (ssz <= 0)
			continue;
		sz += (size_t)ssz;
		assert(sz < SIZE_T_MAX - 4097);
		p = xxrealloc(p, sz + 4096 + 1);
	} while (ssz > 0);

	p[sz] = '\0';
	parse_pairs(kv, kvsz, p);
	free(p);
}

static char *
getformln(void)
{
	char		*line;
	size_t		 sz;

	if (NULL == (line = fgetln(stdin, &sz)))
		return(NULL);
	else if (sz < 2)
		return(NULL);
	else if ('\n' != line[sz - 1])
		return(NULL);
	else if ('\r' != line[sz - 2])
		return(NULL);

	sz -= 2;
	line[sz] = '\0';
	return(line);
}

static void
hmime_reset(char **dst, const char *src)
{

	if (*dst)
		free(*dst);

	*dst = xstrdup(src);
}

static int
hmime_parse(struct hmime *mime)
{
	char		*key, *val, *line;

	memset(mime, 0, sizeof(struct hmime));

	while (NULL != (line = getformln())) {
		if ('\0' == *line)
			return(1);

		key = line;
		if (NULL == (val = strchr(key, ':')))
			return(0);
		*val++ = '\0';

		while (isspace((unsigned char)*val))
			val++;

		line = NULL;
		if (NULL != val && NULL != (line = strchr(val, ';')))
			*line++ = '\0';

		if (0 == strcasecmp(key, "content-transfer-encoding"))
			hmime_reset(&mime->xcode, val);
		else if (0 == strcasecmp(key, "content-disposition"))
			hmime_reset(&mime->disp, val);
		else if (0 == strcasecmp(key, "content-type"))
			hmime_reset(&mime->ctype, val);
		else
			return(0);

		while (NULL != (key = line)) {
			while (isspace((unsigned char)*key))
				key++;
			if ('\0' == *key)
				break;
			if (NULL == (val = strchr(key, '=')))
				return(0);
			*val++ = '\0';

			if ('"' == *val) {
				val++;
				line = strchr(val, '"');
				if (NULL == line)
					return(0);
				*line++ = '\0';
				if (';' == *line)
					line++;
			} else if (NULL != (line = strchr(val, ';')))
				*line++ = '\0';

			if (0 == strcasecmp(key, "filename"))
				hmime_reset(&mime->file, val);
			else if (0 == strcasecmp(key, "name"))
				hmime_reset(&mime->name, val);
			else if (0 == strcasecmp(key, "boundary"))
				hmime_reset(&mime->bound, val);
			else
				return(0);
		}
	} 

	return(0);
}

static char *
getbinln(size_t *sz)
{
	char		*line;

	if (NULL == (line = fgetln(stdin, sz)))
		return(NULL);
	else if ('\n' != line[*sz - 1])
		return(NULL);

	return(line);
}

static int
boundary(const char *line, size_t sz, const char *b, size_t bsz)
{
	size_t		 extra;

	if (sz < bsz + 4)
		return(-1);
	if ('\n' != line[sz - 1] && '\r' != line[sz - 2])
		return(-1);

	sz -= 2;
	extra = sz - (bsz + 2);

	if ('-' != line[0] || '-' != line[1])
		return(-1);

	line += 2;
	if (memcmp(line, b, bsz))
		return(-1);

	line += bsz;
	if (2 == extra && 0 == memcmp(line, "--", 2))
		return(0);
	else if (0 == extra)
		return(1);

	return(-1);
}

static int
form_parse(struct kpair **kv, size_t *kvsz, 
		const char *name, const char *b, size_t bsz)
{
	char		*cp, *line;
	size_t		 sz, valsz;
	int		 rc;

	valsz = 0;
	cp = NULL;
	rc = -1;

	while (NULL != (line = getbinln(&sz))) {
		if ((rc = boundary(line, sz, b, bsz)) >= 0)
			break;
		assert(sz > 0);
		if (valsz + sz > UPLOAD_LIMIT)
			break;
		cp = xxrealloc(cp, valsz + sz);
		memcpy(cp + valsz, line, sz);
		valsz += sz;
	}

	cp = xxrealloc(cp, valsz + 1);
	cp[valsz] = '\0';

	if (NULL == line) 
		rc = -1;

	if (rc >= 0 && valsz > 3)
		cp[valsz - 2] = '\0';
	else {
		free(cp);
		return(-1);
	}

	*kv = xrealloc(*kv, *kvsz + 1, sizeof(struct kpair));
	(*kv)[*kvsz].key = xstrdup(name);
	(*kv)[*kvsz].val = cp;
	(*kvsz)++;

	return(rc);
}

static void
hmime_free(struct hmime *mime)
{

	free(mime->disp);
	free(mime->name);
	free(mime->file);
	free(mime->ctype);
	free(mime->xcode);
	free(mime->bound);
	/* Help us segfault if re-used. */
	memset(mime, 0, sizeof(struct hmime));
}

static int
multiform_parse(struct kpair **kv, size_t *kvsz, const char *b)
{
	struct hmime	 mime;
	char		*line;
	int		 rc;
	size_t		 bsz;

	if (NULL == b) 
		return(-1);
	else if (NULL == (line = getformln())) 
		return(-1);
	else if ('-' != line[0] || '-' != line[1])
		return(-1);
	else if (strcmp(line + 2, b))
		return(-1);

	rc = 1;
	bsz = strlen(b);

	while (rc > 0) {
		rc = -1;
		if ( ! hmime_parse(&mime))
			break;

		if (NULL == mime.disp)
			break;
		else if (NULL == mime.name)
			break;
		else if (strcmp(mime.disp, "form-data")) 
			break;

		if (NULL == mime.ctype || NULL != mime.file) {
			rc = form_parse(kv, kvsz, mime.name, b, bsz);
			hmime_free(&mime);
			continue;
		} else if (strcmp(mime.ctype, "multipart/mixed"))
			break;

		rc = multiform_parse(kv, kvsz, mime.bound);
		hmime_free(&mime);
	}

	hmime_free(&mime);
	return(rc);
}

static void
parse_multi(struct kpair **kv, size_t *kvsz, char *line)
{
	size_t		 sz;

	while (' ' == *line)
		line++;
	if (';' != *line++)
		return;
	while (' ' == *line)
		line++;

	sz = strlen("boundary=");
	if (strncmp(line, "boundary=", sz)) 
		return;
	line += sz;

	while (' ' == *line) 
		line++;

	multiform_parse(kv, kvsz, line);
}

/*
 * Parse a request from an HTTP request.
 * This consists of paths, suffixes, methods, and most importantly,
 * pasred query string, cookie, and form data.
 */
void
http_parse(struct req *req, 
	const struct kvalid *keys, size_t keymax,
	const char *const *pages, size_t pagemax,
	size_t defpage)
{
	char		*cp, *ep, *sub;
	enum mime	 m;
	size_t		 p;
	size_t		 i, j;

	if (NULL == (pname = getenv("SCRIPT_NAME")))
		pname = "";

	memset(req, 0, sizeof(struct req));

	req->cookiemap = xcalloc(keymax, sizeof(struct kpair *));
	req->fieldmap = xcalloc(keymax, sizeof(struct kpair *));

	sub = NULL;
	p = defpage;
	m = MIME_HTML;

	/*
	 * First, parse the first path element (the page we want to
	 * access), subsequent path information, and the file suffix.
	 * We convert suffix and path element into the respective enum's
	 * inline.
	 */

	req->method = METHOD_GET;
	if (NULL != (cp = getenv("REQUEST_METHOD")) &&
			0 == strcasecmp(cp, "post"))
		req->method = METHOD_POST;

	if (NULL != (cp = getenv("PATH_INFO")) && '/' == *cp)
		cp++;

	if (NULL != cp && '\0' != *cp) {
		ep = cp + strlen(cp) - 1;
		while (ep > cp && '/' != *ep && '.' != *ep)
			ep--;

		if ('.' == *ep)
			for (*ep++ = '\0', m = 0; m < MIME__MAX; m++)
				if (0 == strcasecmp(mimes[m], ep))
					break;

		if (NULL != (sub = strchr(cp, '/')))
			*sub++ = '\0';

		for (p = 0; p < pagemax; p++)
			if (0 == strcasecmp(pages[p], cp))
				break;
	}

	req->mime = m;
	req->page = p;
	if (NULL != sub)
		req->path = xstrdup(sub);

	/*
	 * If a CONTENT_TYPE has been specified (i.e., POST or GET has
	 * been set -- we don't care which), then switch on that type
	 * for parsing out key value pairs.
	 */

	if (NULL != (cp = getenv("CONTENT_TYPE"))) {
		if (0 == strcmp(cp, "application/x-www-form-urlencoded"))
			parse_urlenc(&req->fields, &req->fieldsz);
		else if (0 == strncmp(cp, "multipart/form-data", 19)) 
			parse_multi(&req->fields, &req->fieldsz, cp + 19);
	}

	/*
	 * Even POST requests are allowed to have QUERY_STRING elements,
	 * so parse those out now.
	 * Note: both QUERY_STRING and CONTENT_TYPE fields share the
	 * same field space.
	 */

	if (NULL != (cp = getenv("QUERY_STRING")))
		parse_pairs(&req->fields, &req->fieldsz, cp);

	/*
	 * Cookies come last.
	 * These use the same syntax as QUERY_STRING elements, but don't
	 * share the same namespace (just as a means to differentiate
	 * the same names).
	 */

	if (NULL != (cp = getenv("HTTP_COOKIE")))
		parse_pairs(&req->cookies, &req->cookiesz, cp);

	/*
	 * Run through all fields and sort them into named buckets.
	 * This will let us do constant-time lookups within the
	 * application itself.  Nice.
	 */

	for (i = 0; i < keymax; i++) {
		for (j = 0; j < req->fieldsz; j++) {
			if (strcmp(req->fields[j].key, keys[i].name))
				continue;
			if (NULL != keys[i].valid)
				if ( ! (*keys[i].valid)(&req->fields[j]))
					continue;
			req->fields[j].next = req->fieldmap[i];
			req->fieldmap[i] = &req->fields[j];
		}
		for (j = 0; j < req->cookiesz; j++) {
			if (strcmp(req->cookies[j].key, keys[i].name))
				continue;
			if (NULL != keys[i].valid)
				if ( ! keys[i].valid(&req->cookies[j]))
					continue;
			req->cookies[j].next = req->cookiemap[i];
			req->cookiemap[i] = &req->cookies[j];
		}
	}
}

static void
kpair_free(struct kpair *p, size_t sz)
{
	size_t		 i;

	for (i = 0; i < sz; i++) {
		free(p[i].key);
		free(p[i].val);
	}
	free(p);
}

void
http_free(struct req *req)
{

	kpair_free(req->cookies, req->cookiesz);
	kpair_free(req->fields, req->fieldsz);
	free(req->path);
	free(req->cookies);
	free(req->fields);
}

void
sym(enum entity entity)
{

	assert(entity < ENTITY__MAX);
	printf("&%s;", entities[entity]);
}

void
text(const char *cp)
{

	for ( ; NULL != cp && '\0' != *cp; cp++)
		switch (*cp) {
		case ('>'):
			sym(ENTITY_GT);
			break;
		case ('<'):
			sym(ENTITY_LT);
			break;
		default:
			putchar((int)*cp);
			break;
		}
}

/*
 * Trim leading and trailing whitespace from a word.
 */
static char *
trim(char *val)
{
	char		*cp;

	if ('\0' == *val)
		return(val);

	cp = val + strlen(val) - 1;
	while (cp > val && isspace((unsigned char)*cp))
		*cp-- = '\0';

	cp = val;
	while (isspace((unsigned char)*cp))
		cp++;

	return(cp);
}

/*
 * Simple email address validation.
 * Note that this lowercases the mail address.
 */
static char *
valid_email(char *p)
{
	char		*domain, *cp, *start;
	size_t		 i, sz;

	cp = start = trim(p);

	if ((sz = strlen(cp)) < 5 || sz > 254)
		return(NULL);
	if (NULL == (domain = strchr(cp, '@')))
		return(NULL);
	if ((sz = domain - cp) < 1 || sz > 64)
		return(NULL);

	for (i = 0; i < sz; i++) {
		if (isalnum((unsigned char)cp[i]))
			continue;
		if (NULL == strchr("!#$%&'*+-/=?^_`{|}~.", cp[i]))
			return(NULL);
	}

	assert('@' == cp[i]);
	cp = &cp[++i];
	if ((sz = strlen(cp)) < 4 || sz > 254)
		return(NULL);

	for (i = 0; i < sz; i++) 
		if ( ! isalnum((unsigned char)cp[i]))
			if (NULL == strchr("-.", cp[i]))
				return(NULL);

	for (cp = start; '\0' != *cp; cp++)
		*cp = tolower((unsigned char)*cp);

	return(start);
}

int
kvalid_email(struct kpair *p)
{

	return(NULL != (p->parsed.s = valid_email(p->val)));
}

int
kvalid_udouble(struct kpair *p)
{

	if ( ! kvalid_double(p))
		return(0);
	return(isnormal(p->parsed.d) && p->parsed.d > 0.0);
}

int
kvalid_double(struct kpair *p)
{
	char		*ep;
	double		 lval;
	const char	*cp;

	if (NULL != (cp = strrchr(p->val, '.')))
		if (strlen(cp + 1) > 2) 
			return(0);

	errno = 0;
	lval = strtod(p->val, &ep);
	if (p->val[0] == '\0' || *ep != '\0')
		return(0);
	if (errno == ERANGE && (lval == HUGE_VAL || lval == -HUGE_VAL))
		return(0);
	p->parsed.d = lval;
	return(1);
}

int
kvalid_int(struct kpair *p)
{
	const char	*ep;

	p->parsed.i = strtonum
		(trim(p->val), INT64_MIN, INT64_MAX, &ep);
	return(NULL == ep);
}

int
kvalid_uint(struct kpair *p)
{
	const char	*ep;

	p->parsed.i = strtonum(trim(p->val), 1, INT64_MAX, &ep);
	return(NULL == ep);
}

#if 0
int
kvalid_pageid(struct kpair *p)
{

	if ( ! kvalid_int(p))
		return(0);
	return(p->parsed.i >= 0 && p->parsed.i < PAGE__MAX);
}
#endif
