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
#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif
#include <sys/stat.h>

#include <fcntl.h>
#include <paths.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include "../kcgi.h"
#include "../extern.h"

int
main(int argc, char *argv[])
{
	int	 	 fdout, fdin;
	enum kcgi_err	 kerr;
	struct stat	 st;
	char		 buf[1024];

	if (2 != argc)
		return(EXIT_FAILURE);

	if (-1 == (fdout = open(_PATH_DEVNULL, O_RDWR, 0))) {
		perror(_PATH_DEVNULL);
		return(EXIT_FAILURE);
	} else if (-1 == (fdin = open(argv[1], O_RDONLY, 0))) {
		perror(argv[1]);
		close(fdout);
		return(EXIT_FAILURE);
	} else if (-1 == fstat(fdin, &st)) {
		perror(argv[1]);
		close(fdout);
		close(fdin);
		return(EXIT_FAILURE);
	} else if (KCGI_OK != kxsocketprep(fdin)) {
		perror(argv[1]);
		close(fdout);
		close(fdin);
		return(EXIT_FAILURE);
	} else if (-1 == dup2(fdin, STDIN_FILENO)) {
		perror(argv[1]);
		close(fdout);
		close(fdin);
		return(EXIT_FAILURE);
	}

	snprintf(buf, sizeof(buf), "%llu", 
		(unsigned long long)st.st_size);
	setenv("CONTENT_TYPE", "application/x-www-form-urlencoded", 1);
	setenv("REQUEST_METHOD", "post", 1);
	setenv("CONTENT_LENGTH", buf, 1);
	kerr = kworker_child(fdout, NULL, 0, kmimetypes, KMIME__MAX, 0);
	close(fdin);
	close(fdout);
	return(KCGI_OK == kerr ? EXIT_SUCCESS : EXIT_FAILURE);
}
