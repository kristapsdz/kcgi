/*	$Id$ */
/*
 * Copyright (c) 2012, 2014, 2015  Kristaps Dzonsons <kristaps@bsd.lv>
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
#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "kcgi.h"
#include "extern.h"

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
	rc = fullread(fd, type, sizeof(enum input), 1, ke);
	if (0 == rc)
		return(0);
	else if (rc < 0)
		return(-1);

	if (*type == IN__MAX)
		return(0);

	if (*type > IN__MAX) {
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
kworker_parent(int fd, struct kreq *r)
{
	struct kpair	 kp;
	struct kpair	*kpp;
	enum krequ	 requ;
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
	if (fullread(fd, &r->reqsz, sizeof(size_t), 0, &ke) < 0) {
		XWARNX("failed to read request header size");
		goto out;
	}
	r->reqs = XCALLOC(r->reqsz, sizeof(struct khead));
	if (NULL == r->reqs) {
		ke = KCGI_ENOMEM;
		goto out;
	}
	for (i = 0; i < r->reqsz; i++) {
		if (fullread(fd, &requ, sizeof(enum krequ), 0, &ke) < 0) {
			XWARNX("failed to read request identifier");
			goto out;
		}
		if (KCGI_OK != (ke = fullreadword(fd, &r->reqs[i].key))) {
			XWARNX("failed to read request key");
			goto out;
		}
		if (KCGI_OK != (ke = fullreadword(fd, &r->reqs[i].val))) {
			XWARNX("failed to read request value");
			goto out;
		}
		if (requ != KREQU__MAX)
			r->reqmap[requ] = &r->reqs[i];
	}

	if (fullread(fd, &r->method, sizeof(enum kmethod), 0, &ke) < 0) {
		XWARNX("failed to read request method");
		goto out;
	} else if (fullread(fd, &r->auth, sizeof(enum kauth), 0, &ke) < 0) {
		XWARNX("failed to read authorisation type");
		goto out;
	} else if (KCGI_OK != (ke = kworker_auth_parent(fd, &r->rawauth))) {
		XWARNX("failed to read raw authorisation");
		goto out;
	} else if (fullread(fd, &r->scheme, sizeof(enum kscheme), 0, &ke) < 0) {
		XWARNX("failed to read scheme");
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
	} else if (KCGI_OK != (ke = fullreadword(fd, &r->pname))) {
		XWARNX("failed to read script name");
		goto out;
	} else if (KCGI_OK != (ke = fullreadword(fd, &r->host))) {
		XWARNX("failed to read host name");
		goto out;
	} else if (fullread(fd, &r->port, sizeof(uint16_t), 0, &ke) < 0) {
		XWARNX("failed to read port");
		goto out;
	}

	while ((rc = input(&type, &kp, fd, &ke)) > 0) {
		assert(type < IN__MAX);
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
