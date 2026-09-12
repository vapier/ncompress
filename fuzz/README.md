# Fuzzing ncompress

`uncompress_fuzzer.c` is a libFuzzer harness for the `.Z` decompressor.
It feeds each input through `decompress()` and needs no external tools,
account or network access.

## Build

```
clang -O1 -g -fsanitize=fuzzer,address,undefined \
      -D_GNU_SOURCE -Wno-implicit-function-declaration \
      fuzz/uncompress_fuzzer.c -o uncompress_fuzzer
```

`-Wno-implicit-function-declaration` is needed because `utime()` is used
without including `<utime.h>`; gcc accepts this silently, clang 16 and later
reject it.

## Run

```
./uncompress_fuzzer fuzz/corpus
```

Run with `-fork=1` for anything longer than a quick check, and always in CI:

```
./uncompress_fuzzer fuzz/corpus -fork=1 -max_total_time=300
```

`decompress()` ends the process through `abort_compress()` on some malformed
streams, and the decoder tables are not reset between calls, so a persistent
single-process loop would either stop the fuzzer or carry state from one
input into the next.

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
