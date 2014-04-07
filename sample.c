/*	$Id$ */
#include <sys/types.h>

#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "kcgi.h"

enum	page {
	PAGE_INDEX,
	PAGE__MAX
};

enum	key {
	KEY_SESSID, 
	KEY_SESSCOOKIE, 
	KEY__MAX
};

struct	user {
	char		  name[257];
	int64_t		  id;
};

struct	session {
	struct user	  user;
	int64_t		  id;
};

struct	dispatch {
	void		(*disp)(struct req *, struct session *);
	unsigned int	  flags; 
#define	LOGIN 	  	  1 
	int		  mimes[MIME__MAX]; 
};

const char * const pages[PAGE__MAX] = {
	"index", /* PAGE_INDEX */
};

#define	PAGE_NEEDLOGIN PAGE_INDEX

const struct kvalid keys[KEY__MAX] = {
#if 0
	{ kvalid_mail, "email", KFIELD_EMAIL, "E-mail Address", NULL },
	{ kvalid_pass, "password", KFIELD_PASSWORD, "Password", NULL }, /* KEY_PASS */
	{ kvalid_id, "userid", KFIELD__MAX, NULL, NULL }, /* KEY_USERID */
#endif
	{ kvalid_int, "sessid", KFIELD__MAX, NULL, NULL }, /* KEY_SESSID */
	{ kvalid_int, "sesscookie", KFIELD__MAX, NULL, NULL }, /* KEY_SESSCOOKIE */
};

static void sendindex(struct req *, struct session *);

static const struct dispatch disps[PAGE__MAX] = {
	{ sendindex, 0, {1, 0, 0}}, /* PAGE_INDEX */
};

static void
resp_http_open(struct req *req, enum http http)
{

	switch (http) {
	case (HTTP_200):
		break;
	case (HTTP_303):
		puts("Status: 303 See Other");
		break;
	case (HTTP_403):
		puts("Status: 403 Forbidden");
		break;
	case (HTTP_404):
		puts("Status: 404 Page Not Found");
		break;
	case (HTTP_409):
		puts("Status: 409 Conflict");
		break;
	case (HTTP_415):
		puts("Status: 415 Unsuppoted Media Type");
		break;
	default:
		abort();
		/* NOTREACHED */
	}

	printf("Content-Type: %s\n", mimetypes[req->mime]);
}

static void
resp_http_close(void)
{

	putchar('\n');
	fflush(stdout);
}

static void
resp_head(struct req *req, const char *title)
{

	if (MIME_HTML != req->mime)
		return;

	decl();
	elem(req, ELEM_HTML);
	elem(req, ELEM_HEAD);
	elem(req, ELEM_TITLE);
	text(NULL == title ? "" : title);
	closure(req, 2);
	elem(req, ELEM_BODY);
}

static void
resp(struct req *req, enum http http, const char *title)
{

	resp_http_open(req, http);
	resp_http_close();
	resp_head(req, title);
}

static void
resp_close(struct req *req)
{

	if (MIME_HTML != req->mime)
		return;

	while (req->elemsz) {
		if (ELEM_BODY == req->elems[req->elemsz - 1]) 
			break;
		closure(req, 1);
	}

	while (req->elemsz)
		closure(req, 1);
}

#if 0
static void
send303(struct req *req, enum page page)
{

	resp_http_open(req, HTTP_303);
#if 0
	printf("Location: %s\n", pageuri(req, page));
#endif
	resp_http_close();
	if (MIME_HTML == req->mime) {
		resp_head(req, "Redirecting");
		text("Redirecting.");
	}
	resp_close(req);
}
#endif

static void
sendindex(struct req *req, struct session *sess)
{

	resp(req, HTTP_200, "welcome");
	if (MIME_HTML == req->mime)
		text("welcome.");
	resp_close(req);
}

static void
send403(struct req *req)
{

	resp(req, HTTP_404, "Forbidden");
	if (MIME_HTML == req->mime)
		text("Forbidden.");
	resp_close(req);
}

static void
send404(struct req *req)
{

	resp(req, HTTP_404, "Page Not Found");
	if (MIME_HTML == req->mime)
		text("Page not found.");
	resp_close(req);
}

int
main(int argc, char *argv[])
{
	struct req	 r;
	struct session	*s;

	http_parse(&r, keys, KEY__MAX, pages, PAGE__MAX, PAGE_INDEX);

	s = NULL;
#if 0
	if (NULL != r.cookiemap[KEY_USERID] &&
			NULL != r.cookiemap[KEY_SESSID] &&
			NULL != r.cookiemap[KEY_SESSCOOKIE])
		s = session_lookup
			(r.cookiemap[KEY_SESSID]->parsed.i,
			 r.cookiemap[KEY_USERID]->parsed.i,
			 r.cookiemap[KEY_SESSCOOKIE]->parsed.i,
			 r.page);
#endif
	/*
	 * We've been asked for an unknown page or something with an
	 * unknown extension.
	 * Re-write our response as HTML and send a 404.
	 */
	if (PAGE__MAX == r.page || MIME__MAX == r.mime) {
		r.mime = MIME_HTML;
		send404(&r);
		goto out;
	}

	if (0 == disps[r.page].mimes[r.mime]) {
		/*
		 * The given page doesn't support this MIME.
		 * Send a 415 to indicate so.
		 */
		resp_http_open(&r, HTTP_415);
		resp_http_close();
		goto out;
	} else if (NULL == s && LOGIN & disps[r.page].flags) {
		/*
		 * This page demands that we be logged in.
		 * Send is to the login page.
		 */
		send403(&r);
		goto out;
	} 

	(*disps[r.page].disp)(&r, s);
out:
	free(s);
	http_free(&r);
	return(EXIT_SUCCESS);
}

