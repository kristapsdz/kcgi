## Synopsis

kcgi is an open source CGI and FastCGI library for C web applications.
It is minimal, secure, and auditable; and fits within your
[BCHS](https://learnbchs.org) software stack.  This repository is
consists of bleeding-edge code between versions: to keep up to date with
the current stable release of kcgi, visit the [kcgi
website](https://kristaps.bsd.lv/kcgi).

To get started with a kcgi project, see
[kcgi-framework](https://github.com/kristapsdz/kcgi-framework) for a set
of files to get your project going.

## Code Example

Implementing a CGI or FastCGI application with kcgi is easy (for values
of easy greater than "knows C").
One usually specifies the pages recognised by the application and the
known form inputs.
kcgi then parses the request.

``` c
#include <stdarg.h> /* va_list */
#include <stdint.h> /* int64_t */
#include <stdlib.h>
#include <unistd.h> /* ssize_t */
#include <kcgi.h>
 
int main(void) {
  struct kreq r;
  const char *page = "index";
  if (KCGI_OK != khttp_parse(&r, NULL, 0, &page, 1, 0))
	return(EXIT_FAILURE);
  khttp_head(&r, kresps[KRESP_STATUS],
	"%s", khttps[KHTTP_200]);
  khttp_head(&r, kresps[KRESP_CONTENT_TYPE], 
	"%s", kmimetypes[r.mime]);
  khttp_body(&r);
  khttp_puts(&r, "Hello, world!\n");
  khttp_free(&r);
  return(EXIT_SUCCESS);
}
```

## Installation

kcgi works out-of-the-box with modern UNIX systems.
Simply download the latest version's [source
archive](https://kristaps.bsd.lv/kcgi/snapshots/kcgi.tar.gz) (or download
the project from GitHub), compile with `make`, then `sudo make install`
(or using `doas`).
Your operating system might already have kcgi as one of its third-party
libraries: check to make sure!

## API Reference

See the [kcgi(3) manpage](https://kristaps.bsd.lv/kcgi/kcgi.3.html) for
complete library documentation.
You can also browse [all
functions](https://kristaps.bsd.lv/kcgi/functions.html).

## Tests

The system contains a full regression suite and is also built to work
with [AFL](http://lcamtuf.coredump.cx/afl/).
See the [kcgi website](https://kristaps.bsd.lv/kcgi) for details on how
to deploy (or write) tests.

## License

All sources use the ISC (like OpenBSD) license.
See the [LICENSE.md](LICENSE.md) file for details.
