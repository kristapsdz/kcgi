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
#include <inttypes.h>
#include <poll.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h> /* BUFSIZ */
#include <string.h>
#include <unistd.h>

#include "kcgi.h"
#include "extern.h"

struct	kfcgi {
	struct kvalid	 *keys;
	size_t		  keysz;
	char		**mimes;
	size_t		  mimesz;
	void		 *work_box;
	pid_t		  work_pid;
	pid_t		  sock_pid;
	int		  work_dat;
	int		  sock_ctl;
};

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
	int		 fd;
	uint32_t	 cookie, test;
	struct pollfd	 pfd[2];
	char		 buf[BUFSIZ];
	ssize_t		 ssz;
	enum kcgi_err	 kerr;
	uint16_t	 rid, rtest;

	for (;;) {
		/* 
		 * Blocking accept from FastCGI socket.
		 * This will be round-robined by the kernel so that
		 * other control processes are fairly notified.
		 */
		fprintf(stderr, "%s: DEBUG: accepting...\n", __func__);
		sslen = sizeof(ss);
		fd = accept(STDIN_FILENO, 
			(struct sockaddr *)&ss, &sslen);
		fprintf(stderr, "%s: DEBUG: accepted\n", __func__);
		if (fd < 0) {
			if (EAGAIN == errno)
				break;
			XWARN("accept");
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
		fprintf(stderr, "%s: DEBUG: writing cookie\n", __func__);
		fullwrite(pfd[1].fd, &cookie, sizeof(uint32_t));
		for (;;) {
			fprintf(stderr, "%s: DEBUG: waiting for data\n", __func__);
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
			fprintf(stderr, "%s: DEBUG: writing data\n", __func__);
			fullwrite(pfd[1].fd, buf, ssz);
		}

		fprintf(stderr, "%s: DEBUG: waiting for cookie\n", __func__);
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

		fprintf(stderr, "%s: DEBUG: waiting for requestId\n", __func__);
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
		fprintf(stderr, "%s: DEBUG: writing descriptor\n", __func__);
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
		fprintf(stderr, "%s: DEBUG: waiting for ack\n", __func__);
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
	size_t	 	 i;

	close(fcgi->work_dat);
	close(fcgi->sock_ctl);
	ksandbox_free(fcgi->work_box);
	for (i = 0; i < fcgi->mimesz; i++)
		free(fcgi->mimes[i]);
	free(fcgi->mimes);
	free(fcgi->keys);
	free(fcgi);
}

enum kcgi_err
khttp_fcgi_free(struct kfcgi *fcgi)
{
	size_t	 	 i;

	/* Allow a NULL pointer. */
	if (NULL == fcgi)
		return(KCGI_OK);

	close(fcgi->work_dat);
	close(fcgi->sock_ctl);
	waitpid(fcgi->work_pid, NULL, 0);
	waitpid(fcgi->sock_pid, NULL, 0);
	ksandbox_close(fcgi->work_box);
	ksandbox_free(fcgi->work_box);

	for (i = 0; i < fcgi->mimesz; i++)
		free(fcgi->mimes[i]);
	free(fcgi->mimes);
	free(fcgi->keys);
	free(fcgi);
	return(KCGI_OK);
}

enum kcgi_err
khttp_fcgi_initx(struct kfcgi **fcgip, 
	const char *const *mimes, size_t mimesz,
	const struct kvalid *keys, size_t keysz, 
	void *arg, void (*argfree)(void *))
{
	enum kcgi_err	 kerr;
	struct kfcgi	*fcgi;
	int 		 er;
	size_t		 i;
	int		 work_ctl[2], work_dat[2], sock_ctl[2];
	void		*work_box;
	pid_t		 work_pid, sock_pid;

	fprintf(stderr, "%s: DEBUG: startup\n", __func__);

	if ( ! ksandbox_alloc(&work_box))
		return(KCGI_ENOMEM);

	fprintf(stderr, "%s: DEBUG: socketpair(control)\n", __func__);
	if (KCGI_OK != xsocketpair(AF_UNIX, SOCK_STREAM, 0, work_ctl)) {
		ksandbox_free(work_box);
		return(KCGI_SYSTEM);
	}

	fprintf(stderr, "%s: DEBUG: socketpair(data)\n", __func__);
	if (KCGI_OK != xsocketpair(AF_UNIX, SOCK_STREAM, 0, work_dat)) {
		close(work_ctl[KWORKER_PARENT]);
		close(work_ctl[KWORKER_CHILD]);
		ksandbox_free(work_box);
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
		ksandbox_init_child(work_box, 
			work_dat[KWORKER_CHILD],
			work_ctl[KWORKER_CHILD]);
		fprintf(stderr, "%s: DEBUG: child ready\n", __func__);
		kworker_fcgi_child
			(work_dat[KWORKER_CHILD],
			 work_ctl[KWORKER_CHILD],
			 keys, keysz, mimes, mimesz);
		ksandbox_free(work_box);
		close(work_dat[KWORKER_CHILD]);
		close(work_ctl[KWORKER_CHILD]);
		_exit(EXIT_SUCCESS);
		/* NOTREACHED */
	}

	fprintf(stderr, "%s: DEBUG: child started\n", __func__);
	close(work_dat[KWORKER_CHILD]);
	close(work_ctl[KWORKER_CHILD]);
	ksandbox_init_parent(work_box, work_pid);

	if (KCGI_OK != xsocketpair(AF_UNIX, SOCK_STREAM, 0, sock_ctl)) {
		close(work_dat[KWORKER_PARENT]);
		close(work_ctl[KWORKER_PARENT]);
		waitpid(work_pid, NULL, 0);
		ksandbox_close(work_box);
		ksandbox_free(work_box);
		return(KCGI_SYSTEM);
	}

	if (-1 == (sock_pid = fork())) {
		er = errno;
		XWARN("fork");
		close(work_dat[KWORKER_PARENT]);
		close(work_ctl[KWORKER_PARENT]);
		close(sock_ctl[KWORKER_CHILD]);
		close(sock_ctl[KWORKER_PARENT]);
		waitpid(work_pid, NULL, 0);
		ksandbox_close(work_box);
		ksandbox_free(work_box);
		return(EAGAIN == er ? KCGI_EAGAIN : KCGI_ENOMEM);
	} else if (0 == sock_pid) {
		if (NULL != argfree)
			argfree(arg);
		close(work_dat[KWORKER_PARENT]);
		close(sock_ctl[KWORKER_PARENT]);
		ksandbox_free(work_box);
		er = kfcgi_control
			(work_ctl[KWORKER_PARENT], 
			 sock_ctl[KWORKER_CHILD]);
		close(work_ctl[KWORKER_PARENT]);
		close(sock_ctl[KWORKER_CHILD]);
		_exit(er);
		/* NOTREACHED */
	}

	close(sock_ctl[KWORKER_CHILD]);
	close(work_ctl[KWORKER_PARENT]);
	close(STDIN_FILENO);

	/* Now allocate our device. */
	*fcgip = fcgi = XCALLOC(1, sizeof(struct kfcgi));
	if (NULL == fcgi) {
		close(sock_ctl[KWORKER_PARENT]);
		close(work_dat[KWORKER_PARENT]);
		waitpid(work_pid, NULL, 0);
		waitpid(sock_pid, NULL, 0);
		ksandbox_close(work_box);
		ksandbox_free(work_box);
		return(KCGI_ENOMEM);
	}

	kerr = KCGI_ENOMEM;

	fcgi->work_box = work_box;
	fcgi->work_pid = work_pid;
	fcgi->work_dat = work_dat[KWORKER_PARENT];
	fcgi->sock_pid = sock_pid;
	fcgi->sock_ctl = sock_ctl[KWORKER_PARENT];

	fcgi->mimes = XCALLOC(sizeof(char *), mimesz);
	if (NULL == fcgi->mimes)
		goto err;
	fcgi->mimesz = mimesz;
	fcgi->keys = XCALLOC(sizeof(struct kvalid), keysz);
	if (NULL == fcgi->keys)
		goto err;
	fcgi->keysz = keysz;

	for (i = 0; i < mimesz; i++)
		if (NULL == (fcgi->mimes[i] = XSTRDUP(mimes[i])))
			goto err;
	for (i = 0; i < keysz; i++)
		fcgi->keys[i] = keys[i];

	return(KCGI_OK);
err:
	/* 
	 * Bail out: kill child process and all memory.
	 */
	khttp_fcgi_free(fcgi);
	return(kerr);
}

enum kcgi_err
khttp_fcgi_init(struct kfcgi **fcgi, 
	const struct kvalid *keys, size_t keysz)
{

	return(khttp_fcgi_initx(fcgi, kmimetypes, 
		KMIME__MAX, keys, keysz, NULL, NULL));
}

enum kcgi_err
khttp_fcgi_parsex(struct kfcgi *fcgi, struct kreq *req, 
	const struct kmimemap *suffixmap, 
	const char *const *pages, size_t pagesz,
	size_t defmime, size_t defpage, void *arg)
{
	enum kcgi_err	 kerr;
	const struct kmimemap *mm;
	int		 c, fd;
	uint16_t	 rid;

	memset(req, 0, sizeof(struct kreq));
	kerr = KCGI_ENOMEM;

	/*
	 * Blocking wait until our control process sends us the file
	 * descriptor and requestId of the current sequence.
	 * It may also decide to exit.
	 */
	fprintf(stderr, "%s: DEBUG: waiting for descriptor\n", __func__);
	if ((c = fullreadfd(fcgi->sock_ctl, &fd, &rid, sizeof(uint16_t))) <= 0) {
		if (0 == c)
			return(KCGI_HUP);
		XWARNX("failed to read FastCGI socket");
		return(KCGI_SYSTEM);
	}
	fprintf(stderr, "%s: DEBUG: received descriptor\n", __func__);

	req->arg = arg;
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

	kerr = kworker_parent(fcgi->work_dat, req, fcgi->work_pid);
	if (KCGI_OK != kerr)
		goto err;

	/* Look up page type from component. */
	req->page = defpage;
	if ('\0' != *req->pagename)
		for (req->page = 0; req->page < pagesz; req->page++)
			if (0 == strcasecmp(pages[req->page], req->pagename))
				break;

	/* Start with the default. */
	req->mime = defmime;
	if ('\0' != *req->suffix) {
		for (mm = suffixmap; NULL != mm->name; mm++)
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

enum kcgi_err
khttp_fcgi_parse(struct kfcgi *fcgi, struct kreq *req, 
	const char *const *pages, size_t pagesz,
	size_t defpage)
{

	return(khttp_fcgi_parsex(fcgi, req, ksuffixmap, 
		pages, pagesz, KMIME_TEXT_HTML, defpage, NULL));
}
