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
#include "config.h"

#if HAVE_SANDBOX_INIT

#include <sys/resource.h>

#include <sandbox.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>

#include "kcgi.h"
#include "extern.h"

int
ksandbox_darwin_init_child(enum sandtype type)
{
	int	 	 rc;
	char		*er;
	struct rlimit	 rl_zero;

	rc = type == SAND_WORKER ?
		sandbox_init(kSBXProfilePureComputation, 
			SANDBOX_NAMED, &er) :
		sandbox_init(kSBXProfileNoWrite, 
			SANDBOX_NAMED, &er);

	if (rc != 0) {
		kutil_warn(NULL, NULL, "sandbox_init: %s", er);
		sandbox_free_error(er);
		rc = 0;
	} else
		rc = 1;

	rl_zero.rlim_cur = rl_zero.rlim_max = 0;

#if 0
	/*
	 * This doesn't play with kutil_openlog.
	 */
	if (setrlimit(RLIMIT_FSIZE, &rl_zero) == -1)
		kutil_warn(NULL, NULL, "setrlimit");

	/*
	 * FIXME: I've taken out the RLIMIT_NOFILE setrlimit() because
	 * it causes strange behaviour.  On Mac OS X, it fails with
	 * EPERM no matter what (the same code runs fine when not run as
	 * a CGI instance).
	 */
	if (setrlimit(RLIMIT_NOFILE, &rl_zero) == -1)
		kutil_warn(NULL, NULL, "setrlimit");
#endif
	if (setrlimit(RLIMIT_NPROC, &rl_zero) == -1)
		kutil_warn(NULL, NULL, "setrlimit");

	return rc;
}

#endif
