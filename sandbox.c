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
#ifdef HAVE_SYSTRACE
#include <sys/param.h>
#include <sys/syscall.h>
#include <dev/systrace.h>
#endif

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#ifdef HAVE_SANDBOX_INIT
#include <sandbox.h>
#endif

#include "kcgi.h"
#include "extern.h"

#ifdef HAVE_SYSTRACE
struct sandbox_policy {
	int syscall;
	int action;
};

/* 
 * Permitted syscalls in preauth. 
 * Unlisted syscalls get SYSTR_POLICY_KILL.
 */
static const struct sandbox_policy preauth_policy[] = {
	{ SYS_open, SYSTR_POLICY_NEVER },

	{ SYS___sysctl, SYSTR_POLICY_PERMIT },
	{ SYS_close, SYSTR_POLICY_PERMIT },
	{ SYS_exit, SYSTR_POLICY_PERMIT },
	{ SYS_getpid, SYSTR_POLICY_PERMIT },
	{ SYS_gettimeofday, SYSTR_POLICY_PERMIT },
	{ SYS_clock_gettime, SYSTR_POLICY_PERMIT },
	{ SYS_madvise, SYSTR_POLICY_PERMIT },
	{ SYS_mmap, SYSTR_POLICY_PERMIT },
	{ SYS_mprotect, SYSTR_POLICY_PERMIT },
	{ SYS_mquery, SYSTR_POLICY_PERMIT },
	{ SYS_poll, SYSTR_POLICY_PERMIT },
	{ SYS_munmap, SYSTR_POLICY_PERMIT },
	{ SYS_read, SYSTR_POLICY_PERMIT },
	{ SYS_select, SYSTR_POLICY_PERMIT },
	{ SYS_shutdown, SYSTR_POLICY_PERMIT },
	{ SYS_sigprocmask, SYSTR_POLICY_PERMIT },
	{ SYS_write, SYSTR_POLICY_PERMIT },
	{ -1, -1 }
};

static void
ksandbox_systrace_init_child(void)
{

}

static int
ksandbox_systrace_init_parent(pid_t child)
{

	return(1);
}
#endif

#ifdef HAVE_SANDBOX_INIT
static void
ksandbox_sandbox_init(void)
{
	int	 rc;
	char	*er;

	rc = sandbox_init
		(kSBXProfilePureComputation, 
		 SANDBOX_NAMED, &er);
	if (0 == rc)
		return;
	perror(er);
	sandbox_free_error(er);
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
	/*
	 * FIXME: I've taken out the RLIMIT_NOFILE setrlimit() because
	 * it causes strange behaviour.  On Mac OS X, it fails with
	 * EPERM no matter what (the same code runs fine when not run as
	 * a CGI instance), while on OpenBSD, failures occur later on.
	 */
	} else if (-1 == setrlimit(RLIMIT_NOFILE, &rl_zero)) {
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
ksandbox_init_parent(pid_t child)
{

#if defined(HAVE_SYSTRACE)
	return(ksandbox_systrace_init_parent(child));
#endif
	return(1);
}

int
ksandbox_init_child(void)
{
	/*
	 * First, try to do our system-specific methods.
	 * If those fail (or either way, really, then fall back to
	 * setrlimit.
	 */
#if defined(HAVE_SANDBOX_INIT)
	ksandbox_sandbox_init();
#elif defined(HAVE_SYSTRACE)
	ksandbox_systrace_init_child();
#endif
	return(ksandbox_rlimit_init());
}
