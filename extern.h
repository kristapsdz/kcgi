/*	$Id$ */
/*
 * Copyright (c) 2014 Kristaps Dzonsons <kristaps@bsd.lv>
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
 * A worker extracts the HTTP document body and passes parsed key-value
 * pairs (and environment data) via the "sock".
 * It's sandboxed with a configuration specified by "sand", which is
 * operating-system specific (e.g., systrace, capsicum).
 */
struct	kworker {
#define	KWORKER_READ	 1
#define	KWORKER_WRITE	 0
	int	 	 sock[2];
	pid_t		 pid;
	void		*sand;
	int		 input;
};

__BEGIN_DECLS

enum kcgi_err	 khttp_input_parent(int, struct kreq *, pid_t);
void	 	 khttp_input_child(const struct kworker *, 
			const struct kvalid *, size_t, 
			const char *const *, size_t);

void		 ksandbox_free(void *);
void		*ksandbox_alloc(void);
void		 ksandbox_close(void *);
void		 ksandbox_init_child(void *, int);
void		 ksandbox_init_parent(void *, pid_t);

#ifdef HAVE_CAPSICUM
int	 	 ksandbox_capsicum_init_child(void *, int);
#endif
#ifdef HAVE_SANDBOX_INIT
int	 	 ksandbox_darwin_init_child(void *);
#endif
#ifdef HAVE_SYSTRACE
void		*ksandbox_systrace_alloc(void);
void	 	 ksandbox_systrace_close(void *);
int	 	 ksandbox_systrace_init_child(void *);
int	 	 ksandbox_systrace_init_parent(void *, pid_t);
#endif

void	 	 kworker_prep_child(struct kworker *);
void	 	 kworker_prep_parent(struct kworker *);
void	 	 kworker_free(struct kworker *);
enum kcgi_err	 kworker_init(struct kworker *);
void		 kworker_kill(struct kworker *);
enum kcgi_err	 kworker_close(struct kworker *);

/*
 * These are just wrappers over the native functions that report when
 * failure has occured.
 * They do nothing else and return the normal values.
 */
int		 xasprintf(const char *, int, 
			char **, const char *, ...);
int		 xbindpath(const char *);
int		 xvasprintf(const char *, int, 
			char **, const char *, va_list);
void		*xcalloc(const char *, int, size_t, size_t);
void		*xmalloc(const char *, int, size_t);
void		*xrealloc(const char *, int, void *, size_t);
void		*xreallocarray(const char *, 
			int, void *, size_t, size_t);
enum kcgi_err	 xsocketpair(int, int, int, int[2]);
char		*xstrdup(const char *, int, const char *);
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
