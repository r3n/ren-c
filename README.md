![Ren-C Logo][100]

# Ren-C
[![Build Status][101]](https://travis-ci.com/github/metaeducation/ren-c)


**Ren-C** is a branch of the [Apache 2.0 open-sourced][1] [Rebol 3][2] [codebase](https://github.com/rebol/rebol).

[1]: http://www.rebol.com/cgi-bin/blog.r?view=0519
[2]: https://en.wikipedia.org/wiki/Rebol

The goal of the project isn't to be a "new" language, but to solve many of the
outstanding design problems historically present in Rebol.  Several of these
problems have been solved already.  For progress and notes on these issues, a
[Trello board][3] is semi-frequently updated to reflect a summary of changes.

[3]: https://trello.com/b/l385BE7a/rebol3-porting-guide-ren-c-branch

The project's name comes from the idea that it is a C implementation of the
"REadable Notation" (a name given to Rebol's file format).  It is deliberately
transitional--because rather than be a brand or product in its own right,
Ren-C intends to provide smooth APIs for embedding an interpreter in C
programs...hopefully eventually `rebol.exe` itself.

One of these APIs (libRebol) is "user-friendly" to C programmers, allowing
them to avoid the  low-level concerns of the interpreter and just run snippets
of code mixed with values, as easily as:

    int x = 1020;
    REBVAL *negate = rebValue("get 'negate");  // runs code, returns value

    rebElide("print [", rebI(x), "+ (2 *", negate, "358)]");

    // Would print 304--e.g. `1020 + (2 * -358)`, rebElide() returns C void.

The other API (libRebolCore) would offer nearly the full range of power that
is internally offered to the core.  It would allow one to pick apart value
cells and write extensions that are equally efficient to built-in natives like
REDUCE.  This more heavyweight API would be used by extensions for which
performance is critical.

The current way to explore the new features of Ren-C is using the `r3`
console.  It is *significantly* enhanced from the open-sourced R3-Alpha...with
much of its behavior coming from [userspace Rebol code][4] (as opposed to
hardcoded C).  In addition to multi-line editing and UTF-8 support, it
[can be "skinned"][5] and configured in various ways, and non-C programmers
can easily help contribute to enhancing it.

[4]: https://github.com/metaeducation/ren-c/blob/master/src/os/host-console.r 
[5]: https://github.com/r3n/reboldocs/wiki/User-and-Console 

A C++ binding is also available, and for those interested in a novel
application of this code, they might want to see the experimental console
based on it and Qt: [Ren Garden][6].

[6]: http://rencpp.hostilefork.com

In doing this work, the hope is to provide an artifact that would rally common
usage between the [mainline builds][7], community builds, and those made by
[Atronix Engineering][8] and [Saphirion AG][9].

[7]: http://rebolsource.net
[8]: http://www.atronixengineering.com/downloads
[9]: http://development.saphirion.com/rebol/saphir/


## Community

To promote community's participation in public forums, development discussion
for Ren-C generally takes place in the [`Rebol*` StackOverflow Chat][10].

[10]: http://rebolsource.net/go/chat-faq

There is [a Discourse forum][11] available for more long-form discussion.

[11]: https://forum.rebol.info

It's also possible to contact the developers via [Ren-C GitHub Issues][11].
This should be limited to questions regarding the Ren-C builds specifically,
as overall language design wishes and debates in the [`rebol-issues`][12]
repository of Rebol's GitHub.

[12]: https://github.com/metaeducation/ren-c/issues
[13]: https://github.com/rebol/rebol-issues/issues


## Building

Ren-C does not require GNU Make, CMake, or any other make tools.  It only
needs a copy of a Ren-C executable to build itself.  To do a full build, it
can just invoke a C compiler using [the CALL facility][14], with the
appropriate command lines.

[14]: http://www.rebol.com/docs/shell.html

Several platforms are supported, including Linux, Windows, OS X, Android, and
support for JavaScript via Webassembly.  Configurations for each platform are
in the %configs/ directory.  When the build process is run, you should be in
the directory where you want the build products to go (e.g. %build/).  Here
is a sample of how to compile under Linux:

    # You need a Ren-C-based Rebol to use in the make process
    # See %tools/bootstrap-shim.r regarding what versions are usable
    # Currently there are usable executables in %/prebuilt ...
    # ...but that's not a permanent solution!
    #
    ~/ren-c$ export R3_MAKE="$(pwd)/prebuilt/r3-linux-x64-8994d23"

    ~/ren-c$ cd build

    ~/ren-c/build/$ "$R3_MAKE" ../make.r \
        config: ../configs/default-config.r \
        debug: asserts \
        optimize: 2

For a list of options, run %make.r with `--help`.

Though it does not *require* other make tools, it is optional to generate a
`makefile` target, or a Visual Studio solution.  %make.r takes parameters like
`target: makefile` or `target: vs2017`.  But there are several complicating
factors related to incremental builds, due to the fact that there's a large
amount of C code and header files generated from tables and scans of the
source code.  If you're not familiar with the source and what kinds of changes
require rebuilding which parts, you should probably do full builds.

As a design goal, compiling Ren-C requires [very little beyond ANSI C89][15].
Attempts to rein in compiler dependencies have been a large amount of work,
and it still supports a [number of older platforms][16].  However, if it is
compiled with a C++ compiler then there is significantly more static analysis
at build time, to catch errors.

*(Note: The build process is much more complicated than it should be, but
other priorities mean it isn't getting the attention it deserves.  It would be
strongly desirable if community member(s) could get involved to help
streamline and document it!  Since it's now *all* written in Rebol, that
should be more possible--and maybe even a little "fun" (?))*

[15]: https://github.com/metaeducation/ren-c/wiki/On-Building-Ren-C-With-Cpp-Compilers 
[16]: https://github.com/metaeducation/ren-c/blob/master/make/tools/systems.r

[100]: https://raw.githubusercontent.com/metaeducation/ren-c/master/docs/ren-c-logo.png
[101]: https://travis-ci.com/metaeducation/ren-c.svg?branch=master
