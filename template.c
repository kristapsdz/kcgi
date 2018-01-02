/*	$Id$ */
/*
 * Copyright (c) 2017 Kristaps Dzonsons <kristaps@bsd.lv>
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

#include <sys/mman.h>
#include <sys/stat.h>

#include <fcntl.h>
#include <limits.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "kcgi.h"
#include "extern.h"

static int
khttp_templatex_write(const char *dat, size_t sz, void *arg)
{

	khttp_write(arg, dat, sz);
	return(1);
}

int
khttp_template_buf(struct kreq *req, 
	const struct ktemplate *t, const char *buf, size_t sz)
{
	struct ktemplatex x;

	memset(&x, 0, sizeof(struct ktemplatex));
	x.writer = khttp_templatex_write;

	return(khttp_templatex_buf(t, buf, sz, &x, req));
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
khttp_templatex_buf(const struct ktemplate *t, 
	const char *buf, size_t sz, 
	const struct ktemplatex *opt, void *arg)
{
	size_t		 i, j, len, start, end;
	ktemplate_writef fp;

	if (0 == sz)
		return(1);

	/* Require a writer. */

	if (NULL == opt || NULL == opt->writer)
		return(0);
	
	fp = opt->writer;

	/*
	 * If we have no callback mechanism, then we're going to push
	 * the unmodified text out.
	 * Check both the per-key template system and the fallback, if
	 * provided.
	 */

	if (NULL == t && NULL == opt->fbk)
		return(fp(buf, sz, arg));

	for (i = 0; i < sz - 1; i++) {
		/* Look for the starting "@@" marker. */
		if ('@' != buf[i] || '@' != buf[i + 1]) {
			if ( ! fp(&buf[i], 1, arg))
				return(0);
			continue;
		} 

		/* Seek to find the end "@@" marker. */

		start = i + 2;
		for (end = start + 1; end < sz - 1; end++)
			if ('@' == buf[end] && '@' == buf[end + 1])
				break;

		/* Continue printing if not found of 0-length. */

		if (end == sz - 1 || end == start) {
			if ( ! fp(&buf[i], 1, arg))
				return(0);
			continue;
		}

		/* 
		 * Look for a matching key.
		 * If we find no matching key, use the fallback (if
		 * found), otherwise just continue as if the key were
		 * opaque text.
		 */

		for (j = 0; j < t->keysz; j++) {
			len = strlen(t->key[j]);
			if (len != end - start)
				continue;
			else if (memcmp(&buf[start], t->key[j], len))
				continue;
			if ( ! (*t->cb)(j, t->arg)) {
				XWARNX("template error");
				return(0);
			}
			break;
		}

		if (j == t->keysz && NULL != opt->fbk) {
			len = end - start;
			if ( ! (*opt->fbk)(&buf[start], len, t->arg)) {
				XWARNX("template error");
				return(0);
			}
		} else if (j == t->keysz) {
			if ( ! fp(&buf[i], 1, arg))
				return(0);
		} else
			i = end + 1;
	}

	if (i < sz && ! fp(&buf[i], 1, arg))
		return(0);

	return(1);
}

int
khttp_template(struct kreq *req, 
	const struct ktemplate *t, const char *fname)
{
	struct ktemplatex x;

	memset(&x, 0, sizeof(struct ktemplatex));
	x.writer = khttp_templatex_write;
	return(khttp_templatex(t, fname, &x, req));
}

int
khttp_templatex(const struct ktemplate *t, 
	const char *fname, const struct ktemplatex *opt, void *arg)
{
	int		 fd, rc;

	if (-1 == (fd = open(fname, O_RDONLY, 0))) {
		XWARN("open: %s", fname);
		return(0);
	}

	rc = khttp_templatex_fd(t, fd, fname, opt, arg);
	close(fd);
	return(rc);
}

int
khttp_template_fd(struct kreq *req, 
	const struct ktemplate *t, int fd, const char *fname)
{
	struct ktemplatex x;

	memset(&x, 0, sizeof(struct ktemplatex));
	x.writer = khttp_templatex_write;
	return(khttp_templatex_fd(t, fd, fname, &x, req));
}

int
khttp_templatex_fd(const struct ktemplate *t, 
	int fd, const char *fname,
	const struct ktemplatex *opt, void *arg)
{
	struct stat 	 st;
	char		*buf;
	size_t		 sz;
	int		 rc;

	if (NULL == fname)
		fname = "<unknown descriptor>";

	if (-1 == fstat(fd, &st)) {
		XWARN("fstat: %s", fname);
		return(0);
	} else if (st.st_size > SSIZE_MAX) {
		XWARNX("size overflow: %s", fname);
		return(0);
	} else if (st.st_size <= 0) {
		XWARNX("zero-length: %s", fname);
		return(1);
	}

	sz = (size_t)st.st_size;
	buf = mmap(NULL, sz, PROT_READ, MAP_SHARED, fd, 0);

	if (MAP_FAILED == buf) {
		XWARN("mmap: %s", fname);
		return(0);
	}

	rc = khttp_templatex_buf(t, buf, sz, opt, arg);
	munmap(buf, sz);
	return(rc);
}
