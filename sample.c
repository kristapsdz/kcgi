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

/*
 * All of the pages we're going to display.
 */
enum	page {
	PAGE_INDEX,
	PAGE_TEMPLATE,
	PAGE__MAX
};

/*
 * All of the keys we accept. 
 * See sendindex() for how these are used.
 */
enum	key {
	KEY_INTEGER1, 
	KEY_INTEGER2, 
	KEY_INTEGER3, 
	KEY_FILE,
	KEY__MAX
};

/*
 * The elements in our template file.
 * See sendtemplate() for how this is used.
 */
enum	templ {
	TEMPL_TITLE,
	TEMPL_NAME,
	TEMPL__MAX
};

/*
 * A dispatcher for our pages and their MIME types.
 * We'll use this to route pages.
 * We also specify which MIME types are assigned to which pages.
 */
struct	dispatch {
	void		(*disp)(struct kreq *);
	int		  mimes[KMIME__MAX]; 
};

static void sendindex(struct kreq *);
static void sendtemplate(struct kreq *);

static const struct dispatch disps[PAGE__MAX] = {
	{ sendindex, {1, 0, 0}}, /* PAGE_INDEX */
	{ sendtemplate, {1, 0, 0}}, /* PAGE_TEMPLATE */
};

const struct kvalid keys[KEY__MAX] = {
	{ kvalid_int, "integer1", KFIELD_NUMBER }, /* KEY_INTEGER1 */
	{ kvalid_int, "integer2", KFIELD_NUMBER }, /* KEY_INTEGER2 */
	{ kvalid_int, "integer3", KFIELD_NUMBER }, /* KEY_INTEGER3 */
	{ NULL, "file", KFIELD__MAX }, /* KEY_FILE */
};

/*
 * Template key names (as in @@TITLE@@ in the file).
 */
const char *const templs[TEMPL__MAX] = {
	"title", /* TEMPL_TITLE */
	"name" /* TEMPL_NAME */
};

/* 
 * Page names (as in the URL component).
 */
const char *const pages[PAGE__MAX] = {
	"index", /* PAGE_INDEX */
	"template" /* PAGE_TEMPLATE */
};

/*
 * Open an HTTP response with a status code and a particular
 * content-type, then open the HTTP content body.
 * We'll usually just use the same content type...
 */
static void
resp_open(struct kreq *req, enum khttp http)
{

	khead(req, "Status", khttps[http]);
	khead(req, "Content-Type", kmimetypes[req->mime]);
	kbody(req);
}

/*
 * Callback for filling in a particular template part.
 * Let's just be simple for simplicity's sake.
 */
static int
template(size_t key, void *arg)
{
	struct kreq	*req = arg;

	switch (key) {
	case (TEMPL_TITLE):
		ktext(req, "title");
		break;
	case (TEMPL_NAME):
		ktext(req, "name");
		break;
	default:
		abort();
	}

	return(1);
}

/*
 * Send the "templated" file.
 * This demonstrates how to use templates.
 */
static void
sendtemplate(struct kreq *req)
{
	struct ktemplate t;

	memset(&t, 0, sizeof(struct ktemplate));

	t.key = templs;
	t.keysz = TEMPL__MAX;
	t.arg = req;
	t.cb = template;

	resp_open(req, KHTTP_200);
	ktemplate(&t, "template.xml");
}

/*
 * Send the "index" page.
 * This demonstrates how to use GET and POST forms.
 */
static void
sendindex(struct kreq *req)
{
	size_t	 sv;
	char	*page;

	asprintf(&page, "%s/%s", pname, pages[PAGE_INDEX]);
	resp_open(req, KHTTP_200);
	kdecl(req);
	kelem(req, KELEM_HTML);
	kelem(req, KELEM_HEAD);
	kelem(req, KELEM_TITLE);
	ktext(req, "Welcome!");
	kclosure(req, 2);
	kelem(req, KELEM_BODY);
	ktext(req, "Welcome!");

	/* Start with a standard url-encoded POST form. */
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

	/* Now process a GET form. */
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

	/* Lastly, process a multipart POST form. */
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
	kclosure(req, 0);
	free(page);
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
		 */
		resp_open(&r, KHTTP_404);
		if (KMIME_HTML == r.mime)
			ktext(&r, "Page not found.");
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

