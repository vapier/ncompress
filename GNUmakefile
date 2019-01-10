# this assumes a recent system -- you're using GNU make right ?

compress cleanup install: Makefile
	$(MAKE) -f Makefile $@

clean: cleanup
distclean: cleanup
	rm -f Makefile

Makefile: Makefile.def GNUmakefile
	sed \
		-e 's:options= :options= $$(CFLAGS) -DUTIME_H -DLSTAT $$(LDFLAGS) :' \
		Makefile.def > Makefile

check:
	./tests/runtests.sh

PN = ncompress
PV = $(shell awk '{print $$NF; exit}' Changes)
P = $(PN)-$(PV)
dist:
	git archive --prefix=$(P)/ HEAD | gzip -9 > $(P).tar.gz

.PHONY: check clean cleanup compress dist distclean install
