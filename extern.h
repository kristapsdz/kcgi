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

__BEGIN_DECLS

int	 khttp_input_parent(int fd, struct kreq *r, pid_t pid);
void	 khttp_input_child(int fd, const struct kvalid *keys, 
		size_t keysz, const char *const *mimes, size_t mimesz);

void	 ksandbox_free(void *arg);
void	*ksandbox_alloc(void);
void	 ksandbox_close(void *arg, pid_t pid);
void	 ksandbox_init_child(void *arg);
void	 ksandbox_init_parent(void *arg, pid_t pid);

#ifdef HAVE_SANDBOX_INIT
int	 ksandbox_darwin_init_child(void *arg);
#endif
#ifdef HAVE_SYSTRACE
void	*ksandbox_systrace_alloc(void);
void	 ksandbox_systrace_close(void *arg);
int	 ksandbox_systrace_init_child(void *arg);
int	 ksandbox_systrace_init_parent(void *arg, pid_t child);
#endif

/*
 * These are just wrappers over the native functions that report when
 * failure has occured.
 * They do nothing else and return the normal values.
 */
int	 xasprintf(const char *file, int line, 
		char **p, const char *fmt, ...);
int	 xvasprintf(const char *file, int line, 
		char **p, const char *fmt, va_list ap);
void	*xcalloc(const char *file, int line, size_t nm, size_t sz);
void	*xmalloc(const char *file, int line, size_t sz);
void	*xrealloc(const char *file, int line, void *p, size_t sz);
void	*xreallocarray(const char *file, 
		int line, void *p, size_t nm, size_t sz);
char	*xstrdup(const char *file, int line, const char *p);
void	 xwarn(const char *file, int line, const char *fmt, ...);
void	 xwarnx(const char *file, int line, const char *fmt, ...);
#define	 XASPRINTF(_p, ...) \
 	 xasprintf(__FILE__, __LINE__, (_p), __VA_ARGS__)
#define	 XVASPRINTF(_p, _fmt, _va) \
 	 xvasprintf(__FILE__, __LINE__, (_p), _fmt, _va)
#define	 XCALLOC(_nm, _sz) \
 	 xcalloc(__FILE__, __LINE__, (_nm), (_sz))
#define	 XMALLOC(_sz) \
 	 xmalloc(__FILE__, __LINE__, (_sz))
#define	 XREALLOC(_p, _sz) \
 	 xrealloc(__FILE__, __LINE__, (_p), (_sz))
#define	 XREALLOCARRAY(_p, _nm, _sz) \
	 xreallocarray(__FILE__, __LINE__, (_p), (_nm), (_sz))
#define	 XSTRDUP(_p) \
 	 xstrdup(__FILE__, __LINE__, (_p))
#define	 XWARN(...) \
 	 xwarn(__FILE__, __LINE__, __VA_ARGS__)
#define	 XWARNX(...) \
 	 xwarnx(__FILE__, __LINE__, __VA_ARGS__)

__END_DECLS

#endif
