PREFIX = /usr/local
DATADIR = $(PREFIX)/share/kcgi
MANDIR = $(PREFIX)/man/man3
VERSION = 0.1.2

www: index.html kcgi.tgz kcgi.3.html

install:
	mkdir -p $(MANDIR)
	mkdir -p $(DATADIR)
	sed -e "s!@VERSION@!$(VERSION)!g" -e "s!@DATADIR@!$(DATADIR)!g" kcgi.h >kcgi.h~
	sed -e "s!@VERSION@!$(VERSION)!g" -e "s!@DATADIR@!$(DATADIR)!g" kcgi.3 >kcgi.3~
	install -m 0644 sample.c kcgi-local.h Makefile.sample $(DATADIR)
	install -m 0644 kcgi.3~ $(MANDIR)/kcgi.3
	install -m 0644 kcgi.h~ $(DATADIR)/kcgi.h
	rm -f kcgi.3~ kcgi.h~

index.html: index.xml
	sed "s!@VERSION@!$(VERSION)!g" index.xml >$@

kcgi.3.html: kcgi.3
	sed -e "s!@VERSION@!$(VERSION)!g" -e "s!@DATADIR@!$(DATADIR)!g" kcgi.3 | mandoc -Thtml >$@

kcgi.tgz:
	mkdir -p .dist/kcgi-$(VERSION)
	cp Makefile Makefile.sample sample.c kcgi-local.h kcgi.h kcgi.3 .dist/kcgi-$(VERSION)
	(cd .dist && tar zcf ../$@ kcgi-$(VERSION))
	rm -rf .dist

clean:
	rm -f kcgi.tgz index.html kcgi.3.html
