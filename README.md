# Introduction

**kcgi** is an open source CGI and FastCGI library for C/C++ web applications.
It's minimal, secure, auditable, and fits within your
[BCHS](https://learnbchs.org) software stack.

To keep up to date with the current stable release of **kcgi**, visit
https://kristaps.bsd.lv/kcgi.  The website also contains canonical
installation, deployment, examples, and usage documentation.

# Installation

You'll need a C compiler ([gcc](https://gcc.gnu.org/) or
[clang](https://clang.llvm.org/)), [zlib](https://zlib.net) (*zlib* or
*zlib-dev* for some package managers), and BSD make (*bmake* for some
managers) for building.

On some Linux systems, you might additionally need the Linux kernel
headers installed using the *linux-headers* package or similar.

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

A common idiom for deploying on Linux is to use
[libbsd](https://libbsd.freedesktop.org/wiki/) as noted in the
[oconfigure](https://github.com/kristapsdz/oconfigure) documentation:

```
CFLAGS=$(pkg-config --cflags libbsd-overlay) \
    ./configure LDFLAGS=$(pkg-config --libs libbsd-overlay)
make
make install
```

# Testing

It's useful to run the installed regression tests on the bleeding edge
sources.  (Again, this uses BSD make, so it may be `bmake` on your
system.)  You'll need [libcurl](https://curl.se/libcurl/) installed
(*curl-dev*, *libcurl-dev*, or *libcurl4-openssl-dev* with some package
managers).

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

The public GitHub repository repository for **kcgi** uses automated
testing on each check-in to run the regression tests.  These automated
tests are primarily aimed at Linux, whose security mechanism requires
constant maintenance.  The following systems are checked:

- Alpine/musl Linux (latest, aarch64, sandboxed)
- Alpine/musl Linux (latest, armv7, sandboxed)
- Alpine/musl Linux (latest, ppc64le, sandboxed)
- Alpine/musl Linux (latest, s390x, sandboxed)
- Alpine/musl Linux (latest, x86\_64, sandboxed)
- FreeBSD (latest, x86\_64, sandboxed)
- Mac OS X (latest, x86, sandboxed)
- Ubuntu/glibc Linux (latest, x86\_64, un-sandboxed)
- Ubuntu/glibc Linux (latest, x86\_64, sandboxed)
- Ubuntu/glibc Linux (latest, x86\_64, sandboxed, libbsd)

Development is primarily on OpenBSD.

These are also run weekly to catch any changes as new operating system
features come into play.

# License

All sources use the ISC (like OpenBSD) license.
See the [LICENSE.md](LICENSE.md) file for details.
