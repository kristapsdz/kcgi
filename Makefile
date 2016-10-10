.SUFFIXES: .3 .3.html .8 .8.html .dot .svg .gnuplot .png .xml .html
.PHONY: regress

# Comment if you don't need statically linked.
# This is only for the sample program!
STATIC 		 = -static

# You probably don't need to change anything else...

CFLAGS 		+= -g -W -Wall -Wstrict-prototypes -Wno-unused-parameter -Wwrite-strings -DHAVE_CONFIG_H
#CFLAGS		+= -DSANDBOX_SECCOMP_DEBUG
PREFIX 		 = /usr/local
DATADIR 	 = $(PREFIX)/share/kcgi
TUTORIALXMLS	 = tutorial0.xml \
		   tutorial1.xml \
		   tutorial2.xml \
		   tutorial3.xml
TUTORIALHTMLS	 = tutorial0.html \
		   tutorial1.html \
		   tutorial2.html \
		   tutorial3.html
SBLGS		 = archive.html \
		   index.html
MAN3DIR	 	 = $(PREFIX)/man/man3
MAN8DIR	 	 = $(PREFIX)/man/man8
SBINDIR		 = $(PREFIX)/sbin
LIBDIR 		 = $(PREFIX)/lib
INCLUDEDIR 	 = $(PREFIX)/include
VERSION 	 = 0.9.0
LIBCONFIGOBJS	 = compat-memmem.o \
		   compat-reallocarray.o \
		   compat-strlcat.o \
		   compat-strlcpy.o \
		   compat-strtonum.o
LIBOBJS 	 = auth.o \
		   child.o \
		   datetime.o \
		   fcgi.o \
		   httpauth.o \
		   kcgi.o \
		   md5.o \
		   output.o \
		   parent.o \
		   sandbox.o \
		   sandbox-capsicum.o \
		   sandbox-darwin.o \
		   sandbox-pledge.o \
		   sandbox-seccomp-filter.o \
		   sandbox-systrace.o \
		   wrappers.o
HTMLS		 = man/kcgi.3.html \
		   man/kcgihtml.3.html \
		   man/kcgijson.3.html \
		   man/kcgiregress.3.html \
		   man/kcgixml.3.html \
		   man/kfcgi.8.html \
		   man/khttp_body.3.html \
		   man/khttp_fcgi_free.3.html \
		   man/khttp_fcgi_init.3.html \
		   man/khttp_fcgi_parse.3.html \
		   man/khttp_fcgi_test.3.html \
		   man/khttp_free.3.html \
		   man/khttp_head.3.html \
		   man/khttp_parse.3.html \
		   man/khttp_template.3.html \
		   man/khttp_write.3.html \
		   man/khttpbasic_validate.3.html \
		   man/khttpdigest_validate.3.html \
		   man/kmalloc.3.html \
		   man/kutil_urlencode.3.html \
		   man/kutil_epoch2str.3.html \
		   man/kvalid_string.3.html
TESTS 		 = test-arc4random.c \
		   test-memmem.c \
      		   test-reallocarray.c \
      		   test-sandbox_init.c \
      		   test-capsicum.c \
      		   test-pledge.c \
      		   test-strlcat.c \
      		   test-strlcpy.c \
      		   test-strtonum.c \
      		   test-seccomp-filter.c \
      		   test-systrace.c \
      		   test-zlib.c
MAN3S		 = man/kcgi.3 \
		   man/kcgihtml.3 \
		   man/kcgijson.3 \
		   man/kcgiregress.3 \
		   man/kcgixml.3 \
		   man/khttp_body.3 \
		   man/khttp_fcgi_free.3 \
		   man/khttp_fcgi_init.3 \
		   man/khttp_fcgi_parse.3 \
		   man/khttp_fcgi_test.3 \
		   man/khttp_free.3 \
		   man/khttp_head.3 \
		   man/khttp_parse.3 \
		   man/khttp_template.3 \
		   man/khttp_write.3 \
		   man/khttpbasic_validate.3 \
		   man/khttpdigest_validate.3 \
		   man/kmalloc.3 \
		   man/kutil_urlencode.3 \
		   man/kutil_epoch2str.3 \
		   man/kvalid_string.3
MAN8S		 = man/kfcgi.8 
MANS		 = $(MAN3S) \
		   $(MAN8S)
SRCS 		 = auth.c \
		   child.c \
		   compat-memmem.c \
     		   compat-reallocarray.c \
     		   compat-strlcat.c \
     		   compat-strlcpy.c \
     		   compat-strtonum.c \
     		   extern.h \
		   datetime.c \
		   fcgi.c \
		   httpauth.c \
		   md5.c \
		   md5.h \
     		   kcgi.c \
     		   kcgihtml.c \
		   kcgijson.c \
		   kcgiregress.c \
		   kcgixml.c \
     		   kcgi.h \
     		   kcgihtml.h \
		   kcgijson.h \
		   kcgiregress.h \
		   kcgixml.h \
		   kfcgi.c \
		   output.c \
		   parent.c \
     		   sample.c \
     		   sample-cgi.c \
     		   sample-fcgi.c \
     		   sandbox.c \
     		   sandbox-capsicum.c \
     		   sandbox-darwin.c \
     		   sandbox-pledge.c \
     		   sandbox-seccomp-filter.c \
     		   sandbox-systrace.c \
     		   wrappers.c \
     		   $(MANS) \
     		   $(TESTS)
AFL		 = afl/afl-multipart \
		   afl/afl-plain \
		   afl/afl-urlencoded
REGRESS		 = regress/test-abort-validator \
		   regress/test-basic \
		   regress/test-bigfile \
		   regress/test-digest \
		   regress/test-fcgi-abort-validator \
		   regress/test-fcgi-bigfile \
		   regress/test-fcgi-file-get \
		   regress/test-fcgi-path-check \
		   regress/test-fcgi-ping \
		   regress/test-fcgi-upload \
		   regress/test-file-get \
		   regress/test-fork \
		   regress/test-gzip \
		   regress/test-gzip-bigfile \
		   regress/test-header \
		   regress/test-httpdate \
		   regress/test-nogzip \
		   regress/test-path-check \
		   regress/test-ping \
		   regress/test-post \
		   regress/test-returncode \
		   regress/test-upload
REGRESS_OBJS	 = regress/regress.o \
		   regress/test-abort-validator.o \
		   regress/test-basic.o \
		   regress/test-bigfile.o \
		   regress/test-digest.o \
		   regress/test-fcgi-abort-validator.o \
		   regress/test-fcgi-bigfile.o \
		   regress/test-fcgi-file-get.o \
		   regress/test-fcgi-path-check.o \
		   regress/test-fcgi-ping.o \
		   regress/test-fcgi-upload.o \
		   regress/test-file-get.o \
		   regress/test-fork.o \
		   regress/test-gzip.o \
		   regress/test-gzip-bigfile.o \
		   regress/test-header.o \
		   regress/test-httpdate.o \
		   regress/test-nogzip.o \
		   regress/test-path-check.o \
		   regress/test-ping.o \
		   regress/test-post.o \
		   regress/test-returncode.o \
		   regress/test-upload.o
AFL_SRCS	 = afl/afl-multipart.c \
		   afl/afl-plain.c \
		   afl/afl-urlencoded.c
REGRESS_SRCS	 = regress/regress.c \
		   regress/regress.h \
		   regress/test-abort-validator.c \
		   regress/test-basic.c \
		   regress/test-bigfile.c \
		   regress/test-digest.c \
		   regress/test-fcgi-abort-validator.c \
		   regress/test-fcgi-bigfile.c \
		   regress/test-fcgi-file-get.c \
		   regress/test-fcgi-path-check.c \
		   regress/test-fcgi-ping.c \
		   regress/test-fcgi-upload.c \
		   regress/test-file-get.c \
		   regress/test-fork.c \
		   regress/test-gzip.c \
		   regress/test-gzip-bigfile.c \
		   regress/test-header.c \
		   regress/test-httpdate.c \
		   regress/test-nogzip.c \
		   regress/test-path-check.c \
		   regress/test-ping.c \
		   regress/test-post.c \
		   regress/test-returncode.c \
		   regress/test-upload.c
SVGS		 = figure1.svg \
		   figure2.png \
		   figure4.svg \
		   extending01-a.svg \
		   extending01-b.svg \
		   extending01-c.svg \
		   extending01-d.svg
CSSS	         = archive.css \
	           index.css \
	           mandoc.css

all: kfcgi libkcgi.a libkcgihtml.a libkcgijson.a libkcgixml.a libkcgiregress.a

afl: $(AFL)

samples: all sample sample-fcgi sample-cgi

regress: $(REGRESS)
	@for f in $(REGRESS) ; do \
		/bin/echo -n "./$${f}... " ; \
		./$$f >/dev/null 2>/dev/null || { /bin/echo "fail" ; exit 1 ; } ; \
		/bin/echo "ok" ; \
	done

regresslog: $(REGRESS)
	@for f in $(REGRESS) ; do \
		echo "./$${f}... " ; \
		./$$f >/dev/null || exit 1 ; \
	done

libconfig.a: config.h $(LIBCONFIGOBJS)
	$(AR) rs $@ $(LIBCONFIGOBJS)

kfcgi: kfcgi.o libconfig.a
	$(CC) $(CFLAGS) -o $@ kfcgi.o libconfig.a

kfcgi.o: config.h

$(REGRESS_OBJS): config.h

regress/test-bigfile: regress/test-bigfile.c regress/regress.o libkcgiregress.a libkcgi.a
	$(CC) $(CFLAGS) `curl-config --cflags` -o $@ regress/test-bigfile.c regress/regress.o libkcgiregress.a `curl-config --libs` libkcgi.a -lz

regress/test-returncode: regress/test-returncode.c regress/regress.o libkcgiregress.a libkcgi.a
	$(CC) $(CFLAGS) `curl-config --cflags` -o $@ regress/test-returncode.c regress/regress.o libkcgiregress.a `curl-config --libs` libkcgi.a -lz

regress/test-basic: regress/test-basic.c regress/regress.o libkcgiregress.a libkcgi.a
	$(CC) $(CFLAGS) `curl-config --cflags` -o $@ regress/test-basic.c regress/regress.o libkcgiregress.a `curl-config --libs` libkcgi.a -lz

regress/test-post: regress/test-post.c regress/regress.o libkcgiregress.a libkcgi.a
	$(CC) $(CFLAGS) `curl-config --cflags` -o $@ regress/test-post.c regress/regress.o libkcgiregress.a `curl-config --libs` libkcgi.a -lz

regress/test-ping: regress/test-ping.c regress/regress.o libkcgiregress.a libkcgi.a
	$(CC) $(CFLAGS) `curl-config --cflags` -o $@ regress/test-ping.c regress/regress.o libkcgiregress.a `curl-config --libs` libkcgi.a -lz

regress/test-digest: regress/test-digest.c regress/regress.o libkcgiregress.a libkcgi.a
	$(CC) $(CFLAGS) `curl-config --cflags` -o $@ regress/test-digest.c regress/regress.o libkcgiregress.a `curl-config --libs` libkcgi.a -lz

regress/test-fcgi-abort-validator: regress/test-fcgi-abort-validator.c regress/regress.o libkcgiregress.a libkcgi.a
	$(CC) $(CFLAGS) `curl-config --cflags` -o $@ regress/test-fcgi-abort-validator.c regress/regress.o libkcgiregress.a `curl-config --libs` libkcgi.a -lz

regress/test-fcgi-bigfile: regress/test-fcgi-bigfile.c regress/regress.o libkcgiregress.a libkcgi.a
	$(CC) $(CFLAGS) `curl-config --cflags` -o $@ regress/test-fcgi-bigfile.c regress/regress.o libkcgiregress.a `curl-config --libs` libkcgi.a -lz

regress/test-fcgi-file-get: regress/test-fcgi-file-get.c regress/regress.o libkcgiregress.a libkcgi.a
	$(CC) $(CFLAGS) `curl-config --cflags` -o $@ regress/test-fcgi-file-get.c regress/regress.o libkcgiregress.a `curl-config --libs` libkcgi.a -lz

regress/test-fcgi-path-check: regress/test-fcgi-path-check.c regress/regress.o libkcgiregress.a libkcgi.a
	$(CC) $(CFLAGS) `curl-config --cflags` -o $@ regress/test-fcgi-path-check.c regress/regress.o libkcgiregress.a `curl-config --libs` libkcgi.a -lz

regress/test-fcgi-ping: regress/test-fcgi-ping.c regress/regress.o libkcgiregress.a libkcgi.a
	$(CC) $(CFLAGS) `curl-config --cflags` -o $@ regress/test-fcgi-ping.c regress/regress.o libkcgiregress.a `curl-config --libs` libkcgi.a -lz

regress/test-fcgi-upload: regress/test-fcgi-upload.c regress/regress.o libkcgiregress.a libkcgi.a
	$(CC) $(CFLAGS) `curl-config --cflags` -o $@ regress/test-fcgi-upload.c regress/regress.o libkcgiregress.a `curl-config --libs` libkcgi.a -lz

regress/test-path-check: regress/test-path-check.c regress/regress.o libkcgiregress.a libkcgi.a
	$(CC) $(CFLAGS) `curl-config --cflags` -o $@ regress/test-path-check.c regress/regress.o libkcgiregress.a `curl-config --libs` libkcgi.a -lz

regress/test-nogzip: regress/test-nogzip.c regress/regress.o libkcgiregress.a libkcgi.a
	$(CC) $(CFLAGS) `curl-config --cflags` -o $@ regress/test-nogzip.c regress/regress.o libkcgiregress.a `curl-config --libs` libkcgi.a -lz

regress/test-fork: regress/test-fork.c regress/regress.o libkcgiregress.a libkcgi.a
	$(CC) $(CFLAGS) `curl-config --cflags` -o $@ regress/test-fork.c regress/regress.o libkcgiregress.a `curl-config --libs` libkcgi.a -lz

regress/test-gzip: regress/test-gzip.c regress/regress.o libkcgiregress.a libkcgi.a
	$(CC) $(CFLAGS) `curl-config --cflags` -o $@ regress/test-gzip.c regress/regress.o libkcgiregress.a `curl-config --libs` libkcgi.a -lz

regress/test-gzip-bigfile: regress/test-gzip-bigfile.c regress/regress.o libkcgiregress.a libkcgi.a
	$(CC) $(CFLAGS) `curl-config --cflags` -o $@ regress/test-gzip-bigfile.c regress/regress.o libkcgiregress.a `curl-config --libs` libkcgi.a -lz

regress/test-header: regress/test-header.c regress/regress.o libkcgiregress.a libkcgi.a
	$(CC) $(CFLAGS) `curl-config --cflags` -o $@ regress/test-header.c regress/regress.o libkcgiregress.a `curl-config --libs` libkcgi.a -lz

regress/test-file-get: regress/test-file-get.c regress/regress.o libkcgiregress.a libkcgi.a
	$(CC) $(CFLAGS) `curl-config --cflags` -o $@ regress/test-file-get.c regress/regress.o libkcgiregress.a `curl-config --libs` libkcgi.a -lz

regress/test-abort-validator: regress/test-abort-validator.c regress/regress.o libkcgiregress.a libkcgi.a
	$(CC) $(CFLAGS) `curl-config --cflags` -o $@ regress/test-abort-validator.c regress/regress.o libkcgiregress.a `curl-config --libs` libkcgi.a -lz

regress/test-upload: regress/test-upload.c regress/regress.o libkcgiregress.a libkcgi.a
	$(CC) $(CFLAGS) `curl-config --cflags` -o $@ regress/test-upload.c regress/regress.o libkcgiregress.a `curl-config --libs` libkcgi.a -lz

regress/test-httpdate: regress/test-httpdate.c regress/regress.o libkcgiregress.a libkcgi.a
	$(CC) $(CFLAGS) `curl-config --cflags` -o $@ regress/test-httpdate.c regress/regress.o libkcgiregress.a `curl-config --libs` libkcgi.a -lz

regress/regress.o: regress/regress.c
	$(CC) $(CFLAGS) `curl-config --cflags` -o $@ -c regress/regress.c

afl/afl-multipart: afl/afl-multipart.c libkcgi.a
	$(CC) $(CFLAGS) -o $@ afl/afl-multipart.c libkcgi.a -lz

afl/afl-plain: afl/afl-plain.c libkcgi.a
	$(CC) $(CFLAGS) -o $@ afl/afl-plain.c libkcgi.a -lz

afl/afl-urlencoded: afl/afl-urlencoded.c libkcgi.a
	$(CC) $(CFLAGS) -o $@ afl/afl-urlencoded.c libkcgi.a -lz

libkcgi.a: $(LIBOBJS) $(LIBCONFIGOBJS) $(LIBSANDBOXOBJS)
	$(AR) rs $@ $(LIBOBJS) $(LIBCONFIGOBJS) $(LIBSANDBOXOBJS)

kcgihtml.o: kcgihtml.h

kcgijson.o: kcgijson.h

kcgixml.o: kcgixml.h

kcgiregress.o: kcgiregress.h

libkcgihtml.a: kcgihtml.o
	$(AR) rs $@ kcgihtml.o

libkcgijson.a: kcgijson.o
	$(AR) rs $@ kcgijson.o

libkcgixml.a: kcgixml.o
	$(AR) rs $@ kcgixml.o

libkcgiregress.a: kcgiregress.o
	$(AR) rs $@ kcgiregress.o

$(LIBOBJS) sample.o sample-fcgi.o kcgihtml.o kcgijson.o kcgixml.o: kcgi.h

$(LIBOBJS) kcgihtml.o kcgijson.o kcgixml.o kcgiregress.o: config.h extern.h

auth.c child.c md5.c parent.c: md5.h

$(LIBCONFIGOBJS): config.h

config.h: config.h.pre config.h.post configure $(TESTS)
	rm -f config.log
	CC="$(CC)" CFLAGS="$(CFLAGS)" ./configure

installcgi: sample  sample-fcgi sample-cgi
	install -m 0755 sample $(PREFIX)/sample.cgi
	install -m 0755 sample-cgi $(PREFIX)/sample-simple.cgi
	install -m 0755 sample-fcgi $(PREFIX)/sample-fcgi.cgi
	install -m 0444 template.xml $(PREFIX)

install: all
	mkdir -p $(DESTDIR)$(LIBDIR)
	mkdir -p $(DESTDIR)$(INCLUDEDIR)
	mkdir -p $(DESTDIR)$(DATADIR)
	mkdir -p $(DESTDIR)$(MAN3DIR)
	mkdir -p $(DESTDIR)$(MAN8DIR)
	mkdir -p $(DESTDIR)$(SBINDIR)
	install -m 0444 libkcgi.a libkcgihtml.a libkcgijson.a libkcgixml.a libkcgiregress.a $(DESTDIR)$(LIBDIR)
	install -m 0444 kcgi.h kcgihtml.h kcgijson.h kcgixml.h kcgiregress.h $(DESTDIR)$(INCLUDEDIR)
	install -m 0444 $(MAN3S) $(DESTDIR)$(MAN3DIR)
	install -m 0444 $(MAN8S) $(DESTDIR)$(MAN8DIR)
	install -m 0555 kfcgi $(DESTDIR)$(SBINDIR)
	install -m 0444 template.xml sample.c sample-fcgi.c sample-cgi.c $(DESTDIR)$(DATADIR)
	rm -f kcgi.h~

sample: sample.o libkcgi.a libkcgihtml.a
	$(CC) -o $@ $(STATIC) sample.o -L. libkcgihtml.a libkcgi.a -lz

sample-fcgi: sample-fcgi.o libkcgi.a 
	$(CC) -o $@ $(STATIC) sample-fcgi.o -L. libkcgi.a -lz

sample-cgi: sample-cgi.o 
	$(CC) -o $@ $(STATIC) sample-cgi.o 

www: $(SVGS) $(SBLGS) kcgi.tgz kcgi.tgz.sha512 $(HTMLS) $(TUTORIALHTMLS) extending01.html

installwww: www
	mkdir -p $(PREFIX)/snapshots
	install -m 0444 $(SBLGS) $(CSSS) extending01.html $(TUTORIALHTMLS) $(SVGS) $(HTMLS) $(PREFIX)
	install -m 0444 sample.c $(PREFIX)/sample.c.txt
	install -m 0444 sample-fcgi.c $(PREFIX)/sample-fcgi.c.txt
	install -m 0444 kcgi.tgz kcgi.tgz.sha512 $(PREFIX)/snapshots/
	install -m 0444 kcgi.tgz $(PREFIX)/snapshots/kcgi-$(VERSION).tgz
	install -m 0444 kcgi.tgz.sha512 $(PREFIX)/snapshots/kcgi-$(VERSION).tgz.sha512

index.html: index.xml versions.xml $(TUTORIALHTMLS)
	sblg -t index.xml -o- versions.xml $(TUTORIALHTMLS) | sed "s!@VERSION@!$(VERSION)!g" >$@

archive.html: archive.xml versions.xml $(TUTORIALHTMLS)
	sblg -t archive.xml -o- versions.xml $(TUTORIALHTMLS) | sed "s!@VERSION@!$(VERSION)!g" >$@

$(TUTORIALHTMLS): tutorial.xml versions.xml $(TUTORIALXMLS)

extending01.html: extending01.xml tutorial.xml
	sblg -t tutorial.xml -o- -c $< | sed "s!@VERSION@!$(VERSION)!g" >$@

.xml.html:
	sblg -t tutorial.xml -o- -C $< versions.xml $(TUTORIALXMLS) | sed "s!@VERSION@!$(VERSION)!g" >$@

.3.3.html:
	mandoc -Thtml -Ostyle=mandoc.css,man=%N.%S.html $< >$@

.8.8.html:
	mandoc -Thtml -Ostyle=mandoc.css,man=%N.%S.html $< >$@

kcgi.tgz.sha512: kcgi.tgz
	openssl dgst -sha512 kcgi.tgz >$@

kcgi.tgz:
	mkdir -p .dist/kcgi-$(VERSION)
	mkdir -p .dist/kcgi-$(VERSION)/man
	mkdir -p .dist/kcgi-$(VERSION)/regress
	mkdir -p .dist/kcgi-$(VERSION)/afl
	cp $(SRCS) .dist/kcgi-$(VERSION)
	cp $(REGRESS_SRCS) .dist/kcgi-$(VERSION)/regress
	cp $(AFL_SRCS) .dist/kcgi-$(VERSION)/afl
	cp Makefile template.xml .dist/kcgi-$(VERSION)
	cp $(MANS) .dist/kcgi-$(VERSION)/man
	cp configure config.h.pre config.h.post .dist/kcgi-$(VERSION)
	(cd .dist && tar zcf ../$@ kcgi-$(VERSION))
	rm -rf .dist

.dot.svg:
	dot -Tsvg -o $@ $<

.gnuplot.png:
	gnuplot $<

clean:
	rm -f kcgi.tgz kcgi.tgz.sha512 $(SVGS) $(HTMLS) sample sample-fcgi sample.o sample-fcgi.o kfcgi kfcgi.o sample-cgi sample-cgi.o
	rm -f $(SBLGS) $(TUTORIALHTMLS) extending01.html
	rm -f libconfig.a
	rm -f libkcgi.a $(LIBOBJS) $(LIBCONFIGOBJS) 
	rm -f libkcgihtml.a kcgihtml.o
	rm -f libkcgijson.a kcgijson.o
	rm -f libkcgixml.a kcgixml.o
	rm -f libkcgiregress.a kcgiregress.o
	rm -f config.log config.h
	rm -f test-arc4random
	rm -f test-abort-valid.core core
	rm -f test-memmem test-memmem.o 
	rm -f test-reallocarray test-reallocarray.o 
	rm -f test-sandbox_init test-sandbox_init.o
	rm -f test-seccomp-filter test-seccomp-filter.o
	rm -f test-strlcat test-strlcat.o
	rm -f test-strlcpy test-strlcpy.o
	rm -f test-strtonum test-strtonum.o
	rm -f test-systrace test-systrace.o
	rm -f test-zlib test-zlib.o 
	rm -f test-capsicum test-capsicum.o
	rm -f $(REGRESS) $(AFL) $(REGRESS_OBJS)
	rm -rf *.dSYM regress/*.dSYM afl/*.dSYM
