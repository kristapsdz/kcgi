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
	struct kvalid	 *keys;
	size_t		  keysz;
	char		**mimes;
	size_t		  mimesz;
	struct kworker	  work;
};

static	volatile sig_atomic_t handle = 0;

static void
kfcgi_sighandle(int code)
{

	handle = 1;
}

/*
 * Set up a non-blocking read for our next connection.
 * If we're interrupting during this operation, then bail out.
 */
static int 
kfcgi_control_read(int *newfd, int socket)
{
	struct sockaddr_storage	  ss;
	socklen_t		  sslen;
	int			  fd;

	if (handle)
		return(0);

	sslen = sizeof(ss);
	fd = accept(STDIN_FILENO, (struct sockaddr *)&ss, &sslen);
	if (fd < 0) {
		if (EAGAIN == errno)
			return(0);
		XWARN("accept");
		return(-1);
	} 

	*newfd = fd;
	return(1);
}

void
khttp_fcgi_child_free(struct kfcgi *fcgi)
{
	size_t	 	 i;

	close(STDIN_FILENO);
	kworker_hup(&fcgi->work);
	for (i = 0; i < fcgi->mimesz; i++)
		free(fcgi->mimes[i]);
	free(fcgi->mimes);
	free(fcgi->keys);
	kworker_free(&fcgi->work);
	free(fcgi);
}

enum kcgi_err
khttp_fcgi_free(struct kfcgi *fcgi)
{
	size_t	 	 i;
	enum kcgi_err	 er;

	/* 
	 * Do this first: give the child a chance to see that we've
	 * closed the control socket and exit on its own.
	 */
	kworker_hup(&fcgi->work);

	for (i = 0; i < fcgi->mimesz; i++)
		free(fcgi->mimes[i]);
	free(fcgi->mimes);
	free(fcgi->keys);
	/*
	 * This blocks on the PID.
	 */
	er = kworker_close(&fcgi->work);
	kworker_free(&fcgi->work);
	free(fcgi);
	return(er);
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
	struct kworker	 work;

	/* FIXME: restore on khttp_fcgi_free(). */
	signal(SIGHUP, kfcgi_sighandle);

	/*
	 * Begin by initialising our worker.
	 * Everything open now (file descriptors, etc.) will be
	 * inherited by the child, as we don't do an execve.
	 */
	memset(&work, 0, sizeof(struct kworker));
	if (KCGI_OK != (kerr = kworker_fcgi_init(&work)))
		return(kerr);

	if (-1 == (work.pid = fork())) {
		er = errno;
		XWARN("fork");
		return(EAGAIN == er ? KCGI_EAGAIN : KCGI_ENOMEM);
	} else if (0 == work.pid) {
		if (NULL != argfree)
			argfree(arg);
		/* 
		 * STDIN_FILENO isn't really stdin, it's the control
		 * socket used to pass input sockets to us.
		 * Thus, close the parent's control socket. 
		 */
		close(STDIN_FILENO);
		signal(SIGHUP, SIG_IGN);
		kworker_prep_child(&work);
		kworker_fcgi_child(&work, keys, keysz, mimes, mimesz);
		kworker_free(&work);
		_exit(EXIT_SUCCESS);
		/* NOTREACHED */
	}
	kworker_prep_parent(&work);

	/* Now allocate our device. */
	*fcgip = fcgi = XCALLOC(1, sizeof(struct kfcgi));
	if (NULL == fcgi) 
		return(KCGI_ENOMEM);

	kerr = KCGI_ENOMEM;
	fcgi->work = work;
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
	(void)khttp_fcgi_free(fcgi);
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
	ssize_t		 ssz;
	uint16_t	 rid;
	uint32_t	 cookie, test;
	char		 buf[BUFSIZ];
	struct pollfd	 pfd[2];

	memset(req, 0, sizeof(struct kreq));
	kerr = KCGI_ENOMEM;

	/*
	 * Poll in our input socket (stdin) until the parent sends us a
	 * file descriptor to munch.
	 * If this returns zero, then the parent has killed our channel.
	 */
	if (0 == (c = kfcgi_control_read(&fd, STDIN_FILENO)))
		return(KCGI_HUP);
	else if (c < 0)
		return(KCGI_SYSTEM);

	pfd[0].fd = fd;
	pfd[0].events = POLLIN;
	pfd[1].fd = fcgi->work.sock[KWORKER_READ];
	pfd[1].events = POLLIN;

	cookie = arc4random();
	fullwrite(fcgi->work.control[KWORKER_WRITE], 
		&cookie, sizeof(uint32_t));

	/*
	 * Keep reading then writing data into the child context.
	 * It will let us know that all data has been read and processed
	 * when we have a write event from it.
	 * TODO: have the first byte from the child be a cookie that
	 * indicates that all data has been read.
	 * Randomly generate the cookie each time.
	 */
	for (;;) {
		if (-1 == poll(pfd, 2, -1)) {
			XWARN("poll: control socket");
			close(fd);
			return(KCGI_SYSTEM);
		} 
		
		if (POLLIN & pfd[1].revents) {
			/* 
			 * The child is starting to send us data.
			 * Stop trying to read more data and roll.
			 */
			break;
		} else if ( ! (POLLIN & pfd[0].revents)) {
			XWARNX("poll: bad events");
			close(fd);
			return(KCGI_SYSTEM);
		} else if ((ssz = read(fd, buf, BUFSIZ)) < 0) {
			XWARN("read: control socket");
			close(fd);
			return(KCGI_SYSTEM);
		} else if (0 == ssz) {
			close(fd);
			return(KCGI_SYSTEM);
		}
		fullwrite(fcgi->work.control[KWORKER_WRITE], buf, ssz);
	}

	/*
	 * Read our cookie and request ID.
	 * The cookie was sent before the first transmission; in reading
	 * it back, we add an additional check to be sure that the
	 * process hasn't been naughty.
	 */
	if (fullread(fcgi->work.sock[KWORKER_READ], 
		 &test, sizeof(uint32_t), 0, &kerr) < 0) {
		XWARNX("failed to read FastCGI cookie");
		close(fd);
		return(KCGI_FORM);
	} else if (cookie != test) {
		XWARNX("failed to verify FastCGI cookie");
		close(fd);
		return(KCGI_FORM);
	} else if (fullread(fcgi->work.sock[KWORKER_READ], 
		 &rid, sizeof(uint16_t), 0, &kerr) < 0) {
		XWARNX("failed to read FastCGI requestId");
		close(fd);
		return(KCGI_FORM);
	}

	req->arg = arg;
	req->keys = fcgi->keys;
	req->keysz = fcgi->keysz;
	if (NULL == (req->kdata = kdata_alloc(fd, rid)))
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

	kerr = kworker_parent
		(fcgi->work.sock[KWORKER_READ], 
		 req, fcgi->work.pid);
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
