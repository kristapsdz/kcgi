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
	"100 Continue",				/* KHTTP_100 */
	"101 Switching Protocols",              /* KHTTP_101 */
	"103 Checkpoint",                       /* KHTTP_103 */
	"200 OK",                               /* KHTTP_200 */
	"201 Created",                          /* KHTTP_201 */
	"202 Accepted",                         /* KHTTP_202 */
	"203 Non-Authoritative Information",    /* KHTTP_203 */
	"204 No Content",                       /* KHTTP_204 */
	"205 Reset Content",                    /* KHTTP_205 */
	"206 Partial Content",                  /* KHTTP_206 */
	"207 Multi-Status",                     /* KHTTP_207 */
	"300 Multiple Choices",                 /* KHTTP_300 */
	"301 Moved Permanently",                /* KHTTP_301 */
	"302 Found",                            /* KHTTP_302 */
	"303 See Other",                        /* KHTTP_303 */
	"304 Not Modified",                     /* KHTTP_304 */
	"306 Switch Proxy",                     /* KHTTP_306 */
	"307 Temporary Redirect",               /* KHTTP_307 */
	"308 Resume Incomplete",                /* KHTTP_308 */
	"400 Bad Request",                      /* KHTTP_400 */
	"401 Unauthorized",                     /* KHTTP_401 */
	"402 Payment Required",                 /* KHTTP_402 */
	"403 Forbidden",                        /* KHTTP_403 */
	"404 Not Found",                        /* KHTTP_404 */
	"405 Method Not Allowed",               /* KHTTP_405 */
	"406 Not Acceptable",                   /* KHTTP_406 */
	"407 Proxy Authentication Required",    /* KHTTP_407 */
	"408 Request Timeout",                  /* KHTTP_408 */
	"409 Conflict",                         /* KHTTP_409 */
	"410 Gone",                             /* KHTTP_410 */
	"411 Length Required",                  /* KHTTP_411 */
	"412 Precondition Failed",              /* KHTTP_412 */
	"413 Request Entity Too Large",         /* KHTTP_413 */
	"414 Request-URI Too Long",             /* KHTTP_414 */
	"415 Unsupported Media Type",           /* KHTTP_415 */
	"416 Requested Range Not Satisfiable",  /* KHTTP_416 */
	"417 Expectation Failed",               /* KHTTP_417 */
	"424 Failed Dependency",                /* KHTTP_424 */
	"428 Precondition Required",            /* KHTTP_428 */
	"429 Too Many Requests",                /* KHTTP_429 */
	"431 Request Header Fields Too Large",  /* KHTTP_431 */
	"500 Internal Server Error",            /* KHTTP_500 */
	"501 Not Implemented",                  /* KHTTP_501 */
	"502 Bad Gateway",                      /* KHTTP_502 */
	"503 Service Unavailable",              /* KHTTP_503 */
	"504 Gateway Timeout",                  /* KHTTP_504 */
	"505 HTTP Version Not Supported",       /* KHTTP_505 */
	"507 Insufficient Storage",             /* KHTTP_507 */
	"511 Network Authentication Required",  /* KHTTP_511 */
};

/*
 * This doesn't have a preset size.
 */
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
	"js",	/* KMIME_APP_JAVASCRIPT */
	"json", /* KMIME_APP_JSON */
	NULL,	/* KMIME_APP_OCTET_STREAM */
	"pdf",	/* KMIME_APP_PDF */
	"xml",	/* KMIME_APP_XML */
	"zip",	/* KMIME_APP_ZIP */
	"gif",	/* KMIME_IMAGE_GIF */
	"jpg",	/* KMIME_IMAGE_JPEG */
	"png",	/* KMIME_IMAGE_PNG */
	"svg",	/* KMIME_IMAGE_PNG */
	"ics",	/* KMIME_TEXT_CALENDAR */
	"css",	/* KMIME_TEXT_CSS */
	"csv",	/* KMIME_TEXT_CSV */
	"html",	/* KMIME_TEXT_HTML */
	"txt",	/* KMIME_TEXT_PLAIN */
	"xml",	/* KMIME_TEXT_XML */
};

const char *const kerrors[] = {
	"success", 			/* KCGI_OK */
	"cannot allocate memory",	/* KCGI_ENOMEM */
	"FastCGI exit",			/* KCGI_EXIT */
	"end-point connection closed",	/* KCGI_HUP */
	"too many open sockets",	/* KCGI_ENFILE */
	"failed to fork child",		/* KCGI_EAGAIN */
	"internal error",		/* KCGI_FORM */
	"system error",			/* KCGI_SYSTEM */
};

const char *
kcgi_strerror(enum kcgi_err er)
{

	assert(er <= KCGI_SYSTEM);
	return kerrors[er];
}

/*
 * Safe strdup(): don't return on exhaustion.
 */
char *
kstrdup(const char *cp)
{
	char	*p;

	if ((p = XSTRDUP(cp)) != NULL)
		return p;
	exit(EXIT_FAILURE);
}

/*
 * Safe realloc(): don't return on exhaustion.
 */
void *
krealloc(void *pp, size_t sz)
{
	char	*p;

	if ((p = XREALLOC(pp, sz)) != NULL)
		return p;
	exit(EXIT_FAILURE);
}

/*
 * Safe reallocarray(): don't return on exhaustion.
 */
void *
kreallocarray(void *pp, size_t nm, size_t sz)
{
	char	*p;

	if ((p = XREALLOCARRAY(pp, nm, sz)) != NULL)
		return p;
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
		return len;
	exit(EXIT_FAILURE);
}

/*
 * Safe vasprintf(): don't return on exhaustion.
 */
int
kvasprintf(char **p, const char *fmt, va_list ap)
{
	int	 len;

	if ((len = XVASPRINTF(p, fmt, ap)) >= 0)
		return len;
	exit(EXIT_FAILURE);
}

/*
 * Safe calloc(): don't return on exhaustion.
 */
void *
kcalloc(size_t nm, size_t sz)
{
	char	*p;

	if ((p = XCALLOC(nm, sz)) != NULL)
		return p;
	exit(EXIT_FAILURE);
}

/*
 * Safe malloc(): don't return on exhaustion.
 */
void *
kmalloc(size_t sz)
{
	char	*p;

	if ((p = XMALLOC(sz)) != NULL)
		return p;
	exit(EXIT_FAILURE);
}

char *
kutil_urlencode(const char *cp)
{
	char	*p;
	char	 ch;
	size_t	 sz, cur;

	if (cp == NULL)
		return NULL;

	/* 
	 * Leave three bytes per input byte for encoding. 
	 * This ensures we needn't range-check.
	 * First check whether our size overflows. 
	 * We do this here because we need our size!
	 */

	sz = strlen(cp) + 1;
	if (SIZE_MAX / 3 < sz) {
		XWARNX("multiplicative overflow: %zu", sz);
		return NULL;
	}
	if ((p = XCALLOC(sz, 3)) == NULL)
		return NULL;

	for (cur = 0; (ch = *cp) != '\0'; cp++) {
		if (isalnum((unsigned char)ch) || ch == '-' || 
		    ch == '_' || ch == '.' || ch == '~') {
			p[cur++] = ch;
			continue;
		} else if (' ' == ch) {
			p[cur++] = '+';
			continue;
		}
		cur += snprintf(p + cur, 4, "%%%.2hhX", 
			(unsigned char)ch);
	}

	return p;
}

enum kcgi_err
kutil_urldecode_inplace(char *p)
{
	char	 	 c, d;
	const char	*tail;

	if (p == NULL)
		return KCGI_FORM;

	/*
	 * Keep track of two positions: "p", where we'll write the
	 * decoded results, and "tail", which is from where we'll
	 * decode hex or copy data.
	 */

	for (tail = p; (c = *tail) != '\0'; *p++ = c) {
		if (c != '%') {
			if (c == '+')
				c = ' ';
			tail++;
			continue;
		}

		/* 
		 * Read hex '%xy' as two unsigned chars "c" and "d" then
		 * combine them back into "c".
		 */

		if (sscanf(tail + 1, "%1hhx%1hhx", &d, &c) != 2 ||
		    (c |= d << 4) == '\0') {
			XWARNX("urldecode: bad hex");
			return KCGI_FORM;
		}
		tail += 3;
	}

	*p = '\0';
	return KCGI_OK;
}

enum kcgi_err
kutil_urldecode(const char *src, char **dst)
{
	enum kcgi_err	 er;

	*dst = NULL;

	if (src == NULL)
		return KCGI_FORM;
	if ((*dst = XSTRDUP(src)) == NULL)
		return KCGI_ENOMEM;
	if ((er = kutil_urldecode_inplace(*dst)) == KCGI_OK)
		return KCGI_OK;

	/* If we have decoding errors, clear the output. */

	free(*dst);
	*dst = NULL;
	return er;
}

char *
kutil_urlabs(enum kscheme scheme, 
	const char *host, uint16_t port, const char *path)
{
	char	*p;

	XASPRINTF(&p, "%s://%s:%" PRIu16 "%s", 
	    kschemes[scheme], host, port, path);
	return p;
}

char *
kutil_urlpartx(struct kreq *req, const char *path,
	const char *mime, const char *page, ...)
{
	va_list		 ap;
	int		 rc;
	char		*p, *pp, *keyp, *valp, *valpp;
	size_t		 total, count = 0;
	char	 	 buf[256]; /* max double/int64_t */

	if ((pp = kutil_urlencode(page)) == NULL)
		return NULL;

	/* If we have a MIME type, append it. */

	rc = mime != NULL ?
		XASPRINTF(&p, "%s%s%s.%s", 
			NULL != path ? path : "",
			NULL != path ? "/" : "", pp, mime) :
		XASPRINTF(&p, "%s%s%s", 
			NULL != path ? path : "",
			NULL != path ? "/" : "", pp);

	free(pp);

	if (rc == -1)
		return NULL;

	total = strlen(p) + 1;

	va_start(ap, page);
	while ((pp = va_arg(ap, char *)) != NULL) {
		if ((keyp = kutil_urlencode(pp)) == NULL) {
			free(p);
			va_end(ap);
			return NULL;
		}

		valp = valpp = NULL;

		switch (va_arg(ap, enum kattrx)) {
		case KATTRX_STRING:
			valp = kutil_urlencode(va_arg(ap, char *));
			valpp = valp;
			break;
		case KATTRX_INT:
			(void)snprintf(buf, sizeof(buf),
				"%" PRId64, va_arg(ap, int64_t));
			valp = buf;
			break;
		case KATTRX_DOUBLE:
			(void)snprintf(buf, sizeof(buf),
				"%g", va_arg(ap, double));
			valp = buf;
			break;
		default:
			free(p);
			free(keyp);
			va_end(ap);
			return NULL;
		}

		if (valp == NULL) {
			free(p);
			free(keyp);
			va_end(ap);
			return NULL;
		}

		/* Size for key, value, ? or &, and =. */
		/* FIXME: check for overflow! */

		total += strlen(keyp) + strlen(valp) + 2;

		if ((pp = XREALLOC(p, total)) == NULL) {
			free(p);
			free(keyp);
			free(valpp);
			va_end(ap);
			return NULL;
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

	return p;
}

char *
kutil_urlpart(struct kreq *req, const char *path,
	const char *mime, const char *page, ...)
{
	va_list		 ap;
	char		*p, *pp, *keyp, *valp;
	size_t		 total, count = 0;
	int		 len;

	if ((pp = kutil_urlencode(page)) == NULL)
		return NULL;

	/* If we have a MIME type, append it. */

	len = mime != NULL ?
		XASPRINTF(&p, "%s%s%s.%s", 
			NULL != path ? path : "",
			NULL != path ? "/" : "", pp, mime) :
		XASPRINTF(&p, "%s%s%s", 
			NULL != path ? path : "",
			NULL != path ? "/" : "", pp);
	free(pp);

	if (len < 0)
		return NULL;

	total = strlen(p) + 1;

	va_start(ap, page);
	while (NULL != (pp = va_arg(ap, char *))) {
		if ((keyp = kutil_urlencode(pp)) == NULL) {
			free(p);
			va_end(ap);
			return NULL;
		}

		valp = kutil_urlencode(va_arg(ap, char *));
		if (valp == NULL) {
			free(p);
			free(keyp);
			va_end(ap);
			return NULL;
		}

		/* Size for key, value, ? or &, and =. */
		/* FIXME: check for overflow! */

		total += strlen(keyp) + strlen(valp) + 2;

		if ((pp = XREALLOC(p, total)) == NULL) {
			free(p);
			free(keyp);
			free(valp);
			va_end(ap);
			return NULL;
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

	return p;
}

static void
kpair_free(struct kpair *p, size_t sz)
{
	size_t	 i;

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
	if (req->rawauth.type == KAUTH_DIGEST) {
		free(req->rawauth.d.digest.user);
		free(req->rawauth.d.digest.uri);
		free(req->rawauth.d.digest.realm);
		free(req->rawauth.d.digest.nonce);
		free(req->rawauth.d.digest.cnonce);
		free(req->rawauth.d.digest.response);
		free(req->rawauth.d.digest.opaque);
	} else if (req->rawauth.type == KAUTH_BASIC) 
		free(req->rawauth.d.basic.response);
}

enum kcgi_err
khttp_parse(struct kreq *req, 
	const struct kvalid *keys, size_t keysz,
	const char *const *pages, size_t pagesz,
	size_t defpage)
{

	return khttp_parsex(req, ksuffixmap, kmimetypes, 
		KMIME__MAX, keys, keysz, pages, pagesz, 
		KMIME_TEXT_HTML, defpage, NULL, NULL, 0, NULL);
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
	int		  work_dat[2];
	pid_t		  work_pid;

	memset(req, 0, sizeof(struct kreq));

	/*
	 * We'll be using poll(2) for reading our HTTP document, so this
	 * must be non-blocking in order to make the reads not spin the
	 * CPU.
	 */

	if (kxsocketprep(STDIN_FILENO) != KCGI_OK) {
		XWARNX("kxsocketprep");
		return KCGI_SYSTEM;
	}

	if (kxsocketpair(AF_UNIX, SOCK_STREAM, 0, work_dat) != KCGI_OK)
		return KCGI_SYSTEM;

	if ((work_pid = fork()) == -1) {
		er = errno;
		XWARN("fork");
		close(work_dat[KWORKER_PARENT]);
		close(work_dat[KWORKER_CHILD]);
		return EAGAIN == er ? KCGI_EAGAIN : KCGI_ENOMEM;
	} else if (work_pid == 0) {
		if (argfree != NULL)
			(*argfree)(arg);
		close(STDOUT_FILENO);
		close(work_dat[KWORKER_PARENT]);
		er = EXIT_FAILURE;
		if (!ksandbox_init_child
			(SAND_WORKER,
			 work_dat[KWORKER_CHILD], -1, -1, -1)) {
			XWARNX("ksandbox_init_child");
		} else if (KCGI_OK != kworker_child
			   (work_dat[KWORKER_CHILD], keys,
			    keysz, mimes, mimesz, debugging)) {
			XWARNX("kworker_child");
		} else
			er = EXIT_SUCCESS;
		close(work_dat[KWORKER_CHILD]);
		_exit(er);
		/* NOTREACHED */
	}

	close(work_dat[KWORKER_CHILD]);
	work_dat[KWORKER_CHILD] = -1;

	if (opts == NULL)
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
	if (req->kdata == NULL)
		goto err;

	if (keysz) {
		req->cookiemap = XCALLOC(keysz, sizeof(struct kpair *));
		if (req->cookiemap == NULL)
			goto err;
		req->cookienmap = XCALLOC(keysz, sizeof(struct kpair *));
		if (req->cookienmap == NULL)
			goto err;
		req->fieldmap = XCALLOC(keysz, sizeof(struct kpair *));
		if (req->fieldmap == NULL)
			goto err;
		req->fieldnmap = XCALLOC(keysz, sizeof(struct kpair *));
		if (req->fieldnmap == NULL)
			goto err;
	}

	/*
	 * Now read the input fields from the child and conditionally
	 * assign them to our lookup table.
	 */

	kerr = kworker_parent(work_dat[KWORKER_PARENT], req, 1, mimesz);
	if (kerr != KCGI_OK)
		goto err;

	/* Look up page type from component. */

	req->page = defpage;
	if (*req->pagename != '\0')
		for (req->page = 0; req->page < pagesz; req->page++)
			if (strcasecmp
			    (pages[req->page], req->pagename) == 0)
				break;

	/*
	 * Look up the MIME type, defaulting to defmime if none.
	 * If we can't find it, use the maximum (mimesz).
	 */

	req->mime = defmime;
	if (*req->suffix != '\0') {
		for (mm = suffixmap; mm->name != NULL; mm++)
			if (strcasecmp(mm->name, req->suffix) == 0) {
				req->mime = mm->mime;
				break;
			}
		if (mm->name == NULL)
			req->mime = mimesz;
	}

	close(work_dat[KWORKER_PARENT]);
	work_dat[KWORKER_PARENT] = -1;
	kerr = kxwaitpid(work_pid);
	work_pid = -1;
	if (kerr != KCGI_OK)
		goto err;
	return kerr;
err:
	assert(kerr != KCGI_OK);
	if (work_dat[KWORKER_PARENT] != -1)
		close(work_dat[KWORKER_PARENT]);
	if (work_pid != -1)
		kxwaitpid(work_pid);
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

	if (kp == NULL)
		return;

	kp->type = KPAIR__MAX;
	kp->state = KPAIR_INVALID;
	memset(&kp->parsed, 0, sizeof(union parsed));

	/* We're not bucketed. */

	if ((i = kp->keypos) == r->keysz)
		return;

	/* Is it in our fieldmap? */

	if (r->fieldmap[i] != NULL) {
		if (kp == r->fieldmap[i]) {
			r->fieldmap[i] = kp->next;
			kp->next = r->fieldnmap[i];
			r->fieldnmap[i] = kp;
			return;
		} 
		lastp = r->fieldmap[i];
		p = lastp->next;
		for ( ; p != NULL; lastp = p, p = p->next)
			if (kp == p) {
				lastp->next = kp->next;
				kp->next = r->fieldnmap[i];
				r->fieldnmap[i] = kp;
				return;
			}
	}

	/* ...cookies? */

	if (r->cookiemap[i] != NULL) {
		if (kp == r->cookiemap[i]) {
			r->cookiemap[i] = kp->next;
			kp->next = r->cookienmap[i];
			r->cookienmap[i] = kp;
			return;
		} 
		lastp = r->cookiemap[i];
		p = lastp->next;
		for ( ; p != NULL; lastp = p, p = p->next) 
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
	char	*cp;

	while (isspace((unsigned char)*val))
		val++;

	cp = strchr(val, '\0') - 1;
	while (cp > val && isspace((unsigned char)*cp))
		*cp-- = '\0';

	return val;
}

/*
 * Simple email address validation: this is NOT according to the spec,
 * but a simple heuristic look at the address.
 * Note that this lowercases the mail address.
 */
static char *
valid_email(char *p)
{
	char	*cp, *start;
	size_t	 sz;

	/* 
	 * Trim all white-space before and after.
	 * Length check (min: a@b, max: 254 bytes).
	 * Make sure we have an at-sign.
	 * Make sure at signs aren't at the start or end.
	 */

	cp = start = trim(p);

	if ((sz = strlen(cp)) < 3 || sz > 254)
		return NULL;
	if (cp[0] == '@' || cp[sz - 1] == '@')
		return NULL;
	if (strchr(cp, '@') == NULL)
		return NULL;

	for (cp = start; *cp != '\0'; cp++)
		*cp = tolower((unsigned char)*cp);

	return start;
}

int
kvalid_date(struct kpair *kp)
{
	int		 mday, mon, year;

	if (kp->valsz != 10 || kp->val[10] != '\0' ||
	    !isdigit((unsigned char)kp->val[0]) ||
	    !isdigit((unsigned char)kp->val[1]) ||
	    !isdigit((unsigned char)kp->val[2]) ||
	    !isdigit((unsigned char)kp->val[3]) ||
	    kp->val[4] != '-' || 
	    !isdigit((unsigned char)kp->val[5]) ||
	    !isdigit((unsigned char)kp->val[6]) ||
	    kp->val[7] != '-' || 
	    !isdigit((unsigned char)kp->val[8]) ||
	    !isdigit((unsigned char)kp->val[9]))
		return 0;

	year = atoi(&kp->val[0]);
	mon = atoi(&kp->val[5]);
	mday = atoi(&kp->val[8]);

	if (!kutil_date_check(mday, mon, year))
		return 0;

	kp->parsed.i = kutil_date2epoch(mday, mon, year);
	kp->type = KPAIR_INTEGER;
	return 1;
}

int
kvalid_stringne(struct kpair *p)
{
	/*
	 * To check if we're a valid string, simply make sure that the
	 * NUL-terminator is where we expect it to be.
	 */

	if (strlen(p->val) != p->valsz || p->valsz == 0)
		return 0;
	p->type = KPAIR_STRING;
	p->parsed.s = p->val;
	return 1;
}

int
kvalid_string(struct kpair *p)
{
	/*
	 * To check if we're a valid string, simply make sure that the
	 * NUL-terminator is where we expect it to be.
	 */

	if (strlen(p->val) != p->valsz)
		return 0;
	p->type = KPAIR_STRING;
	p->parsed.s = p->val;
	return 1;
}

int
kvalid_email(struct kpair *p)
{

	if (!kvalid_stringne(p))
		return 0;
	return (p->parsed.s = valid_email(p->val)) != NULL;
}

int
kvalid_udouble(struct kpair *p)
{

	return kvalid_double(p) && p->parsed.d > 0.0;
}

int
kvalid_double(struct kpair *p)
{
	char		*ep;
	const char	*nval;
	double		 lval;
	int		 er;

	if (!kvalid_stringne(p))
		return 0;

	/* 
	 * We might get an empty string from trim, which constitutes a
	 * valid double (!?), so double check that the string is
	 * non-empty after trimming whitespace.
	 * We trim white-space because strtod(3) accepts white-space
	 * before but not after the string.
	 */

	nval = trim(p->val);
	if (nval[0] == '\0')
		return 0;

	/* Save errno so we can restore it later. */

	er = errno;
	errno = 0;
	lval = strtod(nval, &ep);
	if (errno == ERANGE)
		return 0;

	/* Restore errno. */

	errno = er;

	if (*ep != '\0')
		return 0;

	p->parsed.d = lval;
	p->type = KPAIR_DOUBLE;
	return 1;
}

int
kvalid_int(struct kpair *p)
{
	const char	*ep;

	if (!kvalid_stringne(p))
		return 0;
	p->parsed.i = strtonum
		(trim(p->val), INT64_MIN, INT64_MAX, &ep);
	p->type = KPAIR_INTEGER;
	return ep == NULL;
}

int
kvalid_bit(struct kpair *p)
{

	if (!kvalid_uint(p))
		return 0;
	return p->parsed.i <= 64;
}

int
kvalid_uint(struct kpair *p)
{
	const char	*ep;

	p->parsed.i = strtonum(trim(p->val), 0, INT64_MAX, &ep);
	p->type = KPAIR_INTEGER;
	return ep == NULL;
}

enum kcgi_err
kcgi_buf_write(const char *s, size_t sz, void *arg)
{
	struct kcgi_buf	*b = arg;
	void		*pp;

	/* Let empty/NULL pass through. */

	if (s == NULL || sz == 0)
		return KCGI_OK;

	/* Grow the buffer and leave some slop space. */

	if (b->sz + sz + 1 > b->maxsz) {
		b->maxsz = b->sz + sz + 1 + 
			(b->growsz == 0 ? 1024 : b->growsz);
		pp = XREALLOC(b->buf, b->maxsz);
		if (pp == NULL)
			return KCGI_ENOMEM;
		b->buf = pp;
	}
	
	/* Always NUL-terminate even though we accept binary data. */

	memcpy(&b->buf[b->sz], s, sz);
	b->sz += sz;
	b->buf[b->sz] = '\0';
	return KCGI_OK;
}

enum kcgi_err
kcgi_buf_printf(struct kcgi_buf *buf, const char *fmt, ...)
{
	char		*nbuf;
	int		 len;
	va_list		 ap;
	enum kcgi_err	 er;

	/* Let this bogus case pass through. */

	if (fmt == NULL)
		return KCGI_OK;

	/* Allocate temporary buffer. */

	va_start(ap, fmt);
	len = XVASPRINTF(&nbuf, fmt, ap);
	va_end(ap);
	if (len == -1)
		return KCGI_ENOMEM;

	/* Write and free. */

	er = kcgi_buf_write(nbuf, (size_t)len, buf);
	free(nbuf);
	return er;
}

enum kcgi_err
kcgi_buf_putc(struct kcgi_buf *buf, char c)
{

	return kcgi_buf_write(&c, 1, buf);
}

enum kcgi_err
kcgi_buf_puts(struct kcgi_buf *buf, const char *cp)
{

	if (cp == NULL)
		return KCGI_OK;
	return kcgi_buf_write(cp, strlen(cp), buf);
}
