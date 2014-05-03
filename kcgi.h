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
	KHTTP_100,
	KHTTP_101,
	KHTTP_103,
	KHTTP_200,
	KHTTP_201,
	KHTTP_202,
	KHTTP_203,
	KHTTP_204,
	KHTTP_205,
	KHTTP_206,
	KHTTP_300,
	KHTTP_301,
	KHTTP_302,
	KHTTP_303,
	KHTTP_304,
	KHTTP_306,
	KHTTP_307,
	KHTTP_308,
	KHTTP_400,
	KHTTP_401,
	KHTTP_402,
	KHTTP_403,
	KHTTP_404,
	KHTTP_405,
	KHTTP_406,
	KHTTP_407,
	KHTTP_408,
	KHTTP_409,
	KHTTP_410,
	KHTTP_411,
	KHTTP_412,
	KHTTP_413,
	KHTTP_414,
	KHTTP_415,
	KHTTP_416,
	KHTTP_417,
	KHTTP_500,
	KHTTP_501,
	KHTTP_502,
	KHTTP_503,
	KHTTP_504,
	KHTTP_505,
	KHTTP_511,
	KHTTP__MAX
};

enum	kentity {
	KENTITY_AMP,
	KENTITY_EACUTE,
	KENTITY_GT,
	KENTITY_LARR,
	KENTITY_LT,
	KENTITY_MDASH,
	KENTITY_NDASH,
	KENTITY_RARR,
	KENTITY__MAX
};

enum	kattrx {
	KATTRX_STRING,
	KATTRX_INT,
	KATTRX_DOUBLE
};

enum	kattr {
	KATTR_ACCEPT_CHARSET,
	KATTR_ACCESSKEY,
	KATTR_ACTION,
	KATTR_ALT,
	KATTR_ASYNC,
	KATTR_AUTOCOMPLETE,
	KATTR_AUTOFOCUS,
	KATTR_AUTOPLAY,
	KATTR_BORDER,
	KATTR_CHALLENGE,
	KATTR_CHARSET,
	KATTR_CHECKED,
	KATTR_CITE,
	KATTR_CLASS,
	KATTR_COLS,
	KATTR_COLSPAN,
	KATTR_CONTENT,
	KATTR_CONTENTEDITABLE,
	KATTR_CONTEXTMENU,
	KATTR_CONTROLS,
	KATTR_COORDS,
	KATTR_DATETIME,
	KATTR_DEFAULT,
	KATTR_DEFER,
	KATTR_DIR,
	KATTR_DIRNAME,
	KATTR_DISABLED,
	KATTR_DRAGGABLE,
	KATTR_DROPZONE,
	KATTR_ENCTYPE,
	KATTR_FOR,
	KATTR_FORM,
	KATTR_FORMACTION,
	KATTR_FORMENCTYPE,
	KATTR_FORMMETHOD,
	KATTR_FORMNOVALIDATE,
	KATTR_FORMTARGET,
	KATTR_HEADER,
	KATTR_HEIGHT,
	KATTR_HIDDEN,
	KATTR_HIGH,
	KATTR_HREF,
	KATTR_HREFLANG,
	KATTR_HTTP_EQUIV,
	KATTR_ICON,
	KATTR_ID,
	KATTR_ISMAP,
	KATTR_KEYTYPE,
	KATTR_KIND,
	KATTR_LABEL,
	KATTR_LANG,
	KATTR_LANGUAGE,
	KATTR_LIST,
	KATTR_LOOP,
	KATTR_LOW,
	KATTR_MANIFEST,
	KATTR_MAX,
	KATTR_MAXLENGTH,
	KATTR_MEDIA,
	KATTR_MEDIAGROUP,
	KATTR_METHOD,
	KATTR_MIN,
	KATTR_MULTIPLE,
	KATTR_MUTED,
	KATTR_NAME,
	KATTR_NOVALIDATE,
	KATTR_ONABORT,
	KATTR_ONAFTERPRINT,
	KATTR_ONBEFOREPRINT,
	KATTR_ONBEFOREUNLOAD,
	KATTR_ONBLUR,
	KATTR_ONCANPLAY,
	KATTR_ONCANPLAYTHROUGH,
	KATTR_ONCHANGE,
	KATTR_ONCLICK,
	KATTR_ONCONTEXTMENU,
	KATTR_ONDBLCLICK,
	KATTR_ONDRAG,
	KATTR_ONDRAGEND,
	KATTR_ONDRAGENTER,
	KATTR_ONDRAGLEAVE,
	KATTR_ONDRAGOVER,
	KATTR_ONDRAGSTART,
	KATTR_ONDROP,
	KATTR_ONDURATIONCHANGE,
	KATTR_ONEMPTIED,
	KATTR_ONENDED,
	KATTR_ONERROR,
	KATTR_ONFOCUS,
	KATTR_ONHASHCHANGE,
	KATTR_ONINPUT,
	KATTR_ONINVALID,
	KATTR_ONKEYDOWN,
	KATTR_ONKEYPRESS,
	KATTR_ONKEYUP,
	KATTR_ONLOAD,
	KATTR_ONLOADEDDATA,
	KATTR_ONLOADEDMETADATA,
	KATTR_ONLOADSTART,
	KATTR_ONMESSAGE,
	KATTR_ONMOUSEDOWN,
	KATTR_ONMOUSEMOVE,
	KATTR_ONMOUSEOUT,
	KATTR_ONMOUSEOVER,
	KATTR_ONMOUSEUP,
	KATTR_ONMOUSEWHEEL,
	KATTR_ONOFFLINE,
	KATTR_ONONLINE,
	KATTR_ONPAGEHIDE,
	KATTR_ONPAGESHOW,
	KATTR_ONPAUSE,
	KATTR_ONPLAY,
	KATTR_ONPLAYING,
	KATTR_ONPOPSTATE,
	KATTR_ONPROGRESS,
	KATTR_ONRATECHANGE,
	KATTR_ONREADYSTATECHANGE,
	KATTR_ONRESET,
	KATTR_ONRESIZE,
	KATTR_ONSCROLL,
	KATTR_ONSEEKED,
	KATTR_ONSEEKING,
	KATTR_ONSELECT,
	KATTR_ONSHOW,
	KATTR_ONSTALLED,
	KATTR_ONSTORAGE,
	KATTR_ONSUBMIT,
	KATTR_ONSUSPEND,
	KATTR_ONTIMEUPDATE,
	KATTR_ONUNLOAD,
	KATTR_ONVOLUMECHANGE,
	KATTR_ONWAITING,
	KATTR_OPEN,
	KATTR_OPTIMUM,
	KATTR_PATTERN,
	KATTR_PLACEHOLDER,
	KATTR_POSTER,
	KATTR_PRELOAD,
	KATTR_RADIOGROUP,
	KATTR_READONLY,
	KATTR_REL,
	KATTR_REQUIRED,
	KATTR_REVERSED,
	KATTR_ROWS,
	KATTR_ROWSPAN,
	KATTR_SANDBOX,
	KATTR_SCOPE,
	KATTR_SEAMLESS,
	KATTR_SELECTED,
	KATTR_SHAPE,
	KATTR_SIZE,
	KATTR_SIZES,
	KATTR_SPAN,
	KATTR_SPELLCHECK,
	KATTR_SRC,
	KATTR_SRCDOC,
	KATTR_SRCLANG,
	KATTR_START,
	KATTR_STEP,
	KATTR_STYLE,
	KATTR_TABINDEX,
	KATTR_TARGET,
	KATTR_TITLE,
	KATTR_TRANSLATE,
	KATTR_TYPE,
	KATTR_USEMAP,
	KATTR_VALUE,
	KATTR_WIDTH,
	KATTR_WRAP,
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
	KELEM_ABBR,
	KELEM_ADDRESS,
	KELEM_AREA,
	KELEM_ARTICLE,
	KELEM_ASIDE,
	KELEM_AUDIO,
	KELEM_B,
	KELEM_BASE,
	KELEM_BDI,
	KELEM_BDO,
	KELEM_BLOCKQUOTE,
	KELEM_BODY,
	KELEM_BR,
	KELEM_BUTTON,
	KELEM_CANVAS,
	KELEM_CAPTION,
	KELEM_CITE,
	KELEM_CODE,
	KELEM_COL,
	KELEM_COLGROUP,
	KELEM_DATALIST,
	KELEM_DD,
	KELEM_DEL,
	KELEM_DETAILS,
	KELEM_DFN,
	KELEM_DIV,
	KELEM_DL,
	KELEM_DOCTYPE,
	KELEM_DT,
	KELEM_EM,
	KELEM_EMBED,
	KELEM_FIELDSET,
	KELEM_FIGCAPTION,
	KELEM_FIGURE,
	KELEM_FOOTER,
	KELEM_FORM,
	KELEM_H1,
	KELEM_H2,
	KELEM_H3,
	KELEM_H4,
	KELEM_H5,
	KELEM_H6,
	KELEM_HEAD,
	KELEM_HEADER,
	KELEM_HGROUP,
	KELEM_HR,
	KELEM_HTML,
	KELEM_I,
	KELEM_IFRAME,
	KELEM_IMG,
	KELEM_INPUT,
	KELEM_INS,
	KELEM_KBD,
	KELEM_KEYGEN,
	KELEM_LABEL,
	KELEM_LEGEND,
	KELEM_LI,
	KELEM_LINK,
	KELEM_MAP,
	KELEM_MARK,
	KELEM_MENU,
	KELEM_META,
	KELEM_METER,
	KELEM_NAV,
	KELEM_NOSCRIPT,
	KELEM_OBJECT,
	KELEM_OL,
	KELEM_OPTGROUP,
	KELEM_OPTION,
	KELEM_OUTPUT,
	KELEM_P,
	KELEM_PARAM,
	KELEM_PRE,
	KELEM_PROGRESS,
	KELEM_Q,
	KELEM_RP,
	KELEM_RT,
	KELEM_RUBY,
	KELEM_S,
	KELEM_SAMP,
	KELEM_SCRIPT,
	KELEM_SECTION,
	KELEM_SELECT,
	KELEM_SMALL,
	KELEM_SOURCE,
	KELEM_SPAN,
	KELEM_STRONG,
	KELEM_STYLE,
	KELEM_SUB,
	KELEM_SUMMARY,
	KELEM_SUP,
	KELEM_TABLE,
	KELEM_TBODY,
	KELEM_TD,
	KELEM_TEXTAREA,
	KELEM_TFOOT,
	KELEM_TH,
	KELEM_THEAD,
	KELEM_TIME,
	KELEM_TITLE,
	KELEM_TR,
	KELEM_TRACK,
	KELEM_U,
	KELEM_UL,
	KELEM_VAR,
	KELEM_VIDEO,
	KELEM_WBR,
	KELEM__MAX
};

enum	kmethod {
	KMETHOD_POST,
	KMETHOD_GET
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
	size_t		 valsz; /* length of "val" */
	char		*file; /* content filename (or NULL) */
	char		*ctype; /* content type (or NULL) */
	char		*xcode; /* content xfer encoding (or NULL) */
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
	char			 *suffix;
	char			 *fullpath;
	char			 *remote;
	struct kdata		 *kdata;
	const struct kvalid	 *keys;
	size_t			  keysz;
	const char *const	 *pages;
	size_t			  pagesz;
	void			 *arg; 
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
void		 khtml_attrx(struct kreq *req, enum kelem elem, ...);
void		 khtml_close(struct kreq *req, size_t count);
void		 khtml_closeto(struct kreq *req, size_t pos);
void		 khtml_double(struct kreq *req, double val);
void		 khtml_elem(struct kreq *req, enum kelem elem);
size_t		 khtml_elemat(struct kreq *req);
void		 khtml_entity(struct kreq *req, enum kentity entity);
void		 khtml_int(struct kreq *req, int64_t val);
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

char		*kutil_urlpart(struct kreq *req, 
			enum kmime mime, size_t page, ...);
char		*kutil_urlencode(const char *cp);
void		 kutil_invalidate(struct kreq *req,
			struct kpair *pair);

void		*kasprintf(const char *fmt, ...);
void		*kcalloc(size_t nm, size_t sz);
void		*kmalloc(size_t sz);
void		*krealloc(void *p, size_t nm, size_t sz);
void		*kxrealloc(void *p, size_t sz);
char		*kstrdup(const char *cp);

extern const char *const	 kmimetypes[KMIME__MAX];
extern const char *const	 khttps[KHTTP__MAX];
extern const char *const	 ksuffixes[KMIME__MAX];
extern const char		*pname;

__END_DECLS

#endif /*!KCGI_H*/
