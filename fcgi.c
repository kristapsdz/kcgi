/*	$Id$ */
/*
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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/ioctl.h>
#include <sys/socket.h>

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
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
	unsigned int		  debugging;
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
	struct kopts		  opts;
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
 * It listens for FastCGI connections on STDIN_FILENO ("fdaccept") xor
 * for file descriptors (from "fdfiled").
 * When it has one, it reads and passes to the worker (sibling) process,
 * which will be parsing the data.
 * When the worker has finished, it passes back the request identifier,
 * which this passes to the main application for output.
 * This exits when the FastCGI connection fd HUPs.
 * It will close the fdaccept or fdfiled descriptor.
 */
static int
kfcgi_control(int work, int ctrl, int fdaccept, int fdfiled)
{
	struct sockaddr_storage ss;
	socklen_t	 sslen;
	int		 fd, rc, ourfd, erc;
	uint64_t	 magic;
	uint32_t	 cookie, test;
	struct pollfd	 pfd[2];
	char		 buf[BUFSIZ];
	ssize_t		 ssz;
	enum kcgi_err	 kerr;
	uint16_t	 rid, rtest;

	erc = EXIT_FAILURE;
	ourfd = -1 == fdaccept ? fdfiled : fdaccept;
	assert(-1 != ourfd);
	fd = -1;

	if (KCGI_OK != kxsocketprep(ourfd)) {
		XWARNX("manager socket error");
		goto out;
	} 

	for (;;) {
		pfd[0].fd = ourfd;
		pfd[0].events = POLLIN;
		pfd[0].revents = 0;
		pfd[1].fd = ctrl;
		pfd[1].events = POLLIN;
		pfd[1].revents = 0;
		/*
		 * If either the worker or manager disconnect, then exit
		 * cleanly.
		 * The calling application will check the worker's exit
		 * code, which will say whether it did something bad, so
		 * we don't really care.
		 */
		if ((rc = poll(pfd, 2, -1)) < 0) {
			XWARN("poll");
			goto out;
		} else if (0 == rc) {
			XWARNX("poll expired!?");
			continue;
		} else if (POLLHUP & pfd[1].revents ||
			   ! (POLLIN & pfd[0].revents)) 
			break;

		if (-1 != fdaccept) {
			assert(-1 == fdfiled);
			/* 
			 * Blocking accept from FastCGI socket.
			 * This will be round-robined by the kernel so
			 * that other control processes are fairly
			 * notified.
			 */
			sslen = sizeof(ss);
			fd = accept(fdaccept, 
				(struct sockaddr *)&ss, &sslen);
			if (fd < 0) {
				if (EAGAIN == errno || 
				    EWOULDBLOCK == errno)
					continue;
				XWARN("accept");
				goto out;
			} 
		} else {
			assert(-1 != fdfiled);
			/*
			 * Accept a file descriptor from the parent
			 * process in the "new way".
			 * This usually means we're being run by a
			 * kfcgi-like application that's managing
			 * connection counts.
			 */
			rc = fullreadfd(fdfiled, 
				&fd, &magic, sizeof(uint64_t));
			if (rc < 0) {
				XWARNX("manager socket error");
				goto out;
			} else if (0 == rc) {
				/* XWARNX("manager has disconnected"); */
				break;
			}
		}
		
		/* 
		 * We then set that the FastCGI socket is non-blocking,
		 * making it consistent with the behaviour of the CGI
		 * socket, which is also set as such.
		 */
		if (KCGI_OK != kxsocketprep(fd)) {
			XWARNX("work request socket error");
			goto out;
		}

		/* This doesn't need to be crypto quality. */
#ifndef HAVE_ARC4RANDOM
		cookie = random();
#else
		cookie = arc4random();
#endif

		/* Write a header cookie to the work. */
		fullwrite(work, &cookie, sizeof(uint32_t));
#if 0 /* Soon: __OpenBSD__ */
		/*
		 * OpenBSD (and maybe others?) have the ability to
		 * splice using the setsockopt() capability.
		 * Splice so that the worker can read; when the worker
		 * has drained all input, it will notify us.
		 */
		if (-1 == setsockopt(fd, SO_SPLICE, &work)) {
			XWARN("setsockopt: SO_SPLICE");
			goto out;
		}
		pfd[0].fd = work;
		pfd[0].events = POLLIN;
		for (;;) {
			if ((rc = poll(pfd, 1, -1)) < 0) {
				XWARN("poll");
				goto out;
			} else if (0 == rc) {
				XWARNX("poll expired!?");
				continue;
			} else if (POLLIN & pfd[0].revents) 
				break;
			XWARNX("work request poll error");
			goto out;
		}
#else
		/*
		 * Doing this the slow way...
		 * Keep pushing data into the worker til it has read it
		 * all, at which point it will write to us.
		 */
		pfd[0].fd = fd;
		pfd[0].events = POLLIN;
		pfd[1].fd = work;
		pfd[1].events = POLLIN;
		for (;;) {
			if ((rc = poll(pfd, 2, -1)) < 0) {
				XWARN("poll");
				goto out;
			} else if (0 == rc) {
				XWARNX("poll expired!?");
				continue;
			}
			if (POLLIN & pfd[1].revents) {
				/* Child is responding! */
				break;
			} else if ( ! (POLLIN & pfd[0].revents)) {
				XWARNX("work request poll error");
				goto out;
			} else if ((ssz = read(fd, buf, BUFSIZ)) < 0) {
				XWARN("read");
				goto out;
			} else if (0 == ssz) {
				XWARNX("work request empty read");
				goto out;
			}
			rc = fullwritenoerr(pfd[1].fd, buf, ssz);
			if (rc < 0) {
				XWARNX("worker write error");
				goto out;
			} else if (0 == rc) {
				XWARNX("worker has disconnected");
				goto out;
			}
		}
#endif

		/* Now verify that the worker is sane. */
		if (fullread(pfd[1].fd, &test, 
			 sizeof(uint32_t), 0, &kerr) < 0) {
			XWARNX("failed to read FastCGI cookie");
			goto out;
		} else if (cookie != test) {
			XWARNX("failed to verify FastCGI cookie");
			goto out;
		} 
		if (fullread(pfd[1].fd, &rid, 
			 sizeof(uint16_t), 0, &kerr) < 0) {
			XWARNX("failed to read FastCGI requestId");
			goto out;
		}

		/* Doesn't need to be crypto quality. */
#ifndef HAVE_ARC4RANDOM
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
			goto out;
		}

		/* 
		 * This will wait til the application is finished.
		 * It will then double-check the requestId. 
		 */
		if (fullread(ctrl, &rtest, sizeof(uint16_t), 0, &kerr) < 0) {
			XWARNX("failed to read FastCGI cookie");
			goto out;
		} else if (rid != rtest) {
			XWARNX("failed to verify FastCGI requestId");
			goto out;
		}

		/*
		 * If we are being passed descriptors (instead of
		 * waiting on the accept()), then notify the manager
		 * that we've finished processing this request.
		 */
		if (-1 != fdfiled) {
			rc = fullwritenoerr(fdfiled, 
				&magic, sizeof(uint64_t));
			if (rc < 0) {
				XWARNX("failed ack to manager");
				goto out;
			} else if (0 == rc) {
				XWARNX("manager has exited");
				break;
			}
		}

		close(fd);
		fd = -1;
	}

	erc = EXIT_SUCCESS;
out:
	if (-1 != fd)
		close(fd);
	close(ourfd);
	return(erc);
}

void
khttp_fcgi_child_free(struct kfcgi *fcgi)
{

	close(fcgi->sock_ctl);
	close(fcgi->work_dat);
	ksandbox_free(fcgi->work_box);
	free(fcgi);
}

enum kcgi_err
khttp_fcgi_free(struct kfcgi *fcgi)
{

	/* Allow a NULL pointer. */
	if (NULL == fcgi)
		return(KCGI_OK);

	close(fcgi->sock_ctl);
	close(fcgi->work_dat);
	kxwaitpid(fcgi->work_pid);
	kxwaitpid(fcgi->sock_pid);
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
	size_t defpage, void *arg, void (*argfree)(void *),
	unsigned int debugging, const struct kopts *opts)
{
	struct kfcgi	*fcgi;
	int 		 er, fdaccept, fdfiled;
	int		 work_ctl[2], work_dat[2], sock_ctl[2];
	void		*work_box, *sock_box;
	pid_t		 work_pid, sock_pid;
	const char	*cp, *ercp;
	sigset_t	 mask;
	enum sandtype	 st;

	/*
	 * Determine whether we're supposed to accept() on a socket or,
	 * rather, we're supposed to receive file descriptors from a
	 * kfcgi-like manager.
	 */
	st = SAND_CONTROL_OLD;
	fdaccept = fdfiled = -1;
	if (NULL != (cp = getenv("FCGI_LISTENSOCK_DESCRIPTORS"))) {
		fdfiled = strtonum(cp, 0, INT_MAX, &ercp);
		if (NULL != ercp) {
			fdaccept = STDIN_FILENO;
			fdfiled = -1;
		} else
			st = SAND_CONTROL_NEW;
	} else
		fdaccept = STDIN_FILENO;

	/*
	 * Block this signal unless we're right at the fullreadfd
	 * function, at which point unblock and let it interrupt us.
	 * We don't save the signal mask because we're allowed free
	 * reign on the SIGTERM value.
	 */
	signal(SIGTERM, dosignal);
	sigemptyset(&mask);
	sigaddset(&mask, SIGTERM);
	sigprocmask(SIG_BLOCK, &mask, NULL);
	sig = 0;

	if ( ! ksandbox_alloc(&work_box))
		return(KCGI_ENOMEM);

	if ( ! ksandbox_alloc(&sock_box)) {
		ksandbox_free(work_box);
		return(KCGI_ENOMEM);
	}

	if (KCGI_OK != kxsocketpair
		 (AF_UNIX, SOCK_STREAM, 0, work_ctl)) {
		ksandbox_free(work_box);
		ksandbox_free(sock_box);
		return(KCGI_SYSTEM);
	}

	if (KCGI_OK != kxsocketpair
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
		signal(SIGTERM, SIG_IGN);
		if (NULL != argfree)
			argfree(arg);
		/* 
		 * STDIN_FILENO isn't really stdin, it's the control
		 * socket used to pass input sockets to us.
		 * Thus, close the parent's control socket. 
		 */
		if (-1 != fdaccept)
			close(fdaccept);
		if (-1 != fdfiled)
			close(fdfiled);
		close(STDOUT_FILENO);
		close(work_dat[KWORKER_PARENT]);
		close(work_ctl[KWORKER_PARENT]);
		if ( ! ksandbox_init_child(work_box, 
			 SAND_WORKER,
			 work_dat[KWORKER_CHILD],
			 work_ctl[KWORKER_CHILD], -1, -1)) {
			XWARNX("ksandbox_init_child");
			er = EXIT_FAILURE;
		} else {
			kworker_fcgi_child
				(work_dat[KWORKER_CHILD],
				 work_ctl[KWORKER_CHILD],
				 keys, keysz, mimes, mimesz,
				 debugging);
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
		kxwaitpid(work_pid);
		ksandbox_free(work_box);
		ksandbox_free(sock_box);
		return(KCGI_SYSTEM);
	}

	if (KCGI_OK != kxsocketpair
		 (AF_UNIX, SOCK_STREAM, 0, sock_ctl)) {
		close(work_dat[KWORKER_PARENT]);
		close(work_ctl[KWORKER_PARENT]);
		kxwaitpid(work_pid);
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
		kxwaitpid(work_pid);
		ksandbox_close(work_box);
		ksandbox_free(work_box);
		ksandbox_free(sock_box);
		return(EAGAIN == er ? KCGI_EAGAIN : KCGI_ENOMEM);
	} else if (0 == sock_pid) {
		signal(SIGTERM, SIG_IGN);
		if (NULL != argfree)
			argfree(arg);
		close(STDOUT_FILENO);
		close(work_dat[KWORKER_PARENT]);
		close(sock_ctl[KWORKER_PARENT]);
		ksandbox_free(work_box);
		if ( ! ksandbox_init_child(sock_box, 
			 st, sock_ctl[KWORKER_CHILD], -1,
			 fdfiled, fdaccept)) {
			XWARNX("ksandbox_init_child");
			er = EXIT_FAILURE;
		} else 
			er = kfcgi_control
				(work_ctl[KWORKER_PARENT], 
				 sock_ctl[KWORKER_CHILD],
				 fdaccept, fdfiled);
		close(work_ctl[KWORKER_PARENT]);
		close(sock_ctl[KWORKER_CHILD]);
		ksandbox_free(sock_box);
		_exit(er);
		/* NOTREACHED */
	}

	close(sock_ctl[KWORKER_CHILD]);
	close(work_ctl[KWORKER_PARENT]);
	if (-1 != fdaccept)
		close(fdaccept);
	if (-1 != fdfiled)
		close(fdfiled);

	if ( ! ksandbox_init_parent(sock_box, st, sock_pid)) {
		XWARNX("ksandbox_init_parent");
		close(sock_ctl[KWORKER_PARENT]);
		close(work_dat[KWORKER_PARENT]);
		kxwaitpid(work_pid);
		kxwaitpid(sock_pid);
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
		kxwaitpid(work_pid);
		kxwaitpid(sock_pid);
		ksandbox_close(work_box);
		ksandbox_free(work_box);
		ksandbox_close(sock_box);
		ksandbox_free(sock_box);
		return(KCGI_ENOMEM);
	}

	if (NULL == opts)
		fcgi->opts.sndbufsz = -1;
	else
		memcpy(&fcgi->opts, opts, sizeof(struct kopts));

	if (fcgi->opts.sndbufsz < 0)
		fcgi->opts.sndbufsz = UINT16_MAX;

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
	fcgi->debugging = debugging;
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
		NULL, NULL, 0, NULL));
}

/*
 * Here we wait for the next FastCGI connection in such a way that, if
 * we're notified that we must exit via a SIGTERM, we'll properly close
 * down without spurious warnings.
 */
static int 
fcgi_waitread(int fd)
{
	int		 rc;
	struct pollfd	 pfd;
	sigset_t	 mask;

again:
	pfd.fd = fd;
	pfd.events = POLLIN;

	/* Unblock SIGTERM around the poll(). */
	sigemptyset(&mask);
	sigaddset(&mask, SIGTERM);
	sigprocmask(SIG_UNBLOCK, &mask, NULL);
	rc = poll(&pfd, 1, 1000);
	sigprocmask(SIG_UNBLOCK, &mask, NULL);

	/* Exit signal has been set. */
	if (sig) {
		sig = 0;
		return(0);
	}

	/* Problems?  Exit.  Timeout?  Retry. */
	if (rc < 0) {
		XWARN("poll");
		return(-1);
	} else if (0 == rc) 
		goto again;

	/* Only POLLIN is a "good" exit from this. */
	if (POLLIN & pfd.revents)
		return(1);
	else if (POLLHUP & pfd.revents)
		return(0);

	XWARNX("poll: error");
	return(-1);
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
	 * It may also decide to exit, which we note by seeing that "sig" has
	 * been set or the channel closed gracefully.
	 */
	c = fcgi_waitread(fcgi->sock_ctl);
	if (c < 0)
		return(KCGI_SYSTEM);
	else if (0 == c)
		return(KCGI_HUP);

	c = fullreadfd(fcgi->sock_ctl, &fd, &rid, sizeof(uint16_t));
	if (c < 0 && ! sig) {
		XWARNX("fullreadfd");
		return(KCGI_SYSTEM);
	} else if (0 == c || (c < 0 && sig)) {
		XWARNX("application signalled");
		return(KCGI_HUP);
	}

	req->arg = fcgi->arg;
	req->keys = fcgi->keys;
	req->keysz = fcgi->keysz;
	req->kdata = kdata_alloc(fcgi->sock_ctl, 
		fd, rid, fcgi->debugging, &fcgi->opts);
	if (NULL == req->kdata)
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
	 */
	kerr = kworker_parent(fcgi->work_dat, req, 0);
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
	return(kerr);
}

int
khttp_fcgi_test(void)
{
	socklen_t	 len = 0;
	const char	*cp, *ercp;

	if (NULL != (cp = getenv("FCGI_LISTENSOCK_DESCRIPTORS"))) {
		(void)strtonum(cp, 0, INT_MAX, &ercp);
		if (NULL != ercp) 
			return(1);
	} 

	if (-1 != getpeername(STDIN_FILENO, NULL, &len))
		return(0);
	return(ENOTCONN == errno);
}
