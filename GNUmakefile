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
		   tutorial4.xml
TUTORIALHTMLS	 = $(addsuffix .html, $(foreach xml, $(TUTORIALXMLS), $(basename $(xml) .xml)))
SBLGS		 = archive.html \
		   index.html \
		   sample.c.html
MAN3DIR	 	 = $(MANDIR)/man3
MAN8DIR	 	 = $(MANDIR)/man8
VERSION 	 = 0.10.0
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
		   man/kcgi_writer_disable.3 \
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
		   man/kutil_log.3 \
		   man/kutil_openlog.3 \
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
REGRESS		 = regress/test-abort-validator \
		   regress/test-basic \
		   regress/test-bigfile \
		   regress/test-datetime \
		   regress/test-digest \
		   regress/test-fcgi-abort-validator \
		   regress/test-fcgi-bigfile \
		   regress/test-fcgi-file-get \
		   regress/test-fcgi-header \
		   regress/test-fcgi-header-bad \
		   regress/test-fcgi-path-check \
		   regress/test-fcgi-ping \
		   regress/test-fcgi-upload \
		   regress/test-file-get \
		   regress/test-fork \
		   regress/test-gzip \
		   regress/test-gzip-bigfile \
		   regress/test-header \
		   regress/test-header-bad \
		   regress/test-httpdate \
		   regress/test-nogzip \
		   regress/test-nullqueryval \
		   regress/test-path-check \
		   regress/test-ping \
		   regress/test-post \
		   regress/test-returncode \
		   regress/test-template \
		   regress/test-upload \
		   regress/test-urlencode \
		   regress/test-urlpart \
		   regress/test-valid-date \
		   regress/test-valid-double
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
		   extending01-d.svg
CSSS	         = archive.css \
	           index.css \
	           mandoc.css \
		   tutorial.css
LIBS		 = libkcgi.a \
		   libkcgihtml.a \
		   libkcgijson.a \
		   libkcgixml.a \
		   libkcgiregress.a

all: kfcgi $(LIBS)

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

libconfig.a: config.h compats.o
	$(AR) rs $@ compats.o

kfcgi: kfcgi.o libconfig.a
	$(CC) $(CFLAGS) -o $@ kfcgi.o libconfig.a

kfcgi.o: config.h

regress/%.o: regress/%.c config.h regress/regress.h
	$(CC) $(CFLAGS) `curl-config --cflags` -o $@ -c $<

regress/%: regress/%.o regress/regress.o libkcgiregress.a libkcgi.a
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

$(LIBOBJS) sample.o sample-fcgi.o kcgihtml.o kcgijson.o kcgixml.o: kcgi.h

$(LIBOBJS) kcgihtml.o kcgijson.o kcgixml.o kcgiregress.o: config.h extern.h

compats.o: config.h

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
	install -m 0444 $(LIBS) $(DESTDIR)$(LIBDIR)
	install -m 0444 kcgi.h kcgihtml.h kcgijson.h kcgixml.h kcgiregress.h $(DESTDIR)$(INCLUDEDIR)
	install -m 0444 $(MAN3S) $(DESTDIR)$(MAN3DIR)
	install -m 0444 $(MAN8S) $(DESTDIR)$(MAN8DIR)
	install -m 0555 kfcgi $(DESTDIR)$(SBINDIR)
	install -m 0444 template.xml sample.c sample-fcgi.c sample-cgi.c $(DESTDIR)$(DATADIR)

uninstall:
	$(foreach $@_LIB, $(LIBS), rm -f $(DESTDIR)$(LIBDIR)/$($@_LIB);)
	$(foreach $@_INC, kcgi.h kcgihtml.h kcgijson.h kcgixml.h kcgiregress.h, rm -f $(DESTDIR)$(INCLUDEDIR)/$($@_INC);)
	$(foreach $@_MAN, $(MAN3S), rm -f $(DESTDIR)$(MAN3DIR)/$(notdir $($@_MAN));)
	$(foreach $@_MAN, $(MAN8S), rm -f $(DESTDIR)$(MAN8DIR)/$(notdir $($@_MAN));)
	rm -f $(DESTDIR)$(SBINDIR)/kfcgi
	$(foreach $@_TMP, template.xml sample.c sample-fcgi.c sample-cgi.c, rm -f $(DESTDIR)$(DATADIR)/$($@_TMP);)

sample: sample.o libkcgi.a libkcgihtml.a
	$(CC) -o $@ $(STATIC) sample.o -L. libkcgihtml.a libkcgi.a -lz

sample-fcgi: sample-fcgi.o libkcgi.a 
	$(CC) -o $@ $(STATIC) sample-fcgi.o -L. libkcgi.a -lz

sample-cgi: sample-cgi.o 
	$(CC) -o $@ $(STATIC) sample-cgi.o 

www: $(SVGS) $(SBLGS) kcgi.tgz kcgi.tgz.sha512 $(HTMLS) $(TUTORIALHTMLS) extending01.html atom.xml

installwww: www
	mkdir -p $(WWWDIR)/snapshots
	install -m 0444 $(SBLGS) $(CSSS) extending01.html $(TUTORIALHTMLS) $(SVGS) $(HTMLS) atom.xml $(WWWDIR)
	install -m 0444 sample.c $(WWWDIR)/sample.c.txt
	install -m 0444 sample-fcgi.c $(WWWDIR)/sample-fcgi.c.txt
	install -m 0444 kcgi.tgz kcgi.tgz.sha512 $(WWWDIR)/snapshots/
	install -m 0444 kcgi.tgz $(WWWDIR)/snapshots/kcgi-$(VERSION).tgz
	install -m 0444 kcgi.tgz.sha512 $(WWWDIR)/snapshots/kcgi-$(VERSION).tgz.sha512

index.html: index.xml versions.xml $(TUTORIALHTMLS)
	sblg -t index.xml -s date -o- versions.xml $(TUTORIALHTMLS) | sed "s!@VERSION@!$(VERSION)!g" >$@

sample.c.html: sample.c
	highlight -o $@ --inline-css --doc sample.c

archive.html: archive.xml versions.xml $(TUTORIALHTMLS)
	sblg -t archive.xml -s date -o- versions.xml $(TUTORIALHTMLS) | sed "s!@VERSION@!$(VERSION)!g" >$@

$(TUTORIALHTMLS): tutorial.xml versions.xml $(TUTORIALXMLS)

extending01.html: extending01.xml tutorial.xml
	sblg -t tutorial.xml -o- -c $< | sed "s!@VERSION@!$(VERSION)!g" >$@

.xml.html:
	sblg -t tutorial.xml -s date -o- -C $< versions.xml $(TUTORIALXMLS) | sed "s!@VERSION@!$(VERSION)!g" >$@

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
	cp GNUmakefile template.xml .dist/kcgi-$(VERSION)
	cp $(MANS) .dist/kcgi-$(VERSION)/man
	cp configure .dist/kcgi-$(VERSION)
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
	rm -f kcgi.tgz kcgi.tgz.sha512 $(SVGS) $(HTMLS) sample sample-fcgi sample.o sample-fcgi.o kfcgi kfcgi.o sample-cgi sample-cgi.o
	rm -f $(SBLGS) $(TUTORIALHTMLS) extending01.html atom.xml
	rm -f libconfig.a
	rm -f $(LIBOBJS) compats.o
	rm -f $(LIBS) kcgihtml.o kcgijson.o kcgixml.o kcgiregress.o
	rm -f test-abort-valid.core core
	rm -f $(REGRESS) $(AFL) $(REGRESS_OBJS)
	rm -rf *.dSYM regress/*.dSYM afl/*.dSYM

distclean: clean
	rm -f config.h config.log Makefile.configure
