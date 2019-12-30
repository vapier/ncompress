# this assumes a recent system -- you're using GNU make right ?

# POSIX make doesn't support default values, so we export from here.
CFLAGS ?= -O2 -g
CFLAGS += -Wall
export CFLAGS

compress cleanup install install_core install_extra: Makefile
	$(MAKE) -f Makefile $@

clean: cleanup
distclean: cleanup
	rm -f Makefile

Makefile: Makefile.def GNUmakefile
	sed \
		-e 's:options= :options= -DUTIME_H -DLSTAT :' \
		Makefile.def > Makefile

check:
	./tests/runtests.sh

PN = ncompress
PV = $(shell awk '{print $$NF; exit}' Changes)
P = $(PN)-$(PV)
dist:
	git archive --prefix=$(P)/ HEAD | gzip -9 > $(P).tar.gz

.PHONY: check clean cleanup compress dist distclean install
