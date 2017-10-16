/*	$Id$ */
/*
 * Copyright (c) 2012, 2014--2017 Kristaps Dzonsons <kristaps@bsd.lv>
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
#include "md5.h"
#include "extern.h"

/*
 * Read a single kpair from the child.
 * This returns 0 if there are no more pairs to read (and eofok has been
 * set) and -1 if any errors occur (the parent should also exit with
 * server failure).
 * Otherwise, it returns 1 and the pair is zeroed and filled in.
 */
static int
input(enum input *type, struct kpair *kp, 
	int fd, enum kcgi_err *ke, int eofok)
{
	size_t		 sz;
	int		 rc;
	ptrdiff_t	 diff;

	memset(kp, 0, sizeof(struct kpair));

	rc = fullread(fd, type, sizeof(enum input), 1, ke);
	if (0 == rc) {
		if (eofok) 
			return(0);
		XWARNX("parent: unexpected eof from child");
		*ke = KCGI_FORM;
		return(-1);
	} else if (rc < 0)
		return(-1);

	if (*type == IN__MAX)
		return(0);

	if (*type > IN__MAX) {
		XWARNX("parent: unknown input type");
		*ke = KCGI_FORM;
		return(-1);
	}

	*ke = fullreadword(fd, &kp->key);
	if (KCGI_OK != *ke) {
		XWARNX("parent: failed read kpair key");
		return(-1);
	}

	*ke = fullreadwordsz(fd, &kp->val, &kp->valsz);
	if (KCGI_OK != *ke) {
		XWARNX("parent: failed read kpair val");
		return(-1);
	}

	sz = sizeof(enum kpairstate);
	if (fullread(fd, &kp->state, sz, 0, ke) < 0) {
		XWARNX("parent: failed read kpair state");
		return(-1);
	} else if (kp->state > KPAIR_INVALID) {
		XWARNX("parent: unknown kpair state");
		return(-1);
	}

	sz = sizeof(enum kpairtype);
	if (fullread(fd, &kp->type, sz, 0, ke) < 0) {
		XWARNX("parent: failed read kpair type");
		return(-1);
	} else if (kp->type > KPAIR__MAX) {
		XWARNX("parent: unknown kpair type");
		return(-1);
	}

	sz = sizeof(size_t);
	if (fullread(fd, &kp->keypos, sz, 0, ke) < 0) {
		XWARNX("parent: failed read kpair pos");
		return(-1);
	} 

	if (KPAIR_VALID == kp->state)
		switch (kp->type) {
		case (KPAIR_DOUBLE):
			sz = sizeof(double);
			rc = fullread(fd, &kp->parsed.d, sz, 0, ke);
			if (rc < 0) {
				XWARNX("parent: failed "
					"read kpair double");
				return(-1);
			}
			break;
		case (KPAIR_INTEGER):
			sz = sizeof(int64_t);
			rc = fullread(fd, &kp->parsed.i, sz, 0, ke);
			if (rc < 0) {
				XWARNX("parent: failed "
					"read kpair integer");
				return(-1);
			}
			break;
		case (KPAIR_STRING):
			sz = sizeof(ptrdiff_t);
			rc = fullread(fd, &diff, sz, 0, ke);
			if (rc < 0) {
				XWARNX("parent: failed "
					"read kpair ptrdiff");
				return(-1);
			}
			if (diff > (ssize_t)kp->valsz) {
				*ke = KCGI_FORM;
				XWARNX("parent: kpair offset"
					"exceeds value size");
				return(-1);
			}
			kp->parsed.s = kp->val + diff;
			break;
		default:
			break;
		}

	*ke = fullreadword(fd, &kp->file);
	if (KCGI_OK != *ke) {
		XWARNX("parent: failed read kpair file");
		return(-1);
	}

	*ke = fullreadword(fd, &kp->ctype);
	if (KCGI_OK != *ke) {
		XWARNX("parent: failed read kpair ctype");
		return(-1);
	}

	sz = sizeof(size_t);
	if (fullread(fd, &kp->ctypepos, sz, 0, ke) < 0) {
		XWARNX("parent: failed read kpair ctypepos");
		return(-1);
	}

	*ke = fullreadword(fd, &kp->xcode);
	if (KCGI_OK != *ke) {
		XWARNX("parent: failed read kpair xcode");
		return(-1);
	}

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
kworker_parent(int fd, struct kreq *r, int eofok)
{
	struct kpair	 kp;
	struct kpair	*kpp;
	enum krequ	 requ;
	enum input	 type;
	int		 rc;
	enum kcgi_err	 ke;
	size_t		 i, dgsz;

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
	if (r->reqsz) {
		r->reqs = XCALLOC(r->reqsz, sizeof(struct khead));
		if (NULL == r->reqs) {
			ke = KCGI_ENOMEM;
			goto out;
		}
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
	} else if (fullread(fd, &dgsz, sizeof(size_t), 0, &ke) < 0) {
		XWARNX("failed to read digest length");
		goto out;
	} else if (MD5_DIGEST_LENGTH == dgsz) {
		/* This is a binary value. */
		if (NULL == (r->rawauth.digest = XMALLOC(dgsz)))
			goto out;
		if (fullread(fd, r->rawauth.digest, dgsz, 0, &ke) < 0) {
			XWARNX("failed to read digest");
			goto out;
		}
	}

	while ((rc = input(&type, &kp, fd, &ke, eofok)) > 0) {
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
	 * Disallow excessive keyposes.
	 */

	for (i = 0; i < r->fieldsz; i++) {
		kpp = &r->fields[i];
		if (kpp->keypos > r->keysz) {
			XWARNX("parent: field keypos exceeds size");
			continue;
		} else if (kpp->keypos == r->keysz)
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
		if (kpp->keypos > r->keysz) {
			XWARNX("parent: cookie keypos exceeds size");
			continue;
		} else if (kpp->keypos == r->keysz)
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
