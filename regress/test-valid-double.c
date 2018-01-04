/*	$Id$ */
/*
 * Copyright (c) 2017 Kristaps Dzonsons <kristaps@bsd.lv>
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

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../kcgi.h"

int
main(int argc, char *argv[])
{
	struct kpair	 kp;
	char		 buf[32];

	memset(&kp, 0, sizeof(struct kpair));
	kp.val = buf;

	kp.valsz = strlcpy(buf, "0.0", sizeof(buf));
	if ( ! kvalid_double(&kp)) {
		printf("[%s]\n", buf);
		return(EXIT_FAILURE);
	} else if (kp.parsed.d != 0.0) {
		printf("%g != %g\n", kp.parsed.d, 0.0);
		return(EXIT_FAILURE);
	}

	kp.valsz = strlcpy(buf, "  0.0", sizeof(buf));
	if ( ! kvalid_double(&kp)) {
		printf("[%s]\n", buf);
		return(EXIT_FAILURE);
	} else if (kp.parsed.d != 0.0) {
		printf("%g != %g\n", kp.parsed.d, 0.0);
		return(EXIT_FAILURE);
	}

	kp.valsz = strlcpy(buf, "0.0    ", sizeof(buf));
	if ( ! kvalid_double(&kp)) {
		printf("[%s]\n", buf);
		return(EXIT_FAILURE);
	} else if (kp.parsed.d != 0.0) {
		printf("%g != %g\n", kp.parsed.d, 0.0);
		return(EXIT_FAILURE);
	}

	kp.valsz = strlcpy(buf, "a0.0", sizeof(buf));
	if (kvalid_double(&kp)) {
		printf("[%s]\n", buf);
		return(EXIT_FAILURE);
	} 

	kp.valsz = strlcpy(buf, "0.0a", sizeof(buf));
	if (kvalid_double(&kp)) {
		printf("[%s]\n", buf);
		return(EXIT_FAILURE);
	} 

	kp.valsz = strlcpy(buf, "", sizeof(buf));
	if (kvalid_double(&kp)) {
		printf("[%s]\n", buf);
		return(EXIT_FAILURE);
	} 

	kp.valsz = strlcpy(buf, "   ", sizeof(buf));
	if (kvalid_double(&kp)) {
		printf("[%s]\n", buf);
		return(EXIT_FAILURE);
	} 

	return(EXIT_SUCCESS);
}
