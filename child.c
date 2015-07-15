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

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <poll.h>
#include <stdarg.h>
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "kcgi.h"
#include "extern.h"

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
	size_t	  ctypepos; /* position of ctype in mimes */
	char	 *xcode; /* encoding type */
	char	 *bound; /* form entry boundary */
};

struct	env {
	char	*key;
	size_t	 keysz;
	char	*val;
	size_t	 valsz;
};

struct 	fcgi_hdr {
	uint8_t	 version;
	uint8_t	 type;
	uint16_t requestId;
	uint16_t contentLength;
	uint8_t	 paddingLength;
	uint8_t	 reserved;
};

struct	fcgi_bgn {
	uint16_t role;
	uint8_t	 flags;
	uint8_t	 res[5];
};

struct	parms {
	int	 		 fd;
	const char *const	*mimes;
	size_t			 mimesz;
	const struct kvalid	*keys;
	size_t			 keysz;
	enum input		 type;
};

const char *const kmethods[KMETHOD__MAX] = {
	"ACL", /* KMETHOD_ACL */
	"CONNECT", /* KMETHOD_CONNECT */
	"COPY", /* KMETHOD_COPY */
	"DELETE", /* KMETHOD_DELETE */
	"GET", /* KMETHOD_GET */
	"HEAD", /* KMETHOD_HEAD */
	"LOCK", /* KMETHOD_LOCK */
	"MKCALENDAR", /* KMETHOD_MKCALENDAR */
	"MKCOL", /* KMETHOD_MKCOL */
	"MOVE", /* KMETHOD_MOVE */
	"OPTIONS", /* KMETHOD_OPTIONS */
	"POST", /* KMETHOD_POST */
	"PROPFIND", /* KMETHOD_PROPFIND */
	"PROPPATCH", /* KMETHOD_PROPPATCH */
	"PUT", /* KMETHOD_PUT */
	"REPORT", /* KMETHOD_REPORT */
	"TRACE", /* KMETHOD_TRACE */
	"UNLOCK", /* KMETHOD_UNLOCK */
};

static	const char *const krequs[KREQU__MAX] = {
	"HTTP_ACCEPT", /* KREQU_ACCEPT */
	"HTTP_ACCEPT_CHARSET", /* KREQU_ACCEPT_CHARSET */
	"HTTP_ACCEPT_ENCODING", /* KREQU_ACCEPT_ENCODING */
	"HTTP_ACCEPT_LANGUAGE", /* KREQU_ACCEPT_LANGUAGE */
	"HTTP_AUTHORIZATION", /* KREQU_AUTHORIZATION */
	"HTTP_DEPTH", /* KREQU_DEPTH */
	"HTTP_FROM", /* KREQU_FROM */
	"HTTP_HOST", /* KREQU_HOST */
	"HTTP_IF", /* KREQU_IF */
	"HTTP_IF_MODIFIED_SINCE", /* KREQU_IF_MODIFIED_SINCE */
	"HTTP_IF_MATCH", /* KREQU_IF_MATCH */
	"HTTP_IF_NONE_MATCH", /* KREQU_IF_NONE_MATCH */
	"HTTP_IF_RANGE", /* KREQU_IF_RANGE */
	"HTTP_IF_UNMODIFIED_SINCE", /* KREQU_IF_UNMODIFIED_SINCE */
	"HTTP_MAX_FORWARDS", /* KREQU_MAX_FORWARDS */
	"HTTP_PROXY_AUTHORIZATION", /* KREQU_PROXY_AUTHORIZATION */
	"HTTP_RANGE", /* KREQU_RANGE */
	"HTTP_REFERER", /* KREQU_REFERER */
	"HTTP_USER_AGENT", /* KREQU_USER_AGENT */
};

static	const char *const kauths[KAUTH_UNKNOWN] = {
	NULL,
	"basic",
	"digest"
};

/*
 * Parse the type/subtype field out of a content-type.
 * The content-type is defined (among other places) in RFC 822, and is
 * either the whole string or up until the ';', which marks the
 * beginning of the parameters.
 */
static size_t
str2ctype(const struct parms *pp, const char *ctype)
{
	size_t		 i, sz;
	const char	*end;
	

	if (NULL == ctype) 
		return(pp->mimesz);

	/* Stop at the content-type parameters. */
	if (NULL == (end = strchr(ctype, ';'))) 
		sz = strlen(ctype);
	else
		sz = end - ctype;

	for (i = 0; i < pp->mimesz; i++) {
		if (sz != strlen(pp->mimes[i]))
			continue;
		if (0 == strncasecmp(pp->mimes[i], ctype, sz))
			return(i);
	}

	return(i);
}

/*
 * Given a parsed field, first try to look it up and conditionally
 * validate any looked-up fields.
 * Then output the type, parse status (key, type, etc.), and values to
 * the output stream.
 * This is read by the parent's input() function.
 */
static void
output(const struct parms *pp, char *key, 
	char *val, size_t valsz, struct mime *mime)
{
	size_t	 	 i, sz;
	ptrdiff_t	 diff;
	char		*save;
	struct kpair	 pair;

	memset(&pair, 0, sizeof(struct kpair));

	pair.key = key;
	pair.val = save = val;
	pair.valsz = valsz;
	pair.file = NULL == mime ? NULL : mime->file;
	pair.ctype = NULL == mime ? NULL : mime->ctype;
	pair.xcode = NULL == mime ? NULL : mime->xcode;
	pair.ctypepos = NULL == mime ? pp->mimesz : mime->ctypepos;

	/*
	 * Look up the key name in our key table.
	 * If we find it and it has a validator, then run the validator
	 * and record the output.
	 * Either way, the keypos parameter is going to be the key
	 * identifier or keysz if none is found.
	 */
	for (i = 0; i < pp->keysz; i++) {
		if (strcmp(pp->keys[i].name, pair.key)) 
			continue;
		if (NULL != pp->keys[i].valid)
			pair.state = pp->keys[i].valid(&pair) ?
				KPAIR_VALID : KPAIR_INVALID;
		break;
	}
	pair.keypos = i;

	fullwrite(pp->fd, &pp->type, sizeof(enum input));

	sz = strlen(key);
	fullwrite(pp->fd, &sz, sizeof(size_t));
	fullwrite(pp->fd, pair.key, sz);

	fullwrite(pp->fd, &pair.valsz, sizeof(size_t));
	fullwrite(pp->fd, pair.val, pair.valsz);

	fullwrite(pp->fd, &pair.state, sizeof(enum kpairstate));
	fullwrite(pp->fd, &pair.type, sizeof(enum kpairtype));
	fullwrite(pp->fd, &pair.keypos, sizeof(size_t));

	if (KPAIR_VALID == pair.state) 
		switch (pair.type) {
		case (KPAIR_DOUBLE):
			fullwrite(pp->fd, &pair.parsed.d, sizeof(double));
			break;
		case (KPAIR_INTEGER):
			fullwrite(pp->fd, &pair.parsed.i, sizeof(int64_t));
			break;
		case (KPAIR_STRING):
			assert(pair.parsed.s >= pair.val);
			assert(pair.parsed.s <= pair.val + pair.valsz);
			diff = pair.val - pair.parsed.s;
			fullwrite(pp->fd, &diff, sizeof(ptrdiff_t));
			break;
		default:
			break;
		}

	sz = NULL != pair.file ? strlen(pair.file) : 0;
	fullwrite(pp->fd, &sz, sizeof(size_t));
	fullwrite(pp->fd, pair.file, sz);

	sz = NULL != pair.ctype ? strlen(pair.ctype) : 0;
	fullwrite(pp->fd, &sz, sizeof(size_t));
	fullwrite(pp->fd, pair.ctype, sz);
	fullwrite(pp->fd, &pair.ctypepos, sizeof(size_t));

	sz = NULL != pair.xcode ? strlen(pair.xcode) : 0;
	fullwrite(pp->fd, &sz, sizeof(size_t));
	fullwrite(pp->fd, pair.xcode, sz);

	/*
	 * We can write a new "val" in the validator allocated on the
	 * heap.
	 * If we do, free it here.
	 */
	if (save != pair.val)
		free(pair.val);
}

/*
 * Read full stdin request into memory.
 * This reads at most "len" bytes and nil-terminates the results, the
 * length of which may be less than "len" and is stored in *szp if not
 * NULL.
 * Returns the pointer to the data.
 */
static char *
scanbuf(const struct kworker *work, size_t len, size_t *szp)
{
	ssize_t		 ssz;
	size_t		 sz;
	char		*p;
	struct pollfd	 pfd;

	pfd.fd = work->input;
	pfd.events = POLLIN;

	/* Allocate the entire buffer here. */
	if (NULL == (p = XMALLOC(len + 1)))
		_exit(EXIT_FAILURE);

	/* Keep reading til we get all the data. */
	for (sz = 0; sz < len; sz += (size_t)ssz) {
		if (-1 == poll(&pfd, 1, -1)) {
			XWARN("poll: POLLIN");
			_exit(EXIT_FAILURE);
		}
		ssz = read(work->input, p + sz, len - sz);
		if (ssz < 0 && EAGAIN == errno) {
			XWARN("read: trying again");
			ssz = 0;
			continue;
		} else if (ssz < 0) {
			XWARN("short read: %zu", len - sz);
			_exit(EXIT_FAILURE);
		} else if (0 == ssz)
			break;
	}

	/* ALWAYS nil-terminate. */
	p[sz] = '\0';
	if (NULL != szp)
		*szp = sz;

	return(p);
}

/*
 * Reset a particular mime component.
 * We can get duplicates, so reallocate.
 */
static int
mime_reset(char **dst, const char *src)
{

	free(*dst);
	return(NULL != (*dst = XSTRDUP(src)));
}

/*
 * Parse out all MIME headers.
 * This is defined by RFC 2045.
 * This returns TRUE if we've parsed up to (and including) the last
 * empty CRLF line, or FALSE if something has gone wrong (e.g., parse
 * error, out of memory).
 * If FALSE, parsing should stop immediately.
 */
static int
mime_parse(const struct parms *pp, struct mime *mime, 
	char *buf, size_t len, size_t *pos)
{
	char	*key, *val, *end, *start, *line;
	int	 rc = 0;

	memset(mime, 0, sizeof(struct mime));

	while (*pos < len) {
		/* Each MIME line ends with a CRLF. */
		start = &buf[*pos];
		end = memmem(start, len - *pos, "\r\n", 2);
		if (NULL == end) {
			XWARNX("MIME header without CRLF");
			return(0);
		}
		/* Nil-terminate to make a nice line. */
		*end = '\0';
		/* Re-set our starting position. */
		*pos += (end - start) + 2;

		/* Empty line: we're done here! */
		if ('\0' == *start) {
			rc = 1;
			break;
		}

		/* Find end of MIME statement name. */
		key = start;
		if (NULL == (val = strchr(key, ':'))) {
			XWARNX("MIME header without key-value colon");
			return(0);
		}

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
		if (0 == strcasecmp(key, "content-transfer-encoding")) {
			if ( ! mime_reset(&mime->xcode, val))
				return(0);
		} else if (0 == strcasecmp(key, "content-disposition")) {
			if ( ! mime_reset(&mime->disp, val))
				return(0);
		} else if (0 == strcasecmp(key, "content-type")) {
			if ( ! mime_reset(&mime->ctype, val))
				return(0);
		} else
			continue;

		/* Now process any familiar MIME components. */
		while (NULL != (key = line)) {
			while (' ' == *key)
				key++;
			if ('\0' == *key)
				break;
			if (NULL == (val = strchr(key, '='))) {
				XWARNX("MIME header without "
					"subpart separator");
				return(0);
			}
			*val++ = '\0';

			if ('"' == *val) {
				val++;
				if (NULL == (line = strchr(val, '"'))) {
					XWARNX("MIME header quote "
						"not terminated");
					return(0);
				}
				*line++ = '\0';
				if (';' == *line)
					line++;
			} else if (NULL != (line = strchr(val, ';')))
				*line++ = '\0';

			/* White-listed sub-commands. */
			if (0 == strcasecmp(key, "filename")) {
				if ( ! mime_reset(&mime->file, val))
					return(0);
			} else if (0 == strcasecmp(key, "name")) {
				if ( ! mime_reset(&mime->name, val))
					return(0);
			} else if (0 == strcasecmp(key, "boundary")) {
				if ( ! mime_reset(&mime->bound, val))
					return(0);
			} else
				continue;
		}
	} 

	mime->ctypepos = str2ctype(pp, mime->ctype);

	if ( ! rc)
		XWARNX("MIME header unexpected EOF");

	return(rc);
}

/*
 * Free up all MIME headers.
 * We might call this more than once, so make sure that it can be
 * invoked again by setting the memory to zero.
 */
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
 * Parse keys and values separated by newlines.
 * I'm not aware of any standard that defines this, but the W3
 * guidelines for HTML give a rough idea.
 */
static void
parse_pairs_text(const struct parms *pp, char *p)
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
			XWARNX("text key: no value");
			continue;
		}

		if ('\0' == *key) {
			XWARNX("text key: zero-length");
			continue;
		}
		output(pp, key, val, strlen(val), NULL);
	}
}

/*
 * Parse an HTTP message that has a given content-type.
 * This happens with, e.g., PUT requests.
 * We fake up a "name" for this (it's not really a key-value pair) of an
 * empty string, then pass that to the validator and forwarder.
 */
static void
parse_body(const char *ct, const struct parms *pp, char *b, size_t bsz)
{
	char		 name;
	struct mime	 mime;

	memset(&mime, 0, sizeof(struct mime));
	mime.ctype = strdup(ct);
	mime.ctypepos = str2ctype(pp, mime.ctype);

	name = '\0';
	output(pp, &name, b, bsz, &mime);
	free(mime.ctype);
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
			if ('\0' == (hex[0] = *(p + 1))) {
				XWARNX("urldecode: short hex");
				return(0);
			} else if ('\0' == (hex[1] = *(p + 2))) {
				XWARNX("urldecode: short hex");
				return(0);
			} else if (1 != sscanf(hex, "%x", &c)) {
				XWARN("urldecode: bad hex");
				return(0);
			} else if ('\0' == c) {
				XWARN("urldecode: nil byte");
				return(0);
			}

			*p = (char)c;
			memmove(p + 1, p + 3, strlen(p + 3) + 1);
		} else
			*p = '+' == *p ? ' ' : *p;
	}

	*p = '\0';
	return(1);
}

/*
 * Parse out key-value pairs from an HTTP request variable.
 * This can be either a cookie or a POST/GET string.
 * This MUST be a non-binary (i.e., nil-terminated) string!
 */
static void
parse_pairs_urlenc(const struct parms *pp, char *p)
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
			XWARNX("url key: no value");
			continue;
		}

		if ('\0' == *key) {
			XWARNX("url key: zero length");
			continue;
		} else if ( ! urldecode(key)) {
			XWARNX("url key: key decode");
			break;
		} else if ( ! urldecode(val)) {
			XWARNX("url key: val decode");
			break;
		}

		output(pp, key, val, strlen(val), NULL);
	}
}

/*
 * This is described by the "multipart-body" BNF part of RFC 2046,
 * section 5.1.1.
 * We return TRUE if the parse was ok, FALSE if errors occured (all
 * calling parsers should bail too).
 */
static int
parse_multiform(const struct parms *pp, char *name, 
	const char *bound, char *buf, size_t len, size_t *pos)
{
	struct mime	 mime;
	size_t		 endpos, bbsz, partsz;
	char		*ln, *bb;
	int		 rc, first;

	rc = 0;
	/* Define our buffer boundary. */
	bbsz = asprintf(&bb, "\r\n--%s", bound);
	if (NULL == bb) {
		XWARN("asprintf");
		_exit(EXIT_FAILURE);
	}
	assert(bbsz > 0);
	memset(&mime, 0, sizeof(struct mime));

	/* Read to the next instance of a buffer boundary. */
	for (first = 1; *pos < len; first = 0, *pos = endpos) {
		/*
		 * The conditional is because the prologue FIRST
		 * boundary will not incur an initial CRLF, so our bb is
		 * past the CRLF and two bytes smaller.
		 */
		ln = memmem(&buf[*pos], len - *pos, 
			bb + (first ? 2 : 0), bbsz - (first ? 2 : 0));

		if (NULL == ln) {
			XWARNX("multiform: unexpected eof");
			goto out;
		}

		/* 
		 * Set "endpos" to point to the beginning of the next
		 * multipart component, i.e, the end of the boundary
		 * "bb" string.
		 */
		endpos = *pos + (ln - &buf[*pos]) + 
			bbsz - (first ? 2 : 0);

		/* Check buffer space... */
		if (endpos > len - 2) {
			XWARNX("multiform: end position out of bounds");
			goto out;
		}

		/* Terminating boundary or not... */
		if (memcmp(&buf[endpos], "--", 2)) {
			while (endpos < len && ' ' == buf[endpos])
				endpos++;
			/* We need the CRLF... */
			if (memcmp(&buf[endpos], "\r\n", 2)) {
				XWARNX("multiform: missing crlf");
				goto out;
			}
			endpos += 2;
		} else
			endpos = len;

		/* 
		 * Zero-length part or beginning of sequence.
		 * This shouldn't occur, but if it does, it'll screw up
		 * the MIME parsing (which requires a blank CRLF before
		 * considering itself finished).
		 */
		if (first || 0 == (partsz = ln - &buf[*pos]))
			continue;

		/* We now read our MIME headers, bailing on error. */
		mime_free(&mime);
		if ( ! mime_parse(pp, &mime, buf, *pos + partsz, pos)) {
			XWARNX("multiform: bad MIME headers");
			goto out;
		}
		/* 
		 * As per RFC 2388, we need a name and disposition. 
		 * Note that multipart/mixed bodies will inherit the
		 * name of their parent, so the mime.name is ignored.
		 */
		if (NULL == mime.name && NULL == name) {
			XWARNX("multiform: no MIME name");
			continue;
		} else if (NULL == mime.disp) {
			XWARNX("multiform: no MIME disposition");
			continue;
		}

		/* As per RFC 2045, we default to text/plain. */
		if (NULL == mime.ctype) {
			mime.ctype = XSTRDUP("text/plain");
			if (NULL == mime.ctype)
				_exit(EXIT_FAILURE);
		}

		partsz = ln - &buf[*pos];

		/* 
		 * Multipart sub-handler. 
		 * We only recognise the multipart/mixed handler.
		 * This will route into our own function, inheriting the
		 * current name for content.
		 */
		if (0 == strcasecmp(mime.ctype, "multipart/mixed")) {
			if (NULL == mime.bound) {
				XWARNX("multiform: missing boundary");
				goto out;
			}
			if ( ! parse_multiform
				(pp, NULL != name ? name :
				 mime.name, mime.bound, buf, 
				 *pos + partsz, pos)) {
				XWARNX("multiform: mixed part error");
				goto out;
			}
			continue;
		}

		assert('\r' == buf[*pos + partsz] || 
		       '\0' == buf[*pos + partsz]);
		if ('\0' != buf[*pos + partsz])
			buf[*pos + partsz] = '\0';

		/* Assign all of our key-value pair data. */
		output(pp, NULL != name ? name : 
			mime.name, &buf[*pos], partsz, &mime);
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
parse_multi(const struct kworker *work,
	const struct parms *pp, char *line, 
	size_t len, char *b, size_t bsz)
{
	char		*cp;

	while (' ' == *line)
		line++;
	if (';' != *line++)
		return;
	while (' ' == *line)
		line++;

	/* We absolutely need the boundary marker. */
	if (strncmp(line, "boundary", 8)) {
		XWARNX("multiform: missing boundary");
		return;
	}
	line += 8;
	while (' ' == *line)
		line++;
	if ('=' != *line++)
		return;
	while (' ' == *line)
		line++;

	/* Make sure the line is terminated in the right place .*/
	if ('"' == *line) {
		if (NULL == (cp = strchr(++line, '"'))) {
			XWARNX("multiform: unterminated quote");
			return;
		}
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
	len = 0;
	parse_multiform(pp, NULL, line, b, bsz, &len);
}

/*
 * Output all of the HTTP_xxx headers.
 * This transforms the HTTP_xxx header (CGI form) into HTTP form, which
 * is the second part title-cased, e.g., HTTP_FOO = Foo.
 */
static void
kworker_child_env(const struct env *env, int fd, size_t envsz)
{
	size_t	 	 i, j, sz, reqs;
	int		 first;
	enum krequ	 requ;
	char		 c;
	const char	*cp;

	for (reqs = i = 0; i < envsz; i++)
		if (0 == strncmp(env[i].key, "HTTP_", 5))
			reqs++;

	fullwrite(fd, &reqs, sizeof(size_t));
	for (i = 0; i < envsz; i++) {
		/*
		 * First, search for the key name (HTTP_XXX) in our list
		 * of known headers.
		 */
		if (strncmp(env[i].key, "HTTP_", 5))
			continue;
		for (requ = 0; requ < KREQU__MAX; requ++) {
			if (0 == strcmp(krequs[requ], env[i].key))
				break;
		}
		fullwrite(fd, &requ, sizeof(enum krequ));

		/*
		 * According to RFC 3875, 4.1.18, HTTP headers are
		 * re-written into CGI environment variables by
		 * uppercasing and converting dashes to underscores.
		 * In this part, we [try to] reverse that so that the
		 * headers are properly identified.
		 * (We also skip the HTTP_ leading part.)
		 */
		sz = env[i].keysz - 5;
		cp = env[i].key + 5;
		fullwrite(fd, &sz, sizeof(size_t));
		for (j = 0, first = 1; j < sz; j++) {
			if ('_' == cp[j]) {
				c = '-';
				first = 1;
			} else if (first) {
				c = cp[j];
				first = 0;
			} else
				c = tolower((int)cp[j]);
			fullwrite(fd, &c, 1);
		}

		fullwrite(fd, &env[i].valsz, sizeof(size_t));
		fullwrite(fd, env[i].val, env[i].valsz);
	}
}

/*
 * Output the method found in our environment.
 * Returns the method.
 * Defaults to KMETHOD_GET, uses KETHOD__MAX if the method was bad.
 */
static enum kmethod
kworker_child_method(const struct env *env, int fd, size_t envsz)
{
	enum kmethod	 meth;
	size_t		 i;

	/* RFC 3875, 4.1.12. */
	/* We assume GET if not supplied. */
	meth = KMETHOD_GET;
	for (i = 0; i < envsz; i++) {
		if (strcmp(env[i].key, "REQUEST_METHOD"))
			continue;
		for (meth = 0; meth < KMETHOD__MAX; meth++)
			if (0 == strcmp(kmethods[meth], env[i].val))
				break;
		break;
	}
	fullwrite(fd, &meth, sizeof(enum kmethod));
	return(meth);
}

/*
 * Output the web server's authentication.
 * Defaults to KAUTH_NONE.
 */
static void
kworker_child_auth(const struct env *env, int fd, size_t envsz)
{
	enum kauth	 auth;
	size_t		 i;

	/* Determine authentication: RFC 3875, 4.1.1. */
	auth = KAUTH_NONE;
	for (i = 0; i < envsz; i++) {
		if (strcmp(env[i].key, "AUTH_TYPE"))
			continue;
		for (auth = 0; auth < KAUTH_UNKNOWN; auth++) {
			if (NULL == kauths[auth])
				continue;
			if (0 == strcmp(kauths[auth], env[i].val))
				break;
		}
	}
	fullwrite(fd, &auth, sizeof(enum kauth));
}

static void
kworker_child_rawauth(const struct env *env, int fd, size_t envsz)
{
	size_t	 i;

	for (i = 0; i < envsz; i++) 
		if (0 == strcmp(env[i].key, "HTTP_AUTHORIZATION"))
			kworker_auth_child(fd, env[i].val);
	if (i == envsz)
		kworker_auth_child(fd, NULL);
}

static void
kworker_child_scheme(const struct env *env, int fd, size_t envsz)
{
	size_t	 	 i;
	const char	*cp;
	enum kscheme	 scheme;

	/* 
	 * This isn't defined in any RFC.
	 * It seems to be the best way of getting whether we're HTTPS,
	 * as the SERVER_PROTOCOL (RFC 3875, 4.1.16) doesn't reliably
	 * return the scheme.
	 */
	cp = "off";
	for (i = 0; i < envsz; i++)
		if (0 == strcmp(env[i].key, "HTTPS")) {
			cp = env[i].val;
			break;
		}

	scheme = 0 == strcasecmp(cp, "on") ?
		KSCHEME_HTTPS : KSCHEME_HTTP;
	fullwrite(fd, &scheme, sizeof(enum kscheme));
}

static void
kworker_child_remote(const struct env *env, int fd, size_t envsz)
{
	const char	*cp;
	size_t	 	 i, len;

	/* RFC 3875, 4.1.8. */
	/* Never supposed to be NULL, but to be sure... */
	cp = "127.0.0.1";
	for (i = 0; i < envsz; i++)
		if (0 == strcmp(env[i].key, "REMOTE_ADDR")) {
			cp = env[i].val;
			break;
		}

	len = strlen(cp);
	fullwrite(fd, &len, sizeof(size_t));
	fullwrite(fd, cp, len);
}

static void
kworker_child_path(struct env *env, int fd, size_t envsz)
{
	char	*cp, *ep, *sub;
	size_t	 i, len;

	cp = NULL;
	for (i = 0; i < envsz; i++) 
		if (0 == strcmp(env[i].key, "PATH_INFO")) {
			cp = env[i].val;
			break;
		}

	/*
	 * Parse the first path element (the page we want to access),
	 * subsequent path information, and the file suffix.  We convert
	 * suffix and path element into the respective enum's inline.
	 */
	if (NULL != cp) {
		len = strlen(cp);
		fullwrite(fd, &len, sizeof(size_t));
		fullwrite(fd, cp, len);
	} else {
		len = 0;
		fullwrite(fd, &len, sizeof(size_t));
	}

	/* This isn't possible in the real world. */
	if (NULL != cp && '/' == *cp)
		cp++;

	if (NULL != cp && '\0' != *cp) {
		ep = cp + strlen(cp) - 1;
		while (ep > cp && '/' != *ep && '.' != *ep)
			ep--;

		/* Start with writing our suffix. */
		if ('.' == *ep) {
			*ep++ = '\0';
			len = strlen(ep);
			fullwrite(fd, &len, sizeof(size_t));
			fullwrite(fd, ep, len);
		} else {
			len = 0;
			fullwrite(fd, &len, sizeof(size_t));
		}

		/* Now find the top-most path part. */
		if (NULL != (sub = strchr(cp, '/')))
			*sub++ = '\0';

		/* Send the base path. */
		len = strlen(cp);
		fullwrite(fd, &len, sizeof(size_t));
		fullwrite(fd, cp, len);

		/* Send the path part. */
		if (NULL != sub) {
			len = strlen(sub);
			fullwrite(fd, &len, sizeof(size_t));
			fullwrite(fd, sub, len);
		} else {
			len = 0;
			fullwrite(fd, &len, sizeof(size_t));
		}
	} else {
		len = 0;
		/* Suffix, base path, and path part. */
		fullwrite(fd, &len, sizeof(size_t));
		fullwrite(fd, &len, sizeof(size_t));
		fullwrite(fd, &len, sizeof(size_t));
	}
}

static void
kworker_child_body(struct env *env, int fd, size_t envsz,
	struct parms *pp, enum kmethod meth,
	const struct kworker *work, char *b, size_t bsz)
{
	size_t 	 i, len;
	char	*cp, *bp = b;

	/*
	 * The CONTENT_LENGTH must be a valid integer.
	 * Since we're storing into "len", make sure it's in size_t.
	 * If there's an error, it will default to zero.
	 * Note that LLONG_MAX < SIZE_MAX.
	 * RFC 3875, 4.1.2.
	 */
	len = 0;
	for (i = 0; i < envsz; i++)
		if (0 == strcmp(env[i].key, "CONTENT_LENGTH")) {
			cp = env[i].val;
			len = strtonum(cp, 0, LLONG_MAX, NULL);
			break;
		}

	if (NULL != bp && bsz != len)
		XWARNX("content real and reported length differ");

	/*
	 * If a CONTENT_TYPE has been specified (i.e., POST or GET has
	 * been set -- we don't care which), then switch on that type
	 * for parsing out key value pairs.
	 * RFC 3875, 4.1.3.
	 * HTML5, 4.10.
	 * We only support the main three content types.
	 */
	pp->type = IN_FORM;
	for (i = 0; i < envsz; i++)
		if (0 == strcmp(env[i].key, "CONTENT_TYPE"))
			break;

	if (NULL == b) {
		assert(NULL != work);
		b = scanbuf(work, len, &bsz);
	} else
		assert(NULL == work);

	if (i < envsz) {
		cp = env[i].val;
		if (0 == strcasecmp(cp, "application/x-www-form-urlencoded"))
			parse_pairs_urlenc(pp, b);
		else if (0 == strncasecmp(cp, "multipart/form-data", 19)) 
			parse_multi(work, pp, cp + 19, len, b, bsz);
		else if (KMETHOD_POST == meth && 0 == strcasecmp(cp, "text/plain"))
			parse_pairs_text(pp, b);
		else
			parse_body(cp, pp, b, bsz);
	} else
		parse_body(kmimetypes[KMIME_APP_OCTET_STREAM], pp, b, bsz);

	if (NULL == bp)
		free(b);
}

static void
kworker_child_query(struct env *env, 
	int fd, size_t envsz, struct parms *pp)
{
	size_t	 i;

	/*
	 * Even POST requests are allowed to have QUERY_STRING elements,
	 * so parse those out now.
	 * Note: both QUERY_STRING and CONTENT_TYPE fields share the
	 * same field space.
	 * Since this is a getenv(), we know the returned value is
	 * nil-terminated.
	 */
	pp->type = IN_QUERY;
	for (i = 0; i < envsz; i++) {
		if (strcmp(env[i].key, "QUERY_STRING"))
			continue;
		parse_pairs_urlenc(pp, env[i].val);
		break;
	}
}

static void
kworker_child_cookies(struct env *env, 
	int fd, size_t envsz, struct parms *pp)
{
	size_t	 i;

	/*
	 * Cookies come last.
	 * These use the same syntax as QUERY_STRING elements, but don't
	 * share the same namespace (just as a means to differentiate
	 * the same names).
	 * Since this is a getenv(), we know the returned value is
	 * nil-terminated.
	 */
	pp->type = IN_COOKIE;
	for (i = 0; i < envsz; i++) {
		if (strcmp(env[i].key, "HTTP_COOKIE"))
			continue;
		parse_pairs_urlenc(pp, env[i].val);
		break;
	}
}

/*
 * This is the child kcgi process that's going to do the unsafe reading
 * of network data to parse input.
 * When it parses a field, it outputs the key, key size, value, and
 * value size along with the field type.
 * We use the CGI specification in RFC 3875.
 */
void
kworker_child(const struct kworker *work, 
	const struct kvalid *keys, size_t keysz, 
	const char *const *mimes, size_t mimesz)
{
	struct parms	  pp;
	char		 *cp;
	char		**evp;
	int		  wfd;
	enum kmethod	  meth;
	size_t	 	  i;
	extern char	**environ;
	struct env	 *envs;
	size_t		  envsz;

	pp.fd = wfd = work->sock[KWORKER_WRITE];
	pp.keys = keys;
	pp.keysz = keysz;
	pp.mimes = mimes;
	pp.mimesz = mimesz;

	/*
	 * Pull the enire environment into an array.
	 */
	for (envsz = 0, evp = environ; NULL != *evp; evp++) 
		envsz++;
	envs = XCALLOC(envsz, sizeof(struct env));
	if (NULL == envs)
		return;
	/*
	 * While pulling in the environment, look for HTTP headers
	 * (those beginning with HTTP_).
	 * This conforms to RFC 3875, 4.1.18.
	 */
	for (i = 0, evp = environ; NULL != *evp; evp++) {
		/* Disregard crappy environment entries. */
		if (NULL == (cp = strchr(*evp, '=')))
			continue;
		envs[i].key = XSTRDUP(*evp);
		envs[i].val = strchr(envs[i].key, '=');
		*envs[i].val++ = '\0';
		envs[i].keysz = strlen(envs[i].key);
		envs[i].valsz = strlen(envs[i].val);
		i++;
	}
	/* Reset this, accounting for crappy entries. */
	envsz = i;

	/*
	 * Now run a series of transmissions based upon what's in our
	 * environment array.
	 */
	kworker_child_env(envs, wfd, envsz);
	meth = kworker_child_method(envs, wfd, envsz);
	kworker_child_auth(envs, wfd, envsz);
	kworker_child_rawauth(envs, wfd, envsz);
	kworker_child_scheme(envs, wfd, envsz);
	kworker_child_remote(envs, wfd, envsz);
	kworker_child_path(envs, wfd, envsz);

	/* And now the message body itself. */
	kworker_child_body(envs, wfd, envsz, &pp, 
		meth, work, NULL, 0);
	kworker_child_query(envs, wfd, envsz, &pp);
	kworker_child_cookies(envs, wfd, envsz, &pp);

	/* Note: the "val" is from within the key. */
	for (i = 0; i < envsz; i++) 
		free(envs[i].key);
	free(envs);
}

/*
 * Read the FastCGI header (see section 8, Types and Contents,
 * FCGI_Header, in the FastCGI Specification v1.0).
 * Return zero if have data, non-zero on success.
 */
static struct fcgi_hdr *
kworker_fcgi_header(int fd, struct fcgi_hdr *hdr, unsigned char *buf)
{
	enum kcgi_err	 er;
	struct fcgi_hdr	*ptr;
	int		 rc;

	if ((rc = fullread(fd, buf, 8, 1, &er)) < 0) {
		XWARNX("failed read fcgi header");
		return(NULL);
	} else if (rc == 0) {
		XWARNX("end of fcgi headers");
		return(NULL);
	}

	ptr = (struct fcgi_hdr *)buf;

	hdr->version = ptr->version;
	hdr->type = ptr->type;
	hdr->requestId = ntohs(ptr->requestId);
	hdr->contentLength = ntohs(ptr->contentLength);
	hdr->paddingLength = ntohs(ptr->paddingLength);
	if (1 != hdr->version) {
		XWARNX("bad fastcgi header version");
		return(NULL);
	}
	fprintf(stderr, "%s: version: %" PRIu8 "\n", 
		__func__, hdr->version);
	fprintf(stderr, "%s: type: %" PRIu8 "\n", 
		__func__, hdr->type);
	fprintf(stderr, "%s: id: %" PRIu8 "\n", 
		__func__, hdr->requestId);
	fprintf(stderr, "%s: content-length: %" PRIu16 "\n", 
		__func__, hdr->contentLength);
	fprintf(stderr, "%s: padding-length: %" PRIu16 "\n", 
		__func__, hdr->paddingLength);
	return(hdr);
}

static int
kworker_fcgi_content(int fd, const struct fcgi_hdr *hdr,
	unsigned char **buf, size_t *bufmaxsz)
{
	void		*ptr;
	enum kcgi_err	 er;

	if (hdr->contentLength > *bufmaxsz) {
		*bufmaxsz = hdr->contentLength;
		if (NULL == (ptr = XREALLOC(*buf, *bufmaxsz)))
			return(0);
		*buf = ptr;
	}
	return(fullread(fd, *buf, hdr->contentLength, 0, &er));

}

static struct fcgi_bgn *
kworker_fcgi_begin(int fd, struct fcgi_bgn *bgn,
	unsigned char **b, size_t *bsz)
{
	struct fcgi_hdr	*hdr, realhdr;
	struct fcgi_bgn	*ptr;
	enum kcgi_err	 er;

	assert(*bsz >= 8);
	if (NULL == (hdr = kworker_fcgi_header(fd, &realhdr, *b)))
		return(NULL);

	if (1 != hdr->type) {
		XWARNX("bad fastcgi initial header type");
		return(NULL);
	} else if ( ! kworker_fcgi_content(fd, hdr, b, bsz)) {
		XWARNX("failed read fcgi begin");
		return(NULL);
	}

	ptr = (struct fcgi_bgn *)*b;
	bgn->role = ntohs(ptr->role);
	bgn->flags = ptr->flags;

	fprintf(stderr, "%s: role: %" PRIu16 "\n", 
		__func__, bgn->role);
	fprintf(stderr, "%s: flags: %" PRIu8 "\n", 
		__func__, bgn->flags);

	if (0 == fulldiscard(fd, hdr->paddingLength, &er)) {
		XWARNX("failed discard fcgi begin pad");
		return(NULL);
	}
	return(bgn);
}

static int
kworker_fcgi_stdin(int fd, const struct fcgi_hdr *hdr,
	unsigned char **bp, size_t *bsz, 
	unsigned char **sbp, size_t *ssz)
{
	enum kcgi_err	 er;
	void		*ptr;

	if ( ! kworker_fcgi_content(fd, hdr, bp, bsz)) {
		XWARNX("failed read fcgi params");
		return(-1);
	} else if (0 == fulldiscard(fd, hdr->paddingLength, &er)) {
		XWARNX("failed discard fcgi params pad");
		return(-1);
	} 
	
	
	ptr = XREALLOC(*sbp, *ssz + hdr->contentLength + 1);
	if (NULL == ptr)
		return(-1);
	*sbp = ptr;
	memcpy(*sbp, *bp, hdr->contentLength);
	(*sbp)[*ssz + hdr->contentLength] = '\0';
	*ssz += hdr->contentLength;

	fprintf(stderr, "%s: data: %" PRIu16 " bytes\n", 
		__func__, hdr->contentLength);

	return(hdr->contentLength > 0);
}

static int
kworker_fcgi_params(int fd, const struct fcgi_hdr *hdr,
	unsigned char **bp, size_t *bsz,
	struct env **envs, size_t *envsz, size_t *envmaxsz)
{
	size_t	 	 i, remain, pos, keysz, valsz;
	unsigned char	*b;
	enum kcgi_err	 er;
	void		*ptr;

	if ( ! kworker_fcgi_content(fd, hdr, bp, bsz)) {
		XWARNX("failed read fcgi params");
		return(0);
	} else if (0 == fulldiscard(fd, hdr->paddingLength, &er)) {
		XWARNX("failed discard fcgi params pad");
		return(0);
	}

	b = *bp;
	remain = hdr->contentLength;
	pos = 0;
	while (remain > 0) {
		assert(pos < hdr->contentLength);
		if (0 != b[pos] >> 7) {
			if (remain > 3) {
				keysz = ((b[pos] & 0x7f) << 24) + 
					  (b[pos + 1] << 16) + 
					  (b[pos + 2] << 8) + 
					   b[pos + 3];
				pos += 4;
				remain -= 4;
			} else {
				XWARNX("invalid fcgi params");
				return(0);
			}
		} else {
			keysz = b[pos];
			pos++;
			remain--;
		}
		if (remain < 1) {
			XWARNX("invalid fcgi params");
			return(0);
		}
		assert(pos < hdr->contentLength);
		if (0 != b[pos] >> 7) {
			if (remain > 3) {
				valsz = ((b[pos] & 0x7f) << 24) + 
					  (b[pos + 1] << 16) + 
					  (b[pos + 2] << 8) + 
					   b[pos + 3];
				pos += 4;
				remain -= 4;
			} else {
				XWARNX("invalid fcgi params");
				return(0);
			}
		} else {
			valsz = b[pos];
			pos++;
			remain--;
		}
		if (pos + keysz + valsz > hdr->contentLength) {
			XWARNX("invalid fcgi params");
			return(0);
		}
		remain -= keysz;
		remain -= valsz;
		for (i = 0; i < *envsz; i++) {
			if ((*envs)[i].keysz != keysz)
				continue;
			if (0 == memcmp((*envs)[i].key, &b[pos], keysz))
				break;
		}

		if (i == *envsz) {
			ptr = XREALLOCARRAY
				(*envs, *envsz + 1, 
				 sizeof(struct env));
			if (NULL == ptr)
				return(0);
			*envs = ptr;
			(*envs)[i].key = XMALLOC(keysz + 1);
			if (NULL == (*envs)[i].key)
				return(0);
			memcpy((*envs)[i].key, &b[pos], keysz);
			(*envs)[i].key[keysz] = '\0';
			(*envs)[i].keysz = keysz;
			(*envsz)++;
		} else
			free((*envs)[i].val);

		pos += keysz;

		(*envs)[i].val = XMALLOC(valsz + 1);
		if (NULL == (*envs)[i].val)
			return(0);
		memcpy((*envs)[i].val, &b[pos], valsz);
		(*envs)[i].val[valsz] = '\0';
		(*envs)[i].valsz = valsz;
		pos += valsz;
		fprintf(stderr, "%s: params: %s=%s\n", 
			__func__, (*envs)[i].key, (*envs)[i].val);
	}
	return(1);
}

/*
 * This is executed by the untrusted child for FastCGI setups.
 * Throughout, we follow the FastCGI specification, version 1.0, 29
 * April 1996.
 */
void
kworker_fcgi_child(const struct kworker *work, 
	const struct kvalid *keys, size_t keysz, 
	const char *const *mimes, size_t mimesz)
{
	struct parms 	 pp;
	struct fcgi_hdr	*hdr, realhdr;
	struct fcgi_bgn	*bgn, realbgn;
	unsigned char	*buf, *sbuf;
	struct env	*envs;
	size_t		 i, bsz, ssz, envsz, envmaxsz;
	int		 wfd, rc;
	enum kmethod	 meth;

	sbuf = NULL;
	envmaxsz = 0;
	envs = NULL;
	bsz = 8;
	if (NULL == (buf = XMALLOC(bsz)))
		return;

	pp.fd = wfd = work->sock[KWORKER_WRITE];
	pp.keys = keys;
	pp.keysz = keysz;
	pp.mimes = mimes;
	pp.mimesz = mimesz;

	/*
	 * Loop over all incoming sequences to this particular slave.
	 * Sequences must consist of a single FastCGI session as defined
	 * in the FastCGI version 1.0 reference document.
	 */
	for (;;) {
		free(sbuf);
		ssz = 0;
		fprintf(stderr, "%s: new sequence\n", __func__);
		bgn = kworker_fcgi_begin
			(work->control[KWORKER_READ],
			 &realbgn, &buf, &bsz);
		if (NULL == bgn)
			break;

		/*
		 * Now read one or more parameters.
		 * We'll first read them all at once, then parse the
		 * headers in the content one by one.
		 */
		envsz = 0;
		for (;;) {
			hdr = kworker_fcgi_header
				(work->control[KWORKER_READ],
				 &realhdr, buf);
			if (NULL == hdr)
				break;
			if (4 != hdr->type)
				break;
			if (kworker_fcgi_params
				(work->control[KWORKER_READ],
				 hdr, &buf, &bsz,
				 &envs, &envsz, &envmaxsz))
				continue;
			hdr = NULL;
			break;
		}
		if (NULL == hdr)
			break;

		if (5 != hdr->type) {
			XWARNX("unknown fcgi header type");
			break;
		}

		fprintf(stderr, "%s: report "
			"sequence parameters\n", __func__);
		kworker_child_env(envs, wfd, envsz);
		meth = kworker_child_method(envs, wfd, envsz);
		kworker_child_auth(envs, wfd, envsz);
		kworker_child_rawauth(envs, wfd, envsz);
		kworker_child_scheme(envs, wfd, envsz);
		kworker_child_remote(envs, wfd, envsz);
		kworker_child_path(envs, wfd, envsz);

		/*
		 * Lastly, we want to process the stdin content.
		 * These will end with a single zero-length record.
		 * Keep looping til we've flushed all input.
		 */
		for (rc = 0;;) {
			rc = kworker_fcgi_stdin
				(work->control[KWORKER_READ],
				 hdr, &buf, &bsz, &sbuf, &ssz);
			if (0 == rc)
				break;
			hdr = kworker_fcgi_header
				(work->control[KWORKER_READ],
				 &realhdr, buf);
			if (NULL == hdr)
				break;
			if (5 == hdr->type) 
				continue;
			XWARNX("unknown fcgi header type");
			hdr = NULL;
			break;
		}
		if (rc < 0 || NULL == hdr)
			break;

		/* And now the message body itself. */
		kworker_child_body(envs, wfd, envsz, &pp, 
			meth, NULL, sbuf, ssz);
		kworker_child_query(envs, wfd, envsz, &pp);
		kworker_child_cookies(envs, wfd, envsz, &pp);

		fprintf(stderr, "%s: finished sequence\n", __func__);
	}

	for (i = 0; i < envmaxsz; i++) {
		free(envs[i].key);
		free(envs[i].val);
	}

	free(sbuf);
	free(buf);
	free(envs);
}
