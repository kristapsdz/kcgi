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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/mman.h>
#include <sys/stat.h>

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <math.h> /* HUGE_VAL */
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <zlib.h>

#include "kcgi.h"

enum	kstate {
	KSTATE_HEAD = 0,
	KSTATE_BODY
};

struct	kdata {
	enum kelem	 elems[128];
	size_t		 elemsz;
	enum kstate	 state;
#ifdef	HAVE_ZLIB
	gzFile		 gz;
#endif
};

struct	mimemap {
	const char	*name;
	enum kmime	 mime;
};

/*
 * For handling HTTP multipart forms.
 * This consists of data for a single multipart form entry.
 */
struct	mime {
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

static	const uint16_t entities[KENTITY__MAX] = {
	38, /* KENTITY_AMP */
	0xE9, /* KENTITY_EACUTE */
	0x3E, /* KENTITY_GT */
	0x2190, /* KENTITY_LARR */
	0x3C, /* KENTITY_LT */
	0x2014, /* KENTITY_MDASH */
	0x2013, /* KENTITY_NDASH */
	0x2192, /* KENTITY_RARR */
};

static	const struct tag tags[KELEM__MAX] = {
	{ 0, "a" }, /* KELEM_A */
	{ 1, "blockquote" }, /* KELEM_BLOCKQUOTE */
	{ 1, "body" }, /* KELEM_BODY */
	{ 3, "br" }, /* KELEM_BR */
	{ 1, "caption" }, /* KELEM_CAPTION */
	{ 0, "code" }, /* KELEM_CODE */
	{ 3, "col" }, /* KELEM_COL */
	{ 1, "dd" }, /* KELEM_DD */
	{ 1, "div" }, /* KELEM_DIV */
	{ 1, "dl" }, /* KELEM_DL */
	{ 1, "dt" }, /* KELEM_DT */
	{ 1, "fieldset" }, /* KELEM_FIELDSET */
	{ 1, "form" }, /* KELEM_FORM */
	{ 1, "head" }, /* KELEM_HEAD */
	{ 1, "html" }, /* KELEM_HTML */
	{ 0, "i" }, /* KELEM_I */
	{ 2, "img" }, /* KELEM_IMG */
	{ 2, "input" }, /* KELEM_INPUT */
	{ 0, "label" }, /* KELEM_LABEL */
	{ 0, "legend" }, /* KELEM_LEGEND */
	{ 1, "li" }, /* KELEM_LI */
	{ 3, "link" }, /* KELEM_LINK */
	{ 3, "meta" }, /* KELEM_META */
	{ 0, "option" }, /* KELEM_OPTION */
	{ 1, "p" }, /* KELEM_P */
	{ 0, "q" }, /* KELEM_Q */
	{ 1, "select" }, /* KELEM_SELECT */
	{ 0, "span" }, /* KELEM_SPAN */
	{ 0, "strong" }, /* KELEM_STRONG */
	{ 1, "table" }, /* KELEM_TABLE */
	{ 1, "td" }, /* KELEM_TD */
	{ 1, "textarea" }, /* KELEM_TEXTAREA */
	{ 1, "th" }, /* KELEM_TH */
	{ 0, "title" }, /* KELEM_TITLE */
	{ 1, "tr" }, /* KELEM_TR */
	{ 1, "ul" }, /* KELEM_UL */
	{ 0, "var" }, /* KELEM_VAR */
};

const char *const kfields[KFIELD__MAX] = {
	"email",
	"password",
	"text",
	"number",
	"submit"
};

const char *const kmimetypes[KMIME__MAX] = {
	"text/html", /* KMIME_HTML */
	"text/csv", /* KMIME_CSV */
	"image/png", /* KMIME_PNG */
};

const char *const khttps[KHTTP__MAX] = {
	"100 Continue",
	"101 Switching Protocols",
	"103 Checkpoint",
	"200 OK",
	"201 Created",
	"202 Accepted",
	"203 Non-Authoritative Information",
	"204 No Content",
	"205 Reset Content",
	"206 Partial Content",
	"300 Multiple Choices",
	"301 Moved Permanently",
	"302 Found",
	"303 See Other",
	"304 Not Modified",
	"306 Switch Proxy",
	"307 Temporary Redirect",
	"308 Resume Incomplete",
	"400 Bad Request",
	"401 Unauthorized",
	"402 Payment Required",
	"403 Forbidden",
	"404 Not Found",
	"405 Method Not Allowed",
	"406 Not Acceptable",
	"407 Proxy Authentication Required",
	"408 Request Timeout",
	"409 Conflict",
	"410 Gone",
	"411 Length Required",
	"412 Precondition Failed",
	"413 Request Entity Too Large",
	"414 Request-URI Too Long",
	"415 Unsupported Media Type",
	"416 Requested Range Not Satisfiable",
	"417 Expectation Failed",
	"500 Internal Server Error",
	"501 Not Implemented",
	"502 Bad Gateway",
	"503 Service Unavailable",
	"504 Gateway Timeout",
	"505 HTTP Version Not Supported",
	"511 Network Authentication Required",
};

static 	const struct mimemap suffixmap[] = {
	{ "html", KMIME_HTML },
	{ "htm", KMIME_HTML },
	{ "csv", KMIME_CSV },
	{ "png", KMIME_PNG },
	{ NULL, KMIME__MAX },
};

/*
 * Default MIME suffix per type.
 */
const char *const ksuffixes[KMIME__MAX] = {
	"html", /* KMIME_HTML */
	"csv", /* KMIME_CSV */
	"png", /* KIME_PNG */
};

static	const char *const attrs[KATTR__MAX] = {
	"action", /* KATTR_ACTION */
	"checked", /* KATTR_CHECKED */
	"class", /* KATTR_CLASS */
	"clear", /* KATTR_CLEAR */
	"colspan", /* KATTR_COLSPAN */
	"content", /* KATTR_CONTENT */
	"disabled", /* KATTR_DISABLED */
	"enctype", /* KATTR_ENCTYPE */
	"for", /* KATTR_FOR */
	"href", /* KATTR_HREF */
	"http-equiv", /* KATTR_HTTP_EQUIV */
	"id", /* KATTR_ID */
	"max", /* KATTR_MAX */
	"method", /* KATTR_METHOD */
	"min", /* KATTR_MIN */
	"multiple", /* KATTR_MULTIPLE */
	"name", /* KATTR_NAME */
	"onclick", /* KATTR_ONCLICK */
	"rel", /* KATTR_REL */
	"rowspan", /* KATTR_ROWSPAN */
	"selected", /* KATTR_SELECTED */
	"span", /* KATTR_SPAN */
	"src", /* KATTR_SRC */
	"step", /* KATTR_STEP */
	"style", /* KATTR_STYLE */
	"target", /* KATTR_TARGET */
	"type", /* KATTR_TYPE */
	"value", /* KATTR_VALUE */
	"width", /* KATTR_WIDTH */
};

/* 
 * Name of executing CGI script.
 */
const char	*pname = NULL;

/*
 * Wrapper for printing data to the standard output.
 * This switches depending on compression support and whether our system
 * supports zlib compression at all.
 */
#ifdef HAVE_ZLIB
#define	KPRINTF(_req, ...) \
	do if (NULL != (_req)->kdata->gz) \
		gzprintf((_req)->kdata->gz, __VA_ARGS__); \
	else \
		printf(__VA_ARGS__); \
	while (0)
#else
#define	KPRINTF(_req, ...) printf(__VA_ARGS__)
#endif

void
khttp_puts(struct kreq *req, const char *cp)
{

	if (NULL != req->kdata->gz)
		gzputs(req->kdata->gz, cp);
	else 
		fputs(cp, stdout);
}

void
khttp_putc(struct kreq *req, int c)
{

	if (NULL != req->kdata->gz)
		gzputc(req->kdata->gz, c);
	else 
		putchar(c);
}

/* 
 * Safe strdup(): don't return on memory failure.
 */
char *
kstrdup(const char *cp)
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
kxrealloc(void *pp, size_t sz)
{
	char	*p;

	assert(sz > 0);
	if (NULL != (p = realloc(pp, sz)))
		return(p);

	perror(NULL);
	exit(EXIT_FAILURE);
}

/*
 * Safe realloc() with overflow-checking.
 */
void *
krealloc(void *pp, size_t nm, size_t sz)
{
	char	*p;

	if (nm && sz && SIZE_MAX / nm < sz) {
		errno = ENOMEM;
		perror(NULL);
		exit(EXIT_FAILURE);
	}

	assert(nm * sz > 0);
	if (NULL != (p = realloc(pp, nm * sz)))
		return(p);

	perror(NULL);
	exit(EXIT_FAILURE);
}

void *
kasprintf(const char *fmt, ...)
{
	char	*p;
	va_list	 ap;

	va_start(ap, fmt);
	vasprintf(&p, fmt, ap);
	va_end(ap);

	if (NULL != p)
		return(p);

	perror(NULL);
	exit(EXIT_FAILURE);
}

/*
 * Safe calloc(): don't return on exhaustion.
 */
void *
kcalloc(size_t nm, size_t sz)
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
kmalloc(size_t sz)
{
	char	*p;

	if (NULL != (p = malloc(sz)))
		return(p);

	perror(NULL);
	exit(EXIT_FAILURE);
}

static void
kpair_expand(struct kpair **kv, size_t *kvsz)
{

	*kv = krealloc(*kv, *kvsz + 1, sizeof(struct kpair));
	memset(&(*kv)[*kvsz], 0, sizeof(struct kpair));
	(*kvsz)++;
}

size_t
khtml_elemat(struct kreq *req)
{

	assert(KSTATE_BODY == req->kdata->state);
	return(req->kdata->elemsz);
}

void
khtml_elem(struct kreq *req, enum kelem elem)
{

	assert(KSTATE_BODY == req->kdata->state);
	khtml_attr(req, elem, KATTR__MAX);
}

void
khtml_input(struct kreq *req, size_t key)
{
	const char	*cp;
	char		 buf[64];

	assert(KSTATE_BODY == req->kdata->state);

	cp = "";
	if (NULL != req->fieldmap[key])
		switch (req->fieldmap[key]->type) {
		case (KPAIR_DOUBLE):
			snprintf(buf, sizeof(buf), "%.2f",
				req->fieldmap[key]->parsed.d);
			cp = buf;
			break;
		case (KPAIR_INTEGER):
			snprintf(buf, sizeof(buf), "%" PRId64, 
				req->fieldmap[key]->parsed.i);
			cp = buf;
			break;
		case (KPAIR_STRING):
			cp = req->fieldmap[key]->parsed.s;
			break;
		default:
			abort();
			break;
		}

	assert(req->keys[key].field < KFIELD__MAX);
	khtml_attr(req, KELEM_INPUT,
		KATTR_TYPE, kfields[req->keys[key].field],
		KATTR_NAME, req->keys[key].name,
		KATTR_VALUE, cp,
		KATTR__MAX);
}

char *
kutil_urlencode(const char *cp)
{
	char	*p;
	char	 ch;
	size_t	 sz;
	char	 buf[4];

	if (NULL == cp)
		return(NULL);

	/* 
	 * Leave three bytes per input byte for encoding. 
	 * This ensures we needn't range-check.
	 * First check whether our size overflows. 
	 * We do this here because we need our size!
	 */
	sz = strlen(cp) + 1;
	if (SIZE_MAX / 3 < sz) {
		errno = ENOMEM;
		perror(NULL);
		exit(EXIT_FAILURE);
	}
	p = kcalloc(sz, 3);
	sz *= 3;

	for ( ; '\0' != (ch = *cp); cp++) {
		/* Put in a temporary buffer then concatenate. */
		memset(buf, 0, sizeof(buf));
		if (' ' == ch) 
			buf[0] = '+';
		else if (isalnum((int)ch))
			buf[0] = ch;
		else
			(void)snprintf(buf, sizeof(buf), "%%%.2x", ch);
		(void)strlcat(p, buf, sz);
	}

	return(p);
}

char *
kutil_urlpart(struct kreq *req, enum kmime mime, size_t page, ...)
{
	va_list		 ap;
	char		*p, *pagep, *keyp, *valp;
	size_t		 total, key, count;

	pagep = kutil_urlencode(req->pages[page]);
	p = kasprintf("%s/%s.%s", pname, pagep, ksuffixes[mime]);
	free(pagep);
	total = strlen(p) + 1;

	va_start(ap, page);
	count = 0;
	while ((key = va_arg(ap, size_t)) < req->keysz) {
		keyp = kutil_urlencode(req->keys[key].name);
		valp = kutil_urlencode(va_arg(ap, char *));
		/* Size for key, value, ? or &, and =. */
		/* FIXME: check for overflow! */
		total += strlen(keyp) + strlen(valp) + 2;
		p = kxrealloc(p, total);

		if (count > 0)
			(void)strlcat(p, "&", total);
		else
			(void)strlcat(p, "?", total);

		(void)strlcat(p, keyp, total);
		(void)strlcat(p, "=", total);
		(void)strlcat(p, valp, total);

		free(keyp);
		free(valp);
		count++;
	}
	va_end(ap);
	return(p);
}

void
khtml_attr(struct kreq *req, enum kelem elem, ...)
{
	va_list		 ap;
	enum kattr	 at;
	struct kdata	*k = req->kdata;
	const char	*cp;

	assert(KSTATE_BODY == req->kdata->state);
	KPRINTF(req, "<%s", tags[elem].name);

	va_start(ap, elem);
	while (KATTR__MAX != (at = va_arg(ap, enum kattr))) {
		cp = va_arg(ap, char *);
		assert(NULL != cp);
		KPRINTF(req, " %s=\"%s\"", attrs[at], cp);

	}
	va_end(ap);

	if (TAG_ACLOSE & tags[elem].flags)
		khttp_putc(req, '/');
	khttp_putc(req, '>');

	if (TAG_BLOCK & tags[elem].flags) 
		khttp_putc(req, '\n');

	if ( ! (TAG_ACLOSE & tags[elem].flags))
		k->elems[k->elemsz++] = elem;
	assert(k->elemsz < 128);
}

void
khtml_close(struct kreq *req, size_t sz)
{
	size_t		 i;
	struct kdata	*k = req->kdata;

	assert(KSTATE_BODY == req->kdata->state);

	if (0 == sz)
		sz = k->elemsz;

	for (i = 0; i < sz; i++) {
		assert(k->elemsz > 0);
		k->elemsz--;
		KPRINTF(req, "</%s>", tags[k->elems[k->elemsz]].name);
		if (TAG_BLOCK & tags[k->elems[k->elemsz]].flags) 
			khttp_putc(req, '\n');
	}
}

void
khtml_closeto(struct kreq *req, size_t pos)
{

	assert(KSTATE_BODY == req->kdata->state);
	assert(pos < req->kdata->elemsz);
	khtml_close(req, req->kdata->elemsz - pos);
}

void
khtml_decl(struct kreq *req)
{

	assert(KSTATE_BODY == req->kdata->state);
	khttp_puts(req, "<!DOCTYPE html>\n");
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

static void
parse_pairs_text(struct kreq *req, char *p)
{
	char            *key, *val;

	while (NULL != p && '\0' != *p) {
		/* Skip leading whitespace. */
		while (' ' == *p)
			p++;

		key = p;
		val = NULL;
		if (NULL != (p = strchr(p, '='))) {
			/* Key/value pair. */
			*p++ = '\0';
			val = p;
			p = strstr(val, "\r\n");
			if (NULL != p) {
				*p = '\0';
				p += 2;
			}
		} else {
			/* No value--error. */
			p = strstr(key, "\r\n");
			if (NULL != p) {
				*p = '\0';
				p += 2;
			}
			continue;
		}

		if ('\0' == *key || '\0' == *val)
			continue;
		kpair_expand(&req->fields, &req->fieldsz);
		req->fields[req->fieldsz - 1].key = kstrdup(key);
		req->fields[req->fieldsz - 1].val = kstrdup(val);
		req->fields[req->fieldsz - 1].valsz = strlen(val);
	}
}


/*
 * Parse out key-value pairs from an HTTP request variable.
 * This can be either a cookie or a POST/GET string.
 * This MUST be a non-binary (i.e., nil-terminated) string!
 */
static void
parse_pairs_urlenc(struct kpair **kv, size_t *kvsz, char *p)
{
	char            *key, *val;
	size_t           sz;

	while (NULL != p && '\0' != *p) {
		/* Skip leading whitespace. */
		while (' ' == *p)
			p++;

		key = p;
		val = NULL;
		if (NULL != (p = strchr(p, '='))) {
			/* Key/value pair. */
			*p++ = '\0';
			val = p;
			sz = strcspn(p, ";&");
			p += sz;
			if ('\0' != *p)
				*p++ = '\0';
		} else {
			/* No value--error. */
			p = key;
			sz = strcspn(p, ";&");
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
		kpair_expand(kv, kvsz);
		(*kv)[*kvsz - 1].key = kstrdup(key);
		(*kv)[*kvsz - 1].val = kstrdup(val);
		(*kv)[*kvsz - 1].valsz = strlen(val);
	}
}

/*
 * Read full stdin request into memory.
 * This reads at most "len" bytes and nil-terminates the results, the
 * length of which may be less than "len" and is stored in *szp if not
 * NULL.
 * Returns the pointer to the data.
 */
static char *
scanbuf(size_t len, size_t *szp)
{
	ssize_t		 ssz;
	size_t		 sz;
	char		*p;

	/* Allocate the entire buffer here. */
	p = kmalloc(len + 1);

	/* Keep reading til we get all the data. */
	/* FIXME: use poll() to avoid blocking. */
	for (sz = 0; sz < len; sz += (size_t)ssz) {
		ssz = read(STDIN_FILENO, p + sz, len - sz);
		if (ssz < 0) {
			perror(NULL);
			exit(EXIT_FAILURE);
		} else if (0 == ssz)
			break;
	}

	/* ALWAYS nil-terminate. */
	p[sz] = '\0';
	if (NULL != szp)
		*szp = sz;
	return(p);
}

static void
parse_text(size_t len, struct kreq *req)
{
	char		*p;

	p = scanbuf(len, NULL);
	parse_pairs_text(req, p);
	free(p);
}

static void
parse_urlenc(size_t len, struct kreq *req)
{
	char		*p;

	p = scanbuf(len, NULL);
	parse_pairs_urlenc(&req->fields, &req->fieldsz, p);
	free(p);
}

/*
 * Reset a particular mime component.
 * We can get duplicates, so reallocate.
 */
static void
mime_reset(char **dst, const char *src)
{

	free(*dst);
	*dst = kstrdup(src);
}

/*
 * Parse out all MIME headers.
 * This is defined by RFC 2045.
 * This returns TRUE if we've parsed up to (and including) the last
 * empty CRLF line, or FALSE if something has gone wrong.
 * If FALSE, parsing should stop immediately.
 */
static int
mime_parse(struct mime *mime, char *buf, size_t len, size_t *pos)
{
	char		*key, *val, *end, *start, *line;

	memset(mime, 0, sizeof(struct mime));

	while (*pos < len) {
		/* Each MIME line ends with a CRLF. */
		start = &buf[*pos];
		end = memmem(start, len - *pos, "\r\n", 2);
		if (NULL == end)
			return(0);
		/* Nil-terminate to make a nice line. */
		*end = '\0';
		/* Re-set our starting position. */
		*pos += (end - start) + 2;

		/* Empty line: we're done here! */
		if ('\0' == *start)
			return(1);

		/* Find end of MIME statement name. */
		key = start;
		if (NULL == (val = strchr(key, ':')))
			return(0);
		*val++ = '\0';
		while (' ' == *val)
			val++;
		line = NULL;
		if (NULL != (line = strchr(val, ';')))
			*line++ = '\0';

		/* 
		 * Allow these specific MIME header statements.
		 * Ignore all others.
		 */
		if (0 == strcasecmp(key, "content-transfer-encoding"))
			mime_reset(&mime->xcode, val);
		else if (0 == strcasecmp(key, "content-disposition"))
			mime_reset(&mime->disp, val);
		else if (0 == strcasecmp(key, "content-type"))
			mime_reset(&mime->ctype, val);
		else
			continue;

		/* Now process any familiar MIME components. */
		while (NULL != (key = line)) {
			while (' ' == *key)
				key++;
			if ('\0' == *key)
				break;
			if (NULL == (val = strchr(key, '=')))
				return(0);
			*val++ = '\0';

			if ('"' == *val) {
				val++;
				if (NULL == (line = strchr(val, '"')))
					return(0);
				*line++ = '\0';
				if (';' == *line)
					line++;
			} else if (NULL != (line = strchr(val, ';')))
				*line++ = '\0';

			/* White-listed sub-commands. */
			if (0 == strcasecmp(key, "filename"))
				mime_reset(&mime->file, val);
			else if (0 == strcasecmp(key, "name"))
				mime_reset(&mime->name, val);
			else if (0 == strcasecmp(key, "boundary"))
				mime_reset(&mime->bound, val);
			else
				continue;
		}
	} 

	return(0);
}

static void
mime_free(struct mime *mime)
{

	free(mime->disp);
	free(mime->name);
	free(mime->file);
	free(mime->ctype);
	free(mime->xcode);
	free(mime->bound);
	memset(mime, 0, sizeof(struct mime));
}

/*
 * This is described by the "multipart-body" BNF part of RFC 2046,
 * section 5.1.1.
 * We return TRUE if the parse was ok, FALSE if errors occured (all
 * calling parsers should bail too).
 */
static int
parse_multiform(struct kreq *req, const char *name,
	const char *bound, char *buf, size_t len, size_t *pos)
{
	struct mime	 mime;
	size_t		 endpos, bbsz, partsz;
	char		*ln, *bb;
	int		 rc, first;

	rc = 0;
	/* Define our buffer boundary. */
	bb = kasprintf("\r\n--%s", bound);
	bbsz = strlen(bb);
	memset(&mime, 0, sizeof(struct mime));

	/* Read to the next instance of a buffer boundary. */
	for (first = 1; *pos < len; first = 0, *pos = endpos) {
		/*
		 * The conditional is because the prologue, if not
		 * specified, will not incur an initial CRLF, so our bb
		 * is past the CRLF and two bytes smaller.
		 * This ONLY occurs for the first read, however.
		 */
		ln = memmem(&buf[*pos], len - *pos, 
			bb + (first ? 2 : 0), bbsz - (first ? 2 : 0));

		/* Unexpected EOF for this part. */
		if (NULL == ln)
			goto out;

		/* 
		 * Set "endpos" to point to the beginning of the next
		 * multipart component.
		 * We set "endpos" to be at the very end if the
		 * terminating boundary buffer occurs.
		 */
		endpos = *pos + (ln - &buf[*pos]) + bbsz - (first ? 2 : 0);
		/* Check buffer space... */
		if (endpos > len - 2)
			goto out;

		/* Terminating boundary or not... */
		if (memcmp(&buf[endpos], "--", 2)) {
			while (endpos < len && ' ' == buf[endpos])
				endpos++;
			/* We need the CRLF... */
			if (memcmp(&buf[endpos], "\r\n", 2))
				goto out;
			endpos += 2;
		} else
			endpos = len;

		/* 
		 * Zero-length part. 
		 * This shouldn't occur, but if it does, it'll screw up
		 * the MIME parsing (which requires a blank CRLF before
		 * considering itself finished).
		 */
		if (0 == (partsz = ln - &buf[*pos]))
			continue;

		/* We now read our MIME headers, bailing on error. */
		mime_free(&mime);
		if ( ! mime_parse(&mime, buf, *pos + partsz, pos))
			goto out;
		/* 
		 * As per RFC 2388, we need a name and disposition. 
		 * Note that multipart/mixed bodies will inherit the
		 * name of their parent, so the mime.name is ignored.
		 */
		if (NULL == mime.name && NULL == name)
			continue;
		else if (NULL == mime.disp) 
			continue;
		/* As per RFC 2045, we default to text/plain. */
		if (NULL == mime.ctype) 
			mime.ctype = kstrdup("text/plain");

		partsz = ln - &buf[*pos];

		/* 
		 * Multipart sub-handler. 
		 * We only recognise the multipart/mixed handler.
		 * This will route into our own function, inheriting the
		 * current name for content.
		 */
		if (0 == strcasecmp(mime.ctype, "multipart/mixed")) {
			if (NULL == mime.bound)
				goto out;
			if ( ! parse_multiform
				(req, NULL != name ? name :
				 mime.name, mime.bound,
				 buf, *pos + partsz, pos))
				goto out;
			continue;
		}

		/* Assign all of our key-value pair data. */
		kpair_expand(&req->fields, &req->fieldsz);
		req->fields[req->fieldsz - 1].key = 
			kstrdup(NULL != name ? name : mime.name);
		req->fields[req->fieldsz - 1].val = kmalloc(partsz + 1);
		memcpy(req->fields[req->fieldsz - 1].val, &buf[*pos], partsz);
		req->fields[req->fieldsz - 1].val[partsz] = '\0';
		req->fields[req->fieldsz - 1].valsz = partsz;
		if (NULL != mime.file)
			req->fields[req->fieldsz - 1].file = 
				kstrdup(mime.file);
		if (NULL != mime.ctype)
			req->fields[req->fieldsz - 1].ctype = 
				kstrdup(mime.ctype);
		if (NULL != mime.xcode)
			req->fields[req->fieldsz - 1].xcode = 
				kstrdup(mime.xcode);
	}

	/*
	 * According to the specification, we can have transport
	 * padding, a CRLF, then the epilogue.
	 * But since we don't care about that crap, just pretend that
	 * everything's fine and exit.
	 */
	rc = 1;
out:
	free(bb);
	mime_free(&mime);
	return(rc);
}

/*
 * Parse the boundary from a multipart CONTENT_TYPE and pass it to the
 * actual parsing engine.
 * This doesn't actually handle any part of the MIME specification.
 */
static void
parse_multi(struct kreq *req, char *line, size_t len)
{
	char		*cp;
	size_t		 sz;

	while (' ' == *line)
		line++;
	if (';' != *line++)
		return;
	while (' ' == *line)
		line++;

	/* We absolutely need the boundary marker. */
	if (strncmp(line, "boundary", 8)) 
		return;
	line += 8;
	while (' ' == *line)
		line++;
	if ('=' != *line++)
		return;
	while (' ' == *line)
		line++;

	/* Make sure the line is terminated in the right place .*/
	if ('"' == *line) {
		if (NULL == (cp = strchr(++line, '"')))
			return;
		*cp = '\0';
	} else {
		/*
		 * XXX: this may not properly follow RFC 2046, 5.1.1,
		 * which specifically lays out the boundary characters.
		 * We simply jump to the first whitespace.
		 */
		for (cp = line; '\0' != *cp && ' ' != *cp; cp++)
			/* Spin. */ ;
		*cp = '\0';
	}

	/* Read in full file. */
	cp = scanbuf(len, &sz);
	len = 0;
	parse_multiform(req, NULL, line, cp, sz, &len);
	free(cp);
}

/*
 * Parse a request from an HTTP request.
 * This consists of paths, suffixes, methods, and most importantly,
 * pasred query string, cookie, and form data.
 */
void
khttp_parse(struct kreq *req, 
	const struct kvalid *keys, size_t keysz,
	const char *const *pages, size_t pagesz,
	size_t defpage)
{
	char		*cp, *ep, *sub;
	enum kmime	 m;
	const struct mimemap *mm;
	size_t		 p, i, j, len;

	if (NULL == (pname = getenv("SCRIPT_NAME")))
		pname = "";

	memset(req, 0, sizeof(struct kreq));

	req->keys = keys;
	req->keysz = keysz;
	req->pages = pages;
	req->pagesz = pagesz;
	req->kdata = kcalloc(1, sizeof(struct kdata));
	req->cookiemap = kcalloc(keysz, sizeof(struct kpair *));
	req->cookienmap = kcalloc(keysz, sizeof(struct kpair *));
	req->fieldmap = kcalloc(keysz, sizeof(struct kpair *));
	req->fieldnmap = kcalloc(keysz, sizeof(struct kpair *));

	/* Determine authenticaiton: RFC 3875, 4.1.1. */
	if (NULL != (cp = getenv("AUTH_TYPE"))) {
		if (0 == strcasecmp(cp, "basic"))
			req->auth = KAUTH_BASIC;
		else if (0 == strcasecmp(cp, "digest"))
			req->auth = KAUTH_DIGEST;
		else
			req->auth = KAUTH_UNKNOWN;
	}

	sub = NULL;
	p = defpage;
	m = KMIME_HTML;

	/* RFC 3875, 4.1.8. */
	/* Never supposed to be NULL, but to be sure... */
	if (NULL == (cp = getenv("REMOTE_ADDR")))
		req->remote = kstrdup("127.0.0.1");
	else
		req->remote = kstrdup(cp);

	/* RFC 3875, 4.1.12. */
	/* Note that we assume GET, POST being explicit. */
	req->method = KMETHOD_GET;
	if (NULL != (cp = getenv("REQUEST_METHOD")))
		if (0 == strcmp(cp, "POST"))
			req->method = KMETHOD_POST;

	/*
	 * First, parse the first path element (the page we want to
	 * access), subsequent path information, and the file suffix.
	 * We convert suffix and path element into the respective enum's
	 * inline.
	 */
	if (NULL != (cp = getenv("PATH_INFO")))
		req->fullpath = kstrdup(cp);

	/* This isn't possible in the real world. */
	if (NULL != cp && '/' == *cp)
		cp++;

	if (NULL != cp && '\0' != *cp) {
		ep = cp + strlen(cp) - 1;
		/* Look up mime type from suffix. */
		while (ep > cp && '/' != *ep && '.' != *ep)
			ep--;
		if ('.' == *ep) {
			*ep++ = '\0';
			req->suffix = kstrdup(ep);
			for (mm = suffixmap; NULL != mm->name; mm++)
				if (0 == strcasecmp(mm->name, ep)) {
					m = mm->mime;
					break;
				}
			if (NULL == mm)
				m = KMIME__MAX;
		}
		if (NULL != (sub = strchr(cp, '/')))
			*sub++ = '\0';
		/* Look up page type from component. */
		for (p = 0; p < pagesz; p++)
			if (0 == strcasecmp(pages[p], cp))
				break;
	}

	req->mime = m;
	req->page = p;

	/* Assign subpath to remaining parts. */
	if (NULL != sub)
		req->path = kstrdup(sub);

	/*
	 * The CONTENT_LENGTH must be a valid integer.
	 * Since we're storing into "len", make sure it's in size_t.
	 * If there's an error, it will default to zero.
	 * Note that LLONG_MAX < SIZE_MAX.
	 * RFC 3875, 4.1.2.
	 */
	len = 0;
	if (NULL != (cp = getenv("CONTENT_LENGTH")))
		len = strtonum(cp, 0, LLONG_MAX, NULL);

	/*
	 * If a CONTENT_TYPE has been specified (i.e., POST or GET has
	 * been set -- we don't care which), then switch on that type
	 * for parsing out key value pairs.
	 * RFC 3875, 4.1.3.
	 * HTML5, 4.10.
	 * We only support the main three content types.
	 */
	if (NULL != (cp = getenv("CONTENT_TYPE"))) {
		if (0 == strcasecmp(cp, "application/x-www-form-urlencoded"))
			parse_urlenc(len, req);
		else if (0 == strncasecmp(cp, "multipart/form-data", 19)) 
			parse_multi(req, cp + 19, len);
		else if (0 == strcasecmp(cp, "text/plain"))
			parse_text(len, req);
	} else
		parse_text(len, req);

	/*
	 * Even POST requests are allowed to have QUERY_STRING elements,
	 * so parse those out now.
	 * Note: both QUERY_STRING and CONTENT_TYPE fields share the
	 * same field space.
	 * Since this is a getenv(), we know the returned value is
	 * nil-terminated.
	 */
	if (NULL != (cp = getenv("QUERY_STRING")))
		parse_pairs_urlenc(&req->fields, &req->fieldsz, cp);

	/*
	 * Cookies come last.
	 * These use the same syntax as QUERY_STRING elements, but don't
	 * share the same namespace (just as a means to differentiate
	 * the same names).
	 * Since this is a getenv(), we know the returned value is
	 * nil-terminated.
	 */
	if (NULL != (cp = getenv("HTTP_COOKIE")))
		parse_pairs_urlenc(&req->cookies, &req->cookiesz, cp);

	/*
	 * Run through all fields and sort them into named buckets.
	 * This will let us do constant-time lookups within the
	 * application itself.  Niiiice.
	 */
	for (i = 0; i < keysz; i++) {
		for (j = 0; j < req->fieldsz; j++) {
			if (strcmp(req->fields[j].key, keys[i].name))
				continue;
			if (NULL != keys[i].valid &&
				! (*keys[i].valid)(&req->fields[j])) {
				req->fields[j].type = KPAIR__MAX;
				req->fields[j].next = req->fieldnmap[i];
				req->fieldnmap[i] = &req->fields[j];
				continue;
			}
			assert(NULL == keys[i].valid ||
				KPAIR__MAX != req->fields[j].type);
			req->fields[j].next = req->fieldmap[i];
			req->fieldmap[i] = &req->fields[j];
		}
		for (j = 0; j < req->cookiesz; j++) {
			if (strcmp(req->cookies[j].key, keys[i].name))
				continue;
			if (NULL != keys[i].valid &&
				! keys[i].valid(&req->cookies[j])) {
				req->cookies[j].type = KPAIR__MAX;
				req->cookies[j].next = req->cookienmap[i];
				req->cookienmap[i] = &req->cookies[j];
				continue;
			}
			assert(NULL == keys[i].valid ||
				KPAIR__MAX != req->cookies[j].type);
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
		free(p[i].file);
		free(p[i].ctype);
	}
	free(p);
}

void
khttp_free(struct kreq *req)
{

#ifdef HAVE_ZLIB
	if (NULL != req->kdata->gz)
		gzclose(req->kdata->gz);
#endif
	kpair_free(req->cookies, req->cookiesz);
	kpair_free(req->fields, req->fieldsz);
	free(req->path);
	free(req->fullpath);
	free(req->remote);
	free(req->cookiemap);
	free(req->cookienmap);
	free(req->fieldmap);
	free(req->fieldnmap);
	free(req->kdata);
	free(req->suffix);
}

void
khtml_entity(struct kreq *req, enum kentity entity)
{

	assert(entity < KENTITY__MAX);
	assert(KSTATE_BODY == req->kdata->state);
	khtml_ncr(req, entities[entity]);
}

void
khtml_ncr(struct kreq *req, uint16_t ncr)
{

	assert(KSTATE_BODY == req->kdata->state);
	KPRINTF(req, "&#x%" PRIu16 ";", ncr);
}

void
khttp_head(struct kreq *req, const char *key, const char *fmt, ...)
{
	va_list	 ap;

	assert(KSTATE_HEAD == req->kdata->state);

	printf("%s: ", key);
	va_start(ap, fmt);
	vprintf(fmt, ap);
	printf("\r\n");
	va_end(ap);
}

void
khttp_body(struct kreq *req)
{
#ifdef HAVE_ZLIB
	const char	*cp;
#endif
	assert(KSTATE_HEAD == req->kdata->state);

	/*
	 * If gzip is an accepted encoding, then create the "gz" stream
	 * that will be used for all subsequent I/O operations.
	 */
#ifdef HAVE_ZLIB
	if (NULL != (cp = getenv("HTTP_ACCEPT_ENCODING")) &&
			NULL != strstr(cp, "gzip")) {
		req->kdata->gz = gzdopen(STDOUT_FILENO, "w");
		if (NULL == req->kdata->gz) {
			perror(NULL);
			exit(EXIT_FAILURE);
		}
		khttp_head(req, "Content-Encoding", "%s", "gzip");
	} 
#endif
	/*
	 * XXX: newer versions of zlib have a "T" transparent mode that
	 * one can add to gzdopen() that allows using the gz without any
	 * compression.
	 * Unfortunately, that's not guaranteed on all systems, so we
	 * must do without it.
	 */

	fputs("\r\n", stdout);
	fflush(stdout);
	req->kdata->state = KSTATE_BODY;
}

/*
 * Emit text in an HTML document.
 * This means, minimally, that we need to escape the open and close
 * delimiters for HTML tags.
 */
void
khtml_text(struct kreq *req, const char *cp)
{

	/* TODO: speed up with strcspn. */
	assert(KSTATE_BODY == req->kdata->state);
	for ( ; NULL != cp && '\0' != *cp; cp++)
		switch (*cp) {
		case ('>'):
			khtml_entity(req, KENTITY_GT);
			break;
		case ('&'):
			khtml_entity(req, KENTITY_AMP);
			break;
		case ('<'):
			khtml_entity(req, KENTITY_LT);
			break;
		default:
			khttp_putc(req, *cp);
			break;
		}
}

/*
 * Trim leading and trailing whitespace from a word.
 * Note that this returns a pointer within "val" and optionally sets the
 * nil-terminator, so don't free() the returned value.
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
 * Simple email address validation: this is NOT according to the spec,
 * but a simple heuristic look at the address.
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
kvalid_string(struct kpair *p)
{

	/*
	 * To check if we're a valid string, simply make sure that the
	 * nil pointer is where we expect it to be.
	 */
	if (strlen(p->val) != p->valsz)
		return(0);
	p->type = KPAIR_STRING;
	p->parsed.s = p->val;
	return(1);
}

int
kvalid_email(struct kpair *p)
{

	if ( ! kvalid_string(p))
		return(0);
	return(NULL != (p->parsed.s = valid_email(p->val)));
}

int
kvalid_udouble(struct kpair *p)
{

	if ( ! kvalid_double(p))
		return(0);
	p->type = KPAIR_DOUBLE;
	return(p->parsed.d > 0.0);
}

int
kvalid_double(struct kpair *p)
{
	char		*ep;
	double		 lval;
	const char	*cp;

	if ( ! kvalid_string(p))
		return(0);

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
	p->type = KPAIR_DOUBLE;
	return(1);
}

int
kvalid_int(struct kpair *p)
{
	const char	*ep;

	if ( ! kvalid_string(p))
		return(0);
	p->parsed.i = strtonum
		(trim(p->val), INT64_MIN, INT64_MAX, &ep);
	p->type = KPAIR_INTEGER;
	return(NULL == ep);
}

int
kvalid_uint(struct kpair *p)
{
	const char	*ep;

	p->parsed.i = strtonum(trim(p->val), 1, INT64_MAX, &ep);
	p->type = KPAIR_INTEGER;
	return(NULL == ep);
}

/*
 * There are all sorts of ways to make this faster and more efficient.
 * For now, do it the easily-auditable way.
 * Memory-map the given file and look through it character by character
 * til we get to the key delimiter "@@".
 * Once there, scan to the matching "@@".
 * Look for the matching key within these pairs.
 * If found, invoke the callback function with the given key.
 */
int
khtml_template(struct kreq *req, 
	const struct ktemplate *t, const char *fname)
{
	struct stat 	 st;
	char		*buf;
	size_t		 sz, i, j, len, start, end;
	int		 fd, rc;

	assert(KSTATE_BODY == req->kdata->state);

	if (-1 == (fd = open(fname, O_RDONLY, 0)))
		return(0);

	if (-1 == fstat(fd, &st)) {
		close(fd);
		return(0);
	} else if (st.st_size >= (1U << 31)) {
		close(fd);
		return(0);
	} else if (0 == st.st_size) {
		close(fd);
		return(1);
	}

	sz = (size_t)st.st_size;
	buf = mmap(NULL, sz, PROT_READ, MAP_SHARED, fd, 0);

	if (NULL == buf) {
		close(fd);
		return(0);
	}

	rc = 0;

	for (i = 0; i < sz - 1; i++) {
		/* Look for the starting "@@" marker. */
		if ('@' != buf[i]) {
			khttp_putc(req, buf[i]);
			continue;
		} else if ('@' != buf[i + 1]) {
			khttp_putc(req, buf[i]);
			continue;
		} 

		/* Seek to find the end "@@" marker. */
		start = i + 2;
		for (end = start + 2; end < sz - 1; end++)
			if ('@' == buf[end] && '@' == buf[end + 1])
				break;

		/* Continue printing if not found of 0-length. */
		if (end == sz - 1 || end == start) {
			khttp_putc(req, buf[i]);
			continue;
		}

		/* Look for a matching key. */
		for (j = 0; j < t->keysz; j++) {
			len = strlen(t->key[j]);
			if (len != end - start)
				continue;
			else if (memcmp(&buf[start], t->key[j], len))
				continue;
			if ( ! (*t->cb)(j, t->arg))
				goto out;
			break;
		}

		/* Didn't find it... */
		if (j == t->keysz)
			khttp_putc(req, buf[i]);
		else
			i = end + 1;
	}

	if (i < sz)
		khttp_putc(req, buf[i]);

	rc = 1;
out:
	munmap(buf, sz);
	close(fd);
	return(rc);
}
