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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

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
#ifdef HAVE_ZLIB
#include <zlib.h>
#endif

#include "kcgi.h"
#include "extern.h"

/*
 * Maximum size of printing a signed 64-bit integer.
 */
#define	INT_MAXSZ	 22

/*
 * The state of our HTTP response.
 * We can be in KSTATE_HEAD, where we're printing HTTP headers; or
 * KSTATE_BODY, where we're printing the body parts.
 * So if we try to print a header when we're supposed to be in the body,
 * this will be caught.
 */
enum	kstate {
	KSTATE_HEAD = 0,
	KSTATE_BODY
};

/*
 * Interior data.
 * This is used for managing HTTP compression.
 */
struct	kdata {
	enum kstate	 state;
#ifdef	HAVE_ZLIB
	gzFile		 gz;
#endif
};

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
	{ "png", KMIME_IMAGE_PNG },
	{ "shtml", KMIME_TEXT_HTML },
	{ "svg", KMIME_IMAGE_SVG_XML },
	{ "svgz", KMIME_IMAGE_SVG_XML },
	{ NULL, KMIME__MAX },
};

/*
 * Default MIME suffix per type.
 */
const char *const ksuffixes[KMIME__MAX] = {
	"js", /* KMIME_APP_JAVASCRIPT */
	"json", /* KMIME_APP_JSON */
	NULL, /* KMIME_APP_OCTET_STREAM */
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

/* 
 * Name of executing CGI script.
 */
const char	*pname = NULL;

void
khttp_write(struct kreq *req, const char *buf, size_t sz)
{

#ifdef HAVE_ZLIB
	if (NULL != req->kdata->gz)
		gzwrite(req->kdata->gz, buf, sz);
	else 
#endif
		fwrite(buf, 1, sz, stdout);
}

static int
khttp_templatex_write(const char *dat, size_t sz, void *arg)
{

	khttp_write(arg, dat, sz);
	return(1);
}

void
khttp_puts(struct kreq *req, const char *cp)
{

#ifdef HAVE_ZLIB
	if (NULL != req->kdata->gz)
		gzputs(req->kdata->gz, cp);
	else 
#endif
		fputs(cp, stdout);
}

void
khttp_putc(struct kreq *req, int c)
{

#ifdef HAVE_ZLIB
	if (NULL != req->kdata->gz)
		gzputc(req->kdata->gz, c);
	else 
#endif
		putchar(c);
}

char *
kstrdup(const char *cp)
{
	char	*p;

	if (NULL != (p = XSTRDUP(cp)))
		return(p);

	exit(EXIT_FAILURE);
}

void *
krealloc(void *pp, size_t sz)
{
	char	*p;

	assert(sz > 0);
	if (NULL != (p = XREALLOC(pp, sz)))
		return(p);

	exit(EXIT_FAILURE);
}

void *
kreallocarray(void *pp, size_t nm, size_t sz)
{
	char	*p;

	if (NULL != (p = XREALLOCARRAY(pp, nm, sz)))
		return(p);

	exit(EXIT_FAILURE);
}

int
kasprintf(char **p, const char *fmt, ...)
{
	va_list	 ap;
	int	 len;

	va_start(ap, fmt);
	len = XVASPRINTF(p, fmt, ap);
	va_end(ap);

	if (-1 != len)
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
	size_t	 sz;
	char	 buf[4];

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
	sz *= 3;

	for ( ; '\0' != (ch = *cp); cp++) {
		/* Put in a temporary buffer then concatenate. */
		memset(buf, 0, sizeof(buf));
		if (' ' == ch) 
			buf[0] = '+';
		else if (isalnum((int)ch) || ch == '-' || 
			ch == '_' || ch == '.' || ch == '~') 
			buf[0] = ch;
		else
			(void)snprintf(buf, sizeof(buf), "%%%.2x", ch);
		(void)strlcat(p, buf, sz);
	}

	return(p);
}

char *
kutil_urlabs(enum kscheme scheme, 
	const char *host, uint16_t port, const char *path)
{
	char	*p;

	(void)kasprintf(&p, "%s://%s:%" PRIu16 "%s", 
		kschemes[scheme], host, port, path);

	return(p);
}

char *
kutil_urlpartx(struct kreq *req, const char *path,
	const char *mime, const char *page, ...)
{
	va_list		 ap;
	char		*p, *pp, *keyp, *valp;
	size_t		 total, count;
	char	 	 buf[256]; /* max double/int64_t */

	if (NULL == (pp = kutil_urlencode(page)))
		exit(EXIT_FAILURE);

	if (NULL != mime)
		(void)kasprintf(&p, "%s/%s.%s", path, pp, mime);
	else
		(void)kasprintf(&p, "%s/%s", path, pp);

	free(pp);
	total = strlen(p) + 1;
	va_start(ap, page);
	count = 0;
	while (NULL != (pp = va_arg(ap, char *))) {
		keyp = kutil_urlencode(pp);
		if (NULL == keyp)
			exit(EXIT_FAILURE);

		valp = NULL;
		switch (va_arg(ap, enum kattrx)) {
		case (KATTRX_STRING):
			valp = kutil_urlencode(va_arg(ap, char *));
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
		}

		if (NULL == valp) 
			exit(EXIT_FAILURE);

		/* Size for key, value, ? or &, and =. */
		/* FIXME: check for overflow! */
		total += strlen(keyp) + strlen(valp) + 2;
		p = krealloc(p, total);

		if (count > 0)
			(void)strlcat(p, "&", total);
		else
			(void)strlcat(p, "?", total);

		(void)strlcat(p, keyp, total);
		(void)strlcat(p, "=", total);
		(void)strlcat(p, valp, total);

		free(keyp);
		if (valp != buf)
			free(valp);
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

	if (NULL == (pp = kutil_urlencode(page)))
		exit(EXIT_FAILURE);

	if (NULL != mime)
		(void)kasprintf(&p, "%s/%s.%s", path, pp, mime);
	else
		(void)kasprintf(&p, "%s/%s", path, pp);

	free(pp);
	total = strlen(p) + 1;
	va_start(ap, page);
	count = 0;
	while (NULL != (pp = va_arg(ap, char *))) {
		keyp = kutil_urlencode(pp);
		if (NULL == keyp)
			exit(EXIT_FAILURE);
		valp = kutil_urlencode(va_arg(ap, char *));
		if (NULL == valp) 
			exit(EXIT_FAILURE);

		/* Size for key, value, ? or &, and =. */
		/* FIXME: check for overflow! */
		total += strlen(keyp) + strlen(valp) + 2;
		p = krealloc(p, total);

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

static void
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
	free(req->kdata);
	free(req->suffix);
	free(req->pagename);
	free(req->pname);
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
		KMIME_TEXT_HTML, defpage, NULL, NULL));
}

enum kcgi_err
khttp_parsex(struct kreq *req, 
	const struct kmimemap *suffixmap, 
	const char *const *mimes, size_t mimesz,
	const struct kvalid *keys, size_t keysz,
	const char *const *pages, size_t pagesz,
	size_t defmime, size_t defpage, void *arg,
	void (*argfree)(void *arg))
{
	char		*cp;
	const struct kmimemap *mm;
	enum kcgi_err	 kerr;
	int 		 er;
	struct kworker	 work;

	/*
	 * We'll be using poll(2) for reading our HTTP document, so this
	 * must be non-blocking in order to make the reads not spin the
	 * CPU.
	 */
	if (-1 == fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK)) {
		XWARN("fcntl: O_NONBLOCK");
		return(KCGI_SYSTEM);
	}

	if (KCGI_OK != (kerr = kworker_init(&work)))
		return(kerr);
	work.input = STDIN_FILENO;

	if (-1 == (work.pid = fork())) {
		er = errno;
		XWARN("fork");
		return(EAGAIN == er ? KCGI_EAGAIN : KCGI_ENOMEM);
	} else if (0 == work.pid) {
		/* Conditionally free our argument. */
		if (NULL != argfree && NULL != arg)
			(*argfree)(arg);
		kworker_prep_child(&work);
		kworker_child(&work, keys, keysz, mimes, mimesz);
		kworker_free(&work);
		_exit(EXIT_SUCCESS);
		/* NOTREACHED */
	}
	kworker_prep_parent(&work);

	memset(req, 0, sizeof(struct kreq));
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
	req->kdata = XCALLOC(1, sizeof(struct kdata));
	if (NULL == req->kdata)
		goto err;
	req->cookiemap = XCALLOC(keysz, sizeof(struct kpair *));
	if (keysz && NULL == req->cookiemap)
		goto err;
	req->cookienmap = XCALLOC(keysz, sizeof(struct kpair *));
	if (keysz && NULL == req->cookienmap)
		goto err;
	req->fieldmap = XCALLOC(keysz, sizeof(struct kpair *));
	if (keysz && NULL == req->fieldmap)
		goto err;
	req->fieldnmap = XCALLOC(keysz, sizeof(struct kpair *));
	if (keysz && NULL == req->fieldnmap)
		goto err;

	if (NULL == (cp = getenv("SCRIPT_NAME")))
		req->pname = XSTRDUP("");
	else
		req->pname = XSTRDUP(cp);

	if (NULL == req->pname)
		goto err;

	/* Never supposed to be NULL, but to be sure... */
	if (NULL == (cp = getenv("HTTP_HOST")))
		req->host = XSTRDUP("localhost");
	else
		req->host = XSTRDUP(cp);

	if (NULL == req->host)
		goto err;

	req->port = 80;
	if (NULL != (cp = getenv("SERVER_PORT")))
		req->port = strtonum(cp, 0, 80, NULL);

	/*
	 * Now read the input fields from the child and conditionally
	 * assign them to our lookup table.
	 */
	kerr = kworker_parent(work.sock[KWORKER_READ], req, work.pid);
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

	kerr = kworker_close(&work);
	kworker_free(&work);
	return(kerr);
err:
	kworker_kill(&work);
	kreq_free(req);
	(void)kworker_close(&work);
	kworker_free(&work);
	return(kerr);
}

/*
 * This entire function would be two lines if we were using the
 * sys/queue macros.h.
 * FIXME: this can be simplified with the new "keypos" value.
 */
void
kutil_invalidate(struct kreq *r, struct kpair *kp)
{
	struct kpair	*p, *lastp;
	size_t		 i;

	if (NULL == kp)
		return;
	else if (KPAIR__MAX == kp->type)
		return;

	/*
	 * Look through our entire fieldmap to locate the pair, then
	 * move it into the "bad" list.
	 */
	kp->type = KPAIR__MAX;
	for (i = 0; i < r->keysz; i++) {
		/* First time's the charm. */
		if (kp == r->fieldmap[i]) {
			/* Move existing fieldmap. */
			r->fieldmap[i] = kp->next;
			/* Invalidate, append to fieldnmap. */
			kp->next = r->fieldnmap[i];
			r->fieldnmap[i] = kp;
			return;
		} else if (NULL == r->fieldmap[i])
			continue;

		/* See if we're buried in the list. */
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
	/*
	 * Same as above, but look in the cookie map.
	 */
	for (i = 0; i < r->keysz; i++) {
		if (kp == r->cookiemap[i]) {
			r->cookiemap[i] = kp->next;
			kp->next = r->cookienmap[i];
			r->cookienmap[i] = kp;
			return;
		} else if (NULL == r->cookiemap[i])
			continue;
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

	close(STDOUT_FILENO);
	close(STDIN_FILENO);

#ifdef HAVE_ZLIB
	if (NULL != req->kdata->gz)
		gzclose(req->kdata->gz);
#endif
	kreq_free(req);
}


void
khttp_free(struct kreq *req)
{

#ifdef HAVE_ZLIB
	if (NULL != req->kdata->gz)
		gzclose(req->kdata->gz);
	else
#endif
		fflush(stdout);
	kreq_free(req);
}

void
khttp_head(struct kreq *req, const char *key, const char *fmt, ...)
{
	va_list	 ap;

	assert(KSTATE_HEAD == req->kdata->state);

	printf("%s: ", key);
	va_start(ap, fmt);
	vprintf(fmt, ap);
	printf("\r\n");
	va_end(ap);
}

void
khttp_body(struct kreq *req)
{
#ifdef HAVE_ZLIB
	const char	*cp;
#endif
	assert(KSTATE_HEAD == req->kdata->state);

	/*
	 * If gzip is an accepted encoding, then create the "gz" stream
	 * that will be used for all subsequent I/O operations.
	 * Gracefully fall through on errors.
	 */
#ifdef HAVE_ZLIB
	if (NULL != (cp = getenv("HTTP_ACCEPT_ENCODING")) &&
			NULL != strstr(cp, "gzip")) {
		req->kdata->gz = gzdopen(STDOUT_FILENO, "w");
		if (NULL == req->kdata->gz)
			XWARN("gzdopen");
		else
			khttp_head(req, kresps[KRESP_CONTENT_ENCODING], 
				"%s", "gzip");
	} 
#endif
	/*
	 * XXX: newer versions of zlib have a "T" transparent mode that
	 * one can add to gzdopen() that allows using the gz without any
	 * compression.
	 * Unfortunately, that's not guaranteed on all systems, so we
	 * must do without it.
	 */

	fputs("\r\n", stdout);
	fflush(stdout);
	req->kdata->state = KSTATE_BODY;
}

/*
 * Trim leading and trailing whitespace from a word.
 * Note that this returns a pointer within "val" and optionally sets the
 * nil-terminator, so don't free() the returned value.
 */
static char *
trim(char *val)
{
	char		*cp;

	if ('\0' == *val)
		return(val);

	cp = val + strlen(val) - 1;
	while (cp > val && isspace((unsigned char)*cp))
		*cp-- = '\0';

	cp = val;
	while (isspace((unsigned char)*cp))
		cp++;

	return(cp);
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

static long
mkdate(int d, int m, int y)
{

	m = (m + 9) % 12;
	y = y - m / 10;
	return(365 * y + y / 4 - y / 100 + 
		y / 400 + (m * 306 + 5) / 10 + (d - 1));
}

int
kvalid_date(struct kpair *kp)
{
	int		 mday, mon, year;
	long		 v;

	if ( ! kvalid_stringne(kp))
		return(0);
	else if (kp->valsz != 10)
		return(0);
	else if ( ! isdigit((int)kp->val[0]) ||
		! isdigit((int)kp->val[1]) ||
		! isdigit((int)kp->val[2]) ||
		! isdigit((int)kp->val[3]) ||
		'-' != kp->val[4] || 
		! isdigit((int)kp->val[5]) ||
		! isdigit((int)kp->val[6]) ||
		'-' != kp->val[7] || 
		! isdigit((int)kp->val[8]) ||
		! isdigit((int)kp->val[9]))
		return(0);

	year = atoi(&kp->val[0]);
	mon = atoi(&kp->val[5]);
	mday = atoi(&kp->val[8]);

	v = mkdate(mday, mon, year) * 86400;
	kp->parsed.i = v - mkdate(1, 1, 1970) * 86400;
	kp->type = KPAIR_INTEGER;
	return(1);
}

int
kvalid_stringne(struct kpair *p)
{

	/*
	 * To check if we're a valid string, simply make sure that the
	 * nil pointer is where we expect it to be.
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
	 * nil pointer is where we expect it to be.
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

	if ( ! kvalid_double(p))
		return(0);
	p->type = KPAIR_DOUBLE;
	return(p->parsed.d > 0.0);
}

int
kvalid_double(struct kpair *p)
{
	char		*ep;
	double		 lval;

	if ( ! kvalid_stringne(p))
		return(0);

	errno = 0;
	lval = strtod(p->val, &ep);
	if (p->val[0] == '\0' || *ep != '\0')
		return(0);
	if (errno == ERANGE && (lval == HUGE_VAL || lval == -HUGE_VAL))
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
kvalid_uint(struct kpair *p)
{
	const char	*ep;

	p->parsed.i = strtonum(trim(p->val), 0, INT64_MAX, &ep);
	p->type = KPAIR_INTEGER;
	return(NULL == ep);
}

int
khttp_template_buf(struct kreq *req, 
	const struct ktemplate *t, const char *buf, size_t sz)
{

	assert(KSTATE_BODY == req->kdata->state);
	return(khttp_templatex_buf(t, buf, sz, khttp_templatex_write, req));
}

int
khttp_templatex_buf(const struct ktemplate *t, 
	const char *buf, size_t sz, ktemplate_writef fp, void *arg)
{
	size_t		 i, j, len, start, end;

	if (NULL == t)
		return(fp(buf, sz, arg));

	for (i = 0; i < sz - 1; i++) {
		/* Look for the starting "@@" marker. */
		if ('@' != buf[i] || '@' != buf[i + 1]) {
			if ( ! fp(&buf[i], 1, arg))
				return(0);
			continue;
		} 

		/* Seek to find the end "@@" marker. */
		start = i + 2;
		for (end = start + 2; end < sz - 1; end++)
			if ('@' == buf[end] && '@' == buf[end + 1])
				break;

		/* Continue printing if not found of 0-length. */
		if (end == sz - 1 || end == start) {
			if ( ! fp(&buf[i], 1, arg))
				return(0);
			continue;
		}

		/* Look for a matching key. */
		for (j = 0; j < t->keysz; j++) {
			len = strlen(t->key[j]);
			if (len != end - start)
				continue;
			else if (memcmp(&buf[start], t->key[j], len))
				continue;
			if ( ! (*t->cb)(j, t->arg)) {
				XWARNX("template error");
				return(0);
			}
			break;
		}

		/* Didn't find it... */
		if (j == t->keysz) {
			if ( ! fp(&buf[i], 1, arg))
				return(0);
		} else
			i = end + 1;
	}

	if (i < sz && ! fp(&buf[i], 1, arg))
		return(0);

	return(1);
}

int
khttp_template(struct kreq *req, 
	const struct ktemplate *t, const char *fname)
{

	assert(KSTATE_BODY == req->kdata->state);
	return(khttp_templatex(t, fname, khttp_templatex_write, req));
}

/*
 * There are all sorts of ways to make this faster and more efficient.
 * For now, do it the easily-auditable way.
 * Memory-map the given file and look through it character by character
 * til we get to the key delimiter "@@".
 * Once there, scan to the matching "@@".
 * Look for the matching key within these pairs.
 * If found, invoke the callback function with the given key.
 */
int
khttp_templatex(const struct ktemplate *t, 
	const char *fname, ktemplate_writef fp, void *arg)
{
	struct stat 	 st;
	char		*buf;
	size_t		 sz;
	int		 fd, rc;

	if (-1 == (fd = open(fname, O_RDONLY, 0))) {
		XWARN("open: %s", fname);
		return(0);
	} else if (-1 == fstat(fd, &st)) {
		XWARN("fstat: %s", fname);
		close(fd);
		return(0);
	} else if (st.st_size >= (1U << 31)) {
		XWARNX("size overflow: %s", fname);
		close(fd);
		return(0);
	} else if (0 == st.st_size) {
		XWARNX("zero-length: %s", fname);
		close(fd);
		return(1);
	}

	sz = (size_t)st.st_size;
	buf = mmap(NULL, sz, PROT_READ, MAP_SHARED, fd, 0);

	if (MAP_FAILED == buf) {
		XWARN("mmap: %s", fname);
		close(fd);
		return(0);
	}

	rc = khttp_templatex_buf(t, buf, sz, fp, arg);
	munmap(buf, sz);
	close(fd);
	return(rc);
}
