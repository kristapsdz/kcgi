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
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
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
	IN_COOKIE, /* cookies (environment variable) */
	IN_QUERY, /* query string */
	IN_FORM /* any sort of standard input form */
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
 * Read the contents of buf, size bufsz, entirely.
 * This will exit with -1 on fatal errors (the child didn't return
 * enough data or we received an unexpected EOF) or 0 on EOF (only if
 * it's allowed), otherwise 1.
 */
static int
fullread(int fd, void *buf, size_t bufsz, int eofok)
{
	ssize_t	 ssz;
	size_t	 sz;

	for (sz = 0; sz < bufsz; sz += (size_t)ssz) {
		ssz = read(fd, buf + sz, bufsz - sz);
		if (ssz < 0) {
			perror("read");
			return(-1);
		} else if (0 == ssz && sz > 0) {
			fprintf(stderr, "read: short read\n");
			return(-1);
		} else if (0 == ssz && sz == 0 && ! eofok) {
			fprintf(stderr, "read: unexpected eof\n");
			return(-1);
		} else if (0 == ssz && sz == 0 && eofok)
			return(0);

		/* Additive overflow check. */
		if (sz > SIZE_MAX - (size_t)ssz) {
			fprintf(stderr, "additive overflow\n");
			return(0);
		}
	}
	return(1);
}

/*
 * Read a single kpair from the child.
 * This returns 0 if there are no more pairs to read and -1 if any
 * errors occur (the parent should also exit with server failure).
 * Otherwise, it returns 1 and the pair is zeroed and filled in.
 */
static int
input(enum input *type, struct kpair *kp, int fd)
{
	size_t		 sz;
	int		 rc;

	memset(kp, 0, sizeof(struct kpair));

	/* This will return EOF for the last one. */
	if (0 == (rc = fullread(fd, type, sizeof(enum input), 1)))
		return(0);
	else if (rc < 0)
		return(-1);
	if (fullread(fd, &sz, sizeof(size_t), 0) < 0)
		return(-1);
	/* TODO: check additive overflow. */
	if (NULL == (kp->key = calloc(sz + 1, 1)))
		return(-1);
	if (fullread(fd, kp->key, sz, 0) < 0)
		return(-1);
	if (fullread(fd, &kp->valsz, sizeof(size_t), 0) < 0)
		return(-1);
	/* TODO: check additive overflow. */
	if (NULL == (kp->val = calloc(kp->valsz + 1, 1)))
		return(-1);
	if (fullread(fd, kp->val, kp->valsz, 0) < 0)
		return(-1);
	if (fullread(fd, &sz, sizeof(size_t), 0) < 0)
		return(-1);
	/* TODO: check additive overflow. */
	if (NULL == (kp->file = calloc(sz + 1, 1)))
		return(-1);
	if (fullread(fd, kp->file, sz, 0) < 0)
		return(-1);
	if (fullread(fd, &sz, sizeof(size_t), 0) < 0)
		return(-1);
	/* TODO: check additive overflow. */
	if (NULL == (kp->ctype = calloc(sz + 1, 1)))
		return(-1);
	if (fullread(fd, kp->ctype, sz, 0) < 0)
		return(-1);
	if (fullread(fd, &sz, sizeof(size_t), 0) < 0)
		return(-1);
	/* TODO: check additive overflow. */
	if (NULL == (kp->xcode = calloc(sz + 1, 1)))
		return(-1);
	if (fullread(fd, kp->xcode, sz, 0) < 0)
		return(-1);

	return(1);
}

/*
 * Output a type, parsed key, and value to the output stream.
 */
static void
output(enum input type, const char *key, const char *val, 
	size_t valsz, const char *file, const char *ctype,
	const char *xcode)
{
	size_t	 sz;

	/* FIXME: use poll() to avoid blocking. */
	write(STDOUT_FILENO, &type, sizeof(enum input));
	sz = strlen(key);
	write(STDOUT_FILENO, &sz, sizeof(size_t));
	write(STDOUT_FILENO, key, sz);
	write(STDOUT_FILENO, &valsz, sizeof(size_t));
	write(STDOUT_FILENO, val, valsz);
	sz = NULL != file ? strlen(file) : 0;
	write(STDOUT_FILENO, &sz, sizeof(size_t));
	write(STDOUT_FILENO, file, sz);
	sz = NULL != ctype ? strlen(ctype) : 0;
	write(STDOUT_FILENO, &sz, sizeof(size_t));
	write(STDOUT_FILENO, ctype, sz);
	sz = NULL != xcode ? strlen(xcode) : 0;
	write(STDOUT_FILENO, &sz, sizeof(size_t));
	write(STDOUT_FILENO, xcode, sz);
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
	if (NULL == (p = malloc(len + 1))) {
		perror("malloc");
		_exit(EXIT_FAILURE);
	}

	/* Keep reading til we get all the data. */
	/* FIXME: use poll() to avoid blocking. */
	for (sz = 0; sz < len; sz += (size_t)ssz) {
		ssz = read(STDIN_FILENO, p + sz, len - sz);
		if (ssz < 0) {
			perror("read");
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
 * Parse keys and values separated by newlines.
 * I'm not aware of any standard that defines this, but the W3
 * guidelines for HTML give a rough idea.
 */
static void
parse_pairs_text(enum input type, char *p)
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
		output(type, key, val, strlen(val), NULL, NULL, NULL);
	}
}

/*
 * Parse an encoding type given as `text/plan'.
 * We simply suck this into a buffer and start parsing pairs directly
 * from the input.
 */
static void
parse_text(size_t len)
{
	char	*p;

	p = scanbuf(len, NULL);
	parse_pairs_text(IN_FORM, p);
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
 * This MUST be a non-binary (i.e., nil-terminated) string!
 */
static void
parse_pairs_urlenc(enum input type, char *p)
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
		output(type, key, val, strlen(val), NULL, NULL, NULL);
	}
}

static void
parse_urlenc(size_t len)
{
	char	*p;

	p = scanbuf(len, NULL);
	parse_pairs_urlenc(IN_FORM, p);
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
	if (NULL == (*dst = strdup(src))) {
		perror("strdup");
		_exit(EXIT_FAILURE);
	}
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
 * This is described by the "multipart-body" BNF part of RFC 2046,
 * section 5.1.1.
 * We return TRUE if the parse was ok, FALSE if errors occured (all
 * calling parsers should bail too).
 */
static int
parse_multiform(const char *name, const char *bound, 
	char *buf, size_t len, size_t *pos)
{
	struct mime	 mime;
	size_t		 endpos, bbsz, partsz;
	char		*ln, *bb;
	int		 rc, first;

	rc = 0;
	/* Define our buffer boundary. */
	asprintf(&bb, "\r\n--%s", bound);
	if (NULL == bb) {
		perror("asprintf");
		_exit(EXIT_FAILURE);
	}
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
		if (NULL == mime.ctype) {
			mime.ctype = strdup("text/plain");
			if (NULL == mime.ctype) {
				perror("strdup");
				_exit(EXIT_FAILURE);
			}
		}

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
				(NULL != name ? name :
				 mime.name, mime.bound,
				 buf, *pos + partsz, pos))
				goto out;
			continue;
		}

		/* Assign all of our key-value pair data. */
		output(IN_FORM, NULL != name ? name : mime.name, 
			&buf[*pos], partsz, mime.file, mime.ctype,
			mime.xcode);
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
parse_multi(char *line, size_t len)
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
	parse_multiform(NULL, line, cp, sz, &len);
	free(cp);
}

static struct kpair *
kpair_expand(struct kpair **kv, size_t *kvsz)
{

	*kv = reallocarray(*kv, *kvsz + 1, sizeof(struct kpair));
	if (NULL == *kv)
		return(NULL);
	memset(&(*kv)[*kvsz], 0, sizeof(struct kpair));
	(*kvsz)++;
	return(&(*kv)[*kvsz - 1]);
}

/*
 * This is the parent kcgi process.
 * It spins on input from the child until 
 */
int
khttp_input_parent(int fd, struct kreq *r)
{
	struct kpair	 kp;
	struct kpair	*kpp;
	enum input	 type;
	int		 rc;

	while ((rc = input(&type, &kp, fd)) > 0) {
		switch (type) {
		case (IN_COOKIE):
			kpp = kpair_expand(&r->cookies, &r->cookiesz);
			break;
		case (IN_QUERY):
			/* FALLTHROUGH */
		case (IN_FORM):
			kpp = kpair_expand(&r->fields, &r->fieldsz);
			break;
		default:
			fprintf(stderr, "bad child input type\n");
			return(0);
		}
		if (NULL == kpp)
			return(0);
		*kpp = kp;
	}

	return(0 == rc);
}

/*
 * This is the child kcgi process that's going to do the unsafe reading
 * of network data to parse input.
 * When it parses a field, it outputs the key, key size, value, and
 * value size along with the field type.
 */
void
khttp_input_child(void)
{
	char	*cp;
	size_t	 len;

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
			parse_urlenc(len);
		else if (0 == strncasecmp(cp, "multipart/form-data", 19)) 
			parse_multi(cp + 19, len);
		else if (0 == strcasecmp(cp, "text/plain"))
			parse_text(len);
	} else
		parse_text(len);

	/*
	 * Even POST requests are allowed to have QUERY_STRING elements,
	 * so parse those out now.
	 * Note: both QUERY_STRING and CONTENT_TYPE fields share the
	 * same field space.
	 * Since this is a getenv(), we know the returned value is
	 * nil-terminated.
	 */
	if (NULL != (cp = getenv("QUERY_STRING")))
		parse_pairs_urlenc(IN_QUERY, cp);

	/*
	 * Cookies come last.
	 * These use the same syntax as QUERY_STRING elements, but don't
	 * share the same namespace (just as a means to differentiate
	 * the same names).
	 * Since this is a getenv(), we know the returned value is
	 * nil-terminated.
	 */
	if (NULL != (cp = getenv("HTTP_COOKIE")))
		parse_pairs_urlenc(IN_COOKIE, cp);
}
