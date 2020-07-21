/*	$Id$ */
/*
 * Copyright (c) 2020 Kristaps Dzonsons <kristaps@bsd.lv>
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

#include <sys/wait.h>

#include <ctype.h>
#include <errno.h>
#if HAVE_ERR
# include <err.h>
#endif
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <curl/curl.h>

#include "../kcgi.h"
#include "regress.h"

#define	TEMPL "/tmp/test-logging.XXXXXXXXXX"

/*
 * Actually run the test pair of child and parent.
 * Child must log whatever it wants to log and parent processes the log
 * messages (if any).
 * Return zero on failure, non-zero on success.
 */
static int
test_runner(int (*child)(void), int (*parent)(FILE *))
{
	char	 sfn[32];
	int	 fd, st, rc;
	FILE	*f;
	pid_t	 pid;

	/* Create the temporary file we'll use. */

	strlcpy(sfn, TEMPL, sizeof(sfn));
	if ((fd = mkstemp(sfn)) == -1)
		err(EXIT_FAILURE, "%s", sfn);

	/* Start the child that will emit the message. */

	if ((pid = fork()) == -1)
		err(EXIT_FAILURE, "fork");

	if (pid == 0) {
		close(fd);
		if (!kutil_openlog(sfn))
			errx(EXIT_FAILURE, "kutil_openlog");
		exit(child() ? EXIT_SUCCESS : EXIT_FAILURE);
	}

	/* Wait for child to do its printing, then cleanup. */

	if (waitpid(pid, &st, 0) == -1) {
		warn("waitpid");
		unlink(sfn);
		exit(EXIT_FAILURE);
	}
	unlink(sfn);

	/* Make sure it exited properly. */

	if (!WIFEXITED(st) || WEXITSTATUS(st) != EXIT_SUCCESS)
		errx(EXIT_FAILURE, "child failure (%d)", WIFEXITED(st));

	/* Now run the parent test component. */

	if ((f = fdopen(fd, "r")) == NULL)
		err(EXIT_FAILURE, "%s", sfn);

	rc = parent(f);
	fclose(f);
	return rc;
}

/*
 * Wrap around getline().
 */
static char *
get_line(FILE *f)
{
           char		*line = NULL;
           size_t	 linesize = 0;
           ssize_t	 linelen;

           if ((linelen = getline(&line, &linesize, f)) == -1)
		   errx(EXIT_FAILURE, "getline");
	   return line;
}

static int
child4(void)
{

	errno = EINVAL;
	kutil_warn(NULL, NULL, "%s", "foo");
	return 1;
}

static int
parent4(FILE *f)
{
	char		*buf;
	struct log_line	 line;
	int		 rc = 0;

	buf = get_line(f);
	if (!log_line_parse(buf, &line))
		goto out;
	if (strcmp(line.addr, "-") ||
	    strcmp(line.ident, "-") ||
	    strcmp(line.level, "WARN") ||
	    strcmp(line.umsg, "foo: Invalid argument\n") ||
	    line.errmsg == NULL)
		goto out;
	rc = 1;
out:
	free(buf);
	return rc;
}

static int
child3(void)
{

	kutil_warnx(NULL, NULL, "%s", "foo");
	return 1;
}

static int
parent3(FILE *f)
{
	char		*buf;
	struct log_line	 line;
	int		 rc = 0;

	buf = get_line(f);
	if (!log_line_parse(buf, &line))
		goto out;
	if (strcmp(line.addr, "-") ||
	    strcmp(line.ident, "-") ||
	    strcmp(line.level, "WARN") ||
	    strcmp(line.umsg, "foo\n") ||
	    line.errmsg != NULL)
		goto out;
	rc = 1;
out:
	free(buf);
	return rc;
}

static int
child2(void)
{

	kutil_warnx(NULL, "foo", NULL);
	return 1;
}

static int
parent2(FILE *f)
{
	char		*buf;
	struct log_line	 line;
	int		 rc = 0;

	buf = get_line(f);
	if (!log_line_parse(buf, &line))
		goto out;
	if (strcmp(line.addr, "-") ||
	    strcmp(line.ident, "foo") ||
	    strcmp(line.level, "WARN") ||
	    strcmp(line.umsg, "-\n") ||
	    line.errmsg != NULL)
		goto out;
	rc = 1;
out:
	free(buf);
	return rc;
}

static int
child1(void)
{

	kutil_warnx(NULL, NULL, NULL);
	return 1;
}

static int
parent1(FILE *f)
{
	char		*buf;
	struct log_line	 line;
	int		 rc = 0;

	buf = get_line(f);
	if (!log_line_parse(buf, &line))
		goto out;
	if (strcmp(line.addr, "-") ||
	    strcmp(line.ident, "-") ||
	    strcmp(line.level, "WARN") ||
	    strcmp(line.umsg, "-\n") ||
	    line.errmsg != NULL)
		goto out;
	rc = 1;
out:
	free(buf);
	return rc;
}

int
main(void)
{

	if (!test_runner(child1, parent1))
		return EXIT_FAILURE;
	if (!test_runner(child2, parent2))
		return EXIT_FAILURE;
	if (!test_runner(child3, parent3))
		return EXIT_FAILURE;
	if (!test_runner(child4, parent4))
		return EXIT_FAILURE;
	return EXIT_SUCCESS;
}
