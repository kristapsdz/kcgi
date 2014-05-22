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

#include <errno.h>
#ifdef HAVE_SANDBOX_INIT
#include <sandbox.h>
#endif
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "kcgi.h"
#include "extern.h"

#ifdef HAVE_SYSTRACE
/* $OpenBSD$ */
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
struct systrace_sandbox {
	int	  systrace_fd;
	pid_t	  child_pid;
	void 	(*osigchld)(int);
};

struct systrace_policy {
	int	  syscall;
	int	  action;
};

/* 
 * Permitted syscalls in preauth. 
 * Unlisted syscalls get SYSTR_POLICY_KILL.
 */
static const struct systrace_policy preauth_policy[] = {
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

static void *
ksandbox_systrace_alloc(void)
{
	struct systrace_sandbox *box;

	box = calloc(1, sizeof(struct systrace_sandbox));
	if (NULL == box) {
		fprintf(stderr, "%s:%d: calloc(1, %zu)\n",
			__FILE__, __LINE__, 
			sizeof(struct systrace_sandbox));
		return(NULL);
	}
	box->systrace_fd = -1;
	box->child_pid = 0;
	box->osigchld = signal(SIGCHLD, SIG_IGN);
	return(box);
}

static int
ksandbox_systrace_init_child(void *arg)
{
	struct systrace_sandbox *box = arg;

	if (NULL -= arg) {
		fprintf(stderr, "%s:%d: systrace child "
			"passed null config\n", __FILE__, __LINE__);
		return(0);
	}

	signal(SIGCHLD, box->osigchld);
	if (kill(getpid(), SIGSTOP) == 0) 
		return(1);

	fprintf(stderr, "%s:%d: sigstop: %s\n", 
		__FILE__, __LINE__, strerror(errno));
	_exit(EXIT_FAILURE);
}

static int
ksandbox_systrace_init_parent(void *arg, pid_t child)
{
	struct systrace_sandbox *box = arg;
	int		dev, i, j, found, status;
	pid_t		pid;
	struct systrace_policy policy;

	if (NULL == arg) {
		fprintf(stderr, "%s:%d: systrace parent "
			"passed null config\n", __FILE__, __LINE__);
		return(0);
	}

	/* Wait for the child to send itself a SIGSTOP */
	do {
		pid = waitpid(child_pid, &status, WUNTRACED);
	} while (pid == -1 && errno == EINTR);

	signal(SIGCHLD, box->osigchld);

	if (!WIFSTOPPED(status)) {
		if (WIFSIGNALED(status)) {
			fprintf(stderr, "%s:%d: child "
				"stopped with signal %d\n", 
				__FILE__, __LINE__, WTERMSIG(status));
			exit(EXIT_FAILURE);
		}
		if (WIFEXITED(status)) {
			fprintf(stderr, "%s:%d: child "
				"stopped with status %d\n", 
				__FILE__, __LINE__, WEXITSTATUS(status));
			exit(EXIT_FAILURE);
		}
		fprintf(stderr, "%s:%d: child "
			"not stopped\n", __FILE__, __LINE__);
		return(0);
		exit(EXIT_FAILURE);
	}

	box->child_pid = child_pid;

	/* Set up systracing of child */
	if ((dev = open("/dev/systrace", O_RDONLY)) == -1) {
		fprintf(stderr, "%s:%d open(\"/dev/systrace\"): %s\n", 
			__FILE__, __LINE__, strerror(errno));
		exit(EXIT_FAILURE);
	}

	if (ioctl(dev, STRIOCCLONE, &box->systrace_fd) == -1) {
		fprintf(stderr, "%s:%d: ioctl(STRIOCCLONE, %d): %s\n", 
			__FILE__, __LINE__, dev, strerror(errno));
		exit(EXIT_FAILURE);
	}

	close(dev);
	
	if (ioctl(box->systrace_fd, STRIOCATTACH, &child_pid) == -1) {
		fprintf(stderr, "%s:%d: ioctl(%d, STRIOCATTACH, "
			"%d): %s\n", __FILE__, __LINE__, box->systrace_fd, 
			child_pid, strerror(errno));
		exit(EXIT_FAILURE);
	}

	/* Allocate and assign policy */
	memset(&policy, 0, sizeof(policy));
	policy.strp_op = SYSTR_POLICY_NEW;
	policy.strp_maxents = SYS_MAXSYSCALL;
	if (ioctl(box->systrace_fd, STRIOCPOLICY, &policy) == -1) {
		fprintf(stderr, "%s:%d: ioctl(%d, STRIOCPOLICY "
			"(new)): %s\n", __FILE__, __LINE__,
			box->systrace_fd, strerror(errno));
		exit(EXIT_FAILURE);
	}

	policy.strp_op = SYSTR_POLICY_ASSIGN;
	policy.strp_pid = box->child_pid;
	if (ioctl(box->systrace_fd, STRIOCPOLICY, &policy) == -1) {
		fprintf(stderr, "%s:%d: ioctl(%d, STRIOCPOLICY "
			"(assign)): %s\n", __FILE__, __LINE__,
			box->systrace_fd, strerror(errno));
		exit(EXIT_FAILURE);
	}

	/* Set per-syscall policy */
	for (i = 0; i < SYS_MAXSYSCALL; i++) {
		found = 0;
		for (j = 0; allowed_syscalls[j].syscall != -1; j++) {
			if (allowed_syscalls[j].syscall == i) {
				found = 1;
				break;
			}
		}
		policy.strp_op = SYSTR_POLICY_MODIFY;
		policy.strp_code = i;
		policy.strp_policy = found ?
		    allowed_syscalls[j].action : SYSTR_POLICY_KILL;
		if (ioctl(box->systrace_fd, STRIOCPOLICY, &policy) == -1) {
			fprintf(stderr, "%s:%d: ioctl(%d, STRIOCPOLICY "
				"(modify)): %s\n", __FILE__, __LINE__,
				box->systrace_fd, strerror(errno));
			exit(EXIT_FAILURE);
		}
	}

	/* Signal the child to start running */
	if (kill(box->child_pid, SIGCONT) == 0)
		return(1);

	fprintf(stderr, "%s:%d: kill(%d, SIGCONT)\n", 
		__FILE__, __LINE__, box->child_pid);
	exit(EXIT_FAILURE);
}

static void
ksandbox_systrace_close(void *arg)
{
	struct systrace_sandbox *box = arg;

	/* Closing this before the child exits will terminate it */
	close(box->systrace_fd);
}
#endif

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
	fprintf(stderr, "%s:%d: sandbox_init: %s\n",
		__FILE__, __LINE__, er);
	sandbox_free_error(er);
	return(0);
}
#endif

/* $OpenBSD$ */
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
		fprintf(stderr, "%s:%d: setrlimit-fsize: %s\n", 
			__FILE__, __LINE__, strerror(errno));
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
		fprintf(stderr, "%s:%d: setrlimit-nproc: %s\n", 
			__FILE__, __LINE__, strerror(errno));
		return(0);
	}

	return(1);
}

void
ksandbox_init_parent(void *arg, pid_t child)
{

#if defined(HAVE_SYSTRACE)
	if ( ! ksandbox_systrace_init_parent(arg, child))
		fprintf(stderr, "%s:%d: systrace sandbox "
			"failed (parent)\n", __FILE__, __LINE__);
#endif
}

void *
ksandbox_alloc(void)
{
	void	*p;

	p = NULL;
#ifdef HAVE_SYSTRACE
	if (NULL == (p = (ksandbox_systrace_alloc())))
		fprintf(stderr, "%s:%d: systrace alloc "
			"failed\n", __FILE__, __LINE__);
#endif
	return(p);
}

void
ksandbox_free(void *arg)
{
	
	free(arg);
}

void
ksandbox_close(void *arg, pid_t pid)
{

	waitpid(pid, NULL, 0);
#ifdef HAVE_SYSTRACE
	ksandbox_systrace_close(arg);
#endif
}

void
ksandbox_init_child(void *arg)
{
	/*
	 * First, try to do our system-specific methods.
	 * If those fail (or either way, really, then fall back to
	 * setrlimit.
	 */
#if defined(HAVE_SANDBOX_INIT)
	if ( ! ksandbox_sandbox_init())
		fprintf(stderr, "%s:%d: darwin sandbox "
			"failed (child)\n", __FILE__, __LINE__);
#elif defined(HAVE_SYSTRACE)
	if ( ! ksandbox_systrace_init_child(arg))
		fprintf(stderr, "%s:%d: systrace sandbox "
			"failed (child)\n", __FILE__, __LINE__);
#endif
	if ( ! ksandbox_rlimit_init())
		fprintf(stderr, "%s:%d: rlimit sandbox "
			"failed (child)\n", __FILE__, __LINE__);
}
