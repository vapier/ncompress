#
# Makefile for compress version 4.1
#
CC=cc
#CC=gcc -traditional
#
# set your compile flags.
# set -DVOIDSIG if your signal() function returns a pointer to
# a function returning void.
# set BSD4 if you have a generally BSDish system - SunOS, Ultrix, etc.
# If you're saddled with a system that only allows 14-character
# filenames, set -DSHORTNAMES
# include -DDIRENT if your system wants you to #include <dirent.h>
# instead of <sys/dir.h>
# The README file describes other flags you may need.
#
# CFLAGS for SunOS4.1
CFLAGS=-O -DVOIDSIG -DBSD4 -DDIRENT
# CFLAGS for System V Rel 3.x
#CFLAGS=-O -DSHORTNAMES -DDIRENT
#
# BIN is where the executables (compress, uncompress, zcat, zcmp, zdiff
# and zmore) will be installed.
#
BIN=/usr/local/bin
#
# MAN says where to install the man page
#
MAN=/usr/man/manl
#
# MANSUF is the suffix the installed manual page should have
#
MANSUF=l
#
# LN is how to make links (hard or symbolic) on your system
#
LN=ln -s
#
# LIBS contains any additional libraries you may need to link.
# In particular, you may need -lndir or equiv. to get the
# public domain directory access routines if they aren't in your libc.
#
LIBS=
#
# END OF CONFIGURATION OPTIONS
#

SHARSET1=README compress.c
SHARSET2=Changes Acks Makefile usermem  compress.1 zcmp zcmp.1 \
	zdiff zmore zmore.1 patchlevel.h

all: compress

compress: USERMEM compress.c
	$(CC) $(CFLAGS) -DUSERMEM=`cat USERMEM` -o compress compress.c $(LIBS)

# USERMEM may have to be set by hand.  It should contain the amount of
# available user memory in bytes.  See the README file for more info.
USERMEM:
	sh usermem > USERMEM

install: compress compress.1
	-mv $(BIN)/compress $(BIN)/compress.old
	cp compress $(BIN)
	rm -f $(BIN)/uncompress $(BIN)/zcat
	$(LN) $(BIN)/compress $(BIN)/uncompress
	$(LN) $(BIN)/compress $(BIN)/zcat
	cp compress.1 $(MAN)/compress.$(MANSUF)
	rm -f $(MAN)/uncompress.$(MANSUF) $(MAN)/zcat.$(MANSUF)
	$(LN) $(MAN)/compress.$(MANSUF) $(MAN)/uncompress.$(MANSUF)
	$(LN) $(MAN)/compress.$(MANSUF) $(MAN)/zcat.$(MANSUF)
	cp zmore zcmp zdiff $(BIN)
	cp zmore.1 $(MAN)/zmore.$(MANSUF)
	cp zcmp.1 $(MAN)/zcmp.$(MANSUF)
	rm -f $(MAN)/zdiff.$(MANSUF)
	$(LN) $(MAN)/zcmp.$(MANSUF) $(MAN)/zdiff.$(MANSUF)

clean:
	rm -f compress uncompress zcat core

shar:
	shar -o compress41.shar.1 -n01 -e02 $(SHARSET1)
	shar -o compress41.shar.2 -n02 -e02 $(SHARSET2)





