/*	$Id$ */
/*
 * Copyright (c) 2014, 2015 Kristaps Dzonsons <kristaps@bsd.lv>
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
	SAND_CONTROL
};

#define KWORKER_PARENT  1
#define KWORKER_CHILD	0


__BEGIN_DECLS

struct kdata	*kdata_alloc(int, int, uint16_t);
void		 kdata_body(struct kdata *);
int		 kdata_compress(struct kdata *);
void		 kdata_free(struct kdata *, int);

void		 kworker_auth_child(int, const char *);
enum kcgi_err	 kworker_auth_parent(int, struct khttpauth *);
void	 	 kworker_child(int,
			const struct kvalid *, size_t, 
			const char *const *, size_t);
void	 	 kworker_fcgi_child(int, int,
			const struct kvalid *, size_t, 
			const char *const *, size_t);
enum kcgi_err	 kworker_parent(int, struct kreq *);

int		 fulldiscard(int, size_t, enum kcgi_err *);
int		 fullread(int, void *, size_t, int, enum kcgi_err *);
enum kcgi_err	 fullreadword(int, char **);
int		 fullreadfd(int, int *, void *, size_t);
void		 fullwrite(int, const void *, size_t);
void		 fullwritenoerr(int, const void *, size_t);
void		 fullwriteword(int, const char *);
int		 fullwritefd(int, int, void *, size_t);

int		 ksandbox_alloc(void **);
void		 ksandbox_close(void *);
void		 ksandbox_free(void *);
int		 ksandbox_init_child(void *, enum sandtype, int, int);
int		 ksandbox_init_parent(void *, enum sandtype, pid_t);
#ifdef HAVE_CAPSICUM
int	 	 ksandbox_capsicum_init_child(void *, enum sandtype, int, int);
#endif
#ifdef HAVE_PLEDGE
int	 	 ksandbox_pledge_init_child(void *, enum sandtype);
#endif
#ifdef HAVE_SANDBOX_INIT
int	 	 ksandbox_darwin_init_child(void *, enum sandtype);
#endif
#ifdef HAVE_SYSTRACE
void		*ksandbox_systrace_alloc(void);
void	 	 ksandbox_systrace_close(void *);
int	 	 ksandbox_systrace_init_child(void *, enum sandtype);
int	 	 ksandbox_systrace_init_parent(void *, enum sandtype, pid_t);
#endif
#ifdef HAVE_SECCOMP_FILTER
int	 	 ksandbox_seccomp_init_child(void *, enum sandtype);
#endif

/*
 * These are just wrappers over the native functions that report when
 * failure has occured.
 * They do nothing else and return the normal values.
 */
int		 xasprintf(const char *, int, 
			char **, const char *, ...);
int		 xvasprintf(const char *, int, 
			char **, const char *, va_list);

void		*xcalloc(const char *, int, size_t, size_t);
void		*xmalloc(const char *, int, size_t);
void		*xrealloc(const char *, int, void *, size_t);
void		*xreallocarray(const char *, 
			int, void *, size_t, size_t);
enum kcgi_err	 xsocketpair(int, int, int, int[2]);
enum kcgi_err	 xsocketprep(int);
char		*xstrdup(const char *, int, const char *);
enum kcgi_err	 xwaitpid(pid_t);
void		 xwarn(const char *, int, const char *, ...);
void		 xwarnx(const char *, int, const char *, ...);
#define		 XASPRINTF(_p, ...) \
		 xasprintf(__FILE__, __LINE__, (_p), __VA_ARGS__)
#define		 XVASPRINTF(_p, _fmt, _va) \
		 xvasprintf(__FILE__, __LINE__, (_p), _fmt, _va)
#define		 XCALLOC(_nm, _sz) \
		 xcalloc(__FILE__, __LINE__, (_nm), (_sz))
#define		 XMALLOC(_sz) \
		 xmalloc(__FILE__, __LINE__, (_sz))
#define		 XREALLOC(_p, _sz) \
		 xrealloc(__FILE__, __LINE__, (_p), (_sz))
#define		 XREALLOCARRAY(_p, _nm, _sz) \
		 xreallocarray(__FILE__, __LINE__, (_p), (_nm), (_sz))
#define		 XSTRDUP(_p) \
		 xstrdup(__FILE__, __LINE__, (_p))
#define		 XWARN(...) \
		 xwarn(__FILE__, __LINE__, __VA_ARGS__)
#define		 XWARNX(...) \
		 xwarnx(__FILE__, __LINE__, __VA_ARGS__)

__END_DECLS

#endif
