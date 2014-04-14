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
	KATTR_ACTION,
	KATTR_CHECKED,
	KATTR_CLASS,
	KATTR_CLEAR,
	KATTR_COLSPAN,
	KATTR_CONTENT,
	KATTR_DISABLED,
	KATTR_ENCTYPE,
	KATTR_FOR,
	KATTR_HREF,
	KATTR_HTTP_EQUIV,
	KATTR_ID,
	KATTR_MAX,
	KATTR_METHOD,
	KATTR_MIN,
	KATTR_NAME,
	KATTR_ONCLICK,
	KATTR_REL,
	KATTR_ROWSPAN,
	KATTR_SELECTED,
	KATTR_SPAN,
	KATTR_SRC,
	KATTR_STEP,
	KATTR_STYLE,
	KATTR_TARGET,
	KATTR_TYPE,
	KATTR_VALUE,
	KATTR_WIDTH,
	KATTR__MAX
};

enum	kmime {
	KMIME_HTML,
	KMIME_CSV,
	KMIME_PNG,
	KMIME__MAX
};

enum	kelem {
	KELEM_A,
	KELEM_BLOCKQUOTE,
	KELEM_BODY,
	KELEM_BR,
	KELEM_CAPTION,
	KELEM_CODE,
	KELEM_COL,
	KELEM_DD,
	KELEM_DIV,
	KELEM_DL,
	KELEM_DT,
	KELEM_FIELDSET,
	KELEM_FORM,
	KELEM_HEAD,
	KELEM_HTML,
	KELEM_I,
	KELEM_IMG,
	KELEM_INPUT,
	KELEM_LABEL,
	KELEM_LEGEND,
	KELEM_LI,
	KELEM_LINK,
	KELEM_META,
	KELEM_OPTION,
	KELEM_P,
	KELEM_Q,
	KELEM_SELECT,
	KELEM_SPAN,
	KELEM_STRONG,
	KELEM_TABLE,
	KELEM_TD,
	KELEM_TEXTAREA,
	KELEM_TH,
	KELEM_TITLE,
	KELEM_TR,
	KELEM_UL,
	KELEM_VAR,
	KELEM__MAX
};

enum	kmethod {
	KMETHOD_POST,
	KMETHOD_GET
};

enum	kfield {
	KFIELD_EMAIL,
	KFIELD_PASSWORD,
	KFIELD_TEXT,
	KFIELD_NUMBER,
	KFIELD_SUBMIT,
	KFIELD__MAX
};

enum	kpairtype {
	KPAIR_INTEGER,
	KPAIR_STRING,
	KPAIR_DOUBLE,
	KPAIR__MAX
};

struct	kpair {
	char		*key; /* key name */
	char		*val; /*  key value */
	size_t		 valsz; /* length of "value" */
	char		*file; /* if from a file, that filename */
	char		*ctype; /* content-type, if defined */
	struct kpair	*next; /* next in map entry */
	enum kpairtype	 type; /* if parsed, the parse type */
	union parsed {
		int64_t i; /* validated integer */
		const char *s; /* validated string */
		double d; /* validated decimal */
	} parsed;
};

struct	kvalid {
	int		(*valid)(struct kpair *);
	const char	 *name;
	enum kfield	  field;
};

enum	kauth {
	KAUTH_NONE = 0,
	KAUTH_BASIC,
	KAUTH_DIGEST,
	KAUTH_UNKNOWN
};

struct	kdata;

struct	kreq {
	enum kmethod		  method;
	enum kauth		  auth;
	struct kpair		 *cookies;
	size_t			  cookiesz;
	struct kpair		**cookiemap;
	struct kpair		**cookienmap;
	struct kpair		 *fields;
	struct kpair		**fieldmap;
	struct kpair		**fieldnmap;
	size_t			  fieldsz;
	enum kmime		  mime;
	size_t			  page;
	char			 *path;
	char			 *fullpath;
	struct kdata		 *kdata;
	const struct kvalid	 *keys;
	size_t			  keysz;
	const char *const	 *pages;
	size_t			  pagesz;
};

struct	ktemplate {
	const char *const	 *key;
	size_t		 	  keysz;
	void		 	 *arg;
	int		 	(*cb)(size_t key, void *arg);
};

__BEGIN_DECLS

void		 khttp_body(struct kreq *req);
void		 khttp_free(struct kreq *req);
void		 khttp_head(struct kreq *req, const char *key, 
			const char *fmt, ...)
			__attribute__((format(printf, 3, 4)));
void		 khttp_parse(struct kreq *req, 
			const struct kvalid *keys, size_t keymax,
			const char *const *pages, size_t pagemax,
			size_t defpage);
void		 khttp_putc(struct kreq *req, int c);
void		 khttp_puts(struct kreq *req, const char *cp);

void		 khtml_attr(struct kreq *req, enum kelem elem, ...);
void		 khtml_close(struct kreq *req, size_t count);
void		 khtml_closeto(struct kreq *req, size_t pos);
void		 khtml_decl(struct kreq *req);
void		 khtml_elem(struct kreq *req, enum kelem elem);
size_t		 khtml_elemat(struct kreq *req);
void		 khtml_entity(struct kreq *req, enum kentity entity);
void		 khtml_input(struct kreq *req, size_t key);
void		 khtml_ncr(struct kreq *req, uint16_t ncr);
void		 khtml_text(struct kreq *req, const char *cp);

int		 kvalid_double(struct kpair *);
int		 kvalid_email(struct kpair *);
int		 kvalid_int(struct kpair *);
int		 kvalid_string(struct kpair *);
int		 kvalid_udouble(struct kpair *);
int		 kvalid_uint(struct kpair *);

int		 khtml_template(struct kreq *req, 
			const struct ktemplate *t, 
			const char *fname);

void		*kcalloc(size_t nm, size_t sz);
void		*kmalloc(size_t sz);
void		*krealloc(void *p, size_t nm, size_t sz);
void		*kxrealloc(void *p, size_t sz);
char		*kstrdup(const char *cp);

extern const char *const	 kmimes[KMIME__MAX];
extern const char *const	 kmimetypes[KMIME__MAX];
extern const char *const	 khttps[KHTTP__MAX];
extern const char *const	 kfields[KFIELD__MAX];
extern const char		*pname;

__END_DECLS

#endif /*!KCGI_H*/
