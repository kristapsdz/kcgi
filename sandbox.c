/*	$Id$ */
/*
 * Copyright (c) 2012, 2014, 2015 Kristaps Dzonsons <kristaps@bsd.lv>
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

#if !HAVE_PLEDGE
#if !HAVE_SECCOMP_FILTER
#if !HAVE_CAPSICUM
#if !HAVE_SANDBOX_INIT
#warning Compiling without a sandbox!?
#endif
#endif
#endif
#endif

#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include "kcgi.h"
#include "extern.h"

/*
 * Structure for a running sandbox.
 */
struct	ksandbox {
	int	 fd; /* communicator within sandbox */
	pid_t	 pid; /* pid of sandbox */
};

/*
 * Perform system-specific initialisation for the parent.
 * This is only used by systrace(4), which requires the parent to
 * register the child's systrace hooks.
 */
int
ksandbox_init_parent(void *arg, enum sandtype type, pid_t child)
{

#if HAVE_CAPSICUM
	return(1);
#elif HAVE_SANDBOX_INIT
	return(1);
#elif HAVE_PLEDGE
	return(1);
#elif 0
	if ( ! ksandbox_systrace_init_parent(arg, type, child)) {
		XWARNX("ksandbox_systrace_init_child");
		return(0);
	}
#elif HAVE_SECCOMP_FILTER
	return(1);
#endif
	return(1);
}

/*
 * Allocate system-specific data for the sandbox.
 * This is only used by systrace, which requires extra accounting
 * information for the child.
 */
int
ksandbox_alloc(void **pp)
{

	*pp = NULL;

#if HAVE_CAPSICUM
	return(1);
#elif HAVE_SANDBOX_INIT
	return(1);
#elif HAVE_PLEDGE
	return(1);
#elif 0
	if (NULL == (*pp = (ksandbox_systrace_alloc()))) {
		XWARNX("ksandbox_systrace_alloc");
		return(0);
	}
#elif HAVE_SECCOMP_FILTER
	return(1);
#endif
	return(1);
}

void
ksandbox_free(void *arg)
{
	
	/* This is either NULL of something allocated. */
	free(arg);
}

/*
 * Close the sandbox from the parent.
 */
void
ksandbox_close(void *arg)
{

#if HAVE_CAPSICUM
	return;
#elif HAVE_SANDBOX_INIT
	return;
#elif HAVE_PLEDGE
	return;
#elif 0
	ksandbox_systrace_close(arg);
#elif HAVE_SECCOMP_FILTER
	return;
#endif
}

/*
 * Initialise the child context of a sandbox.
 * Each sandbox will want to do something here to make sure that the
 * child context is sandboxed properly.
 * This function depends on "type": if SAND_WORKER, we set fd1 to be the
 * descriptor between the child and the application; if fd2 isn't -1,
 * it's the FastCGI control connection (fdfiled and fdaccept should be
 * ignored in SAND_WORKER case).
 * If not SAND_WORKER, we're the control process in a FastCGI context:
 * fd1 is the control connection; fd2 is -1; fdaccept, if not -1, is the
 * old-style FastCGI socket; fdfiled, if not -1, is the new-style
 * transport descriptor interface.
 * Whew!
 */
int
ksandbox_init_child(void *arg, 
	enum sandtype type, int fd1, int fd2,
	int fdfiled, int fdaccept)
{

#if HAVE_CAPSICUM
	if ( ! ksandbox_capsicum_init_child(arg, type, 
	    fd1, fd2, fdfiled, fdaccept)) {
		XWARNX("ksandbox_capsicum_init_child");
		return(0);
	}
#elif HAVE_SANDBOX_INIT
	if ( ! ksandbox_darwin_init_child(arg, type)) {
		XWARNX("ksandbox_darwin_init_child");
		return(0);
	}
#elif HAVE_PLEDGE
	if ( ! ksandbox_pledge_init_child(arg, type)) {
		XWARNX("ksandbox_pledge_init_child");
		return(0);
	}
#elif 0
	if ( ! ksandbox_systrace_init_child(arg, type)) {
		XWARNX("ksandbox_systrace_init_child");
		return(0);
	}
#elif HAVE_SECCOMP_FILTER
	if ( ! ksandbox_seccomp_init_child(arg, type)) {
		XWARNX("ksandbox_seccomp_init_child");
		return(0);
	}
#endif
	return(1);
}
