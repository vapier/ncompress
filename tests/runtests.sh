#!/bin/sh
# Ugly, but works ?

set -e

fullpath() {
	# Because portability is a pita.
	realpath "$0" 2>/dev/null && return 0
	readlink -f "$0" 2>/dev/null && return 0
	python -c 'import os, sys; print(os.path.realpath(sys.argv[1]))' "$0"
}

SCRIPT="$(fullpath "$@")"
TESTDIR="$(dirname "${SCRIPT}")"
TOP_SRCDIR="$(dirname "${TESTDIR}")"

if [ -z "${COMPRESS}" ]; then
	COMPRESS="${TOP_SRCDIR}/compress"
fi
compress() { "${COMPRESS}" -v "$@"; }
uncompress() { "${COMPRESS}" -v -d "$@"; }

echo "using compress: ${COMPRESS}"

echo "Setting up test env"

TMPDIR=""
trap 'rm -rf "${TMPDIR}"' EXIT
TMPDIR="$(mktemp -d)"

cd "${TMPDIR}"

cp "${TOP_SRCDIR}/README.md" input
cp input i
mkdir subdir
cp input i subdir/
mkdir emptydir
touch emptyfile

set -x

: "### Check basic CLI exit status"
compress -V
compress -h >/dev/null

out=$(compress -h)
[ -n "${out}" ]
(
# The shell trace output can write to stderr which we check here.
set +x
out=$(compress -h 2>&1 >/dev/null)
[ -z "${out}" ]
)

if compress -X 2>/dev/null; then false; fi

: "### Check unknown flags spit to stderr only"
out=$(compress -X 2>/dev/null) || :
[ -z "${out}" ]

out=$(compress -X 2>&1) || :
[ -n "${out}" ]

: "### Check -- handling"
cp input ./-X
compress -- -X
[ -e ./-X.Z ]
uncompress -- -X
[ -e ./-X ]
rm ./-X

: "### Check compression"
compress input
[ ! -e input ]
[ -e input.Z ]

: "### Check compressing .Z file fails"
if compress input.Z; then false; fi

: "### Check decompression w/explicit .Z"
uncompress input.Z
[ -e input ]
[ ! -e input.Z ]

: "### Check decompression w/implicit .Z"
compress input
uncompress input.Z
[ -e input ]
[ ! -e input.Z ]

: "### Check uncompressing non-.Z file fails"
if uncompress input; then false; fi

: "### Check empty directory compression"
if compress emptydir; then false; fi
if compress -r emptydir; then false; fi

: "### Check directory compression"
compress -r subdir

: "### Check empty directory decompression"
if uncompress -r emptydir; then false; fi

: "### Check directory decompression"
uncompress -r subdir

: "### Check uncompressed directory decompression"
if uncompress -r subdir; then false; fi

: "### Check various error edge cases"
if compress missing; then false; fi
if uncompress missing; then false; fi

: "### Check forced compression"
if compress emptyfile; then false; fi
if compress <emptyfile >/dev/null; then false; fi
compress -f emptyfile
uncompress emptyfile

: "### Check stdin/stdout handling"
compress -c input >input.Z
[ -e input ]
compress <input >input.Z
uncompress -c input.Z >input.new
cmp input input.new
uncompress <input.Z >input.new
cmp input input.new

: "### Check existing files"
if compress input </dev/null; then false; fi
compress -f input
mv input.new input
if uncompress input.Z </dev/null; then false; fi
uncompress -f input.Z

: "### Check empty file names"
if compress ""; then false; fi
if uncompress ""; then false; fi

: "### Check .Z dir edge case"
mkdir .Z
if uncompress ""; then false; fi
rmdir .Z

: "### Check short filenames"
compress i
uncompress i
compress i
uncompress i.Z

: "### All passed!"
