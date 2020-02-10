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

static	const char *valids[] = {
	"first.last@iana.org",
	"1234567890123456789012345678901234567890123456789012345678"
		"901234@iana.org",
	"\"first\\\"last\"@iana.org",
	"\"first@last\"@iana.org",
	"\"first\\\\last\"@iana.org",
	"x@x23456789.x23456789.x23456789.x23456789.x23456789.x23456789."
		"x23456789.x23456789.x23456789.x23456789.x23456789."
		"x23456789.x23456789.x23456789.x23456789.x23456789."
		"x23456789.x23456789.x23456789.x23456789.x23456789."
		"x23456789.x23456789.x23456789.x23456789.x2",
	"1234567890123456789012345678901234567890123456789012345678"
		"@1234567890123456789012345678901234567890123456789"
		"0123456789.123456789012345678901234567890123456789"
		"01234567890123456789.12345678901234567890123456789"
		"0123456789012345678901234567890123.iana.org",
	"first.last@[12.34.56.78]",
	"first.last@[IPv6:::12.34.56.78]",
	"first.last@[IPv6:1111:2222:3333::4444:12.34.56.78]",
	"first.last@[IPv6:1111:2222:3333:4444:5555:6666:12.34.56.78]",
	"first.last@[IPv6:::1111:2222:3333:4444:5555:6666]",
	"first.last@[IPv6:1111:2222:3333::4444:5555:6666]",
	"first.last@[IPv6:1111:2222:3333:4444:5555:6666::]",
	"first.last@[IPv6:1111:2222:3333:4444:5555:6666:7777:8888]",
	"first.last@x2345678901234567890123456789012345678901234567"
		"8901234567890123.iana.org",
	"first.last@3com.com",
	"first.last@123.iana.org",
	"\"first\\last\"@iana.org",
	"first.last@[IPv6:1111:2222:3333::4444:5555:12.34.56.78]",
	"first.last@[IPv6:1111:2222:3333::4444:5555:6666:7777]",
	"first.last@example.123",
	"first.last@com",
	"\"Abc\\@def\"@iana.org",
	"\"Fred\\ Bloggs\"@iana.org",
	"\"Joe.\\\\Blow\"@iana.org",
	"\"Abc@def\"@iana.org",
	"\"Fred Bloggs\"@iana.org",
	"user+mailbox@iana.org",
	"customer/department=shipping@iana.org",
	"$A12345@iana.org",
	"!def!xyz%abc@iana.org",
	"_somename@iana.org",
	"dclo@us.ibm.com",
	"peter.piper@iana.org",
	"\"Doug \\\"Ace\\\" L.\"@iana.org",
	"test@iana.org",
	"TEST@iana.org",
	"1234567890@iana.org",
	"test+test@iana.org",
	"test-test@iana.org",
	"t*est@iana.org",
	"+1~1+@iana.org",
	"{_test_}@iana.org",
	"\"[[ test ]]\"@iana.org",
	"test.test@iana.org",
	"\"test.test\"@iana.org",
	"test.\"test\"@iana.org",
	"\"test@test\"@iana.org",
	"test@123.123.123.x123",
	"test@123.123.123.123",
	"test@[123.123.123.123]",
	"test@example.iana.org",
	"test@example.example.iana.org",
	"\"test\\test\"@iana.org",
	"test@example",
	"\"test\\\\blah\"@iana.org",
	"\"test\\blah\"@iana.org",
	"\"test\\&#13;blah\"@iana.org",
	"\"test\\\"blah\"@iana.org",
	"customer/department@iana.org",
	"_Yosemite.Sam@iana.org",
	"~@iana.org",
	"\"Austin@Powers\"@iana.org",
	"Ima.Fool@iana.org",
	"\"Ima.Fool\"@iana.org",
	"\"Ima Fool\"@iana.org",
	"\"first\".\"last\"@iana.org",
	"\"first\".middle.\"last\"@iana.org",
	"\"first\".last@iana.org",
	"first.\"last\"@iana.org",
	"\"first\".\"middle\".\"last\"@iana.org",
	"\"first.middle\".\"last\"@iana.org",
	"\"first.middle.last\"@iana.org",
	"\"first..last\"@iana.org",
	"\"first\\\\\\\"last\"@iana.org",
	"first.\"mid\\dle\".\"last\"@iana.org",
	"Test.&#13;&#10; Folding.&#13;&#10; Whitespace@iana.org",
	"\"test&#13;&#10; blah\"@iana.org",
	"(foo)cal(bar)@(baz)iamcal.com(quux)",
	"cal@iamcal(woo).(yay)com",
	"\"foo\"(yay)@(hoopla)[1.2.3.4]",
	"cal(woo(yay)hoopla)@iamcal.com",
	"cal(foo\\@bar)@iamcal.com",
	"cal(foo\\)bar)@iamcal.com",
	"first().last@iana.org",
	"first.(&#13;&#10; middle&#13;&#10; )last@iana.org",
	"first(Welcome to&#13;&#10; the (\"wonderful\" (!)) world&#"
		"13;&#10; of email)@iana.org",
	"pete(his account)@silly.test(his host)",
	"c@(Chris's host.)public.example",
	"jdoe@machine(comment).  example",
	"1234   @   local(blah)  .machine .example",
	"first(abc.def).last@iana.org",
	"first(a\"bc.def).last@iana.org",
	"first.(\")middle.last(\")@iana.org",
	"first(abc\\(def)@iana.org",
	"first.last@x(123456789012345678901234567890123456789012345"
		"6789012345678901234567890).com",
	"a(a(b(c)d(e(f))g)h(i)j)@iana.org",
	"name.lastname@domain.com",
	"a@b",
	"a@bar.com",
	"aaa@[123.123.123.123]",
	"a@bar",
	"a-b@bar.com",
	"+@b.c",
	"+@b.com",
	"a@b.co-foo.uk",
	"\"hello my name is\"@stutter.com",
	"\"Test \\\"Fail\\\" Ing\"@iana.org",
	"valid@about.museum",
	"shaitan@my-domain.thisisminekthx",
	"foobar@192.168.0.1",
	"\"Joe\\\\Blow\"@iana.org",
	"HM2Kinsists@(that comments are allowed)this.is.ok",
	"user%uucp!path@berkeley.edu",
	"\"first(last)\"@iana.org",
	" &#13;&#10; (&#13;&#10; x &#13;&#10; ) &#13;&#10; first&#1"
		"3;&#10; ( &#13;&#10; x&#13;&#10; ) &#13;&#10; .&#1"
		"3;&#10; ( &#13;&#10; x) &#13;&#10; last &#13;&#10;"
		"	(  x &#13;&#10; ) &#13;&#10; @iana.org",
	"first.last @iana.org",
	"test. &#13;&#10; &#13;&#10; obs@syntax.com",
	"\"Unicode NULL \\␀\"@char.com",
	"cdburgess+!#$%&'*-/=?+_{}|~test@gmail.com",
	"first.last@[IPv6:::a2:a3:a4:b1:b2:b3:b4]",
	"first.last@[IPv6:a1:a2:a3:a4:b1:b2:b3::]",
	"first.last@[IPv6:::]",
	"first.last@[IPv6:::b4]",
	"first.last@[IPv6:::b3:b4]",
	"first.last@[IPv6:a1::b4]",
	"first.last@[IPv6:a1::]",
	"first.last@[IPv6:a1:a2::]",
	"first.last@[IPv6:0123:4567:89ab:cdef::]",
	"first.last@[IPv6:0123:4567:89ab:CDEF::]",
	"first.last@[IPv6:::a3:a4:b1:ffff:11.22.33.44]",
	"first.last@[IPv6:::a2:a3:a4:b1:ffff:11.22.33.44]",
	"first.last@[IPv6:a1:a2:a3:a4::11.22.33.44]",
	"first.last@[IPv6:a1:a2:a3:a4:b1::11.22.33.44]",
	"first.last@[IPv6:a1::11.22.33.44]",
	"first.last@[IPv6:a1:a2::11.22.33.44]",
	"first.last@[IPv6:0123:4567:89ab:cdef::11.22.33.44]",
	"first.last@[IPv6:0123:4567:89ab:CDEF::11.22.33.44]",
	"first.last@[IPv6:a1::b2:11.22.33.44]",
	"test@test.com",
	"test@example.com&#10;",
	"test@xn--example.com",
	"test@Bücher.ch",
	NULL
};

static	const char *invalids[] = {
	"@",
	"a",
	"a@",
	"@a",
	"aa",
	"aa@",
	"aaa@",
	"@@@@",
	"@aaa",
	"aaa",
	"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
	"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
	"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
	"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
	"a@aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
	"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
	"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
	"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
	NULL
};

int
main(int argc, char *argv[])
{
	struct kpair	 kp;
	size_t		 i;
	const char	*name = "test";

	for (i = 0; invalids[i] != NULL; i++) {
		memset(&kp, 0, sizeof(struct kpair));
		kp.key = (char *)name;
		kp.val = kstrdup(invalids[i]);
		kp.valsz = strlen(invalids[i]);
		if (kvalid_email(&kp))
			errx(EXIT_FAILURE, "failed "
				"invalid check: %s", invalids[i]);
		free(kp.val);
	}

	for (i = 0; valids[i] != NULL; i++) {
		memset(&kp, 0, sizeof(struct kpair));
		kp.key = (char *)name;
		kp.val = kstrdup(valids[i]);
		kp.valsz = strlen(valids[i]);
		if (!kvalid_email(&kp))
			errx(EXIT_FAILURE, "failed "
				"valid check: %s", valids[i]);
		free(kp.val);
	}

	return EXIT_SUCCESS;
}
