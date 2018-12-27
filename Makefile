.SUFFIXES: .3 .3.html .8 .8.html .dot .svg .gnuplot .png .xml .html
.PHONY: regress afl

include Makefile.configure

# Comment if you don't need statically linked.
# This is only for the sample program!
STATIC 		 = -static

# Linux's fpclassify needs -lm and turn on seccomp debugging.
#LIBADD		+= -lm
#CPPFLAGS	+= -DSANDBOX_SECCOMP_DEBUG

# FreeBSD requires -lmd for MD5.
#LIBADD		+= -lmd

# Mac OS X doesn't support static linking and depcrecates daemon(3).
#STATIC 	 = 
#CFLAGS		+= -Wno-deprecated-declarations

# You probably don't need to change anything else...

WWWDIR		 = /var/www/vhosts/kristaps.bsd.lv/htdocs/kcgi
DATADIR 	 = $(SHAREDIR)/kcgi
TXMLS		 = tutorial0.xml \
		   tutorial1.xml \
		   tutorial2.xml \
		   tutorial3.xml \
		   tutorial4.xml \
		   tutorial5.xml \
		   tutorial6.xml
THTMLS		 = tutorial0.html \
		   tutorial1.html \
		   tutorial2.html \
		   tutorial3.html \
		   tutorial4.html \
		   tutorial5.html \
		   tutorial6.html
SBLGS		 = archive.html \
		   index.html \
		   sample.c.html \
		   samplepp.cc.html
MAN3DIR	 	 = $(MANDIR)/man3
MAN8DIR	 	 = $(MANDIR)/man8
VMAJOR		!= grep 'define	KCGI_VMAJOR' kcgi.h | cut -f3
VMINOR		!= grep 'define	KCGI_VMINOR' kcgi.h | cut -f3
VBUILD		!= grep 'define	KCGI_VBUILD' kcgi.h | cut -f3
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
		   template.o \
		   wrappers.o
HTMLS		 = man/kcgi.3.html \
		   man/kcgihtml.3.html \
		   man/kcgijson.3.html \
		   man/kcgiregress.3.html \
		   man/kcgixml.3.html \
		   man/kcgi_buf_write.3.html \
		   man/kcgi_writer_disable.3.html \
		   man/kcgi_strerror.3.html \
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
		   man/kutil_epoch2str.3.html \
		   man/kutil_invalidate.3.html \
		   man/kutil_log.3.html \
		   man/kutil_openlog.3.html \
		   man/kutil_urlencode.3.html \
		   man/kvalid_string.3.html \
		   man/kfcgi.8.html \
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
		   regress/test-basic-curl \
		   regress/test-bigfile \
		   regress/test-buf \
		   regress/test-datetime \
		   regress/test-digest \
		   regress/test-digest-auth-int \
		   regress/test-digest-auth-int-bad \
		   regress/test-fcgi-abort-validator \
		   regress/test-fcgi-bigfile \
		   regress/test-fcgi-file-get \
		   regress/test-fcgi-header \
		   regress/test-fcgi-header-bad \
		   regress/test-fcgi-path-check \
		   regress/test-fcgi-ping \
		   regress/test-fcgi-ping-double \
		   regress/test-fcgi-upload \
		   regress/test-file-get \
		   regress/test-fork \
		   regress/test-gzip \
		   regress/test-gzip-bigfile \
		   regress/test-header \
		   regress/test-header-bad \
		   regress/test-httpdate \
		   regress/test-invalidate \
		   regress/test-json-controlchars \
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
		   regress/test-urldecode \
		   regress/test-urlpart \
		   regress/test-valid-date \
		   regress/test-valid-double 
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

samples: sample samplepp sample-fcgi sample-cgi

regress: $(REGRESS)
	@for f in $(REGRESS) ; do \
		/bin/echo -n "./$${f}... " ; \
		./$$f >/dev/null 2>/dev/null || { /bin/echo "fail" ; exit 1 ; } ; \
		/bin/echo "ok" ; \
	done

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

#uninstall:
#	$(foreach $@_LIB, $(LIBS), rm -f $(DESTDIR)$(LIBDIR)/$($@_LIB);)
#	$(foreach $@_INC, kcgi.h kcgihtml.h kcgijson.h kcgixml.h kcgiregress.h, rm -f $(DESTDIR)$(INCLUDEDIR)/$($@_INC);)
#	$(foreach $@_MAN, $(MAN3S), rm -f $(DESTDIR)$(MAN3DIR)/$(notdir $($@_MAN));)
#	$(foreach $@_MAN, $(MAN8S), rm -f $(DESTDIR)$(MAN8DIR)/$(notdir $($@_MAN));)
#	rm -f $(DESTDIR)$(SBINDIR)/kfcgi
#	$(foreach $@_TMP, template.xml sample.c samplepp.cc sample-fcgi.c sample-cgi.c, rm -f $(DESTDIR)$(DATADIR)/$($@_TMP);)

www: $(SVGS) $(SBLGS) kcgi.tgz kcgi.tgz.sha512 $(HTMLS) $(THTMLS) extending01.html atom.xml

installwww: www
	mkdir -p $(WWWDIR)/snapshots
	$(INSTALL_DATA) $(SBLGS) $(CSSS) extending01.html $(THTMLS) $(SVGS) $(HTMLS) atom.xml $(WWWDIR)
	$(INSTALL_DATA) sample.c $(WWWDIR)/sample.c.txt
	$(INSTALL_DATA) sample-fcgi.c $(WWWDIR)/sample-fcgi.c.txt
	$(INSTALL_DATA) kcgi.tgz kcgi.tgz.sha512 $(WWWDIR)/snapshots/
	$(INSTALL_DATA) kcgi.tgz $(WWWDIR)/snapshots/kcgi-$(VERSION).tgz
	$(INSTALL_DATA) kcgi.tgz.sha512 $(WWWDIR)/snapshots/kcgi-$(VERSION).tgz.sha512

clean:
	rm -f kcgi.tgz kcgi.tgz.sha512 $(SVGS) $(HTMLS) 
	rm -f sample samplepp samplepp.o sample-fcgi sample.o sample-fcgi.o kfcgi kfcgi.o sample-cgi sample-cgi.o
	rm -f $(SBLGS) $(THTMLS) extending01.html atom.xml
	rm -f $(LIBOBJS) compats.o 
	rm -f $(LIBS) kcgihtml.o kcgijson.o kcgixml.o kcgiregress.o regress/regress.o
	rm -f *.core
	rm -f $(REGRESS) $(AFL) regress/*.o

distclean: clean
	rm -f config.h config.log Makefile.configure

# Our compatibility layer used throughout.

compats.o: config.h

# The FastCGI manager is pretty standalone.

kfcgi: kfcgi.o compats.o
	$(CC) $(CFLAGS) -o $@ kfcgi.o compats.o

kfcgi.o: config.h

# Regression targets.
# All regress programs ("REGRESS") build in regress/regress.o.
# Furthermore, they all link into libkcgi and libkcgiregress.
# Some of them use the JSON library, so pull those in as well.
# Of course, all need the config.h and headers.

$(REGRESS): config.h kcgi.h regress/regress.h
$(REGRESS): regress/regress.o libkcgi.a libkcgiregress.a libkcgijson.a

.for BIN in $(REGRESS)
$(BIN): $(BIN).c
	$(CC) $(CFLAGS) `curl-config --cflags` -o $@ $(BIN).c \
		regress/regress.o libkcgiregress.a libkcgijson.a \
		libkcgi.a `curl-config --libs` -lz $(LIBADD)
.endfor

regress/regress.o: regress/regress.h kcgiregress.h config.h

regress/regress.o: regress/regress.c
	$(CC) $(CFLAGS) `curl-config --cflags` -c -o $@ $<

# The AFL programs call directly into libkcgi.a.
# So require that and our configuration.

.for BIN in $(AFL)
$(BIN).o: $(BIN).c config.h kcgi.h extern.h
	$(CC) $(CFLAGS) -c -o $@ $(BIN).c
$(BIN): $(BIN).o libkcgi.a
	$(CC) $(CFLAGS) -o $@ $(BIN).o libkcgi.a -lz
.endfor

# The main kcgi library.
# Pulls in all objects along with the compatibility layer.

libkcgi.a: $(LIBOBJS) compats.o
	$(AR) rs $@ $(LIBOBJS) compats.o

$(LIBOBJS): kcgi.h config.h extern.h

# Our companion libraries.
# There are many of these.

kcgihtml.o: kcgi.h config.h kcgihtml.h

kcgijson.o: kcgi.h config.h kcgijson.h

kcgixml.o: kcgi.h config.h kcgixml.h

kcgiregress.o: config.h kcgiregress.h

libkcgihtml.a: kcgihtml.o
	$(AR) rs $@ kcgihtml.o

libkcgijson.a: kcgijson.o
	$(AR) rs $@ kcgijson.o

libkcgixml.a: kcgixml.o
	$(AR) rs $@ kcgixml.o

libkcgiregress.a: kcgiregress.o
	$(AR) rs $@ kcgiregress.o

# Sample programs.
# These demonstrate FastCGI, CGI, and standard.

samplepp: samplepp.cc libkcgi.a libkcgihtml.a kcgi.h
	c++ $(CFLAGS) $(STATIC) -o $@ samplepp.cc -L. libkcgi.a -lz

sample: sample.o libkcgi.a libkcgihtml.a kcgi.h kcgihtml.h
	$(CC) -o $@ $(STATIC) sample.o -L. libkcgihtml.a libkcgi.a -lz

sample-fcgi: sample-fcgi.o libkcgi.a kcgi.h
	$(CC) -o $@ $(STATIC) sample-fcgi.o -L. libkcgi.a -lz

sample-cgi: sample-cgi.o 
	$(CC) -o $@ $(STATIC) sample-cgi.o 

# Now a lot of HTML and web media files.
# These are only used with the `www' target, so we can assume
# that they're running in-house for releases.

index.html: index.xml versions.xml $(THTMLS)
	sblg -t index.xml -s date -o- versions.xml $(THTMLS) | \
		sed "s!@VERSION@!$(VERSION)!g" >$@

sample.c.html: sample.c
	highlight -o $@ --inline-css --doc sample.c

samplepp.cc.html: samplepp.cc
	highlight -o $@ --inline-css --doc samplepp.cc

archive.html: archive.xml versions.xml $(THTMLS)
	sblg -t archive.xml -s date -o- versions.xml $(THTMLS) | \
		sed "s!@VERSION@!$(VERSION)!g" >$@

$(THTMLS): tutorial.xml versions.xml $(TXMLS)

extending01.html: extending01.xml tutorial.xml
	sblg -t tutorial.xml -o- -c $< | sed "s!@VERSION@!$(VERSION)!g" >$@

.xml.html:
	sblg -t tutorial.xml -s date -o- -C $< versions.xml $(TXMLS) | \
		sed "s!@VERSION@!$(VERSION)!g" >$@

.3.3.html:
	mandoc -Thtml -Ostyle=mandoc.css $< >$@

.8.8.html:
	mandoc -Thtml -Ostyle=mandoc.css $< >$@

functions.html: functions.xml genindex.sh
	sh ./genindex.sh functions.xml | \
		sed "s!@VERSION@!$(VERSION)!g" >$@

atom.xml: versions.xml
	sblg -s date -a versions.xml >$@

.dot.svg:
	dot -Tsvg -o $@ $<

.gnuplot.png:
	gnuplot $<

# Distribution files.
# Also only used with the `www' target.

kcgi.tgz.sha512: kcgi.tgz
	openssl dgst -sha512 kcgi.tgz >$@

kcgi.tgz:
	mkdir -p .dist/kcgi-$(VERSION)
	mkdir -p .dist/kcgi-$(VERSION)/man
	mkdir -p .dist/kcgi-$(VERSION)/regress
	mkdir -p .dist/kcgi-$(VERSION)/afl
	install -m 0644 $(SRCS) .dist/kcgi-$(VERSION)
	install -m 0644 regress/*.c regress/*.h .dist/kcgi-$(VERSION)/regress
	install -m 0644 afl/*.c .dist/kcgi-$(VERSION)/afl
	install -m 0644 Makefile template.xml .dist/kcgi-$(VERSION)
	install -m 0644 $(MANS) .dist/kcgi-$(VERSION)/man
	install -m 0755 configure .dist/kcgi-$(VERSION)
	(cd .dist && tar zcf ../$@ kcgi-$(VERSION))
	rm -rf .dist
