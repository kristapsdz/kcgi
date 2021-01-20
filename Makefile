.SUFFIXES: .3 .3.html .8 .8.html .dot .svg .gnuplot .png .xml .html .in.pc .pc
.PHONY: regress afl

include Makefile.configure

# Uncomment this to disable -static linking.
# This is set to -static only for supporting systems.
# This is only for the sample program!

#LDADD_STATIC	 =

# Linux apps might one seccomp debugging.

#CPPFLAGS	+= -DSANDBOX_SECCOMP_DEBUG

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
PCS		 = kcgi.pc \
		   kcgi-html.pc \
		   kcgi-json.pc \
		   kcgi-regress.pc \
		   kcgi-xml.pc
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
MAN3S		 = man/kcgi.3 \
		   man/kcgihtml.3 \
		   man/kcgijson.3 \
		   man/kcgiregress.3 \
		   man/kcgixml.3 \
		   man/kcgi_buf_printf.3 \
		   man/kcgi_buf_putc.3 \
		   man/kcgi_buf_puts.3 \
		   man/kcgi_buf_write.3 \
		   man/kcgi_writer_disable.3 \
		   man/kcgi_strerror.3 \
		   man/khtml_attr.3 \
		   man/khtml_close.3 \
		   man/khtml_closeelem.3 \
		   man/khtml_closeto.3 \
		   man/khtml_double.3 \
		   man/khtml_elem.3 \
		   man/khtml_elemat.3 \
		   man/khtml_entity.3 \
		   man/khtml_int.3 \
		   man/khtml_ncr.3 \
		   man/khtml_open.3 \
		   man/khtml_printf.3 \
		   man/khtml_putc.3 \
		   man/khtml_puts.3 \
		   man/khtml_write.3 \
		   man/khttp_body.3 \
		   man/khttp_datetime2epoch.3 \
		   man/khttp_epoch2datetime.3 \
		   man/khttp_epoch2str.3 \
		   man/khttp_epoch2tms.3 \
		   man/khttp_fcgi_free.3 \
		   man/khttp_fcgi_init.3 \
		   man/khttp_fcgi_parse.3 \
		   man/khttp_fcgi_test.3 \
		   man/khttp_free.3 \
		   man/khttp_head.3 \
		   man/khttp_parse.3 \
		   man/khttp_printf.3 \
		   man/khttp_putc.3 \
		   man/khttp_puts.3 \
		   man/khttp_template.3 \
		   man/khttp_templatex.3 \
		   man/khttp_urlabs.3 \
		   man/khttp_urldecode.3 \
		   man/khttp_urlencode.3 \
		   man/khttp_urlpart.3 \
		   man/khttp_write.3 \
		   man/khttpbasic_validate.3 \
		   man/khttpdigest_validate.3 \
		   man/kjson_array_close.3 \
		   man/kjson_array_open.3 \
		   man/kjson_close.3 \
		   man/kjson_obj_close.3 \
		   man/kjson_obj_open.3 \
		   man/kjson_open.3 \
		   man/kjson_putbool.3 \
		   man/kjson_putdouble.3 \
		   man/kjson_putint.3 \
		   man/kjson_putnull.3 \
		   man/kjson_putstring.3 \
		   man/kjson_string_close.3 \
		   man/kjson_string_open.3 \
		   man/kjson_string_write.3 \
		   man/kmalloc.3 \
		   man/kutil_invalidate.3 \
		   man/kutil_log.3 \
		   man/kutil_openlog.3 \
		   man/kvalid_string.3 \
		   man/kxml_close.3 \
		   man/kxml_open.3 \
		   man/kxml_pop.3 \
		   man/kxml_popall.3 \
		   man/kxml_prologue.3 \
		   man/kxml_push.3 \
		   man/kxml_pushnull.3 \
		   man/kxml_putc.3 \
		   man/kxml_puts.3 \
		   man/kxml_write.3
MAN8S		 = man/kfcgi.8 
MANHTMLS	 =
.for f in $(MAN3S) $(MAN8S)
MANHTMLS	+= ${f}.html
HTMLS		+= ${f}.html
.endfor
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
		   regress/test-date2epoch \
		   regress/test-datetime2epoch \
		   regress/test-datetime-deprecated \
		   regress/test-debug-read \
		   regress/test-debug-read-long \
		   regress/test-debug-write \
		   regress/test-debug-write-long \
		   regress/test-digest \
		   regress/test-digest-auth-int \
		   regress/test-digest-auth-int-bad \
		   regress/test-epoch2datetime \
		   regress/test-epoch2str \
		   regress/test-epoch2tm \
		   regress/test-epoch2tms \
		   regress/test-epoch2ustr \
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
		   regress/test-header-builtin \
		   regress/test-html-disabled \
		   regress/test-html-noscope \
		   regress/test-html-null-strings \
		   regress/test-html-simple \
		   regress/test-html-void-elems \
		   regress/test-httpdate \
		   regress/test-invalidate \
		   regress/test-json-controlchars \
		   regress/test-json-simple \
		   regress/test-logging \
		   regress/test-logging-errors \
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
		   regress/test-urlencode-deprecated \
		   regress/test-urldecode \
		   regress/test-urldecode-deprecated \
		   regress/test-urlabs \
		   regress/test-urlpart \
		   regress/test-urlpartx \
		   regress/test-urlpart-deprecated \
		   regress/test-urlpartx-deprecated \
		   regress/test-valid-date \
		   regress/test-valid-double \
		   regress/test-valid-email \
		   regress/test-write
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
		   tutorial.css
LIBS		 = libkcgi.a \
		   libkcgihtml.a \
		   libkcgijson.a \
		   libkcgixml.a \
		   libkcgiregress.a
CURL_LIBS_PKG	 != curl-config --libs 2>/dev/null || echo "-lcurl"
CURL_CFLAGS_PKG	 != curl-config --cflags 2>/dev/null || echo ""
LIBS_PKG	 != pkg-config --libs zlib 2>/dev/null || echo "-lz"
CFLAGS_PKG	 != pkg-config --cflags zlib 2>/dev/null || echo ""

REGRESS_LIBS	  = $(CURL_LIBS_PKG) $(LIBS_PKG) $(LDADD_MD5) -lm

# The -Wno-deprecated is because the regression tests still check
# functions that have been since deprecated.

REGRESS_CFLAGS	  = $(CURL_CFLAGS_PKG) $(CFLAGS_PKG) -Wno-deprecated-declarations

all: kfcgi $(LIBS) $(PCS)

afl: $(AFL)

samples: sample samplepp sample-fcgi sample-cgi

regress: $(REGRESS)
	@for f in $(REGRESS) ; do \
		printf "%s" "./$${f}... " ; \
		./$$f >/dev/null 2>/dev/null || { echo "fail" ; exit 1 ; } ; \
		echo "ok" ; \
	done

installcgi: sample samplepp sample-fcgi sample-cgi
	$(INSTALL_PROGRAM) sample $(PREFIX)/sample.cgi
	$(INSTALL_PROGRAM) samplepp $(PREFIX)/samplepp.cgi
	$(INSTALL_PROGRAM) sample-cgi $(PREFIX)/sample-simple.cgi
	$(INSTALL_PROGRAM) sample-fcgi $(PREFIX)/sample-fcgi.cgi
	$(INSTALL_DATA) template.xml $(PREFIX)

install: all
	mkdir -p $(DESTDIR)$(LIBDIR)/pkgconfig
	mkdir -p $(DESTDIR)$(INCLUDEDIR)
	mkdir -p $(DESTDIR)$(DATADIR)
	mkdir -p $(DESTDIR)$(MAN3DIR)
	mkdir -p $(DESTDIR)$(MAN8DIR)
	mkdir -p $(DESTDIR)$(SBINDIR)
	$(INSTALL_LIB) $(LIBS) $(DESTDIR)$(LIBDIR)
	$(INSTALL_DATA) kcgi.h kcgihtml.h kcgijson.h kcgixml.h kcgiregress.h $(DESTDIR)$(INCLUDEDIR)
	$(INSTALL_DATA) $(PCS) $(DESTDIR)$(LIBDIR)/pkgconfig
	$(INSTALL_MAN) $(MAN3S) $(DESTDIR)$(MAN3DIR)
	$(INSTALL_MAN) $(MAN8S) $(DESTDIR)$(MAN8DIR)
	$(INSTALL_PROGRAM) kfcgi $(DESTDIR)$(SBINDIR)
	$(INSTALL_DATA) template.xml sample.c samplepp.cc sample-fcgi.c sample-cgi.c $(DESTDIR)$(DATADIR)

www: $(SVGS) $(SBLGS) kcgi.tgz kcgi.tgz.sha512 $(HTMLS) $(THTMLS) extending01.html atom.xml

installwww: www
	mkdir -p $(WWWDIR)/snapshots
	$(INSTALL_DATA) $(SBLGS) $(CSSS) extending01.html $(THTMLS) $(SVGS) $(HTMLS) atom.xml $(WWWDIR)
	$(INSTALL_DATA) sample.c $(WWWDIR)/sample.c.txt
	$(INSTALL_DATA) sample-fcgi.c $(WWWDIR)/sample-fcgi.c.txt
	$(INSTALL_DATA) kcgi.tgz kcgi.tgz.sha512 $(WWWDIR)/snapshots/
	$(INSTALL_DATA) kcgi.tgz $(WWWDIR)/snapshots/kcgi-$(VERSION).tgz
	$(INSTALL_DATA) kcgi.tgz.sha512 $(WWWDIR)/snapshots/kcgi-$(VERSION).tgz.sha512

distcheck: kcgi.tgz.sha512
	mandoc -Tlint -Werror $(MANS)
	newest=`grep "<h1>" versions.xml | head -1 | sed 's![ 	]*!!g'` ; \
	       [ "$$newest" = "<h1>$(VERSION)</h1>" ] || \
		{ echo "Version $(VERSION) not newest in versions.xml" 1>&2 ; exit 1 ; }
	rm -rf .distcheck
	[ "`openssl dgst -sha512 -hex kcgi.tgz`" = "`cat kcgi.tgz.sha512`" ] || \
 		{ echo "Checksum does not match." 1>&2 ; exit 1 ; }
	mkdir -p .distcheck
	( cd .distcheck && tar -zvxpf ../kcgi.tgz )
	( cd .distcheck/kcgi-$(VERSION) && ./configure PREFIX=prefix )
	( cd .distcheck/kcgi-$(VERSION) && $(MAKE) )
	( cd .distcheck/kcgi-$(VERSION) && $(MAKE) regress )
	( cd .distcheck/kcgi-$(VERSION) && $(MAKE) install )
	rm -rf .distcheck

clean:
	rm -f kcgi.tgz kcgi.tgz.sha512 $(SVGS) $(HTMLS) 
	rm -f sample samplepp samplepp.o sample-fcgi sample.o sample-fcgi.o kfcgi kfcgi.o sample-cgi sample-cgi.o
	rm -f $(SBLGS) $(THTMLS) extending01.html atom.xml
	rm -f $(LIBOBJS) compats.o 
	rm -f $(LIBS) kcgihtml.o kcgijson.o kcgixml.o kcgiregress.o regress/regress.o
	rm -f *.core
	rm -f $(REGRESS) $(AFL) regress/*.o
	rm -f $(PCS)

distclean: clean
	rm -f config.h config.log Makefile.configure

# Our compatibility layer used throughout.

compats.o: config.h

# The FastCGI manager is pretty standalone.

kfcgi: kfcgi.o compats.o
	$(CC) $(CFLAGS) -o $@ kfcgi.o compats.o $(LDADD_LIB_SOCKET)

kfcgi.o: config.h

# Regression targets.
# All regress programs ("REGRESS") build in regress/regress.o.
# Furthermore, they all link into libkcgi and libkcgiregress.
# Some of them use the JSON library, so pull those in as well.
# Of course, all need the config.h and headers.

$(REGRESS): config.h kcgi.h regress/regress.h
$(REGRESS): regress/regress.o 
$(REGRESS): libkcgi.a libkcgiregress.a libkcgijson.a libkcgihtml.a

# -lm required by kcgi-json (on some systems---better safe than sorry)

.for BIN in $(REGRESS)
$(BIN): $(BIN).c
	$(CC) $(CFLAGS) $(REGRESS_CFLAGS) -o $@ $(BIN).c regress/regress.o \
		libkcgiregress.a libkcgijson.a libkcgihtml.a libkcgi.a $(REGRESS_LIBS)
.endfor

regress/regress.o: regress/regress.h kcgiregress.h config.h

regress/regress.o: regress/regress.c
	$(CC) $(CFLAGS) $(REGRESS_CFLAGS) -c -o $@ $<

# The AFL programs call directly into libkcgi.a.
# So require that and our configuration.

.for BIN in $(AFL)
$(BIN).o: $(BIN).c config.h kcgi.h extern.h
	$(CC) $(CFLAGS) -c -o $@ $(BIN).c
$(BIN): $(BIN).o libkcgi.a
	$(CC) $(CFLAGS) $(CFLAGS_PKG) -o $@ $(BIN).o libkcgi.a $(LIBS_PKG) $(LDADD_MD5)
.endfor

# The main kcgi library.
# Pulls in all objects along with the compatibility layer.

libkcgi.a: $(LIBOBJS) compats.o
	$(AR) rs $@ $(LIBOBJS) compats.o

$(LIBOBJS): kcgi.h config.h extern.h

# Our companion libraries.
# There are many of these.

kcgihtml.o: kcgi.h config.h kcgihtml.h extern.h

kcgijson.o: kcgi.h config.h kcgijson.h extern.h

kcgixml.o: kcgi.h config.h kcgixml.h extern.h

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
	c++ $(CFLAGS) $(CFLAGS_PKG) $(LDADD_STATIC) -o $@ samplepp.cc -L. libkcgi.a $(LIBS_PKG)

sample: sample.o libkcgi.a libkcgihtml.a kcgi.h kcgihtml.h
	$(CC) -o $@ $(LDADD_STATIC) sample.o -L. libkcgihtml.a libkcgi.a $(LIBS_PKG)

sample-fcgi: sample-fcgi.o libkcgi.a kcgi.h
	$(CC) -o $@ $(LDADD_STATIC) sample-fcgi.o -L. libkcgi.a $(LIBS_PKG)

sample-cgi: sample-cgi.o 
	$(CC) -o $@ $(LDADD_STATIC) sample-cgi.o 

# Now a lot of HTML and web media files.
# These are only used with the `www' target, so we can assume
# that they're running in-house for releases.

index.html: index.xml versions.xml $(THTMLS) $(MANHTMLS)
	sblg -t index.xml -s date -o- versions.xml $(THTMLS) $(MANHTMLS) >$@

sample.c.html: sample.c
	highlight -o $@ --inline-css --doc sample.c

samplepp.cc.html: samplepp.cc
	highlight -o $@ --inline-css --doc samplepp.cc

archive.html: archive.xml versions.xml $(THTMLS)
	sblg -t archive.xml -s date -o- versions.xml $(THTMLS) >$@

$(THTMLS): tutorial.xml versions.xml $(TXMLS)

extending01.html: extending01.xml tutorial.xml
	sblg -t tutorial.xml -o- -c $< | sed "s!@VERSION@!$(VERSION)!g" >$@

.xml.html:
	sblg -t tutorial.xml -s date -o- -C $< versions.xml $(TXMLS) | \
		sed "s!@VERSION@!$(VERSION)!g" >$@

.3.3.html .8.8.html:
	( echo "<!DOCTYPE html>" ; \
	  echo "<html lang=\"en\">" ; \
	  echo "<head>" ; \
	  echo "<meta charset=\"utf-8\"/>" ; \
	  echo "<link rel=\"stylesheet\" " \
	  	"href=\"https://bsd.lv/css/mandoc.css\" />" ; \
          echo "<title>`basename $<`</title>" ; \
	  echo "</head>" ; \
	  echo "<body>" ; \
	  echo "<article data-sblg-article=\"1\"" \
	  	"data-sblg-tags=\"man\" " \
		"data-sblg-title=\"`basename $<`\">" ; \
 	  mandoc -Ofragment -Thtml $< ; \
	  echo "</article>" ; \
	  echo "</body>" ; \
	  echo "</html>" ; ) >$@

atom.xml: versions.xml
	sblg -s date -a versions.xml >$@

.dot.svg:
	dot -Tsvg -o $@ $<

.gnuplot.png:
	gnuplot $<

# Distribution files.
# Also only used with the `www' target.

kcgi.tgz.sha512: kcgi.tgz
	openssl dgst -sha512 -hex kcgi.tgz >$@

kcgi.tgz:
	mkdir -p .dist/kcgi-$(VERSION)
	mkdir -p .dist/kcgi-$(VERSION)/man
	mkdir -p .dist/kcgi-$(VERSION)/regress
	mkdir -p .dist/kcgi-$(VERSION)/afl
	install -m 0644 $(SRCS) *.in.pc .dist/kcgi-$(VERSION)
	install -m 0644 regress/*.c regress/*.h .dist/kcgi-$(VERSION)/regress
	install -m 0644 afl/*.c .dist/kcgi-$(VERSION)/afl
	install -m 0644 Makefile template.xml .dist/kcgi-$(VERSION)
	install -m 0644 $(MANS) .dist/kcgi-$(VERSION)/man
	install -m 0755 configure .dist/kcgi-$(VERSION)
	(cd .dist && tar zcf ../$@ kcgi-$(VERSION))
	rm -rf .dist

# Catch version updates.

$(PCS): kcgi.h

.in.pc.pc:
	sed -e "s!@PREFIX@!$(PREFIX)!g" \
	    -e "s!@LDADD_ZLIB@!$(LIBS_PKG)!g" \
	    -e "s!@LDADD_LIB_SOCKET@!$(LDADD_LIB_SOCKET)!g" \
	    -e "s!@LDADD_MD5@!$(LDADD_MD5)!g" \
	    -e "s!@LIBDIR@!$(LIBDIR)!g" \
	    -e "s!@INCLUDEDIR@!$(INCLUDEDIR)!g" \
	    -e "s!@VERSION@!$(VERSION)!g" $< >$@
