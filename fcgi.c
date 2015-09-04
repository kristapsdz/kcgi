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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/ioctl.h>
#include <sys/socket.h>

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <poll.h>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h> /* BUFSIZ */
#include <string.h>
#include <unistd.h>

#include "kcgi.h"
#include "extern.h"

struct	kfcgi {
	const struct kvalid	 *keys;
	size_t			  keysz;
	const char *const	 *mimes;
	size_t			  mimesz;
	size_t			  defmime;
	const char *const	 *pages;
	size_t			  pagesz;
	size_t			  defpage;
	const struct kmimemap 	 *mimemap;
	void			 *work_box;
	void			 *sock_box;
	pid_t			  work_pid;
	pid_t			  sock_pid;
	int			  work_dat;
	int			  sock_ctl;
	void			 *arg;
};

static	volatile sig_atomic_t sig = 0;

static void
dosignal(int arg)
{

	sig = 1;
}

/*
 * This is our control process.
 * It listens for FastCGI connections on STDIN_FILENO.
 * When it has one, it reads and passes to the worker (sibling) process,
 * which will be parsing the data.
 * When the worker has finished, it passes back the request identifier,
 * which this passes to the main application for output.
 */
static int
kfcgi_control(int work, int ctrl)
{
	struct sockaddr_storage ss;
	socklen_t	 sslen;
	int		 fd, rc;
	uint32_t	 cookie, test;
	struct pollfd	 pfd[2];
	char		 buf[BUFSIZ];
	ssize_t		 ssz;
	enum kcgi_err	 kerr;
	uint16_t	 rid, rtest;

	if (KCGI_OK != xsocketprep(STDIN_FILENO)) {
		XWARNX("xsocketprep");
		return(EXIT_FAILURE);
	} 

	for (;;) {
		pfd[0].fd = STDIN_FILENO;
		pfd[0].events = POLLIN;
		pfd[0].revents = 0;
		pfd[1].fd = ctrl;
		pfd[1].events = POLLIN;
		pfd[1].revents = 0;

		if ((rc = poll(pfd, 2, -1)) < 0) {
			XWARN("poll");
			return(EXIT_FAILURE);
		} else if (0 == rc) {
			/*
			 * This seems to happen on Mac OSX from time to
			 * time and I have no idea why: it's blatantly
			 * bad behaviour.
			 */
			XWARNX("poll expired!?");
			continue;
		} else if (POLLHUP & pfd[1].revents) {
			break;
		} else if ( ! (POLLIN & pfd[0].revents)) {
			XWARNX("poll: control (%d, %d | %d, %d; %d, %d; %d, %d; %d, %d)",
				pfd[0].revents,
				pfd[1].revents,
				POLLIN & pfd[0].revents,
				POLLIN & pfd[1].revents,
				POLLHUP & pfd[0].revents,
				POLLHUP & pfd[1].revents,
				POLLNVAL & pfd[0].revents,
				POLLNVAL & pfd[1].revents,
				POLLERR & pfd[0].revents,
				POLLERR & pfd[1].revents);
			return(EXIT_FAILURE);
		}

		/* 
		 * Blocking accept from FastCGI socket.
		 * This will be round-robined by the kernel so that
		 * other control processes are fairly notified.
		 * We then set that the FastCGI socket is non-blocking,
		 * making it consistent with the behaviour of the CGI
		 * socket, which is also set as such.
		 */
		sslen = sizeof(ss);
		fd = accept(STDIN_FILENO, 
			(struct sockaddr *)&ss, &sslen);
		if (fd < 0) {
			if (EAGAIN == errno)
				continue;
			XWARN("accept");
			return(EXIT_FAILURE);
		} else if (KCGI_OK != xsocketprep(fd)) {
			XWARNX("xsocketprep");
			return(EXIT_FAILURE);
		}

		pfd[0].fd = fd;
		pfd[0].events = POLLIN;
		pfd[1].fd = work;
		pfd[1].events = POLLIN;
#ifdef __linux__
		cookie = random();
#else
		cookie = arc4random();
#endif
		/*
		 * Write a header cookie then the entire message body to
		 * the worker.
		 * We don't know when the data is finished, but the
		 * worker does, so wait for it to notify us back.
		 */
		fullwrite(pfd[1].fd, &cookie, sizeof(uint32_t));
		for (;;) {
			if (-1 == poll(pfd, 2, -1)) {
				XWARN("poll: control socket");
				close(fd);
				return(EXIT_FAILURE);
			} 
			if (POLLIN & pfd[1].revents) {
				/* Child is responding! */
				break;
			} else if ( ! (POLLIN & pfd[0].revents)) {
				XWARNX("poll: bad events");
				close(fd);
				return(EXIT_FAILURE);
			} else if ((ssz = read(fd, buf, BUFSIZ)) < 0) {
				XWARN("read: control socket");
				close(fd);
				return(EXIT_FAILURE);
			} else if (0 == ssz) {
				close(fd);
				return(EXIT_FAILURE);
			}
			fullwrite(pfd[1].fd, buf, ssz);
		}

		/* Now verify that the worker is sane. */
		if (fullread(pfd[1].fd, &test, 
			 sizeof(uint32_t), 0, &kerr) < 0) {
			XWARNX("failed to read FastCGI cookie");
			close(fd);
			return(EXIT_FAILURE);
		} else if (cookie != test) {
			XWARNX("failed to verify FastCGI cookie");
			close(fd);
			return(EXIT_FAILURE);
		} 

		if (fullread(pfd[1].fd, &rid, 
			 sizeof(uint16_t), 0, &kerr) < 0) {
			XWARNX("failed to read FastCGI requestId");
			close(fd);
			return(EXIT_FAILURE);
		}
#ifdef __linux__
		cookie = random();
#else
		cookie = arc4random();
#endif
		/*
		 * Pass the file descriptor, which has had its data
		 * sucked dry, to the main application.
		 * It will do output, so it also needs the FastCGI
		 * socket request identifier.
		 */
		if ( ! fullwritefd(ctrl, fd, &rid, sizeof(uint16_t))) {
			XWARNX("failed to send FastCGI socket");
			close(fd);
			return(EXIT_FAILURE);
		}

		/* 
		 * This will wait til the application is finished.
		 * It will then double-check the requestId. 
		 */
		if (fullread(ctrl, &rtest, sizeof(uint16_t), 0, &kerr) < 0) {
			XWARNX("failed to read FastCGI cookie");
			close(fd);
			return(EXIT_FAILURE);
		} else if (rid != rtest) {
			XWARNX("%" PRIu16 " != %" PRIu16 "\n", rid, rtest);
			XWARNX("failed to verify FastCGI requestId");
			close(fd);
			return(EXIT_FAILURE);
		}

		/* We're done: try again. */
		close(fd);
	}

	return(EXIT_SUCCESS);
}

void
khttp_fcgi_child_free(struct kfcgi *fcgi)
{

	close(fcgi->work_dat);
	close(fcgi->sock_ctl);
	ksandbox_free(fcgi->work_box);
	free(fcgi);
}

enum kcgi_err
khttp_fcgi_free(struct kfcgi *fcgi)
{

	/* Allow a NULL pointer. */
	if (NULL == fcgi)
		return(KCGI_OK);

	close(fcgi->work_dat);
	close(fcgi->sock_ctl);
	xwaitpid(fcgi->work_pid);
	xwaitpid(fcgi->sock_pid);
	ksandbox_close(fcgi->work_box);
	ksandbox_free(fcgi->work_box);
	ksandbox_close(fcgi->sock_box);
	ksandbox_free(fcgi->sock_box);
	free(fcgi);
	return(KCGI_OK);
}

enum kcgi_err
khttp_fcgi_initx(struct kfcgi **fcgip, 
	const char *const *mimes, size_t mimesz,
	const struct kvalid *keys, size_t keysz, 
	const struct kmimemap *mimemap, size_t defmime,
	const char *const *pages, size_t pagesz,
	size_t defpage, void *arg, void (*argfree)(void *))
{
	struct kfcgi	*fcgi;
	int 		 er;
	int		 work_ctl[2], work_dat[2], sock_ctl[2];
	void		*work_box, *sock_box;
	pid_t		 work_pid, sock_pid;

	/*
	 * FIXME: block this signal unless we're right at the fullreadfd
	 * function, at which point unblock and let it interrupt us.
	 * This will have a "race condition" where we might receive a
	 * signal and ignore it.
	 */
	signal(SIGTERM, dosignal);

	if ( ! ksandbox_alloc(&work_box))
		return(KCGI_ENOMEM);

	if ( ! ksandbox_alloc(&sock_box)) {
		ksandbox_free(work_box);
		return(KCGI_ENOMEM);
	}

	if (KCGI_OK != xsocketpair
		 (AF_UNIX, SOCK_STREAM, 0, work_ctl)) {
		ksandbox_free(work_box);
		ksandbox_free(sock_box);
		return(KCGI_SYSTEM);
	}

	if (KCGI_OK != xsocketpair
		 (AF_UNIX, SOCK_STREAM, 0, work_dat)) {
		close(work_ctl[KWORKER_PARENT]);
		close(work_ctl[KWORKER_CHILD]);
		ksandbox_free(work_box);
		ksandbox_free(sock_box);
		return(KCGI_SYSTEM);
	}

	if (-1 == (work_pid = fork())) {
		er = errno;
		XWARN("fork");
		close(work_ctl[KWORKER_PARENT]);
		close(work_ctl[KWORKER_CHILD]);
		close(work_dat[KWORKER_PARENT]);
		close(work_dat[KWORKER_CHILD]);
		ksandbox_free(work_box);
		ksandbox_free(sock_box);
		return(EAGAIN == er ? KCGI_EAGAIN : KCGI_ENOMEM);
	} else if (0 == work_pid) {
		if (NULL != argfree)
			argfree(arg);
		/* 
		 * STDIN_FILENO isn't really stdin, it's the control
		 * socket used to pass input sockets to us.
		 * Thus, close the parent's control socket. 
		 */
		close(STDIN_FILENO);
		close(STDOUT_FILENO);
		close(work_dat[KWORKER_PARENT]);
		close(work_ctl[KWORKER_PARENT]);
		if ( ! ksandbox_init_child(work_box, 
			 SAND_WORKER,
			 work_dat[KWORKER_CHILD],
			 work_ctl[KWORKER_CHILD])) {
			XWARNX("ksandbox_init_child");
			er = EXIT_FAILURE;
		} else {
			kworker_fcgi_child
				(work_dat[KWORKER_CHILD],
				 work_ctl[KWORKER_CHILD],
				 keys, keysz, mimes, mimesz);
			er = EXIT_SUCCESS;
		}
		ksandbox_free(work_box);
		ksandbox_free(sock_box);
		close(work_dat[KWORKER_CHILD]);
		close(work_ctl[KWORKER_CHILD]);
		_exit(er);
		/* NOTREACHED */
	}

	close(work_dat[KWORKER_CHILD]);
	close(work_ctl[KWORKER_CHILD]);

	if ( ! ksandbox_init_parent
		 (work_box, SAND_WORKER, work_pid)) {
		XWARNX("ksandbox_init_parent");
		close(work_dat[KWORKER_PARENT]);
		close(work_ctl[KWORKER_PARENT]);
		xwaitpid(work_pid);
		ksandbox_free(work_box);
		ksandbox_free(sock_box);
		return(KCGI_SYSTEM);
	}

	if (KCGI_OK != xsocketpair
		 (AF_UNIX, SOCK_STREAM, 0, sock_ctl)) {
		close(work_dat[KWORKER_PARENT]);
		close(work_ctl[KWORKER_PARENT]);
		xwaitpid(work_pid);
		ksandbox_close(work_box);
		ksandbox_free(work_box);
		ksandbox_free(sock_box);
		return(KCGI_SYSTEM);
	}

	if (-1 == (sock_pid = fork())) {
		er = errno;
		XWARN("fork");
		close(work_dat[KWORKER_PARENT]);
		close(work_ctl[KWORKER_PARENT]);
		close(sock_ctl[KWORKER_CHILD]);
		close(sock_ctl[KWORKER_PARENT]);
		xwaitpid(work_pid);
		ksandbox_close(work_box);
		ksandbox_free(work_box);
		ksandbox_free(sock_box);
		return(EAGAIN == er ? KCGI_EAGAIN : KCGI_ENOMEM);
	} else if (0 == sock_pid) {
		if (NULL != argfree)
			argfree(arg);
		close(STDOUT_FILENO);
		close(work_dat[KWORKER_PARENT]);
		close(sock_ctl[KWORKER_PARENT]);
		ksandbox_free(work_box);
		if ( ! ksandbox_init_child(sock_box, 
			 SAND_CONTROL,
			 sock_ctl[KWORKER_CHILD], -1)) {
			XWARNX("ksandbox_init_child");
			er = EXIT_FAILURE;
		} else 
			er = kfcgi_control
				(work_ctl[KWORKER_PARENT], 
				 sock_ctl[KWORKER_CHILD]);
		close(work_ctl[KWORKER_PARENT]);
		close(sock_ctl[KWORKER_CHILD]);
		ksandbox_free(sock_box);
		_exit(er);
		/* NOTREACHED */
	}

	close(sock_ctl[KWORKER_CHILD]);
	close(work_ctl[KWORKER_PARENT]);
	close(STDIN_FILENO);

	if ( ! ksandbox_init_parent
		 (sock_box, SAND_CONTROL, sock_pid)) {
		XWARNX("ksandbox_init_parent");
		close(sock_ctl[KWORKER_PARENT]);
		close(work_dat[KWORKER_PARENT]);
		xwaitpid(work_pid);
		xwaitpid(sock_pid);
		ksandbox_close(work_box);
		ksandbox_free(work_box);
		ksandbox_close(sock_box);
		ksandbox_free(sock_box);
		return(KCGI_SYSTEM);
	}

	/* Now allocate our device. */
	*fcgip = fcgi = XCALLOC(1, sizeof(struct kfcgi));
	if (NULL == fcgi) {
		close(sock_ctl[KWORKER_PARENT]);
		close(work_dat[KWORKER_PARENT]);
		xwaitpid(work_pid);
		xwaitpid(sock_pid);
		ksandbox_close(work_box);
		ksandbox_free(work_box);
		ksandbox_close(sock_box);
		ksandbox_free(sock_box);
		return(KCGI_ENOMEM);
	}

	fcgi->work_box = work_box;
	fcgi->work_pid = work_pid;
	fcgi->work_dat = work_dat[KWORKER_PARENT];
	fcgi->sock_box = sock_box;
	fcgi->sock_pid = sock_pid;
	fcgi->sock_ctl = sock_ctl[KWORKER_PARENT];
	fcgi->arg = arg;
	fcgi->mimes = mimes;
	fcgi->mimesz = mimesz;
	fcgi->defmime = defmime;
	fcgi->keys = keys;
	fcgi->keysz = keysz;
	fcgi->mimemap = mimemap;
	fcgi->pages = pages;
	fcgi->pagesz = pagesz;
	fcgi->defpage = defpage;
	return(KCGI_OK);
}

enum kcgi_err
khttp_fcgi_init(struct kfcgi **fcgi, 
	const struct kvalid *keys, size_t keysz,
	const char *const *pages, size_t pagesz,
	size_t defpage)
{

	return(khttp_fcgi_initx(fcgi, kmimetypes, 
		KMIME__MAX, keys, keysz, ksuffixmap,
		KMIME_TEXT_HTML, pages, pagesz, defpage, 
		NULL, NULL));
}

enum kcgi_err
khttp_fcgi_parse(struct kfcgi *fcgi, struct kreq *req)
{
	enum kcgi_err	 kerr;
	const struct kmimemap *mm;
	int		 c, fd;
	uint16_t	 rid;

	memset(req, 0, sizeof(struct kreq));
	kerr = KCGI_ENOMEM;

	/*
	 * Blocking wait until our control process sends us the file descriptor
	 * and requestId of the current sequence.
	 * It may also decide to exit, which we note by seeing that
	 * "sig" has been set or the channel closed gracefully.
	 */
	c = fullreadfd(fcgi->sock_ctl, &fd, &rid, sizeof(uint16_t));
	if (c < 0 && ! sig) {
		XWARNX("fullreadfd");
		return(KCGI_SYSTEM);
	} else if (0 == c || (c < 0 && sig))
		return(KCGI_HUP);

	req->arg = fcgi->arg;
	req->keys = fcgi->keys;
	req->keysz = fcgi->keysz;
	if (NULL == (req->kdata = kdata_alloc(fcgi->sock_ctl, fd, rid)))
		goto err;
	fd = -1;
	req->cookiemap = XCALLOC
		(fcgi->keysz, sizeof(struct kpair *));
	if (fcgi->keysz && NULL == req->cookiemap)
		goto err;
	req->cookienmap = XCALLOC
		(fcgi->keysz, sizeof(struct kpair *));
	if (fcgi->keysz && NULL == req->cookienmap)
		goto err;
	req->fieldmap = XCALLOC
		(fcgi->keysz, sizeof(struct kpair *));
	if (fcgi->keysz && NULL == req->fieldmap)
		goto err;
	req->fieldnmap = XCALLOC
		(fcgi->keysz, sizeof(struct kpair *));
	if (fcgi->keysz && NULL == req->fieldnmap)
		goto err;

	/*
	 * Now read the request itself from the worker child.
	 * We'll wait perpetually on data until the channel closes or
	 * until we're interrupted during a read by the parent.
	 * FIXME: set a signal mask and ignore SIGTERM while we're
	 * processing the request itself!
	 */
	kerr = kworker_parent(fcgi->work_dat, req);
	if (sig) {
		kerr = KCGI_OK;
		goto err;
	} else if (KCGI_OK != kerr)
		goto err;

	/* Look up page type from component. */
	req->page = fcgi->defpage;
	if ('\0' != *req->pagename)
		for (req->page = 0; req->page < fcgi->pagesz; req->page++)
			if (0 == strcasecmp(fcgi->pages[req->page], req->pagename))
				break;

	/* Start with the default. */
	req->mime = fcgi->defmime;
	if ('\0' != *req->suffix) {
		for (mm = fcgi->mimemap; NULL != mm->name; mm++)
			if (0 == strcasecmp(mm->name, req->suffix)) {
				req->mime = mm->mime;
				break;
			}
		 /* Could not find this mime type! */
		if (NULL == mm->name)
			req->mime = fcgi->mimesz;
	}

	return(kerr);
err:
	if (-1 != fd)
		close(fd);
	/*kreq_free(req);*/
	khttp_free(req);
	return(kerr);
}

int
khttp_fcgi_test(void)
{
	socklen_t	len = 0;

	if (-1 != getpeername(STDIN_FILENO, NULL, &len))
		return(0);
	return(ENOTCONN == errno);
}
