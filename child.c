/*	$Id$ */
/*
 * Copyright (c) 2012, 2014--2016, 2018 Kristaps Dzonsons <kristaps@bsd.lv>
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
#include "config.h"

#include <arpa/inet.h>

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#if HAVE_MD5
# include <sys/types.h>
# include <md5.h>
#endif
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

#define MD5Updatec(_ctx, _b, _sz) \
	MD5Update((_ctx), (const uint8_t *)(_b), (_sz))

enum	mimetype {
	MIMETYPE_UNKNOWN,
	MIMETYPE_TRANSFER_ENCODING,
	MIMETYPE_DISPOSITION,
	MIMETYPE_TYPE
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
	size_t	  ctypepos; /* position of ctype in mimes */
	char	 *xcode; /* encoding type */
	char	 *bound; /* form entry boundary */
};

/*
 * Both CGI and FastCGI use an environment for their HTTP parameters.
 * CGI gets it from the actual environment; FastCGI from a transmitted
 * environment.
 * We use an abstract representation of those key-value pairs here so
 * that we can use the same functions for both.
 */
struct	env {
	char	*key; /* key (e.g., HTTP_HOST) */
	size_t	 keysz;
	char	*val; /* value (e.g., `foo.com') */
	size_t	 valsz;
};

/* 
 * Types of FastCGI requests.
 * Defined in the FastCGI v1.0 spec, section 8.
 */
enum	fcgi_type {
	FCGI_BEGIN_REQUEST = 1,
	FCGI_ABORT_REQUEST = 2,
	FCGI_END_REQUEST = 3,
	FCGI_PARAMS = 4,
	FCGI_STDIN = 5,
	FCGI_STDOUT = 6,
	FCGI_STDERR = 7,
	FCGI_DATA = 8,
	FCGI_GET_VALUES = 9,
	FCGI_GET_VALUES_RESULT = 10,
	FCGI_UNKNOWN_TYPE = 11,
	FCGI__MAX
};

/*
 * The FastCGI `FCGI_Header' header layout.
 * Defined in the FastCGI v1.0 spec, section 8.
 */
struct 	fcgi_hdr {
	uint8_t	 version;
	uint8_t	 type;
	uint16_t requestId;
	uint16_t contentLength;
	uint8_t	 paddingLength;
	uint8_t	 reserved;
};

/*
 * The FastCGI `FCGI_BeginRequestBody' header layout.
 * Defined in the FastCGI v1.0 spec, section 8.
 */
struct	fcgi_bgn {
	uint16_t role;
	uint8_t	 flags;
	uint8_t	 res[5];
};

/*
 * A buffer of reads from kfcgi_control().
 */
struct	fcgi_buf {
	size_t	 sz; /* bytes in buffer */
	size_t	 pos; /* current position (from last read) */
	int	 fd; /* file descriptor */
	char	*buf; /* buffer itself */
};

/*
 * Parameters required to validate fields.
 */
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

	if (NULL == ctype) 
		return(pp->mimesz);

	/* Stop at the content-type parameters. */
	sz = strcspn(ctype, ";");

	for (i = 0; i < pp->mimesz; i++)
		if (sz == strlen(pp->mimes[i]) &&
		    0 == strncasecmp(pp->mimes[i], ctype, sz))
			break;

	return(i);
}

/*
 * Given a parsed field "key" with value "val" of size "valsz" and MIME
 * information "mime", first try to look it up in the array of
 * recognised keys ("pp->keys") and optionally validate.
 * Then output the type, parse status (key, type, etc.), and values read
 * by the parent input() function.
 */
static void
output(const struct parms *pp, char *key, 
	char *val, size_t valsz, struct mime *mime)
{
	size_t	 	 i;
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
	pair.type = KPAIR__MAX;

	/*
	 * Look up the key name in our key table.
	 * If we find it and it has a validator, then run the validator
	 * and record the output.
	 * If we fail, reset the type and clear the results.
	 * Either way, the keypos parameter is going to be the key
	 * identifier or keysz if none is found.
	 */

	for (i = 0; i < pp->keysz; i++) {
		if (strcmp(pp->keys[i].name, pair.key)) 
			continue;
		if (NULL == pp->keys[i].valid) 
			break;
		if ( ! pp->keys[i].valid(&pair)) {
			pair.state = KPAIR_INVALID;
			pair.type = KPAIR__MAX;
			memset(&pair.parsed, 0, sizeof(union parsed));
		} else
			pair.state = KPAIR_VALID;
		break;
	}
	pair.keypos = i;

	fullwrite(pp->fd, &pp->type, sizeof(enum input));
	fullwriteword(pp->fd, pair.key);
	fullwrite(pp->fd, &pair.valsz, sizeof(size_t));
	fullwrite(pp->fd, pair.val, pair.valsz);
	fullwrite(pp->fd, &pair.state, sizeof(enum kpairstate));
	fullwrite(pp->fd, &pair.type, sizeof(enum kpairtype));
	fullwrite(pp->fd, &pair.keypos, sizeof(size_t));

	if (KPAIR_VALID == pair.state) 
		switch (pair.type) {
		case (KPAIR_DOUBLE):
			fullwrite(pp->fd, 
				&pair.parsed.d, sizeof(double));
			break;
		case (KPAIR_INTEGER):
			fullwrite(pp->fd, 
				&pair.parsed.i, sizeof(int64_t));
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

	fullwriteword(pp->fd, pair.file);
	fullwriteword(pp->fd, pair.ctype);
	fullwrite(pp->fd, &pair.ctypepos, sizeof(size_t));
	fullwriteword(pp->fd, pair.xcode);

	/*
	 * We can write a new "val" in the validator allocated on the
	 * heap: if we do, free it here.
	 */

	if (save != pair.val)
		free(pair.val);
}

/*
 * Read full stdin request into memory.
 * This reads at most "len" bytes and NUL-terminates the results, the
 * length of which may be less than "len" and is stored in *szp if not
 * NULL.
 * Returns the pointer to the data.
 * NOTE: we can't use fullread() here because we may not get the total
 * number of bytes requested.
 * NOTE: "szp" can legit be set to zero.
 */
static char *
scanbuf(size_t len, size_t *szp)
{
	ssize_t		 ssz;
	size_t		 sz;
	char		*p;
	int		 rc;
	struct pollfd	 pfd;

	pfd.fd = STDIN_FILENO;
	pfd.events = POLLIN;

	/* Allocate the entire buffer here. */

	if ((p = kxmalloc(len + 1)) == NULL)
		_exit(EXIT_FAILURE);

	/* 
	 * Keep reading til we get all the data or the sender stops
	 * giving us data---whichever comes first.
	 * Use kutil_warn[x] and _exit to avoid flushing buffers.
	 */

	for (sz = 0; sz < len; sz += (size_t)ssz) {
		if ((rc = poll(&pfd, 1, INFTIM)) < 0) {
			kutil_warn(NULL, NULL, "poll");
			_exit(EXIT_FAILURE);
		} else if (0 == rc) {
			kutil_warnx(NULL, NULL, "poll: timeout!?");
			ssz = 0;
			continue;
		}
		
		if (!(pfd.revents & POLLIN))
			break;

		if ((ssz = read(STDIN_FILENO, p + sz, len - sz)) < 0) {
			kutil_warn(NULL, NULL, "read");
			_exit(EXIT_FAILURE);
		} else if (ssz == 0)
			break;
	}

	if (sz < len)
		kutil_warnx(NULL, NULL, "content size mismatch: "
			"have %zu while %zu specified", sz, len);

	/* ALWAYS NUL-terminate. */

	p[sz] = '\0';

	if (szp != NULL)
		*szp = sz;

	return p;
}

/*
 * Reset a particular mime component.
 * We can get duplicates, so reallocate.
 */
static void
mime_reset(char **dst, const char *src)
{

	free(*dst);
	if ((*dst = kxstrdup(src)) == NULL)
		_exit(EXIT_FAILURE);
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
	char		*key, *val, *keyend, *end, *start, *line;
	enum mimetype	 type;
	int	 	 rc = 0;

	mime_free(mime);

	while (*pos < len) {
		/* Each MIME line ends with a CRLF. */

		start = &buf[*pos];
		end = memmem(start, len - *pos, "\r\n", 2);
		if (end == NULL) {
			kutil_warnx(NULL, NULL, "RFC error: "
				"MIME header line without CRLF");
			return 0;
		}

		/* 
		 * NUL-terminate to make a nice line.
		 * Then re-set our starting position. 
		 */

		*end = '\0';
		*pos += (end - start) + 2;

		/* Empty CRLF line: we're done here! */

		if (*start == '\0') {
			rc = 1;
			break;
		}

		/* 
		 * Find end of MIME statement name. 
		 * The RFCs disagree on white-space before the colon,
		 * but as it's allowed in the original RFC 822 and
		 * obsolete syntax should be supported, we do so here.
		 */

		key = start;
		if ((val = strchr(key, ':')) == NULL) {
			kutil_warnx(NULL, NULL, "RFC error: "
				"MIME header without colon separator");
			return 0;
		} else if (key != val) {
			keyend = val - 1;
			while (keyend >= key && *keyend == ' ')
				*keyend-- = '\0';
		}

		*val++ = '\0';
		while (*val == ' ')
			val++;

		if (*key == '\0') 
			kutil_warnx(NULL, NULL, "RFC "
				"warning: empty MIME header name");

		/* 
		 * Set "line" to be at the MIME value subpart, for
		 * example, "Content-type: text/plain; charset=us-ascii"
		 * would put us at the parts before "charset".
		 */

		line = NULL;
		if ((line = strchr(val, ';')) != NULL)
			*line++ = '\0';

		/* 
		 * Allow these specific MIME header statements.
		 * We'll follow up by parsing specific information from
		 * the header values, so remember what we parsed.
		 */

		if (strcasecmp(key, "content-transfer-encoding") == 0) {
			mime_reset(&mime->xcode, val);
			type = MIMETYPE_TRANSFER_ENCODING;
		} else if (strcasecmp(key, "content-disposition") == 0) {
			mime_reset(&mime->disp, val);
			type = MIMETYPE_DISPOSITION;
		} else if (strcasecmp(key, "content-type") == 0) {
			mime_reset(&mime->ctype, val);
			type = MIMETYPE_TYPE;
		} else
			type = MIMETYPE_UNKNOWN;

		/* 
		 * Process subpart only for content-type and
		 * content-disposition.
		 * The rest have no information we want: silently ignore them.
		 */

		if (type != MIMETYPE_TYPE &&
		    type != MIMETYPE_DISPOSITION)
			continue;

		while ((key = line) != NULL) {
			while (*key == ' ')
				key++;
			if (*key == '\0')
				break;

			/*
			 * It's not clear whether we're allowed to have
			 * OWS before the separator, but allow for it
			 * anyway.
			 */

			if ((val = strchr(key, '=')) == NULL) {
				kutil_warnx(NULL, NULL, "RFC error: "
					"MIME header without sub-part "
					"separator");
				return 0;
			} else if (key != val) {
				keyend = val - 1;
				while (keyend >= key && *keyend == ' ')
					*keyend-- = '\0';
			} 

			*val++ = '\0';

			if (*key == '\0')
				kutil_warnx(NULL, NULL, "RFC warning: "
					"empty MIME sub-part name");

			/* Quoted string. */

			if (*val == '"') {
				val++;
				line = strchr(val, '"');
				if (line == NULL) {
					kutil_warnx(NULL, NULL, "RFC "
						"error: quoted MIME "
						"header sub-part not "
						"terminated");
					return 0;
				}
				*line++ = '\0';

				/* 
				 * It's unclear as to whether this is
				 * allowed (white-space before the
				 * semicolon separator), but let's
				 * accommodate for it anyway.
				 */

				while (*line == ' ')
					line++;
				if (*line == ';')
					line++;
			} else if ((line = strchr(val, ';')) != NULL)
				*line++ = '\0';

			/* White-listed sub-commands. */

			if (type == MIMETYPE_DISPOSITION) {
				if (strcasecmp(key, "filename") == 0)
					mime_reset(&mime->file, val);
				else if (strcasecmp(key, "name") == 0)
					mime_reset(&mime->name, val);
			} else if (type == MIMETYPE_TYPE) {
				if (strcasecmp(key, "boundary") == 0)
					mime_reset(&mime->bound, val);
			}
		}
	} 

	mime->ctypepos = str2ctype(pp, mime->ctype);

	if (!rc)
		kutil_warnx(NULL, NULL, "RFC error: unexpected "
			"end of file while parsing MIME headers");

	return rc;
}

/*
 * Parse keys and values separated by newlines.
 * I'm not aware of any standard that defines this, but the W3
 * guidelines for HTML give a rough idea.
 * FIXME: deprecate this.
 */
static void
parse_pairs_text(const struct parms *pp, char *p)
{
	char	*key, *val;

	kutil_warnx(NULL, NULL, "RFC warning: "
		"text/plain encoding is deprecated");

	while (p != NULL && *p != '\0') {
		while (*p == ' ')
			p++;

		/* 
		 * Key/value pair.
		 * No value is a warning (not processed).
		 */

		key = p;
		val = NULL;
		if (NULL != (p = strchr(p, '='))) {
			*p++ = '\0';
			val = p;
			if ((p = strstr(val, "\r\n")) != NULL) {
				*p = '\0';
				p += 2;
			}
		} else {
			if ((p = strstr(key, "\r\n")) != NULL) {
				*p = '\0';
				p += 2;
			}
			kutil_warnx(NULL, NULL, "RFC warning: "
				"key with no value");
			continue;
		}

		if (*key == '\0')
			kutil_warnx(NULL, NULL, "RFC warning: "
				"zero-length key");
		else
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

	if ((mime.ctype = kxstrdup(ct)) == NULL)
		_exit(EXIT_FAILURE);
	mime.ctypepos = str2ctype(pp, mime.ctype);

	name = '\0';
	output(pp, &name, b, bsz, &mime);
	free(mime.ctype);
}

/*
 * Parse out key-value pairs from an HTTP cookie.
 * These are not URL encoded (at this phase): they're just simple
 * key-values "crumbs" with opaque values.
 * This is defined by RFC 6265, however, we don't [yet] do the
 * quoted-string implementation, nor do we check for accepted
 * characters so long as the delimiters aren't used.
 */
static void
parse_pairs(const struct parms *pp, char *p)
{
	char	*key, *val;

	while (p != NULL && *p != '\0') {
		while (*p == ' ')
			p++;

		/* 
		 * Don't allow key-pair without a value.
		 * Keys shouldn't be zero-length.
		 */

		key = p;
		val = NULL;
		if ((p = strchr(p, '=')) != NULL) {
			*p++ = '\0';
			val = p;
			if ((p = strchr(p, ';')) != NULL)
				*p++ = '\0';
		} else {
			if ((p = strchr(key, ';')) != NULL)
				p++;
			kutil_warnx(NULL, NULL, "RFC error: "
				"cookie key pair without value");
			continue;
		}

		/* This is sort-of allowed. */

		if (*key == '\0')
			kutil_warnx(NULL, NULL, "RFC warning: "
				"cookie zero-length key");
		else
			output(pp, key, val, strlen(val), NULL);
	}
}

/*
 * Parse out key-value pairs from an HTTP request variable.
 * This is either a POST or GET string.
 * This MUST be a non-binary (i.e., NUL-terminated) string!
 */
static void
parse_pairs_urlenc(const struct parms *pp, char *p)
{
	char	*key, *val;

	assert(p != NULL);

	while (*p != '\0') {
		while (*p == ' ')
			p++;

		key = p;

		/* 
		 * Look ahead to either '=' or one of the key-value
		 * terminators (or the end of the string).
		 * If we have the equal sign, then we're a key-value
		 * pair; otherwise, we're a standalone key value.
		 */

		p += strcspn(p, "=;&");

		if (*p == '=') {
			*p++ = '\0';
			val = p;
			p += strcspn(p, ";&");
		} else
			val = p;

		if (*p != '\0')
			*p++ = '\0';

		/*
		 * Both the key and the value can be URL encoded, so
		 * decode those into the character string now.
		 * If decoding fails, don't decode the given pair, but
		 * instead move on to the next one after logging the
		 * failure.
		 */

		if (*key == '\0')
			kutil_warnx(NULL, NULL, "RFC warning: "
				"zero-length URL-encoded key");
		else if (khttp_urldecode_inplace(key) == KCGI_FORM)
			kutil_warnx(NULL, NULL, "RFC warning: "
				"malformed key URL-encoding");
		else if (khttp_urldecode_inplace(val) == KCGI_FORM)
			kutil_warnx(NULL, NULL, "RFC warning: "
				"malformed value URL-encoding");
		else
			output(pp, key, val, strlen(val), NULL);
	}
}

/*
 * This is described by the "multipart-body" BNF part of RFC 2046,
 * section 5.1.1.
 * We return TRUE if the parse was ok, FALSE if errors occurred (all
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

	/* Define our buffer boundary. */
	
	if ((rc = kxasprintf(&bb, "\r\n--%s", bound)) == -1)
		_exit(EXIT_FAILURE);

	assert(rc > 0);
	bbsz = rc;
	rc = 0;

	memset(&mime, 0, sizeof(struct mime));

	/* Read to the next instance of a buffer boundary. */

	for (first = 1; *pos < len; first = 0, *pos = endpos) {
		/*
		 * The (first ? 2 : 0) is because the first prologue
		 * boundary will not incur an initial CRLF, so our bb is
		 * past the CRLF and two bytes smaller.
		 */

		ln = memmem(&buf[*pos], len - *pos, 
			bb + (first ? 2 : 0), 
			bbsz - (first ? 2 : 0));

		if (ln == NULL) {
			kutil_warnx(NULL, NULL, "RFC error: "
				"EOF when scanning for boundary");
			goto out;
		}

		/* 
		 * Set "endpos" to point to the beginning of the next
		 * multipart component, i.e, the end of the boundary
		 * "bb" string.
		 * Again, be respectful of whether we should scan after
		 * the lack of initial CRLF.
		 */

		endpos = *pos + (ln - &buf[*pos]) + 
			bbsz - (first ? 2 : 0);

		/* Check buffer space. */

		if (endpos > len - 2) {
			kutil_warnx(NULL, NULL, "RFC error: multipart "
				"section writes into trailing CRLF");
			goto out;
		}

		/* 
		 * Terminating boundary has an initial trailing "--".
		 * If not terminating, must be followed by a CRLF.
		 * If terminating, RFC 1341 says we can ignore whatever
		 * comes after the last boundary.
		 */

		if (memcmp(&buf[endpos], "--", 2)) {
			while (endpos < len && buf[endpos] == ' ')
				endpos++;
			if (endpos > len - 2 ||
			    memcmp(&buf[endpos], "\r\n", 2)) {
				kutil_warnx(NULL, NULL, "RFC error: "
					"multipart boundary without "
					"CRLF");
				goto out;
			}
			endpos += 2;
		} else
			endpos = len;

		/* First section: jump directly to reprocess. */

		if (first)
			continue;

		/* 
		 * Zero-length part.
		 * This shouldn't occur, but if it does, it'll screw up
		 * the MIME parsing (which requires a blank CRLF before
		 * considering itself finished).
		 */

		if ((partsz = ln - &buf[*pos]) == 0) {
			kutil_warnx(NULL, NULL, "RFC error: "
				"zero-length multipart section");
			continue;
		}

		/* We now read our MIME headers, bailing on error. */

		if (!mime_parse(pp, &mime, buf, *pos + partsz, pos)) {
			kutil_warnx(NULL, NULL, "RFC error: "
				"nested error parsing MIME headers");
			goto out;
		}

		/* 
		 * As per RFC 2388, we need a name and disposition. 
		 * Note that multipart/mixed bodies will inherit the
		 * name of their parent, so the mime.name is ignored.
		 */

		if (mime.name == NULL && name == NULL) {
			kutil_warnx(NULL, NULL, 
				"RFC error: no MIME name");
			continue;
		} else if (mime.disp == NULL) {
			kutil_warnx(NULL, NULL, 
				"RFC error: no MIME disposition");
			continue;
		}

		/* 
		 * As per RFC 2045, we default to text/plain.
		 * We then re-lookup the ctypepos after doing so.
		 */

		if (mime.ctype == NULL) {
			mime.ctype = kxstrdup("text/plain");
			if (mime.ctype == NULL)
				_exit(EXIT_FAILURE);
			mime.ctypepos = str2ctype(pp, mime.ctype);
		}

		partsz = ln - &buf[*pos];

		/* 
		 * Multipart sub-handler. 
		 * We only recognise the multipart/mixed handler.
		 * This will route into our own function, inheriting the
		 * current name for content.
		 */

		if (strcasecmp(mime.ctype, "multipart/mixed") == 0) {
			if (mime.bound == NULL) {
				kutil_warnx(NULL, NULL, "RFC error: "
					"no mixed multipart boundary");
				goto out;
			}
			if (!parse_multiform(pp, 
			    name != NULL ? name : mime.name, 
			    mime.bound, buf, *pos + partsz, pos)) {
				kutil_warnx(NULL, NULL, "RFC error: "
					"nested error parsing mixed "
					"multipart section");
				goto out;
			}
			continue;
		}

		assert(buf[*pos + partsz] == '\r' || 
		       buf[*pos + partsz] == '\0');

		if (buf[*pos + partsz] != '\0')
			buf[*pos + partsz] = '\0';

		/* Assign all of our key-value pair data. */

		output(pp, name != NULL ? name : mime.name, 
			&buf[*pos], partsz, &mime);
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
	return rc;
}

/*
 * Parse the boundary from a multipart CONTENT_TYPE and pass it to the
 * actual parsing engine.
 * This doesn't actually handle any part of the MIME specification.
 */
static void
parse_multi(const struct parms *pp, char *line, char *b, size_t bsz)
{
	char		*cp;
	size_t		 len = 0;

	while (*line == ' ')
		line++;

	if (*line++ != ';') {
		kutil_warnx(NULL, NULL, "RFC error: expected "
			"semicolon following multipart declaration");
		return;
	}

	while (*line == ' ')
		line++;

	/* We absolutely need the boundary marker. */

	if (strncmp(line, "boundary", 8)) {
		kutil_warnx(NULL, NULL, "RFC error: expected "
			"boundary following multipart declaration");
		return;
	}

	line += 8;

	while (*line == ' ')
		line++;

	if (*line++ != '=') {
		kutil_warnx(NULL, NULL, "RFC error: expected "
			"key-value for multipart boundary");
		return;
	}

	while (*line == ' ')
		line++;

	/* 
	 * Make sure the line is terminated in the right place.
	 * XXX: if it's not, what we do may not properly follow RFC
	 * 2046, 5.1.1, which specifically lays out the boundary
	 * characters.
	 * We simply jump to the first whitespace.
	 */

	if (*line == '"') {
		if ((cp = strchr(++line, '"')) == NULL) {
			kutil_warnx(NULL, NULL, "RFC error: "
				"unterminated boundary quoted string");
			return;
		}
		*cp = '\0';
	} else
		line[strcspn(line, " ")] = '\0';

	/*
	 * If we have data following the boundary declaration, we simply
	 * ignore it.
	 * The RFC mandates the existence of the boundary, but is silent
	 * as to whether anything can come after it.
	 */

	parse_multiform(pp, NULL, line, b, bsz, &len);
}

/*
 * Output all of the HTTP_xxx headers.
 * This transforms the HTTP_xxx header (CGI form) into HTTP form, which
 * is the second part title-cased, e.g., HTTP_FOO = Foo.
 * Disallow zero-length values as per RFC 3875, 4.1.18.
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
		if (strncmp(env[i].key, "HTTP_", 5) == 0 &&
		    env[i].key[5] != '\0')
			reqs++;

	fullwrite(fd, &reqs, sizeof(size_t));

	/*
	 * Process known headers (starting with HTTP_).
	 * We must have non-zero-length keys.
	 */

	for (i = 0; i < envsz; i++) {
		if (strncmp(env[i].key, "HTTP_", 5) || 
		    env[i].key[5] == '\0')
			continue;

		for (requ = 0; requ < KREQU__MAX; requ++)
			if (strcmp(krequs[requ], env[i].key) == 0)
				break;

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
			if (cp[j] == '_') {
				c = '-';
				first = 1;
			} else if (first) {
				c = cp[j];
				first = 0;
			} else
				c = tolower((unsigned char)cp[j]);

			fullwrite(fd, &c, 1);
		}

		fullwrite(fd, &env[i].valsz, sizeof(size_t));
		fullwrite(fd, env[i].val, env[i].valsz);
	}
}

/*
 * Like getenv() but for our env structure.
 */
static char *
kworker_env(struct env *env, size_t envsz, const char *key)
{
	size_t	 i;

	for (i = 0; i < envsz; i++) 
		if (strcmp(env[i].key, key) == 0)
			return env[i].val;
	return NULL;
}

/*
 * Output the method found in our environment.
 * Returns the method.
 * Defaults to KMETHOD_GET, uses KETHOD__MAX if the method was bad.
 */
static enum kmethod
kworker_child_method(struct env *env, int fd, size_t envsz)
{
	enum kmethod	 meth;
	const char	*cp;

	/* RFC 3875, 4.1.12. */
	/* We assume GET if not supplied. */

	meth = KMETHOD_GET;
	if ((cp = kworker_env(env, envsz, "REQUEST_METHOD")) != NULL)
		for (meth = 0; meth < KMETHOD__MAX; meth++)
			if (strcmp(kmethods[meth], cp) == 0)
				break;

	fullwrite(fd, &meth, sizeof(enum kmethod));
	return meth;
}

/*
 * Output the web server's authentication.
 * Defaults to KAUTH_NONE.
 */
static void
kworker_child_auth(struct env *env, int fd, size_t envsz)
{
	enum kauth	 auth = KAUTH_NONE;
	const char	*cp;	

	/* Determine authentication: RFC 3875, 4.1.1. */

	if ((cp = kworker_env(env, envsz, "AUTH_TYPE")) != NULL)
		for (auth = 0; auth < KAUTH_UNKNOWN; auth++) {
			if (kauths[auth] == NULL)
				continue;
			if (strcmp(kauths[auth], cp) == 0)
				break;
		}

	fullwrite(fd, &auth, sizeof(enum kauth));
}

/*
 * Send the raw (i.e., un-webserver-filtered) authorisation to the
 * parent.
 * Most web servers will `handle this for us'.  Ugh.
 */
static int
kworker_child_rawauth(struct env *env, int fd, size_t envsz)
{

	return kworker_auth_child(fd, 
	  	kworker_env(env, envsz, "HTTP_AUTHORIZATION"));
}

/*
 * Send our HTTP scheme (secure or not) to the parent.
 */
static void
kworker_child_scheme(struct env *env, int fd, size_t envsz)
{
	const char	*cp;
	enum kscheme	 scheme;

	/* 
	 * This isn't defined in any RFC.
	 * It seems to be the best way of getting whether we're HTTPS,
	 * as the SERVER_PROTOCOL (RFC 3875, 4.1.16) doesn't reliably
	 * return the scheme.
	 */

	if ((cp = kworker_env(env, envsz, "HTTPS")) == NULL)
		cp = "off";

	scheme = strcasecmp(cp, "on") == 0 ?
		KSCHEME_HTTPS : KSCHEME_HTTP;
	fullwrite(fd, &scheme, sizeof(enum kscheme));
}

/*
 * Send remote address to the parent.
 * This is required by RFC 3875, 4.1.8.
 * Use 127.0.0.1 on protocol violation.
 */
static void
kworker_child_remote(struct env *env, int fd, size_t envsz)
{
	const char	*cp;

	if ((cp = kworker_env(env, envsz, "REMOTE_ADDR")) == NULL) {
		kutil_warnx(NULL, NULL, "RFC warning: "
			"remote address not set");
		cp = "127.0.0.1";
	}

	fullwriteword(fd, cp);
}

/*
 * Parse and send the port to the parent.
 * This is required by RFC 3875, 4.1.15.
 * Use port 80 if not provided or on parse error.
 */
static void
kworker_child_port(struct env *env, int fd, size_t envsz)
{
	uint16_t	 port = 80;
	const char	*cp, *er;

	if ((cp = kworker_env(env, envsz, "SERVER_PORT")) != NULL) {
		port = strtonum(cp, 0, UINT16_MAX, &er);
		if (er != NULL) {
			kutil_warnx(NULL, NULL, "RFC warning: "
				"invalid server port value");
			port = 80;
		}
	} else
		kutil_warnx(NULL, NULL, "RFC warning: "
			"server port not set");

	fullwrite(fd, &port, sizeof(uint16_t));
}

/*
 * Send requested host to the parent.
 * This is required by RFC 7230, 5.4.
 * Use "localhost" if not provided.
 */
static void
kworker_child_httphost(struct env *env, int fd, size_t envsz)
{
	const char	*cp;

	if ((cp = kworker_env(env, envsz, "HTTP_HOST")) == NULL) {
		kutil_warnx(NULL, NULL, "RFC warning: host not set");
		cp = "localhost";
	}

	fullwriteword(fd, cp);
}

/* 
 * Send script name to the parent.
 * This is required by RFC 3875, 4.1.13.
 * Use the empty string on error.
 */
static void
kworker_child_scriptname(struct env *env, int fd, size_t envsz)
{
	const char	*cp;

	if ((cp = kworker_env(env, envsz, "SCRIPT_NAME")) == NULL) {
		kutil_warnx(NULL, NULL, "RFC warning: "
			"script name not set");
		cp = "";
	}

	fullwriteword(fd, cp);
}

/*
 * Parse all path information (subpath, path, etc.) and send to parent.
 */
static void
kworker_child_path(struct env *env, int fd, size_t envsz)
{
	char	*cp, *ep, *sub;
	size_t	 len;

	/*
	 * Parse the first path element (the page we want to access),
	 * subsequent path information, and the file suffix.  We convert
	 * suffix and path element into the respective enum's inline.
	 */

	cp = kworker_env(env, envsz, "PATH_INFO");
	fullwriteword(fd, cp);

	/* This isn't possible in the real world. */

	if (cp != NULL && *cp == '/')
		cp++;

	if (cp != NULL && *cp != '\0') {
		ep = cp + strlen(cp) - 1;
		while (ep > cp && *ep != '/' && *ep != '.')
			ep--;

		/* Start with writing our suffix. */

		if (*ep == '.') {
			*ep++ = '\0';
			fullwriteword(fd, ep);
		} else
			fullwriteword(fd, NULL);

		/* Now find the top-most path part. */

		if ((sub = strchr(cp, '/')) != NULL)
			*sub++ = '\0';

		/* Send the base path. */

		fullwriteword(fd, cp);

		/* Send the path part. */

		fullwriteword(fd, sub);
	} else {
		len = 0;

		/* Suffix, base path, and path part. */

		fullwrite(fd, &len, sizeof(size_t));
		fullwrite(fd, &len, sizeof(size_t));
		fullwrite(fd, &len, sizeof(size_t));
	}
}

/*
 * Construct the body hash component of an HTTP digest hash.
 * See khttpdigest_validatehash(3) for where this is used.
 * See RFC 2617.
 * We only do this if our authorisation requires it!
 */
static void
kworker_child_bodymd5(int fd, const char *b, size_t bsz, int md5)
{
	MD5_CTX		 ctx;
	unsigned char 	 hab[MD5_DIGEST_LENGTH];
	size_t		 sz;

	if (!md5) {
		sz = 0;
		fullwrite(fd, &sz, sizeof(size_t));
		return;
	}

	MD5Init(&ctx);
	MD5Updatec(&ctx, b, bsz);
	MD5Final(hab, &ctx);

	/* This is a binary write! */

	sz = MD5_DIGEST_LENGTH;
	fullwrite(fd, &sz, sizeof(size_t));
	fullwrite(fd, hab, sz);
}

/*
 * Parse and send the body of the request to the parent.
 * This is arguably the most complex part of the system.
 */
static void
kworker_child_body(struct env *env, int fd, size_t envsz,
	struct parms *pp, enum kmethod meth, char *b, 
	size_t bsz, unsigned int debugging, int md5)
{
	size_t		 i, len = 0, sz;
	char		*cp, *bp = b;
	const char	*end;
	int		 wrap;

	/*
	 * The CONTENT_LENGTH must be a valid integer.
	 * Since we're storing into "len", make sure it's in size_t.
	 * If there's an error, it will default to zero.
	 * Note that LLONG_MAX < SIZE_MAX.
	 * RFC 3875, 4.1.2.
	 */

	if ((cp = kworker_env(env, envsz, "CONTENT_LENGTH")) != NULL)
		len = strtonum(cp, 0, LLONG_MAX, NULL);

	/* If zero, remember to print our MD5 value. */

	if (len == 0) {
		kworker_child_bodymd5(fd, "", 0, md5);
		return;
	}

	/* Check FastCGI input lengths. */

	if (bp != NULL && bsz != len)
		kutil_warnx(NULL, NULL, "RFC warning: real (%zu) "
			"and reported (%zu) content lengths differ", 
			bsz, len);

	/*
	 * If a CONTENT_TYPE has been specified (i.e., POST or GET has
	 * been set -- we don't care which), then switch on that type
	 * for parsing out key value pairs.
	 * RFC 3875, 4.1.3.
	 * HTML5, 4.10.
	 * We only support the main three content types.
	 */

	pp->type = IN_FORM;
	cp = kworker_env(env, envsz, "CONTENT_TYPE");

	/* 
	 * If we're CGI, read the request now.
	 * Note that the "bsz" can come out as zero.
	 */

	if (b == NULL)
		b = scanbuf(len, &bsz);

	assert(b != NULL);

	/* If requested, print our MD5 value. */

	kworker_child_bodymd5(fd, b, bsz, md5);

	/*
	 * If we're debugging read bodies, emit the body line by line
	 * (or split at the 80-character mark).
	 */

	if (bsz && (debugging & KREQ_DEBUG_READ_BODY)) {
		i = 0;
		do {
			if ((end = memchr(&b[i], '\n', bsz - i)) == NULL)
				sz = bsz - i;
			else
				sz = (size_t)(end - &b[i]);
			if ((wrap = sz > 80))
				sz = 80;
			kutil_info(NULL, NULL, "%lu-rx: %.*s%s",
				(unsigned long)getpid(), (int)sz, 
				&b[i], wrap ? "..." : "");

			i += wrap ? sz : sz + 1;
		} while (i < bsz);
		kutil_info(NULL, NULL, "%lu-rx: %zu B",
			(unsigned long)getpid(), bsz);
	}

	if (cp != NULL) {
		if (strcasecmp(cp, "application/x-www-form-urlencoded") == 0)
			parse_pairs_urlenc(pp, b);
		else if (strncasecmp(cp, "multipart/form-data", 19) == 0) 
			parse_multi(pp, cp + 19, b, bsz);
		else if (meth == KMETHOD_POST && strcasecmp(cp, "text/plain") == 0)
			parse_pairs_text(pp, b);
		else
			parse_body(cp, pp, b, bsz);
	} else
		parse_body(kmimetypes[KMIME_APP_OCTET_STREAM], pp, b, bsz);

	/* Free CGI parsed buffer (FastCGI is done elsewhere). */

	if (bp == NULL)
		free(b);
}

/*
 * Send query string data to parent.
 * Even POST requests are allowed to have QUERY_STRING elements.
 * Note: both QUERY_STRING and CONTENT_TYPE fields share the same field
 * space.
 */
static void
kworker_child_query(struct env *env, 
	int fd, size_t envsz, struct parms *pp)
{
	char 	*cp;

	pp->type = IN_QUERY;
	if (NULL != (cp = kworker_env(env, envsz, "QUERY_STRING")))
		parse_pairs_urlenc(pp, cp);
}

/*
 * Send cookies to our parent.
 * These use the same syntax as QUERY_STRING elements, but don't share
 * the same namespace (just as a means to differentiate the same names).
 */
static void
kworker_child_cookies(struct env *env, 
	int fd, size_t envsz, struct parms *pp)
{
	char	*cp;

	pp->type = IN_COOKIE;
	if ((cp = kworker_env(env, envsz, "HTTP_COOKIE")) != NULL)
		parse_pairs(pp, cp);
}

/*
 * Terminate the input fields for the parent. 
 */
static void
kworker_child_last(int fd)
{
	enum input last = IN__MAX;

	fullwrite(fd, &last, sizeof(enum input));
}

/*
 * This is the child kcgi process that's going to do the unsafe reading
 * of network data to parse input.
 * When it parses a field, it outputs the key, key size, value, and
 * value size along with the field type.
 * We use the CGI specification in RFC 3875.
 */
enum kcgi_err
kworker_child(int wfd,
	const struct kvalid *keys, size_t keysz, 
	const char *const *mimes, size_t mimesz,
	unsigned int debugging)
{
	struct parms	  pp;
	char		 *cp;
	const char	 *start;
	char		**evp;
	int		  md5;
	enum kmethod	  meth;
	size_t	 	  i;
	extern char	**environ;
	struct env	 *envs = NULL;
	size_t		  envsz;

	pp.fd = wfd;
	pp.keys = keys;
	pp.keysz = keysz;
	pp.mimes = mimes;
	pp.mimesz = mimesz;

	/*
	 * Pull the entire environment into an array.
	 */
	for (envsz = 0, evp = environ; NULL != *evp; evp++) 
		envsz++;

	if (envsz) {
		envs = kxcalloc(envsz, sizeof(struct env));
		if (envs == NULL)
			return KCGI_ENOMEM;
	}

	/* 
	 * Pull all reasonable values from the environment into "envs".
	 * Filter out variables that don't meet RFC 3875, section 4.1.
	 * However, we're a bit more relaxed: we don't let through
	 * zero-length, non-ASCII, control characters, and whitespace.
	 */

	for (i = 0, evp = environ; *evp != NULL; evp++) {
		if ((cp = strchr(*evp, '=')) == NULL || cp == *evp)
			continue;
		for (start = *evp; *start != '='; start++)
			if (!isascii((unsigned char)*start) ||
			    !isgraph((unsigned char)*start))
				break;

		/* 
		 * This means something is seriously wrong, so make sure
		 * that the operator knows.
		 */

		if (*start != '=') {
			kutil_warnx(NULL, NULL, "RFC warning: "
				"bad character in environment pair");
			continue;
		}

		assert(i < envsz);

		if ((envs[i].key = kxstrdup(*evp)) == NULL)
			_exit(EXIT_FAILURE);
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
	 * environment.
	 */

	kworker_child_env(envs, wfd, envsz);
	meth = kworker_child_method(envs, wfd, envsz);
	kworker_child_auth(envs, wfd, envsz);
	md5 = kworker_child_rawauth(envs, wfd, envsz);
	kworker_child_scheme(envs, wfd, envsz);
	kworker_child_remote(envs, wfd, envsz);
	kworker_child_path(envs, wfd, envsz);
	kworker_child_scriptname(envs, wfd, envsz);
	kworker_child_httphost(envs, wfd, envsz);
	kworker_child_port(envs, wfd, envsz);

	/* And now the message body itself. */

	kworker_child_body(envs, wfd, envsz, 
		&pp, meth, NULL, 0, debugging, md5);
	kworker_child_query(envs, wfd, envsz, &pp);
	kworker_child_cookies(envs, wfd, envsz, &pp);
	kworker_child_last(wfd);

	/* Note: the "val" is from within the key. */

	for (i = 0; i < envsz; i++) 
		free(envs[i].key);
	free(envs);
	return KCGI_OK;
}

/*
 * Reads from the FastCGI control process, kfcgi_control(), are buffered
 * according to what the control process can read from the web server.
 * Here we read ahead til we have enough data for what currently needs
 * to be read.
 * Returns a pointer to the data of size "sz" or NULL if errors occured.
 * If error is KCGI_OK, this *always* returns a buffer.
 * The error is reported in "er".
 */
static char *
kworker_fcgi_read(struct fcgi_buf *b, size_t nsz, enum kcgi_err *er)
{
	void	*pp;
	int	 rc;
	size_t	 sz;

again:
	*er = KCGI_OK;
	if (b->pos + nsz <= b->sz) {
		b->pos += nsz;
		return &b->buf[b->pos - nsz];
	}

	/* Fill up the next frame. */

	rc = fullread(b->fd, &sz, sizeof(size_t), 0, er);
	if (rc <= 0) {
		kutil_warnx(NULL, NULL, "FastCGI: "
			"error reading frame size from control");
		return NULL;
	} else if (sz == 0) {
		kutil_warnx(NULL, NULL, "FastCGI: connection "
			"closed while reading frame size");
		*er = KCGI_HUP;
		return NULL;
	} 

	if ((pp = kxrealloc(b->buf, b->sz + sz)) == NULL) {
		*er = KCGI_ENOMEM;
		return NULL;
	}

	b->buf = pp;
	rc = fullread(b->fd, b->buf + b->sz, sz, 0, er);
	if (rc <= 0) {
		kutil_warnx(NULL, NULL, "FastCGI: error "
			"reading frame data from control");
		return NULL;
	}

	b->sz += sz;
	goto again;
}


/*
 * Read the FastCGI header (see section 8, Types and Contents,
 * FCGI_Header, in the FastCGI Specification v1.0).
 * Return KCGI_OK on success, KCGI_HUP on connection close, KCGI_FORM
 * with FastCGI protocol errors, and a fatal error otherwise.
 */
static enum kcgi_err
kworker_fcgi_header(struct fcgi_buf *b, struct fcgi_hdr *hdr)
{
	enum kcgi_err	 er;
	const char	*cp;
	struct fcgi_hdr	 buf;

	if ((cp = kworker_fcgi_read(b, 8, &er)) == NULL)
		return er;

	memcpy(&buf, cp, 8);

	/* Translate from network-byte order. */

	hdr->version = buf.version;
	hdr->type = buf.type;
	hdr->requestId = ntohs(buf.requestId);
	hdr->contentLength = ntohs(buf.contentLength);
	hdr->paddingLength = buf.paddingLength;

	if (hdr->version == 1)
		return KCGI_OK;

	kutil_warnx(NULL, NULL, "FastCGI: bad header "
		"version: %" PRIu8 " (want 1)", hdr->version);
	return KCGI_FORM;
}

/*
 * Read in the entire header and data for the begin sequence request.
 * This is defined in section 5.1 of the v1.0 specification.
 * Return KCGI_OK on success, KCGI_HUP on connection close, KCGI_FORM
 * with FastCGI protocol errors, and a fatal error otherwise.
 */
static enum kcgi_err
kworker_fcgi_begin(struct fcgi_buf *b, uint16_t *rid)
{
	struct fcgi_hdr	 hdr;
	const struct fcgi_bgn *ptr;
	const char	*buf;
	enum kcgi_err	 er;

	/* Read the header entry. */

	if ((er = kworker_fcgi_header(b, &hdr)) != KCGI_OK)
		return er;

	*rid = hdr.requestId;

	if (hdr.type != FCGI_BEGIN_REQUEST) {
		kutil_warnx(NULL, NULL, "FastCGI: bad type: %" PRIu8 
			" (want %d)", hdr.type, FCGI_BEGIN_REQUEST);
		return KCGI_FORM;
	} 
	
	/* Read the "begin" content and discard padding. */

	buf = kworker_fcgi_read(b, 
		hdr.contentLength + 
		hdr.paddingLength, &er);

	ptr = (const struct fcgi_bgn *)buf;

	if (ptr->flags) {
		kutil_warnx(NULL, NULL, "FastCGI: bad flags: %" PRId8 
			" (want 0)", ptr->flags);
		return KCGI_FORM;
	}

	return KCGI_OK;
}

/*
 * Read in a data stream as defined within section 5.3 of the v1.0
 * specification.
 * We might have multiple stdin buffers for the same data, so always
 * append to the existing NUL-terminated buffer.
 * Return KCGI_OK on success, KCGI_HUP on connection close, KCGI_FORM
 * with FastCGI protocol errors, and a fatal error otherwise.
 */
static enum kcgi_err
kworker_fcgi_stdin(struct fcgi_buf *b, const struct fcgi_hdr *hdr,
	unsigned char **sbp, size_t *ssz)
{
	enum kcgi_err	 er;
	void		*ptr;
	char		*bp;

	/* Read the "begin" content and discard padding. */

	bp = kworker_fcgi_read(b, 
		hdr->contentLength + 
		hdr->paddingLength, &er);

	/* 
	 * Short-circuit: no data to read.
	 * The caller should detect this and stop reading from the
	 * FastCGI connection.
	 */

	if (hdr->contentLength == 0)
		return KCGI_OK;

	/* 
	 * Use another buffer for the stdin.
	 * This is because our buffer (b->buf) consists of FastCGI
	 * frames (data interspersed with control information).
	 * Obviously, we want to extract our data from that.
	 * Make sure it's NUL-terminated!
	 * FIXME: check for addition overflow.
	 */

	ptr = kxrealloc(*sbp, *ssz + hdr->contentLength + 1);
	if (ptr == NULL)
		return KCGI_ENOMEM;

	*sbp = ptr;
	memcpy(*sbp + *ssz, bp, hdr->contentLength);
	(*sbp)[*ssz + hdr->contentLength] = '\0';
	*ssz += hdr->contentLength;
	return KCGI_OK;
}

/*
 * Read out a series of parameters contained within a FastCGI parameter
 * request defined in section 5.2 of the v1.0 specification.
 * Return KCGI_OK on success, KCGI_HUP on connection close, KCGI_FORM
 * with FastCGI protocol errors, and a fatal error otherwise.
 */
static enum kcgi_err
kworker_fcgi_params(struct fcgi_buf *buf, const struct fcgi_hdr *hdr, 
	struct env **envs, size_t *envsz)
{
	size_t	 	 i, remain, pos, keysz, valsz;
	const unsigned char *b;
	enum kcgi_err	 er;
	void		*ptr;

	b = (unsigned char *)kworker_fcgi_read
		(buf, hdr->contentLength + 
		 hdr->paddingLength, &er);

	if (b == NULL)
		return er;

	/*
	 * Loop through the string data that's laid out as a key length
	 * then value length, then key, then value.
	 * There can be arbitrarily many key-values per string.
	 */

	remain = hdr->contentLength;
	pos = 0;

	while (remain > 0) {
		/* First read the lengths. */
		assert(pos < hdr->contentLength);
		if (0 != b[pos] >> 7) {
			if (remain <= 3) {
				kutil_warnx(NULL, NULL, 
					"FastCGI: bad parameter data");
				return 0;
			}
			keysz = ((b[pos] & 0x7f) << 24) + 
				  (b[pos + 1] << 16) + 
				  (b[pos + 2] << 8) + b[pos + 3];
			pos += 4;
			remain -= 4;
		} else {
			keysz = b[pos];
			pos++;
			remain--;
		}

		if (remain < 1) {
			kutil_warnx(NULL, NULL, 
				"FastCGI: bad parameter data");
			return KCGI_FORM;
		}

		assert(pos < hdr->contentLength);
		if (0 != b[pos] >> 7) {
			if (remain <= 3) {
				kutil_warnx(NULL, NULL, 
					"FastCGI: bad parameter data");
				return KCGI_FORM;
			}
			valsz = ((b[pos] & 0x7f) << 24) + 
				  (b[pos + 1] << 16) + 
				  (b[pos + 2] << 8) + b[pos + 3];
			pos += 4;
			remain -= 4;
		} else {
			valsz = b[pos];
			pos++;
			remain--;
		}

		/* Make sure we have room for data. */

		if (pos + keysz + valsz > hdr->contentLength) {
			kutil_warnx(NULL, NULL, 
				"FastCGI: bad parameter data");
			return KCGI_FORM;
		}

		remain -= keysz;
		remain -= valsz;

		/* 
		 * First, make sure that the key is valid.
		 * There's no documented precedent for this, so we
		 * follow CGI's constraints in RFC 3875, sec. 4.1.
		 * If it's not valid, just skip it.
		 */

		for (i = 0; i < keysz; i++)
			if (!isascii((unsigned char)b[pos + i]) ||
			    !isgraph((unsigned char)b[pos + i]))
				break;

		if (keysz == 0) {
			kutil_warnx(NULL, NULL, "FastCGI warning: "
				"empty environment parameter");
			pos += valsz;
			continue;
		} else if (i < keysz) {
			kutil_warnx(NULL, NULL, "RFC warning: bad "
				"character in environment parameters");
			pos += keysz + valsz;
			continue;
		}

		/* Look up the key in our existing keys. */

		for (i = 0; i < *envsz; i++) {
			if ((*envs)[i].keysz != keysz)
				continue;
			if (memcmp((*envs)[i].key, &b[pos], keysz) == 0)
				break;
		}

		/* 
		 * If we don't have the key: expand our table. 
		 * If we do, clear the current value.
		 */

		if (i == *envsz) {
			ptr = kxreallocarray
				(*envs, *envsz + 1, 
				 sizeof(struct env));
			if (ptr == NULL)
				return KCGI_ENOMEM;

			*envs = ptr;
			(*envs)[i].key = kxmalloc(keysz + 1);
			if ((*envs)[i].key == NULL)
				return KCGI_ENOMEM;

			memcpy((*envs)[i].key, &b[pos], keysz);
			(*envs)[i].key[keysz] = '\0';
			(*envs)[i].keysz = keysz;
			(*envsz)++;
		} else
			free((*envs)[i].val);

		pos += keysz;

		/* Copy the value. */

		(*envs)[i].val = kxmalloc(valsz + 1);
		if ((*envs)[i].val == NULL)
			return KCGI_ENOMEM;

		memcpy((*envs)[i].val, &b[pos], valsz);
		(*envs)[i].val[valsz] = '\0';
		(*envs)[i].valsz = valsz;

		pos += valsz;
	}

	return KCGI_OK;
}

/*
 * This is executed by the untrusted child for FastCGI setups.
 * Throughout, we follow the FastCGI specification, version 1.0, 29
 * April 1996.
 */
void
kworker_fcgi_child(int wfd, int work_ctl,
	const struct kvalid *keys, size_t keysz, 
	const char *const *mimes, size_t mimesz,
	unsigned int debugging)
{
	struct parms 	 pp;
	struct fcgi_hdr	 hdr;
	enum kcgi_err	 er;
	unsigned char	*sbuf = NULL;
	struct env	*envs = NULL;
	uint16_t	 rid;
	uint32_t	 cookie = 0;
	size_t		 i, ssz = 0, sz, envsz = 0;
	int		 rc, md5;
	enum kmethod	 meth;
	struct fcgi_buf	 fbuf;

	memset(&fbuf, 0, sizeof(struct fcgi_buf));

	pp.fd = wfd;
	pp.keys = keys;
	pp.keysz = keysz;
	pp.mimes = mimes;
	pp.mimesz = mimesz;

	/*
	 * Loop over all incoming sequences to this particular slave.
	 * Sequences must consist of a single FastCGI session as defined
	 * in the FastCGI version 1.0 reference document.
	 *
	 * If the connection closes out at any point, we receive a
	 * zero-length read from the control socket.
	 * The response to this should be to write an zero error code
	 * back to the control socket, then keep on listening.
	 * Otherwise, if we've read the full message, write a non-zero
	 * error code, then our identifier and cookie, then the rest
	 * goes directly to the parse routines in kworker_parent().
	 */

	for (;;) {
		free(sbuf);
		for (i = 0; i < envsz; i++) {
			free(envs[i].key);
			free(envs[i].val);
		}
		free(envs);
		free(fbuf.buf);

		sbuf = NULL;
		ssz = 0;
		envs = NULL;
		envsz = 0;
		cookie = 0;
		memset(&fbuf, 0, sizeof(struct fcgi_buf));
		fbuf.fd = work_ctl;

		/* 
		 * Begin by reading our magic cookie.
		 * This is emitted by kfcgi_control() at the start of
		 * our sequence.
		 * When we've finished reading data with success, we'll
		 * respond with this value.
		 */

		rc = fullread(fbuf.fd, 
			&cookie, sizeof(uint32_t), 1, &er);
		if (rc < 0) {
			kutil_warnx(NULL, NULL, "FastCGI: "
				"error reading worker cookie");
			break;
		} else if (rc == 0) {
			kutil_warnx(NULL, NULL, "FastCGI: "
				"worker process termination");
			break;
		}

		/* Now start the FastCGI sequence. */

		er = kworker_fcgi_begin(&fbuf, &rid);
		if (er == KCGI_HUP) {
			kutil_warnx(NULL, NULL, "FastCGI: "
				"connection severed at start");
			/* Note: writing error code... */
			rc = 0;
			fullwrite(work_ctl, &rc, sizeof(int));
			continue;
		} else if (er != KCGI_OK) {
			kutil_warnx(NULL, NULL, "FastCGI: "
				"unrecoverable error at start");
			break;
		}

		/*
		 * Now read one or more parameters.
		 * We read them all at once, then do the parsing later
		 * after we've read all of our data.
		 * We read parameters til we no longer have the
		 * FCGI_PARAMS type on the current header.
		 */

		er = KCGI_OK;
		envsz = 0;
		memset(&hdr, 0, sizeof(struct fcgi_hdr));

		while (er == KCGI_OK) {
			er = kworker_fcgi_header(&fbuf, &hdr);
			if (er != KCGI_OK)
				break;
			if (rid != hdr.requestId) {
				kutil_warnx(NULL, NULL, 
					"FastCGI: wrong request ID");
				er = KCGI_FORM;
				break;
			} 
			if (hdr.type != FCGI_PARAMS)
				break;
			er = kworker_fcgi_params
				(&fbuf, &hdr, &envs, &envsz);
		}

		if (er == KCGI_HUP) {
			kutil_warnx(NULL, NULL, "FastCGI: "
				"connection severed at parameters");
			/* Note: writing error code... */
			rc = 0;
			fullwrite(work_ctl, &rc, sizeof(int));
			continue;
		} else if (er != KCGI_OK) {
			kutil_warnx(NULL, NULL, "FastCGI: "
				"unrecoverable error at parameters");
			break;
		} else if (hdr.type != FCGI_STDIN) {
			kutil_warnx(NULL, NULL, "FastCGI: "
				"bad header type");
			er = KCGI_FORM;
			break;
		} else if (rid != hdr.requestId) {
			kutil_warnx(NULL, NULL, "FastCGI: "
				"wrong request ID");
			er = KCGI_FORM;
			break;
		}

		/*
		 * Lastly, we want to process the stdin content.
		 * These will end with a single zero-length record.
		 * Keep looping til we've flushed all input.
		 */

		for (;;) {
			/*
			 * Call this even if we have a zero-length data
			 * payload as specified by contentLength.
			 * This is because there might be padding, and
			 * we want to make sure we've drawn everything
			 * from the socket before exiting.
			 */

			er = kworker_fcgi_stdin
				(&fbuf, &hdr, &sbuf, &ssz);
			if (er != KCGI_OK || hdr.contentLength == 0)
				break;

			/* Now read the next header. */

			er = kworker_fcgi_header(&fbuf, &hdr);
			if (er != KCGI_OK)
				break;
			if (rid != hdr.requestId) {
				kutil_warnx(NULL, NULL, "FastCGI: "
					"wrong FastCGI request ID");
				er = KCGI_FORM;
				break;
			} 

			if (hdr.type == FCGI_STDIN)
				continue;
			kutil_warnx(NULL, NULL, 
				"FastCGI: bad header type");
			er = KCGI_FORM;
			break;
		}

		if (er == KCGI_HUP) {
			kutil_warnx(NULL, NULL, "FastCGI: "
				"connection severed at stdin");
			/* Note: writing error code. */
			rc = 0;
			fullwrite(work_ctl, &rc, sizeof(int));
			continue;
		} else if (er != KCGI_OK) {
			kutil_warnx(NULL, NULL, "FastCGI: "
				"unrecoverable error at stdin");
			break;
		}

		/* 
		 * Notify the control process that we've received all of
		 * our data by giving back the cookie and requestId.
		 * FIXME: merge cookie and rc.
		 */

		rc = 1;
		fullwrite(work_ctl, &rc, sizeof(int));
		fullwrite(work_ctl, &cookie, sizeof(uint32_t));
		fullwrite(work_ctl, &rid, sizeof(uint16_t));

		/* 
		 * Read our last zero-length frame.
		 * This is because kfcgi_control() always ends with an
		 * empty frame, regardless of whether we're in an error
		 * or not.
		 * So if we're this far, we've read the full request,
		 * and we should have an empty frame.
		 */

		rc = fullread(fbuf.fd, &sz, sizeof(size_t), 0, &er);
		if (rc <= 0) {
			kutil_warnx(NULL, NULL, 
				"FastCGI: error reading trailer");
			break;
		} else if (sz != 0) {
			kutil_warnx(NULL, NULL, 
				"FastCGI: trailer not zero-length");
			er = KCGI_FORM;
			break;
		} 

		/* 
		 * Now we can reply to our request.
		 * See kworker_parent().
		 * These are in a very specific order.
		 */

		kworker_child_env(envs, wfd, envsz);
		meth = kworker_child_method(envs, wfd, envsz);
		kworker_child_auth(envs, wfd, envsz);
		md5 = kworker_child_rawauth(envs, wfd, envsz);
		kworker_child_scheme(envs, wfd, envsz);
		kworker_child_remote(envs, wfd, envsz);
		kworker_child_path(envs, wfd, envsz);
		kworker_child_scriptname(envs, wfd, envsz);
		kworker_child_httphost(envs, wfd, envsz);
		kworker_child_port(envs, wfd, envsz);

		/* 
		 * And now the message body itself.
		 * We must either have a NULL message or non-zero
		 * length.
		 */

		assert(ssz == 0 || sbuf != NULL);
		kworker_child_body(envs, wfd, envsz, &pp, 
			meth, (char *)sbuf, ssz, debugging, md5);
		kworker_child_query(envs, wfd, envsz, &pp);
		kworker_child_cookies(envs, wfd, envsz, &pp);
		kworker_child_last(wfd);
	}

	/* The same as what we do at the loop start. */

	free(sbuf);
	for (i = 0; i < envsz; i++) {
		free(envs[i].key);
		free(envs[i].val);
	}
	free(envs);
	free(fbuf.buf);
}
