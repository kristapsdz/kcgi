/*	$Id$ */
/*
 * Copyright (c) 2014, 2015, 2017 Kristaps Dzonsons <kristaps@bsd.lv>
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
#include <sys/types.h> /* size_t, ssize_t */
#include <stdarg.h> /* va_list */
#include <stddef.h> /* NULL */
#include <stdint.h> /* int64_t */
#include <stdlib.h>
#include <string.h> /* memset */

#include "kcgi.h"
#include "kcgihtml.h"

/*
 * Simple CGI application.
 * Compile it with `make samples` (or using gmake) and install it into
 * your web server's /cgi-bin.
 * The "template.xml" file should be in the /cgi-bin directory as well
 * and readable by the server process.
 * (Obviously this is just for a sample.)
 *
 * Assuming localhost/cgi-bin, the script is localhost/cgi-bin/sample.
 * The pages recognised are:
 *
 *   - /cgi-bin/sample/index.html
 *   - /cgi-bin/sample/template.html
 *   - /cgi-bin/sample/senddata.html
 *
 * See the sendindex et al. functions for what these do.
 */

/* Recognised page requests.  See pages[]. */
enum	page {
	PAGE_INDEX,
	PAGE_TEMPLATE,
	PAGE_SENDDATA,
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
	KEY_PAGECOUNT,
	KEY_PAGESIZE,
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
	TEMPL_REMOTE_ADDR,
	TEMPL__MAX
};

/*
 * We'll use this to route pages by creating an array indexed by our
 * page.
 * Then when the page is parsed, we'll route directly into it.
 */
typedef	void (*disp)(struct kreq *);

static void senddata(struct kreq *);
static void sendindex(struct kreq *);
static void sendtemplate(struct kreq *);

static const disp disps[PAGE__MAX] = {
	sendindex, /* PAGE_INDEX */
	sendtemplate, /* PAGE_TEMPLATE */
	senddata, /* PAGE_SENDDATA */
};

static const struct kvalid keys[KEY__MAX] = {
	{ kvalid_int, "integer" }, /* KEY_INTEGER */
	{ NULL, "file" }, /* KEY_FILE */
	{ kvalid_uint, "count" }, /* KEY_PAGECOUNT */
	{ kvalid_uint, "size" }, /* KEY_PAGESIZE */
};

/*
 * Template key names (as in @@TITLE@@ in the file).
 */
static const char *const templs[TEMPL__MAX] = {
	"title", /* TEMPL_TITLE */
	"name", /* TEMPL_NAME */
	"remote_addr", /* TEMPL_REMOTE_ADDR */
};

/* 
 * Page names (as in the URL component) mapped from the first name part
 * of requests, e.g., /sample.cgi/index.html -> index -> PAGE_INDEX.
 */
static const char *const pages[PAGE__MAX] = {
	"index", /* PAGE_INDEX */
	"template", /* PAGE_TEMPLATE */
	"senddata" /* PAGE_SENDDATA */
};

/*
 * Open an HTTP response with a status code and a particular
 * content-type, then open the HTTP content body.
 * You can call khttp_head(3) before this: CGI doesn't dictate any
 * particular header order.
 */
static void
resp_open(struct kreq *req, enum khttp http)
{
	enum kmime	 mime;

	/*
	 * If we've been sent an unknown suffix like '.foo', we won't
	 * know what it is.
	 * Default to an octet-stream response.
	 */
	if (KMIME__MAX == (mime = req->mime))
		mime = KMIME_APP_OCTET_STREAM;

	khttp_head(req, kresps[KRESP_STATUS], 
		"%s", khttps[http]);
	khttp_head(req, kresps[KRESP_CONTENT_TYPE], 
		"%s", kmimetypes[req->mime]);
	khttp_body(req);
}

/*
 * Callback for filling in a particular template part.
 * Let's just be simple for simplicity's sake.
 */
static int
template(size_t key, void *arg)
{
	struct khtmlreq	*req = arg;

	switch (key) {
	case (TEMPL_TITLE):
		khtml_puts(req, "title");
		break;
	case (TEMPL_NAME):
		khtml_puts(req, "name");
		break;
	case (TEMPL_REMOTE_ADDR):
		khtml_puts(req, req->req->remote);
		break;
	default:
		return(0);
	}

	return(1);
}

/*
 * Demonstrates how to use templates.
 * Returns HTTP 200 and the template content.
 */
static void
sendtemplate(struct kreq *req)
{
	struct ktemplate t;
	struct khtmlreq	 r;

	memset(&t, 0, sizeof(struct ktemplate));
	memset(&r, 0, sizeof(struct khtmlreq));

	r.req = req;
	t.key = templs;
	t.keysz = TEMPL__MAX;
	t.arg = &r;
	t.cb = template;

	resp_open(req, KHTTP_200);
	khttp_template(req, &t, "template.xml");
}

/*
 * Send a random amount of data.
 * Requires KEY_PAGECOUNT (optional), KEY_PAGESIZE (optional).
 * Page count is the number of times we flush a page (with the given
 * size) to the wire.
 * Returns HTTP 200 and the random data.
 */
static void
senddata(struct kreq *req)
{
	int64_t	  i, j, nm, sz;
	char	 *buf;

	nm = 1024 * 1024;
	if (NULL != req->fieldmap[KEY_PAGECOUNT])
		nm = req->fieldmap[KEY_PAGECOUNT]->parsed.i;
	if (0 == nm)
		nm = 1;

	sz = 1;
	if (NULL != req->fieldmap[KEY_PAGESIZE])
		sz = req->fieldmap[KEY_PAGESIZE]->parsed.i;
	if (0 == sz || (uint64_t)sz > SIZE_MAX)
		sz = 1;
	
	buf = kmalloc(sz);

	resp_open(req, KHTTP_200);
	for (i = 0; i < nm; i++) {
		for (j = 0; j < sz; j++)
#ifndef __linux__
			buf[j] = 65 + arc4random_uniform(24);
#else
			buf[j] = 65 + (random() % 24);
#endif
		khttp_write(req, buf, sz);
	}

	free(buf);
}

/*
 * Demonstrates how to use GET and POST forms and building with the HTML
 * builder functions.
 * Returns HTTP 200 and HTML content.
 */
static void
sendindex(struct kreq *req)
{
	char		*page;
	struct khtmlreq	 r;
	const char	*cp;

	cp = NULL == req->fieldmap[KEY_INTEGER] ?
		"" : req->fieldmap[KEY_INTEGER]->val;
	kasprintf(&page, "%s/%s", req->pname, pages[PAGE_INDEX]);

	resp_open(req, KHTTP_200);
	khtml_open(&r, req, 0);
	khtml_elem(&r, KELEM_DOCTYPE);
	khtml_elem(&r, KELEM_HTML);
	khtml_elem(&r, KELEM_HEAD);
	khtml_elem(&r, KELEM_TITLE);
	khtml_puts(&r, "Welcome!");
	khtml_closeelem(&r, 2);
	khtml_elem(&r, KELEM_BODY);
	khtml_puts(&r, "Welcome!");
	khtml_attr(&r, KELEM_FORM,
		KATTR_METHOD, "post",
		KATTR_ENCTYPE, "multipart/form-data",
		KATTR_ACTION, page,
		KATTR__MAX);
	khtml_elem(&r, KELEM_FIELDSET);
	khtml_elem(&r, KELEM_LEGEND);
	khtml_puts(&r, "Post (multipart)");
	khtml_closeelem(&r, 1);
	khtml_elem(&r, KELEM_P);
	khtml_attr(&r, KELEM_INPUT,
		KATTR_TYPE, "number",
		KATTR_NAME, keys[KEY_INTEGER].name,
		KATTR_VALUE, cp, KATTR__MAX);
	khtml_closeelem(&r, 1);
	khtml_elem(&r, KELEM_P);
	khtml_attr(&r, KELEM_INPUT,
		KATTR_TYPE, "file",
		KATTR_MULTIPLE, "",
		KATTR_NAME, keys[KEY_FILE].name,
		KATTR__MAX);
	if (NULL != req->fieldmap[KEY_FILE]) {
		if (NULL != req->fieldmap[KEY_FILE]->file) {
			khtml_puts(&r, "file: ");
			khtml_puts(&r, req->fieldmap[KEY_FILE]->file);
			khtml_puts(&r, " ");
		} 
		if (NULL != req->fieldmap[KEY_FILE]->ctype) {
			khtml_puts(&r, "ctype: ");
			khtml_puts(&r, req->fieldmap[KEY_FILE]->ctype);
		} 
	}
	khtml_closeelem(&r, 1);
	khtml_elem(&r, KELEM_P);
	khtml_attr(&r, KELEM_INPUT,
		KATTR_TYPE, "submit",
		KATTR__MAX);
	khtml_closeelem(&r, 0);
	khtml_close(&r);
	free(page);
}

int
main(void)
{
	struct kreq	 r;
	enum kcgi_err	 er;

	/* Set up our main HTTP context. */

	er = khttp_parse(&r, keys, KEY__MAX, 
		pages, PAGE__MAX, PAGE_INDEX);

	if (KCGI_OK != er)
		return(EXIT_FAILURE);

	/* 
	 * Accept only GET, POST, and OPTIONS.
	 * Restrict to text/html and a valid page.
	 * If all of our parameters are valid, use a dispatch array to
	 * send us to the page handlers.
	 */

	if (KMETHOD_OPTIONS == r.method) {
		khttp_head(&r, kresps[KRESP_ALLOW], 
			"OPTIONS GET POST");
		resp_open(&r, KHTTP_200);
	} else if (KMETHOD_GET != r.method && 
		   KMETHOD_POST != r.method) {
		resp_open(&r, KHTTP_405);
	} else if (PAGE__MAX == r.page || 
		   KMIME_TEXT_HTML != r.mime) {
		resp_open(&r, KHTTP_404);
	} else
		(*disps[r.page])(&r);

	khttp_free(&r);
	return(EXIT_SUCCESS);
}

