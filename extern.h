/*	$Id$ */
/*
 * Copyright (c) 2014, 2015, 2016, 2018 Kristaps Dzonsons <kristaps@bsd.lv>
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
#ifndef EXTERN_H
#define EXTERN_H

/*
 * Types of field we can read from the child.
 * This governs the list of fields into which we'll assign this
 * particular field.
 */
enum	input {
	IN_COOKIE = 0, /* cookies (environment variable) */
	IN_QUERY, /* query string */
	IN_FORM, /* any sort of standard input form */
	IN__MAX
};

enum	sandtype {
	SAND_WORKER,
	SAND_CONTROL_NEW,
	SAND_CONTROL_OLD
};

#define KWORKER_PARENT  1
#define KWORKER_CHILD	0

__BEGIN_DECLS

struct kdata	*kdata_alloc(int, int, uint16_t, 
			unsigned int, const struct kopts *);
void		 kdata_free(struct kdata *, int);

int		 kworker_auth_child(int, const char *);
enum kcgi_err	 kworker_auth_parent(int, struct khttpauth *);
enum kcgi_err	 kworker_child(int,
			const struct kvalid *, size_t, 
			const char *const *, size_t,
			unsigned int);
void	 	 kworker_fcgi_child(int, int,
			const struct kvalid *, size_t, 
			const char *const *, size_t,
			unsigned int);
enum kcgi_err	 kworker_parent(int, struct kreq *, int, size_t);

int		 fullread(int, void *, size_t, int, enum kcgi_err *);
enum kcgi_err	 fullreadword(int, char **);
enum kcgi_err	 fullreadwordsz(int, char **, size_t *);
int		 fullreadfd(int, int *, void *, size_t);
void		 fullwrite(int, const void *, size_t);
enum kcgi_err	 fullwritenoerr(int, const void *, size_t);
void		 fullwriteword(int, const char *);
int		 fullwritefd(int, int, void *, size_t);

int		 ksandbox_init_child(enum sandtype, int, int, int, int);
#if HAVE_CAPSICUM
int	 	 ksandbox_capsicum_init_child
			(enum sandtype, int, int, int, int);
#endif
#if HAVE_PLEDGE
int	 	 ksandbox_pledge_init_child(enum sandtype);
#endif
#if HAVE_SANDBOX_INIT
int	 	 ksandbox_darwin_init_child(enum sandtype);
#endif
#if HAVE_SECCOMP_FILTER
int	 	 ksandbox_seccomp_init_child(enum sandtype);
#endif
void		 kreq_free(struct kreq *);

enum kcgi_err	 kxsocketpair(int[2]);
enum kcgi_err	 kxsocketprep(int);
enum kcgi_err	 kxwaitpid(pid_t);

int		 kxasprintf(char **, const char *, ...)
			__attribute__((format(printf, 2, 3)));
int		 kxvasprintf(char **, const char *, va_list);
void		*kxcalloc(size_t, size_t);
void		*kxmalloc(size_t);
void		*kxrealloc(void *, size_t);
void		*kxreallocarray(void *, size_t, size_t);
char		*kxstrdup(const char *);

__END_DECLS

#endif
