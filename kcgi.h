/*	$Id$ */
/*
 * Copyright (c) 2012, 2014, 2015, 2016 Kristaps Dzonsons <kristaps@bsd.lv>
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

#if !defined(__BEGIN_DECLS)
#  ifdef __cplusplus
#  define       __BEGIN_DECLS           extern "C" {
#  else
#  define       __BEGIN_DECLS
#  endif
#endif
#if !defined(__END_DECLS)
#  ifdef __cplusplus
#  define       __END_DECLS             }
#  else
#  define       __END_DECLS
#  endif
#endif

/*
 * All of the public functions, variables, and structures in this header
 * file are described in kcgi(3).
 * If you don't see something in kcgi(3) and see it here instead, it
 * shouldn't be used.
 */

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
	KHTTP_207,
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
	KHTTP_424,
	KHTTP_428,
	KHTTP_429,
	KHTTP_431,
	KHTTP_500,
	KHTTP_501,
	KHTTP_502,
	KHTTP_503,
	KHTTP_504,
	KHTTP_505,
	KHTTP_507,
	KHTTP_511,
	KHTTP__MAX
};

enum	krequ {
	KREQU_ACCEPT,
	KREQU_ACCEPT_CHARSET,
	KREQU_ACCEPT_ENCODING,
	KREQU_ACCEPT_LANGUAGE,
	KREQU_AUTHORIZATION,
	KREQU_DEPTH,
	KREQU_FROM,
	KREQU_HOST,
	KREQU_IF,
	KREQU_IF_MODIFIED_SINCE,
	KREQU_IF_MATCH,
	KREQU_IF_NONE_MATCH,
	KREQU_IF_RANGE,
	KREQU_IF_UNMODIFIED_SINCE,
	KREQU_MAX_FORWARDS,
	KREQU_PROXY_AUTHORIZATION,
	KREQU_RANGE,
	KREQU_REFERER,
	KREQU_USER_AGENT,
	KREQU__MAX
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

enum	kattrx {
	KATTRX_STRING,
	KATTRX_INT,
	KATTRX_DOUBLE
};

enum	kmethod {
	KMETHOD_ACL,
	KMETHOD_CONNECT,
	KMETHOD_COPY,
	KMETHOD_DELETE,
	KMETHOD_GET,
	KMETHOD_HEAD,
	KMETHOD_LOCK,
	KMETHOD_MKCALENDAR,
	KMETHOD_MKCOL,
	KMETHOD_MOVE,
	KMETHOD_OPTIONS,
	KMETHOD_POST,
	KMETHOD_PROPFIND,
	KMETHOD_PROPPATCH,
	KMETHOD_PUT,
	KMETHOD_REPORT,
	KMETHOD_TRACE,
	KMETHOD_UNLOCK,
	KMETHOD__MAX
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

enum	kmime {
	KMIME_APP_JAVASCRIPT,
	KMIME_APP_JSON,
	KMIME_APP_OCTET_STREAM,
	KMIME_IMAGE_GIF,
	KMIME_IMAGE_JPEG,
	KMIME_IMAGE_PNG,
	KMIME_IMAGE_SVG_XML,
	KMIME_TEXT_CALENDAR,
	KMIME_TEXT_CSS,
	KMIME_TEXT_CSV,
	KMIME_TEXT_HTML,
	KMIME_TEXT_PLAIN,
	KMIME_TEXT_XML,
	KMIME__MAX
};

struct	kmimemap {
	const char	*name;
	size_t		 mime;
};

enum	kpairstate {
	KPAIR_UNCHECKED = 0,
	KPAIR_VALID,
	KPAIR_INVALID
};

#define KREQ_DEBUG_WRITE	  0x01
#define KREQ_DEBUG_READ_BODY	  0x02

struct	kpair {
	char		*key; /* key name */
	size_t		 keypos; /* bucket (if assigned) */
	char		*val; /*  key value */
	size_t		 valsz; /* length of "val" */
	char		*file; /* content filename (or NULL) */
	char		*ctype; /* content type (or NULL) */
	size_t		 ctypepos; /* content type index */
	char		*xcode; /* content xfer encoding (or NULL) */
	struct kpair	*next; /* next in map entry */
	enum kpairstate	 state; /* parse state */
	enum kpairtype	 type; /* if parsed, the parse type */
	union parsed {
		int64_t i; /* validated integer */
		const char *s; /* validated string */
		double d; /* validated decimal */
	} parsed;
};

struct	kreq; /* forward declaration */
struct	kfcgi;

struct	kvalid {
	int		(*valid)(struct kpair *kp);
	const char	 *name;
};

enum	kauth {
	KAUTH_NONE = 0,
	KAUTH_BASIC,
	KAUTH_DIGEST,
	KAUTH_UNKNOWN
};

enum	khttpalg {
	KHTTPALG_MD5 = 0,
	KHTTPALG_MD5_SESS,
	KHTTPALG__MAX
};

enum	khttpqop {
	KHTTPQOP_NONE = 0,
	KHTTPQOP_AUTH,
	KHTTPQOP_AUTH_INT,
	KHTTPQOP__MAX
};

struct	khttpdigest {
	enum khttpalg	 alg;
	enum khttpqop	 qop;
	char		*user;
	char		*uri;
	char		*realm;
	char		*nonce;
	char		*cnonce;
	char		*response;
	size_t		 count;
	char		*opaque;
};

struct	khttpbasic {
	char		*response;
};

struct	khttpauth {
	enum kauth	 type;
	int		 authorised;
	char		*digest;
	union {
		struct khttpdigest digest;
		struct khttpbasic basic;
	} d;
};

struct	kdata;

struct	khead {
	char		*key;
	char		*val;
};

struct	kreq {
	struct khead		 *reqmap[KREQU__MAX];
	struct khead		 *reqs;
	size_t		 	  reqsz;
	enum kmethod		  method;
	enum kauth		  auth;
	struct khttpauth	  rawauth;
	struct kpair		 *cookies;
	size_t			  cookiesz;
	struct kpair		**cookiemap;
	struct kpair		**cookienmap;
	struct kpair		 *fields;
	struct kpair		**fieldmap;
	struct kpair		**fieldnmap;
	size_t			  fieldsz;
	size_t			  mime;
	size_t			  page;
	enum kscheme		  scheme;
	char			 *path;
	char			 *suffix;
	char			 *fullpath;
	char			 *pagename;
	char			 *remote;
	char			 *host;
	uint16_t		  port;
	struct kdata		 *kdata;
	const struct kvalid	 *keys;
	size_t			  keysz;
	char			 *pname;
	void			 *arg; 
};

struct	kopts {
	ssize_t		  	  sndbufsz;
};

struct	ktemplate {
	const char *const	 *key;
	size_t		 	  keysz;
	void		 	 *arg;
	int		 	(*cb)(size_t key, void *arg);
};

enum	kcgi_err {
	KCGI_OK = 0,
	/* ENOMEM (fork, malloc, etc.). */
	KCGI_ENOMEM,
	/* FastCGI request to exit. */
	KCGI_HUP,
	/* ENFILE or EMFILE (fd ops). */
	KCGI_ENFILE,
	/* EAGAIN (fork). */
	KCGI_EAGAIN,
	/* Internal system error (malformed data). */
	KCGI_FORM,
	/* Opaque operating-system error. */
	KCGI_SYSTEM
};

__BEGIN_DECLS

typedef int	(*ktemplate_writef)(const char *, size_t, void *);

int		 khttp_body(struct kreq *);
int		 khttp_body_compress(struct kreq *, int);
void		 khttp_free(struct kreq *);
void		 khttp_child_free(struct kreq *);
void		 khttp_head(struct kreq *, const char *, 
			const char *, ...) 
			__attribute__((format(printf, 3, 4)));
enum kcgi_err	 khttp_parse(struct kreq *, 
			const struct kvalid *, size_t,
			const char *const *, size_t, size_t);
enum kcgi_err	 khttp_parsex(struct kreq *, const struct kmimemap *, 
			const char *const *, size_t, 
			const struct kvalid *, size_t,
			const char *const *, size_t,
			size_t, size_t, void *, void (*)(void *),
			unsigned int, const struct kopts *);
void		 khttp_putc(struct kreq *, int);
void		 khttp_puts(struct kreq *, const char *);
int		 khttp_template(struct kreq *, 
			const struct ktemplate *, const char *);
int		 khttp_template_buf(struct kreq *, 
			const struct ktemplate *, const char *, size_t);
int		 khttp_templatex(const struct ktemplate *, 
			const char *, ktemplate_writef, void *);
int		 khttp_templatex_buf(const struct ktemplate *, 
			const char *, size_t, ktemplate_writef, void *);
void		 khttp_write(struct kreq *, const char *, size_t);

int		 khttpdigest_validate(const struct kreq *, 
			const char *);
int		 khttpdigest_validatehash(const struct kreq *, 
			const char *);
int		 khttpbasic_validate(const struct kreq *, 
			const char *, const char *);

int		 kvalid_date(struct kpair *);
int		 kvalid_double(struct kpair *);
int		 kvalid_email(struct kpair *);
int		 kvalid_int(struct kpair *);
int		 kvalid_string(struct kpair *);
int		 kvalid_stringne(struct kpair *);
int		 kvalid_udouble(struct kpair *);
int		 kvalid_uint(struct kpair *);

enum kcgi_err	 khttp_fcgi_parse(struct kfcgi *, struct kreq *);
enum kcgi_err	 khttp_fcgi_init(struct kfcgi **, 
			const struct kvalid *, size_t,
			const char *const *, size_t, size_t);
enum kcgi_err	 khttp_fcgi_initx(struct kfcgi **, 
			const char *const *, size_t,
			const struct kvalid *, size_t, 
			const struct kmimemap *, size_t,
			const char *const *, size_t, size_t,
			void *, void (*)(void *), unsigned int,
			const struct kopts *);
enum kcgi_err	 khttp_fcgi_free(struct kfcgi *);
void		 khttp_fcgi_child_free(struct kfcgi *);
int		 khttp_fcgi_test(void);

#define		KUTIL_EPOCH2TM(_tt, _tm) \
		kutil_epoch2tmvals((_tt), \
			&(_tm)->tt_sec, \
			&(_tm)->tt_min, \
			&(_tm)->tt_hour, \
			&(_tm)->tt_mday, \
			&(_tm)->tt_mon, \
			&(_tm)->tt_year, \
			&(_tm)->tt_wday, \
			&(_tm)->tt_yday);
void		 kutil_epoch2tmvals(int64_t, int *, int *, int *, 
			int *, int *, int *, int *, int *);
char		*kutil_epoch2str(int64_t, char *, size_t);
int64_t	 	 kutil_date2epoch(int64_t, int64_t, int64_t);
int64_t	 	 kutil_datetime2epoch(int64_t, int64_t, int64_t,
			int64_t, int64_t, int64_t);

char		*kutil_urlabs(enum kscheme, const char *, 
			uint16_t, const char *);
char		*kutil_urlpart(struct kreq *, const char *,
			const char *, const char *, ...);
char		*kutil_urlpartx(struct kreq *, const char *,
			const char *, const char *, ...);
char		*kutil_urlencode(const char *);
void		 kutil_invalidate(struct kreq *, struct kpair *);

int		 kutil_openlog(const char *);
void		 kutil_vinfo(const struct kreq *, 
			const char *, const char *, va_list);
void		 kutil_vlog(const struct kreq *, const char *,
			const char *, const char *, va_list);
void		 kutil_vlogx(const struct kreq *, const char *,
			const char *, const char *, va_list);
void		 kutil_vwarn(const struct kreq *, 
			const char *, const char *, va_list);
void		 kutil_vwarnx(const struct kreq *, 
			const char *, const char *, va_list);
void		 kutil_log(const struct kreq *, const char *,
			const char *, const char *, ...)
			__attribute__((format(printf, 4, 5)));
void		 kutil_logx(const struct kreq *, const char *,
			const char *, const char *, ...)
			__attribute__((format(printf, 4, 5)));
void		 kutil_info(const struct kreq *, 
			const char *, const char *, ...)
			__attribute__((format(printf, 3, 4)));
void		 kutil_warn(const struct kreq *, 
			const char *, const char *, ...)
			__attribute__((format(printf, 3, 4)));
void		 kutil_warnx(const struct kreq *, 
			const char *, const char *, ...)
			__attribute__((format(printf, 3, 4)));

int		 kasprintf(char **, const char *, ...)
			__attribute__((format(printf, 2, 3)));
void		*kcalloc(size_t, size_t);
void		*kmalloc(size_t);
void		*krealloc(void *, size_t);
void		*kreallocarray(void *, size_t, size_t);
char		*kstrdup(const char *);

extern const char *const	 kmimetypes[KMIME__MAX];
extern const char *const	 khttps[KHTTP__MAX];
extern const char *const	 kschemes[KSCHEME__MAX];
extern const char *const	 kresps[KRESP__MAX];
extern const char *const	 kmethods[KMETHOD__MAX];
extern const struct kmimemap	 ksuffixmap[];
extern const char *const	 ksuffixes[KMIME__MAX];

__END_DECLS

#endif /*!KCGI_H*/
