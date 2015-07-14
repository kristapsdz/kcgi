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

#include <sys/socket.h>

#include <errno.h>
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
	struct kworker	  work;
	int		  curfd;
};

/*
 * Set up a non-blocking read for our next 
 */
static int 
kfcgi_control_read(int *newfd, int socket)
{
	struct msghdr 	 msg;
	char 		 m_buffer[256];
	struct iovec	 io;
	char 		 c_buffer[256];
	struct cmsghdr	*cmsg;
	unsigned char	*data;
	struct pollfd	 pfd;

	memset(&msg, 0, sizeof(struct msghdr));
	memset(&io, 0, sizeof(struct iovec));

	io.iov_base = m_buffer;
	io.iov_len = sizeof(m_buffer);

	msg.msg_iov = &io;
	msg.msg_iovlen = 1;
	msg.msg_control = c_buffer;
	msg.msg_controllen = sizeof(c_buffer);

	pfd.fd = socket;
	pfd.events = POLLIN;
again:
	if (-1 == poll(&pfd, 1, -1)) {
		XWARN("poll: %d, POLLIN", socket);
		return(-1);
	} else if (POLLIN != pfd.revents) {
		XWARN("poll: closed?", socket);
		return(0);
	} else if (recvmsg(socket, &msg, 0) < 0) {
		if (EAGAIN == errno) {
			XWARN("recvmsg: trying again");
			goto again;
		}
		XWARN("recvmsg: %d", socket);
		return(-1);
	}

	cmsg = CMSG_FIRSTHDR(&msg);
	data = CMSG_DATA(cmsg);

	*newfd = *((int *)data);
	return(1);
}

enum kcgi_err
khttp_fcgi_free(struct kfcgi *fcgi)
{
	size_t	 	 i;
	enum kcgi_err	 er;

	fprintf(stderr, "%s: freeing\n", __func__);

	for (i = 0; i < fcgi->mimesz; i++)
		free(fcgi->mimes[i]);

	free(fcgi->mimes);
	free(fcgi->keys);

	if (-1 != fcgi->curfd)
		if (-1 == close(fcgi->curfd))
			XWARN("close: control");

	kworker_kill(&fcgi->work);
	er = kworker_close(&fcgi->work);
	kworker_free(&fcgi->work);
	free(fcgi);
	return(er);
}

enum kcgi_err
khttp_fcgi_initx(struct kfcgi **fcgip, 
	const char *const *mimes, size_t mimesz,
	const struct kvalid *keys, size_t keysz)
{
	enum kcgi_err	 kerr;
	struct kfcgi	*fcgi;
	int 		 er;
	size_t		 i;

	if (NULL == (fcgi = XCALLOC(1, sizeof(struct kfcgi))))
		return(KCGI_ENOMEM);
	*fcgip = fcgi;
	fcgi->curfd = -1;

	fprintf(stderr, "%s: starting up\n", __func__);

	if (KCGI_OK != (kerr = kworker_fcgi_init(&fcgi->work)))
		return(kerr);

	fprintf(stderr, "%s: starting worker\n", __func__);

	if (-1 == (fcgi->work.pid = fork())) {
		er = errno;
		XWARN("fork");
		return(EAGAIN == er ? KCGI_EAGAIN : KCGI_ENOMEM);
	} else if (0 == fcgi->work.pid) {
		kworker_prep_child(&fcgi->work);
		kworker_fcgi_child(&fcgi->work, 
			keys, keysz, mimes, mimesz);
		kworker_free(&fcgi->work);
		_exit(EXIT_SUCCESS);
		/* NOTREACHED */
	}
	kworker_prep_parent(&fcgi->work);

	fprintf(stderr, "%s: initialised\n", __func__);

	kerr = KCGI_ENOMEM;
	fcgi->mimes = XCALLOC(sizeof(char *), mimesz);
	if (NULL == fcgi->mimes)
		goto err;

	for (i = 0; i < mimesz; i++) {
		fcgi->mimes[fcgi->mimesz] = XSTRDUP(mimes[i]);
		if (NULL == fcgi->mimes[i])
			goto err;
		fcgi->mimesz++;
	}

	fcgi->keys = XCALLOC(sizeof(struct kvalid), keysz);

	for (i = 0; i < keysz; i++)
		fcgi->keys[fcgi->keysz++] = keys[i];

	return(KCGI_OK);
err:
	(void)khttp_fcgi_free(fcgi);
	return(kerr);
}

enum kcgi_err
khttp_fcgi_init(struct kfcgi **fcgi, 
	const struct kvalid *keys, size_t keysz)
{

	return(khttp_fcgi_initx(fcgi, 
		kmimetypes, KMIME__MAX, keys, keysz));
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
	ssize_t		 ssz, wsz;
	char		 buf[BUFSIZ];
	struct pollfd	 pfd[2];

	fprintf(stderr, "%s: parsing\n", __func__);

	if (-1 != fcgi->curfd) {
		fprintf(stderr, "%s: closing prior\n", __func__);
		if (-1 == close(fcgi->curfd))
			XWARN("close: control");
		fcgi->curfd = -1;
	}

	memset(req, 0, sizeof(struct kreq));
	kerr = KCGI_ENOMEM;

	if ((c = kfcgi_control_read(&fd, STDIN_FILENO)) < 0)
		return(KCGI_SYSTEM);
	else if (c == 0) {
		fprintf(stderr, "%s: control HUP\n", __func__);
		return(KCGI_HUP);
	}

	fprintf(stderr, "%s: control is GO\n", __func__);

	fcgi->curfd = fd;

	pfd[0].fd = fd;
	pfd[1].fd = fcgi->work.sock[KWORKER_READ];
	pfd[0].events = pfd[1].events = POLLIN;

	/*
	 * Keep writing data into the child context.
	 * It will let us know that all data has been read and processed
	 * when we have a write event from it.
	 * Why is this so fucking retarded?  FastCGI.
	 * It doesn't use an EOF (because the socket is bidirectional)
	 * to let us know when data has finished reading.
	 * This is also why we don't just hand the socket to the child
	 * to read from itself.
	 * TODO: have the first byte from the child be a cookie that
	 * indicates that all data has been read.
	 * Randomly generate the cookie each time.
	 */
	for (;;) {
		if (-1 == poll(pfd, 2, -1)) {
			XWARN("poll: control socket");
			return(KCGI_SYSTEM);
		} else if (POLLIN == pfd[1].revents) {
			/* Read to roll... */
			break;
		} else if (POLLIN != pfd[0].revents) {
			XWARNX("poll: bad events");
			return(KCGI_SYSTEM);
		} else if ((ssz = read(fd, buf, BUFSIZ)) < 0) {
			XWARN("read: control socket");
			return(KCGI_SYSTEM);
		}
		wsz = write(fcgi->work.control[KWORKER_WRITE], buf, ssz);
		if (ssz != wsz) {
			XWARN("write: control socket");
			return(KCGI_SYSTEM);
		}
	}

	req->arg = arg;
	req->keys = fcgi->keys;
	req->keysz = fcgi->keysz;
	/*req->kdata = XCALLOC(1, sizeof(struct kdata));
	if (NULL == req->kdata)
		goto err;*/

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

#if 0
	if (NULL == (cp = getenv("SCRIPT_NAME")))
		req->pname = XSTRDUP("");
	else
		req->pname = XSTRDUP(cp);

	if (NULL == req->pname)
		goto err;

	/* Never supposed to be NULL, but to be sure... */
	if (NULL == (cp = getenv("HTTP_HOST")))
		req->host = XSTRDUP("localhost");
	else
		req->host = XSTRDUP(cp);

	if (NULL == req->host)
		goto err;

	req->port = 80;
	if (NULL != (cp = getenv("SERVER_PORT")))
		req->port = strtonum(cp, 0, 80, NULL);
#endif

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
