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
#include <sys/wait.h>

#include <errno.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>

#include "kcgi.h"
#include "extern.h"

void
ksandbox_init_parent(void *arg, pid_t child)
{
#if defined(HAVE_SYSTRACE)
	if ( ! ksandbox_systrace_init_parent(arg, child))
		XWARNX("systrace sandbox failed (parent)");
#endif
}

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

void
ksandbox_close(void *arg, pid_t pid)
{
	pid_t	 rp;
	int	 st;

	/* First wait til our child exits. */
	do
		rp = waitpid(pid, &st, 0);
	while (rp == -1 && errno == EINTR);

	if (-1 == rp)
		XWARN("waiting for child");
	else if (WIFEXITED(st) && EXIT_SUCCESS != WEXITSTATUS(st))
		XWARNX("child status %d", WEXITSTATUS(st));
	else if (WIFSIGNALED(st))
		XWARNX("child signal %d", WTERMSIG(st));

	/* Now run system-specific closure stuff. */
#ifdef HAVE_SYSTRACE
	ksandbox_systrace_close(arg);
#endif
}

void
ksandbox_init_child(void *arg)
{
#if defined(HAVE_SANDBOX_INIT)
	if ( ! ksandbox_darwin_init_child(arg))
		XWARNX("darwin sandbox failed (child)");
#elif defined(HAVE_SYSTRACE)
	if ( ! ksandbox_systrace_init_child(arg))
		XWARNX("systrace sandbox failed (child)");
#endif
}
