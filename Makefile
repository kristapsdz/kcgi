CFLAGS += -g -W -Wall -Wstrict-prototypes -Wno-unused-parameter -Wwrite-strings -DHAVE_CONFIG_H
# Comment this if you don't want zlib.
CFLAGS += -DHAVE_ZLIB
PREFIX = /usr/local
DATADIR = $(PREFIX)/share/kcgi
MANDIR = $(PREFIX)/man/man3
LIBDIR = $(PREFIX)/lib
INCLUDEDIR = $(PREFIX)/include
VERSION = 0.1.14
WWWDIR = /usr/vhosts/kristaps.bsd.lv/www/htdocs/kcgi

all: sample 

mime2c: mime2c.o
	$(CC) -o $@ mime2c.o -lutil

libkcgi.a: kcgi.o compat.o
	$(AR) rs $@ kcgi.o compat.o

kcgi.o sample.o: kcgi.h

kcgi.o compat.o: config.h

config.h: config.h.pre config.h.post configure test-memmem.c test-strtonum.c
	rm -f config.log
	CC="$(CC)" CFLAGS="$(CFLAGS)" ./configure

installcgi: sample 
	install -m 0755 sample $(PREFIX)/sample.cgi
	install -m 0755 sample $(PREFIX)
	install -m 0444 template.xml $(PREFIX)

install: libkcgi.a
	sed -e "s!@VERSION@!$(VERSION)!g" -e "s!@DATADIR@!$(DATADIR)!g" kcgi.h >kcgi.h~
	sed -e "s!@VERSION@!$(VERSION)!g" -e "s!@DATADIR@!$(DATADIR)!g" kcgi.3 >kcgi.3~
	mkdir -p $(LIBDIR)
	mkdir -p $(INCLUDEDIR)
	mkdir -p $(DATADIR)
	mkdir -p $(MANDIR)
	install -m 0444 libkcgi.a $(LIBDIR)
	install -m 0444 kcgi.h~ $(INCLUDEDIR)/kcgi.h
	install -m 0444 kcgi.3~ $(MANDIR)/kcgi.3
	install -m 0444 template.xml sample.c $(DATADIR)
	rm -f kcgi.h~ kcgi.3~

sample: sample.o libkcgi.a
	$(CC) -o $@ sample.o -L. -lkcgi -lz

www: index.html kcgi-$(VERSION).tgz kcgi.3.html

installwww: www
	mkdir -p $(WWWDIR)/snapshots
	install -m 0444 index.html kcgi.3.html $(WWWDIR)
	install -m 0444 sample.c $(WWWDIR)/sample.c.txt
	install -m 0444 kcgi-$(VERSION).tgz $(WWWDIR)/snapshots/
	install -m 0444 kcgi-$(VERSION).tgz $(WWWDIR)/snapshots/kcgi.tgz

index.html: index.xml
	sed "s!@VERSION@!$(VERSION)!g" index.xml >$@

kcgi.3.html: kcgi.3
	sed -e "s!@VERSION@!$(VERSION)!g" -e "s!@DATADIR@!$(DATADIR)!g" kcgi.3 | mandoc -Thtml >$@

kcgi-$(VERSION).tgz:
	mkdir -p .dist/kcgi-$(VERSION)
	cp Makefile sample.c kcgi.c kcgi.h kcgi.3 template.xml .dist/kcgi-$(VERSION)
	cp compat.c configure config.h.pre config.h.post test-memmem.c test-strtonum.c .dist/kcgi-$(VERSION)
	(cd .dist && tar zcf ../$@ kcgi-$(VERSION))
	rm -rf .dist

clean:
	rm -f kcgi-$(VERSION).tgz index.html kcgi.3.html
	rm -f libkcgi.a kcgi.o sample.o sample compat.o
	rm -f config.log config.h
	rm -f test-memmem test-strtonum 
	rm -f test-memmem.o test-strtonum.o
	rm -rf *.dSYM
