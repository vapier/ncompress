# this assumes a recent system -- you're using GNU make right ?

compress %: Makefile
	$(MAKE) -f Makefile $@

Makefile: Makefile.def
	sed \
		-e 's:options= :options= $$(CFLAGS) -DNOFUNCDEF -DUTIME_H $$(LDFLAGS) :' \
		Makefile.def > Makefile

PN = ncompress
PV = $(shell awk '{print $$NF; exit}' Changes)
P = $(PN)-$(PV)
dist:
	git archive --prefix=$(P)/ HEAD | gzip -9 > $(P).tar.gz

.PHONY: compress clean dist distclean
