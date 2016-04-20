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
#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#include <sys/stat.h>

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <curl/curl.h>

#include "../kcgi.h"
#include "regress.h"

static int
parent(CURL *curl)
{
	struct curl_httppost	*post, *last;
	char 		 	 namebuffer[] = "name buffer";
	size_t			 namelength, htmlbufferlength, record_length;
	char 			 buffer[] = "test buffer";
	char 			 htmlbuffer[] = "<HTML>test buffer</HTML>";
	struct curl_forms 	 forms[3];
	char 			 file1[] = "kcgi.c";
	char 			 file2[] = "kcgi.h";
	char 			 record[] = "asdfasdf";
	int			 rc;

	/*
	 * Add null character into htmlbuffer, to demonstrate that
	 * transfers of buffers containing null characters actually
	 * work.
	 */
	namelength = strlen(namebuffer);
	htmlbufferlength = strlen(htmlbuffer);
	record_length = strlen(htmlbuffer);
	htmlbuffer[8] = '\0';
	post = last = NULL;

	/* Add simple name/content section */
	curl_formadd(&post, &last, CURLFORM_COPYNAME,
		"name", CURLFORM_COPYCONTENTS, "content", CURLFORM_END);

	/* Add simple name/content/contenttype section */
	curl_formadd(&post, &last, CURLFORM_COPYNAME,
		"htmlcode", CURLFORM_COPYCONTENTS, "<HTML></HTML>",
		CURLFORM_CONTENTTYPE, "text/html", CURLFORM_END);

	/* Add name/ptrcontent section */
	curl_formadd(&post, &last, CURLFORM_COPYNAME,
		"name_for_ptrcontent", CURLFORM_PTRCONTENTS,
		buffer, CURLFORM_END);

	/* Add ptrname/ptrcontent section */
	curl_formadd(&post, &last, CURLFORM_PTRNAME,
		namebuffer, CURLFORM_PTRCONTENTS, buffer,
		CURLFORM_NAMELENGTH, namelength, CURLFORM_END);

	/* Add name/ptrcontent/contenttype section */
	curl_formadd(&post, &last, CURLFORM_COPYNAME,
		"html_code_with_hole", CURLFORM_PTRCONTENTS,
		htmlbuffer, CURLFORM_CONTENTSLENGTH,
		htmlbufferlength, CURLFORM_CONTENTTYPE,
		"text/html", CURLFORM_END);

	/* Add simple file section */
	curl_formadd(&post, &last, CURLFORM_COPYNAME,
		"picture", CURLFORM_FILE, "kcgi.c",
		CURLFORM_END);

	 /* Add file/contenttype section */
	curl_formadd(&post, &last, CURLFORM_COPYNAME,
		"picture", CURLFORM_FILE, "kcgi.c",
		CURLFORM_CONTENTTYPE, "text/x-c", CURLFORM_END);

	/* Add two file section */
	curl_formadd(&post, &last, CURLFORM_COPYNAME,
		"pictures", CURLFORM_FILE, "kcgi.c",
		CURLFORM_FILE, "kcgi.h", CURLFORM_END);

	/* Add two file section using CURLFORM_ARRAY */
	forms[0].option = CURLFORM_FILE;
	forms[0].value = file1;
	forms[1].option = CURLFORM_FILE;
	forms[1].value = file2;
	forms[2].option = CURLFORM_END;

	/* Add a buffer to upload */
	curl_formadd(&post, &last, CURLFORM_COPYNAME,
		"name", CURLFORM_BUFFER, "data",
		CURLFORM_BUFFERPTR, record,
		CURLFORM_BUFFERLENGTH, record_length, CURLFORM_END);

	/* no option needed for the end marker */
	curl_formadd(&post, &last, CURLFORM_COPYNAME, "pictures",
		CURLFORM_ARRAY, forms, CURLFORM_END);

	/* Add the content of a file as a normal post text value */
	curl_formadd(&post, &last, CURLFORM_COPYNAME, "filecontent",
		CURLFORM_FILECONTENT, "Makefile", CURLFORM_END);

	/* Set the form info */
	curl_easy_setopt(curl, CURLOPT_HTTPPOST, post);
	curl_easy_setopt(curl, CURLOPT_URL,
		"http://localhost:17123/");
	rc = curl_easy_perform(curl);
	curl_formfree(post);
	return(CURLE_OK == rc);
}

static int
child(void)
{
	struct kreq	 r;
	const char 	*page = "index";
	size_t		 i;
	struct stat	 st;

	if (KCGI_OK != khttp_parse(&r, NULL, 0, &page, 1, 0))
		return(0);

	/* TODO: check individual ones */
	khttp_head(&r, kresps[KRESP_STATUS],
		"%s", khttps[KHTTP_200]);
	khttp_head(&r, kresps[KRESP_CONTENT_TYPE],
		"%s", kmimetypes[KMIME_TEXT_HTML]);
	khttp_body(&r);
	for (i = 0; i < r.fieldsz; i++) {
		if (strcmp(r.fields[i].key, "picture"))
			continue;
		if (NULL == r.fields[i].file)
			return(0);
		if (-1 == stat(r.fields[i].file, &st)) {
			perror(r.fields[i].file);
			return(0);
		} else if ((size_t)st.st_size != r.fields[i].valsz)
			return(0);
	}
	khttp_free(&r);
	return(1);
}

int
main(int argc, char *argv[])
{

	return(regress_cgi(parent, child) ? EXIT_SUCCESS : EXIT_FAILURE);
}
