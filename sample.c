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
	void		(*disp)(struct kreq *, struct session *);
	unsigned int	  flags; 
#define	LOGIN 	  	  1 
	int		  mimes[KMIME__MAX]; 
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

static void sendindex(struct kreq *, struct session *);

static const struct dispatch disps[PAGE__MAX] = {
	{ sendindex, 0, {1, 0, 0}}, /* PAGE_INDEX */
};

static void
resp_http_open(struct kreq *req, enum khttp http)
{

	switch (http) {
	case (KHTTP_200):
		break;
	case (KHTTP_303):
		puts("Status: 303 See Other");
		break;
	case (KHTTP_403):
		puts("Status: 403 Forbidden");
		break;
	case (KHTTP_404):
		puts("Status: 404 Page Not Found");
		break;
	case (KHTTP_409):
		puts("Status: 409 Conflict");
		break;
	case (KHTTP_415):
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
resp_head(struct kreq *req, const char *title)
{

	if (KMIME_HTML != req->mime)
		return;

	kdecl();
	kelem(req, KELEM_HTML);
	kelem(req, KELEM_HEAD);
	kelem(req, KELEM_TITLE);
	ktext(NULL == title ? "" : title);
	kclosure(req, 2);
	kelem(req, KELEM_BODY);
}

static void
resp(struct kreq *req, enum khttp http, const char *title)
{

	resp_http_open(req, http);
	resp_http_close();
	resp_head(req, title);
}

static void
resp_close(struct kreq *req)
{

	if (KMIME_HTML != req->mime)
		return;
	while (req->elemsz)
		kclosure(req, 1);
}

#if 0
static void
send303(struct kreq *req, enum page page)
{

	resp_http_open(req, KHTTP_303);
#if 0
	printf("Location: %s\n", pageuri(req, page));
#endif
	resp_http_close();
	if (KMIME_HTML == req->mime) {
		resp_head(req, "Redirecting");
		text("Redirecting.");
	}
	resp_close(req);
}
#endif

static void
sendindex(struct kreq *req, struct session *sess)
{

	resp(req, KHTTP_200, "welcome");
	if (KMIME_HTML == req->mime)
		ktext("welcome.");
	resp_close(req);
}

static void
send403(struct kreq *req)
{

	resp(req, KHTTP_404, "Forbidden");
	if (KMIME_HTML == req->mime)
		ktext("Forbidden.");
	resp_close(req);
}

static void
send404(struct kreq *req)
{

	resp(req, KHTTP_404, "Page Not Found");
	if (KMIME_HTML == req->mime)
		ktext("Page not found.");
	resp_close(req);
}

int
main(int argc, char *argv[])
{
	struct kreq	 r;
	struct session	*s;

	khttp_parse(&r, keys, KEY__MAX, 
		pages, PAGE__MAX, PAGE_INDEX);

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
	if (PAGE__MAX == r.page || KMIME__MAX == r.mime) {
		r.mime = KMIME_HTML;
		send404(&r);
		goto out;
	}

	if (0 == disps[r.page].mimes[r.mime]) {
		/*
		 * The given page doesn't support this MIME.
		 * Send a 415 to indicate so.
		 */
		resp_http_open(&r, KHTTP_415);
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
	khttp_free(&r);
	return(EXIT_SUCCESS);
}

