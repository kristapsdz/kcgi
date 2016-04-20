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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
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
#ifdef HAVE_ZLIB
#include <zlib.h>
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
#ifdef	HAVE_ZLIB
	gzFile		 gz;
#endif
	char		*outbuf;
	size_t		 outbufpos;
	size_t		 outbufsz;
};

static char *
fcgi_header(uint8_t type, uint16_t requestId,
	size_t contentLength, size_t paddingLength)
{
	static char	header[8];
	uint8_t		byte;

	byte = 1;
	memcpy(header, &byte, sizeof(uint8_t));
	memcpy(header + 1, &type, sizeof(uint8_t));
	byte = (requestId >> 8) & 0xff;
	memcpy(header + 2, &byte, sizeof(uint8_t));
	byte = requestId & 0xff;
	memcpy(header + 3, &byte, sizeof(uint8_t));
	byte = (contentLength >> 8) & 0xff;
	memcpy(header + 4, &byte, sizeof(uint8_t));
	byte = contentLength & 0xff;
	memcpy(header + 5, &byte, sizeof(uint8_t));
	memcpy(header + 6, &paddingLength, sizeof(uint8_t));
	byte = 0;
	memcpy(header + 7, &byte, sizeof(uint8_t));
	return(header);
}


/*
 * Write a `stdout' FastCGI packet.
 * This involves writing the header, then the data itself.
 */
static void
fcgi_write(uint8_t type, const struct kdata *p, const char *buf, size_t sz)
{
	char	 padding[256];
	size_t	 rsz, paddingLength;

	/* Break up the data stream into FastCGI-capable chunks. */
	do {
		paddingLength = 0;
		rsz = sz > UINT16_MAX ? UINT16_MAX : sz;
		/* Pad to 8-byte boundary. */
		while (0 != ((rsz + paddingLength) % 8)) {
			assert(paddingLength < 256);
			padding[paddingLength++] = '\0';
		}
#if 0
		fprintf(stderr, "%s: DEBUG send type: %" PRIu8 "\n",
			__func__, type);
		fprintf(stderr, "%s: DEBUG send requestId: %" PRIu16 "\n",
			__func__, p->requestId);
		fprintf(stderr, "%s: DEBUG send contentLength: %zu\n",
			__func__, rsz);
		fprintf(stderr, "%s: DEBUG send paddingLength: %zu\n",
			__func__, paddingLength);
#endif
		fullwritenoerr(p->fcgi, fcgi_header
			(type, p->requestId, rsz, paddingLength), 8);
		fullwritenoerr(p->fcgi, buf, rsz);
		fullwritenoerr(p->fcgi, padding, paddingLength);
		sz -= rsz;
		buf += rsz;
	} while (sz > 0);
}

static void
linebuf_init(struct kdata *p)
{

	if (NULL != p->linebuf)
		return;
	p->linebufsz = BUFSIZ;
	p->linebuf = kmalloc(p->linebufsz);
	p->linebuf[0] = '\0';
	p->linebufpos = 0;
}

static void
linebuf_flush(struct kdata *p, int newln)
{

	fprintf(stderr, "%u: %s%s", getpid(),
		p->linebuf, newln ? "\n" : "");
	fflush(stderr);
	p->linebufpos = 0;
	p->linebuf[0] = '\0';
}

/*
 * Flushes a buffer "buf" of size "sz" to the wire (stdout in the case
 * of CGI, the socket for FastCGI, and a gz stream for compression IFF
 * in body parts).
 * If sz is zero or buf is NULL, this is a no-op.
 */
static void
kdata_flush(struct kdata *p, const char *buf, size_t sz)
{

	if (0 == sz || NULL == buf)
		return;
#ifdef HAVE_ZLIB
	if (NULL != p->gz && KSTATE_HEAD != p->state) {
		/*
		 * FIXME: make this work properly on all systems.
		 * This is known to break on FreeBSD: we may need to
		 * break the uncompressed buffer into chunks that will
		 * not cause EAGAIN to be raised.
		 */
		if (0 == gzwrite(p->gz, buf, sz))
			XWARNX("gzwrite");
		return;
	}
#endif
	if (-1 == p->fcgi)
		fullwritenoerr(STDOUT_FILENO, buf, sz);
	else
		fcgi_write(6, p, buf, sz);
}

/*
 * Drain the output buffer.
 */
static void
kdata_drain(struct kdata *p)
{

	if (0 == p->outbufpos)
		return;
	kdata_flush(p, p->outbuf, p->outbufpos);
	p->outbufpos = 0;
}

/*
 * In this function, we handle arbitrary writes of data to the output.
 * In the event of CGI, this will be to stdout; in the event of FastCGI,
 * this is to the wire.
 * We want to handle buffering of output, so we maintain an output
 * buffer that we fill and drain as needed.
 * This will end up calling kdata_flush(), directly or indirectly.
 * This will also handle debugging.
 */
static void
kdata_write(struct kdata *p, const char *buf, size_t sz)
{
	size_t	 	 i;

	assert(NULL != p);

	if (0 == sz || NULL == buf)
		return;

	/*
	 * We want to debug writes.
	 * To do so, we write into a line buffer.
	 * Whenever we hit a newline (or the line buffer is filled),
	 * flush the buffer to stderr.
	 */
	if (KREQ_DEBUG_WRITE & p->debugging) {
		linebuf_init(p);
		for (i = 0; i < sz; i++, p->bytes++) {
			if (p->linebufpos + 4 >= p->linebufsz)
				linebuf_flush(p, 1);
			if (isprint((int)buf[i]) || '\n' == buf[i]) {
				p->linebuf[p->linebufpos++] = buf[i];
				p->linebuf[p->linebufpos] = '\0';
			} else if ('\t' == buf[i]) {
				p->linebuf[p->linebufpos++] = '\\';
				p->linebuf[p->linebufpos++] = 't';
				p->linebuf[p->linebufpos] = '\0';
			} else if ('\r' == buf[i]) {
				p->linebuf[p->linebufpos++] = '\\';
				p->linebuf[p->linebufpos++] = 'r';
				p->linebuf[p->linebufpos] = '\0';
			} else if ('\v' == buf[i]) {
				p->linebuf[p->linebufpos++] = '\\';
				p->linebuf[p->linebufpos++] = 'v';
				p->linebuf[p->linebufpos] = '\0';
			} else if ('\b' == buf[i]) {
				p->linebuf[p->linebufpos++] = '\\';
				p->linebuf[p->linebufpos++] = 'b';
				p->linebuf[p->linebufpos] = '\0';
			} else {
				p->linebuf[p->linebufpos++] = '?';
				p->linebuf[p->linebufpos] = '\0';
			}
			if ('\n' == buf[i])
				linebuf_flush(p, 0);
		}
	}

	/*
	 * Short-circuit: if we have no output buffer, flush directly to
	 * the wire.
	 */
	if (0 == p->outbufsz) {
		kdata_flush(p, buf, sz);
		return;
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
		kdata_drain(p);
		if (sz > p->outbufsz) {
			kdata_flush(p, buf, sz);
			return;
		}
	}

	assert(p->outbufpos + sz <= p->outbufsz);
	assert(NULL != p->outbuf);
	memcpy(p->outbuf + p->outbufpos, buf, sz);
	p->outbufpos += sz;
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
		if (p->linebufpos > 0)
			linebuf_flush(p, 1);
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

#ifdef HAVE_ZLIB
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
int
kdata_compress(struct kdata *p)
{

	assert(KSTATE_HEAD == p->state);
#ifdef	HAVE_ZLIB
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
void
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
