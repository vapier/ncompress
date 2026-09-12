/* libFuzzer harness for ncompress' .Z decompressor.
 *
 * Build:
 *   clang -O1 -g -fsanitize=fuzzer,address,undefined -DUTIME_H -DLSTAT \
 *         fuzz/uncompress_fuzzer.c -o uncompress_fuzzer
 *
 * Run:
 *   ./uncompress_fuzzer fuzz/corpus
 *
 * decompress() is not resumable: abort_compress() ends the process with
 * exit() on some malformed streams, and the decoder tables are global.
 * Both are handled here so that a single process can fuzz many inputs:
 *
 *   * exit() is redirected to a longjmp before compress.c is included, so a
 *     malformed stream unwinds back to the harness instead of killing the
 *     fuzzer.  (Forking per input would also work, but it loses the coverage
 *     counters libFuzzer needs, and -fork=1 reports the target's own exit()
 *     as a crash.)
 *   * The decoder tables are cleared before each input, so every input runs
 *     against the state a fresh process would have.  Without this, a leftover
 *     entry from a previous input could look like a crash in the current one.
 *
 * Sanitizer failures are not routed through exit(), so they still abort the
 * process and are reported normally.
 *
 * decompress() has internal linkage, so this pulls in the implementation
 * directly.  compress.c's main() is renamed so that libFuzzer's main() can
 * be linked alongside it.
 */
#include <fcntl.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static jmp_buf exit_jmp;

static void
ncompress_exit(int status)
{

	(void)status;
	longjmp(exit_jmp, 1);
}

/* <stdlib.h> is already included above, so this rewrites calls made by
 * compress.c only, not the declaration in the header. */
#define exit(status) ncompress_exit(status)
#define main ncompress_main
#include "../compress.c"
#undef main
#undef exit

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

	/* Start from the state a fresh process would have. */
	memset(htab, 0, sizeof(htab));
	memset(codetab, 0, sizeof(codetab));
	bytes_in = bytes_out = 0;
	exit_code = -1;

	/* decompress() reports errors using ifname; main() always sets it
	 * ("" when reading stdin), so do the same here. */
	ifname = "";
	remove_ofname = 0;

	if (setjmp(exit_jmp) == 0)
		decompress(fdin, fdout);
	/* else: ncompress called exit() on this input; treat it as handled. */

	close(fdin);
	close(fdout);

	return (0);
}
