## Synopsis

kcgi is an open source CGI and FastCGI library for C web applications.
It is minimal, secure, and auditable.
This is the README file for display with
[GitHub](https://www.github.com), which hosts a read-only source
repository of the project. 
To keep up to date with the current stable release of kcgi, visit the
[kcgi website](http://kristaps.bsd.lv/kcgi).

## Code Example

Implementing a CGI or FastCGI application with kcgi is easy.
One usually specifies the pages recognised by the application and the
known form inputs.
kcgi then parses the request.

	#include <stdint.h>
	#include <stdlib.h>
	#include <kcgi.h>
	 
	int main(void) {
	  struct kreq r;
	  const char *page = "index";
	  if (KCGI_OK != khttp_parse(&r, NULL, 0, &page, 1, 0))
		return(EXIT_FAILURE);
	  khttp_head(&r, kresps[KRESP_STATUS], "%s", khttps[KHTTP_200]);
	  khttp_head(&r, kresps[KRESP_CONTENT_TYPE], "%s", kmimetypes[r.mime]);
	  khttp_body(&r);
	  khttp_puts(&r, "Hello, world!");
	  khttp_free(&r);
	  return(EXIT_SUCCESS);
	}

## Installation

kcgi works out-of-the-box with modern UNIX systems.
Simply download the latest version's [source
archive](http://kristaps.bsd.lv/kcgi/snapshots/kcgi.tar.gz) (or download
the project from GitHub), compile with `make`, then `sudo make install`.
Your operating system might already have kcgi as one of its third-party
libraries: check to make sure!

## API Reference

See the [kcgi(3) manpage](http://kristaps.bsd.lv/kcgi/kcgi.3.html) for
complete library documentation.

## Tests

The system contains a full regression suite and is also built to work
with [AFL](http://lcamtuf.coredump.cx/afl/).
See the [kcgi website](http://kristaps.bsd.lv/kcgi) for details on how
to deploy (or write) tests.

## License

All sources use the ISC (like OpenBSD) license.
