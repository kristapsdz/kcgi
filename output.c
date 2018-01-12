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
	int		 disabled;
};

/*
 * A writer is allocated for use by a front-end formatter.
 * It is basically the bridge between the back-end writer (currently
 * operated by kdata_write) and the front-ends like kcgijson.
 */
struct	kcgi_writer {
	struct kdata	*kdata; /* the back-end writer context */
	int		 type; /* currently unused */
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
	if (0 != fflush(stderr)) {
		XWARN("flush");
		return(0);
	}
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

enum kcgi_err
khttp_write(struct kreq *req, const char *buf, size_t sz)
{

	assert(NULL != req->kdata);
	assert(KSTATE_BODY == req->kdata->state);
	assert( ! req->kdata->disabled);
	return(kdata_write(req->kdata, buf, sz));
}

enum kcgi_err
khttp_puts(struct kreq *req, const char *cp)
{

	return(khttp_write(req, cp, strlen(cp)));
}

enum kcgi_err
khttp_putc(struct kreq *req, int c)
{
	char		cc = c;

	return(khttp_write(req, &cc, 1));
}

enum kcgi_err
khttp_head(struct kreq *req, const char *key, const char *fmt, ...)
{
	va_list		 ap;
	char		*buf;
	size_t		 ksz;
	int		 len;
	enum kcgi_err	 er;

	assert(NULL != req->kdata);
	assert(KSTATE_HEAD == req->kdata->state);

	va_start(ap, fmt);
	len = XVASPRINTF(&buf, fmt, ap);
	va_end(ap);
	if (len < 0) 
		return(KCGI_ENOMEM);

	ksz = strlen(key);
	if (KCGI_OK != (er = kdata_write(req->kdata, key, ksz)))
		goto out;
	if (KCGI_OK != (er = kdata_write(req->kdata, ": ", 2)))
		goto out;
	if (KCGI_OK != (er = kdata_write(req->kdata, buf, len)))
		goto out;
	if (KCGI_OK != (er = kdata_write(req->kdata, "\r\n", 2)))
		goto out;
out:
	free(buf);
	return(er);
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
		if (0 != fflush(stderr))
			XWARN("flush");
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
 * Set "ret" to zero if compression is not enabled, non-zero if enabled.
 * Returns zero if allocation errors occured (via gzdopen(3)), non-zero
 * otherwise.
 * On failure, "ret" is always zero.
 */
static int
kdata_compress(struct kdata *p, int *ret)
{

	*ret = 0;
	assert(KSTATE_HEAD == p->state);
#if HAVE_ZLIB
	if (-1 != p->fcgi)
		return(1);
	assert(NULL == p->gz);
	p->gz = gzdopen(STDOUT_FILENO, "w");
	if (NULL == p->gz) {
		XWARN("gzdopen");
		return(0);
	}
	*ret = 1;
#endif
	return(1);
}

/*
 * Begin the body sequence by draining the headers to the wire and
 * marking that the body has begun.
 * Returns KCGI_OK on success, KCGI_ENOMEM on memory exhaustion, and
 * KCGI_SYSTEM on wire-writing failure.
 */
static enum kcgi_err
kdata_body(struct kdata *p)
{
	enum kcgi_err	 er;

	assert(p->state == KSTATE_HEAD);

	if (KCGI_OK != (er = kdata_write(p, "\r\n", 2)))
		return(er);

	/*
	 * XXX: we always drain our buffer after the headers have been
	 * written.
	 * This incurs more chat on the wire, but also ensures that our
	 * response gets to the server as quickly as possible.
	 * Should an option be added to disable this?
	 */

	if ( ! kdata_drain(p))
		return(KCGI_SYSTEM);

	if (-1 == p->fcgi)
		if (0 != fflush(stdout)) {
			XWARN("flush");
			return(KCGI_SYSTEM);
		}

	p->state = KSTATE_BODY;
	return(KCGI_OK);
}

enum kcgi_err
khttp_body(struct kreq *req)
{
	int	 	 hasreq = 0;
	enum kcgi_err	 er;
	const char	*cp;

	/*
	 * First determine if the request wants HTTP compression.
	 * Use RFC 2616 14.3 as a guide for checking this: if we have
	 * the "gzip" accept encoding and a non-zero quality, then use
	 * compression.
	 */

	if (NULL != req->reqmap[KREQU_ACCEPT_ENCODING]) {
		cp = req->reqmap[KREQU_ACCEPT_ENCODING]->val;
		if (NULL != (cp = strstr(cp, "gzip"))) {
			hasreq = 1;
			cp += 4;
			if (0 == strncmp(cp, ";q=0", 4)) 
				hasreq = '.' == cp[4];
		}
	}

	/*
	 * Note: the underlying writing functions will not do any
	 * compression even if we have compression enabled when in
	 * header mode, so the order of these operations (enable
	 * compression then write headers) is ok.
	 */

#if HAVE_ZLIB
	if (hasreq) {
		/*
		 * We could just ignore this error, which means gzdopen
		 * failed, and just continue with hasreq=0.
		 * However, if gzdopen fails (memory allocation), it
		 * probably means other things are going to fail, so we
		 * might as well just die now.
		 */
		if ( ! kdata_compress(req->kdata, &hasreq))
			return(KCGI_ENOMEM);
		if (hasreq) {
			er = khttp_head(req, 
				kresps[KRESP_CONTENT_ENCODING], "gzip");
			if (KCGI_OK != er)
				return(er);
		}
	}
#endif
	return(kdata_body(req->kdata));
}

enum kcgi_err
khttp_body_compress(struct kreq *req, int comp)
{
	int	 didcomp;

	if ( ! comp)
		return(kdata_body(req->kdata));
#if HAVE_ZLIB
	if ( ! kdata_compress(req->kdata, &didcomp))
		return(KCGI_ENOMEM);
	else if ( ! didcomp)
		return(KCGI_FORM);

	return(kdata_body(req->kdata));
#else
	return(KCGI_FORM);
#endif
}

/*
 * Allocate a writer.
 * This only works if we haven't disabled allocation of writers yet via
 * kcgi_writer_disable(), otherwise we abort().
 * Returns the writer or NULL on allocation (memory) failure.
 */
struct kcgi_writer *
kcgi_writer_get(struct kreq *r, int type)
{
	struct kcgi_writer	*p;

	assert( ! r->kdata->disabled);

	p = XMALLOC(sizeof(struct kcgi_writer));
	if (NULL != p)
		p->kdata = r->kdata;
	return(p);
}

/*
 * Disable further allocation of writers with kcgi_writer_get().
 * Following this, kcgi_writer_get() will abort.
 * This may be called as many times as desired: only the first time
 * makes a difference.
 */
void
kcgi_writer_disable(struct kreq *r)
{

	r->kdata->disabled = 1;
}

/*
 * Release an allocation by kcgi_writer_get().
 * May be called with a NULL-valued "p".
 */
void
kcgi_writer_free(struct kcgi_writer *p)
{

	free(p);
}

/*
 * Write "sz" bytes of "buf" into the output.
 * This doesn't necessarily mean that the output has been written: it
 * may be further buffered.
 * Returns KCGI_OK, KCGI_ENOMEM, or KCGI_SYSTEM.
 */
enum kcgi_err
kcgi_writer_write(struct kcgi_writer *p, const void *buf, size_t sz)
{

	assert(KSTATE_BODY == p->kdata->state);
	return(kdata_write(p->kdata, buf, sz));
}

/*
 * Like kcgi_writer_write but for the NUL-terminated string.
 */
enum kcgi_err
kcgi_writer_puts(struct kcgi_writer *p, const char *cp)
{

	return(kcgi_writer_write(p, cp, strlen(cp)));
}

/*
 * Like kcgi_writer_write but for a single character.
 */
enum kcgi_err
kcgi_writer_putc(struct kcgi_writer *p, char c)
{

	return(kcgi_writer_write(p, &c, 1));
}
