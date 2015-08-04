/*	$Id$ */
/*
 * Copyright (c) 2014 Baptiste Daroussin <bapt@freebsd.org>
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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_CAPSICUM

#include <sys/resource.h>
#include <sys/capability.h>

#include <unistd.h>
#include <errno.h>
#include <stdarg.h>

#include "kcgi.h"
#include "extern.h"

static int
ksandbox_capsicum_init_control(void *arg, int fd1, int fd2)
{
	int rc;
	struct rlimit	 rl_zero;
	cap_rights_t	 rights;

	cap_rights_init(&rights);

	cap_rights_init(&rights, CAP_EVENT, CAP_FCNTL, CAP_ACCEPT);
	if (cap_rights_limit(STDIN_FILENO, &rights) < 0 && 
		 errno != ENOSYS) {
 		XWARN("cap_rights_limit: STDIN_FILENO");
		return(0);
	}

	cap_rights_init(&rights, CAP_WRITE, CAP_FSTAT);
	if (cap_rights_limit(STDERR_FILENO, &rights) < 0 && 
		 errno != ENOSYS) {
		XWARN("cap_rights_limit: STDERR_FILENO");
		return(0);
	}

	cap_rights_init(&rights, CAP_EVENT, 
		CAP_FCNTL, CAP_READ, CAP_WRITE);
	if (cap_rights_limit(fd1, &rights) < 0 && 
		 errno != ENOSYS) {
		XWARN("cap_rights_limit: internal socket");
		return(0);
	}

	rl_zero.rlim_cur = rl_zero.rlim_max = 0;

	if (-1 == setrlimit(RLIMIT_FSIZE, &rl_zero)) {
		XWARNX("setrlimit: rlimit_fsize");
		return(0);
	} else if (-1 == setrlimit(RLIMIT_NPROC, &rl_zero)) {
		XWARNX("setrlimit: rlimit_nproc");
		return(0);
	}

	rc = cap_enter();
	if (0 != rc && errno != ENOSYS) {
		XWARN("cap_enter");
		rc = 0;
	} else
		rc = 1;

	return(rc);
}

static int
ksandbox_capsicum_init_worker(void *arg, int fd1, int fd2)
{
	int rc;
	struct rlimit	 rl_zero;
	cap_rights_t	 rights;

	cap_rights_init(&rights);

	/* 
	 * Test for EBADF because STDIN_FILENO is usually closed by the
	 * caller.
	 */
	cap_rights_init(&rights, CAP_EVENT, CAP_READ, CAP_FSTAT);
	if (cap_rights_limit(STDIN_FILENO, &rights) < 0 && 
		 errno != ENOSYS && errno != EBADF) {
 		XWARN("cap_rights_limit: STDIN_FILENO");
		return(0);
	}

	cap_rights_init(&rights, CAP_EVENT, CAP_WRITE, CAP_FSTAT);
	if (cap_rights_limit(STDERR_FILENO, &rights) < 0 && 
		 errno != ENOSYS) {
		XWARN("cap_rights_limit: STDERR_FILENO");
		return(0);
	}

	cap_rights_init(&rights, CAP_EVENT, CAP_READ, CAP_WRITE, CAP_FSTAT);
	if (-1 != fd1 && cap_rights_limit(fd1, &rights) < 0 && 
		 errno != ENOSYS) {
		XWARN("cap_rights_limit: internal socket");
		return(0);
	}
	if (-1 != fd2 && cap_rights_limit(fd2, &rights) < 0 && 
		 errno != ENOSYS) {
		XWARN("cap_rights_limit: internal socket");
		return(0);
	}

	rl_zero.rlim_cur = rl_zero.rlim_max = 0;

	if (-1 == setrlimit(RLIMIT_NOFILE, &rl_zero)) {
		XWARNX("setrlimit: rlimit_fsize");
		return(0);
	} else if (-1 == setrlimit(RLIMIT_FSIZE, &rl_zero)) {
		XWARNX("setrlimit: rlimit_fsize");
		return(0);
	} else if (-1 == setrlimit(RLIMIT_NPROC, &rl_zero)) {
		XWARNX("setrlimit: rlimit_nproc");
		return(0);
	}

	rc = cap_enter();
	if (0 != rc && errno != ENOSYS) {
		XWARN("cap_enter");
		rc = 0;
	} else
		rc = 1;

	return(rc);
}

int
ksandbox_capsicum_init_child(void *arg, 
	enum sandtype type, int fd1, int fd2)
{

	return(SAND_WORKER == type ?
		ksandbox_capsicum_init_worker(arg, fd1, fd2) :
		ksandbox_capsicum_init_control(arg, fd1, fd2));
}

#else
int dummy;
#endif
