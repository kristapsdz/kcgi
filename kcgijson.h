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
#ifndef KCGIJSON_H
#define KCGIJSON_H

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

enum	kjsontype {
	KJSON_ARRAY,
	KJSON_OBJECT,
	KJSON_ROOT,
	KJSON_STRING
};

struct	kjsonscope {
	size_t		  elements;
	enum kjsontype	  type;
};

struct	kjsonreq {
	struct kreq	 *req;
	size_t		  stackpos;
	struct kjsonscope stack[128];
};

__BEGIN_DECLS

void	kjson_open(struct kjsonreq *, struct kreq *);
int	kjson_close(struct kjsonreq *);

int	kjson_putdoublep(struct kjsonreq *, const char *, double);
int	kjson_putintp(struct kjsonreq *, const char *, int64_t);
int	kjson_putintstrp(struct kjsonreq *, const char *, int64_t);
int	kjson_putstringp(struct kjsonreq *, const char *, const char *);
int	kjson_putboolp(struct kjsonreq *, const char *, int);
int	kjson_putnullp(struct kjsonreq *, const char *);

int	kjson_putdouble(struct kjsonreq *, double);
int	kjson_putint(struct kjsonreq *, int64_t);
int	kjson_putintstr(struct kjsonreq *, int64_t);
int	kjson_putstring(struct kjsonreq *, const char *);
int	kjson_putbool(struct kjsonreq *, int);
int	kjson_putnull(struct kjsonreq *);

int	kjson_objp_open(struct kjsonreq *, const char *);
int	kjson_obj_open(struct kjsonreq *);
int	kjson_obj_close(struct kjsonreq *);

int	kjson_arrayp_open(struct kjsonreq *, const char *);
int	kjson_array_open(struct kjsonreq *);
int	kjson_array_close(struct kjsonreq *);

int	kjson_stringp_open(struct kjsonreq *, const char *);
int	kjson_string_open(struct kjsonreq *);
int	kjson_string_close(struct kjsonreq *);
int	kjson_string_write(const char *, size_t, void *);
int	kjson_string_putdouble(struct kjsonreq *, double);
int	kjson_string_putint(struct kjsonreq *, int64_t);
int	kjson_string_puts(struct kjsonreq *, const char *);

__END_DECLS

#endif
