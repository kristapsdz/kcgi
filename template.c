/*	$Id$ */
/*
 * Copyright (c) 2017--2018 Kristaps Dzonsons <kristaps@bsd.lv>
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

static enum kcgi_err
khttp_templatex_write(const char *dat, size_t sz, void *arg)
{

	return khttp_write(arg, dat, sz);
}

enum kcgi_err
khttp_template_buf(struct kreq *req, 
	const struct ktemplate *t, const char *buf, size_t sz)
{
	struct ktemplatex x;

	memset(&x, 0, sizeof(struct ktemplatex));
	x.writer = khttp_templatex_write;

	return khttp_templatex_buf(t, buf, sz, &x, req);
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
enum kcgi_err
khttp_templatex_buf(const struct ktemplate *t, 
	const char *buf, size_t sz, 
	const struct ktemplatex *opt, void *arg)
{
	size_t		 i, j, len, start, end;
	ktemplate_writef fp;
	enum kcgi_err	 er;

	if (sz == 0)
		return KCGI_OK;

	fp = opt->writer;

	/*
	 * If we have no callback mechanism, then we're going to push
	 * the unmodified text out.
	 * Check both the per-key template system and the fallback, if
	 * provided.
	 */

	if (t == NULL && opt->fbk == NULL)
		return fp(buf, sz, arg);

	for (i = 0; i < sz - 1; i++) {
		/* 
		 * Read ahead til one of our significant characters.
		 * Then emit all characters between then and now.
		 */

		for (j = i; j < sz - 1; j++)
			if (buf[j] == '\\' || buf[j] == '@')
				break;
		if (j > i && (er = fp(&buf[i], j - i, arg)) != KCGI_OK)
			return er;
		i = j;

		/* 
		 * See if we're at an escaped @@, i.e., it's preceded by
		 * the backslash.
		 * If we are, then emit the standalone @@.
		 */

		if (i < sz - 2 && buf[i] == '\\' &&
		    buf[i + 1] == '@' && buf[i + 2] == '@') {
			if ((er = fp(&buf[i + 1], 2, arg)) != KCGI_OK)
				return er;
			i += 2;
			continue;
		}

		/* Look for the starting @@ marker. */

		if (!(buf[i] == '@' && buf[i + 1] == '@')) {
			if ((er = fp(&buf[i], 1, arg)) != KCGI_OK)
				return er;
			continue;
		} 

		/* Seek to find the end "@@" marker. */

		start = i + 2;
		for (end = start; end < sz - 1; end++)
			if (buf[end] == '@' && buf[end + 1] == '@')
				break;

		/* Continue printing if not found. */

		if (end >= sz - 1) {
			if ((er = fp(&buf[i], 1, arg)) != KCGI_OK)
				return er;
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
			if (!(*t->cb)(j, t->arg)) {
				kutil_warnx(NULL, NULL, 
					"template callback error");
				return KCGI_FORM;
			}
			break;
		}

		if (j == t->keysz && opt->fbk != NULL) {
			len = end - start;
			if (!(*opt->fbk)(&buf[start], len, t->arg)) {
				kutil_warnx(NULL, NULL, "template "
					"default callback error");
				return KCGI_FORM;
			}
			i = end + 1;
		} else if (j == t->keysz) {
			if ((er = fp(&buf[i], 1, arg)) != KCGI_OK)
				return er;
		} else
			i = end + 1;
	}

	if (i < sz && (er = fp(&buf[i], 1, arg)) != KCGI_OK)
		return er;

	return KCGI_OK;
}

enum kcgi_err
khttp_template(struct kreq *req, 
	const struct ktemplate *t, const char *fname)
{
	struct ktemplatex x;

	memset(&x, 0, sizeof(struct ktemplatex));
	x.writer = khttp_templatex_write;
	return khttp_templatex(t, fname, &x, req);
}

enum kcgi_err
khttp_templatex(const struct ktemplate *t, 
	const char *fname, const struct ktemplatex *opt, void *arg)
{
	int		 fd;
	enum kcgi_err	 rc;

	if ((fd = open(fname, O_RDONLY, 0)) == -1) {
		kutil_warn(NULL, NULL, "%s", fname);
		return KCGI_SYSTEM;
	}

	rc = khttp_templatex_fd(t, fd, fname, opt, arg);
	close(fd);
	return rc;
}

enum kcgi_err
khttp_template_fd(struct kreq *req, 
	const struct ktemplate *t, int fd, const char *fname)
{
	struct ktemplatex x;

	memset(&x, 0, sizeof(struct ktemplatex));
	x.writer = khttp_templatex_write;
	return khttp_templatex_fd(t, fd, fname, &x, req);
}

enum kcgi_err
khttp_templatex_fd(const struct ktemplate *t, 
	int fd, const char *fname,
	const struct ktemplatex *opt, void *arg)
{
	struct stat 	 st;
	char		*buf;
	size_t		 sz;
	enum kcgi_err	 rc;

	if (fname == NULL)
		fname = "<unknown descriptor>";

	if (fstat(fd, &st) == -1) {
		kutil_warn(NULL, NULL, "%s", fname);
		return KCGI_SYSTEM;
	} else if (st.st_size > SSIZE_MAX) {
		kutil_warnx(NULL, NULL, "%s: too large", fname);
		return KCGI_SYSTEM;
	} else if (st.st_size <= 0) {
		kutil_warnx(NULL, NULL, "%s: zero-length", fname);
		return KCGI_OK;
	}

	sz = (size_t)st.st_size;
	buf = mmap(NULL, sz, PROT_READ, MAP_SHARED, fd, 0);

	if (buf == MAP_FAILED) {
		kutil_warn(NULL, NULL, "%s", fname);
		return KCGI_SYSTEM;
	}

	rc = khttp_templatex_buf(t, buf, sz, opt, arg);
	munmap(buf, sz);
	return rc;
}
