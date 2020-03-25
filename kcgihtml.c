/*	$Id$ */
/*
 * Copyright (c) 2012, 2014, 2015, 2017 Kristaps Dzonsons <kristaps@bsd.lv>
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

#include <assert.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "kcgi.h"
#include "kcgihtml.h"
#include "extern.h"

/*
 * Maximum size of printing a signed 64-bit integer.
 */
#define	INT_MAXSZ	 22

/*
 * A type of HTML5 element.
 * Note: we don't list transitional elements, though I do note them in
 * the tag array.
 */
enum	htype {
	TAG_FLOW, /* flow (block) element */
	TAG_PHRASE,/* phrasing (inline) element */
	TAG_VOID, /* auto-closing (e.g., INPUT) */
	TAG_INSTRUCTION /* instruction */
};

/*
 * A tag describes an HTML element and its properties.
 */
struct	tag {
	enum htype	 flags; 
	const char	*name;
};

static	const uint16_t entities[KENTITY__MAX] = {
	198, /* KENTITY_AElig */
	193, /* KENTITY_Aacute */
	194, /* KENTITY_Acirc */
	192, /* KENTITY_Agrave */
	197, /* KENTITY_Aring */
	195, /* KENTITY_Atilde */
	196, /* KENTITY_Auml */
	199, /* KENTITY_Ccedil */
	8225, /* KENTITY_Dagger */
	208, /* KENTITY_ETH */
	201, /* KENTITY_Eacute */
	202, /* KENTITY_Ecirc */
	200, /* KENTITY_Egrave */
	203, /* KENTITY_Euml */
	205, /* KENTITY_Iacute */
	206, /* KENTITY_Icirc */
	204, /* KENTITY_Igrave */
	207, /* KENTITY_Iuml */
	209, /* KENTITY_Ntilde */
	338, /* KENTITY_OElig */
	211, /* KENTITY_Oacute */
	212, /* KENTITY_Ocirc */
	210, /* KENTITY_Ograve */
	216, /* KENTITY_Oslash */
	213, /* KENTITY_Otilde */
	214, /* KENTITY_Ouml */
	352, /* KENTITY_Scaron */
	222, /* KENTITY_THORN */
	218, /* KENTITY_Uacute */
	219, /* KENTITY_Ucirc */
	217, /* KENTITY_Ugrave */
	220, /* KENTITY_Uuml */
	221, /* KENTITY_Yacute */
	376, /* KENTITY_Yuml */
	225, /* KENTITY_aacute */
	226, /* KENTITY_acirc */
	180, /* KENTITY_acute */
	230, /* KENTITY_aelig */
	224, /* KENTITY_agrave */
	38, /* KENTITY_amp */
	39, /* KENTITY_apos */
	229, /* KENTITY_aring */
	227, /* KENTITY_atilde */
	228, /* KENTITY_auml */
	8222, /* KENTITY_bdquo */
	166, /* KENTITY_brvbar */
	231, /* KENTITY_ccedil */
	184, /* KENTITY_cedil */
	162, /* KENTITY_cent */
	710, /* KENTITY_circ */
	169, /* KENTITY_copy */
	164, /* KENTITY_curren */
	8224, /* KENTITY_dagger */
	176, /* KENTITY_deg */
	247, /* KENTITY_divide */
	233, /* KENTITY_eacute */
	234, /* KENTITY_ecirc */
	232, /* KENTITY_egrave */
	8195, /* KENTITY_emsp */
	8194, /* KENTITY_ensp */
	240, /* KENTITY_eth */
	235, /* KENTITY_euml */
	8364, /* KENTITY_euro */
	189, /* KENTITY_frac12 */
	188, /* KENTITY_frac14 */
	190, /* KENTITY_frac34 */
	62, /* KENTITY_gt */
	8230, /* KENTITY_hellip */
	237, /* KENTITY_iacute */
	238, /* KENTITY_icirc */
	161, /* KENTITY_iexcl */
	236, /* KENTITY_igrave */
	191, /* KENTITY_iquest */
	239, /* KENTITY_iuml */
	171, /* KENTITY_laquo */
	8220, /* KENTITY_ldquo */
	8206, /* KENTITY_lrm */
	8249, /* KENTITY_lsaquo */
	8216, /* KENTITY_lsquo */
	60, /* KENTITY_lt */
	175, /* KENTITY_macr */
	8212, /* KENTITY_mdash */
	181, /* KENTITY_micro */
	183, /* KENTITY_middot */
	160, /* KENTITY_nbsp */
	8211, /* KENTITY_ndash */
	172, /* KENTITY_not */
	241, /* KENTITY_ntilde */
	243, /* KENTITY_oacute */
	244, /* KENTITY_ocirc */
	339, /* KENTITY_oelig */
	242, /* KENTITY_ograve */
	170, /* KENTITY_ordf */
	186, /* KENTITY_ordm */
	248, /* KENTITY_oslash */
	245, /* KENTITY_otilde */
	246, /* KENTITY_ouml */
	182, /* KENTITY_para */
	8240, /* KENTITY_permil */
	177, /* KENTITY_plusmn */
	163, /* KENTITY_pound */
	34, /* KENTITY_quot */
	187, /* KENTITY_raquo */
	8221, /* KENTITY_rdquo */
	174, /* KENTITY_reg */
	8207, /* KENTITY_rlm */
	8250, /* KENTITY_rsaquo */
	8217, /* KENTITY_rsquo */
	8218, /* KENTITY_sbquo */
	353, /* KENTITY_scaron */
	167, /* KENTITY_sect */
	173, /* KENTITY_shy */
	185, /* KENTITY_sup1 */
	178, /* KENTITY_sup2 */
	179, /* KENTITY_sup3 */
	223, /* KENTITY_szlig */
	8201, /* KENTITY_thinsp */
	254, /* KENTITY_thorn */
	732, /* KENTITY_tilde */
	215, /* KENTITY_times */
	8482, /* KENTITY_trade */
	250, /* KENTITY_uacute */
	251, /* KENTITY_ucirc */
	249, /* KENTITY_ugrave */
	168, /* KENTITY_uml */
	252, /* KENTITY_uuml */
	253, /* KENTITY_yacute */
	165, /* KENTITY_yen */
	255, /* KENTITY_yuml */
	8205, /* KENTITY_zwj */
	8204, /* KENTITY_zwnj */
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
	{ TAG_PHRASE, "textarea" }, /* KELEM_TEXTAREA */
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

size_t
khtml_elemat(struct khtmlreq *req)
{

	return(req->elemsz);
}

enum kcgi_err
khtml_elem(struct khtmlreq *req, enum kelem elem)
{

	return(khtml_attr(req, elem, KATTR__MAX));
}

/*
 * Open a tag---only used when pretty-printing and a noop otherwise.
 * If we're a flow tag, emit a newline (unless already omitted). 
 * Then if we're at a newline regardless of tag type, indent properly to
 * the point where we'll omit the tag name.
 */
static enum kcgi_err
khtml_flow_open(struct khtmlreq *req, enum kelem elem)
{
	size_t		 i;
	enum kcgi_err	 er;

	if ( ! (KHTML_PRETTY & req->opts))
		return(KCGI_OK);

	if (TAG_FLOW == tags[elem].flags && ! req->newln) {
		if (KCGI_OK != (er = kcgi_writer_putc(req->arg, '\n')))
			return(er);
		req->newln = 1;
	}

	if (req->newln)
		for (i = 0; i < req->elemsz; i++) 
			if (KCGI_OK != (er = 
			    kcgi_writer_puts(req->arg, "  ")))
				return(er);

	req->newln = 0;
	return(KCGI_OK);
}

/*
 * If we're pretty-printing and closing a flow or instruction tag, emit
 * a newline.
 * Otherwise do nothing.
 * Returns the usual write error code.
 */
static enum kcgi_err
khtml_flow_close(struct khtmlreq *req, enum kelem elem)
{
	enum kcgi_err	 er;

	if ( ! (KHTML_PRETTY & req->opts))
		return(KCGI_OK);

	if (TAG_FLOW == tags[elem].flags ||
	    TAG_INSTRUCTION == tags[elem].flags) {
		if (KCGI_OK != (er = kcgi_writer_putc(req->arg, '\n')))
			return(er);
		req->newln = 1;
	} else
		req->newln = 0;

	return(KCGI_OK);
}

enum kcgi_err
khtml_attrx(struct khtmlreq *req, enum kelem elem, ...)
{
	va_list		 ap;
	enum kattr	 at;
	enum kcgi_err	 er;

	if (KCGI_OK != (er = khtml_flow_open(req, elem)))
		return(er);
	if (KCGI_OK != (er = kcgi_writer_putc(req->arg, '<')))
		return(er);
	if (KCGI_OK != (er = kcgi_writer_puts(req->arg, tags[elem].name)))
		return(er);

	va_start(ap, elem);
	while (KATTR__MAX != (at = va_arg(ap, enum kattr))) {
		if (KCGI_OK != (er = kcgi_writer_putc(req->arg, ' ')))
			return(er);
		if (KCGI_OK != (er = kcgi_writer_puts(req->arg, attrs[at])))
			return(er);
		if (KCGI_OK != (er = kcgi_writer_puts(req->arg, "=\"")))
			return(er);

		switch (va_arg(ap, enum kattrx)) {
		case (KATTRX_STRING):
			er = khtml_puts(req, va_arg(ap, char *));
			break;
		case (KATTRX_INT):
			er = khtml_int(req, va_arg(ap, int64_t));
			break;
		case (KATTRX_DOUBLE):
			er = khtml_double(req, va_arg(ap, double));
			break;
		}
		if (KCGI_OK != er)
			return(er);
		if (KCGI_OK != (er = kcgi_writer_putc(req->arg, '"')))
			return(er);
	}
	va_end(ap);

	if (TAG_VOID == tags[elem].flags)
		if (KCGI_OK != (er = kcgi_writer_putc(req->arg, '/')))
			return(er);
	if (KCGI_OK != (er = kcgi_writer_putc(req->arg, '>')))
		return(er);
	if (KCGI_OK != (er = khtml_flow_close(req, elem)))
		return(er);

	if (TAG_VOID != tags[elem].flags &&
	    TAG_INSTRUCTION != tags[elem].flags)
		req->elems[req->elemsz++] = elem;
	assert(req->elemsz < KDATA_MAXELEMSZ);

	return(KCGI_OK);
}

enum kcgi_err
khtml_attr(struct khtmlreq *req, enum kelem elem, ...)
{
	va_list		 ap;
	enum kattr	 at;
	const char	*cp;
	enum kcgi_err	 er;

	if (KCGI_OK != (er = khtml_flow_open(req, elem)))
		return(er);
	if (KCGI_OK != (er = kcgi_writer_putc(req->arg, '<')))
		return(er);
	if (KCGI_OK != (er = kcgi_writer_puts(req->arg, tags[elem].name)))
		return(er);

	va_start(ap, elem);
	while (KATTR__MAX != (at = va_arg(ap, enum kattr))) {
		cp = va_arg(ap, char *);
		assert(NULL != cp);
		if (KCGI_OK != (er = kcgi_writer_putc(req->arg, ' ')))
			return(er);
		if (KCGI_OK != (er = kcgi_writer_puts(req->arg, attrs[at])))
			return(er);
		if (KCGI_OK != (er = kcgi_writer_puts(req->arg, "=\"")))
			return(er);
		if (KCGI_OK != (er = khtml_puts(req, cp)))
			return(er);
		if (KCGI_OK != (er = kcgi_writer_putc(req->arg, '"')))
			return(er);

	}
	va_end(ap);

	if (TAG_VOID == tags[elem].flags)
		if (KCGI_OK != (er = kcgi_writer_putc(req->arg, '/')))
			return(er);
	if (KCGI_OK != (er = kcgi_writer_putc(req->arg, '>')))
		return(er);
	if (KCGI_OK != (er = khtml_flow_close(req, elem)))
		return(er);

	if (TAG_VOID != tags[elem].flags &&
	    TAG_INSTRUCTION != tags[elem].flags)
		req->elems[req->elemsz++] = elem;
	assert(req->elemsz < KDATA_MAXELEMSZ);

	return(KCGI_OK);
}

enum kcgi_err
khtml_closeelem(struct khtmlreq *req, size_t sz)
{
	size_t		 i;
	enum kcgi_err	 er;

	if (0 == sz)
		sz = req->elemsz;
	if (sz > req->elemsz)
		sz = req->elemsz;

	for (i = 0; i < sz; i++) {
		assert(req->elemsz);
		req->elemsz--;
		if (KCGI_OK != (er = khtml_flow_open
		    (req, req->elems[req->elemsz])))
			return(er);
		if (KCGI_OK != (er = kcgi_writer_puts(req->arg, "</")))
			return(er);
		if (KCGI_OK != (er = kcgi_writer_puts
		    (req->arg, tags[req->elems[req->elemsz]].name)))
			return(er);
		if (KCGI_OK != (er = kcgi_writer_putc(req->arg, '>')))
			return(er);
		if (KCGI_OK != (er = khtml_flow_close
	    	    (req, req->elems[req->elemsz])))
			return(er);
	}

	return(KCGI_OK);
}

enum kcgi_err
khtml_closeto(struct khtmlreq *req, size_t pos)
{

	if (pos > req->elemsz)
		return(KCGI_FORM);
	return(khtml_closeelem(req, req->elemsz - pos));
}

enum kcgi_err
khtml_entity(struct khtmlreq *req, enum kentity entity)
{

	assert(entity < KENTITY__MAX);
	return(khtml_ncr(req, entities[entity]));
}

enum kcgi_err
khtml_ncr(struct khtmlreq *req, uint16_t ncr)
{
	char	 	 buf[INT_MAXSZ];
	enum kcgi_err	 er;

	(void)snprintf(buf, sizeof(buf), "%" PRIx16, ncr);
	if (KCGI_OK != (er = kcgi_writer_puts(req->arg, "&#x")))
		return(er);
	if (KCGI_OK != (er = kcgi_writer_puts(req->arg, buf)))
		return(er);
	return(kcgi_writer_putc(req->arg, ';'));
}

enum kcgi_err
khtml_double(struct khtmlreq *req, double val)
{
	char	 buf[256];

	(void)snprintf(buf, sizeof(buf), "%g", val);
	return(khtml_puts(req, buf));
}

enum kcgi_err
khtml_int(struct khtmlreq *req, int64_t val)
{
	char	 buf[INT_MAXSZ];

	(void)snprintf(buf, sizeof(buf), "%" PRId64, val);
	return(khtml_puts(req, buf));
}

enum kcgi_err
khtml_putc(struct khtmlreq *r, char c)
{
	enum kcgi_err	 er;

	switch (c) {
	case ('>'):
		er = khtml_entity(r, KENTITY_gt);
		break;
	case ('&'):
		er = khtml_entity(r, KENTITY_amp);
		break;
	case ('<'):
		er = khtml_entity(r, KENTITY_lt);
		break;
	case ('"'):
		er = khtml_entity(r, KENTITY_quot);
		break;
	case ('\''):
		er = khtml_ncr(r, 39);
		break;
	default:
		er = kcgi_writer_putc(r->arg, c);
		break;
	}

	return(er);
}

enum kcgi_err
khtml_write(const char *cp, size_t sz, void *arg)
{
	struct khtmlreq	*r = arg;
	size_t		 i;
	enum kcgi_err	 er;

	for (i = 0; i < sz; i++) 
		if (KCGI_OK != (er = khtml_putc(r, cp[i])))
			return(er);

	return(KCGI_OK);
}

enum kcgi_err
khtml_printf(struct khtmlreq *req, const char *fmt, ...)
{
	char		*buf;
	int		 len;
	va_list		 ap;
	enum kcgi_err	 er;

	if (fmt == NULL)
		return KCGI_OK;

	va_start(ap, fmt);
	len = XVASPRINTF(&buf, fmt, ap);
	va_end(ap);
	if (len == -1)
		return KCGI_ENOMEM;
	er = khtml_write(buf, (size_t)len, req);
	free(buf);
	return er;
}

enum kcgi_err
khtml_puts(struct khtmlreq *req, const char *cp)
{

	return(khtml_write(cp, strlen(cp), req));
}


enum kcgi_err
khtml_open(struct khtmlreq *r, struct kreq *req, int opts)
{

	memset(r, 0, sizeof(struct khtmlreq));
	if (NULL == (r->arg = kcgi_writer_get(req, 0)))
		return(KCGI_ENOMEM);
	r->opts = opts;
	return(KCGI_OK);
}

enum kcgi_err
khtml_close(struct khtmlreq *r)
{
	enum kcgi_err	 er;

	er = khtml_closeelem(r, 0);
	kcgi_writer_free(r->arg);
	r->arg = NULL;
	return(er);
}
