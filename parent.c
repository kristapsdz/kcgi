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
#include "config.h"

#include <assert.h>
#if HAVE_MD5
# include <sys/types.h>
# include <md5.h>
#endif
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
 * This returns 0 if there are no more pairs to read (and eofok has been
 * set) and -1 if any errors occur (the parent should also exit with
 * server failure).
 * Otherwise, it returns 1 and the pair is zeroed and filled in.
 */
static int
input(enum input *type, struct kpair *kp, int fd, 
	enum kcgi_err *ke, int eofok, size_t mimesz, size_t keysz)
{
	size_t		 sz;
	int		 rc;
	ptrdiff_t	 diff;

	memset(kp, 0, sizeof(struct kpair));

	rc = fullread(fd, type, sizeof(enum input), 1, ke);
	if (rc == 0) {
		if (eofok) 
			return 0;
		kutil_warnx(NULL, NULL, "unexpected EOF from child");
		*ke = KCGI_FORM;
		return (-1);
	} else if (rc < 0) {
		kutil_warnx(NULL, NULL, "failed read kpair type");
		return (-1);
	}

	if (*type == IN__MAX)
		return 0;

	if (*type > IN__MAX) {
		kutil_warnx(NULL, NULL, "invalid kpair type");
		*ke = KCGI_FORM;
		return (-1);
	}

	*ke = fullreadword(fd, &kp->key);
	if (*ke != KCGI_OK) {
		kutil_warnx(NULL, NULL, "failed read kpair key");
		return (-1);
	}

	*ke = fullreadwordsz(fd, &kp->val, &kp->valsz);
	if (*ke != KCGI_OK) {
		kutil_warnx(NULL, NULL, "failed read kpair value");
		return (-1);
	}

	sz = sizeof(enum kpairstate);
	if (fullread(fd, &kp->state, sz, 0, ke) < 0) {
		kutil_warnx(NULL, NULL, "failed read kpair state");
		return (-1);
	} else if (kp->state > KPAIR_INVALID) {
		kutil_warnx(NULL, NULL, "invalid kpair state");
		return (-1);
	}

	sz = sizeof(enum kpairtype);
	if (fullread(fd, &kp->type, sz, 0, ke) < 0) {
		kutil_warnx(NULL, NULL, "failed read kpair type");
		return (-1);
	} else if (kp->type > KPAIR__MAX) {
		kutil_warnx(NULL, NULL, "invalid kpair type");
		return (-1);
	}

	sz = sizeof(size_t);
	if (fullread(fd, &kp->keypos, sz, 0, ke) < 0) {
		kutil_warnx(NULL, NULL, "failed read kpair position");
		return (-1);
	} else if (kp->keypos > keysz) {
		kutil_warnx(NULL, NULL, "invalid kpair position");
		return (-1);
	}

	if (kp->state == KPAIR_VALID)
		switch (kp->type) {
		case KPAIR_DOUBLE:
			sz = sizeof(double);
			if (fullread(fd, &kp->parsed.d, sz, 0, ke) > 0)
				break;
			kutil_warnx(NULL, NULL, 
				"failed read kpair double");
			return (-1);
		case KPAIR_INTEGER:
			sz = sizeof(int64_t);
			if (fullread(fd, &kp->parsed.i, sz, 0, ke) > 0)
				break;
			kutil_warnx(NULL, NULL, 
				"failed read kpair integer");
			return (-1);
		case KPAIR_STRING:
			sz = sizeof(ptrdiff_t);
			if (fullread(fd, &diff, sz, 0, ke) < 0) {
				kutil_warnx(NULL, NULL, 
					"failed read kpair ptrdiff");
				return (-1);
			}
			if (diff > (ssize_t)kp->valsz) {
				*ke = KCGI_FORM;
				kutil_warnx(NULL, NULL, 
					"invalid kpair ptrdiff");
				return (-1);
			}
			kp->parsed.s = kp->val + diff;
			break;
		default:
			break;
		}

	*ke = fullreadword(fd, &kp->file);
	if (*ke != KCGI_OK) {
		kutil_warnx(NULL, NULL, 
			"failed read kpair filename");
		return (-1);
	}

	*ke = fullreadword(fd, &kp->ctype);
	if (*ke != KCGI_OK) {
		kutil_warnx(NULL, NULL, 
			"failed read kpair content type");
		return (-1);
	}

	sz = sizeof(size_t);
	if (fullread(fd, &kp->ctypepos, sz, 0, ke) < 0) {
		kutil_warnx(NULL, NULL, 
			"failed read kpair MIME position");
		return (-1);
	} else if (kp->ctypepos > mimesz) {
		kutil_warnx(NULL, NULL, 
			"invalid kpair MIME position");
		return (-1);
	}

	*ke = fullreadword(fd, &kp->xcode);
	if (*ke != KCGI_OK) {
		kutil_warnx(NULL, NULL, 
			"failed read kpair content transfer encoding");
		return (-1);
	}

	return 1;
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
kworker_parent(int fd, struct kreq *r, int eofok, size_t mimesz)
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

	for (;;) {
		rc = input(&type, &kp, fd, &ke, 
			eofok, mimesz, r->keysz);
		if (rc < 0)
			goto out;
		else if (0 == rc)
			break;

		/*
		 * We have a parsed field from the child process.
		 * Begin by expanding the number of parsed fields
		 * depending on whether we have a cookie or form input.
		 * Then copy the new data.
		 */

		assert(type < IN__MAX);
		kpp = IN_COOKIE == type ?
			kpair_expand(&r->cookies, &r->cookiesz) :
			kpair_expand(&r->fields, &r->fieldsz);

		if (NULL == kpp) {
			ke = KCGI_ENOMEM;
			goto out;
		}

		*kpp = kp;
	}

	assert(0 == rc);

	/*
	 * Now that the field and cookie arrays are fixed and not going
	 * to be reallocated any more, we run through both arrays and
	 * assign the named fields into buckets.
	 * The assignment enqueues into the field.
	 */

	for (i = 0; i < r->fieldsz; i++) {
		kpp = &r->fields[i];
		if (kpp->keypos == r->keysz)
			continue;
		assert(kpp->keypos < r->keysz);

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
		assert(kpp->keypos < r->keysz);

		if (KPAIR_INVALID != kpp->state) {
			kpp->next = r->cookiemap[kpp->keypos];
			r->cookiemap[kpp->keypos] = kpp;
		} else {
			kpp->next = r->cookienmap[kpp->keypos];
			r->cookienmap[kpp->keypos] = kpp;
		}
	}

	return(KCGI_OK);
out:
	assert(KCGI_OK != ke);
	free(kp.key);
	free(kp.val);
	free(kp.file);
	free(kp.ctype);
	free(kp.xcode);
	return(ke);
}
