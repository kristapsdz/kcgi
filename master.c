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
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "kcgi.h"
#include "extern.h"

struct	master;

/*
 * The master arbitrates data from the FastCGI socket.
 */
struct	kmaster {
	size_t		 workersz;
	struct kworker	*workers;
	int	 	*outputs;
#define	KMASTER_READ	 1
#define KMASTER_WRITE	 0
	int		 response[2];
	pid_t		 pid;
};


typedef	int (*master_fp)(struct master *, int);

struct	mstate {
	char		  head[8];
	size_t		  headsz;
	master_fp	  fpin;
	master_fp	  fphup;
};

struct	master {
	struct kmaster	 *m;
	struct pollfd	 *pfds;
	struct mstate	 *states;
	size_t		  pfdsz;
	size_t		  pfdsmax;
};

static void
master_fd_add_read(struct master *m, 
	master_fp fpin, master_fp fphup, int fd)
{

	assert(m->pfdsz < m->pfdsmax);
	fprintf(stderr, "Master adding fd %d as %zu\n", fd, m->pfdsz);
	memset(&m->pfds[m->pfdsz], 0, sizeof(struct pollfd));
	m->pfds[m->pfdsz].fd = fd;
	m->pfds[m->pfdsz].events = POLLIN;
	memset(&m->states[m->pfdsz], 0, sizeof(struct mstate));
	m->states[m->pfdsz].fpin = fpin;
	m->states[m->pfdsz].fphup = fphup;
	m->pfdsz++;
}

static int
master_output_hup(struct master *m, int pos)
{

	fprintf(stderr, "Master output socket closing\n");
	return(0);
}

static int
master_output_read(struct master *m, int pos)
{
	char		buf[1024];
	ssize_t		ssz;

	fprintf(stderr, "Master output socket read\n");

	while ((ssz = read(m->pfds[pos].fd, buf, sizeof(buf))) > 0) {
		fprintf(stderr, "Read: %zd bytes\n", ssz);
	}

	return(1);
}

static int
master_read_input_head(struct master *m, int pos)
{
	ssize_t	 	 ssz;
	unsigned char	 version;

	fprintf(stderr, "Master input read event (head)\n");

	ssz = read(m->pfds[pos].fd, 
		m->states[pos].head + m->states[pos].headsz, 
		8 - m->states[pos].headsz);

	if (ssz < 0) {
		XWARN("read");
		return(-1);
	} else if ((m->states[pos].headsz += ssz) < 8)
		return(1);

	version = m->states[pos].head[0];

	if (1 != version) {
		XWARNX("read bad version: %u", version);
		return(0);
	} 

	return(1);
}

static int
master_accept_hup(struct master *m, int pos)
{

	fprintf(stderr, "Master socket closing\n");
	return(0);
}

static int
master_accept_read(struct master *m, int pos)
{
	struct sockaddr_storage ss;
	socklen_t		ssz;
	int		 	s;

	fprintf(stderr, "Connection on master socket\n");

	s = accept(m->pfds[pos].fd, (struct sockaddr *)&ss, &ssz);

	if (-1 == s) {
		XWARN("accept");
		return(0);
	} else if (-1 == fcntl(s, F_SETFL, O_NONBLOCK)) {
		XWARN("fcntl: O_NONBLOCK");
		close(s);
		return(0);
	}

	fprintf(stderr, "Master socket accepted connection\n");
	
	master_fd_add_read(m, master_read_input_head, 
		m->states[pos].fphup, s);
	return(1);
}

static void
master_sighup(int code)
{
}

static void
worker_loop(struct kmaster *km, size_t pos, 
	const struct kvalid *keys, size_t keysz,
	const char *const *mimes, size_t mimesz)
{
	size_t	 i;

	for (i = 0; i < pos; i++)
		kworker_free(&km->workers[i]);

	if (-1 == close(STDIN_FILENO))
		XWARN("close (STDIN_FILENO, worker[%zu])", pos);

	kworker_prep_child(&km->workers[pos]);
	khttp_input_child(&km->workers[pos], keys, keysz, mimes, mimesz);
	kworker_free(&km->workers[pos]);
}

static int
master_loop(struct kmaster *km)
{
	struct master	 m;
	int		 fd, rc, er;
	const char	*path = "/tmp/fastcgi.sock";
	size_t		 i;

	signal(SIGHUP, master_sighup);

	fprintf(stderr, "%s: starting\n", path);

	if (-1 == (fd = xbindpath(path)))
		return(0);

	if (-1 == listen(fd, km->workersz)) {
		XWARN("%s: listen", path);
		close(fd);
		return(0);
	}

	memset(&m, 0, sizeof(struct master));
	m.m = km;
	m.pfdsmax = km->workersz;
	m.pfds = XCALLOC(m.pfdsmax, sizeof(struct pollfd));
	if (NULL == m.pfds) {
		if (-1 == close(fd))
			XWARN("%s: close", path);
		return(0);
	}

	m.states = XCALLOC(m.pfdsmax, sizeof(struct mstate));
	if (NULL == m.states) {
		free(m.pfds);
		if (-1 == close(fd))
			XWARN("%s: close", path);
		return(0);
	}

	master_fd_add_read(&m, 
		master_accept_read, master_accept_hup, fd);
	master_fd_add_read(&m, master_output_read, 
		master_output_hup, km->response[KMASTER_READ]);

	while (-1 != (rc = poll(m.pfds, m.pfdsz, -1))) {
		for (i = 0; i < m.pfdsz; i++) {
			if (POLLIN & m.pfds[i].revents &&
				 NULL != m.states[i].fpin &&
				 ! (*m.states[i].fpin)(&m, i))
				break;
			if (POLLHUP & m.pfds[i].revents &&
 				 NULL != m.states[i].fphup &&
 				 ! (*m.states[i].fphup)(&m, i))
				break;
		}
		if (i < m.pfdsz)
			break;
	}

	/* EINTR is the only way we exit properly. */
	er = 0;
	if (-1 == rc && EINTR != errno)
		XWARN("%s: poll", path);
	else if (-1 == rc)
		er = 1;

	/* TODO: drain all open file descriptors. */

	/* Clean up all file-descriptors. */
	for (i = 0; i < m.pfdsz; i++) 
		close(m.pfds[i].fd);

	m.m->response[KMASTER_READ] = -1;

	free(m.states);
	free(m.pfds);
	return(er);
}

void
kmaster_free(struct kmaster *m)
{
	size_t	 i;

	if (NULL == m)
		return;

	fprintf(stderr, "application exiting\n");

	/* 
	 * First kill the master.
	 * This will exit immediately without waiting for new
	 * connections to drain their input.
	 */
	if (-1 != m->pid) {
		if (-1 == kill(m->pid, SIGKILL))
			XWARN("kill (SIGHUP, master)");
		if (-1 == waitpid(m->pid, NULL, 0))
			XWARN("waitpid (master)");
	}

	/*
	 * Next, kill all of the workers.
	 * They, too, will exit immediately upon SIGHUP.
	 */
	for (i = 0; i < m->workersz; i++) {
		kworker_kill(&m->workers[i]);
		(void)kworker_close(&m->workers[i]);
		kworker_free(&m->workers[i]);
	}

	if (-1 != m->response[KMASTER_WRITE])
		if (-1 == close(m->response[KMASTER_WRITE]))
			XWARN("close (master write)");

	fprintf(stderr, "application exited\n");

	/* TODO: this. */
	free(m->workers);
	free(m->outputs);
	free(m);
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

	m->workersz = 2;
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
			XWARN("fork (worker[%zu])", i);
			goto err;
		} else if (0 == m->workers[i].pid) {
			worker_loop(m, i, keys, keysz, mimes, mimesz);
			exit(EXIT_SUCCESS);
		}

		close(m->workers[i].input);
		m->workers[i].input = -1;
	}

	fprintf(stderr, "%s: workers started\n", __func__);

	/* Now the socket between the application and master. */
	er = xsocketpair(AF_UNIX, SOCK_STREAM, 0, m->response);
	if (KCGI_OK != er)
		goto err;

	/* Now start the master. */
	if (-1 == (m->pid = fork())) {
		er = EAGAIN == errno ? KCGI_EAGAIN : KCGI_ENOMEM;
		XWARN("fork");
		goto err;
	} else if (0 == m->pid) {
		if (-1 == close(m->response[KMASTER_WRITE]))
			XWARN("close (master write)");
		m->response[KMASTER_WRITE] = -1;
		master_loop(m);
		if (-1 != m->response[KMASTER_READ])
			close(m->response[KMASTER_READ]);
		/* TODO: free... */
		_exit(EXIT_SUCCESS);
	}

	if (-1 == close(m->response[KMASTER_READ]))
		XWARN("application close: master read");
	m->response[KMASTER_READ] = -1;

	fprintf(stderr, "Main returning\n");
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
