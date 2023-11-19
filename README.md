## Synopsis

**kcgi** is an open source CGI and FastCGI library for C/C++ web applications.
It's minimal, secure, auditable, and fits within your
[BCHS](https://learnbchs.org) software stack.

To keep up to date with the current stable release of **kcgi**, visit
https://kristaps.bsd.lv/kcgi.  The website also contains canonical
installation, deployment, examples, and usage documentation.

## Installation

Building the bleeding-edge version of **kcgi** (instead of from your
system's packages or a stable version) is similar to building the source
releases.

You'll a C compiler, zlib (zlib-dev on some systems), and BSD make
(bmake on some systems) for building.

Begin by cloning or downloading.  Then configure with `./configure`,
compile with `make` (BSD make, so it may be `bmake` on your system),
then `make install` (or use `sudo` or `doas`, if applicable).  To
install in an alternative directory to `/usr/local`, set the `PREFIX`
variable when you run `configure`.

```sh
./configure PREFIX=~/.local
make
make install
```

If you plan on using `pkg-config` with the above invocation, make sure
that *~/.local/lib/pkgconfig* is recognised as a path to package
specifications.  You'll also want to make sure that `man` can access the
installed location of *~/.local/man*, in this case.

A common idiom for deploying on Linux is to use
[libbsd](https://libbsd.freedesktop.org/wiki/) as noted in the
[oconfigure](https://github.com/kristapsdz/oconfigure) documentation:

```
CFLAGS=$(pkg-config --cflags libbsd-overlay) \
    ./configure LDFLAGS=$(pkg-config --libs libbsd-overlay)
```

If you intend to run on Linux with seccomp sandboxing, pass the
following to the configuration:

```
./configure CPPFLAGS="-DENABLE_SECCOMP_FILTER=1"
```

You'll need the Linux kernel headers installed, which are usually by
default but sometimes require the *linux-headers* package or similar.


## Tests

It's useful to run the installed regression tests on the bleeding edge
sources.  (Again, this uses BSD make, so it may be `bmake` on your
system.)

```sh
make regress
```

The system contains a full regression suite and is also built to work
with [AFL](http://lcamtuf.coredump.cx/afl/).  To run some of the bundled
tests, use the binaries compiled into the `afl` directory.  (Again, this
uses BSD make, so it may be `bmake` on your system.)

```sh
make afl
cd afl
afl-fuzz -i in/urlencoded -o out -- ./afl-urlencoded
```

## Automated testing

The public GitHub repository repository for **kcgi** uses automated
testing on each check-in to run the regression tests.  The following
systems are checked:

- Alpine/musl Linux (latest, aarch64, sandboxed)
- Alpine/musl Linux (latest, armv7, sandboxed)
- Alpine/musl Linux (latest, ppc64le, sandboxed)
- Alpine/musl Linux (latest, s390x, sandboxed)
- Alpine/musl Linux (latest, x86\_64, sandboxed)
- FreeBSD (latest, x86\_64, sandboxed)
- Mac OS X (latest, x86, sandboxed)
- OpenBSD (latest, x86\_64, sandboxed)
- Ubuntu/glibc Linux (latest, x86\_64, un-sandboxed)
- Ubuntu/glibc Linux (latest, x86\_64, sandboxed)
- Ubuntu/glibc Linux (latest, x86\_64, sandboxed, libbsd)

These are also run weekly to catch any changes as new operating system
features come into play.

## License

All sources use the ISC (like OpenBSD) license.
See the [LICENSE.md](LICENSE.md) file for details.
