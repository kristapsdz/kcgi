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
#include <sys/poll.h>
#include <sys/wait.h>

#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "kcgi.h"
#include "extern.h"

/*
 * Types of field we can read from the child.
 * This governs the list of fields into which we'll assign this
 * particular field.
 */
enum	input {
	IN_COOKIE = 0, /* cookies (environment variable) */
	IN_QUERY, /* query string */
	IN_FORM, /* any sort of standard input form */
	IN__MAX
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

struct	parms {
	int	 		 fd;
	const char *const	*mimes;
	size_t			 mimesz;
	const struct kvalid	*keys;
	size_t			 keysz;
	enum input		 type;
};

static	const char *const kmethods[KMETHOD__MAX] = {
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

static	const char *const kauths[KAUTH_UNKNOWN] = {
	NULL,
	"basic",
	"digest"
};

/*
 * Read the contents of buf, size bufsz, entirely, using non-blocking
 * reads (i.e., poll(2) then read(2)).
 * This will exit with -1 on fatal errors (the child didn't return
 * enough data or we received an unexpected EOF) or 0 on EOF (only if
 * it's allowed), otherwise 1.
 */
static int
fullread(int fd, void *buf, size_t bufsz, int eofok, enum kcgi_err *er)
{
	ssize_t	 	 ssz;
	size_t	 	 sz;
	struct pollfd	 pfd;

	pfd.fd = fd;
	pfd.events = POLLIN;
	*er = KCGI_SYSTEM;

	for (sz = 0; sz < bufsz; sz += (size_t)ssz) {
		if (-1 == poll(&pfd, 1, -1)) {
			XWARN("poll: %d, POLLIN", fd);
			return(-1);
		}
		ssz = read(fd, buf + sz, bufsz - sz);
		if (ssz < 0 && EAGAIN == errno) {
			XWARN("read: trying again");
			ssz = 0;
			continue;
		} else if (ssz < 0) {
			XWARN("read: %d, %zu", fd, bufsz - sz);
			return(-1);
		} else if (0 == ssz && sz > 0) {
			XWARN("read: short read");
			*er = KCGI_FORM;
			return(-1);
		} else if (0 == ssz && sz == 0 && ! eofok) {
			XWARNX("read: unexpected eof");
			*er = KCGI_FORM;
			return(-1);
		} else if (0 == ssz && sz == 0 && eofok) {
			*er = KCGI_OK;
			return(0);
		}

		/* Additive overflow check. */
		if (sz > SIZE_MAX - (size_t)ssz) {
			XWARNX("read: overflow: %zu, %zd", sz, ssz);
			*er = KCGI_FORM;
			return(-1);
		}
	}

	*er = KCGI_OK;
	return(1);
}

/*
 * Read a word from the stream, which consists of the word size followed
 * by the word itself, not including the nil terminator.
 */
static enum kcgi_err
fullreadword(int fd, char **cp)
{
	size_t	 	 sz;
	enum kcgi_err	 ke;

	if (fullread(fd, &sz, sizeof(size_t), 0, &ke) < 0)
		return(ke);

	if (NULL == (*cp = XMALLOC(sz + 1)))
		return(KCGI_ENOMEM);

	(*cp)[sz] = '\0';
	if (0 == sz)
		return(KCGI_OK);

	fullread(fd, *cp, sz, 0, &ke);
	return(ke);
}

/*
 * Read a single kpair from the child.
 * This returns 0 if there are no more pairs to read and -1 if any
 * errors occur (the parent should also exit with server failure).
 * Otherwise, it returns 1 and the pair is zeroed and filled in.
 */
static int
input(enum input *type, struct kpair *kp, int fd, enum kcgi_err *ke)
{
	size_t		 sz;
	int		 rc;
	ptrdiff_t	 diff;

	memset(kp, 0, sizeof(struct kpair));

	/* This will return EOF for the last one. */
	if (0 == (rc = fullread(fd, type, sizeof(enum input), 1, ke)))
		return(0);
	else if (rc < 0)
		return(-1);

	if (*type >= IN__MAX) {
		XWARNX("unknown input type %d", *type);
		*ke = KCGI_FORM;
		return(-1);
	}

	/* TODO: check additive overflow. */
	if (fullread(fd, &sz, sizeof(size_t), 0, ke) < 0)
		return(-1);
	if (NULL == (kp->key = XCALLOC(sz + 1, 1))) {
		*ke = KCGI_ENOMEM;
		return(-1);
	}
	if (fullread(fd, kp->key, sz, 0, ke) < 0)
		return(-1);

	/* TODO: check additive overflow. */
	if (fullread(fd, &kp->valsz, sizeof(size_t), 0, ke) < 0)
		return(-1);
	if (NULL == (kp->val = XCALLOC(kp->valsz + 1, 1))) {
		*ke = KCGI_ENOMEM;
		return(-1);
	}
	if (fullread(fd, kp->val, kp->valsz, 0, ke) < 0)
		return(-1);

	if (fullread(fd, &kp->state, sizeof(enum kpairstate), 0, ke) < 0)
		return(-1);
	if (fullread(fd, &kp->type, sizeof(enum kpairtype), 0, ke) < 0)
		return(-1);
	if (fullread(fd, &kp->keypos, sizeof(size_t), 0, ke) < 0)
		return(-1);

	if (KPAIR_VALID == kp->state)
		switch (kp->type) {
		case (KPAIR_DOUBLE):
			if (fullread(fd, &kp->parsed.d, sizeof(double), 0, ke) < 0)
				return(-1);
			break;
		case (KPAIR_INTEGER):
			if (fullread(fd, &kp->parsed.i, sizeof(int64_t), 0, ke) < 0)
				return(-1);
			break;
		case (KPAIR_STRING):
			if (fullread(fd, &diff, sizeof(ptrdiff_t), 0, ke) < 0)
				return(-1);
			if (diff > (ssize_t)kp->valsz) {
				*ke = KCGI_FORM;
				return(-1);
			}
			kp->parsed.s = kp->val + diff;
			break;
		default:
			break;
		}

	/* TODO: check additive overflow. */
	if (fullread(fd, &sz, sizeof(size_t), 0, ke) < 0)
		return(-1);
	if (NULL == (kp->file = XCALLOC(sz + 1, 1))) {
		*ke = KCGI_ENOMEM;
		return(-1);
	}
	if (fullread(fd, kp->file, sz, 0, ke) < 0)
		return(-1);

	/* TODO: check additive overflow. */
	if (fullread(fd, &sz, sizeof(size_t), 0, ke) < 0)
		return(-1);
	if (NULL == (kp->ctype = XCALLOC(sz + 1, 1))) {
		*ke = KCGI_ENOMEM;
		return(-1);
	}
	if (fullread(fd, kp->ctype, sz, 0, ke) < 0)
		return(-1);
	if (fullread(fd, &kp->ctypepos, sizeof(size_t), 0, ke) < 0)
		return(-1);

	/* TODO: check additive overflow. */
	if (fullread(fd, &sz, sizeof(size_t), 0, ke) < 0)
		return(-1);
	if (NULL == (kp->xcode = XCALLOC(sz + 1, 1)))  {
		*ke = KCGI_ENOMEM;
		return(-1);
	}
	if (fullread(fd, kp->xcode, sz, 0, ke) < 0) 
		return(-1);

	return(1);
}

static void
fullwrite(int fd, const void *buf, size_t bufsz)
{
	ssize_t	 	 ssz;
	size_t	 	 sz;
	struct pollfd	 pfd;

	pfd.fd = fd;
	pfd.events = POLLOUT;

	for (sz = 0; sz < bufsz; sz += (size_t)ssz) {
		if (-1 == poll(&pfd, 1, -1)) {
			XWARN("poll: %d, POLLOUT", fd);
			_exit(EXIT_FAILURE);
		}

		ssz = write(fd, buf + sz, bufsz - sz);
		if (ssz < 0 && EAGAIN == errno) {
			XWARN("write: trying again");
			ssz = 0;
			continue;
		} else if (ssz < 0) {
			XWARN("write: %d, %zu", fd, bufsz - sz);
			_exit(EXIT_FAILURE);
		}
		/* Additive overflow check. */
		if (sz > SIZE_MAX - (size_t)ssz) {
			XWARNX("write: overflow: %zu, %zd", sz, ssz);
			_exit(EXIT_FAILURE);
		}
	}
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
	char		*key, *val, *end, *start, *line;
	size_t		 i;
	int		 rc = 0;

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

	/* 
	 * Try to cast the content-type, if any, into a recognisable
	 * form.
	 * This is a convenience for the calling application.
	 */
	if (NULL != mime->ctype) {
		for (i = 0; i < pp->mimesz; i++)
			if (0 == strcasecmp(pp->mimes[i], mime->ctype))
				break;
		mime->ctypepos = i;
	} else
		mime->ctypepos = pp->mimesz;

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
parse_body(const char *ctype, const struct kworker *work, 
	const struct parms *pp, size_t len)
{
	char		*p;
	char		 name;
	struct mime	 mime;
	size_t	 	 sz;

	p = scanbuf(work, len, &sz);
	memset(&mime, 0, sizeof(struct mime));
	mime.ctype = strdup(ctype);
	name = '\0';
	output(pp, &name, p, sz, &mime);
	free(mime.ctype);
	free(p);
}

/*
 * Parse an encoding type given as `text/plan'.
 * We simply suck this into a buffer and start parsing pairs directly
 * from the input.
 */
static void
parse_text(const struct kworker *work, 
	const struct parms *pp, size_t len)
{
	char	*p;

	p = scanbuf(work, len, NULL);
	parse_pairs_text(pp, p);
	free(p);
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

static void
parse_urlenc(const struct kworker *work,
	const struct parms *pp, size_t len)
{
	char	*p;

	p = scanbuf(work, len, NULL);
	parse_pairs_urlenc(pp, p);
	free(p);
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
	const struct parms *pp, char *line, size_t len)
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
	cp = scanbuf(work, len, &sz);
	len = 0;
	parse_multiform(pp, NULL, line, cp, sz, &len);
	free(cp);
}

static struct kpair *
kpair_expand(struct kpair **kv, size_t *kvsz)
{

	*kv = XREALLOCARRAY(*kv, *kvsz + 1, sizeof(struct kpair));
	memset(&(*kv)[*kvsz], 0, sizeof(struct kpair));
	(*kvsz)++;
	return(&(*kv)[*kvsz - 1]);
}

/*
 * This is the parent kcgi process.
 * It spins on input from the child until all fields have been received.
 * These fields are sent from the child's output() function.
 * Each input field consists of the data and its validation state.
 * We build up the kpair arrays here with this data, then assign the
 * kpairs into named buckets.
 */
enum kcgi_err
khttp_input_parent(int fd, struct kreq *r, pid_t pid)
{
	struct kpair	 kp;
	struct kpair	*kpp;
	enum input	 type;
	int		 rc;
	enum kcgi_err	 ke;
	size_t		 i;

	/* Pointers freed at "out" label. */
	memset(&kp, 0, sizeof(struct kpair));

	/*
	 * First read all of our parsed parameters.
	 * Each parsed parameter is handled a little differently.
	 * This list will end with META__MAX.
	 */
	if (fullread(fd, &r->method, sizeof(enum kmethod), 0, &ke) < 0) {
		XWARNX("failed to read request method");
		goto out;
	} else if (fullread(fd, &r->auth, sizeof(enum kauth), 0, &ke) < 0) {
		XWARNX("failed to read authorisation type");
		goto out;
	} else if (KCGI_OK != (ke = fullreadword(fd, &r->remote))) {
		XWARNX("failed to read remote");
		goto out;
	} else if (KCGI_OK != (ke = fullreadword(fd, &r->fullpath))) {
		XWARNX("failed to read fullpath");
		goto out;
	} else if (KCGI_OK != (ke = fullreadword(fd, &r->suffix))) {
		XWARNX("failed to read suffix");
		goto out;
	} else if (KCGI_OK != (ke = fullreadword(fd, &r->pagename))) {
		XWARNX("failed to read page part");
		goto out;
	} else if (KCGI_OK != (ke = fullreadword(fd, &r->path))) {
		XWARNX("failed to read path part");
		goto out;
	}

	while ((rc = input(&type, &kp, fd, &ke)) > 0) {
		/*
		 * We have a parsed field from the child process.
		 * Begin by expanding the number of parsed fields
		 * depending on whether we have a cookie or form input.
		 * Then copy the new data.
		 */
		kpp = IN_COOKIE == type ?
			kpair_expand(&r->cookies, &r->cookiesz) :
			kpair_expand(&r->fields, &r->fieldsz);

		if (NULL == kpp) {
			rc = -1;
			ke = KCGI_ENOMEM;
			break;
		}

		*kpp = kp;
	}

	if (rc < 0)
		goto out;

	/*
	 * Now that the field and cookie arrays are fixed and not going
	 * to be reallocated any more, we run through both arrays and
	 * assign the named fields into buckets.
	 */
	for (i = 0; i < r->fieldsz; i++) {
		kpp = &r->fields[i];
		if (kpp->keypos == r->keysz)
			continue;
		if (KPAIR_INVALID != kpp->state) {
			kpp->next = r->fieldmap[kpp->keypos];
			r->fieldmap[kpp->keypos] = kpp;
		} else {
			kpp->next = r->fieldnmap[kpp->keypos];
			r->fieldnmap[kpp->keypos] = kpp;
		}
	}
	for (i = 0; i < r->cookiesz; i++) {
		kpp = &r->cookies[i];
		if (kpp->keypos == r->keysz)
			continue;
		if (KPAIR_INVALID != kpp->state) {
			kpp->next = r->cookiemap[kpp->keypos];
			r->cookiemap[kpp->keypos] = kpp;
		} else {
			kpp->next = r->cookienmap[kpp->keypos];
			r->cookienmap[kpp->keypos] = kpp;
		}
	}

	ke = KCGI_OK;
	/*
	 * Usually, "kp" would be zeroed after its memory is copied into
	 * one of the form-input arrays.
	 * However, in the case of error, these may still have
	 * allocations, so free them now.
	 */
out:
	free(kp.key);
	free(kp.val);
	free(kp.file);
	free(kp.ctype);
	free(kp.xcode);

	return(ke);
}

/*
 * This is the child kcgi process that's going to do the unsafe reading
 * of network data to parse input.
 * When it parses a field, it outputs the key, key size, value, and
 * value size along with the field type.
 */
void
khttp_input_child(const struct kworker *work, 
	const struct kvalid *keys, size_t keysz, 
	const char *const *mimes, size_t mimesz)
{
	struct parms	 pp;
	char		*cp, *ep, *sub;
	const char	*ccp;
	int		 wfd;
	enum kmethod	 meth;
	enum kauth	 auth;
	size_t	 	 len;

	pp.fd = wfd = work->sock[KWORKER_WRITE];
	pp.keys = keys;
	pp.keysz = keysz;
	pp.mimes = mimes;
	pp.mimesz = mimesz;

	/* RFC 3875, 4.1.12. */
	/* We assume GET if not supplied. */
	meth = KMETHOD_GET;
	if (NULL != (ccp = getenv("REQUEST_METHOD")))
		for (meth = 0; meth < KMETHOD__MAX; meth++)
			if (0 == strcmp(kmethods[meth], ccp))
				break;
	fullwrite(wfd, &meth, sizeof(enum kmethod));

	/* Determine authenticaiton: RFC 3875, 4.1.1. */
	auth = KAUTH_NONE;
	if (NULL != (ccp = getenv("AUTH_TYPE"))) 
		for (auth = 0; auth < KAUTH_UNKNOWN; auth++) {
			if (NULL == kauths[auth])
				continue;
			if (0 == strcmp(kauths[auth], ccp))
				break;
		}
	fullwrite(wfd, &auth, sizeof(enum kauth));

	/* RFC 3875, 4.1.8. */
	/* Never supposed to be NULL, but to be sure... */
	if (NULL == (ccp = getenv("REMOTE_ADDR")))
		ccp = "127.0.0.1";
	len = strlen(ccp);
	fullwrite(wfd, &len, sizeof(size_t));
	fullwrite(wfd, ccp, len);

	/*
	 * Now parse the first path element (the page we want to
	 * access), subsequent path information, and the file suffix.
	 * We convert suffix and path element into the respective enum's
	 * inline.
	 */
	if (NULL != (cp = getenv("PATH_INFO"))) {
		len = strlen(cp);
		fullwrite(wfd, &len, sizeof(size_t));
		fullwrite(wfd, cp, len);
	} else {
		len = 0;
		fullwrite(wfd, &len, sizeof(size_t));
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
			fullwrite(wfd, &len, sizeof(size_t));
			fullwrite(wfd, ep, len);
		} else {
			len = 0;
			fullwrite(wfd, &len, sizeof(size_t));
		}

		/* Now find the top-most path part. */
		if (NULL != (sub = strchr(cp, '/')))
			*sub++ = '\0';

		/* Send the base path. */
		len = strlen(cp);
		fullwrite(wfd, &len, sizeof(size_t));
		fullwrite(wfd, cp, len);

		/* Send the path part. */
		if (NULL != sub) {
			len = strlen(sub);
			fullwrite(wfd, &len, sizeof(size_t));
			fullwrite(wfd, sub, len);
		} else {
			len = 0;
			fullwrite(wfd, &len, sizeof(size_t));
		}
	} else {
		len = 0;
		/* Suffix, base path, and path part. */
		fullwrite(wfd, &len, sizeof(size_t));
		fullwrite(wfd, &len, sizeof(size_t));
		fullwrite(wfd, &len, sizeof(size_t));
	}

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
	pp.type = IN_FORM;
	if (NULL != (cp = getenv("CONTENT_TYPE"))) {
		if (0 == strcasecmp(cp, "application/x-www-form-urlencoded"))
			parse_urlenc(work, &pp, len);
		else if (0 == strncasecmp(cp, "multipart/form-data", 19)) 
			parse_multi(work, &pp, cp + 19, len);
		else if (KMETHOD_POST == meth && 0 == strcasecmp(cp, "text/plain"))
			parse_text(work, &pp, len);
		else
			parse_body(cp, work, &pp, len);
	} else
		parse_body(kmimetypes[KMIME_APP_OCTET_STREAM], work, &pp, len);

	/*
	 * Even POST requests are allowed to have QUERY_STRING elements,
	 * so parse those out now.
	 * Note: both QUERY_STRING and CONTENT_TYPE fields share the
	 * same field space.
	 * Since this is a getenv(), we know the returned value is
	 * nil-terminated.
	 */
	pp.type = IN_QUERY;
	if (NULL != (cp = getenv("QUERY_STRING")))
		parse_pairs_urlenc(&pp, cp);

	/*
	 * Cookies come last.
	 * These use the same syntax as QUERY_STRING elements, but don't
	 * share the same namespace (just as a means to differentiate
	 * the same names).
	 * Since this is a getenv(), we know the returned value is
	 * nil-terminated.
	 */
	pp.type = IN_COOKIE;
	if (NULL != (cp = getenv("HTTP_COOKIE")))
		parse_pairs_urlenc(&pp, cp);
}
