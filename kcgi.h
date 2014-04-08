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

/* Version of the header. */
#define	VERSION		"@VERSION@"

enum	khttp {
	KHTTP_200,
	KHTTP_303,
	KHTTP_403,
	KHTTP_404,
	KHTTP_409,
	KHTTP_415,
	KHTTP__MAX
};

enum	kentity {
	KENTITY_EACUTE,
	KENTITY_GT,
	KENTITY_LARR,
	KENTITY_LT,
	KENTITY_MDASH,
	KENTITY_NDASH,
	KENTITY_RARR,
	KENTITY__MAX
};

enum	kattr {
	KATTR_HTTP_EQUIV,
	KATTR_CONTENT,
	KATTR_REL,
	KATTR_HREF,
	KATTR_TYPE,
	KATTR_ACTION,
	KATTR_METHOD,
	KATTR_NAME,
	KATTR_VALUE,
	KATTR_ONCLICK,
	KATTR_ID,
	KATTR_FOR,
	KATTR_CLASS,
	KATTR_COLSPAN,
	KATTR_DISABLED,
	KATTR_SELECTED,
	KATTR_SRC,
	KATTR_CLEAR,
	KATTR_CHECKED,
	KATTR_STYLE,
	KATTR_TARGET,
	KATTR_STEP,
	KATTR_MIN,
	KATTR_MAX,
	KATTR_WIDTH,
	KATTR_SPAN,
	KATTR_ROWSPAN,
	KATTR__MAX
};

enum	kmime {
	KMIME_HTML,
	KMIME_CSV,
	KMIME_PNG,
	KMIME__MAX
};

enum	kelem {
	KELEM_HTML,
	KELEM_HEAD,
	KELEM_BODY,
	KELEM_TITLE,
	KELEM_META,
	KELEM_LINK,
	KELEM_FORM,
	KELEM_INPUT,
	KELEM_TEXTAREA,
	KELEM_P,
	KELEM_BLOCKQUOTE,
	KELEM_BR,
	KELEM_FIELDSET,
	KELEM_LEGEND,
	KELEM_LABEL,
	KELEM_A,
	KELEM_CODE,
	KELEM_DIV,
	KELEM_SPAN,
	KELEM_UL,
	KELEM_LI,
	KELEM_STRONG,
	KELEM_TABLE,
	KELEM_CAPTION,
	KELEM_TR,
	KELEM_TD,
	KELEM_TH,
	KELEM_SELECT,
	KELEM_OPTION,
	KELEM_IMG,
	KELEM_I,
	KELEM_Q,
	KELEM_DL,
	KELEM_DT,
	KELEM_DD,
	KELEM_COL,
	KELEM_VAR,
	KELEM__MAX
};

enum	kmethod {
	KMETHOD_POST,
	KMETHOD_GET
};

enum	kfield {
	KFIELD_INTEGER,
	KFIELD_STRING,
	KFIELD_DOUBLE,
	KFIELD_EMAIL,
	KFIELD__MAX
};

struct	kpair {
	char		*key; /* key name */
	char		*val; /*  key value */
	size_t		 valsz; /* length of "value" */
	struct kpair	*next; /* next in map entry */
	enum kfield	 field; /* if parsed, the parse type */
	union parsed {
		int64_t	 i; /* validated integer */
		const char *s; /* validated string */
		double	 d; /* validated decimal */
	} parsed;
};

struct	kvalid {
	int		(*valid)(struct kpair *);
	const char	 *name;
};

struct	kdata;

struct	kreq {
	enum kmethod		  method;
	struct kpair		 *cookies;
	size_t			  cookiesz;
	struct kpair		**cookiemap;
	struct kpair		 *fields;
	struct kpair		**fieldmap;
	size_t			  fieldsz;
	enum kmime		  mime;
	size_t			  page;
	char			 *path;
	struct kdata		 *kdata;
	const struct kvalid	 *keys;
	size_t			  keysz;
	const char *const	 *pages;
	size_t			  pagesz;
};

__BEGIN_DECLS

void		 khttp_free(struct kreq *req);
void		 khttp_parse(struct kreq *req, 
			const struct kvalid *keys, size_t keymax,
			const char *const *pages, size_t pagemax,
			size_t defpage);
void		 kattr(struct kreq *req, enum kelem elem, ...);
void		 kbody(struct kreq *req);
void		 kclosure(struct kreq *req, size_t count);
void		 kclosureto(struct kreq *req, size_t pos);
void		 kdecl(struct kreq *req);
void		 kelem(struct kreq *req, enum kelem elem);
size_t		 kelemsave(struct kreq *req);
void		 kentity(struct kreq *req, enum kentity entity);
void		 khead(struct kreq *req, 
			const char *key, const char *val);
#if 0
void		 input(struct kreq *req, enum key key);
#endif
void		 kncr(struct kreq *req, uint16_t ncr);
void		 ktext(struct kreq *req, const char *cp);

int		 kvalid_double(struct kpair *);
int		 kvalid_email(struct kpair *);
int		 kvalid_int(struct kpair *);
int		 kvalid_string(struct kpair *);
int		 kvalid_udouble(struct kpair *);
int		 kvalid_uint(struct kpair *);

void		*kcalloc(size_t nm, size_t sz);
void		*kmalloc(size_t sz);
void		*krealloc(void *p, size_t nm, size_t sz);
void		*kxrealloc(void *p, size_t sz);
char		*kstrdup(const char *cp);

extern const char *const	 kmimes[KMIME__MAX];
extern const char *const	 kmimetypes[KMIME__MAX];
extern const char *const	 khttps[KHTTP__MAX];
extern const char		*pname;

__END_DECLS

#endif /*!KCGI_H*/
