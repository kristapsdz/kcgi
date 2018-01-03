/*	$Id$ */
/*
 * Copyright (c) 2015, 2016 Kristaps Dzonsons <kristaps@bsd.lv>
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

#include <arpa/inet.h>

#include <assert.h>
#include <ctype.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#if HAVE_ZLIB
# include <zlib.h>
#endif

#include "kcgi.h"
#include "extern.h"

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
	int		 debugging; /* debugging flags */
	int		 fcgi; /* file descriptor or -1 */
	int		 control; /* control socket or -1 */
	char		*linebuf; /* output line buffer */
	size_t		 linebufpos; /* output line buffer */
	size_t		 linebufsz;
	uint64_t	 bytes; /* total bytes written */
	uint16_t	 requestId; /* current requestId or 0 */
	enum kstate	 state;
#if HAVE_ZLIB
	gzFile		 gz;
#endif
	char		*outbuf;
	size_t		 outbufpos;
	size_t		 outbufsz;
};

static char *
fcgi_header(uint8_t type, uint16_t requestId, 
	uint16_t contentLength, uint8_t paddingLength)
{
	static uint8_t	header[8];

	/* Masking probably not necessary: truncation. */

	header[0] = 1;
	header[1] = type;
	header[2] = (requestId >> 8) & 0xff;
	header[3] = requestId & 0xff;
	header[4] = (contentLength >> 8) & 0xff;
	header[5] = contentLength & 0xff;
	header[6] = paddingLength;
	header[7] = 0;
	return(header);
}

/*
 * Write a `stdout' FastCGI packet.
 * This involves writing the header, then the data itself, then padding.
 * Returns zero on failure (system error writing to channel), non-zero
 * for success.
 */
static int
fcgi_write(uint8_t type, const struct kdata *p, const char *buf, size_t sz)
{
	const char	*pad = "\0\0\0\0\0\0\0\0";
	char		*head;
	size_t	 	 rsz, padlen;

	/* 
	 * Break up the data stream into FastCGI-capable chunks of at
	 * most UINT16_MAX bytes.
	 * Send each of these in its own FastCGI frame.
	 */

	do {
		/* Pad to 8-byte boundary. */

		rsz = sz > UINT16_MAX ? UINT16_MAX : sz;
		padlen = -rsz % 8;
		head = fcgi_header(type, p->requestId, rsz, padlen);

		if (fullwritenoerr(p->fcgi, head, 8) <= 0)
			return(0);
		if (fullwritenoerr(p->fcgi, buf, rsz) <= 0)
			return(0);
		if (fullwritenoerr(p->fcgi, pad, padlen) <= 0)
			return(0);
		sz -= rsz;
		buf += rsz;
	} while (sz > 0);

	return(1);
}

/*
 * Initialise the buffer holding our debugging lines of output.
 * This returns zero on failure (memory failure) or non-zero on success.
 * It does nothing if the line buffer is already initialised.
 */
static int
linebuf_init(struct kdata *p)
{

	if (NULL == p->linebuf) {
		p->linebufsz = BUFSIZ;
		p->linebuf = XMALLOC(p->linebufsz);
		if (NULL == p->linebuf)
			return(0);
		p->linebuf[0] = '\0';
		p->linebufpos = 0;
	}
	return(1);
}

/*
 * Flush the debugging line to our output channel.
 * Returns zero on failure (system error), non-zero on success.
 * Does nothing (success) if there are no bytes in the line buffer.
 */
static int
linebuf_flush(struct kdata *p, int newln)
{
	int	 rc;

	if (0 == p->linebufpos)
		return(1);

	rc = fprintf(stderr, "%u: %s%s", 
		getpid(), p->linebuf, newln ? "\n" : "");
	if (rc < 0)
		return(0);
	if (0 != fflush(stderr))
		return(0);
	p->linebufpos = 0;
	p->linebuf[0] = '\0';
	return(1);
}

/*
 * Flushes a buffer "buf" of size "sz" to the wire (stdout in the case
 * of CGI, the socket for FastCGI, and a gz stream for compression IFF
 * in body parts).
 * If sz is zero or buf is NULL, this is a no-op.
 * Return zero on failure (system error writing to output) or non-zero
 * on success.
 */
static int
kdata_flush(struct kdata *p, const char *buf, size_t sz)
{

	if (0 == sz || NULL == buf)
		return(1);
#if HAVE_ZLIB
	if (NULL != p->gz && KSTATE_HEAD != p->state) {
		/*
		 * FIXME: make this work properly on all systems.
		 * This is known to break on FreeBSD: we may need to
		 * break the uncompressed buffer into chunks that will
		 * not cause EAGAIN to be raised.
		 */
		if (0 == gzwrite(p->gz, buf, sz)) {
			XWARNX("gzwrite");
			return(0);
		}
		return(1);
	}
#endif
	if (-1 == p->fcgi) {
		if (fullwritenoerr(STDOUT_FILENO, buf, sz) <= 0)
			return(0);
	} else 
		if ( ! fcgi_write(6, p, buf, sz))
			return(0);

	return(1);
}

/*
 * Drain the output buffer.
 * This does nothing if there's no output buffer.
 * Returns zero on failure (system error), non-zero on success.
 */
static int
kdata_drain(struct kdata *p)
{

	if (0 == p->outbufpos)
		return(1);
	if ( ! kdata_flush(p, p->outbuf, p->outbufpos))
		return(0);
	p->outbufpos = 0;
	return(1);
}

/*
 * In this function, we handle arbitrary writes of data to the output.
 * In the event of CGI, this will be to stdout; in the event of FastCGI,
 * this is to the wire.
 * We want to handle buffering of output, so we maintain an output
 * buffer that we fill and drain as needed.
 * This will end up calling kdata_flush(), directly or indirectly.
 * This will also handle debugging.
 * Returns KCGI_ENOMEM if any allocation failures occur during the
 * sequence, KCGI_SYSTEM if any errors occur writing to the output
 * channel, or KCGI_OK on success.
 */
static enum kcgi_err
kdata_write(struct kdata *p, const char *buf, size_t sz)
{
	size_t	 	 i;

	assert(NULL != p);

	if (0 == sz || NULL == buf)
		return(KCGI_OK);

	/*
	 * We want to debug writes.
	 * To do so, we write into a line buffer.
	 * Whenever we hit a newline (or the line buffer is filled),
	 * flush the buffer to stderr.
	 */

	if (KREQ_DEBUG_WRITE & p->debugging) {
		if ( ! linebuf_init(p))
			return(KCGI_ENOMEM);
		for (i = 0; i < sz; i++, p->bytes++) {
			if (p->linebufpos + 4 >= p->linebufsz)
				if ( ! linebuf_flush(p, 1))
					return(KCGI_SYSTEM);
			if (isprint((unsigned char)buf[i]) || '\n' == buf[i]) {
				p->linebuf[p->linebufpos++] = buf[i];
			} else if ('\t' == buf[i]) {
				p->linebuf[p->linebufpos++] = '\\';
				p->linebuf[p->linebufpos++] = 't';
			} else if ('\r' == buf[i]) {
				p->linebuf[p->linebufpos++] = '\\';
				p->linebuf[p->linebufpos++] = 'r';
			} else if ('\v' == buf[i]) {
				p->linebuf[p->linebufpos++] = '\\';
				p->linebuf[p->linebufpos++] = 'v';
			} else if ('\b' == buf[i]) {
				p->linebuf[p->linebufpos++] = '\\';
				p->linebuf[p->linebufpos++] = 'b';
			} else
				p->linebuf[p->linebufpos++] = '?';

			p->linebuf[p->linebufpos] = '\0';
			if ('\n' == buf[i])
				if ( ! linebuf_flush(p, 0))
					return(KCGI_SYSTEM);
		}
	}

	/* 
	 * Short-circuit: if we have no output buffer, flush directly to
	 * the wire.
	 */

	if (0 == p->outbufsz) {
		if ( ! kdata_flush(p, buf, sz))
			return(KCGI_SYSTEM);
		return(KCGI_OK);
	}

	/*
	 * If we want to accept new data and it exceeds the buffer size,
	 * push out the entire existing buffer to start.
	 * Then re-check if we exceed our buffer size.
	 * If we do, then instead of filling into the temporary buffer
	 * and looping until the new buffer is exhausted, just push the
	 * whole thing out.
	 * If we don't, then copy it into the buffer.
	 */

	if (p->outbufpos + sz > p->outbufsz) {
		if ( ! kdata_drain(p))
			return(KCGI_SYSTEM);
		if (sz > p->outbufsz) {
			if ( ! kdata_flush(p, buf, sz))
				return(KCGI_SYSTEM);
			return(KCGI_OK);
		}
	}

	assert(p->outbufpos + sz <= p->outbufsz);
	assert(NULL != p->outbuf);
	memcpy(p->outbuf + p->outbufpos, buf, sz);
	p->outbufpos += sz;
	return(KCGI_OK);
}

/*
 * Just like khttp_head() except with a KSTATE_BODY.
 */
void
khttp_write(struct kreq *req, const char *buf, size_t sz)
{
	struct kdata	*p = req->kdata;

	assert(NULL != p);
	assert(KSTATE_BODY == p->state);
	kdata_write(req->kdata, buf, sz);
}

void
khttp_puts(struct kreq *req, const char *cp)
{

	khttp_write(req, cp, strlen(cp));
}

void
khttp_putc(struct kreq *req, int c)
{
	char		cc = c;

	khttp_write(req, &cc, 1);
}

/*
 * Fill a static buffer with the header.
 * FIXME: THIS IS NOT THE CORRECT WAY OF DOING THINGS.
 */
void
khttp_head(struct kreq *req, const char *key, const char *fmt, ...)
{
	va_list	 ap;
	char	 buf[BUFSIZ];

	assert(NULL != req->kdata);
	assert(KSTATE_HEAD == req->kdata->state);

	/* FIXME: this does *not* work with long headers. */
	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	kdata_write(req->kdata, key, strlen(key));
	kdata_write(req->kdata, ": ", 2);
	kdata_write(req->kdata, buf, strlen(buf));
	kdata_write(req->kdata, "\r\n", 2);
}

/*
 * Allocate our output data.
 * We accept the file descriptor for the FastCGI stream, if there's any.
 */
struct kdata *
kdata_alloc(int control, int fcgi, uint16_t requestId, 
	unsigned int debugging, const struct kopts *opts)
{
	struct kdata	*p;

	if (NULL == (p = XCALLOC(1, sizeof(struct kdata))))
		return(NULL);

	p->debugging = debugging;
	p->fcgi = fcgi;
	p->control = control;
	p->requestId = requestId;

	if (opts->sndbufsz > 0) {
		p->outbufsz = opts->sndbufsz;
		if (NULL == (p->outbuf = XMALLOC(p->outbufsz))) {
			free(p);
			return(NULL);
		}
	} 

	return(p);
}

/*
 * Two ways of doing this: with or without "flush".
 * If we're flushing, then we drain our output buffers to the output.
 * Either way, we then release all of our internal memory.
 */
void
kdata_free(struct kdata *p, int flush)
{
	char	 	 buf[8];
	uint32_t 	 appStatus;

	if (NULL == p)
		return;

	/* Debugging messages. */
	if (flush && KREQ_DEBUG_WRITE & p->debugging) {
		(void)linebuf_flush(p, 1);
		fprintf(stderr, "%u: %" PRIu64 " B tx\n", 
			getpid(), p->bytes);
		fflush(stderr);
	}

	/* Remaining buffered data. */
	if (flush) 
		kdata_drain(p);

	free(p->linebuf); 
	free(p->outbuf);

	/*
	 * If we're not FastCGI and we're not going to flush, then close
	 * the file descriptors outright: we don't want gzclose()
	 * flushing anything to the wire.
	 */
	if ( ! flush && -1 == p->fcgi) {
		close(STDOUT_FILENO);
		close(STDIN_FILENO);
	}

#if HAVE_ZLIB
	if (NULL != p->gz)
		gzclose(p->gz);
#endif
	if (-1 == p->fcgi) {
		free(p);
		return;
	}

	if (flush) {
		/* 
		 * End of stream.
		 * Note that we've already flushed our last FastCGI
		 * record to the stream, but the standard implies that
		 * we need a blank record to really shut this thing
		 * down.
		 */
		fcgi_write(6, p, "", 0);
		appStatus = htonl(EXIT_SUCCESS);
		memset(buf, 0, 8);
		memcpy(buf, &appStatus, sizeof(uint32_t));
		/* End of request. */
		fcgi_write(3, p, buf, 8);
		/* Close out. */
		close(p->fcgi);
		fullwrite(p->control, &p->requestId, sizeof(uint16_t));
		p->control = -1;
		p->fcgi = -1;
	} else {
		close(p->fcgi);
		p->fcgi = -1;
	}

	free(p);
}

/*
 * Try to enable compression on the output stream itself.
 * We disallow compression on FastCGI streams because I don't yet have
 * the structure in place to copy the compressed data into a buffer then
 * write that out.
 * Return whether we enabled compression.
 */
static int
kdata_compress(struct kdata *p)
{

	assert(KSTATE_HEAD == p->state);
#if HAVE_ZLIB
	if (-1 != p->fcgi)
		return(0);
	assert(NULL == p->gz);
	p->gz = gzdopen(STDOUT_FILENO, "w");
	if (NULL == p->gz)
		XWARN("gzdopen");
	return(NULL != p->gz);
#else
	return(0);
#endif
}

/*
 * Begin the body sequence.
 */
static void
kdata_body(struct kdata *p)
{

	assert(p->state == KSTATE_HEAD);

	kdata_write(p, "\r\n", 2);
	/*
	 * XXX: we always drain our buffer after the headers have been
	 * written.
	 * This incurs more chat on the wire, but also ensures that our
	 * response gets to the server as quickly as possible.
	 * Should an option be added to disable this?
	 */
	kdata_drain(p);
	if (-1 == p->fcgi)
		fflush(stdout);

	p->state = KSTATE_BODY;
}

int
khttp_body(struct kreq *req)
{

	return(khttp_body_compress(req, 1));
}

int
khttp_body_compress(struct kreq *req, int comp)
{
	int	 	 didcomp, hasreq;
	const char	*cp;

	didcomp = 0;
	/*
	 * First determine if the request wants HTTP compression.
	 * Use RFC 2616 14.3 as a guide for checking this.
	 */
	hasreq = 0;
	if (NULL != req->reqmap[KREQU_ACCEPT_ENCODING]) {
		cp = req->reqmap[KREQU_ACCEPT_ENCODING]->val;
		if (NULL != (cp = strstr(cp, "gzip"))) {
			hasreq = 1;
			/* 
			 * We have the "gzip" line, so assume that we're
			 * going to use gzip compression.
			 * However, unset this if we have q=0.
			 * FIXME: we should actually check the number,
			 * not look at the first digit (q=0.0, etc.).
			 */
			cp += 4;
			if (0 == strncmp(cp, ";q=0", 4)) 
				hasreq = '.' == cp[4];
		}
	}

	/* Only work with compression if we support it... */
#if HAVE_ZLIB
	/* 
	 * Enable compression if the function argument is zero or if
	 * it's >0 and the request headers have been set for it.
	 */
	if (0 == comp || (comp > 0 && hasreq))
		didcomp = kdata_compress(req->kdata);
	/* 
	 * Only set the header if we're autocompressing and opening of
	 * the compression stream did not fail.
	 */
	if (comp > 0 && didcomp)
		khttp_head(req, 
			kresps[KRESP_CONTENT_ENCODING], 
			"%s", "gzip");
#endif
	kdata_body(req->kdata);
	return(didcomp);
}

