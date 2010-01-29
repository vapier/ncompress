CC       ?= gcc
CFLAGS   ?= -O2 -g
CFLAGS   += -Wall

DESTDIR =
PREFIX  = /usr
BINDIR  = $(PREFIX)/bin
MANDIR  = $(PREFIX)/share/man
MAN1DIR = $(MANDIR)/man1

# compiler options:
# options is a collection of:
#
#	-DIBUFSIZ=<size>			Input buffer size (default BUFSIZ).
#	-DOBUFSIZ=<size>			Output buffer size (default BUFSIZ)
#	-DBYTEORDER=<order>			Byte order (default: unknown).
#	-DNOALLIGN=1				Data word allignment (default: yes).
#
options = 

CPPFLAGS += $(options)

all: compress

test:
	rm -f zmore.1.Z
	svn revert zmore.1
	./compress zmore.1
	./uncompress zmore.1.Z
	md5sum zmore.1
	md5sum -c zmore.1.md5

patchlevel.h: version
	echo "#define VERSION \"`cat version`\"" > patchlevel.h

compress.o: patchlevel.h

INSTALL_DIR = install -d -m 755
INSTALL_EXE = install -m 755
INSTALL_MAN = install -m 644
LN_S = ln -sf

install: compress
	$(INSTALL_DIR) $(DESTDIR)$(BINDIR)
	$(INSTALL_EXE) compress $(DESTDIR)$(BINDIR)
	$(LN_S) compress $(DESTDIR)$(BINDIR)/uncompress
	$(INSTALL_DIR) $(DESTDIR)$(MAN1DIR)
	$(INSTALL_MAN) compress.1 $(DESTDIR)$(MAN1DIR)

install_extra: install
	$(LN_S) compress $(DESTDIR)$(BINDIR)/zcat
	$(INSTALL_EXE) zcmp zdiff zmore $(DESTDIR)$(BINDIR)
	$(INSTALL_MAN) zcmp.1 zmore.1 $(DESTDIR)$(MAN1DIR)

distclean clean:
	rm -f *.o compress *.log patchlevel.h

.PHONY: all clean distclean install install_extra
