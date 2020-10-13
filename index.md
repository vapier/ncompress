# About

Compress is a fast, simple [LZW] file compressor.
Compress does not have the highest compression rate, but it is one of the
fastest programs to compress data.
Compress is the defacto standard in the UNIX community for compressing files.

The ncompress code is, and will continue to be, released into the
[public domain] as the original authors intended.

Also note that all existing patents on the LZW algorithm have
[expired world-wide](https://en.wikipedia.org/wiki/LZW#Patents).

# Status

The main branch is tracking the 5.x release series.

We will always support the output of older compress versions (including the
compress-2.0 format), and we will never produce files that compress-3.0 is
unable to process.

The ncompress-4.2.4 branch was used to track a series of bugfix releases.
It is no longer used.  See the history section below for more details.

# Downloads

The latest downloads can be found here:
<https://github.com/vapier/ncompress/releases>

# Portability Requirements

Our goal is to remain portable and to rely on the compiler for low level
optimization.  In other words, improved algorithms will be considered, but not
compiler-specific tricks or (ab)use of keywords like `inline` or `register` or
inline assembly.

We require [C99] compilers which means 1999-era software should work OK.
We aim for [POSIX/IEEE Std 1003.1-2001][POSIX-2001] compliance.
That is, we will try to avoid functionality that is not defined there.

If those requirements are too new for your system, older releases of compress
are always available for download.

[C99]: https://en.wikipedia.org/wiki/C99
[POSIX-2001]: https://pubs.opengroup.org/onlinepubs/009695399/

# Contact

Please use the [issue tracker][tracker] to contact us for bugs, questions, etc...

Current primary maintainer:
<a href="mailto:vapier@gmail.com">Mike Frysinger</a>

# Specifications

You can find plenty of info on the LZW algorithm (just use
[Google](https://www.google.com/search?q=lzw)), but for fun, here's some helpful
links.

* [LZW Wikipedia Entry][LZW]
* [LZW Data Compression](https://www.dogma.net/markn/articles/lzw/lzw.htm) by Mark Nelson
* [LZW Encoding Discussion and Implementation](http://michael.dipperstein.com/lzw) by Michael Dipperstein
* [LZW NIST Reference Page](https://www.nist.gov/dads/HTML/lempelZivWelch.html)

# History

The compress/ncompress project has a long history, even by computing standards.
The interwoven patent history hindered its development significantly.

NB: The history here has been reconstructed by people not directly involved at
the time.  Any errors/omissions are not intentional, so please feel free to
[send updates/fixes][tracker].

NB: A lot of effort has been made to track down public releases.  However, as
can be seen below, and from the inline [RCS] history in the compress file
itself, there are many commits that don't have public information.  The actual
[RCS] file appears to have never been published.  If people have access to
missing pieces, please feel free to [pass them along][tracker].

NB: Due to the nature of early software sharing & development (i.e. long before
[VCS] was common practice, or code sharing sites like [GitHub] existed, and many
non-standardized architectures & operating systems were in use), many forks of
the compress code were developed & posted, but eventually died out.  The history
below will only focus on the canonical C implementation and its lineage that has
been widely integrated into other projects.

* 10 Aug 1981: [U.S. Patent 4,464,650] filed for [LZ78].
* 20 Jun 1983: [U.S. Patent 4,558,302] filed for [LZW].
* June 1984: [A Technique for High Performance Data Compression] published by
  Terry A. Welch (the "W" in "[LZW]") in [IEEE Computer Vol 17, No 6].  No
  mention of the pending patent is made in the article.
* 05 Jul 1984: Spencer W. Thomas makes [first release of compress (v1.4)][1.4]
  on [net.sources].  It implements the [LZW] algorithm described in the IEEE
  paper, unaware of the pending patent coverage.
* 05 Aug 1984: Joseph M. Orost [releases a portable update (v1.6)][1.6] on
  [net.sources].
* 07 Aug 1984: [U.S. Patent 4,464,650] granted for [LZ78].  It isn't directly
  related to [LZW], so doesn't impact work.
* 30 Aug 1984: Joseph M. Orost [releases an update (v2.0)][2.0] on
  [net.sources].  This version gets integrated into other projects (like early
  news software).
* 03 Jan 1985: Joseph M. Orost [releases v3.0][3.0] on [mod.sources].  It breaks
  backwards compatibility with earlier versions with compressed outputs.
* 01 Aug 1985: Joseph M. Orost [releases v4.0][4.0] on [mod.sources].  This ends
  up as the last official release and the foundation of all future forks.  Many
  projects integrate this version directly.
* Sep 1985: [compress v4.0 is merged][BSD-compress-merge] into [BSD]
  (after the [4.2BSD] release, but before [4.3BSD] is released).
* 10 Dec 1985: [U.S. Patent 4,558,302] granted for [LZW].  People notice and
  work is effectively halted.
* Jun 1986: [4.3BSD] is released.  All future BSD releases/projects derive from
  this lineage which means they're all based on compress v4.0.  This includes
  modern day [FreeBSD] & [NetBSD] (which forked from [386BSD] which forked from
  [4.3BSD]), and all their derivatives like [OpenBSD] & [Darwin].
* 07 May 1987: Chip Salzenberg releases unofficial v4.1.  Unclear where it was
  announced, but some people seem to have picked it up.
* 04 Jun 1987: Chip Salzenberg releases unofficial v4.2 somewhere.
* 18 Jan 1990: Donald Gloistein [releases unofficial v4.3][4.3-dead].  It was
  supposed to have been published on [comp.sources.misc], but instead is
  distributed via random sites. Many changes are made here, but this fork
  eventually dies out.  It has no relation to Chip's work, just happens to have
  a larger version number.  A copy of this can be found [here][walnut-iso] (look
  for "compress.tar" in the image).
* 25 Jun 1991: Dave Mack [releases unofficial v4.1][4.1] on [comp.sources.misc].
  People are [quickly][confuse1] [confused][confuse2] by two different releases
  of "compress 4.1", both based on v4.0, and without including the other's
  changes.  Nothing is done about the confusion.
* 29 Jun 1991: Dave Mack [releases unofficial v4.1 patch1][4.1p1] on
  [comp.sources.misc].
* 06 Apr 1992: Peter Jannesen submits ncompress v4.2.1 for review.  He bases it
  on Dave Mack's v4.1p1, but renames the project to "(n)compress" to avoid
  further confusion with pre-existing unofficial compress v4.2 releases.
  This version is not made public.
* 23 Jun 1992: Peter Jannesen submits ncompress v4.2.2 for review.
  This version is not made public.
* 28 Aug 1992: Peter Jannesen [releases v4.2.3][4.2.3] on
  [comp.sources.reviewed].
* 05 Oct 1992: Peter Jannesen [releases v4.2.4][4.2.4] (as a patch to v4.2.3)
  on [comp.sources.reviewed].
* 31 Oct 1992: [GNU gzip] is released using code from ncompress v4.2.4.
* 10 Aug 2001: [U.S. Patent 4,464,650] for [LZ78] expires in the US.
* 20 Jun 2003: [U.S. Patent 4,558,302] for [LZW] expires in the US.
* Jun/Jul 2004: The counterpart patents for [LZW] expire in other countries.
  Finally the code is patent-free world-wide.
* 28 Sep 2006: Mike Frysinger starts ncompress maintenance project on
  [sourceforge.net](https://sf.net/p/ncompress/).  It is based on the v4.2.4
  release and aims to unify patches distros have been carrying for years.
  A public [VCS] repository using [Subversion] is used for development.
* 29 Sep 2006: Mike Frysinger releases v4.2.4.1.
* 07 Sep 2007: Mike Frysinger releases v4.2.4.2.
* 29 Jan 2010: [VCS] is migrated to [Git].
* 29 Jan 2010: Mike Frysinger releases v4.2.4.3.
* 09 Sep 2010: Mike Frysinger releases v4.2.4.4.
* 24 Jun 2015: Mike Frysinger moves
  [hosting to GitHub](https://github.com/vapier/ncompress/).
* 04 Jan 2019: Mike Frysinger releases v4.2.4.5.
* 30 Dec 2019: Mike Frysinger releases v4.2.4.6.
* 05 Jan 2020: Mike Frysinger switches to v5.0 series.

[1.4]: https://groups.google.com/d/topic/net.sources/fonve4JCDpQ/discussion
[1.6]: https://groups.google.com/d/topic/net.sources/mXX0Ic7sPgg/discussion
[2.0]: https://groups.google.com/d/topic/net.sources/O-fDfsSluL0/discussion
[3.0]: https://groups.google.com/d/topic/mod.sources/fCpRnZZ2GBw/discussion
[4.0]: https://groups.google.com/d/topic/mod.sources/rMlKWGkEfFs/discussion
[4.1]: https://groups.google.com/d/topic/comp.sources.misc/gCgF0Zo1vp0/discussion
[4.1p1]: https://groups.google.com/d/topic/comp.sources.misc/dov5AFo1rDI/discussion
[4.2.3]: https://groups.google.com/d/topic/comp.sources.reviewed/9Cn4WTNOeoo/discussion
[4.2.4]: https://groups.google.com/d/topic/comp.sources.reviewed/39UrQ3iywAA/discussion
[4.3-dead]: https://groups.google.com/d/msg/comp.sources.bugs/nsa_VVpnkJs/96p7OirdHg4J
[confuse1]: https://groups.google.com/d/msg/comp.sources.bugs/Qh9Fol0By_k/mY2GDjjiulQJ
[confuse2]: https://groups.google.com/d/topic/comp.sources.bugs/nsa_VVpnkJs/discussion
[walnut-iso]: https://archive.org/details/CDROM_March92

[A Technique for High Performance Data Compression]: https://www.cs.duke.edu/courses/spring03/cps296.5/papers/welch_1984_technique_for.pdf
[386BSD]: https://en.wikipedia.org/wiki/386BSD
[4.2BSD]: https://en.wikipedia.org/wiki/History_of_the_Berkeley_Software_Distribution#4.2BSD
[4.3BSD]: https://en.wikipedia.org/wiki/History_of_the_Berkeley_Software_Distribution#4.3BSD
[BSD]: https://en.wikipedia.org/wiki/BSD
[BSD-compress-merge]: https://minnie.tuhs.org/cgi-bin/utree.pl?file=4.3BSD/usr/src/ucb/compress/compress.c
[Darwin]: https://en.wikipedia.org/wiki/Darwin_(operating_system)
[FreeBSD]: https://en.wikipedia.org/wiki/FreeBSD
[NetBSD]: https://en.wikipedia.org/wiki/NetBSD
[OpenBSD]: https://en.wikipedia.org/wiki/OpenBSD
[GNU gzip]: https://www.gnu.org/software/gzip/
[IEEE Computer Vol 17, No 6]: https://ieeexplore.ieee.org/document/1659158
[LZ78]: https://en.wikipedia.org/wiki/LZ78
[LZW]: https://en.wikipedia.org/wiki/LZW
[public domain]: https://en.wikipedia.org/wiki/Public_domain
[U.S. Patent 4,464,650]: https://patents.google.com/patent/US4464650
[U.S. Patent 4,558,302]: https://patents.google.com/patent/US4558302
[comp.sources.misc]: https://groups.google.com/forum/#!forum/comp.sources.misc
[comp.sources.reviewed]: https://groups.google.com/forum/#!forum/comp.sources.reviewed
[mod.sources]: https://groups.google.com/forum/#!forum/mod.sources
[net.sources]: https://groups.google.com/forum/#!forum/net.sources
[tracker]: https://github.com/vapier/ncompress/issues
[Git]: https://en.wikipedia.org/wiki/Git
[RCS]: https://en.wikipedia.org/wiki/Revision_Control_System
[Subversion]: https://en.wikipedia.org/wiki/Apache_Subversion
[VCS]: https://en.wikipedia.org/wiki/Version_control_system
[GitHub]: https://github.com/
