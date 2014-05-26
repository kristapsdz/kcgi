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
#include <sys/queue.h>

#include <assert.h>
#include <ctype.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <util.h>

enum	mimeop {
	OP_ENUM_KMIME, /* enum kmime */
	OP_ARRAY_KMIMETYPES, /* kmimetypes[KMIME__MAX] */
	OP_ARRAY_SUFFIXMAP, /* suffixmap[] */
	OP_ARRAY_KSUFFIXES /* ksuffixes[KMIME__MAX] */
};

/*
 * A single suffix parsed per MIME type.
 */
struct	suffix {
	char	 	*suffix;
	TAILQ_ENTRY(suffix) entry;
};

TAILQ_HEAD(suffixq, suffix);

/*
 * A single MIME type with an array of suffixes.
 */
struct	mime {
	char	 	*enumname;
	char	 	*key;
	char		*val; 
	struct suffixq	 suffixes;
	TAILQ_ENTRY(mime) entry;
};

TAILQ_HEAD(mimeq, mime);

static char *
xstrdup(const char *cp)
{
	char	*p;

	if (NULL != (p = strdup(cp)))
		return(p);

	perror(NULL);
	exit(EXIT_FAILURE);
}

static void *
xmalloc(size_t nm)
{
	void	*p;

	if (NULL != (p = malloc(nm)))
		return(p);

	perror(NULL);
	exit(EXIT_FAILURE);
}

/*
 * Use this utility to create MIME type maps.
 * I only include the most common MIME types in kcgi to prevent bloat.
 * But really it could handle all of them (it'd be a lot of static space).
 *
 * Invocation flags:
 *    -e: print "enum kmime"
 *    -k: print "kmimetypes[KMIME__MAX]"
 *    -K: print "ksuffixes[KMIME__MAX]"
 *    -m: print "suffixmap[]"
 *
 * It accepts, on stdin, the same mime.types format used by Apache to
 * store its MIME database.
 */
int
main(int argc, char *argv[])
{
	char		*ln, *key, *val, *cp;
	size_t		 len, lnn, keysz, i, j;
	int		 c;
	struct suffix	*s;
	struct mimeq	 q;
	enum mimeop	 op;
	struct mime	*p;

	op = OP_ENUM_KMIME;
	while (-1 != (c = getopt(argc, argv, "eksK")))
		switch (c) {
		case ('e'):
			op = OP_ENUM_KMIME;
			break;
		case ('k'):
			op = OP_ARRAY_KMIMETYPES;
			break;
		case ('K'):
			op = OP_ARRAY_KSUFFIXES;
			break;
		case ('s'):
			op = OP_ARRAY_SUFFIXMAP;
			break;
		default:
			return(EXIT_FAILURE);
		}

	TAILQ_INIT(&q);

	/*
	 * Parse out all non-pound-escaped lines.
	 */
	while (NULL != (cp = fparseln(stdin, &len, &lnn, NULL, 0))) {
		ln = cp;
		if ('\0' == *cp) {
			free(ln);
			ln = NULL;
			continue;
		}
		/*
		 * Start with the lowercased MIME type.
		 * It will end on the first whitespace.
		 */
		key = cp;
		while ('\0' != *cp && ! isspace((int)*cp))
			cp++;
		if ('\0' == *cp)
			break;
		if (0 == (keysz = cp - key))
			break;
		*cp++ = '\0';
		/*
		 * Now continue on past the trailing whitespace til the
		 * first suffix entry.
		 */
		while (isspace((int)*cp))
			cp++;
		if ('\0' == *cp)
			break;

		val = cp;
		len = keysz + 6 + 1;
		
		/*
		 * Create the MIME entry.
		 */
		p = xmalloc(sizeof(struct mime));
		p->key = xstrdup(key);
		p->val = xstrdup(val);
		p->enumname = xmalloc(len);
		TAILQ_INIT(&p->suffixes);

		/* Construct its enum name. */
		j = strlcpy(p->enumname, "KMIME_", len);
		assert(j < len);
		for (i = 0; i < keysz; i++, j++)
			if (isalpha(key[i]))
				p->enumname[j] = toupper((int)key[i]);
			else if (isdigit(key[i]))
				p->enumname[j] = key[i];
			else
				p->enumname[j] = '_';
		assert(j == len - 1);
		p->enumname[j] = '\0';

		/* Fill in its suffixes. */
		val = p->val;
		while ('\0' != *val) {
			s = xmalloc(sizeof(struct suffix));
			s->suffix = val;
			TAILQ_INSERT_TAIL(&p->suffixes, s, entry);
			while ('\0' != *val && ! isspace((int)*val))
				val++;
			if ('\0' != *val)
				*val++ = '\0';
			while ('\0' != *val && isspace((int)*val))
				val++;
		}

		TAILQ_INSERT_TAIL(&q, p, entry);
		free(ln);
		ln = NULL;
	}

	free(ln);

	switch (op) {
	case (OP_ENUM_KMIME):
		puts("enum\tkmime {");
		break;
	case (OP_ARRAY_KMIMETYPES):
		puts("const char *const kmimetypes[KMIME__MAX] = {");
		break;
	case (OP_ARRAY_SUFFIXMAP):
		puts("static const struct mimemap suffixmap[] = {");
		break;
	case (OP_ARRAY_KSUFFIXES):
		puts("const char *const ksuffixes[KMIME__MAX] = {");
		break;
	}

	TAILQ_FOREACH(p, &q, entry)
		switch (op) {
		case (OP_ENUM_KMIME):
			printf("\t%s,\n", p->enumname);
			break;
		case (OP_ARRAY_KMIMETYPES):
			printf("\t\"%s\", /* %s */\n",
				p->key, p->enumname);
			break;
		case (OP_ARRAY_SUFFIXMAP):
			TAILQ_FOREACH(s, &p->suffixes, entry)
				printf("\t{ \"%s\", %s },\n",
					s->suffix, p->enumname);
			break;
		case (OP_ARRAY_KSUFFIXES):
			printf("\t\"%s\", /* %s */\n",
				TAILQ_FIRST(&p->suffixes)->suffix,
				p->enumname);
			break;
		}

	if (OP_ENUM_KMIME == op)
		puts("\tKMIME__MAX");

	puts("};");

	while ( ! TAILQ_EMPTY(&q)) {
		p = TAILQ_FIRST(&q);
		TAILQ_REMOVE(&q, p, entry);
		while ( ! TAILQ_EMPTY(&p->suffixes)) {
			s = TAILQ_FIRST(&p->suffixes);
			TAILQ_REMOVE(&p->suffixes, s, entry);
			free(s);
		}
		free(p->key);
		free(p->val);
		free(p->enumname);
		free(p);
	}

	return(NULL == ln ? EXIT_SUCCESS : EXIT_FAILURE);
}
