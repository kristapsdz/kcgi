.SUFFIXES: .3 .3.html .dot .svg .gnuplot .png .xml .html

# Comment if you don't need statically linked.
# This is only for the sample program!
#STATIC 		 = -static

# You probably don't need to change anything else...

CFLAGS 		+= -g -W -Wall -Wstrict-prototypes -Wno-unused-parameter -Wwrite-strings -DHAVE_CONFIG_H
PREFIX 		 = /usr/local
DATADIR 	 = $(PREFIX)/share/kcgi
VERSIONS	 = version_0_4_2.xml \
		   version_0_4_3.xml \
		   version_0_4_4.xml \
		   version_0_5.xml \
		   version_0_5_1.xml \
		   version_0_5_2.xml \
		   version_0_5_3.xml \
		   version_0_5_4.xml \
		   version_0_5_5.xml \
		   version_0_5_6.xml \
		   version_0_5_7.xml \
		   version_0_5_8.xml \
		   version_0_5_9.xml \
		   version_0_6.xml \
		   version_0_6_1.xml
TUTORIALXMLS	 = tutorial0.xml \
		   tutorial1.xml
TUTORIALHTMLS	 = tutorial0.html \
		   tutorial1.html
MANDIR 	 	 = $(PREFIX)/man/man3
LIBDIR 		 = $(PREFIX)/lib
INCLUDEDIR 	 = $(PREFIX)/include
VERSION 	 = 0.6.1
LIBOBJS 	 = child.o \
		   fcgi.o \
		   httpauth.o \
		   kcgi.o \
		   compat-memmem.o \
		   compat-reallocarray.o \
		   compat-strlcat.o \
		   compat-strlcpy.o \
		   compat-strtonum.o \
		   parent.o \
		   sandbox.o \
		   sandbox-capsicum.o \
		   sandbox-darwin.o \
		   sandbox-seccomp-filter.o \
		   sandbox-systrace.o \
		   worker.o \
		   wrappers.o
HTMLS		 = man/kcgi.3.html \
		   man/kcgihtml.3.html \
		   man/kcgijson.3.html \
		   man/kcgixml.3.html \
		   man/kcgi_regress.3.html \
		   man/khttp_body.3.html \
		   man/khttp_free.3.html \
		   man/khttp_head.3.html \
		   man/khttp_parse.3.html \
		   man/khttp_template.3.html \
		   man/khttp_write.3.html \
		   man/kmalloc.3.html \
		   man/kutil_urlencode.3.html \
		   man/kvalid_string.3.html
TESTS 		 = test-memmem.c \
      		   test-reallocarray.c \
      		   test-sandbox_init.c \
      		   test-capsicum.c \
      		   test-strlcat.c \
      		   test-strlcpy.c \
      		   test-strtonum.c \
      		   test-seccomp-filter.c \
      		   test-systrace.c \
      		   test-zlib.c
MANS		 = man/kcgi.3 \
		   man/kcgihtml.3 \
		   man/kcgijson.3 \
		   man/kcgixml.3 \
		   man/kcgi_regress.3 \
		   man/khttp_body.3 \
		   man/khttp_free.3 \
		   man/khttp_head.3 \
		   man/khttp_parse.3 \
		   man/khttp_template.3 \
		   man/khttp_write.3 \
		   man/kmalloc.3 \
		   man/kutil_urlencode.3 \
		   man/kvalid_string.3
SRCS 		 = child.c \
		   compat-memmem.c \
     		   compat-reallocarray.c \
     		   compat-strlcat.c \
     		   compat-strlcpy.c \
     		   compat-strtonum.c \
     		   extern.h \
		   httpauth.c \
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
		   parent.c \
     		   sample.c \
     		   sandbox.c \
     		   sandbox-capsicum.c \
     		   sandbox-darwin.c \
     		   sandbox-seccomp-filter.c \
     		   sandbox-systrace.c \
		   worker.c \
     		   wrappers.c \
     		   $(MANS) \
     		   $(TESTS)
AFL		 = afl/afl-multipart \
		   afl/afl-plain \
		   afl/afl-urlencoded
REGRESS		 = regress/test-abort-validator \
		   regress/test-file-get \
		   regress/test-gzip \
		   regress/test-header \
		   regress/test-nogzip \
		   regress/test-ping \
		   regress/test-upload
REGRESS_OBJS	 = regress/regress.o \
		   regress/test-abort-validator.o \
		   regress/test-file-get.o \
		   regress/test-gzip.o \
		   regress/test-header.o \
		   regress/test-nogzip.o \
		   regress/test-ping.o \
		   regress/test-sandbox.o \
		   regress/test-upload.o
AFL_SRCS	 = afl/afl-multipart.c \
		   afl/afl-plain.c \
		   afl/afl-urlencoded.c
REGRESS_SRCS	 = regress/regress.c \
		   regress/regress.h \
		   regress/test-abort-validator.c \
		   regress/test-file-get.c \
		   regress/test-gzip.c \
		   regress/test-header.c \
		   regress/test-nogzip.c \
		   regress/test-ping.c \
		   regress/test-sandbox.c \
		   regress/test-upload.c
SVGS		 = figure1.svg \
		   figure2.png \
		   figure3.png

all: kfcgi libkcgi.a libkcgihtml.a libkcgijson.a libkcgixml.a libkcgiregress.a

afl: $(AFL)

regress: $(REGRESS)
	@for f in $(REGRESS) ; do \
		/bin/echo -n "./$${f}... " ; \
		./$$f >/dev/null 2>/dev/null || { /bin/echo "fail" ; exit 1 ; } ; \
		/bin/echo "ok" ; \
	done

kfcgi: kfcgi.c
	$(CC) $(CFLAGS) -o $@ kfcgi.c

regress/test-ping: regress/test-ping.c regress/regress.o libkcgiregress.a libkcgi.a
	$(CC) $(CFLAGS) `curl-config --cflags` -o $@ regress/test-ping.c regress/regress.o libkcgiregress.a `curl-config --libs` libkcgi.a -lz

regress/test-nogzip: regress/test-nogzip.c regress/regress.o libkcgiregress.a libkcgi.a
	$(CC) $(CFLAGS) `curl-config --cflags` -o $@ regress/test-nogzip.c regress/regress.o libkcgiregress.a `curl-config --libs` libkcgi.a -lz

regress/test-gzip: regress/test-gzip.c regress/regress.o libkcgiregress.a libkcgi.a
	$(CC) $(CFLAGS) `curl-config --cflags` -o $@ regress/test-gzip.c regress/regress.o libkcgiregress.a `curl-config --libs` libkcgi.a -lz

regress/test-header: regress/test-header.c regress/regress.o libkcgiregress.a libkcgi.a
	$(CC) $(CFLAGS) `curl-config --cflags` -o $@ regress/test-header.c regress/regress.o libkcgiregress.a `curl-config --libs` libkcgi.a -lz

regress/test-file-get: regress/test-file-get.c regress/regress.o libkcgiregress.a libkcgi.a
	$(CC) $(CFLAGS) `curl-config --cflags` -o $@ regress/test-file-get.c regress/regress.o libkcgiregress.a `curl-config --libs` libkcgi.a -lz

regress/test-abort-validator: regress/test-abort-validator.c regress/regress.o libkcgiregress.a libkcgi.a
	$(CC) $(CFLAGS) `curl-config --cflags` -o $@ regress/test-abort-validator.c regress/regress.o libkcgiregress.a `curl-config --libs` libkcgi.a -lz

regress/test-upload: regress/test-upload.c regress/regress.o libkcgiregress.a libkcgi.a
	$(CC) $(CFLAGS) `curl-config --cflags` -o $@ regress/test-upload.c regress/regress.o libkcgiregress.a `curl-config --libs` libkcgi.a -lz

regress/test-sandbox: regress/test-sandbox.c regress/regress.o libkcgiregress.a libkcgi.a
	$(CC) $(CFLAGS) `curl-config --cflags` -o $@ regress/test-sandbox.c regress/regress.o libkcgiregress.a `curl-config --libs` libkcgi.a -lz

regress/regress.o: regress/regress.c
	$(CC) $(CFLAGS) `curl-config --cflags` -o $@ -c regress/regress.c

afl/afl-multipart: afl/afl-multipart.c libkcgi.a
	$(CC) $(CFLAGS) -o $@ afl/afl-multipart.c libkcgi.a -lz

afl/afl-plain: afl/afl-plain.c libkcgi.a
	$(CC) $(CFLAGS) -o $@ afl/afl-plain.c libkcgi.a -lz

afl/afl-urlencoded: afl/afl-urlencoded.c libkcgi.a
	$(CC) $(CFLAGS) -o $@ afl/afl-urlencoded.c libkcgi.a -lz

libkcgi.a: $(LIBOBJS)
	$(AR) rs $@ $(LIBOBJS)

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

$(LIBOBJS) sample.o kcgihtml.o kcgijson.o kcgixml.o: kcgi.h

$(LIBOBJS) kcgihtml.o kcgijson.o kcgixml.o kcgiregress.o: config.h extern.h

config.h: config.h.pre config.h.post configure $(TESTS)
	rm -f config.log
	CC="$(CC)" CFLAGS="$(CFLAGS)" ./configure

installcgi: sample 
	install -m 0755 sample $(PREFIX)/sample.cgi
	install -m 0755 sample $(PREFIX)
	install -m 0444 template.xml $(PREFIX)

install: all
	mkdir -p $(DESTDIR)$(LIBDIR)
	mkdir -p $(DESTDIR)$(INCLUDEDIR)
	mkdir -p $(DESTDIR)$(DATADIR)
	mkdir -p $(DESTDIR)$(MANDIR)
	install -m 0444 libkcgi.a libkcgihtml.a libkcgijson.a libkcgixml.a $(DESTDIR)$(LIBDIR)
	install -m 0444 kcgi.h kcgihtml.h kcgijson.h kcgixml.h $(DESTDIR)$(INCLUDEDIR)
	install -m 0444 $(MANS) $(DESTDIR)$(MANDIR)
	install -m 0444 template.xml sample.c $(DESTDIR)$(DATADIR)
	rm -f kcgi.h~

sample: sample.o libkcgi.a libkcgihtml.a
	$(CC) -o $@ $(STATIC) sample.o -L. libkcgihtml.a libkcgi.a -lz

www: $(SVGS) index.html kcgi.tgz kcgi.tgz.sha512 $(HTMLS) $(TUTORIALHTMLS)

installwww: www
	mkdir -p $(PREFIX)/snapshots
	install -m 0444 index.html index.css $(TUTORIALHTMLS) $(SVGS) $(HTMLS) $(PREFIX)
	install -m 0444 sample.c $(PREFIX)/sample.c.txt
	install -m 0444 kcgi.tgz kcgi.tgz.sha512 $(PREFIX)/snapshots/
	install -m 0444 kcgi.tgz $(PREFIX)/snapshots/kcgi-$(VERSION).tgz
	install -m 0444 kcgi.tgz.sha512 $(PREFIX)/snapshots/kcgi-$(VERSION).tgz.sha512

updatewww: www
	mkdir -p $(PREFIX)
	install -m 0444 index.html index.css $(TUTORIALHTMLS) $(SVGS) $(HTMLS) $(PREFIX)
	install -m 0444 sample.c $(PREFIX)/sample.c.txt

index.html: index.xml $(VERSIONS) $(TUTORIALHTMLS)
	sblg -t index.xml -o- $(VERSIONS) $(TUTORIALHTMLS) | sed "s!@VERSION@!$(VERSION)!g" >$@

$(TUTORIALHTMLS): tutorial.xml $(VERSIONS) $(TUTORIALXMLS)

.xml.html:
	sblg -t tutorial.xml -o- -C $< $(VERSIONS) $(TUTORIALXMLS) | sed "s!@VERSION@!$(VERSION)!g" >$@

.3.3.html:
	mandoc -Thtml -Oman=%N.%S.html $< >$@

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
	rm -f kcgi.tgz kcgi.tgz.sha512 $(SVGS) $(HTMLS) sample sample.o kfcgi
	rm -f index.html $(TUTORIALHTMLS)
	rm -f libkcgi.a $(LIBOBJS)
	rm -f libkcgihtml.a kcgihtml.o
	rm -f libkcgijson.a kcgijson.o
	rm -f libkcgixml.a kcgixml.o
	rm -f libkcgiregress.a kcgiregress.o
	rm -f config.log config.h
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
