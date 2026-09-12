# Fuzzing ncompress

`uncompress_fuzzer.c` is a libFuzzer harness for the `.Z` decompressor.
It feeds each input through `decompress()` and needs no external tools,
account or network access.

## Build

```
clang -O1 -g -fsanitize=fuzzer,address,undefined -DUTIME_H -DLSTAT \
      fuzz/uncompress_fuzzer.c -o uncompress_fuzzer
```

Build with the same defines the makefile passes (`UTIME_H`, `LSTAT`).  Without
`UTIME_H` the file falls back to declaring `struct utimbuf` itself and
`utime()` has no prototype, which clang 16 and later reject outright.

## Run

```
./uncompress_fuzzer fuzz/corpus
```

Run it for as long as you like:

```
./uncompress_fuzzer fuzz/corpus -max_total_time=300
```

`decompress()` ends the process through `abort_compress()` on some malformed
streams, and its decoder tables are global.  The harness handles both so that
one process can fuzz many inputs: it redirects `exit()` to a `longjmp` (by
rewriting the calls inside `compress.c` before including it), and clears the
tables before each input so every input runs against the state a fresh
process would have.  Sanitizer failures are not routed through `exit()`, so
they still abort and are reported normally.

## Corpus

`corpus/` holds small `.Z` streams produced by `compress` itself, covering
different input sizes and `-b` settings.  Add cases with:

```
./compress -c -b N < file > corpus/name.Z
```

## Findings

No crashes so far under ASan + UBSan.  Two historical hazards were checked
by hand and are not reachable:

* `de_stack` cannot underflow.  `htab` is sized so that the downward-growing
  decompression stack has far more room than `maxmaxcode` (65536) entries,
  and `tab_prefixof(free_ent)` is always less than `free_ent` within a block,
  so prefix chains strictly descend and terminate.
* Output files are already created with `O_CREAT|O_EXCL` and mode 0600, so
  there is no symlink or clobber weakness.
