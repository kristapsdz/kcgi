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
#include <sys/poll.h>
#include <sys/un.h>
#include <sys/stat.h>

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

struct	master;

typedef	int (*master_fp)(struct master *, int);

struct	master {
	struct kmaster	 *m;
	struct pollfd	 *pfds;
	size_t		  pfdsz;
	size_t		  pfdsmax;
	master_fp	 *fps;
};

static void
master_fd_add_read(struct master *m, master_fp fp, int fd)
{

	assert(m->pfdsz < m->pfdsmax);
	m->pfds[m->pfdsz].fd = fd;
	m->pfds[m->pfdsz].events = POLLIN;
	m->fps[m->pfdsz] = fp;
	m->pfdsz++;
}


static int
master_read_output(struct master *m, int fd)
{

	return(0);
}

static int
master_read_input(struct master *m, int fd)
{

	return(0);
}

static int
master_accept(struct master *m, int fd)
{
	struct sockaddr_storage ss;
	socklen_t		ssz;
	int		 	s;

	s = accept(fd, (struct sockaddr *)&ss, &ssz);

	if (-1 == s) {
		XWARN("accept");
		return(0);
	} else if (-1 == fcntl(s, F_SETFL, O_NONBLOCK)) {
		XWARN("fcntl: O_NONBLOCK");
		close(s);
		return(0);
	}
	
	master_fd_add_read(m, master_read_input, s);
	return(1);
}

static int
master_loop(struct kmaster *km)
{
	struct master	 m;
	struct sockaddr_un sun;
	size_t		 len;
	mode_t		 old_umask;
	int		 fd, rc;
	const char	*path = "/var/www/run/fastcgi.sock";
	size_t		 i;

	if (-1 == (fd = socket(AF_UNIX, SOCK_STREAM, 0))) {
		XWARN("socket");
		return(0);
	}

	memset(&sun, 0, sizeof(sun));
	sun.sun_family = AF_UNIX;
	len = strlcpy(sun.sun_path, path, sizeof(sun.sun_path));
	if (len >= sizeof(sun.sun_path)) {
		XWARNX("socket path to long");
		return(0);
	}
	sun.sun_len = len;

	if (-1 == unlink(path)) {
		XWARN("unlink: %s", path);
		return(0);
	}

	old_umask = umask(S_IXUSR|S_IXGRP|S_IWOTH|S_IROTH|S_IXOTH);

	if (-1 == bind(fd, (struct sockaddr *)&sun, sizeof(sun))) {
		XWARN("bind: %s", path);
		close(fd);
		return(0);

	}

	umask(old_umask);

	if (-1 == fcntl(fd, F_SETFL, O_NONBLOCK)) {
		XWARN("fcntl: O_NONBLOCK");
		close(fd);
		return(0);
	}

	if (-1 == listen(fd, km->workersz)) {
		XWARN("listen");
		close(fd);
		return(0);
	}

	memset(&m, 0, sizeof(struct master));
	m.m = km;
	m.pfdsmax = 10;
	m.pfds = XCALLOC(m.pfdsmax, sizeof(struct pollfd));
	if (NULL == m.pfds) {
		close(fd);
		return(0);
	}
	m.fps = XCALLOC(m.pfdsmax, sizeof(master_fp));
	if (NULL == m.fps) {
		free(m.pfds);
		close(fd);
		return(0);
	}
	master_fd_add_read(&m, master_accept, fd);
	master_fd_add_read(&m, master_read_output, 
		km->response[KMASTER_READ]);

	while (-1 != (rc = poll(m.pfds, m.pfdsz, -1)))
		for (i = 0; i < m.pfdsz; i++) {
			if ( ! (POLLIN & m.pfds[i].revents))
				continue;
			if ( ! (*m.fps[i])(&m, m.pfds[i].fd))
				break;
		}

	if (-1 == rc)
		XWARN("poll");

	for (i = 0; i < m.pfdsz; i++) {
		if (m.pfds[i].fd == km->response[KMASTER_READ])
			continue;
		close(m.pfds[i].fd);
	}

	free(m.pfds);
	free(m.fps);
	/* TODO: return value. */
	return(1);
}

enum kcgi_err
kmaster_init(struct kmaster *m,
	const struct kvalid *keys, size_t keysz,
	const char *const *mimes, size_t mimesz)
{
	size_t		 i;
	enum kcgi_err	 er;
	int		 sock[2];

	memset(m, 0, sizeof(struct kmaster));

	m->workersz = 10;
	m->workers = XCALLOC(m->workersz, sizeof(struct kworker));
	if (NULL == m->workers)
		return(KCGI_ENOMEM);

	m->outputs = XCALLOC(m->workersz, sizeof(int));
	if (NULL == m->outputs) {
		free(m->workers);
		return(KCGI_ENOMEM);
	}

	/* 
	 * Initialise file descriptors.
	 * When they're -1, we don't try to automatically close them if
	 * things go wrong.
	 */
	er = KCGI_OK;
	for (i = 0; i < m->workersz; i++) {
		m->outputs[i] = -1;
		m->workers[i].sock[0] = -1;
		m->workers[i].sock[1] = -1;
		m->workers[i].pid = -1;
	}
	m->response[0] = m->response[1] = -1;

	/*
	 * Initialise the worker context.
	 * Also initialise the sockets we'll use to communicate from the
	 * master to the workers and us to the master.
	 */
	for (i = 0; i < m->workersz; i++) {
		/* Initialise the worker itself. */
		er = kworker_init(&m->workers[i]);
		if (KCGI_OK != er)
			goto err;

		er = xsocketpair(AF_UNIX, SOCK_STREAM, 0, sock);
		if (KCGI_OK != er)
			goto err;

		m->workers[i].input = sock[0];
		m->outputs[i] = sock[1];

		/* Start the worker spinning. */
		if (-1 == (m->workers[i].pid = fork())) {
			er = EAGAIN == errno ? 
				KCGI_EAGAIN : KCGI_ENOMEM;
			XWARN("fork");
			goto err;
		} else if (0 == m->workers[i].pid) {
			/* TODO: free all other structures. */
			kworker_prep_child(&m->workers[i]);
			khttp_input_child(&m->workers[i], 
				keys, keysz, mimes, mimesz);
			kworker_free(&m->workers[i]);
			_exit(EXIT_SUCCESS);
		}
	}

	er = xsocketpair(AF_UNIX, SOCK_STREAM, 0, m->response);
	if (KCGI_OK != er)
		goto err;

	/*
	 * Now start the master.
	 * The master just copies data around and manages who has what;
	 * it doesn't actually process anything.
	 */
	if (-1 == (m->pid = fork())) {
		er = EAGAIN == errno ? 
			KCGI_EAGAIN : KCGI_ENOMEM;
		XWARN("fork");
		goto err;
	} else if (0 == m->pid) {
		master_loop(m);
		/* TODO: free... */
		_exit(EXIT_SUCCESS);
	}

	/* Everybody's ready! */
	return(KCGI_OK);
err:
	for (i = 0; i < m->workersz; i++) {
		if (-1 != m->outputs[i])
			close(m->outputs[i]);
		kworker_kill(&m->workers[i]);
		(void)kworker_close(&m->workers[i]);
		kworker_free(&m->workers[i]);
	}

	if (-1 != m->response[0])
		close(m->response[0]);
	if (-1 != m->response[1])
		close(m->response[1]);

	free(m->workers);
	free(m->outputs);
	free(m);
	return(er);
}
