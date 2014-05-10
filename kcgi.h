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

enum	kresp {
	KRESP_ACCESS_CONTROL_ALLOW_ORIGIN,
	KRESP_ACCEPT_RANGES,
	KRESP_AGE,
	KRESP_ALLOW,
	KRESP_CACHE_CONTROL,
	KRESP_CONNECTION,
	KRESP_CONTENT_ENCODING,
	KRESP_CONTENT_LANGUAGE,
	KRESP_CONTENT_LENGTH,
	KRESP_CONTENT_LOCATION,
	KRESP_CONTENT_MD5,
	KRESP_CONTENT_DISPOSITION,
	KRESP_CONTENT_RANGE,
	KRESP_CONTENT_TYPE,
	KRESP_DATE,
	KRESP_ETAG,
	KRESP_EXPIRES,
	KRESP_LAST_MODIFIED,
	KRESP_LINK,
	KRESP_LOCATION,
	KRESP_P3P,
	KRESP_PRAGMA,
	KRESP_PROXY_AUTHENTICATE,
	KRESP_REFRESH,
	KRESP_RETRY_AFTER,
	KRESP_SERVER,
	KRESP_SET_COOKIE,
	KRESP_STATUS,
	KRESP_STRICT_TRANSPORT_SECURITY,
	KRESP_TRAILER,
	KRESP_TRANSFER_ENCODING,
	KRESP_UPGRADE,
	KRESP_VARY,
	KRESP_VIA,
	KRESP_WARNING,
	KRESP_WWW_AUTHENTICATE,
	KRESP_X_FRAME_OPTIONS,
	KRESP__MAX
};

enum	kentity {
	KENTITY_AElig,
	KENTITY_Aacute,
	KENTITY_Acirc,
	KENTITY_Agrave,
	KENTITY_Aring,
	KENTITY_Atilde,
	KENTITY_Auml,
	KENTITY_Ccedil,
	KENTITY_Dagger,
	KENTITY_ETH,
	KENTITY_Eacute,
	KENTITY_Ecirc,
	KENTITY_Egrave,
	KENTITY_Euml,
	KENTITY_Iacute,
	KENTITY_Icirc,
	KENTITY_Igrave,
	KENTITY_Iuml,
	KENTITY_Ntilde,
	KENTITY_OElig,
	KENTITY_Oacute,
	KENTITY_Ocirc,
	KENTITY_Ograve,
	KENTITY_Oslash,
	KENTITY_Otilde,
	KENTITY_Ouml,
	KENTITY_Scaron,
	KENTITY_THORN,
	KENTITY_Uacute,
	KENTITY_Ucirc,
	KENTITY_Ugrave,
	KENTITY_Uuml,
	KENTITY_Yacute,
	KENTITY_Yuml,
	KENTITY_aacute,
	KENTITY_acirc,
	KENTITY_acute,
	KENTITY_aelig,
	KENTITY_agrave,
	KENTITY_amp,
	KENTITY_apos,
	KENTITY_aring,
	KENTITY_atilde,
	KENTITY_auml,
	KENTITY_bdquo,
	KENTITY_brvbar,
	KENTITY_ccedil,
	KENTITY_cedil,
	KENTITY_cent,
	KENTITY_circ,
	KENTITY_copy,
	KENTITY_curren,
	KENTITY_dagger,
	KENTITY_deg,
	KENTITY_divide,
	KENTITY_eacute,
	KENTITY_ecirc,
	KENTITY_egrave,
	KENTITY_emsp,
	KENTITY_ensp,
	KENTITY_eth,
	KENTITY_euml,
	KENTITY_euro,
	KENTITY_frac12,
	KENTITY_frac14,
	KENTITY_frac34,
	KENTITY_gt,
	KENTITY_hellip,
	KENTITY_iacute,
	KENTITY_icirc,
	KENTITY_iexcl,
	KENTITY_igrave,
	KENTITY_iquest,
	KENTITY_iuml,
	KENTITY_laquo,
	KENTITY_ldquo,
	KENTITY_lrm,
	KENTITY_lsaquo,
	KENTITY_lsquo,
	KENTITY_lt,
	KENTITY_macr,
	KENTITY_mdash,
	KENTITY_micro,
	KENTITY_middot,
	KENTITY_nbsp,
	KENTITY_ndash,
	KENTITY_not,
	KENTITY_ntilde,
	KENTITY_oacute,
	KENTITY_ocirc,
	KENTITY_oelig,
	KENTITY_ograve,
	KENTITY_ordf,
	KENTITY_ordm,
	KENTITY_oslash,
	KENTITY_otilde,
	KENTITY_ouml,
	KENTITY_para,
	KENTITY_permil,
	KENTITY_plusmn,
	KENTITY_pound,
	KENTITY_quot,
	KENTITY_raquo,
	KENTITY_rdquo,
	KENTITY_reg,
	KENTITY_rlm,
	KENTITY_rsaquo,
	KENTITY_rsquo,
	KENTITY_sbquo,
	KENTITY_scaron,
	KENTITY_sect,
	KENTITY_shy,
	KENTITY_sup1,
	KENTITY_sup2,
	KENTITY_sup3,
	KENTITY_szlig,
	KENTITY_thinsp,
	KENTITY_thorn,
	KENTITY_tilde,
	KENTITY_times,
	KENTITY_trade,
	KENTITY_uacute,
	KENTITY_ucirc,
	KENTITY_ugrave,
	KENTITY_uml,
	KENTITY_uuml,
	KENTITY_yacute,
	KENTITY_yen,
	KENTITY_yuml,
	KENTITY_zwj,
	KENTITY_zwnj,
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

enum	kscheme {
	KSCHEME_AAA,
	KSCHEME_AAAS,
	KSCHEME_ABOUT,
	KSCHEME_ACAP,
	KSCHEME_ACCT,
	KSCHEME_CAP,
	KSCHEME_CID,
	KSCHEME_COAP,
	KSCHEME_COAPS,
	KSCHEME_CRID,
	KSCHEME_DATA,
	KSCHEME_DAV,
	KSCHEME_DICT,
	KSCHEME_DNS,
	KSCHEME_FILE,
	KSCHEME_FTP,
	KSCHEME_GEO,
	KSCHEME_GO,
	KSCHEME_GOPHER,
	KSCHEME_H323,
	KSCHEME_HTTP,
	KSCHEME_HTTPS,
	KSCHEME_IAX,
	KSCHEME_ICAP,
	KSCHEME_IM,
	KSCHEME_IMAP,
	KSCHEME_INFO,
	KSCHEME_IPP,
	KSCHEME_IRIS,
	KSCHEME_IRIS_BEEP,
	KSCHEME_IRIS_XPC,
	KSCHEME_IRIS_XPCS,
	KSCHEME_IRIS_LWZ,
	KSCHEME_JABBER,
	KSCHEME_LDAP,
	KSCHEME_MAILTO,
	KSCHEME_MID,
	KSCHEME_MSRP,
	KSCHEME_MSRPS,
	KSCHEME_MTQP,
	KSCHEME_MUPDATE,
	KSCHEME_NEWS,
	KSCHEME_NFS,
	KSCHEME_NI,
	KSCHEME_NIH,
	KSCHEME_NNTP,
	KSCHEME_OPAQUELOCKTOKEN,
	KSCHEME_POP,
	KSCHEME_PRES,
	KSCHEME_RELOAD,
	KSCHEME_RTSP,
	KSCHEME_RTSPS,
	KSCHEME_RTSPU,
	KSCHEME_SERVICE,
	KSCHEME_SESSION,
	KSCHEME_SHTTP,
	KSCHEME_SIEVE,
	KSCHEME_SIP,
	KSCHEME_SIPS,
	KSCHEME_SMS,
	KSCHEME_SNMP,
	KSCHEME_SOAP_BEEP,
	KSCHEME_SOAP_BEEPS,
	KSCHEME_STUN,
	KSCHEME_STUNS,
	KSCHEME_TAG,
	KSCHEME_TEL,
	KSCHEME_TELNET,
	KSCHEME_TFTP,
	KSCHEME_THISMESSAGE,
	KSCHEME_TN3270,
	KSCHEME_TIP,
	KSCHEME_TURN,
	KSCHEME_TURNS,
	KSCHEME_TV,
	KSCHEME_URN,
	KSCHEME_VEMMI,
	KSCHEME_WS,
	KSCHEME_WSS,
	KSCHEME_XCON,
	KSCHEME_XCON_USERID,
	KSCHEME_XMLRPC_BEEP,
	KSCHEME_XMLRPC_BEEPS,
	KSCHEME_XMPP,
	KSCHEME_Z39_50R,
	KSCHEME_Z39_50S,
	KSCHEME__MAX
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

struct	kreq; /* forward declaration */

struct	kvalid {
	int		(*valid)(struct kreq *r, struct kpair *kp);
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
	char			 *host;
	uint16_t		  port;
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
int		 khttp_parse(struct kreq *req, 
			const struct kvalid *keys, size_t keymax,
			const char *const *pages, size_t pagemax,
			size_t defpage, void *arg);
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
int		 khtml_template(struct kreq *req, 
			const struct ktemplate *t, 
			const char *fname);
void		 khtml_text(struct kreq *req, const char *cp);

int		 kvalid_double(struct kreq *, struct kpair *);
int		 kvalid_email(struct kreq *, struct kpair *);
int		 kvalid_int(struct kreq *, struct kpair *);
int		 kvalid_string(struct kreq *, struct kpair *);
int		 kvalid_udouble(struct kreq *, struct kpair *);
int		 kvalid_uint(struct kreq *, struct kpair *);

char		*kutil_urlabs(enum kscheme scheme, const char *host, 
			uint16_t port, const char *path);
char		*kutil_urlpart(struct kreq *req, 
			enum kmime mime, size_t page, ...);
char		*kutil_urlencode(const char *cp);
void		 kutil_invalidate(struct kreq *req,
			struct kpair *pair);

void		*kasprintf(const char *fmt, ...);
void		*kcalloc(size_t nm, size_t sz);
void		*kmalloc(size_t sz);
void		*krealloc(void *p, size_t sz);
void		*kreallocarray(void *p, size_t nm, size_t sz);
char		*kstrdup(const char *cp);

extern const char *const	 kmimetypes[KMIME__MAX];
extern const char *const	 khttps[KHTTP__MAX];
extern const char *const	 ksuffixes[KMIME__MAX];
extern const char *const	 kresps[KRESP__MAX];
extern const char		*pname;

__END_DECLS

#endif /*!KCGI_H*/
