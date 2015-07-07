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

#ifndef HAVE_SYSTRACE
#ifndef HAVE_SECCOMP_FILTER
#ifndef HAVE_CAPSICUM
#ifndef HAVE_SANDBOX_INIT
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
void
ksandbox_init_parent(void *arg, pid_t child)
{

#if defined(HAVE_SYSTRACE)
	if ( ! ksandbox_systrace_init_parent(arg, child))
		XWARNX("systrace sandbox failed (parent)");
#endif
}

/*
 * Allocate system-specific data for the sandbox.
 * This is only used by systrace, which requires extra accounting
 * information for the child.
 */
void *
ksandbox_alloc(void)
{
	void	*p = NULL;

#ifdef HAVE_SYSTRACE
	if (NULL == (p = (ksandbox_systrace_alloc())))
		XWARNX("systrace alloc failed");
#endif
	return(p);
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

	/* Run system-specific closure stuff. */
#ifdef HAVE_SYSTRACE
	ksandbox_systrace_close(arg);
#endif
}

/*
 * Initialise the child context of a sandbox.
 * Each sandbox will want to do something here to make sure that the
 * child context is sandboxed properly.
 */
void
ksandbox_init_child(void *arg, int fd)
{

#if defined(HAVE_CAPSICUM)
	if ( ! ksandbox_capsicum_init_child(arg, fd))
		XWARNX("capsicum sandbox failed (child)");
#elif defined(HAVE_SANDBOX_INIT)
	if ( ! ksandbox_darwin_init_child(arg))
		XWARNX("darwin sandbox failed (child)");
#elif defined(HAVE_SYSTRACE)
	if ( ! ksandbox_systrace_init_child(arg))
		XWARNX("systrace sandbox failed (child)");
#endif
}
