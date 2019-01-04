#!/bin/bash -e

. "${0%/*}"/lib.sh

main() {
	m
	m check
	m install DESTDIR="${PWD}/root"
}
main "$@"
