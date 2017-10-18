/*	$Id$ */
/*
 * Copyright (c) 2015 Kristaps Dzonsons <kristaps@bsd.lv>
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
#include "../config.h"

#include <sys/mman.h>
#include <sys/stat.h>

#include <assert.h>
#include <fcntl.h>
#include <paths.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include "../kcgi.h"

static int
templatex_write(const char *dat, size_t sz, void *arg)
{

	fwrite(dat, 1, sz, stdout);
	return(1);
}

static int
template_proc(size_t sz, void *arg)
{

	assert(sz < 10);
	printf("[%zu]", sz);
	return(1);
}

int
main(int argc, char *argv[])
{
	int	 	  c, fdin;
	struct stat	  st;
	char		 *buf;
	size_t		  bufsz;
	struct ktemplatex x;
	struct ktemplate  t;
	const char *const keys[] = {
		"a", "b", "c", "d", "e", 
		"ff", "gg", "hh", "ii", "jj" };

	if (2 != argc)
		return(EXIT_FAILURE);

	if (-1 == (fdin = open(argv[1], O_RDONLY, 0))) {
		perror(argv[1]);
		return(EXIT_FAILURE);
	} else if (-1 == fstat(fdin, &st)) {
		perror(argv[1]);
		close(fdin);
		return(EXIT_FAILURE);
	}

	bufsz = (size_t)st.st_size;
	buf = mmap(NULL, bufsz, PROT_READ, MAP_SHARED, fdin, 0);

	if (MAP_FAILED == buf) {
		perror(argv[1]);
		close(fdin);
		return(EXIT_FAILURE);
	}

	memset(&t, 0, sizeof(struct ktemplate));
	memset(&x, 0, sizeof(struct ktemplatex));

	t.key = keys;
	t.keysz = 10;
	t.cb = template_proc;
	x.writer = templatex_write;
	c = khttp_templatex_buf(&t, buf, bufsz, &x, NULL);

	munmap(buf, bufsz);
	close(fdin);
	return(c ? EXIT_SUCCESS : EXIT_FAILURE);
}
