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
#include <sys/resource.h>

#include <stdio.h>
#include <stdlib.h>
#ifdef HAVE_SANDBOX_INIT
#include <sandbox.h>
#endif

#include "kcgi.h"
#include "extern.h"

#ifdef HAVE_SANDBOX_INIT
static int
ksandbox_sandbox_init(void)
{
	int	 rc;
	char	*er;

	rc = sandbox_init
		(kSBXProfilePureComputation, 
		 SANDBOX_NAMED, &er);
	if (0 == rc)
		return(1);
	perror(er);
	sandbox_free_error(er);
	return(0);
}
#endif

/* $OpenBSD: sandbox-rlimit.c,v 1.3 2011/06/23 09:34:13 djm Exp $ */
/*
 * Copyright (c) 2011 Damien Miller <djm@mindrot.org>
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
static int
ksandbox_rlimit_init(void)
{
	struct rlimit rl_zero;

	rl_zero.rlim_cur = rl_zero.rlim_max = 0;

	if (-1 == setrlimit(RLIMIT_FSIZE, &rl_zero)) {
		perror("setrlimit-fsize");
		return(0);
#if 0
	} else if (-1 == setrlimit(RLIMIT_NOFILE, &rl_few)) {
		perror("setrlimit-nofile");
		return(0);
#endif
	} else if (-1 == setrlimit(RLIMIT_NPROC, &rl_zero)) {
		perror("setrlimit-nproc");
		return(0);
	}

	return(1);
}

int
ksandbox_init(void)
{
	/*
	 * First, try to do our system-specific methods.
	 * If those fail, then fall back to setrlimit.
	 * If even that fails, then we're not operating in sandbox mode
	 * at all.
	 */
#ifdef	HAVE_SANDBOX_INIT
	(void)ksandbox_sandbox_init();
#endif
	return(ksandbox_rlimit_init());
}
