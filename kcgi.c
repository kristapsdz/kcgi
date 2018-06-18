/*	$Id$ */
/*
 * Copyright (c) 2012, 2014--2017 Kristaps Dzonsons <kristaps@bsd.lv>
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
#include "config.h"

#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/socket.h>

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <math.h> /* HUGE_VAL */
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "kcgi.h"
#include "extern.h"

/*
 * Maximum size of printing a signed 64-bit integer.
 */
#define	INT_MAXSZ	 22

const char *const kschemes[KSCHEME__MAX] = {
	"aaa", /* KSCHEME_AAA */
	"aaas", /* KSCHEME_AAAS */
	"about", /* KSCHEME_ABOUT */
	"acap", /* KSCHEME_ACAP */
	"acct", /* KSCHEME_ACCT */
	"cap", /* KSCHEME_CAP */
	"cid", /* KSCHEME_CID */
	"coap", /* KSCHEME_COAP */
	"coaps", /* KSCHEME_COAPS */
	"crid", /* KSCHEME_CRID */
	"data", /* KSCHEME_DATA */
	"dav", /* KSCHEME_DAV */
	"dict", /* KSCHEME_DICT */
	"dns", /* KSCHEME_DNS */
	"file", /* KSCHEME_FILE */
	"ftp", /* KSCHEME_FTP */
	"geo", /* KSCHEME_GEO */
	"go", /* KSCHEME_GO */
	"gopher", /* KSCHEME_GOPHER */
	"h323", /* KSCHEME_H323 */
	"http", /* KSCHEME_HTTP */
	"https", /* KSCHEME_HTTPS */
	"iax", /* KSCHEME_IAX */
	"icap", /* KSCHEME_ICAP */
	"im", /* KSCHEME_IM */
	"imap", /* KSCHEME_IMAP */
	"info", /* KSCHEME_INFO */
	"ipp", /* KSCHEME_IPP */
	"iris", /* KSCHEME_IRIS */
	"iris.beep", /* KSCHEME_IRIS_BEEP */
	"iris.xpc", /* KSCHEME_IRIS_XPC */
	"iris.xpcs", /* KSCHEME_IRIS_XPCS */
	"iris.lwz", /* KSCHEME_IRIS_LWZ */
	"jabber", /* KSCHEME_JABBER */
	"ldap", /* KSCHEME_LDAP */
	"mailto", /* KSCHEME_MAILTO */
	"mid", /* KSCHEME_MID */
	"msrp", /* KSCHEME_MSRP */
	"msrps", /* KSCHEME_MSRPS */
	"mtqp", /* KSCHEME_MTQP */
	"mupdate", /* KSCHEME_MUPDATE */
	"news", /* KSCHEME_NEWS */
	"nfs", /* KSCHEME_NFS */
	"ni", /* KSCHEME_NI */
	"nih", /* KSCHEME_NIH */
	"nntp", /* KSCHEME_NNTP */
	"opaquelocktoken", /* KSCHEME_OPAQUELOCKTOKEN */
	"pop", /* KSCHEME_POP */
	"pres", /* KSCHEME_PRES */
	"reload", /* KSCHEME_RELOAD */
	"rtsp", /* KSCHEME_RTSP */
	"rtsps", /* KSCHEME_RTSPS */
	"rtspu", /* KSCHEME_RTSPU */
	"service", /* KSCHEME_SERVICE */
	"session", /* KSCHEME_SESSION */
	"shttp", /* KSCHEME_SHTTP */
	"sieve", /* KSCHEME_SIEVE */
	"sip", /* KSCHEME_SIP */
	"sips", /* KSCHEME_SIPS */
	"sms", /* KSCHEME_SMS */
	"snmp", /* KSCHEME_SNMP */
	"soap.beep", /* KSCHEME_SOAP_BEEP */
	"soap.beeps", /* KSCHEME_SOAP_BEEPS */
	"stun", /* KSCHEME_STUN */
	"stuns", /* KSCHEME_STUNS */
	"tag", /* KSCHEME_TAG */
	"tel", /* KSCHEME_TEL */
	"telnet", /* KSCHEME_TELNET */
	"tftp", /* KSCHEME_TFTP */
	"thismessage", /* KSCHEME_THISMESSAGE */
	"tn3270", /* KSCHEME_TN3270 */
	"tip", /* KSCHEME_TIP */
	"turn", /* KSCHEME_TURN */
	"turns", /* KSCHEME_TURNS */
	"tv", /* KSCHEME_TV */
	"urn", /* KSCHEME_URN */
	"vemmi", /* KSCHEME_VEMMI */
	"ws", /* KSCHEME_WS */
	"wss", /* KSCHEME_WSS */
	"xcon", /* KSCHEME_XCON */
	"xcon-userid", /* KSCHEME_XCON_USERID */
	"xmlrpc.beep", /* KSCHEME_XMLRPC_BEEP */
	"xmlrpc.beeps", /* KSCHEME_XMLRPC_BEEPS */
	"xmpp", /* KSCHEME_XMPP */
	"z39.50r", /* KSCHEME_Z39_50R */
	"z39.50s", /* KSCHEME_Z39_50S */
};

const char *const kresps[KRESP__MAX] = {
	"Access-Control-Allow-Origin", /* KRESP_ACCESS_CONTROL_ALLOW_ORIGIN */
	"Accept-Ranges", /* KRESP_ACCEPT_RANGES */
	"Age", /* KRESP_AGE */
	"Allow", /* KRESP_ALLOW */
	"Cache-Control", /* KRESP_CACHE_CONTROL */
	"Connection", /* KRESP_CONNECTION */
	"Content-Encoding", /* KRESP_CONTENT_ENCODING */
	"Content-Language", /* KRESP_CONTENT_LANGUAGE */
	"Content-Length", /* KRESP_CONTENT_LENGTH */
	"Content-Location", /* KRESP_CONTENT_LOCATION */
	"Content-MD5", /* KRESP_CONTENT_MD5 */
	"Content-Disposition", /* KRESP_CONTENT_DISPOSITION */
	"Content-Range", /* KRESP_CONTENT_RANGE */
	"Content-Type", /* KRESP_CONTENT_TYPE */
	"Date", /* KRESP_DATE */
	"ETag", /* KRESP_ETAG */
	"Expires", /* KRESP_EXPIRES */
	"Last-Modified", /* KRESP_LAST_MODIFIED */
	"Link", /* KRESP_LINK */
	"Location", /* KRESP_LOCATION */
	"P3P", /* KRESP_P3P */
	"Pragma", /* KRESP_PRAGMA */
	"Proxy-Authenticate", /* KRESP_PROXY_AUTHENTICATE */
	"Refresh", /* KRESP_REFRESH */
	"Retry-After", /* KRESP_RETRY_AFTER */
	"Server", /* KRESP_SERVER */
	"Set-Cookie", /* KRESP_SET_COOKIE */
	"Status", /* KRESP_STATUS */
	"Strict-Transport-Security", /* KRESP_STRICT_TRANSPORT_SECURITY */
	"Trailer", /* KRESP_TRAILER */
	"Transfer-Encoding", /* KRESP_TRANSFER_ENCODING */
	"Upgrade", /* KRESP_UPGRADE */
	"Vary", /* KRESP_VARY */
	"Via", /* KRESP_VIA */
	"Warning", /* KRESP_WARNING */
	"WWW-Authenticate", /* KRESP_WWW_AUTHENTICATE */
	"X-Frame-Options", /* KRESP_X_FRAME_OPTIONS */
};

const char *const kmimetypes[KMIME__MAX] = {
	"application/x-javascript", /* KMIME_APP_JAVASCRIPT */
	"application/json", /* KMIME_APP_JSON */
	"application/octet-stream", /* KMIME_APP_OCTET_STREAM */
	"application/pdf", /* KMIME_APP_PDF */
	"application/xml", /* KMIME_APP_XML */
	"application/zip", /* KMIME_APP_ZIP */
	"image/gif", /* KMIME_IMAGE_GIF */
	"image/jpeg", /* KMIME_IMAGE_JPEG */
	"image/png", /* KMIME_IMAGE_PNG */
	"image/svg+xml", /* KMIME_IMAGE_SVG_XML */
	"text/calendar", /* KMIME_TEXT_CALENDAR */
	"text/css", /* KMIME_TEXT_CSS */
	"text/csv", /* KMIME_TEXT_CSV */
	"text/html", /* KMIME_TEXT_HTML */
	"text/plain", /* KMIME_TEXT_PLAIN */
	"text/xml", /* KMIME_TEXT_XML */
};

const char *const khttps[KHTTP__MAX] = {
	"100 Continue",
	"101 Switching Protocols",
	"103 Checkpoint",
	"200 OK",
	"201 Created",
	"202 Accepted",
	"203 Non-Authoritative Information",
	"204 No Content",
	"205 Reset Content",
	"206 Partial Content",
	"207 Multi-Status",
	"300 Multiple Choices",
	"301 Moved Permanently",
	"302 Found",
	"303 See Other",
	"304 Not Modified",
	"306 Switch Proxy",
	"307 Temporary Redirect",
	"308 Resume Incomplete",
	"400 Bad Request",
	"401 Unauthorized",
	"402 Payment Required",
	"403 Forbidden",
	"404 Not Found",
	"405 Method Not Allowed",
	"406 Not Acceptable",
	"407 Proxy Authentication Required",
	"408 Request Timeout",
	"409 Conflict",
	"410 Gone",
	"411 Length Required",
	"412 Precondition Failed",
	"413 Request Entity Too Large",
	"414 Request-URI Too Long",
	"415 Unsupported Media Type",
	"416 Requested Range Not Satisfiable",
	"417 Expectation Failed",
	"424 Failed Dependency",
	"428 Precondition Required",
	"429 Too Many Requests",
	"431 Request Header Fields Too Large",
	"500 Internal Server Error",
	"501 Not Implemented",
	"502 Bad Gateway",
	"503 Service Unavailable",
	"504 Gateway Timeout",
	"505 HTTP Version Not Supported",
	"507 Insufficient Storage",
	"511 Network Authentication Required",
};

const struct kmimemap ksuffixmap[] = {
	{ "css", KMIME_TEXT_CSS },
	{ "csv", KMIME_TEXT_CSV },
	{ "gif", KMIME_IMAGE_GIF },
	{ "htm", KMIME_TEXT_HTML },
	{ "html", KMIME_TEXT_HTML },
	{ "ical", KMIME_TEXT_CALENDAR },
	{ "icalendar", KMIME_TEXT_CALENDAR },
	{ "ics", KMIME_TEXT_CALENDAR },
	{ "ifb", KMIME_TEXT_CALENDAR },
	{ "jpg", KMIME_IMAGE_JPEG },
	{ "jpeg", KMIME_IMAGE_JPEG },
	{ "js", KMIME_APP_JAVASCRIPT },
	{ "json", KMIME_APP_JSON },
	{ "pdf", KMIME_APP_PDF },
	{ "png", KMIME_IMAGE_PNG },
	{ "shtml", KMIME_TEXT_HTML },
	{ "svg", KMIME_IMAGE_SVG_XML },
	{ "svgz", KMIME_IMAGE_SVG_XML },
	{ "txt", KMIME_TEXT_PLAIN },
	{ "xml", KMIME_TEXT_XML },
	{ "zip", KMIME_APP_ZIP },
	{ NULL, KMIME__MAX },
};

/*
 * Default MIME suffix per type.
 */
const char *const ksuffixes[KMIME__MAX] = {
	"js", /* KMIME_APP_JAVASCRIPT */
	"json", /* KMIME_APP_JSON */
	NULL, /* KMIME_APP_OCTET_STREAM */
	"pdf", /* KMIME_APP_PDF */
	"xml", /* KMIME_APP_XML */
	"zip", /* KMIME_APP_ZIP */
	"gif", /* KMIME_IMAGE_GIF */
	"jpg", /* KMIME_IMAGE_JPEG */
	"png", /* KMIME_IMAGE_PNG */
	"svg", /* KMIME_IMAGE_PNG */
	"ics", /* KMIME_TEXT_CALENDAR */
	"css", /* KMIME_TEXT_CSS */
	"csv", /* KMIME_TEXT_CSV */
	"html", /* KMIME_TEXT_HTML */
	"txt", /* KMIME_TEXT_PLAIN */
	"xml", /* KMIME_TEXT_XML */
};

const char *const kerrors[] = {
	"success", /* KCGI_OK */
	"cannot allocate memory", /* KCGI_ENOMEM */
	"FastCGI exit", /* KCGI_EXIT */
	"end-point connection closed", /* KCGI_HUP */
	"too many open sockets", /* KCGI_ENFILE */
	"failed to fork child", /* KCGI_EAGAIN */
	"internal error", /* KCGI_FORM */
	"system error", /* KCGI_SYSTEM */
};

const char *
kcgi_strerror(enum kcgi_err er)
{

	assert(er <= KCGI_SYSTEM);
	assert(er >= KCGI_OK);
	return(kerrors[er]);
}

/*
 * Safe strdup(): don't return on exhaustion.
 */
char *
kstrdup(const char *cp)
{
	char	*p;

	if (NULL != (p = XSTRDUP(cp)))
		return(p);

	exit(EXIT_FAILURE);
}

/*
 * Safe realloc(): don't return on exhaustion.
 */
void *
krealloc(void *pp, size_t sz)
{
	char	*p;

	if (NULL != (p = XREALLOC(pp, sz)))
		return(p);

	exit(EXIT_FAILURE);
}

/*
 * Safe reallocarray(): don't return on exhaustion.
 */
void *
kreallocarray(void *pp, size_t nm, size_t sz)
{
	char	*p;

	if (NULL != (p = XREALLOCARRAY(pp, nm, sz)))
		return(p);

	exit(EXIT_FAILURE);
}

/*
 * Safe asprintf(): don't return on exhaustion.
 */
int
kasprintf(char **p, const char *fmt, ...)
{
	va_list	 ap;
	int	 len;

	va_start(ap, fmt);
	len = XVASPRINTF(p, fmt, ap);
	va_end(ap);

	if (len >= 0)
		return(len);

	exit(EXIT_FAILURE);
}

/*
 * Safe calloc(): don't return on exhaustion.
 */
void *
kcalloc(size_t nm, size_t sz)
{
	char	*p;

	if (NULL != (p = XCALLOC(nm, sz)))
		return(p);

	exit(EXIT_FAILURE);
}

/*
 * Safe malloc(): don't return on exhaustion.
 */
void *
kmalloc(size_t sz)
{
	char	*p;

	if (NULL != (p = XMALLOC(sz)))
		return(p);

	exit(EXIT_FAILURE);
}

char *
kutil_urlencode(const char *cp)
{
	char	*p;
	char	 ch;
	size_t	 sz, cur;

	if (NULL == cp)
		return(NULL);

	/* 
	 * Leave three bytes per input byte for encoding. 
	 * This ensures we needn't range-check.
	 * First check whether our size overflows. 
	 * We do this here because we need our size!
	 */

	sz = strlen(cp) + 1;
	if (SIZE_MAX / 3 < sz) {
		XWARNX("multiplicative overflow: %zu", sz);
		return(NULL);
	}
	if (NULL == (p = XCALLOC(sz, 3)))
		return(NULL);

	for (cur = 0; '\0' != (ch = *cp); cp++) {
		if (isalnum((unsigned char)ch) || ch == '-' || 
		    ch == '_' || ch == '.' || ch == '~') {
			p[cur++] = ch;
			continue;
		} else if (' ' == ch) {
			p[cur++] = '+';
			continue;
		}
		cur += snprintf(p + cur, 4, "%%%.2x", 
			(unsigned char)ch);
	}

	return(p);
}

char *
kutil_urlabs(enum kscheme scheme, 
	const char *host, uint16_t port, const char *path)
{
	char	*p;

	XASPRINTF(&p, "%s://%s:%" PRIu16 "%s", 
	    kschemes[scheme], host, port, path);
	return(p);
}

char *
kutil_urlpartx(struct kreq *req, const char *path,
	const char *mime, const char *page, ...)
{
	va_list		 ap;
	int		 rc;
	char		*p, *pp, *keyp, *valp, *valpp;
	size_t		 total, count;
	char	 	 buf[256]; /* max double/int64_t */

	if (NULL == (pp = kutil_urlencode(page)))
		return(NULL);

	/* If we have a MIME type, append it. */

	rc = NULL != mime ?
		XASPRINTF(&p, "%s%s%s.%s", 
			NULL != path ? path : "",
			NULL != path ? "/" : "", pp, mime) :
		XASPRINTF(&p, "%s%s%s", 
			NULL != path ? path : "",
			NULL != path ? "/" : "", pp);

	free(pp);

	if (rc < 0)
		return(NULL);

	total = strlen(p) + 1;
	va_start(ap, page);
	count = 0;

	while (NULL != (pp = va_arg(ap, char *))) {
		keyp = kutil_urlencode(pp);
		if (NULL == keyp) {
			free(p);
			return(NULL);
		}

		valp = valpp = NULL;

		switch (va_arg(ap, enum kattrx)) {
		case (KATTRX_STRING):
			valp = kutil_urlencode(va_arg(ap, char *));
			valpp = valp;
			break;
		case (KATTRX_INT):
			(void)snprintf(buf, sizeof(buf),
				"%" PRId64, va_arg(ap, int64_t));
			valp = buf;
			break;
		case (KATTRX_DOUBLE):
			(void)snprintf(buf, sizeof(buf),
				"%g", va_arg(ap, double));
			valp = buf;
			break;
		default:
			free(p);
			free(keyp);
			return(NULL);
		}

		if (NULL == valp) {
			free(p);
			free(keyp);
			return(NULL);
		}

		/* Size for key, value, ? or &, and =. */
		/* FIXME: check for overflow! */

		total += strlen(keyp) + strlen(valp) + 2;

		if (NULL == (pp = XREALLOC(p, total))) {
			free(p);
			free(keyp);
			free(valpp);
			return(NULL);
		}
		p = pp;

		if (count > 0)
			(void)strlcat(p, "&", total);
		else
			(void)strlcat(p, "?", total);

		(void)strlcat(p, keyp, total);
		(void)strlcat(p, "=", total);
		(void)strlcat(p, valp, total);

		free(keyp);
		free(valpp);
		count++;
	}

	va_end(ap);
	return(p);
}

char *
kutil_urlpart(struct kreq *req, const char *path,
	const char *mime, const char *page, ...)
{
	va_list		 ap;
	char		*p, *pp, *keyp, *valp;
	size_t		 total, count;
	int		 len;

	if (NULL == (pp = kutil_urlencode(page)))
		return(NULL);

	/* If we have a MIME type, append it. */

	len = NULL != mime ?
		XASPRINTF(&p, "%s%s%s.%s", 
			NULL != path ? path : "",
			NULL != path ? "/" : "", pp, mime) :
		XASPRINTF(&p, "%s%s%s", 
			NULL != path ? path : "",
			NULL != path ? "/" : "", pp);

	free(pp);

	if (len < 0)
		return(NULL);

	total = strlen(p) + 1;
	va_start(ap, page);
	count = 0;

	while (NULL != (pp = va_arg(ap, char *))) {
		keyp = kutil_urlencode(pp);
		if (NULL == keyp) {
			free(p);
			return(NULL);
		}

		valp = kutil_urlencode(va_arg(ap, char *));
		if (NULL == valp) {
			free(p);
			free(keyp);
			return(NULL);
		}

		/* Size for key, value, ? or &, and =. */
		/* FIXME: check for overflow! */

		total += strlen(keyp) + strlen(valp) + 2;

		if (NULL == (pp = XREALLOC(p, total))) {
			free(p);
			free(keyp);
			free(valp);
			return(NULL);
		}
		p = pp;

		if (count > 0)
			(void)strlcat(p, "&", total);
		else
			(void)strlcat(p, "?", total);

		(void)strlcat(p, keyp, total);
		(void)strlcat(p, "=", total);
		(void)strlcat(p, valp, total);

		free(keyp);
		free(valp);
		count++;
	}

	va_end(ap);
	return(p);
}

static void
kpair_free(struct kpair *p, size_t sz)
{
	size_t		 i;

	for (i = 0; i < sz; i++) {
		free(p[i].key);
		free(p[i].val);
		free(p[i].file);
		free(p[i].ctype);
		free(p[i].xcode);
	}
	free(p);
}

void
kreq_free(struct kreq *req)
{
	size_t	 i;

	for (i = 0; i < req->reqsz; i++) {
		free(req->reqs[i].key);
		free(req->reqs[i].val);
	}

	free(req->reqs);
	kpair_free(req->cookies, req->cookiesz);
	kpair_free(req->fields, req->fieldsz);
	free(req->path);
	free(req->fullpath);
	free(req->remote);
	free(req->host);
	free(req->cookiemap);
	free(req->cookienmap);
	free(req->fieldmap);
	free(req->fieldnmap);
	free(req->suffix);
	free(req->pagename);
	free(req->pname);
	free(req->rawauth.digest);
	if (KAUTH_DIGEST == req->rawauth.type) {
		free(req->rawauth.d.digest.user);
		free(req->rawauth.d.digest.uri);
		free(req->rawauth.d.digest.realm);
		free(req->rawauth.d.digest.nonce);
		free(req->rawauth.d.digest.cnonce);
		free(req->rawauth.d.digest.response);
		free(req->rawauth.d.digest.opaque);
	} else if (KAUTH_BASIC == req->rawauth.type) 
		free(req->rawauth.d.basic.response);
}

enum kcgi_err
khttp_parse(struct kreq *req, 
	const struct kvalid *keys, size_t keysz,
	const char *const *pages, size_t pagesz,
	size_t defpage)
{

	return(khttp_parsex(req, ksuffixmap, kmimetypes, 
		KMIME__MAX, keys, keysz, pages, pagesz, 
		KMIME_TEXT_HTML, defpage, NULL, NULL, 0, NULL));
}

enum kcgi_err
khttp_parsex(struct kreq *req, 
	const struct kmimemap *suffixmap, 
	const char *const *mimes, size_t mimesz,
	const struct kvalid *keys, size_t keysz,
	const char *const *pages, size_t pagesz,
	size_t defmime, size_t defpage, void *arg,
	void (*argfree)(void *arg), unsigned debugging,
	const struct kopts *opts)
{
	const struct kmimemap *mm;
	enum kcgi_err	  kerr;
	int 		  er;
	struct kopts	  kopts;
	void		 *work_box;
	int		  work_dat[2];
	pid_t		  work_pid;

	memset(req, 0, sizeof(struct kreq));

	/*
	 * We'll be using poll(2) for reading our HTTP document, so this
	 * must be non-blocking in order to make the reads not spin the
	 * CPU.
	 */
	if (KCGI_OK != kxsocketprep(STDIN_FILENO)) {
		XWARNX("kxsocketprep");
		return KCGI_SYSTEM;
	} else if ( ! ksandbox_alloc(&work_box))
		return KCGI_ENOMEM;

	if (KCGI_OK != kxsocketpair(AF_UNIX, SOCK_STREAM, 0, work_dat)) {
		ksandbox_free(work_box);
		return KCGI_SYSTEM;
	}

	if (-1 == (work_pid = fork())) {
		er = errno;
		XWARN("fork");
		close(work_dat[KWORKER_PARENT]);
		close(work_dat[KWORKER_CHILD]);
		ksandbox_free(work_box);
		return EAGAIN == er ? KCGI_EAGAIN : KCGI_ENOMEM;
	} else if (0 == work_pid) {
		/* Conditionally free our argument. */
		if (NULL != argfree)
			(*argfree)(arg);
		close(STDOUT_FILENO);
		close(work_dat[KWORKER_PARENT]);
		er = EXIT_FAILURE;
		if ( ! ksandbox_init_child
			(work_box, SAND_WORKER,
			 work_dat[KWORKER_CHILD], -1, -1, -1)) {
			XWARNX("ksandbox_init_child");
		} else if (KCGI_OK != kworker_child
			   (work_dat[KWORKER_CHILD], keys,
			    keysz, mimes, mimesz, debugging)) {
			XWARNX("kworker_child");
		} else
			er = EXIT_SUCCESS;
		ksandbox_free(work_box);
		close(work_dat[KWORKER_CHILD]);
		_exit(er);
		/* NOTREACHED */
	}

	close(work_dat[KWORKER_CHILD]);
	work_dat[KWORKER_CHILD] = -1;

	if ( ! ksandbox_init_parent
		 (work_box, SAND_WORKER, work_pid)) {
		XWARNX("ksandbox_init_parent");
		kerr = KCGI_SYSTEM;
		goto err;
	}

	if (NULL == opts)
		kopts.sndbufsz = -1;
	else
		memcpy(&kopts, opts, sizeof(struct kopts));

	if (kopts.sndbufsz < 0)
		kopts.sndbufsz = 1024 * 8;

	kerr = KCGI_ENOMEM;

	/*
	 * After this point, all errors should use "goto err", which
	 * will properly free up our memory and close any extant file
	 * descriptors.
	 * Also, we're running our child in the background, so make sure
	 * that it gets killed!
	 */
	req->arg = arg;
	req->keys = keys;
	req->keysz = keysz;
	req->kdata = kdata_alloc(-1, -1, 0, debugging, &kopts);
	if (NULL == req->kdata)
		goto err;

	if (keysz) {
		req->cookiemap = XCALLOC(keysz, sizeof(struct kpair *));
		if (NULL == req->cookiemap)
			goto err;
		req->cookienmap = XCALLOC(keysz, sizeof(struct kpair *));
		if (NULL == req->cookienmap)
			goto err;
		req->fieldmap = XCALLOC(keysz, sizeof(struct kpair *));
		if (NULL == req->fieldmap)
			goto err;
		req->fieldnmap = XCALLOC(keysz, sizeof(struct kpair *));
		if (NULL == req->fieldnmap)
			goto err;
	}

	/*
	 * Now read the input fields from the child and conditionally
	 * assign them to our lookup table.
	 */
	kerr = kworker_parent(work_dat[KWORKER_PARENT], req, 1, mimesz);
	if (KCGI_OK != kerr)
		goto err;

	/* Look up page type from component. */
	req->page = defpage;
	if ('\0' != *req->pagename)
		for (req->page = 0; req->page < pagesz; req->page++)
			if (0 == strcasecmp(pages[req->page], req->pagename))
				break;

	/* Start with the default. */
	req->mime = defmime;
	if ('\0' != *req->suffix) {
		for (mm = suffixmap; NULL != mm->name; mm++)
			if (0 == strcasecmp(mm->name, req->suffix)) {
				req->mime = mm->mime;
				break;
			}
		 /* Could not find this mime type! */
		if (NULL == mm->name)
			req->mime = mimesz;
	}

	close(work_dat[KWORKER_PARENT]);
	work_dat[KWORKER_PARENT] = -1;
	kerr = kxwaitpid(work_pid);
	work_pid = -1;
	if (KCGI_OK != kerr)
		goto err;
	ksandbox_close(work_box);
	ksandbox_free(work_box);
	return kerr;
err:
	assert(KCGI_OK != kerr);
	if (-1 != work_dat[KWORKER_PARENT])
		close(work_dat[KWORKER_PARENT]);
	if (-1 != work_pid)
		kxwaitpid(work_pid);
	ksandbox_close(work_box);
	ksandbox_free(work_box);
	kdata_free(req->kdata, 0);
	req->kdata = NULL;
	kreq_free(req);
	return kerr;
}

void
kutil_invalidate(struct kreq *r, struct kpair *kp)
{
	struct kpair	*p, *lastp;
	size_t		 i;

	if (NULL == kp)
		return;

	kp->type = KPAIR__MAX;
	kp->state = KPAIR_INVALID;
	memset(&kp->parsed, 0, sizeof(union parsed));

	/* We're not bucketed. */

	if ((i = kp->keypos) == r->keysz)
		return;

	/* Is it in our fieldmap? */

	if (NULL != r->fieldmap[i]) {
		if (kp == r->fieldmap[i]) {
			r->fieldmap[i] = kp->next;
			kp->next = r->fieldnmap[i];
			r->fieldnmap[i] = kp;
			return;
		} 
		lastp = r->fieldmap[i];
		p = lastp->next;
		for ( ; NULL != p; lastp = p, p = p->next)
			if (kp == p) {
				lastp->next = kp->next;
				kp->next = r->fieldnmap[i];
				r->fieldnmap[i] = kp;
				return;
			}
	}

	/* ...cookies? */

	if (NULL != r->cookiemap[i]) {
		if (kp == r->cookiemap[i]) {
			r->cookiemap[i] = kp->next;
			kp->next = r->cookienmap[i];
			r->cookienmap[i] = kp;
			return;
		} 
		lastp = r->cookiemap[i];
		p = lastp->next;
		for ( ; NULL != p; lastp = p, p = p->next) 
			if (kp == p) {
				lastp->next = kp->next;
				kp->next = r->cookienmap[i];
				r->cookienmap[i] = kp;
				return;
			}
	}
}

void
khttp_child_free(struct kreq *req)
{

	kdata_free(req->kdata, 0);
	req->kdata = NULL;
	kreq_free(req);
}

void
khttp_free(struct kreq *req)
{

	kdata_free(req->kdata, 1);
	req->kdata = NULL;
	kreq_free(req);
}

/*
 * Trim leading and trailing whitespace from a word.
 * Note that this returns a pointer within "val" and optionally sets the
 * NUL-terminator, so don't free() the returned value.
 */
static char *
trim(char *val)
{
	char		*cp;

	while (isspace((unsigned char)*val))
		val++;

	cp = strchr(val, '\0') - 1;
	while (cp > val && isspace((unsigned char)*cp))
		*cp-- = '\0';

	return(val);
}

/*
 * Simple email address validation: this is NOT according to the spec,
 * but a simple heuristic look at the address.
 * Note that this lowercases the mail address.
 */
static char *
valid_email(char *p)
{
	char		*domain, *cp, *start;
	size_t		 i, sz;

	cp = start = trim(p);

	if ((sz = strlen(cp)) < 5 || sz > 254)
		return(NULL);
	if (NULL == (domain = strchr(cp, '@')))
		return(NULL);
	if ((sz = domain - cp) < 1 || sz > 64)
		return(NULL);

	for (i = 0; i < sz; i++) {
		if (isalnum((unsigned char)cp[i]))
			continue;
		if (NULL == strchr("!#$%&'*+-/=?^_`{|}~.", cp[i]))
			return(NULL);
	}

	assert('@' == cp[i]);
	cp = &cp[++i];
	if ((sz = strlen(cp)) < 4 || sz > 254)
		return(NULL);

	for (i = 0; i < sz; i++) 
		if ( ! isalnum((unsigned char)cp[i]))
			if (NULL == strchr("-.", cp[i]))
				return(NULL);

	for (cp = start; '\0' != *cp; cp++)
		*cp = tolower((unsigned char)*cp);

	return(start);
}

int
kvalid_date(struct kpair *kp)
{
	int		 mday, mon, year;

	if (kp->valsz != 10 || '\0' != kp->val[10] ||
	    ! isdigit((unsigned char)kp->val[0]) ||
	    ! isdigit((unsigned char)kp->val[1]) ||
	    ! isdigit((unsigned char)kp->val[2]) ||
	    ! isdigit((unsigned char)kp->val[3]) ||
	    '-' != kp->val[4] || 
	    ! isdigit((unsigned char)kp->val[5]) ||
	    ! isdigit((unsigned char)kp->val[6]) ||
	    '-' != kp->val[7] || 
	    ! isdigit((unsigned char)kp->val[8]) ||
	    ! isdigit((unsigned char)kp->val[9]))
		return(0);

	year = atoi(&kp->val[0]);
	mon = atoi(&kp->val[5]);
	mday = atoi(&kp->val[8]);

	if ( ! kutil_date_check(mday, mon, year))
		return(0);

	kp->parsed.i = kutil_date2epoch(mday, mon, year);
	kp->type = KPAIR_INTEGER;
	return(1);
}

int
kvalid_stringne(struct kpair *p)
{

	/*
	 * To check if we're a valid string, simply make sure that the
	 * NUL-terminator is where we expect it to be.
	 */
	if (strlen(p->val) != p->valsz || 0 == p->valsz)
		return(0);
	p->type = KPAIR_STRING;
	p->parsed.s = p->val;
	return(1);
}

int
kvalid_string(struct kpair *p)
{

	/*
	 * To check if we're a valid string, simply make sure that the
	 * NUL-terminator is where we expect it to be.
	 */
	if (strlen(p->val) != p->valsz)
		return(0);
	p->type = KPAIR_STRING;
	p->parsed.s = p->val;
	return(1);
}

int
kvalid_email(struct kpair *p)
{

	if ( ! kvalid_stringne(p))
		return(0);
	return(NULL != (p->parsed.s = valid_email(p->val)));
}

int
kvalid_udouble(struct kpair *p)
{

	return(kvalid_double(p) && p->parsed.d > 0.0);
}

int
kvalid_double(struct kpair *p)
{
	char		*ep;
	const char	*nval;
	double		 lval;
	int		 er;

	if ( ! kvalid_stringne(p))
		return(0);

	/* 
	 * We might get an empty string from trim, which constitutes a
	 * valid double (!?), so double check that the string is
	 * non-empty after trimming whitespace.
	 * We trim white-space because strtod(3) accepts white-space
	 * before but not after the string.
	 */

	nval = trim(p->val);
	if ('\0' == nval[0])
		return(0);

	/* Save errno so we can restore it later. */

	er = errno;
	errno = 0;
	lval = strtod(nval, &ep);
	if (errno == ERANGE)
		return(0);

	/* Restore errno. */

	errno = er;

	if (*ep != '\0')
		return(0);

	p->parsed.d = lval;
	p->type = KPAIR_DOUBLE;
	return(1);
}

int
kvalid_int(struct kpair *p)
{
	const char	*ep;

	if ( ! kvalid_stringne(p))
		return(0);
	p->parsed.i = strtonum
		(trim(p->val), INT64_MIN, INT64_MAX, &ep);
	p->type = KPAIR_INTEGER;
	return(NULL == ep);
}

int
kvalid_bit(struct kpair *p)
{

	if ( ! kvalid_uint(p))
		return(0);
	return(p->parsed.i <= 64);
}

int
kvalid_uint(struct kpair *p)
{
	const char	*ep;

	p->parsed.i = strtonum(trim(p->val), 0, INT64_MAX, &ep);
	p->type = KPAIR_INTEGER;
	return(NULL == ep);
}

enum kcgi_err
kcgi_buf_write(const char *s, size_t sz, void *arg)
{
	struct kcgi_buf	*b = arg;
	void		*pp;

	if (NULL == s)
		return(KCGI_OK);

	if (b->sz + sz + 1 > b->maxsz) {
		b->maxsz = b->sz + sz + 1 + 
			(0 == b->growsz ? 1024 : b->growsz);
		pp = realloc(b->buf, b->maxsz);
		if (NULL == pp)
			return(KCGI_ENOMEM);
		b->buf = pp;
	}

	memcpy(&b->buf[b->sz], s, sz);
	b->sz += sz;
	b->buf[b->sz] = '\0';
	return(KCGI_OK);
}

enum kcgi_err
kcgi_buf_putc(struct kcgi_buf *buf, char c)
{

	return(kcgi_buf_write(&c, 1, buf));
}

enum kcgi_err
kcgi_buf_puts(struct kcgi_buf *buf, const char *cp)
{

	return(kcgi_buf_write(cp, strlen(cp), buf));
}
