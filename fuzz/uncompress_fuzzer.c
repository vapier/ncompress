/* libFuzzer harness for ncompress' .Z decompressor.
 *
 * Build:
 *   clang -O1 -g -fsanitize=fuzzer,address,undefined \
 *         -D_GNU_SOURCE -Wno-implicit-function-declaration \
 *         fuzz/uncompress_fuzzer.c -o uncompress_fuzzer
 *
 * Run:
 *   ./uncompress_fuzzer fuzz/corpus
 *
 * Run with -fork=1.  decompress() is not resumable within a process: it
 * calls exit() through abort_compress() on some malformed streams, and the
 * decoder tables (htab/codetab) are not cleared between calls, so a
 * persistent in-process loop would either terminate the fuzzer or carry
 * state from one input into the next.
 *
 * decompress() has internal linkage, so this pulls in the implementation
 * directly.  compress.c's main() is renamed so that libFuzzer's main() can
 * be linked alongside it.
 */
#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>

#define main ncompress_main
#include "../compress.c"
#undef main

int
LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	char tmpl[] = "/tmp/ncompress-fuzz-XXXXXX";
	int fdin, fdout;
	size_t off;
	ssize_t w;

	if (size == 0)
		return (0);

	/* decompress() works on file descriptors; hand it a temporary file. */
	if ((fdin = mkstemp(tmpl)) < 0)
		return (0);
	unlink(tmpl);

	for (off = 0; off < size; off += (size_t)w) {
		if ((w = write(fdin, data + off, size - off)) <= 0) {
			close(fdin);
			return (0);
		}
	}
	if (lseek(fdin, 0, SEEK_SET) == (off_t)-1) {
		close(fdin);
		return (0);
	}
	if ((fdout = open("/dev/null", O_WRONLY)) < 0) {
		close(fdin);
		return (0);
	}

	/* decompress() reports errors using ifname.  main() always sets it
	 * ("" when reading stdin), so do the same here. */
	ifname = "";
	remove_ofname = 0;

	decompress(fdin, fdout);

	close(fdin);
	close(fdout);

	return (0);
}
