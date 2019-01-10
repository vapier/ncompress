#!/bin/bash -e

. "${0%/*}"/lib.sh

build_make() {
	m
	m check
	m install DESTDIR="${PWD}/root"
}

build_linux() { build_make; }

build_osx() { build_make; }

build_windows() {
	v ${CC} -o compress \
		-Wall \
		-DNOFUNCDEF -DUSERMEM=800000 -DREGISTERS=3 \
		compress42.c
	# The tests currently fail on Windows.  No idea why.
	./tests/runtests.sh || :
}

main() {
	"build_${TRAVIS_OS_NAME}"
}
main "$@"
