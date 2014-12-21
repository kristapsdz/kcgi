/*	$Id$ */
/*
 * Copyright (c) 2014 Kristaps Dzonsons <kristaps@bsd.lv>
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
#include <sys/socket.h>
#include <sys/wait.h>

#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "kcgi.h"
#include "extern.h"

enum kcgi_err
kworker_init(struct kworker *p)
{
	size_t		 i;
	int 	 	 sndbuf;
	socklen_t	 socksz;
	enum kcgi_err	 er;

	memset(p, 0, sizeof(struct kworker));
	p->pid = -1;
	p->sock[0] = p->sock[1] = -1;

	/* Allocate the communication sockets. */
	er = xsocketpair(AF_UNIX, SOCK_STREAM, 0, p->sock);
	if (KCGI_OK != er)
		return(er);

	/* Allocate the sandbox. (FIXME: ENOMEM?) */
	p->sand = ksandbox_alloc();

	/* Enlarge the transfer buffer size. */
	/* FIXME: is this a good idea? */
	socksz = sizeof(sndbuf);
	for (i = 200; i > 0; i--) {
		sndbuf = (i + 1) * 1024;
		if (-1 != setsockopt(p->sock[KWORKER_READ], 
			SOL_SOCKET, SO_RCVBUF, &sndbuf, socksz) &&
			-1 != setsockopt(p->sock[KWORKER_WRITE], 
			SOL_SOCKET, SO_SNDBUF, &sndbuf, socksz))
			break;
		XWARN("sockopt");
	}

	return(KCGI_OK);
}

void
kworker_free(struct kworker *p)
{

	if (-1 != p->sock[0])
		close(p->sock[0]);
	if (-1 != p->sock[1])
		close(p->sock[1]);
	if (-1 != p->input)
		close(p->input);
	ksandbox_free(p->sand);
}

void
kworker_prep_child(struct kworker *p)
{

	close(p->sock[KWORKER_READ]);
	p->sock[KWORKER_READ] = -1;
	ksandbox_init_child(p->sand, p->sock[KWORKER_WRITE]);
}

void
kworker_prep_parent(struct kworker *p)
{

	close(p->sock[KWORKER_WRITE]);
	p->sock[KWORKER_WRITE] = -1;
	ksandbox_init_parent(p->sand, p->pid);
	close(p->input);
	p->input = -1;
}

void
kworker_kill(struct kworker *p)
{

	if (-1 != p->pid)
		(void)kill(p->pid, SIGKILL);
}

enum kcgi_err
kworker_close(struct kworker *p)
{
	pid_t	 	 rp;
	int	 	 st;
	enum kcgi_err	 ke;

	ke = KCGI_OK;

	if (-1 == p->pid) {
		ksandbox_close(p->sand);
		return(ke);
	}

	do
		rp = waitpid(p->pid, &st, 0);
	while (rp == -1 && errno == EINTR);

	if (-1 == rp) {
		ke = KCGI_SYSTEM;
		XWARN("waiting for child");
	} else if (WIFEXITED(st) && EXIT_SUCCESS != WEXITSTATUS(st)) {
		ke = KCGI_FORM;
		XWARNX("child status %d", WEXITSTATUS(st));
	} else if (WIFSIGNALED(st)) {
		ke = KCGI_FORM;
		XWARNX("child signal %d", WTERMSIG(st));
	}

	ksandbox_close(p->sand);
	return(ke);
}
