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

#if HAVE_SYSTRACE

#warning This feature is deprecated!

#include <sys/ioctl.h>
#include <sys/param.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <dev/systrace.h>

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "kcgi.h"
#include "extern.h"

/* $OpenBSD: sandbox-systrace.c,v 1.9 2014/01/31 16:39:19 tedu Exp $ */
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

struct systrace_preauth {
	int	  syscall;
	int	  action;
};

static const struct systrace_preauth preauth_control[] = {
	{ SYS_open, SYSTR_POLICY_NEVER },

	{ SYS_accept, SYSTR_POLICY_PERMIT },
	{ SYS_fcntl, SYSTR_POLICY_PERMIT },
	{ SYS_sendmsg, SYSTR_POLICY_PERMIT },
	{ SYS_recvmsg, SYSTR_POLICY_PERMIT },

#ifdef SYS__sysctl
	{ SYS__sysctl, SYSTR_POLICY_PERMIT },
#endif
#ifdef SYS_sysctl
	{ SYS_sysctl, SYSTR_POLICY_PERMIT },
#endif
	{ SYS_close, SYSTR_POLICY_PERMIT },
	{ SYS_exit, SYSTR_POLICY_PERMIT },
	{ SYS_getpid, SYSTR_POLICY_PERMIT },
	{ SYS_gettimeofday, SYSTR_POLICY_PERMIT },
#ifdef SYS_getentropy
	{ SYS_getentropy, SYSTR_POLICY_PERMIT },
#endif
	{ SYS_clock_gettime, SYSTR_POLICY_PERMIT },
	{ SYS_madvise, SYSTR_POLICY_PERMIT },
	{ SYS_mmap, SYSTR_POLICY_PERMIT },
	{ SYS_mprotect, SYSTR_POLICY_PERMIT },
	{ SYS_mquery, SYSTR_POLICY_PERMIT },
	{ SYS_poll, SYSTR_POLICY_PERMIT },
	{ SYS_munmap, SYSTR_POLICY_PERMIT },
	{ SYS_read, SYSTR_POLICY_PERMIT },
	{ SYS_sigprocmask, SYSTR_POLICY_PERMIT },
	{ SYS_write, SYSTR_POLICY_PERMIT },
	{ -1, -1 }
};

/* 
 * Permitted syscalls in preauth. 
 * Unlisted syscalls get SYSTR_POLICY_KILL.
 */
static const struct systrace_preauth preauth_worker[] = {
	{ SYS_open, SYSTR_POLICY_NEVER },

#ifdef SYS_sysctl
	{ SYS_sysctl, SYSTR_POLICY_PERMIT },
#endif
#ifdef SYS__sysctl
	{ SYS__sysctl, SYSTR_POLICY_PERMIT },
#endif
	{ SYS_close, SYSTR_POLICY_PERMIT },
	{ SYS_exit, SYSTR_POLICY_PERMIT },
	{ SYS_getpid, SYSTR_POLICY_PERMIT },
	{ SYS_gettimeofday, SYSTR_POLICY_PERMIT },
#ifdef SYS_getentropy
	{ SYS_getentropy, SYSTR_POLICY_PERMIT },
#endif
	{ SYS_clock_gettime, SYSTR_POLICY_PERMIT },
	{ SYS_madvise, SYSTR_POLICY_PERMIT },
	{ SYS_mmap, SYSTR_POLICY_PERMIT },
	{ SYS_mprotect, SYSTR_POLICY_PERMIT },
	{ SYS_mquery, SYSTR_POLICY_PERMIT },
	{ SYS_poll, SYSTR_POLICY_PERMIT },
	{ SYS_munmap, SYSTR_POLICY_PERMIT },
	{ SYS_read, SYSTR_POLICY_PERMIT },
	{ SYS_sigprocmask, SYSTR_POLICY_PERMIT },
	{ SYS_write, SYSTR_POLICY_PERMIT },
	{ -1, -1 }
};

void *
ksandbox_systrace_alloc(void)
{
	struct systrace_sandbox *box;

	box = XCALLOC(1, sizeof(struct systrace_sandbox));
	if (NULL == box)
		return(NULL);
	box->systrace_fd = -1;
	box->child_pid = 0;
	box->osigchld = signal(SIGCHLD, SIG_IGN);
	return(box);
}

int
ksandbox_systrace_init_child(void *arg, enum sandtype type)
{
	struct systrace_sandbox *box = arg;

	if (NULL == arg) {
		XWARNX("systrace child passed null config");
		return(0);
	}

	signal(SIGCHLD, box->osigchld);
	if (kill(getpid(), SIGSTOP) == 0) 
		return(1);

	XWARN("kill: SIGSTOP");
	return(0);
}

int
ksandbox_systrace_init_parent(void *arg, enum sandtype type, pid_t child)
{
	struct systrace_sandbox *box = arg;
	int		dev, i, j, found, st, rc;
	pid_t		pid;
	struct systrace_policy policy;
	const struct systrace_preauth *preauth;

	assert(NULL != arg);

	preauth = SAND_WORKER != type ? 
		preauth_control : preauth_worker;

	rc = 0;

	/* 
	 * Wait for the child to send itself a SIGSTOP.
	 * When we receive it, the child is waiting to be sandboxed.
	 */
	do
		pid = waitpid(child, &st, WUNTRACED);
	while (pid == -1 && errno == EINTR);

	if (-1 == pid) {
		XWARN("waitpid");
		return(0);
	}

	/* Catch if it exits. */
	signal(SIGCHLD, box->osigchld);

	if ( ! WIFSTOPPED(st)) {
		if (WIFSIGNALED(st)) {
			XWARNX("child signal %d", WTERMSIG(st));
			return(0);
		} else if (WIFEXITED(st)) {
			XWARNX("child exit %d", WEXITSTATUS(st));
			return(0);
		}
		XWARNX("child not stopped");
		return(0);
	}

	box->child_pid = child;

	/* Set up systracing of child */
	if ((dev = open("/dev/systrace", O_RDONLY)) == -1) {
		if (ENXIO == errno) {
			XWARN("open: /dev/systrace (mounted nodev?)");
			goto out;
		}
		XWARN("open: /dev/systrace");
		goto out;
	}

	if (ioctl(dev, STRIOCCLONE, &box->systrace_fd) == -1) {
		XWARN("ioctl: STRIOCCLONE");
		close(dev);
		goto out;
	}

	close(dev);
	
	if (ioctl(box->systrace_fd, STRIOCATTACH, &child) == -1) {
		XWARN("ioctl: STRIOCATTACH");
		goto out;
	}

	/* Allocate and assign policy */
	memset(&policy, 0, sizeof(policy));
	policy.strp_op = SYSTR_POLICY_NEW;
	policy.strp_maxents = SYS_MAXSYSCALL;
	if (ioctl(box->systrace_fd, STRIOCPOLICY, &policy) == -1) {
		XWARN("ioctl: STRIOCPOLICY (new)");
		goto out;
	}

	policy.strp_op = SYSTR_POLICY_ASSIGN;
	policy.strp_pid = box->child_pid;
	if (ioctl(box->systrace_fd, STRIOCPOLICY, &policy) == -1) {
		XWARN("ioctl: STRIOCPOLICY (assign)");
		goto out;
	}

	/* Set per-syscall policy */
	for (i = 0; i < SYS_MAXSYSCALL; i++) {
		found = 0;
		for (j = 0; preauth[j].syscall != -1; j++) {
			if (preauth[j].syscall == i) {
				found = 1;
				break;
			}
		}
		policy.strp_op = SYSTR_POLICY_MODIFY;
		policy.strp_code = i;
		policy.strp_policy = found ?
		    preauth[j].action : SYSTR_POLICY_KILL;
		if (ioctl(box->systrace_fd, STRIOCPOLICY, &policy) == -1) {
			XWARN("ioctl: STRIOCPOLICY (modify)");
			goto out;
		}
	}

	rc = 1;
out:
	/* Signal the child to start running */
	if (kill(box->child_pid, SIGCONT) == 0)
		return(rc);

	XWARN("kill: SIGCONT");
	return(0);
}

void
ksandbox_systrace_close(void *arg)
{
	struct systrace_sandbox *box = arg;

	/* Closing this before the child exits will terminate it */
	if (-1 != box->systrace_fd) 
		close(box->systrace_fd);
	else
		XWARNX("systrace fd not opened");
}

#else
int dummy;
#endif
