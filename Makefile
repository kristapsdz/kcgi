.SUFFIXES: .3 .3.html

CFLAGS 		+= -g -W -Wall -Wstrict-prototypes -Wno-unused-parameter -Wwrite-strings -DHAVE_CONFIG_H
# Comment if you don't need statically linked.
#STATIC 		 = -static
PREFIX 		 = /usr/local
DATADIR 	 = $(PREFIX)/share/kcgi
MANDIR 	 	 = $(PREFIX)/man/man3
LIBDIR 		 = $(PREFIX)/lib
INCLUDEDIR 	 = $(PREFIX)/include
VERSION 	 = 0.4.0
LIBOBJS 	 = kcgi.o \
		   compat-memmem.o \
		   compat-reallocarray.o \
		   compat-strlcat.o \
		   compat-strlcpy.o \
		   compat-strtonum.o \
		   input.o \
		   sandbox.o \
		   sandbox-capsicum.o \
		   sandbox-darwin.o \
		   sandbox-systrace.o \
		   wrappers.o
HTMLS		 = man/kcgi.3.html \
		   man/kcgihtml.3.html \
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
      		   test-systrace.c \
      		   test-zlib.c
MANS		 = man/kcgi.3 \
		   man/kcgihtml.3 \
		   man/khttp_body.3 \
		   man/khttp_free.3 \
		   man/khttp_head.3 \
		   man/khttp_parse.3 \
		   man/khttp_template.3 \
		   man/khttp_write.3 \
		   man/kmalloc.3 \
		   man/kutil_urlencode.3 \
		   man/kvalid_string.3
SRCS 		 = compat-memmem.c \
     		   compat-reallocarray.c \
     		   compat-strlcat.c \
     		   compat-strlcpy.c \
     		   compat-strtonum.c \
     		   extern.h \
     		   input.c \
     		   kcgi.c \
     		   kcgihtml.c \
     		   kcgi.h \
     		   kcgihtml.h \
     		   sample.c \
     		   sandbox.c \
     		   sandbox-capsicum.c \
     		   sandbox-darwin.c \
     		   sandbox-systrace.c \
     		   wrappers.c \
     		   $(MANS) \
     		   $(TESTS)
# Only for local installation.
WWWDIR 		 = /var/www/vhosts/kristaps.bsd.lv/htdocs/kcgi

all: libkcgi.a libkcgihtml.a

mime2c: mime2c.o
	$(CC) -o $@ mime2c.o -lutil

libkcgi.a: $(LIBOBJS)
	$(AR) rs $@ $(LIBOBJS)

libkcgihtml.a: kcgihtml.o
	$(AR) rs $@ kcgihtml.o

$(LIBOBJS) sample.o kcgihtml.o: kcgi.h

$(LIBOBJS): config.h extern.h

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
	install -m 0444 libkcgi.a libkcgihtml.a $(DESTDIR)$(LIBDIR)
	install -m 0444 kcgi.h kcgihtml.h $(DESTDIR)$(INCLUDEDIR)
	install -m 0444 kcgihtml.h $(DESTDIR)$(INCLUDEDIR)
	install -m 0444 $(MANS) $(DESTDIR)$(MANDIR)
	install -m 0444 template.xml sample.c $(DESTDIR)$(DATADIR)
	rm -f kcgi.h~

sample: sample.o libkcgi.a libkcgihtml.a
	$(CC) -o $@ $(STATIC) sample.o -L. libkcgihtml.a libkcgi.a -lz

www: index.html kcgi-$(VERSION).tgz $(HTMLS)

installwww: www
	mkdir -p $(WWWDIR)/snapshots
	install -m 0444 index.html index.css $(HTMLS) $(WWWDIR)
	install -m 0444 sample.c $(WWWDIR)/sample.c.txt
	install -m 0444 kcgi-$(VERSION).tgz $(WWWDIR)/snapshots/
	install -m 0444 kcgi-$(VERSION).tgz $(WWWDIR)/snapshots/kcgi.tgz

index.html: index.xml
	sed "s!@VERSION@!$(VERSION)!g" index.xml >$@

.3.3.html:
	mandoc -Thtml $< >$@

kcgi-$(VERSION).tgz:
	mkdir -p .dist/kcgi-$(VERSION)
	mkdir -p .dist/kcgi-$(VERSION)/man
	cp $(SRCS) .dist/kcgi-$(VERSION)
	cp Makefile template.xml .dist/kcgi-$(VERSION)
	cp $(MANS) .dist/kcgi-$(VERSION)/man
	cp configure config.h.pre config.h.post .dist/kcgi-$(VERSION)
	(cd .dist && tar zcf ../$@ kcgi-$(VERSION))
	rm -rf .dist

clean:
	rm -f kcgi-$(VERSION).tgz index.html $(HTMLS) sample
	rm -f libkcgi.a libkcgihtml.a $(LIBOBJS) kcgihtml.o
	rm -f config.log config.h
	rm -f test-memmem test-memmem.o 
	rm -f test-reallocarray test-reallocarray.o 
	rm -f test-sandbox_init test-sandbox_init.o
	rm -f test-strlcat test-strlcat.o
	rm -f test-strlcpy test-strlcpy.o
	rm -f test-strtonum test-strtonum.o
	rm -f test-systrace test-systrace.o
	rm -f test-zlib test-zlib.o 
	rm -f test-capsicum test-capsicum.o
	rm -rf *.dSYM
