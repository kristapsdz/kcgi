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
	KEY__MAX
};

struct	dispatch {
	void		(*disp)(struct kreq *);
	unsigned int	  flags; 
#define	LOGIN 	  	  1 
	int		  mimes[KMIME__MAX]; 
};

const char * const pages[PAGE__MAX] = {
	"index", /* PAGE_INDEX */
};

#define	PAGE_NEEDLOGIN PAGE_INDEX

const struct kvalid keys[KEY__MAX] = {
	{ kvalid_int, "sessid" }, /* KEY_SESSID */
};

static void sendindex(struct kreq *);

static const struct dispatch disps[PAGE__MAX] = {
	{ sendindex, 0, {1, 0, 0}}, /* PAGE_INDEX */
};

static void
resp_open(struct kreq *req, enum khttp http)
{

	khead(req, "Status", khttps[http]);
	khead(req, "Content-Type", kmimetypes[req->mime]);
	kbody(req);
}

static void
resp_head(struct kreq *req, const char *title)
{

	if (KMIME_HTML != req->mime)
		return;

	kdecl(req);
	kelem(req, KELEM_HTML);
	kelem(req, KELEM_HEAD);
	kelem(req, KELEM_TITLE);
	ktext(req, NULL == title ? "" : title);
	kclosure(req, 2);
	kelem(req, KELEM_BODY);
}

static void
resp(struct kreq *req, enum khttp http, const char *title)
{

	resp_open(req, http);
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

static void
sendindex(struct kreq *req)
{

	resp(req, KHTTP_200, "Welcome");
	if (KMIME_HTML == req->mime)
		ktext(req, "Welcome!");
	resp_close(req);
}

static void
send403(struct kreq *req)
{

	resp(req, KHTTP_404, "Forbidden");
	if (KMIME_HTML == req->mime)
		ktext(req, "Forbidden.");
	resp_close(req);
}

static void
send404(struct kreq *req)
{

	resp(req, KHTTP_404, "Page Not Found");
	if (KMIME_HTML == req->mime)
		ktext(req, "Page not found.");
	resp_close(req);
}

int
main(void)
{
	struct kreq	 r;

	/* Set up our main HTTP context. */
	khttp_parse(&r, keys, KEY__MAX, 
		pages, PAGE__MAX, PAGE_INDEX);

	if (PAGE__MAX == r.page || KMIME__MAX == r.mime) {
		/*
		 * We've been asked for an unknown page or something
		 * with an unknown extension.
		 * Re-write our response as HTML and send a 404.
		 */
		r.mime = KMIME_HTML;
		send404(&r);
	} else if (0 == disps[r.page].mimes[r.mime]) {
		/*
		 * The given page doesn't support this MIME.
		 * Send a 415 to indicate so.
		 */
		resp_open(&r, KHTTP_415);
	} else if (LOGIN & disps[r.page].flags) {
		/*
		 * This page demands that we be logged in.
		 * Send is to the login page.
		 */
		send403(&r);
	} else
		(*disps[r.page].disp)(&r);

	khttp_free(&r);
	return(EXIT_SUCCESS);
}

