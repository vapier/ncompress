# About

This is version 4.2 of (N)compress (an improved version of compress 4.1).

Compress is a fast, simple LZW file compressor.  Compress does not have
the highest compression rate, but it is one of the fastest programs to
compress data.  Compress is the defacto standard in the UNIX community
for compressing files.

(N)compress 4.2 has a special, fast compression hash algorithm.  This
algorithm uses more memory than the old hash table. If you don't want
the faster hash table algorithm set 'Memory free for compress' below
800000.

zcmp, zdiff, zmore were copied from version 4.1 without any changes.

The output of (N)compress 4.2 is fully compatible with that of compress 3.0.
In other words, the output of (N)compress 4.2 may be fed into uncompress 3.0 or
the output of compress 3.0 may be fed into uncompress 4.2.

The output of (N)compress 4.2 is not compatible with that of
compress 2.0.  However, compress 4.2 still accepts the output of
compress 2.0.  To generate output that is compatible with compress
2.0, use the undocumented "-C" flag.

# Building

For recent systems with GNU make, you can simply run `make` as the default
'GNUMakefile' will get picked up.

'build' is a menu driven shell script for compiling, testing and
installing (N)compress. So to build and install (N)compress all you have to
do is run build.  Build will first test your system for default
settings. The current compile settings are stored in a special file
called compress.def.

For user with problems with build there is a default makefile included
called 'Makefile.def'. Also build is capable of generating a Makefile with
all options (option genmake).

# Support

[![Build Status](https://travis-ci.org/vapier/ncompress.svg?branch=ncompress-4.2.4)](https://travis-ci.org/vapier/ncompress)

Send comments, complaints and especially patches relating to
    https://github.com/vapier/ncompress/issues

# Licensing

The ncompress code is released into the public domain.  See the
[UNLICENSE](UNLICENSE) file for more details.

# Patents

All existing patents on the LZW algorithm have
[expired world-wide](http://en.wikipedia.org/wiki/LZW#Patent_issues).
So LZW is now patent free.

# Remarks

- Build is a bourne shell script. On some system it is necessary to type
  'sh build'.

- The build script usages tput for nice screen handling of the script.
  If your system has no tput no problems.

- For configuration testing build uses a lot of small C programs. During
  those test stderr is redirected to /dev/null.
  During the compilation of compress output is NOT redirected.

- The /bin/sh under Ultrix can't handle ${var:-str} so use ksh for the
  build script.

- The output if (N)compress 4.2 is not exactly the same as compress 4.0
  because of different table reset point. The output of (N)compress 4.2
  is 100% compatible with compress 4.0

- Some systems has performance problems with reads bigger than BUFSIZ
  (The read a head function is not working as expected). For those
  system use the default BSIZE input buffer size.

- (N)compress can by slower on small files (<10Kb) because of a great
  table reset overhead. Use cpio or tar to make 1 bigger file if
  possible, it is faster and also gives a better compression ratio most
  of the time.

- (N)compress is slower in vax systems because of removing some
  undocumented inline assembler.

- files compressed on a large machine with more bits than allowed by
  a version of compress on a smaller machine cannot be decompressed!  Use the
  "-b12" flag to generate a file on a large machine that can be uncompressed
  on a 16-bit machine.

- compatibility with compress 3.0 has not been tested in the 4.2 release of
  (N)compress.

- There has been 1 problem report in relation to GCC 2.0 on a sparc
  workstation. GCC 2.0 seems to generate a bad compress. Use the
  standard c compiler 'cc'.
