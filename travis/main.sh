#!/bin/bash -e

. "${0%/*}"/lib.sh

build_make() {
	m
	m check
	m install DESTDIR="${PWD}/root"
}

build_linux() { build_make; }

main() {
	"build_${TRAVIS_OS_NAME}"
}
main "$@"
