/*	$Id$ */
#include <sys/types.h>

#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "kcgi.h"

enum	page {
	PAGE_INDEX,
	PAGE__MAX
};

enum	key {
	KEY_INTEGER1, 
	KEY_INTEGER2, 
	KEY_INTEGER3, 
	KEY_FILE,
	KEY__MAX
};

struct	dispatch {
	void		(*disp)(struct kreq *);
	int		  mimes[KMIME__MAX]; 
};

static void sendindex(struct kreq *);

static const struct dispatch disps[PAGE__MAX] = {
	{ sendindex, {1, 0, 0}}, /* PAGE_INDEX */
};

const struct kvalid keys[KEY__MAX] = {
	{ kvalid_int, "integer1", KFIELD_NUMBER }, /* KEY_INTEGER1 */
	{ kvalid_int, "integer2", KFIELD_NUMBER }, /* KEY_INTEGER2 */
	{ kvalid_int, "integer3", KFIELD_NUMBER }, /* KEY_INTEGER3 */
	{ NULL, "file", KFIELD__MAX }, /* KEY_FILE */
};

const char *const pages[PAGE__MAX] = {
	"index"
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

	if (KMIME_HTML == req->mime)
		kclosure(req, 0);
}

static void
sendindex(struct kreq *req)
{
	size_t	 sv;
	char	*page;

	asprintf(&page, "%s/%s", pname, pages[PAGE_INDEX]);
	resp(req, KHTTP_200, "Welcome");
	if (KMIME_HTML != req->mime)
		resp_close(req);
	ktext(req, "Welcome!");
	sv = kelemsave(req);
	kattr(req, KELEM_FORM,
		KATTR_METHOD, "post",
		KATTR_ACTION, page,
		KATTR__MAX);
	kelem(req, KELEM_FIELDSET);
	kelem(req, KELEM_LEGEND);
	ktext(req, "Post");
	kclosure(req, 1);
	kelem(req, KELEM_P);
	kinput(req, KEY_INTEGER1);
	kclosure(req, 1);
	kelem(req, KELEM_P);
	kattr(req, KELEM_INPUT,
		KATTR_TYPE, "submit",
		KATTR__MAX);
	kclosureto(req, sv);
	sv = kelemsave(req);
	kattr(req, KELEM_FORM,
		KATTR_METHOD, "get",
		KATTR_ACTION, page,
		KATTR__MAX);
	kelem(req, KELEM_FIELDSET);
	kelem(req, KELEM_LEGEND);
	ktext(req, "Get");
	kclosure(req, 1);
	kelem(req, KELEM_P);
	kinput(req, KEY_INTEGER2);
	kclosure(req, 1);
	kelem(req, KELEM_P);
	kattr(req, KELEM_INPUT,
		KATTR_TYPE, "submit",
		KATTR__MAX);
	kclosureto(req, sv);
	kattr(req, KELEM_FORM,
		KATTR_METHOD, "post",
		KATTR_ENCTYPE, "multipart/form-data",
		KATTR_ACTION, page,
		KATTR__MAX);
	kelem(req, KELEM_FIELDSET);
	kelem(req, KELEM_LEGEND);
	ktext(req, "Post (multipart)");
	kclosure(req, 1);
	kelem(req, KELEM_P);
	kinput(req, KEY_INTEGER3);
	kclosure(req, 1);
	kelem(req, KELEM_P);
	kattr(req, KELEM_INPUT,
		KATTR_TYPE, "file",
		KATTR_NAME, keys[KEY_FILE].name,
		KATTR__MAX);
	if (NULL != req->fieldmap[KEY_FILE]) {
		if (NULL != req->fieldmap[KEY_FILE]->file) {
			ktext(req, "file: ");
			ktext(req, req->fieldmap[KEY_FILE]->file);
			ktext(req, " ");
		} 
		if (NULL != req->fieldmap[KEY_FILE]->ctype) {
			ktext(req, "ctype: ");
			ktext(req, req->fieldmap[KEY_FILE]->ctype);
		} 
	}
	kclosure(req, 1);
	kelem(req, KELEM_P);
	kattr(req, KELEM_INPUT,
		KATTR_TYPE, "submit",
		KATTR__MAX);
	resp_close(req);
	free(page);
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
	} else
		(*disps[r.page].disp)(&r);

	khttp_free(&r);
	return(EXIT_SUCCESS);
}

