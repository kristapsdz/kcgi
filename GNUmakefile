.SUFFIXES: .3 .3.html .8 .8.html .dot .svg .gnuplot .png .xml .html
.PHONY: regress

include Makefile.configure

# Comment if you don't need statically linked.
# This is only for the sample program!
STATIC 		 = -static

ifeq ($(shell uname), Linux)
# Linux needs libbsd for regression tests.
LIBADD		+= $(shell pkg-config --libs libbsd)
CFLAGS		+= $(shell pkg-config --cflags libbsd)
endif

ifeq ($(shell uname), Darwin)
# Mac OS X doesn't support static linking.
STATIC 		 = 
# Mac OS X has deprecated daemon(3).
CFLAGS		+= -Wno-deprecated-declarations
endif

# You probably don't need to change anything else...

#CFLAGS		+= -DSANDBOX_SECCOMP_DEBUG
WWWDIR		 = /var/www/vhosts/kristaps.bsd.lv/htdocs/kcgi
DATADIR 	 = $(SHAREDIR)/kcgi
TUTORIALXMLS	 = tutorial0.xml \
		   tutorial1.xml \
		   tutorial2.xml \
		   tutorial3.xml \
		   tutorial4.xml \
		   tutorial5.xml \
		   tutorial6.xml
TUTORIALHTMLS	 = $(addsuffix .html, $(foreach xml, $(TUTORIALXMLS), $(basename $(xml) .xml)))
SBLGS		 = archive.html \
		   index.html \
		   sample.c.html \
		   samplepp.cc.html
MAN3DIR	 	 = $(MANDIR)/man3
MAN8DIR	 	 = $(MANDIR)/man8
VMAJOR		 = $(shell grep 'define	KCGI_VMAJOR' kcgi.h | cut -f3)
VMINOR		 = $(shell grep 'define	KCGI_VMINOR' kcgi.h | cut -f3)
VBUILD		 = $(shell grep 'define	KCGI_VBUILD' kcgi.h | cut -f3)
VERSION		:= $(VMAJOR).$(VMINOR).$(VBUILD)
LIBOBJS 	 = auth.o \
		   child.o \
		   datetime.o \
		   fcgi.o \
		   httpauth.o \
		   kcgi.o \
		   logging.o \
		   output.o \
		   parent.o \
		   sandbox.o \
		   sandbox-capsicum.o \
		   sandbox-darwin.o \
		   sandbox-pledge.o \
		   sandbox-seccomp-filter.o \
		   sandbox-systrace.o \
		   template.o \
		   wrappers.o
HTMLS	 	 = $(addsuffix .html, $(MANS)) \
		   functions.html
MAN3S		 = man/kcgi.3 \
		   man/kcgihtml.3 \
		   man/kcgijson.3 \
		   man/kcgiregress.3 \
		   man/kcgixml.3 \
		   man/kcgi_buf_write.3 \
		   man/kcgi_writer_disable.3 \
		   man/kcgi_strerror.3 \
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
		   man/kutil_epoch2str.3 \
		   man/kutil_invalidate.3 \
		   man/kutil_log.3 \
		   man/kutil_openlog.3 \
		   man/kutil_urlencode.3 \
		   man/kvalid_string.3
MAN8S		 = man/kfcgi.8 
MANS		 = $(MAN3S) \
		   $(MAN8S)
SRCS 		 = auth.c \
		   child.c \
		   compats.c \
     		   extern.h \
		   datetime.c \
		   fcgi.c \
		   httpauth.c \
		   logging.c \
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
		   samplepp.cc \
     		   sandbox.c \
     		   sandbox-capsicum.c \
     		   sandbox-darwin.c \
     		   sandbox-pledge.c \
     		   sandbox-seccomp-filter.c \
     		   sandbox-systrace.c \
		   template.c \
		   tests.c \
     		   wrappers.c \
     		   $(MANS)
AFL		 = afl/afl-multipart \
		   afl/afl-plain \
		   afl/afl-template \
		   afl/afl-urlencoded
REGRESS		 = regress/test-basic \
		   regress/test-basic-curl \
		   regress/test-bigfile \
		   regress/test-buf \
		   regress/test-datetime \
		   regress/test-digest \
		   regress/test-digest-auth-int \
		   regress/test-digest-auth-int-bad \
		   regress/test-file-get \
		   regress/test-fork \
		   regress/test-gzip \
		   regress/test-gzip-bigfile \
		   regress/test-header \
		   regress/test-header-bad \
		   regress/test-httpdate \
		   regress/test-invalidate \
		   regress/test-json-simple \
		   regress/test-nogzip \
		   regress/test-nullqueryval \
		   regress/test-path-check \
		   regress/test-ping \
		   regress/test-ping-double \
		   regress/test-post \
		   regress/test-returncode \
		   regress/test-template \
		   regress/test-upload \
		   regress/test-urlencode \
		   regress/test-urlpart \
		   regress/test-valid-date \
		   regress/test-valid-double

# Apple's Mac OS X has an operating system failure when running these
# tests (or running the same code as a regular CGI/FastCGI script).

ifneq ($(shell uname), Darwin)
REGRESS		+= regress/test-abort-validator \
		   regress/test-fcgi-abort-validator \
		   regress/test-fcgi-bigfile \
		   regress/test-fcgi-file-get \
		   regress/test-fcgi-header \
		   regress/test-fcgi-header-bad \
		   regress/test-fcgi-path-check \
		   regress/test-fcgi-ping \
		   regress/test-fcgi-ping-double \
		   regress/test-fcgi-upload
endif

REGRESS_OBJS	 = $(addsuffix .o, $(REGRESS)) \
		   regress/regress.o
AFL_SRCS	 = $(addsuffix .c, $(AFL)) 
REGRESS_SRCS	 = regress/regress.c \
		   regress/regress.h \
		   $(addsuffix .c, $(REGRESS))
SVGS		 = figure1.svg \
		   figure2.png \
		   figure4.svg \
		   extending01-a.svg \
		   extending01-b.svg \
		   extending01-c.svg \
		   extending01-d.svg \
		   tutorial6.svg
CSSS	         = archive.css \
	           index.css \
	           mandoc.css \
		   prettify.css \
		   prettify.js \
		   tutorial.css
LIBS		 = libkcgi.a \
		   libkcgihtml.a \
		   libkcgijson.a \
		   libkcgixml.a \
		   libkcgiregress.a

all: kfcgi $(LIBS)

afl: $(AFL)

samples: all sample samplepp sample-fcgi sample-cgi

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

libconfig.a: config.h compats.o
	$(AR) rs $@ compats.o

kfcgi: kfcgi.o libconfig.a
	$(CC) $(CFLAGS) -o $@ kfcgi.o libconfig.a

kfcgi.o: config.h

regress/%.o: regress/%.c config.h regress/regress.h kcgi.h
	$(CC) $(CFLAGS) `curl-config --cflags` -o $@ -c $<

regress/%: regress/%.o regress/regress.o libkcgiregress.a libkcgijson.a libkcgi.a
	$(CC) -o $@ $^ `curl-config --libs` -lz $(LIBADD)

afl/%: afl/%.c libkcgi.a
	$(CC) $(CFLAGS) -o $@ $< libkcgi.a -lz

.PRECIOUS: $(REGRESS_OBJS)

libkcgi.a: $(LIBOBJS) compats.o $(LIBSANDBOXOBJS)
	$(AR) rs $@ $(LIBOBJS) compats.o $(LIBSANDBOXOBJS)

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

$(LIBOBJS) sample.o samplepp sample-fcgi.o kcgihtml.o kcgijson.o kcgixml.o: kcgi.h

$(LIBOBJS) kcgihtml.o kcgijson.o kcgixml.o kcgiregress.o: config.h extern.h

compats.o: config.h

installcgi: sample samplepp sample-fcgi sample-cgi
	$(INSTALL_PROGRAM) sample $(PREFIX)/sample.cgi
	$(INSTALL_PROGRAM) samplepp $(PREFIX)/samplepp.cgi
	$(INSTALL_PROGRAM) sample-cgi $(PREFIX)/sample-simple.cgi
	$(INSTALL_PROGRAM) sample-fcgi $(PREFIX)/sample-fcgi.cgi
	$(INSTALL_DATA) template.xml $(PREFIX)

install: all
	mkdir -p $(DESTDIR)$(LIBDIR)
	mkdir -p $(DESTDIR)$(INCLUDEDIR)
	mkdir -p $(DESTDIR)$(DATADIR)
	mkdir -p $(DESTDIR)$(MAN3DIR)
	mkdir -p $(DESTDIR)$(MAN8DIR)
	mkdir -p $(DESTDIR)$(SBINDIR)
	$(INSTALL_LIB) $(LIBS) $(DESTDIR)$(LIBDIR)
	$(INSTALL_DATA) kcgi.h kcgihtml.h kcgijson.h kcgixml.h kcgiregress.h $(DESTDIR)$(INCLUDEDIR)
	$(INSTALL_MAN) $(MAN3S) $(DESTDIR)$(MAN3DIR)
	$(INSTALL_MAN) $(MAN8S) $(DESTDIR)$(MAN8DIR)
	$(INSTALL_PROGRAM) kfcgi $(DESTDIR)$(SBINDIR)
	$(INSTALL_DATA) template.xml sample.c samplepp.cc sample-fcgi.c sample-cgi.c $(DESTDIR)$(DATADIR)

uninstall:
	$(foreach $@_LIB, $(LIBS), rm -f $(DESTDIR)$(LIBDIR)/$($@_LIB);)
	$(foreach $@_INC, kcgi.h kcgihtml.h kcgijson.h kcgixml.h kcgiregress.h, rm -f $(DESTDIR)$(INCLUDEDIR)/$($@_INC);)
	$(foreach $@_MAN, $(MAN3S), rm -f $(DESTDIR)$(MAN3DIR)/$(notdir $($@_MAN));)
	$(foreach $@_MAN, $(MAN8S), rm -f $(DESTDIR)$(MAN8DIR)/$(notdir $($@_MAN));)
	rm -f $(DESTDIR)$(SBINDIR)/kfcgi
	$(foreach $@_TMP, template.xml sample.c samplepp.cc sample-fcgi.c sample-cgi.c, rm -f $(DESTDIR)$(DATADIR)/$($@_TMP);)

samplepp: samplepp.cc libkcgi.a libkcgihtml.a
	c++ $(CFLAGS) -static -o $@ $(STATIC) samplepp.cc -L. libkcgi.a -lz

sample: sample.o libkcgi.a libkcgihtml.a
	$(CC) -o $@ $(STATIC) sample.o -L. libkcgihtml.a libkcgi.a -lz

sample-fcgi: sample-fcgi.o libkcgi.a 
	$(CC) -o $@ $(STATIC) sample-fcgi.o -L. libkcgi.a -lz

sample-cgi: sample-cgi.o 
	$(CC) -o $@ $(STATIC) sample-cgi.o 

www: $(SVGS) $(SBLGS) kcgi.tgz kcgi.tgz.sha512 $(HTMLS) $(TUTORIALHTMLS) extending01.html atom.xml

installwww: www
	mkdir -p $(WWWDIR)/snapshots
	$(INSTALL_DATA) $(SBLGS) $(CSSS) extending01.html $(TUTORIALHTMLS) $(SVGS) $(HTMLS) atom.xml $(WWWDIR)
	$(INSTALL_DATA) sample.c $(WWWDIR)/sample.c.txt
	$(INSTALL_DATA) sample-fcgi.c $(WWWDIR)/sample-fcgi.c.txt
	$(INSTALL_DATA) kcgi.tgz kcgi.tgz.sha512 $(WWWDIR)/snapshots/
	$(INSTALL_DATA) kcgi.tgz $(WWWDIR)/snapshots/kcgi-$(VERSION).tgz
	$(INSTALL_DATA) kcgi.tgz.sha512 $(WWWDIR)/snapshots/kcgi-$(VERSION).tgz.sha512

index.html: index.xml versions.xml $(TUTORIALHTMLS)
	sblg -t index.xml -s date -o- versions.xml $(TUTORIALHTMLS) | sed "s!@VERSION@!$(VERSION)!g" >$@

sample.c.html: sample.c
	highlight -o $@ --inline-css --doc sample.c

samplepp.cc.html: samplepp.cc
	highlight -o $@ --inline-css --doc samplepp.cc

archive.html: archive.xml versions.xml $(TUTORIALHTMLS)
	sblg -t archive.xml -s date -o- versions.xml $(TUTORIALHTMLS) | sed "s!@VERSION@!$(VERSION)!g" >$@

$(TUTORIALHTMLS): tutorial.xml versions.xml $(TUTORIALXMLS)

extending01.html: extending01.xml tutorial.xml
	sblg -t tutorial.xml -o- -c $< | sed "s!@VERSION@!$(VERSION)!g" >$@

.xml.html:
	sblg -t tutorial.xml -s date -o- -C $< versions.xml $(TUTORIALXMLS) | sed "s!@VERSION@!$(VERSION)!g" >$@

.3.3.html:
	mandoc -Thtml -Ostyle=mandoc.css $< >$@

.8.8.html:
	mandoc -Thtml -Ostyle=mandoc.css $< >$@

kcgi.tgz.sha512: kcgi.tgz
	openssl dgst -sha512 kcgi.tgz >$@

kcgi.tgz:
	mkdir -p .dist/kcgi-$(VERSION)
	mkdir -p .dist/kcgi-$(VERSION)/man
	mkdir -p .dist/kcgi-$(VERSION)/regress
	mkdir -p .dist/kcgi-$(VERSION)/afl
	install -m 0644 $(SRCS) .dist/kcgi-$(VERSION)
	install -m 0644 $(REGRESS_SRCS) .dist/kcgi-$(VERSION)/regress
	install -m 0644 $(AFL_SRCS) .dist/kcgi-$(VERSION)/afl
	install -m 0644 GNUmakefile template.xml .dist/kcgi-$(VERSION)
	install -m 0644 $(MANS) .dist/kcgi-$(VERSION)/man
	install -m 0755 configure .dist/kcgi-$(VERSION)
	(cd .dist && tar zcf ../$@ kcgi-$(VERSION))
	rm -rf .dist

.dot.svg:
	dot -Tsvg -o $@ $<

.gnuplot.png:
	gnuplot $<

functions.html: functions.xml genindex.sh
	sh ./genindex.sh functions.xml | sed "s!@VERSION@!$(VERSION)!g" >$@

atom.xml: versions.xml
	sblg -s date -a versions.xml >$@

clean:
	rm -f kcgi.tgz kcgi.tgz.sha512 $(SVGS) $(HTMLS) 
	rm -f sample samplepp sample-fcgi sample.o sample-fcgi.o kfcgi kfcgi.o sample-cgi sample-cgi.o
	rm -f $(SBLGS) $(TUTORIALHTMLS) extending01.html atom.xml
	rm -f libconfig.a
	rm -f $(LIBOBJS) compats.o
	rm -f $(LIBS) kcgihtml.o kcgijson.o kcgixml.o kcgiregress.o
	rm -f test-abort-valid.core test-fcgi-abort-.core core
	rm -f $(REGRESS) $(AFL) $(REGRESS_OBJS)
	rm -rf *.dSYM regress/*.dSYM afl/*.dSYM

distclean: clean
	rm -f config.h config.log Makefile.configure
