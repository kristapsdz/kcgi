## Synopsis

**kcgi** is an open source CGI and FastCGI library for C/C++ web applications.
It's minimal, secure, auditable, and fits within your
[BCHS](https://learnbchs.org) software stack.

To keep up to date with the current stable release of **kcgi**, visit
https://kristaps.bsd.lv/kcgi.  The website also contains canonical
installation, deployment, examples, and usage documentation.

## Installation

To use the bleeding-edge version of **kcgi** (instead of from your system's
packages or a stable version), the process it the similar as for source
releases.

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

## License

All sources use the ISC (like OpenBSD) license.
See the [LICENSE.md](LICENSE.md) file for details.
