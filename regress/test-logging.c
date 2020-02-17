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
 * These are all the components from the log message.
 */
struct line {
	const char *addr;
	const char *ident;
	const char *date;
	const char *level;
	const char *umsg;
	const char *errmsg;
};

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

	if (!WIFEXITED(st) || st != EXIT_SUCCESS)
		errx(EXIT_FAILURE, "child failure (%d)", WIFEXITED(st));

	/* Now run the parent test component. */

	if ((f = fdopen(fd, "r")) == NULL)
		err(EXIT_FAILURE, "%s", sfn);

	rc = parent(f);
	fclose(f);
	return rc;
}

/*
 * Parse to the end of the current word from a log message and NUL
 * terminate it.
 * The word either ends on a white-space (or NUL) boundary or the given
 * endchar if not NUL.
 * Returns the next word position, which may be the end of the buffer,
 * or NULL if the buffer could not be terminated.
 */
static char *
parse_word(char *msg, char endchar)
{

	if (endchar != '\0')
		while (*msg != '\0' && *msg != endchar)
			msg++;
	else
		while (*msg != '\0' && !isspace((unsigned char)*msg))
			msg++;
	if (*msg == '\0')
		return NULL;
	*msg++ = '\0';
	while (*msg != '\0' && isspace((unsigned char)*msg))
		msg++;
	return msg;
}

/*
 * Parse all components of the line.
 * The error message may be NULL.
 * Return zero on failure, non-zero on success.
 */
static int
parse_line(char *msg, struct line *p)
{
	char	*tmp;

	p->addr = msg;
	if ((msg = parse_word(msg, '\0')) == NULL)
		return 0;
	p->ident = msg;
	if ((msg = parse_word(msg, '\0')) == NULL)
		return 0;
	if ('[' != *msg)
		return 0;
	msg++;
	p->date = msg;
	if ((msg = parse_word(msg, ']')) == NULL)
		return 0;
	p->level = msg;
	if ((msg = parse_word(msg, '\0')) == NULL)
		return 0;
	p->umsg = msg;
	if ((tmp = strrchr(msg, ':')) != NULL) {
		*tmp++ = '\0';
		p->errmsg = ++tmp;
	} else {
		parse_word(msg, '\0');
		p->errmsg = NULL;
	}

	return 1;
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
	struct line	 line;
	int		 rc = 0;

	buf = get_line(f);
	if (!parse_line(buf, &line))
		goto out;
	if (strcmp(line.addr, "-") ||
	    strcmp(line.ident, "-") ||
	    strcmp(line.level, "WARN") ||
	    strcmp(line.umsg, "foo") ||
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
	struct line	 line;
	int		 rc = 0;

	buf = get_line(f);
	if (!parse_line(buf, &line))
		goto out;
	if (strcmp(line.addr, "-") ||
	    strcmp(line.ident, "-") ||
	    strcmp(line.level, "WARN") ||
	    strcmp(line.umsg, "foo") ||
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
	struct line	 line;
	int		 rc = 0;

	buf = get_line(f);
	if (!parse_line(buf, &line))
		goto out;
	if (strcmp(line.addr, "-") ||
	    strcmp(line.ident, "foo") ||
	    strcmp(line.level, "WARN") ||
	    strcmp(line.umsg, "-") ||
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
	struct line	 line;
	int		 rc = 0;

	buf = get_line(f);
	if (!parse_line(buf, &line))
		goto out;
	if (strcmp(line.addr, "-") ||
	    strcmp(line.ident, "-") ||
	    strcmp(line.level, "WARN") ||
	    strcmp(line.umsg, "-") ||
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
