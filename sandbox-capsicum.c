/*	$Id$ */
/*
 * Copyright (c) 2014 Baptiste Daroussin <bapt@freebsd.org>
 * Copyright (c) 2015--2016 Kristaps Dzonsons <kristaps@bsd.lv>
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

#if HAVE_CAPSICUM

#include <sys/resource.h>
#include <sys/capsicum.h>

#include <assert.h>
#include <unistd.h>
#include <errno.h>
#include <stdarg.h>
#include <stdlib.h>

#include "kcgi.h"
#include "extern.h"

static int
ksandbox_capsicum_init_control(int worker, int fdfiled, int fdaccept)
{
	int		 rc;
	struct rlimit	 rl_zero;
	cap_rights_t	 rights;

	cap_rights_init(&rights);

	/*
	 * If we have old-style accept FastCGI sockets, then mark us as
	 * accepting on it.
	 * XXX: the CAP_READ and CAP_WRITE are necessary because they're
	 * required by descriptors we create from the accept().
	 * The new-style passing of descriptors is easier.
	 */

	if (fdaccept != -1) {
		cap_rights_init(&rights, 
			CAP_EVENT, CAP_FCNTL, CAP_ACCEPT, 
			CAP_READ, CAP_WRITE);
		if (cap_rights_limit(fdaccept, &rights) < 0 && 
			 errno != ENOSYS) {
			kutil_warn(NULL, NULL, "cap_rights_limit");
			return 0;
		}
	} else {
		assert(fdfiled != -1);
		cap_rights_init(&rights, CAP_EVENT, 
			CAP_FCNTL, CAP_READ, CAP_WRITE);
		if (cap_rights_limit(fdfiled, &rights) < 0 && 
			 errno != ENOSYS) {
			kutil_warn(NULL, NULL, "cap_rights_limit");
			return 0;
		}
	}

	/* Always pass through write-only stderr. */

	cap_rights_init(&rights, CAP_WRITE, CAP_FSTAT);
	if (cap_rights_limit(STDERR_FILENO, &rights) < 0 && 
		 errno != ENOSYS) {
		kutil_warn(NULL, NULL, "cap_rights_limit");
		return 0;
	}

	/* Interface to worker. */

	cap_rights_init(&rights, CAP_EVENT, 
		CAP_FCNTL, CAP_READ, CAP_WRITE);
	if (cap_rights_limit(worker, &rights) < 0 && 
		 errno != ENOSYS) {
		kutil_warn(NULL, NULL, "cap_rights_limit");
		return 0;
	}

	rl_zero.rlim_cur = rl_zero.rlim_max = 0;
	if (setrlimit(RLIMIT_FSIZE, &rl_zero) == -1) {
		kutil_warn(NULL, NULL, "setrlimit");
		return 0;
	} else if (setrlimit(RLIMIT_NPROC, &rl_zero) == -1) {
		kutil_warn(NULL, NULL, "setrlimit");
		return 0;
	}

	rc = cap_enter();
	if (rc && errno != ENOSYS) {
		kutil_warn(NULL, NULL, "cap_enter");
		rc = 0;
	} else
		rc = 1;

	return rc;
}

static int
ksandbox_capsicum_init_worker(int fd1, int fd2)
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
 		kutil_warn(NULL, NULL, "cap_rights_limit");
		return 0;
	}

	cap_rights_init(&rights, CAP_EVENT, CAP_WRITE, CAP_FSTAT);
	if (cap_rights_limit(STDERR_FILENO, &rights) < 0 && 
		 errno != ENOSYS) {
		kutil_warn(NULL, NULL, "cap_rights_limit");
		return 0;
	}

	/* Only do thesee if the descriptors are valid. */

	cap_rights_init(&rights, CAP_EVENT, CAP_READ, CAP_WRITE, CAP_FSTAT);
	if (fd1 != -1 && cap_rights_limit(fd1, &rights) < 0 && 
		 errno != ENOSYS) {
		kutil_warn(NULL, NULL, "cap_rights_limit");
		return 0;
	}
	if (fd2 != -1 && cap_rights_limit(fd2, &rights) < 0 && 
		 errno != ENOSYS) {
		kutil_warn(NULL, NULL, "cap_rights_limit");
		return 0;
	}

	rl_zero.rlim_cur = rl_zero.rlim_max = 0;

#if 0
	/* Don't run this: if we use openlog, it will fail. */

	if (setrlimit(RLIMIT_FSIZE, &rl_zero) == -1) {
		kutil_warn(NULL, NULL, "setrlimit");
		return 0;
	}
#endif

	if (setrlimit(RLIMIT_NOFILE, &rl_zero) == -1) {
		kutil_warn(NULL, NULL, "setrlimit");
		return 0;
	} else if (setrlimit(RLIMIT_NPROC, &rl_zero) == -1) {
		kutil_warn(NULL, NULL, "setrlimit");
		return 0;
	}

	rc = cap_enter();
	if (rc && errno != ENOSYS) {
		kutil_warn(NULL, NULL, "cap_enter");
		rc = 0;
	} else
		rc = 1;

	return rc;
}

int
ksandbox_capsicum_init_child(enum sandtype type, 
	int fd1, int fd2, int fdfiled, int fdaccept)
{
	int	 rc;

	switch (type) {
	case SAND_WORKER:
		rc = ksandbox_capsicum_init_worker(fd1, fd2);
		break;
	case SAND_CONTROL_OLD:
		assert(fd2 == -1);
		rc = ksandbox_capsicum_init_control
			(fd1, fdfiled, fdaccept);
		break;
	case SAND_CONTROL_NEW:
		assert(fd2 == -1);
		rc = ksandbox_capsicum_init_control
			(fd1, fdfiled, fdaccept);
		break;
	default:
		abort();
	}

	if (!rc) 
		kutil_warnx(NULL, NULL, "capsicum sandbox failure");

	return rc;
}

#else
int dummy;
#endif
