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

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <math.h> /* HUGE_VAL */
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <zlib.h>

#include "kcgi.h"

enum	kstate {
	KSTATE_HEAD = 0,
	KSTATE_BODY
};

struct	kdata {
	enum kelem	 elems[128];
	size_t		 elemsz;
	enum kstate	 state;
#ifdef	HAVE_ZLIB
	gzFile		 gz;
#endif
	int		 newln;
};

struct	mimemap {
	const char	*name;
	enum kmime	 mime;
};

/*
 * For handling HTTP multipart forms.
 * This consists of data for a single multipart form entry.
 */
struct	mime {
	char	 *disp; /* content disposition */
	char	 *name; /* name of form entry */
	size_t	  namesz; /* size of "name" string */
	char	 *file; /* whether a file was specified */
	char	 *ctype; /* content type */
	char	 *xcode; /* encoding type */
	char	 *bound; /* form entry boundary */
};

enum	htype {
	TAG_FLOW, /* flow (block) element */
	TAG_PHRASE,/* phrasing (inline) element */
	TAG_VOID, /* auto-closing (e.g., INPUT) */
	TAG_INSTRUCTION /* instruction */
};

/*
 * A tag describes an HTML element.
 */
struct	tag {
	enum htype	 flags; 
	const char	*name; /* name of element */
};

static	const uint16_t entities[KENTITY__MAX] = {
	38, /* KENTITY_AMP */
	0xE9, /* KENTITY_EACUTE */
	0x3E, /* KENTITY_GT */
	0x2190, /* KENTITY_LARR */
	0x3C, /* KENTITY_LT */
	0x2014, /* KENTITY_MDASH */
	0x2013, /* KENTITY_NDASH */
	0x2192, /* KENTITY_RARR */
};

static	const struct tag tags[KELEM__MAX] = {
	{ TAG_PHRASE, "a" }, /* KELEM_A */ /* XXX: TRANS */
	{ TAG_PHRASE, "abbr" }, /* KELEM_ABBR */
	{ TAG_PHRASE, "address" }, /* KELEM_ADDRESS */
	{ TAG_VOID, "area" }, /* KELEM_AREA */
	{ TAG_FLOW, "article" }, /* KELEM_ARTICLE */
	{ TAG_FLOW, "aside" }, /* KELEM_ASIDE */
	{ TAG_FLOW, "audio" }, /* KELEM_AUDIO */ /* XXX: TRANS */
	{ TAG_PHRASE, "b" }, /* KELEM_B */
	{ TAG_VOID, "base" }, /* KELEM_BASE */
	{ TAG_PHRASE, "bdi" }, /* KELEM_BDI */
	{ TAG_PHRASE, "bdo" }, /* KELEM_BDO */
	{ TAG_FLOW, "blockquote" }, /* KELEM_BLOCKQUOTE */
	{ TAG_FLOW, "body" }, /* KELEM_BODY */
	{ TAG_VOID, "br" }, /* KELEM_BR */
	{ TAG_PHRASE, "button" }, /* KELEM_BUTTON */
	{ TAG_FLOW, "canvas" }, /* KELEM_CANVAS */ /* XXX: TRANS */
	{ TAG_FLOW, "caption" }, /* KELEM_CAPTION */
	{ TAG_PHRASE, "cite" }, /* KELEM_CITE */
	{ TAG_PHRASE, "code" }, /* KELEM_CODE */
	{ TAG_VOID, "col" }, /* KELEM_COL */
	{ TAG_PHRASE, "colgroup" }, /* KELEM_COLGROUP */
	{ TAG_PHRASE, "datalist" }, /* KELEM_DATALIST */
	{ TAG_FLOW, "dd" }, /* KELEM_DD */
	{ TAG_PHRASE, "del" }, /* KELEM_DEL */ /* XXX: TRANS */
	{ TAG_FLOW, "details" }, /* KELEM_DETAILS */
	{ TAG_PHRASE, "dfn" }, /* KELEM_DFN */
	{ TAG_FLOW, "div" }, /* KELEM_DIV */
	{ TAG_FLOW, "dl" }, /* KELEM_DL */
	{ TAG_INSTRUCTION, "!DOCTYPE html" }, /* KELEM_DOCTYPE */
	{ TAG_FLOW, "dt" }, /* KELEM_DT */
	{ TAG_PHRASE, "em" }, /* KELEM_EM */
	{ TAG_VOID, "embed" }, /* KELEM_EMBED */
	{ TAG_FLOW, "fieldset" }, /* KELEM_FIELDSET */
	{ TAG_FLOW, "figcaption" }, /* KELEM_FIGCAPTION */
	{ TAG_FLOW, "figure" }, /* KELEM_FIGURE */
	{ TAG_FLOW, "footer" }, /* KELEM_FOOTER */
	{ TAG_FLOW, "form" }, /* KELEM_FORM */
	{ TAG_PHRASE, "h1" }, /* KELEM_H1 */
	{ TAG_PHRASE, "h2" }, /* KELEM_H2 */
	{ TAG_PHRASE, "h3" }, /* KELEM_H3 */
	{ TAG_PHRASE, "h4" }, /* KELEM_H4 */
	{ TAG_PHRASE, "h5" }, /* KELEM_H5 */
	{ TAG_PHRASE, "h6" }, /* KELEM_H6 */
	{ TAG_FLOW, "head" }, /* KELEM_HEAD */
	{ TAG_FLOW, "header" }, /* KELEM_HEADER */
	{ TAG_FLOW, "hgroup" }, /* KELEM_HGROUP */
	{ TAG_VOID, "hr" }, /* KELEM_HR */
	{ TAG_FLOW, "html" }, /* KELEM_HTML */
	{ TAG_PHRASE, "i" }, /* KELEM_I */
	{ TAG_PHRASE, "iframe" }, /* KELEM_IFRAME */
	{ TAG_VOID, "img" }, /* KELEM_IMG */
	{ TAG_VOID, "input" }, /* KELEM_INPUT */
	{ TAG_PHRASE, "ins" }, /* KELEM_INS */ /* XXX: TRANS */
	{ TAG_PHRASE, "kbd" }, /* KELEM_KBD */
	{ TAG_VOID, "keygen" }, /* KELEM_KEYGEN */
	{ TAG_PHRASE, "label" }, /* KELEM_LABEL */
	{ TAG_PHRASE, "legend" }, /* KELEM_LEGEND */
	{ TAG_FLOW, "li" }, /* KELEM_LI */
	{ TAG_VOID, "link" }, /* KELEM_LINK */
	{ TAG_FLOW, "map" }, /* KELEM_MAP */ /* XXX: TRANS */
	{ TAG_PHRASE, "mark" }, /* KELEM_MARK */
	{ TAG_FLOW, "menu" }, /* KELEM_MENU */
	{ TAG_VOID, "meta" }, /* KELEM_META */
	{ TAG_PHRASE, "meter" }, /* KELEM_METER */
	{ TAG_FLOW, "nav" }, /* KELEM_NAV */
	{ TAG_FLOW, "noscript" }, /* KELEM_NOSCRIPT */ /* XXX: TRANS */
	{ TAG_FLOW, "object" }, /* KELEM_OBJECT */ /* XXX: TRANS */
	{ TAG_FLOW, "ol" }, /* KELEM_OL */
	{ TAG_FLOW, "optgroup" }, /* KELEM_OPTGROUP */
	{ TAG_PHRASE, "option" }, /* KELEM_OPTION */
	{ TAG_PHRASE, "output" }, /* KELEM_OUTPUT */
	{ TAG_PHRASE, "p" }, /* KELEM_P */
	{ TAG_VOID, "param" }, /* KELEM_PARAM */
	{ TAG_PHRASE, "pre" }, /* KELEM_PRE */
	{ TAG_PHRASE, "progress" }, /* KELEM_PROGRESS */
	{ TAG_PHRASE, "q" }, /* KELEM_Q */
	{ TAG_PHRASE, "rp" }, /* KELEM_RP */
	{ TAG_PHRASE, "rt" }, /* KELEM_RT */
	{ TAG_PHRASE, "ruby" }, /* KELEM_RUBY */
	{ TAG_PHRASE, "s" }, /* KELEM_S */
	{ TAG_PHRASE, "samp" }, /* KELEM_SAMP */
	{ TAG_FLOW, "script" }, /* KELEM_SCRIPT */
	{ TAG_FLOW, "section" }, /* KELEM_SECTION */
	{ TAG_FLOW, "select" }, /* KELEM_SELECT */
	{ TAG_PHRASE, "small" }, /* KELEM_SMALL */
	{ TAG_VOID, "source" }, /* KELEM_SOURCE */
	{ TAG_PHRASE, "span" }, /* KELEM_SPAN */
	{ TAG_PHRASE, "strong" }, /* KELEM_STRONG */
	{ TAG_FLOW, "style" }, /* KELEM_STYLE */
	{ TAG_PHRASE, "sub" }, /* KELEM_SUB */
	{ TAG_PHRASE, "summary" }, /* KELEM_SUMMARY */
	{ TAG_PHRASE, "sup" }, /* KELEM_SUP */
	{ TAG_FLOW, "table" }, /* KELEM_TABLE */
	{ TAG_FLOW, "tbody" }, /* KELEM_TBODY */
	{ TAG_FLOW, "td" }, /* KELEM_TD */
	{ TAG_FLOW, "textarea" }, /* KELEM_TEXTAREA */
	{ TAG_FLOW, "tfoot" }, /* KELEM_TFOOT */
	{ TAG_FLOW, "th" }, /* KELEM_TH */
	{ TAG_FLOW, "thead" }, /* KELEM_THEAD */
	{ TAG_PHRASE, "time" }, /* KELEM_TIME */
	{ TAG_PHRASE, "title" }, /* KELEM_TITLE */
	{ TAG_FLOW, "tr" }, /* KELEM_TR */
	{ TAG_VOID, "track" }, /* KELEM_TRACK */
	{ TAG_PHRASE, "u" }, /* KELEM_U */
	{ TAG_FLOW, "ul" }, /* KELEM_UL */
	{ TAG_PHRASE, "var" }, /* KELEM_VAR */
	{ TAG_FLOW, "video" }, /* KELEM_VIDEO */ /* XXX: TRANS */
	{ TAG_VOID, "wbr" }, /* KELEM_WBR */
};

const char *const kmimetypes[KMIME__MAX] = {
	"text/html", /* KMIME_HTML */
	"text/csv", /* KMIME_CSV */
	"image/png", /* KMIME_PNG */
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
	"500 Internal Server Error",
	"501 Not Implemented",
	"502 Bad Gateway",
	"503 Service Unavailable",
	"504 Gateway Timeout",
	"505 HTTP Version Not Supported",
	"511 Network Authentication Required",
};

static 	const struct mimemap suffixmap[] = {
	{ "html", KMIME_HTML },
	{ "htm", KMIME_HTML },
	{ "csv", KMIME_CSV },
	{ "png", KMIME_PNG },
	{ NULL, KMIME__MAX },
};

/*
 * Default MIME suffix per type.
 */
const char *const ksuffixes[KMIME__MAX] = {
	"html", /* KMIME_HTML */
	"csv", /* KMIME_CSV */
	"png", /* KIME_PNG */
};

static	const char *const attrs[KATTR__MAX] = {
	"accept-charset", /* KATTR_ACCEPT_CHARSET */
	"accesskey", /* KATTR_ACCESSKEY */
	"action", /* KATTR_ACTION */
	"alt", /* KATTR_ALT */
	"async", /* KATTR_ASYNC */
	"autocomplete", /* KATTR_AUTOCOMPLETE */
	"autofocus", /* KATTR_AUTOFOCUS */
	"autoplay", /* KATTR_AUTOPLAY */
	"border", /* KATTR_BORDER */
	"challenge", /* KATTR_CHALLENGE */
	"charset", /* KATTR_CHARSET */
	"checked", /* KATTR_CHECKED */
	"cite", /* KATTR_CITE */
	"class", /* KATTR_CLASS */
	"cols", /* KATTR_COLS */
	"colspan", /* KATTR_COLSPAN */
	"content", /* KATTR_CONTENT */
	"contenteditable", /* KATTR_CONTENTEDITABLE */
	"contextmenu", /* KATTR_CONTEXTMENU */
	"controls", /* KATTR_CONTROLS */
	"coords", /* KATTR_COORDS */
	"datetime", /* KATTR_DATETIME */
	"default", /* KATTR_DEFAULT */
	"defer", /* KATTR_DEFER */
	"dir", /* KATTR_DIR */
	"dirname", /* KATTR_DIRNAME */
	"disabled", /* KATTR_DISABLED */
	"draggable", /* KATTR_DRAGGABLE */
	"dropzone", /* KATTR_DROPZONE */
	"enctype", /* KATTR_ENCTYPE */
	"for", /* KATTR_FOR */
	"form", /* KATTR_FORM */
	"formaction", /* KATTR_FORMACTION */
	"formenctype", /* KATTR_FORMENCTYPE */
	"formmethod", /* KATTR_FORMMETHOD */
	"formnovalidate", /* KATTR_FORMNOVALIDATE */
	"formtarget", /* KATTR_FORMTARGET */
	"header", /* KATTR_HEADER */
	"height", /* KATTR_HEIGHT */
	"hidden", /* KATTR_HIDDEN */
	"high", /* KATTR_HIGH */
	"href", /* KATTR_HREF */
	"hreflang", /* KATTR_HREFLANG */
	"http-equiv", /* KATTR_HTTP_EQUIV */
	"icon", /* KATTR_ICON */
	"id", /* KATTR_ID */
	"ismap", /* KATTR_ISMAP */
	"keytype", /* KATTR_KEYTYPE */
	"kind", /* KATTR_KIND */
	"label", /* KATTR_LABEL */
	"lang", /* KATTR_LANG */
	"language", /* KATTR_LANGUAGE */
	"list", /* KATTR_LIST */
	"loop", /* KATTR_LOOP */
	"low", /* KATTR_LOW */
	"manifest", /* KATTR_MANIFEST */
	"max", /* KATTR_MAX */
	"maxlength", /* KATTR_MAXLENGTH */
	"media", /* KATTR_MEDIA */
	"mediagroup", /* KATTR_MEDIAGROUP */
	"method", /* KATTR_METHOD */
	"min", /* KATTR_MIN */
	"multiple", /* KATTR_MULTIPLE */
	"muted", /* KATTR_MUTED */
	"name", /* KATTR_NAME */
	"novalidate", /* KATTR_NOVALIDATE */
	"onabort", /* KATTR_ONABORT */
	"onafterprint", /* KATTR_ONAFTERPRINT */
	"onbeforeprint", /* KATTR_ONBEFOREPRINT */
	"onbeforeunload", /* KATTR_ONBEFOREUNLOAD */
	"onblur", /* KATTR_ONBLUR */
	"oncanplay", /* KATTR_ONCANPLAY */
	"oncanplaythrough", /* KATTR_ONCANPLAYTHROUGH */
	"onchange", /* KATTR_ONCHANGE */
	"onclick", /* KATTR_ONCLICK */
	"oncontextmenu", /* KATTR_ONCONTEXTMENU */
	"ondblclick", /* KATTR_ONDBLCLICK */
	"ondrag", /* KATTR_ONDRAG */
	"ondragend", /* KATTR_ONDRAGEND */
	"ondragenter", /* KATTR_ONDRAGENTER */
	"ondragleave", /* KATTR_ONDRAGLEAVE */
	"ondragover", /* KATTR_ONDRAGOVER */
	"ondragstart", /* KATTR_ONDRAGSTART */
	"ondrop", /* KATTR_ONDROP */
	"ondurationchange", /* KATTR_ONDURATIONCHANGE */
	"onemptied", /* KATTR_ONEMPTIED */
	"onended", /* KATTR_ONENDED */
	"onerror", /* KATTR_ONERROR */
	"onfocus", /* KATTR_ONFOCUS */
	"onhashchange", /* KATTR_ONHASHCHANGE */
	"oninput", /* KATTR_ONINPUT */
	"oninvalid", /* KATTR_ONINVALID */
	"onkeydown", /* KATTR_ONKEYDOWN */
	"onkeypress", /* KATTR_ONKEYPRESS */
	"onkeyup", /* KATTR_ONKEYUP */
	"onload", /* KATTR_ONLOAD */
	"onloadeddata", /* KATTR_ONLOADEDDATA */
	"onloadedmetadata", /* KATTR_ONLOADEDMETADATA */
	"onloadstart", /* KATTR_ONLOADSTART */
	"onmessage", /* KATTR_ONMESSAGE */
	"onmousedown", /* KATTR_ONMOUSEDOWN */
	"onmousemove", /* KATTR_ONMOUSEMOVE */
	"onmouseout", /* KATTR_ONMOUSEOUT */
	"onmouseover", /* KATTR_ONMOUSEOVER */
	"onmouseup", /* KATTR_ONMOUSEUP */
	"onmousewheel", /* KATTR_ONMOUSEWHEEL */
	"onoffline", /* KATTR_ONOFFLINE */
	"ononline", /* KATTR_ONONLINE */
	"onpagehide", /* KATTR_ONPAGEHIDE */
	"onpageshow", /* KATTR_ONPAGESHOW */
	"onpause", /* KATTR_ONPAUSE */
	"onplay", /* KATTR_ONPLAY */
	"onplaying", /* KATTR_ONPLAYING */
	"onpopstate", /* KATTR_ONPOPSTATE */
	"onprogress", /* KATTR_ONPROGRESS */
	"onratechange", /* KATTR_ONRATECHANGE */
	"onreadystatechange", /* KATTR_ONREADYSTATECHANGE */
	"onreset", /* KATTR_ONRESET */
	"onresize", /* KATTR_ONRESIZE */
	"onscroll", /* KATTR_ONSCROLL */
	"onseeked", /* KATTR_ONSEEKED */
	"onseeking", /* KATTR_ONSEEKING */
	"onselect", /* KATTR_ONSELECT */
	"onshow", /* KATTR_ONSHOW */
	"onstalled", /* KATTR_ONSTALLED */
	"onstorage", /* KATTR_ONSTORAGE */
	"onsubmit", /* KATTR_ONSUBMIT */
	"onsuspend", /* KATTR_ONSUSPEND */
	"ontimeupdate", /* KATTR_ONTIMEUPDATE */
	"onunload", /* KATTR_ONUNLOAD */
	"onvolumechange", /* KATTR_ONVOLUMECHANGE */
	"onwaiting", /* KATTR_ONWAITING */
	"open", /* KATTR_OPEN */
	"optimum", /* KATTR_OPTIMUM */
	"pattern", /* KATTR_PATTERN */
	"placeholder", /* KATTR_PLACEHOLDER */
	"poster", /* KATTR_POSTER */
	"preload", /* KATTR_PRELOAD */
	"radiogroup", /* KATTR_RADIOGROUP */
	"readonly", /* KATTR_READONLY */
	"rel", /* KATTR_REL */
	"required", /* KATTR_REQUIRED */
	"reversed", /* KATTR_REVERSED */
	"rows", /* KATTR_ROWS */
	"rowspan", /* KATTR_ROWSPAN */
	"sandbox", /* KATTR_SANDBOX */
	"scope", /* KATTR_SCOPE */
	"seamless", /* KATTR_SEAMLESS */
	"selected", /* KATTR_SELECTED */
	"shape", /* KATTR_SHAPE */
	"size", /* KATTR_SIZE */
	"sizes", /* KATTR_SIZES */
	"span", /* KATTR_SPAN */
	"spellcheck", /* KATTR_SPELLCHECK */
	"src", /* KATTR_SRC */
	"srcdoc", /* KATTR_SRCDOC */
	"srclang", /* KATTR_SRCLANG */
	"start", /* KATTR_START */
	"step", /* KATTR_STEP */
	"style", /* KATTR_STYLE */
	"tabindex", /* KATTR_TABINDEX */
	"target", /* KATTR_TARGET */
	"title", /* KATTR_TITLE */
	"translate", /* KATTR_TRANSLATE */
	"type", /* KATTR_TYPE */
	"usemap", /* KATTR_USEMAP */
	"value", /* KATTR_VALUE */
	"width", /* KATTR_WIDTH */
	"wrap", /* KATTR_WRAP */
};

/* 
 * Name of executing CGI script.
 */
const char	*pname = NULL;

/*
 * Wrapper for printing data to the standard output.
 * This switches depending on compression support and whether our system
 * supports zlib compression at all.
 */
#ifdef HAVE_ZLIB
#define	KPRINTF(_req, ...) \
	do if (NULL != (_req)->kdata->gz) \
		gzprintf((_req)->kdata->gz, __VA_ARGS__); \
	else \
		printf(__VA_ARGS__); \
	while (0)
#else
#define	KPRINTF(_req, ...) printf(__VA_ARGS__)
#endif

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

/* 
 * Safe strdup(): don't return on memory failure.
 */
char *
kstrdup(const char *cp)
{
	char	*p;

	if (NULL != (p = strdup(cp)))
		return(p);

	perror(NULL);
	exit(EXIT_FAILURE);
}

/*
 * Safe realloc(): don't return on memory failure.
 */
void *
kxrealloc(void *pp, size_t sz)
{
	char	*p;

	assert(sz > 0);
	if (NULL != (p = realloc(pp, sz)))
		return(p);

	perror(NULL);
	exit(EXIT_FAILURE);
}

/*
 * Safe realloc() with overflow-checking.
 */
void *
krealloc(void *pp, size_t nm, size_t sz)
{
	char	*p;

	if (nm && sz && SIZE_MAX / nm < sz) {
		errno = ENOMEM;
		perror(NULL);
		exit(EXIT_FAILURE);
	}

	assert(nm * sz > 0);
	if (NULL != (p = realloc(pp, nm * sz)))
		return(p);

	perror(NULL);
	exit(EXIT_FAILURE);
}

void *
kasprintf(const char *fmt, ...)
{
	char	*p;
	va_list	 ap;

	va_start(ap, fmt);
	vasprintf(&p, fmt, ap);
	va_end(ap);

	if (NULL != p)
		return(p);

	perror(NULL);
	exit(EXIT_FAILURE);
}

/*
 * Safe calloc(): don't return on exhaustion.
 */
void *
kcalloc(size_t nm, size_t sz)
{
	char	*p;

	if (NULL != (p = calloc(nm, sz)))
		return(p);

	perror(NULL);
	exit(EXIT_FAILURE);
}

/*
 * Safe malloc(): don't return on exhaustion.
 */
void *
kmalloc(size_t sz)
{
	char	*p;

	if (NULL != (p = malloc(sz)))
		return(p);

	perror(NULL);
	exit(EXIT_FAILURE);
}

static void
kpair_expand(struct kpair **kv, size_t *kvsz)
{

	*kv = krealloc(*kv, *kvsz + 1, sizeof(struct kpair));
	memset(&(*kv)[*kvsz], 0, sizeof(struct kpair));
	(*kvsz)++;
}

size_t
khtml_elemat(struct kreq *req)
{

	assert(KSTATE_BODY == req->kdata->state);
	return(req->kdata->elemsz);
}

void
khtml_elem(struct kreq *req, enum kelem elem)
{

	assert(KSTATE_BODY == req->kdata->state);
	khtml_attr(req, elem, KATTR__MAX);
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
		errno = ENOMEM;
		perror(NULL);
		exit(EXIT_FAILURE);
	}
	p = kcalloc(sz, 3);
	sz *= 3;

	for ( ; '\0' != (ch = *cp); cp++) {
		/* Put in a temporary buffer then concatenate. */
		memset(buf, 0, sizeof(buf));
		if (' ' == ch) 
			buf[0] = '+';
		else if (isalnum((int)ch))
			buf[0] = ch;
		else
			(void)snprintf(buf, sizeof(buf), "%%%.2x", ch);
		(void)strlcat(p, buf, sz);
	}

	return(p);
}

char *
kutil_urlpart(struct kreq *req, enum kmime mime, size_t page, ...)
{
	va_list		 ap;
	char		*p, *pp, *keyp, *valp;
	size_t		 total, count;

	pp = kutil_urlencode(req->pages[page]);
	p = kasprintf("%s/%s.%s", pname, pp, ksuffixes[mime]);
	free(pp);
	total = strlen(p) + 1;

	va_start(ap, page);
	count = 0;
	while (NULL != (pp = va_arg(ap, char *))) {
		keyp = kutil_urlencode(pp);
		valp = kutil_urlencode(va_arg(ap, char *));
		/* Size for key, value, ? or &, and =. */
		/* FIXME: check for overflow! */
		total += strlen(keyp) + strlen(valp) + 2;
		p = kxrealloc(p, total);

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

/*
 * Open a tag.
 * If we're a flow tag, emit a newline (unless already omitted). 
 * Then if we're at a newline regardless of tag type, indent properly to
 * the point where we'll omit the tag name.
 */
static void
khtml_flow_open(struct kreq *req, enum kelem elem)
{
	size_t		 i;

	if (TAG_FLOW == tags[elem].flags)
		if ( ! req->kdata->newln) {
			khttp_putc(req, '\n');
			req->kdata->newln = 1;
		}

	if (req->kdata->newln)
		for (i = 0; i < req->kdata->elemsz; i++) 
			khttp_puts(req, "  ");

	req->kdata->newln = 0;
}

/*
 * If we're closing a flow or instruction tag, emit a newline.
 * Otherwise do nothing.
 */
static void
khtml_flow_close(struct kreq *req, enum kelem elem)
{

	if (TAG_FLOW == tags[elem].flags ||
		TAG_INSTRUCTION == tags[elem].flags) {
		khttp_putc(req, '\n');
		req->kdata->newln = 1;
	} else
		req->kdata->newln = 0;
}

void
khtml_attrx(struct kreq *req, enum kelem elem, ...)
{
	va_list		 ap;
	enum kattr	 at;
	struct kdata	*k = req->kdata;

	assert(KSTATE_BODY == req->kdata->state);
	khtml_flow_open(req, elem);
	KPRINTF(req, "<%s", tags[elem].name);

	va_start(ap, elem);
	while (KATTR__MAX != (at = va_arg(ap, enum kattr))) {
		khttp_putc(req, ' ');
		khttp_puts(req, attrs[at]);
		khttp_putc(req, '=');
		khttp_putc(req, '"');
		switch (va_arg(ap, enum kattrx)) {
		case (KATTRX_STRING):
			khtml_text(req, va_arg(ap, char *));
			break;
		case (KATTRX_INT):
			khtml_int(req, va_arg(ap, int64_t));
			break;
		case (KATTRX_DOUBLE):
			khtml_double(req, va_arg(ap, double));
		}
		khttp_putc(req, '"');
	}
	va_end(ap);

	if (TAG_VOID == tags[elem].flags)
		khttp_putc(req, '/');
	khttp_putc(req, '>');
	khtml_flow_close(req, elem);

	if (TAG_VOID != tags[elem].flags &&
		TAG_INSTRUCTION != tags[elem].flags)
		k->elems[k->elemsz++] = elem;
	assert(k->elemsz < 128);
}

void
khtml_attr(struct kreq *req, enum kelem elem, ...)
{
	va_list		 ap;
	enum kattr	 at;
	struct kdata	*k = req->kdata;
	const char	*cp;

	assert(KSTATE_BODY == req->kdata->state);
	khtml_flow_open(req, elem);
	KPRINTF(req, "<%s", tags[elem].name);

	va_start(ap, elem);
	while (KATTR__MAX != (at = va_arg(ap, enum kattr))) {
		cp = va_arg(ap, char *);
		assert(NULL != cp);
		khttp_putc(req, ' ');
		khttp_puts(req, attrs[at]);
		khttp_putc(req, '=');
		khttp_putc(req, '"');
		khtml_text(req, cp);
		khttp_putc(req, '"');

	}
	va_end(ap);

	if (TAG_VOID == tags[elem].flags)
		khttp_putc(req, '/');
	khttp_putc(req, '>');
	khtml_flow_close(req, elem);

	if (TAG_VOID != tags[elem].flags &&
		TAG_INSTRUCTION != tags[elem].flags)
		k->elems[k->elemsz++] = elem;
	assert(k->elemsz < 128);
}

void
khtml_close(struct kreq *req, size_t sz)
{
	size_t		 i;
	struct kdata	*k = req->kdata;

	assert(KSTATE_BODY == req->kdata->state);

	if (0 == sz)
		sz = k->elemsz;

	for (i = 0; i < sz; i++) {
		assert(k->elemsz > 0);
		k->elemsz--;
		khtml_flow_open(req, k->elems[k->elemsz]);
		KPRINTF(req, "</%s>", tags[k->elems[k->elemsz]].name);
		khtml_flow_close(req, k->elems[k->elemsz]);
	}
}

void
khtml_closeto(struct kreq *req, size_t pos)
{

	assert(KSTATE_BODY == req->kdata->state);
	assert(pos < req->kdata->elemsz);
	khtml_close(req, req->kdata->elemsz - pos);
}

/*
 * In-place HTTP-decode a string.  The standard explanation is that this
 * turns "%4e+foo" into "n foo" in the regular way.  This is done
 * in-place over the allocated string.
 */
static int
urldecode(char *p)
{
	char             hex[3];
	unsigned int	 c;

	hex[2] = '\0';

	for ( ; '\0' != *p; p++) {
		if ('%' == *p) {
			if ('\0' == (hex[0] = *(p + 1)))
				return(0);
			if ('\0' == (hex[1] = *(p + 2)))
				return(0);
			if (1 != sscanf(hex, "%x", &c))
				return(0);
			if ('\0' == c)
				return(0);

			*p = (char)c;
			memmove(p + 1, p + 3, strlen(p + 3) + 1);
		} else
			*p = /* LINTED */
				'+' == *p ? ' ' : *p;
	}

	*p = '\0';
	return(1);
}

static void
parse_pairs_text(struct kreq *req, char *p)
{
	char            *key, *val;

	while (NULL != p && '\0' != *p) {
		/* Skip leading whitespace. */
		while (' ' == *p)
			p++;

		key = p;
		val = NULL;
		if (NULL != (p = strchr(p, '='))) {
			/* Key/value pair. */
			*p++ = '\0';
			val = p;
			p = strstr(val, "\r\n");
			if (NULL != p) {
				*p = '\0';
				p += 2;
			}
		} else {
			/* No value--error. */
			p = strstr(key, "\r\n");
			if (NULL != p) {
				*p = '\0';
				p += 2;
			}
			continue;
		}

		if ('\0' == *key || '\0' == *val)
			continue;
		kpair_expand(&req->fields, &req->fieldsz);
		req->fields[req->fieldsz - 1].key = kstrdup(key);
		req->fields[req->fieldsz - 1].val = kstrdup(val);
		req->fields[req->fieldsz - 1].valsz = strlen(val);
	}
}


/*
 * Parse out key-value pairs from an HTTP request variable.
 * This can be either a cookie or a POST/GET string.
 * This MUST be a non-binary (i.e., nil-terminated) string!
 */
static void
parse_pairs_urlenc(struct kpair **kv, size_t *kvsz, char *p)
{
	char            *key, *val;
	size_t           sz;

	while (NULL != p && '\0' != *p) {
		/* Skip leading whitespace. */
		while (' ' == *p)
			p++;

		key = p;
		val = NULL;
		if (NULL != (p = strchr(p, '='))) {
			/* Key/value pair. */
			*p++ = '\0';
			val = p;
			sz = strcspn(p, ";&");
			p += sz;
			if ('\0' != *p)
				*p++ = '\0';
		} else {
			/* No value--error. */
			p = key;
			sz = strcspn(p, ";&");
			p += sz;
			if ('\0' != *p)
				p++;
			continue;
		}

		if ('\0' == *key || '\0' == *val)
			continue;
		if ( ! urldecode(key))
			break;
		if ( ! urldecode(val))
			break;
		kpair_expand(kv, kvsz);
		(*kv)[*kvsz - 1].key = kstrdup(key);
		(*kv)[*kvsz - 1].val = kstrdup(val);
		(*kv)[*kvsz - 1].valsz = strlen(val);
	}
}

/*
 * Read full stdin request into memory.
 * This reads at most "len" bytes and nil-terminates the results, the
 * length of which may be less than "len" and is stored in *szp if not
 * NULL.
 * Returns the pointer to the data.
 */
static char *
scanbuf(size_t len, size_t *szp)
{
	ssize_t		 ssz;
	size_t		 sz;
	char		*p;

	/* Allocate the entire buffer here. */
	p = kmalloc(len + 1);

	/* Keep reading til we get all the data. */
	/* FIXME: use poll() to avoid blocking. */
	for (sz = 0; sz < len; sz += (size_t)ssz) {
		ssz = read(STDIN_FILENO, p + sz, len - sz);
		if (ssz < 0) {
			perror(NULL);
			exit(EXIT_FAILURE);
		} else if (0 == ssz)
			break;
	}

	/* ALWAYS nil-terminate. */
	p[sz] = '\0';
	if (NULL != szp)
		*szp = sz;
	return(p);
}

static void
parse_text(size_t len, struct kreq *req)
{
	char		*p;

	p = scanbuf(len, NULL);
	parse_pairs_text(req, p);
	free(p);
}

static void
parse_urlenc(size_t len, struct kreq *req)
{
	char		*p;

	p = scanbuf(len, NULL);
	parse_pairs_urlenc(&req->fields, &req->fieldsz, p);
	free(p);
}

/*
 * Reset a particular mime component.
 * We can get duplicates, so reallocate.
 */
static void
mime_reset(char **dst, const char *src)
{

	free(*dst);
	*dst = kstrdup(src);
}

/*
 * Parse out all MIME headers.
 * This is defined by RFC 2045.
 * This returns TRUE if we've parsed up to (and including) the last
 * empty CRLF line, or FALSE if something has gone wrong.
 * If FALSE, parsing should stop immediately.
 */
static int
mime_parse(struct mime *mime, char *buf, size_t len, size_t *pos)
{
	char		*key, *val, *end, *start, *line;

	memset(mime, 0, sizeof(struct mime));

	while (*pos < len) {
		/* Each MIME line ends with a CRLF. */
		start = &buf[*pos];
		end = memmem(start, len - *pos, "\r\n", 2);
		if (NULL == end)
			return(0);
		/* Nil-terminate to make a nice line. */
		*end = '\0';
		/* Re-set our starting position. */
		*pos += (end - start) + 2;

		/* Empty line: we're done here! */
		if ('\0' == *start)
			return(1);

		/* Find end of MIME statement name. */
		key = start;
		if (NULL == (val = strchr(key, ':')))
			return(0);
		*val++ = '\0';
		while (' ' == *val)
			val++;
		line = NULL;
		if (NULL != (line = strchr(val, ';')))
			*line++ = '\0';

		/* 
		 * Allow these specific MIME header statements.
		 * Ignore all others.
		 */
		if (0 == strcasecmp(key, "content-transfer-encoding"))
			mime_reset(&mime->xcode, val);
		else if (0 == strcasecmp(key, "content-disposition"))
			mime_reset(&mime->disp, val);
		else if (0 == strcasecmp(key, "content-type"))
			mime_reset(&mime->ctype, val);
		else
			continue;

		/* Now process any familiar MIME components. */
		while (NULL != (key = line)) {
			while (' ' == *key)
				key++;
			if ('\0' == *key)
				break;
			if (NULL == (val = strchr(key, '=')))
				return(0);
			*val++ = '\0';

			if ('"' == *val) {
				val++;
				if (NULL == (line = strchr(val, '"')))
					return(0);
				*line++ = '\0';
				if (';' == *line)
					line++;
			} else if (NULL != (line = strchr(val, ';')))
				*line++ = '\0';

			/* White-listed sub-commands. */
			if (0 == strcasecmp(key, "filename"))
				mime_reset(&mime->file, val);
			else if (0 == strcasecmp(key, "name"))
				mime_reset(&mime->name, val);
			else if (0 == strcasecmp(key, "boundary"))
				mime_reset(&mime->bound, val);
			else
				continue;
		}
	} 

	return(0);
}

static void
mime_free(struct mime *mime)
{

	free(mime->disp);
	free(mime->name);
	free(mime->file);
	free(mime->ctype);
	free(mime->xcode);
	free(mime->bound);
	memset(mime, 0, sizeof(struct mime));
}

/*
 * This is described by the "multipart-body" BNF part of RFC 2046,
 * section 5.1.1.
 * We return TRUE if the parse was ok, FALSE if errors occured (all
 * calling parsers should bail too).
 */
static int
parse_multiform(struct kreq *req, const char *name,
	const char *bound, char *buf, size_t len, size_t *pos)
{
	struct mime	 mime;
	size_t		 endpos, bbsz, partsz;
	char		*ln, *bb;
	int		 rc, first;

	rc = 0;
	/* Define our buffer boundary. */
	bb = kasprintf("\r\n--%s", bound);
	bbsz = strlen(bb);
	memset(&mime, 0, sizeof(struct mime));

	/* Read to the next instance of a buffer boundary. */
	for (first = 1; *pos < len; first = 0, *pos = endpos) {
		/*
		 * The conditional is because the prologue, if not
		 * specified, will not incur an initial CRLF, so our bb
		 * is past the CRLF and two bytes smaller.
		 * This ONLY occurs for the first read, however.
		 */
		ln = memmem(&buf[*pos], len - *pos, 
			bb + (first ? 2 : 0), bbsz - (first ? 2 : 0));

		/* Unexpected EOF for this part. */
		if (NULL == ln)
			goto out;

		/* 
		 * Set "endpos" to point to the beginning of the next
		 * multipart component.
		 * We set "endpos" to be at the very end if the
		 * terminating boundary buffer occurs.
		 */
		endpos = *pos + (ln - &buf[*pos]) + bbsz - (first ? 2 : 0);
		/* Check buffer space... */
		if (endpos > len - 2)
			goto out;

		/* Terminating boundary or not... */
		if (memcmp(&buf[endpos], "--", 2)) {
			while (endpos < len && ' ' == buf[endpos])
				endpos++;
			/* We need the CRLF... */
			if (memcmp(&buf[endpos], "\r\n", 2))
				goto out;
			endpos += 2;
		} else
			endpos = len;

		/* 
		 * Zero-length part. 
		 * This shouldn't occur, but if it does, it'll screw up
		 * the MIME parsing (which requires a blank CRLF before
		 * considering itself finished).
		 */
		if (0 == (partsz = ln - &buf[*pos]))
			continue;

		/* We now read our MIME headers, bailing on error. */
		mime_free(&mime);
		if ( ! mime_parse(&mime, buf, *pos + partsz, pos))
			goto out;
		/* 
		 * As per RFC 2388, we need a name and disposition. 
		 * Note that multipart/mixed bodies will inherit the
		 * name of their parent, so the mime.name is ignored.
		 */
		if (NULL == mime.name && NULL == name)
			continue;
		else if (NULL == mime.disp) 
			continue;
		/* As per RFC 2045, we default to text/plain. */
		if (NULL == mime.ctype) 
			mime.ctype = kstrdup("text/plain");

		partsz = ln - &buf[*pos];

		/* 
		 * Multipart sub-handler. 
		 * We only recognise the multipart/mixed handler.
		 * This will route into our own function, inheriting the
		 * current name for content.
		 */
		if (0 == strcasecmp(mime.ctype, "multipart/mixed")) {
			if (NULL == mime.bound)
				goto out;
			if ( ! parse_multiform
				(req, NULL != name ? name :
				 mime.name, mime.bound,
				 buf, *pos + partsz, pos))
				goto out;
			continue;
		}

		/* Assign all of our key-value pair data. */
		kpair_expand(&req->fields, &req->fieldsz);
		req->fields[req->fieldsz - 1].key = 
			kstrdup(NULL != name ? name : mime.name);
		req->fields[req->fieldsz - 1].val = kmalloc(partsz + 1);
		memcpy(req->fields[req->fieldsz - 1].val, &buf[*pos], partsz);
		req->fields[req->fieldsz - 1].val[partsz] = '\0';
		req->fields[req->fieldsz - 1].valsz = partsz;
		if (NULL != mime.file)
			req->fields[req->fieldsz - 1].file = 
				kstrdup(mime.file);
		if (NULL != mime.ctype)
			req->fields[req->fieldsz - 1].ctype = 
				kstrdup(mime.ctype);
		if (NULL != mime.xcode)
			req->fields[req->fieldsz - 1].xcode = 
				kstrdup(mime.xcode);
	}

	/*
	 * According to the specification, we can have transport
	 * padding, a CRLF, then the epilogue.
	 * But since we don't care about that crap, just pretend that
	 * everything's fine and exit.
	 */
	rc = 1;
out:
	free(bb);
	mime_free(&mime);
	return(rc);
}

/*
 * Parse the boundary from a multipart CONTENT_TYPE and pass it to the
 * actual parsing engine.
 * This doesn't actually handle any part of the MIME specification.
 */
static void
parse_multi(struct kreq *req, char *line, size_t len)
{
	char		*cp;
	size_t		 sz;

	while (' ' == *line)
		line++;
	if (';' != *line++)
		return;
	while (' ' == *line)
		line++;

	/* We absolutely need the boundary marker. */
	if (strncmp(line, "boundary", 8)) 
		return;
	line += 8;
	while (' ' == *line)
		line++;
	if ('=' != *line++)
		return;
	while (' ' == *line)
		line++;

	/* Make sure the line is terminated in the right place .*/
	if ('"' == *line) {
		if (NULL == (cp = strchr(++line, '"')))
			return;
		*cp = '\0';
	} else {
		/*
		 * XXX: this may not properly follow RFC 2046, 5.1.1,
		 * which specifically lays out the boundary characters.
		 * We simply jump to the first whitespace.
		 */
		for (cp = line; '\0' != *cp && ' ' != *cp; cp++)
			/* Spin. */ ;
		*cp = '\0';
	}

	/* Read in full file. */
	cp = scanbuf(len, &sz);
	len = 0;
	parse_multiform(req, NULL, line, cp, sz, &len);
	free(cp);
}

/*
 * Parse a request from an HTTP request.
 * This consists of paths, suffixes, methods, and most importantly,
 * pasred query string, cookie, and form data.
 */
void
khttp_parse(struct kreq *req, 
	const struct kvalid *keys, size_t keysz,
	const char *const *pages, size_t pagesz,
	size_t defpage)
{
	char		*cp, *ep, *sub;
	enum kmime	 m;
	const struct mimemap *mm;
	size_t		 p, i, j, len;

	if (NULL == (pname = getenv("SCRIPT_NAME")))
		pname = "";

	memset(req, 0, sizeof(struct kreq));

	req->keys = keys;
	req->keysz = keysz;
	req->pages = pages;
	req->pagesz = pagesz;
	req->kdata = kcalloc(1, sizeof(struct kdata));
	req->cookiemap = kcalloc(keysz, sizeof(struct kpair *));
	req->cookienmap = kcalloc(keysz, sizeof(struct kpair *));
	req->fieldmap = kcalloc(keysz, sizeof(struct kpair *));
	req->fieldnmap = kcalloc(keysz, sizeof(struct kpair *));

	/* Determine authenticaiton: RFC 3875, 4.1.1. */
	if (NULL != (cp = getenv("AUTH_TYPE"))) {
		if (0 == strcasecmp(cp, "basic"))
			req->auth = KAUTH_BASIC;
		else if (0 == strcasecmp(cp, "digest"))
			req->auth = KAUTH_DIGEST;
		else
			req->auth = KAUTH_UNKNOWN;
	}

	sub = NULL;
	p = defpage;
	m = KMIME_HTML;

	/* RFC 3875, 4.1.8. */
	/* Never supposed to be NULL, but to be sure... */
	if (NULL == (cp = getenv("REMOTE_ADDR")))
		req->remote = kstrdup("127.0.0.1");
	else
		req->remote = kstrdup(cp);

	/* RFC 3875, 4.1.12. */
	/* Note that we assume GET, POST being explicit. */
	req->method = KMETHOD_GET;
	if (NULL != (cp = getenv("REQUEST_METHOD")))
		if (0 == strcmp(cp, "POST"))
			req->method = KMETHOD_POST;

	/*
	 * First, parse the first path element (the page we want to
	 * access), subsequent path information, and the file suffix.
	 * We convert suffix and path element into the respective enum's
	 * inline.
	 */
	if (NULL != (cp = getenv("PATH_INFO")))
		req->fullpath = kstrdup(cp);

	/* This isn't possible in the real world. */
	if (NULL != cp && '/' == *cp)
		cp++;

	if (NULL != cp && '\0' != *cp) {
		ep = cp + strlen(cp) - 1;
		/* Look up mime type from suffix. */
		while (ep > cp && '/' != *ep && '.' != *ep)
			ep--;
		if ('.' == *ep) {
			*ep++ = '\0';
			req->suffix = kstrdup(ep);
			for (mm = suffixmap; NULL != mm->name; mm++)
				if (0 == strcasecmp(mm->name, ep)) {
					m = mm->mime;
					break;
				}
			if (NULL == mm)
				m = KMIME__MAX;
		}
		if (NULL != (sub = strchr(cp, '/')))
			*sub++ = '\0';
		/* Look up page type from component. */
		for (p = 0; p < pagesz; p++)
			if (0 == strcasecmp(pages[p], cp))
				break;
	}

	req->mime = m;
	req->page = p;

	/* Assign subpath to remaining parts. */
	if (NULL != sub)
		req->path = kstrdup(sub);

	/*
	 * The CONTENT_LENGTH must be a valid integer.
	 * Since we're storing into "len", make sure it's in size_t.
	 * If there's an error, it will default to zero.
	 * Note that LLONG_MAX < SIZE_MAX.
	 * RFC 3875, 4.1.2.
	 */
	len = 0;
	if (NULL != (cp = getenv("CONTENT_LENGTH")))
		len = strtonum(cp, 0, LLONG_MAX, NULL);

	/*
	 * If a CONTENT_TYPE has been specified (i.e., POST or GET has
	 * been set -- we don't care which), then switch on that type
	 * for parsing out key value pairs.
	 * RFC 3875, 4.1.3.
	 * HTML5, 4.10.
	 * We only support the main three content types.
	 */
	if (NULL != (cp = getenv("CONTENT_TYPE"))) {
		if (0 == strcasecmp(cp, "application/x-www-form-urlencoded"))
			parse_urlenc(len, req);
		else if (0 == strncasecmp(cp, "multipart/form-data", 19)) 
			parse_multi(req, cp + 19, len);
		else if (0 == strcasecmp(cp, "text/plain"))
			parse_text(len, req);
	} else
		parse_text(len, req);

	/*
	 * Even POST requests are allowed to have QUERY_STRING elements,
	 * so parse those out now.
	 * Note: both QUERY_STRING and CONTENT_TYPE fields share the
	 * same field space.
	 * Since this is a getenv(), we know the returned value is
	 * nil-terminated.
	 */
	if (NULL != (cp = getenv("QUERY_STRING")))
		parse_pairs_urlenc(&req->fields, &req->fieldsz, cp);

	/*
	 * Cookies come last.
	 * These use the same syntax as QUERY_STRING elements, but don't
	 * share the same namespace (just as a means to differentiate
	 * the same names).
	 * Since this is a getenv(), we know the returned value is
	 * nil-terminated.
	 */
	if (NULL != (cp = getenv("HTTP_COOKIE")))
		parse_pairs_urlenc(&req->cookies, &req->cookiesz, cp);

	/*
	 * Run through all fields and sort them into named buckets.
	 * This will let us do constant-time lookups within the
	 * application itself.  Niiiice.
	 */
	for (i = 0; i < keysz; i++) {
		for (j = 0; j < req->fieldsz; j++) {
			if (strcmp(req->fields[j].key, keys[i].name))
				continue;
			if (NULL != keys[i].valid &&
				! (*keys[i].valid)(&req->fields[j])) {
				req->fields[j].type = KPAIR__MAX;
				req->fields[j].next = req->fieldnmap[i];
				req->fieldnmap[i] = &req->fields[j];
				continue;
			}
			assert(NULL == keys[i].valid ||
				KPAIR__MAX != req->fields[j].type);
			req->fields[j].next = req->fieldmap[i];
			req->fieldmap[i] = &req->fields[j];
		}
		for (j = 0; j < req->cookiesz; j++) {
			if (strcmp(req->cookies[j].key, keys[i].name))
				continue;
			if (NULL != keys[i].valid &&
				! keys[i].valid(&req->cookies[j])) {
				req->cookies[j].type = KPAIR__MAX;
				req->cookies[j].next = req->cookienmap[i];
				req->cookienmap[i] = &req->cookies[j];
				continue;
			}
			assert(NULL == keys[i].valid ||
				KPAIR__MAX != req->cookies[j].type);
			req->cookies[j].next = req->cookiemap[i];
			req->cookiemap[i] = &req->cookies[j];
		}
	}
}

/*
 * This entire function would be two lines if we were using the
 * sys/queue macros.h.
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

static void
kpair_free(struct kpair *p, size_t sz)
{
	size_t		 i;

	for (i = 0; i < sz; i++) {
		free(p[i].key);
		free(p[i].val);
		free(p[i].file);
		free(p[i].ctype);
	}
	free(p);
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
	kpair_free(req->cookies, req->cookiesz);
	kpair_free(req->fields, req->fieldsz);
	free(req->path);
	free(req->fullpath);
	free(req->remote);
	free(req->cookiemap);
	free(req->cookienmap);
	free(req->fieldmap);
	free(req->fieldnmap);
	free(req->kdata);
	free(req->suffix);
}

void
khtml_entity(struct kreq *req, enum kentity entity)
{

	assert(entity < KENTITY__MAX);
	assert(KSTATE_BODY == req->kdata->state);
	khtml_ncr(req, entities[entity]);
}

void
khtml_ncr(struct kreq *req, uint16_t ncr)
{

	assert(KSTATE_BODY == req->kdata->state);
	KPRINTF(req, "&#x%" PRIx16 ";", ncr);
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
	 */
#ifdef HAVE_ZLIB
	if (NULL != (cp = getenv("HTTP_ACCEPT_ENCODING")) &&
			NULL != strstr(cp, "gzip")) {
		req->kdata->gz = gzdopen(STDOUT_FILENO, "w");
		if (NULL == req->kdata->gz) {
			perror(NULL);
			exit(EXIT_FAILURE);
		}
		khttp_head(req, "Content-Encoding", "%s", "gzip");
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

void
khtml_double(struct kreq *req, double val)
{
	char	 buf[256];

	(void)snprintf(buf, sizeof(buf), "%g", val);
	khtml_text(req, buf);
}

void
khtml_int(struct kreq *req, int64_t val)
{
	char	 buf[22];

	(void)snprintf(buf, sizeof(buf), "%" PRId64, val);
	khtml_text(req, buf);
}

/*
 * Emit text in an HTML document.
 * This means, minimally, that we need to escape the open and close
 * delimiters for HTML tags.
 */
void
khtml_text(struct kreq *req, const char *cp)
{

	/* TODO: speed up with strcspn. */
	assert(KSTATE_BODY == req->kdata->state);
	req->kdata->newln = 0;
	for ( ; NULL != cp && '\0' != *cp; cp++)
		switch (*cp) {
		case ('>'):
			khtml_entity(req, KENTITY_GT);
			break;
		case ('&'):
			khtml_entity(req, KENTITY_AMP);
			break;
		case ('<'):
			khtml_entity(req, KENTITY_LT);
			break;
		default:
			khttp_putc(req, *cp);
			break;
		}
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

	if ( ! kvalid_string(p))
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

	if ( ! kvalid_string(p))
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

	if ( ! kvalid_string(p))
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

	p->parsed.i = strtonum(trim(p->val), 1, INT64_MAX, &ep);
	p->type = KPAIR_INTEGER;
	return(NULL == ep);
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
khtml_template(struct kreq *req, 
	const struct ktemplate *t, const char *fname)
{
	struct stat 	 st;
	char		*buf;
	size_t		 sz, i, j, len, start, end;
	int		 fd, rc;

	assert(KSTATE_BODY == req->kdata->state);

	if (-1 == (fd = open(fname, O_RDONLY, 0)))
		return(0);

	if (-1 == fstat(fd, &st)) {
		close(fd);
		return(0);
	} else if (st.st_size >= (1U << 31)) {
		close(fd);
		return(0);
	} else if (0 == st.st_size) {
		close(fd);
		return(1);
	}


	sz = (size_t)st.st_size;
	buf = mmap(NULL, sz, PROT_READ, MAP_SHARED, fd, 0);

	if (MAP_FAILED == buf) {
		close(fd);
		return(0);
	}

	rc = 0;

	for (i = 0; i < sz - 1; i++) {
		/* Look for the starting "@@" marker. */
		if ('@' != buf[i]) {
			khttp_putc(req, buf[i]);
			continue;
		} else if ('@' != buf[i + 1]) {
			khttp_putc(req, buf[i]);
			continue;
		} 

		/* Seek to find the end "@@" marker. */
		start = i + 2;
		for (end = start + 2; end < sz - 1; end++)
			if ('@' == buf[end] && '@' == buf[end + 1])
				break;

		/* Continue printing if not found of 0-length. */
		if (end == sz - 1 || end == start) {
			khttp_putc(req, buf[i]);
			continue;
		}

		/* Look for a matching key. */
		for (j = 0; j < t->keysz; j++) {
			len = strlen(t->key[j]);
			if (len != end - start)
				continue;
			else if (memcmp(&buf[start], t->key[j], len))
				continue;
			if ( ! (*t->cb)(j, t->arg))
				goto out;
			break;
		}

		/* Didn't find it... */
		if (j == t->keysz)
			khttp_putc(req, buf[i]);
		else
			i = end + 1;
	}

	if (i < sz)
		khttp_putc(req, buf[i]);

	rc = 1;
out:
	munmap(buf, sz);
	close(fd);
	return(rc);
}
