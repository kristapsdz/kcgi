/*	$Id$ */
/*
 * Copyright (c) 2014 Baptiste Daroussin <bapt@freebsd.org>
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

int
ksandbox_capsicum_init_child(void *arg, int fd)
{
	int rc;
	struct rlimit	 rl_zero;
	cap_rights_t	 rights;

	cap_rights_init(&rights);

	if (cap_rights_limit(STDIN_FILENO, &rights) < 0 && errno != ENOSYS)
		XWARN("cap_rights_limit: STDIN_FILENO");

	cap_rights_init(&rights, CAP_WRITE);
	if (cap_rights_limit(STDOUT_FILENO, &rights) < 0 && errno != ENOSYS)
		XWARN("cap_rights_limit: STDOUT_FILENO");
	if (cap_rights_limit(STDERR_FILENO, &rights) < 0 && errno != ENOSYS)
		XWARN("cap_rights_limit: STDERR_FILENO");

	cap_rights_init(&rights, CAP_READ|CAP_WRITE);
	if (cap_rights_limit(fd, &rights) < 0 && errno != ENOSYS)
		XWARN("cap_rights_limit: internal socket");

	rl_zero.rlim_cur = rl_zero.rlim_max = 0;

	if (-1 == setrlimit(RLIMIT_NOFILE, &rl_zero))
		XWARNX("setrlimit: rlimit_fsize");
	if (-1 == setrlimit(RLIMIT_FSIZE, &rl_zero))
		XWARNX("setrlimit: rlimit_fsize");
	if (-1 == setrlimit(RLIMIT_NPROC, &rl_zero))
		XWARNX("setrlimit: rlimit_nproc");

	rc = cap_enter();
	if (0 != rc && errno != ENOSYS) {
		XWARN("cap_enter");
		rc = 0;
	} else
		rc = 1;

	return(rc);
}

#else
int dummy;
#endif
