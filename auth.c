/*	$Id$ */
/*
 * Copyright (c) 2015--2018 Kristaps Dzonsons <kristaps@bsd.lv>
 * Copyright (c) 2018 Charles Collicutt <charles@collicutt.co.uk>
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

#include <inttypes.h>
#if HAVE_MD5
# include <sys/types.h>
# include <md5.h>
#endif
#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "kcgi.h"
#include "extern.h"

#define MD5Updatec(_ctx, _b, _sz) \
	MD5Update((_ctx), (const uint8_t *)(_b), (_sz))

static const char b64[] = 
	"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
	"abcdefghijklmnopqrstuvwxyz"
	"0123456789+/";

static size_t 
base64len(size_t len)
{

	return((len + 2) / 3 * 4) + 1;
}

static size_t 
base64buf(char *enc, const char *str, size_t len)
{
	size_t 	i;
	char 	*p;

	p = enc;

	for (i = 0; i < len - 2; i += 3) {
		*p++ = b64[(str[i] >> 2) & 0x3F];
		*p++ = b64[((str[i] & 0x3) << 4) |
			((int)(str[i + 1] & 0xF0) >> 4)];
		*p++ = b64[((str[i + 1] & 0xF) << 2) |
			((int)(str[i + 2] & 0xC0) >> 6)];
		*p++ = b64[str[i + 2] & 0x3F];
	}

	if (i < len) {
		*p++ = b64[(str[i] >> 2) & 0x3F];
		if (i == (len - 1)) {
			*p++ = b64[((str[i] & 0x3) << 4)];
			*p++ = '=';
		} else {
			*p++ = b64[((str[i] & 0x3) << 4) |
				((int)(str[i + 1] & 0xF0) >> 4)];
			*p++ = b64[((str[i + 1] & 0xF) << 2)];
		}
		*p++ = '=';
	}

	*p++ = '\0';
	return(p - enc);
}

int
khttpbasic_validate(const struct kreq *req, 
	const char *user, const char *pass)
{
	char	*buf, *enc;
	size_t	 sz;
	int	 rc;

	if (KAUTH_BASIC != req->rawauth.type)
		return(-1);
	else if (KMETHOD__MAX == req->method)
		return(-1);
	else if (0 == req->rawauth.authorised)
		return(-1);

	/* Make sure we don't bail on memory allocation. */
	sz = strlen(user) + 1 + strlen(pass) + 1;
	if (NULL == (buf = XMALLOC(sz)))
		return(-1);
	sz = snprintf(buf, sz, "%s:%s", user, pass);
	if (NULL == (enc = XMALLOC(base64len(sz)))) {
		free(buf);
		return(-1);
	}
	base64buf(enc, buf, sz);
	rc = 0 == strcmp(enc, req->rawauth.d.basic.response);
	free(enc);
	free(buf);
	return(rc);
}

int
khttpdigest_validatehash(const struct kreq *req, const char *skey4)
{
	MD5_CTX	 	 ctx;
	unsigned char	 ha1[MD5_DIGEST_LENGTH],
			 ha2[MD5_DIGEST_LENGTH],
			 ha3[MD5_DIGEST_LENGTH];
	char		 skey1[MD5_DIGEST_LENGTH * 2 + 1],
			 skey2[MD5_DIGEST_LENGTH * 2 + 1],
			 skey3[MD5_DIGEST_LENGTH * 2 + 1],
	                 skeyb[MD5_DIGEST_LENGTH * 2 + 1],
			 count[9];
	size_t		 i;
	const struct khttpdigest *auth;

	/*
	 * Make sure we're a digest with all fields intact.
	 */
	if (KAUTH_DIGEST != req->rawauth.type)
		return(-1);
	else if (KMETHOD__MAX == req->method)
		return(-1);
	else if (0 == req->rawauth.authorised)
		return(-1);

	auth = &req->rawauth.d.digest;

	/*
	 * MD5-sess hashes the nonce and client nonce as well as the
	 * existing hash (user/real/pass).
	 */

	if (KHTTPALG_MD5_SESS == auth->alg) {
		MD5Init(&ctx);
		MD5Updatec(&ctx, skey4, strlen(skey4));
		MD5Updatec(&ctx, ":", 1);
		MD5Updatec(&ctx, auth->nonce, strlen(auth->nonce));
		MD5Updatec(&ctx, ":", 1);
		MD5Updatec(&ctx, auth->cnonce, strlen(auth->cnonce));
		MD5Final(ha1, &ctx);
		for (i = 0; i < MD5_DIGEST_LENGTH; i++) 
			snprintf(&skey1[i * 2], 3, "%02x", ha1[i]);
	} else 
		strlcpy(skey1, skey4, sizeof(skey1));

	/* Now start the "auth" hash sequence. */

	MD5Init(&ctx);
	MD5Updatec(&ctx, kmethods[req->method],
		strlen(kmethods[req->method]));
	MD5Updatec(&ctx, ":", 1);
	MD5Updatec(&ctx, auth->uri, strlen(auth->uri));

	/*
	 * If we're requesting integrity authentication ("auth-int"),
	 * then we also bring in the hash of the message body.
	 */

	if (KHTTPQOP_AUTH_INT == auth->qop) {
		/* This shouldn't happen... */
		if (NULL == req->rawauth.digest)
			return(-1);

		for (i = 0; i < MD5_DIGEST_LENGTH; i++)
			snprintf(&skeyb[i * 2], 3, "%02x",
			    (unsigned char)req->rawauth.digest[i]);

		MD5Updatec(&ctx, ":", 1);
		MD5Updatec(&ctx, skeyb, MD5_DIGEST_LENGTH * 2);
	}

	MD5Final(ha2, &ctx);

	for (i = 0; i < MD5_DIGEST_LENGTH; i++) 
		snprintf(&skey2[i * 2], 3, "%02x", ha2[i]);

	if (KHTTPQOP_AUTH_INT == auth->qop || 
	    KHTTPQOP_AUTH == auth->qop) {
		snprintf(count, sizeof(count), "%08" PRIx32, auth->count);
		MD5Init(&ctx);
		MD5Updatec(&ctx, skey1, MD5_DIGEST_LENGTH * 2);
		MD5Updatec(&ctx, ":", 1);
		MD5Updatec(&ctx, auth->nonce, strlen(auth->nonce));
		MD5Updatec(&ctx, ":", 1);
		MD5Updatec(&ctx, count, strlen(count));
		MD5Updatec(&ctx, ":", 1);
		MD5Updatec(&ctx, auth->cnonce, strlen(auth->cnonce));
		MD5Updatec(&ctx, ":", 1);
		if (KHTTPQOP_AUTH_INT == auth->qop)
			MD5Updatec(&ctx, "auth-int", 8);
		else
			MD5Updatec(&ctx, "auth", 4);
		MD5Updatec(&ctx, ":", 1);
		MD5Updatec(&ctx, skey2, MD5_DIGEST_LENGTH * 2);
		MD5Final(ha3, &ctx);
	} else {
		MD5Init(&ctx);
		MD5Updatec(&ctx, skey1, MD5_DIGEST_LENGTH * 2);
		MD5Updatec(&ctx, ":", 1);
		MD5Updatec(&ctx, auth->nonce, strlen(auth->nonce));
		MD5Updatec(&ctx, ":", 1);
		MD5Updatec(&ctx, skey2, MD5_DIGEST_LENGTH * 2);
		MD5Final(ha3, &ctx);
	}

	for (i = 0; i < MD5_DIGEST_LENGTH; i++) 
		snprintf(&skey3[i * 2], 3, "%02x", ha3[i]);

	return(0 == strcmp(auth->response, skey3));
}

int
khttpdigest_validate(const struct kreq *req, const char *pass)
{
	MD5_CTX	 	 ctx;
	unsigned char	 ha4[MD5_DIGEST_LENGTH];
	char		 skey4[MD5_DIGEST_LENGTH * 2 + 1];
	size_t		 i;
	const struct khttpdigest *auth;

	/*
	 * Make sure we're a digest with all fields intact.
	 */

	if (KAUTH_DIGEST != req->rawauth.type)
		return(-1);
	else if (KMETHOD__MAX == req->method)
		return(-1);
	else if (0 == req->rawauth.authorised)
		return(-1);

	auth = &req->rawauth.d.digest;

	MD5Init(&ctx);
	MD5Updatec(&ctx, auth->user, strlen(auth->user));
	MD5Updatec(&ctx, ":", 1);
	MD5Updatec(&ctx, auth->realm, strlen(auth->realm));
	MD5Updatec(&ctx, ":", 1);
	MD5Updatec(&ctx, pass, strlen(pass));
	MD5Final(ha4, &ctx);

	for (i = 0; i < MD5_DIGEST_LENGTH; i++) 
		snprintf(&skey4[i * 2], 3, "%02x", ha4[i]);

	return(khttpdigest_validatehash(req, skey4));
}
