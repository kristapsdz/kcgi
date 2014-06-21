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
 * These will be mapped from the first name part of requests, e.g.,
 * /sample.cgi/index.html will map index.html into PAGE_INDEX.
 * The names are in the "pages" array.
 */
enum	page {
	PAGE_INDEX,
	PAGE_TEMPLATE,
	PAGE__MAX
};

/*
 * All of the keys (input field names) we accept. 
 * The key names are in the "keys" array.
 * See sendindex() for how these are used.
 */
enum	key {
	KEY_INTEGER, 
	KEY_FILE,
	KEY__MAX
};

/*
 * The elements in our template file.
 * The element key names are in the "templs" array.
 * See sendtemplate() for how this is used.
 */
enum	templ {
	TEMPL_TITLE,
	TEMPL_NAME,
	TEMPL__MAX
};

/*
 * We'll use this to route pages by creating an array indexed by our
 * page.
 * Then when the page is parsed, we'll route directly into it.
 */
typedef	void (*disp)(struct kreq *);

static void sendindex(struct kreq *);
static void sendtemplate(struct kreq *);

static const disp disps[PAGE__MAX] = {
	sendindex, /* PAGE_INDEX */
	sendtemplate, /* PAGE_TEMPLATE */
};

static const struct kvalid keys[KEY__MAX] = {
	{ kvalid_int, "integer" }, /* KEY_INTEGER */
	{ NULL, "file" }, /* KEY_FILE */
};

/*
 * Template key names (as in @@TITLE@@ in the file).
 */
static const char *const templs[TEMPL__MAX] = {
	"title", /* TEMPL_TITLE */
	"name" /* TEMPL_NAME */
};

/* 
 * Page names (as in the URL component).
 */
static const char *const pages[PAGE__MAX] = {
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

	khttp_head(req, "Status", "%s", khttps[http]);
	khttp_head(req, "Content-Type", "%s", kmimetypes[req->mime]);
	khttp_body(req);
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
		khtml_text(req, "title");
		break;
	case (TEMPL_NAME):
		khtml_text(req, "name");
		break;
	default:
		abort();
	}

	return(1);
}

/*
 * Send the "templated" file.
 * This demonstrates how to use templates.
 * One will usually have more powerful machinery to look up the XML file
 * in a template directory, or possibly populate it from a database via
 * khttp_template_buf().
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
	khttp_template(req, &t, "template.xml");
}

/*
 * Send the "index" page.
 * This demonstrates how to use GET and POST forms and building with the
 * HTML builder functions.
 * You probably want to use the template instead...
 */
static void
sendindex(struct kreq *req)
{
	char		*page;
	const char	*cp;

	(void)kasprintf(&page, "%s/%s", 
		req->pname, pages[PAGE_INDEX]);
	resp_open(req, KHTTP_200);
	khtml_elem(req, KELEM_DOCTYPE);
	khtml_elem(req, KELEM_HTML);
	khtml_elem(req, KELEM_HEAD);
	khtml_elem(req, KELEM_TITLE);
	khtml_text(req, "Welcome!");
	khtml_close(req, 2);
	khtml_elem(req, KELEM_BODY);
	khtml_text(req, "Welcome!");
	khtml_attr(req, KELEM_FORM,
		KATTR_METHOD, "post",
		KATTR_ENCTYPE, "multipart/form-data",
		KATTR_ACTION, page,
		KATTR__MAX);
	khtml_elem(req, KELEM_FIELDSET);
	khtml_elem(req, KELEM_LEGEND);
	khtml_text(req, "Post (multipart)");
	khtml_close(req, 1);
	khtml_elem(req, KELEM_P);
	cp = NULL == req->fieldmap[KEY_INTEGER] ?
		"" : req->fieldmap[KEY_INTEGER]->val;
	khtml_attr(req, KELEM_INPUT,
		KATTR_TYPE, "number",
		KATTR_NAME, keys[KEY_INTEGER].name,
		KATTR_VALUE, cp, KATTR__MAX);
	khtml_close(req, 1);
	khtml_elem(req, KELEM_P);
	khtml_attr(req, KELEM_INPUT,
		KATTR_TYPE, "file",
		KATTR_MULTIPLE, "",
		KATTR_NAME, keys[KEY_FILE].name,
		KATTR__MAX);
	if (NULL != req->fieldmap[KEY_FILE]) {
		if (NULL != req->fieldmap[KEY_FILE]->file) {
			khtml_text(req, "file: ");
			khtml_text(req, req->fieldmap[KEY_FILE]->file);
			khtml_text(req, " ");
		} 
		if (NULL != req->fieldmap[KEY_FILE]->ctype) {
			khtml_text(req, "ctype: ");
			khtml_text(req, req->fieldmap[KEY_FILE]->ctype);
		} 
	}
	khtml_close(req, 1);
	khtml_elem(req, KELEM_P);
	khtml_attr(req, KELEM_INPUT,
		KATTR_TYPE, "submit",
		KATTR__MAX);
	khtml_close(req, 0);
	free(page);
}

int
main(void)
{
	struct kreq	 r;

	/* Set up our main HTTP context. */
	if ( ! khttp_parse(&r, keys, KEY__MAX, 
		pages, PAGE__MAX, PAGE_INDEX, NULL, NULL))
		return(EXIT_FAILURE);

	if (PAGE__MAX == r.page || KMIME_HTML != r.mime) {
		/*
		 * We've been asked for an unknown page or something
		 * with an unknown extension.
		 */
		resp_open(&r, KHTTP_404);
		if (KMIME_HTML == r.mime)
			khtml_text(&r, "Page not found.");
	} else
		(*disps[r.page])(&r);

	khttp_free(&r);
	return(EXIT_SUCCESS);
}

