/*	$Id$ */
/*
 * Copyright (c) 2015 Kristaps Dzonsons <kristaps@bsd.lv>
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

#if HAVE_SECCOMP_FILTER

#ifndef SECCOMP_AUDIT_ARCH
#error Unknown seccomp arch: please contact us with your uname -m
#endif 

/*
 * Copyright (c) 2012 Will Drewry <wad@dataspill.org>
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

#include <sys/types.h>
#include <sys/resource.h>
#include <sys/prctl.h>

#include <linux/audit.h>
#include <linux/filter.h>
#include <linux/seccomp.h>
#include <elf.h>

#ifdef AUDIT_ARCH_AARCH64
#define __ARCH_WANT_SYSCALL_NO_AT
#define __ARCH_WANT_SYSCALL_DEPRECATED
#endif
#include <asm/unistd.h>

#include <errno.h>
#include <signal.h>
#include <stdarg.h>
#include <stddef.h>  /* for offsetof */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "kcgi.h"
#include "extern.h"

/* Linux seccomp_filter sandbox */
#ifdef SANDBOX_SECCOMP_DEBUG
# define SECCOMP_FILTER_FAIL SECCOMP_RET_TRAP
#else
# define SECCOMP_FILTER_FAIL SECCOMP_RET_KILL
#endif

/* Simple helpers to avoid manual errors (but larger BPF programs). */
#define SC_DENY(_nr, _errno) \
	BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_K, __NR_ ## _nr, 0, 1), \
	BPF_STMT(BPF_RET+BPF_K, SECCOMP_RET_ERRNO|(_errno))
#define SC_ALLOW(_nr) \
	BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_K, __NR_ ## _nr, 0, 1), \
	BPF_STMT(BPF_RET+BPF_K, SECCOMP_RET_ALLOW)

static const struct sock_filter preauth_ctrl[] = {
	/* Ensure the syscall arch convention is as expected. */
	BPF_STMT(BPF_LD+BPF_W+BPF_ABS,
		offsetof(struct seccomp_data, arch)),
	BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_K, SECCOMP_AUDIT_ARCH, 1, 0),
	BPF_STMT(BPF_RET+BPF_K, SECCOMP_FILTER_FAIL),
	/* Load the syscall number for checking. */
	BPF_STMT(BPF_LD+BPF_W+BPF_ABS,
		offsetof(struct seccomp_data, nr)),
	SC_DENY(open, EACCES),
	SC_ALLOW(getpid),
	SC_ALLOW(gettimeofday),
	SC_ALLOW(clock_gettime),
#ifdef __NR_time /* not defined on EABI ARM */
	SC_ALLOW(time),
#endif
#ifdef __NR_accept /* not defined for __i386__ (linux) */
	SC_ALLOW(accept),
#endif
	SC_ALLOW(fcntl),
#ifdef __NR_fcntl64 /* only noted on arm */
	SC_ALLOW(fcntl64),
#endif
#ifdef __NR_sendmsg /* not defined for __i386__ (linux) */
	SC_ALLOW(sendmsg),
#endif
#ifdef __NR_recvmsg /* XXX: untested: mirroring __NR_sendmsg */
	SC_ALLOW(recvmsg),
#endif
	SC_ALLOW(read),
	SC_ALLOW(readv),
	SC_ALLOW(write),
	SC_ALLOW(writev),
	SC_ALLOW(close),
#ifdef __NR_shutdown /* not defined on archs that go via socketcall(2) */
	SC_ALLOW(shutdown),
#endif
	SC_ALLOW(brk),
#ifdef __NR_ppoll
	SC_ALLOW(ppoll),
#endif
	SC_ALLOW(poll),
#ifdef __NR__newselect
	SC_ALLOW(_newselect),
#else
	SC_ALLOW(select),
#endif
	SC_ALLOW(madvise),
#ifdef __NR_mmap2 /* EABI ARM only has mmap2() */
	SC_ALLOW(mmap2),
#endif
#ifdef __NR_mmap
	SC_ALLOW(mmap),
#endif
	SC_ALLOW(munmap),
	SC_ALLOW(exit_group),
#ifdef __NR_rt_sigprocmask
	SC_ALLOW(rt_sigprocmask),
#else
	SC_ALLOW(sigprocmask),
#endif
#ifdef __NR_rt_sigaction
	SC_ALLOW(rt_sigaction),
#else
	SC_ALLOW(sigaction),
#endif
	BPF_STMT(BPF_RET+BPF_K, SECCOMP_FILTER_FAIL),
};

/* Syscall filtering set for preauth. */
static const struct sock_filter preauth_work[] = {
	/* Ensure the syscall arch convention is as expected. */
	BPF_STMT(BPF_LD+BPF_W+BPF_ABS,
		offsetof(struct seccomp_data, arch)),
	BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_K, SECCOMP_AUDIT_ARCH, 1, 0),
	BPF_STMT(BPF_RET+BPF_K, SECCOMP_FILTER_FAIL),
	/* Load the syscall number for checking. */
	BPF_STMT(BPF_LD+BPF_W+BPF_ABS,
		offsetof(struct seccomp_data, nr)),
	SC_DENY(open, EACCES),
	SC_ALLOW(getpid),
#ifdef __NR_getrandom
	SC_ALLOW(getrandom),
#endif
	SC_ALLOW(gettimeofday),
	SC_ALLOW(clock_gettime),
#ifdef __NR_time /* not defined on EABI ARM */
	SC_ALLOW(time),
#endif
	SC_ALLOW(read),
	SC_ALLOW(readv),
	SC_ALLOW(lseek), /* for kutil_openlog logging */
	SC_ALLOW(fstat), /* for kutil_openlog logging */
	SC_ALLOW(write),
	SC_ALLOW(writev),
	SC_ALLOW(close),
#ifdef __NR_fcntl64 /* only noted on arm */
	SC_ALLOW(fcntl64),
#endif
#ifdef __NR_shutdown /* not defined on archs that go via socketcall(2) */
	SC_ALLOW(shutdown),
#endif
	SC_ALLOW(brk),
#ifdef __NR_ppoll
	SC_ALLOW(ppoll),
#endif
	SC_ALLOW(poll),
#ifdef __NR__newselect
	SC_ALLOW(_newselect),
#else
	SC_ALLOW(select),
#endif
	SC_ALLOW(madvise),
#ifdef __NR_mmap2 /* EABI ARM only has mmap2() */
	SC_ALLOW(mmap2),
#endif
#ifdef __NR_mmap
	SC_ALLOW(mmap),
#endif
	SC_ALLOW(mremap),
	SC_ALLOW(munmap),
	SC_ALLOW(exit_group),
#ifdef __NR_rt_sigprocmask
	SC_ALLOW(rt_sigprocmask),
#else
	SC_ALLOW(sigprocmask),
#endif
	BPF_STMT(BPF_RET+BPF_K, SECCOMP_FILTER_FAIL),
};

static const struct sock_fprog preauth_prog_work = {
	.len = (unsigned short)(sizeof(preauth_work)/sizeof(preauth_work[0])),
	.filter = (struct sock_filter *)preauth_work,
};

static const struct sock_fprog preauth_prog_ctrl = {
	.len = (unsigned short)(sizeof(preauth_ctrl)/sizeof(preauth_ctrl[0])),
	.filter = (struct sock_filter *)preauth_ctrl,
};

#ifdef SANDBOX_SECCOMP_DEBUG
static void
ssh_sandbox_violation_control(int signum, 
	siginfo_t *info, void *void_context)
{

	fprintf(stderr, 
		"%s: unexpected system call (control) "
		"(arch:0x%x,syscall:%d @ %p)\n",
		__func__, info->si_arch, 
		info->si_syscall, info->si_call_addr);
	exit(1);
}

static void
ssh_sandbox_violation_worker(int signum, 
	siginfo_t *info, void *void_context)
{

	fprintf(stderr, 
		"%s: unexpected system call (worker) "
		"(arch:0x%x,syscall:%d @ %p)\n",
		__func__, info->si_arch, 
		info->si_syscall, info->si_call_addr);
	exit(1);
}

static void
ssh_sandbox_child_debugging(enum sandtype type)
{
	struct sigaction act;
	sigset_t mask;

	memset(&act, 0, sizeof(act));
	sigemptyset(&mask);
	sigaddset(&mask, SIGSYS);

	act.sa_sigaction = SAND_WORKER == type ?
		&ssh_sandbox_violation_worker :
		&ssh_sandbox_violation_control;
	act.sa_flags = SA_SIGINFO;
	if (sigaction(SIGSYS, &act, NULL) == -1)
		fprintf(stderr, "%s: sigaction(SIGSYS): %s\n", 
			__func__, strerror(errno));
	if (sigprocmask(SIG_UNBLOCK, &mask, NULL) == -1)
		fprintf(stderr, "%s: sigprocmask(SIGSYS): %s\n",
			__func__, strerror(errno));
}
#endif /* SANDBOX_SECCOMP_DEBUG */

int
ksandbox_seccomp_init_child(enum sandtype type)
{
	struct rlimit	rl_zero;
	int 		nnp_failed = 0;

	rl_zero.rlim_cur = rl_zero.rlim_max = 0;
#if 0
	/*
	 * Don't do this: we might write into a kutil_openlog(3).
	 */
	if (setrlimit(RLIMIT_FSIZE, &rl_zero) == -1)
		kutil_warn(NULL, NULL, "setrlimit");

	/*
	 * Don't do like OpenSSH: we need to pass stuff back and forth
	 * over pipes, and this will prevent that from happening.
	 */
	if (setrlimit(RLIMIT_NOFILE, &rl_zero) == -1)
		kutil_warn(NULL, NULL, "setrlimit");
#endif

	if (setrlimit(RLIMIT_NPROC, &rl_zero) == -1)
		kutil_warn(NULL, NULL, "setrlimit");

#ifdef SANDBOX_SECCOMP_DEBUG
	ssh_sandbox_child_debugging(type);
#endif 

	if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) == -1) {
		kutil_warn(NULL, NULL, "prctl");
		nnp_failed = 1;
	}

	if (prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, 
	    SAND_WORKER != type ?
	    &preauth_prog_ctrl : &preauth_prog_work) == -1)
		kutil_warn(NULL, NULL, "prctl");
	else if (nnp_failed) {
		kutil_warnx(NULL, NULL, "SECCOMP_MODE_FILTER "
		    "activated but PR_SET_NO_NEW_PRIVS failed");
		_exit(EXIT_FAILURE);
	}

	return 1;
}

#endif
