/*	$Id$ */
/*
 * Copyright (c) 2012, 2014 Kristaps Dzonsons <kristaps@bsd.lv>
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
#ifndef KCGI_H
#define KCGI_H

#define	VERSION "@VERSION@"

/* The following header should contain these enums. */
/* 
 * enum	page {
 * 	PAGE_INDEX,
 * 	PAGE__MAX
 * };
 * 
 * enum	key {
 * 	KEY_SESSID, 
 * 	KEY_SESSCOOKIE, 
 * 	KEY__MAX
 * };
 */
#include "kcgi-local.h"

enum	http {
	HTTP_200,
	HTTP_303,
	HTTP_403,
	HTTP_404,
	HTTP_409,
	HTTP_415,
	HTTP__MAX
};

enum	entity {
	ENTITY_EACUTE,
	ENTITY_GT,
	ENTITY_LARR,
	ENTITY_LT,
	ENTITY_MDASH,
	ENTITY_NDASH,
	ENTITY_RARR,
	ENTITY__MAX
};

enum	attr {
	ATTR_HTTP_EQUIV,
	ATTR_CONTENT,
	ATTR_REL,
	ATTR_HREF,
	ATTR_TYPE,
	ATTR_ACTION,
	ATTR_METHOD,
	ATTR_NAME,
	ATTR_VALUE,
	ATTR_ONCLICK,
	ATTR_ID,
	ATTR_FOR,
	ATTR_CLASS,
	ATTR_COLSPAN,
	ATTR_DISABLED,
	ATTR_SELECTED,
	ATTR_SRC,
	ATTR_CLEAR,
	ATTR_CHECKED,
	ATTR_STYLE,
	ATTR_TARGET,
	ATTR_STEP,
	ATTR_MIN,
	ATTR_MAX,
	ATTR_WIDTH,
	ATTR_SPAN,
	ATTR_ROWSPAN,
	ATTR__MAX
};

enum	mime {
	MIME_HTML,
	MIME_CSV,
	MIME_PNG,
	MIME__MAX
};

enum	elem {
	ELEM_HTML,
	ELEM_HEAD,
	ELEM_BODY,
	ELEM_TITLE,
	ELEM_META,
	ELEM_LINK,
	ELEM_FORM,
	ELEM_INPUT,
	ELEM_TEXTAREA,
	ELEM_P,
	ELEM_BLOCKQUOTE,
	ELEM_BR,
	ELEM_FIELDSET,
	ELEM_LEGEND,
	ELEM_LABEL,
	ELEM_A,
	ELEM_CODE,
	ELEM_DIV,
	ELEM_SPAN,
	ELEM_UL,
	ELEM_LI,
	ELEM_STRONG,
	ELEM_TABLE,
	ELEM_CAPTION,
	ELEM_TR,
	ELEM_TD,
	ELEM_TH,
	ELEM_SELECT,
	ELEM_OPTION,
	ELEM_IMG,
	ELEM_I,
	ELEM_Q,
	ELEM_DL,
	ELEM_DT,
	ELEM_DD,
	ELEM_COL,
	ELEM_VAR,
	ELEM__MAX
};

enum	method {
	METHOD_POST,
	METHOD_GET
};

struct	kpair {
	char		*key; /* key name */
	char		*val; /*  key value */
	struct kpair	*next; /* next in map entry */
	union parsed {
		int64_t	 i; /* validated integer */
		const char *s; /* validated string */
		double	 d; /* validated decimal */
	} parsed;
};

enum	kfield {
	KFIELD_INT,
	KFIELD_STRING,
	KFIELD_DOUBLE,
	KFIELD_PASSWORD,
	KFIELD_SUBMIT,
	KFIELD_EMAIL,
	KFIELD__MAX
};

struct	kvalid {
	int		(*valid)(struct kpair *);
	const char	 *name;
	enum kfield	  field;
	const char	 *label;
	const char	 *def;
};

struct	session {
	time_t		 mtime; /* last modification time */
	int64_t		 cookie; /* unique session cookie */
	enum page	 page; /* current page */
	int64_t		 id; 
};

struct	req {
	enum method	 method;
	struct kpair	*cookies;
	size_t		 cookiesz;
	struct kpair	*cookiemap[KEY__MAX];
	struct kpair	*fields;
	struct kpair	*fieldmap[KEY__MAX];
	size_t		 fieldsz;
	enum mime	 mime;
	enum page	 page;
	char		*path;
	char		*pathstacked;
	enum elem	 elems[128];
	size_t		 elemsz;
};

__BEGIN_DECLS

void		 http_free(struct req *req);
void		 http_parse(struct req *req);

void		 attr(struct req *req, enum elem elem, ...);
void		 closure(struct req *req, size_t count);
void		 decl(void);
void		 elem(struct req *req, enum elem elem);
void		 input(struct req *req, enum key key);
void		 sym(enum entity entity);
void		 text(const char *cp);

int		 kvalid_double(struct kpair *);
int		 kvalid_email(struct kpair *);
int		 kvalid_int(struct kpair *);
int		 kvalid_pageid(struct kpair *);
int		 kvalid_udouble(struct kpair *);
int		 kvalid_uint(struct kpair *);

void		*xmalloc(size_t sz);
void		*xrealloc(void *p, size_t nm, size_t sz);
void		*xxrealloc(void *p, size_t sz);
char		*xstrdup(const char *cp);

extern const struct kvalid	 keys[KEY__MAX];
extern const char * const 	 pages[PAGE__MAX];
extern const char * const	 mimes[MIME__MAX];
extern const char * const	 mimetypes[MIME__MAX];
extern const char		*pname;

__END_DECLS

#endif /*!KCGI_H*/
