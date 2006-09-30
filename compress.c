/* File:          (N)compress.c - File compression ala IEEE Computer, Mar 1992.
 * Authors:       see the Acknowleds file
 * License:       public domain
 * Contact:       ncompress-devel@lists.sourceforge.net
 */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <ctype.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <utime.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <dirent.h>
#include <getopt.h>

#include "patchlevel.h"

#ifndef SIG_TYPE
# define SIG_TYPE void (*)()
#endif

/* Default buffer sizes */
#ifndef	IBUFSIZ
# define IBUFSIZ BUFSIZ
#endif
#ifndef	OBUFSIZ
# define OBUFSIZ BUFSIZ
#endif

/* MAXPATHLEN - maximum length of a pathname we allow 	*/
#if !defined(_POSIX_PATH_MAX) && !defined(PATH_MAX)
# define MAXPATHLEN 8192
#elif _POSIX_PATH_MAX > PATH_MAX
# define MAXPATHLEN _POSIX_PATH_MAX
#else
# define MAXPATHLEN PATH_MAX
#endif

/* Defines for first few bytes of header */
#define	MAGIC_1		(char_type)'\037'/* First byte of compressed file				*/
#define	MAGIC_2		(char_type)'\235'/* Second byte of compressed file				*/
#define BIT_MASK	0x1f			/* Mask for 'number of compresssion bits'		*/
									/* Masks 0x20 and 0x40 are free.  				*/
									/* I think 0x20 should mean that there is		*/
									/* a fourth header byte (for expansion).    	*/
#define BLOCK_MODE	0x80			/* Block compresssion if table is full and		*/
									/* compression rate is dropping flush tables	*/

/* the next two codes should not be changed lightly, as they must not	*/
/* lie within the contiguous general code space.						*/
#define FIRST	257					/* first free entry 							*/
#define	CLEAR	256					/* table clear output code 						*/

#define INIT_BITS 9			/* initial number of bits/code */

#ifndef	BYTEORDER
# if defined(BYTE_ORDER)
#  if BYTE_ORDER == LITTLE_ENDIAN
#   define BYTEORDER 4321
#  else
#   define BYTEORDER 1234
#  endif
# else
#  warning Unable to figure out byteorder, defaulting to slow byte swapping
#  define BYTEORDER 0000
# endif
#endif

#ifndef	NOALLIGN
# define NOALLIGN 0
#endif

/* System has no binary mode */
#ifndef	O_BINARY
# define O_BINARY 0
#endif

/* Modern machines should work fine with FAST */
#define FAST

#ifdef FAST
#	define	HBITS		17			/* 50% occupancy */
#	define	HSIZE	   (1<<HBITS)
#	define	HMASK	   (HSIZE-1)
#	define	HPRIME		 9941
#	define	BITS		   16
#else
#	if BITS == 16
#		define HSIZE	69001		/* 95% occupancy */
#	endif
#	if BITS == 15
#		define HSIZE	35023		/* 94% occupancy */
#	endif
#	if BITS == 14
#		define HSIZE	18013		/* 91% occupancy */
#	endif
#	if BITS == 13
#		define HSIZE	9001		/* 91% occupancy */
#	endif
#	if BITS <= 12
#		define HSIZE	5003		/* 80% occupancy */
#	endif
#endif

#define CHECK_GAP 10000

typedef long int      code_int;
typedef long int      count_int;
typedef long int      cmp_code_int;
typedef unsigned char char_type;

#define ARGVAL() (*++(*argv) || (--argc && *++argv))

#define MAXCODE(n)	(1L << (n))

union bytes {
	long word;
	struct {
#if BYTEORDER == 4321
		char_type b1;
		char_type b2;
		char_type b3;
		char_type b4;
#elif BYTEORDER == 1234
		char_type b4;
		char_type b3;
		char_type b2;
		char_type b1;
#else
		int dummy;
#endif
	} bytes;
};
#if BYTEORDER == 4321 && NOALLIGN == 1
# define	output(b,o,c,n)	{													\
							*(long *)&((b)[(o)>>3]) |= ((long)(c))<<((o)&0x7);\
							(o) += (n);										\
						}
#elif BYTEORDER == 1234
# define	output(b,o,c,n)	{	char_type	*p = &(b)[(o)>>3];				\
							union bytes i;									\
							i.word = ((long)(c))<<((o)&0x7);				\
							p[0] |= i.bytes.b1;								\
							p[1] |= i.bytes.b2;								\
							p[2] |= i.bytes.b3;								\
							(o) += (n);										\
						}
#else
# define	output(b,o,c,n)	{	char_type	*p = &(b)[(o)>>3];				\
							long		 i = ((long)(c))<<((o)&0x7);	\
							p[0] |= (char_type)(i);							\
							p[1] |= (char_type)(i>>8);						\
							p[2] |= (char_type)(i>>16);						\
							(o) += (n);										\
						}
#endif
#if BYTEORDER == 4321 && NOALLIGN == 1
# define	input(b,o,c,n,m){													\
							(c) = (*(long *)(&(b)[(o)>>3])>>((o)&0x7))&(m);	\
							(o) += (n);										\
						}
#else
# define	input(b,o,c,n,m){	char_type 		*p = &(b)[(o)>>3];			\
							(c) = ((((long)(p[0]))|((long)(p[1])<<8)|		\
									 ((long)(p[2])<<16))>>((o)&0x7))&(m);	\
							(o) += (n);										\
						}
#endif

char			*progname;			/* Program name									*/
int 			silent = 0;			/* don't tell me about errors					*/
int 			quiet = 1;			/* don't tell me about compression 				*/
int				do_decomp = 0;		/* Decompress mode								*/
int				force = 0;			/* Force overwrite of files and links			*/
int				nomagic = 0;		/* Use a 3-byte magic number header,			*/
									/* unless old file 								*/
int				block_mode = BLOCK_MODE;/* Block compress mode -C compatible with 2.0*/
int				maxbits = BITS;		/* user settable max # bits/code 				*/
int 			zcat_flg = 0;		/* Write output on stdout, suppress messages 	*/
int				recursive = 0;  	/* compress directories 						*/
int				exit_code = -1;		/* Exitcode of compress (-1 no file compressed)	*/

char_type		inbuf[IBUFSIZ+64];	/* Input buffer									*/
char_type		outbuf[OBUFSIZ+2048];/* Output buffer								*/

struct stat		infstat;			/* Input file status							*/
char			*ifname;			/* Input filename								*/
int				remove_ofname = 0;	/* Remove output file on a error				*/
char 			ofname[MAXPATHLEN];	/* Output filename								*/
int				fgnd_flag = 0;		/* Running in background (SIGINT=SIGIGN)		*/

long 			bytes_in;			/* Total number of byte from input				*/
long 			bytes_out;			/* Total number of byte to output				*/

/* Normal machine */
count_int		htab[HSIZE];
unsigned short	codetab[HSIZE];

#define	htabof(i)				htab[i]
#define	codetabof(i)			codetab[i]
#define	tab_prefixof(i)			codetabof(i)
#define	tab_suffixof(i)			((char_type *)(htab))[i]
#define	de_stack				((char_type *)&(htab[HSIZE-1]))
#define	clear_htab()			memset(htab, -1, sizeof(htab))
#define	clear_tab_prefixof()	memset(codetab, 0, 256);

#ifdef FAST
int primetab[256] =		/* Special secondary hash table.		*/
{
	 1013, -1061, 1109, -1181, 1231, -1291, 1361, -1429,
	 1481, -1531, 1583, -1627, 1699, -1759, 1831, -1889,
	 1973, -2017, 2083, -2137, 2213, -2273, 2339, -2383,
	 2441, -2531, 2593, -2663, 2707, -2753, 2819, -2887,
	 2957, -3023, 3089, -3181, 3251, -3313, 3361, -3449,
	 3511, -3557, 3617, -3677, 3739, -3821, 3881, -3931,
	 4013, -4079, 4139, -4219, 4271, -4349, 4423, -4493,
	 4561, -4639, 4691, -4783, 4831, -4931, 4973, -5023,
	 5101, -5179, 5261, -5333, 5413, -5471, 5521, -5591,
	 5659, -5737, 5807, -5857, 5923, -6029, 6089, -6151,
	 6221, -6287, 6343, -6397, 6491, -6571, 6659, -6709,
	 6791, -6857, 6917, -6983, 7043, -7129, 7213, -7297,
	 7369, -7477, 7529, -7577, 7643, -7703, 7789, -7873,
	 7933, -8017, 8093, -8171, 8237, -8297, 8387, -8461,
	 8543, -8627, 8689, -8741, 8819, -8867, 8963, -9029,
	 9109, -9181, 9241, -9323, 9397, -9439, 9511, -9613,
	 9677, -9743, 9811, -9871, 9941,-10061,10111,-10177,
	10259,-10321,10399,-10477,10567,-10639,10711,-10789,
	10867,-10949,11047,-11113,11173,-11261,11329,-11423,
	11491,-11587,11681,-11777,11827,-11903,11959,-12041,
	12109,-12197,12263,-12343,12413,-12487,12541,-12611,
	12671,-12757,12829,-12917,12979,-13043,13127,-13187,
	13291,-13367,13451,-13523,13619,-13691,13751,-13829,
	13901,-13967,14057,-14153,14249,-14341,14419,-14489,
	14557,-14633,14717,-14767,14831,-14897,14983,-15083,
	15149,-15233,15289,-15359,15427,-15497,15583,-15649,
	15733,-15791,15881,-15937,16057,-16097,16189,-16267,
	16363,-16447,16529,-16619,16691,-16763,16879,-16937,
	17021,-17093,17183,-17257,17341,-17401,17477,-17551,
	17623,-17713,17791,-17891,17957,-18041,18097,-18169,
	18233,-18307,18379,-18451,18523,-18637,18731,-18803,
	18919,-19031,19121,-19211,19273,-19381,19429,-19477
};
#endif

void Usage(int);
void comprexx(char *);
void compdir(char *);
void compress(int, int);
void decompress(int, int);
void read_error(void);
void write_error(void);
void abort_compress(void);
void prratio(FILE *, long, long);
void about(void);

/*
 * TAG( main )
 *
 * Algorithm from "A Technique for High Performance Data Compression",
 * Terry A. Welch, IEEE Computer Vol 17, No 6 (June 1984), pp 8-19.
 *
 * Usage: compress [-dfvc] [-b bits] [file ...]
 * Inputs:
 *   -d:     If given, decompression is done instead.
 *
 *   -c:     Write output on stdout, don't remove original.
 *
 *   -b:     Parameter limits the max number of bits/code.
 *
 *   -f:     Forces output file to be generated, even if one already
 *           exists, and even if no space is saved by compressing.
 *           If -f is not used, the user will be prompted if stdin is
 *           a tty, otherwise, the output file will not be overwritten.
 *
 *   -v:     Write compression statistics
 *
 *   -r:     Recursive. If a filename is a directory, descend
 *           into it and compress everything in it.
 *
 * file ...:
 *           Files to be compressed.  If none specified, stdin is used.
 * Outputs:
 *   file.Z:     Compressed form of file with same mode, owner, and utimes
 *   or stdout   (if stdin used as input)
 *
 * Assumptions:
 *   When filenames are given, replaces with the compressed version
 *   (.Z suffix) only if the file decreases in size.
 *
 * Algorithm:
 *   Modified Lempel-Ziv method (LZW).  Basically finds common
 *   substrings and replaces them with a variable size code.  This is
 *   deterministic, and can be done on the fly.  Thus, the decompression
 *   procedure needs no input table, but tracks the way the table was built.
 */
int main(int argc, char *argv[])
{
	int i;

	if ((fgnd_flag = (signal(SIGINT, SIG_IGN)) != SIG_IGN))
		signal(SIGINT, (SIG_TYPE)abort_compress);

	signal(SIGTERM, (SIG_TYPE)abort_compress);
	signal(SIGHUP, (SIG_TYPE)abort_compress);

	if ((progname = strrchr(argv[0], '/')) != 0)
		progname++;
	else
		progname = argv[0];

	if (strcmp(progname, "uncompress") == 0)
		do_decomp = 1;
	else if (strcmp(progname, "zcat") == 0)
		do_decomp = zcat_flg = 1;

	/* Argument Processing
	 * All flags are optional.
	 * -V => print Version; debug verbose
	 * -d => do_decomp
	 * -v => unquiet
	 * -f => force overwrite of output file
	 * -n => no header: useful to uncompress old files
	 * -b maxbits => maxbits.  If -b is specified, then maxbits MUST be given also.
	 * -c => cat all output to stdout
	 * -C => generate output compatible with compress 2.0.
	 * -r => recursively compress directories
	 * if a string is left, must be an input filename.
	 */

	while ((i = getopt(argc, argv, "VsvdfFnCb:cqrRh")) != -1) {
		switch (i) {
			case 'V':
				about();
				break;
			case 's':
				silent = 1;
				quiet = 1;
				break;
			case 'v':
				silent = 0;
				quiet = 0;
				break;
			case 'd':
				do_decomp = 1;
				break;
			case 'f':
			case 'F':
				force = 1;
				break;
			case 'n':
				nomagic = 1;
				break;
			case 'C':
				block_mode = 0;
				break;
			case 'b':
				maxbits = atoi(optarg);
				break;
			case 'c':
				zcat_flg = 1;
				break;
			case 'q':
				quiet = 1;
				break;
			case 'r':
			case 'R':
				recursive = 1;
				break;
			case 'h':
				Usage(0);
			case ':':
				fprintf(stderr, "Option '%c' is missing parameter\n", optopt);
				Usage(1);
			case '?':
				fprintf(stderr, "Unknown option '%c' or argument missing\n", optopt);
				Usage(1);
			default:
				fprintf(stderr, "Unknown flag '%c'\n", **argv);
				Usage(1);
		}
	}

	if (maxbits < INIT_BITS)	maxbits = INIT_BITS;
	if (maxbits > BITS) 		maxbits = BITS;

	if (argc > optind) {
 		for (i = optind; i < argc; ++i)
			comprexx(argv[i]);
	} else {
		/* Standard input */
		ifname = "";
		exit_code = 0;
		remove_ofname = 0;

		if (do_decomp == 0) {
			compress(0, 1);

			if (zcat_flg == 0 && !quiet) {
				fprintf(stderr, "Compression: ");
				prratio(stderr, bytes_in-bytes_out, bytes_in);
				fprintf(stderr, "\n");
			}

			if (bytes_out >= bytes_in && !(force))
				exit_code = 2;
		} else
			decompress(0, 1);
	}

	exit(exit_code == -1 ? 1 : exit_code);
}

void Usage(int exit_status)
{
	fprintf((exit_status == 0 ? stdout : stderr),
		"Usage: %s [-dfvcVr] [-b maxbits] [file ...]\n"
		"       -d   If given, decompression is done instead.\n"
		"       -c   Write output on stdout, don't remove original.\n"
		"       -b   Parameter limits the max number of bits/code.\n"
		"       -f   Forces output file to be generated, even if one already.\n"
		"            exists, and even if no space is saved by compressing.\n"
		"            If -f is not used, the user will be prompted if stdin is.\n"
		"            a tty, otherwise, the output file will not be overwritten.\n"
		"       -v   Write compression statistics.\n"
		"       -V   Output vesion and compile options.\n"
		"       -r   Recursive. If a filename is a directory, descend\n"
		"            into it and compress everything in it.\n",
		progname);

	exit(exit_status);
}

void comprexx(char *fileptr)
{
	int fdin, fdout;
	char tempname[MAXPATHLEN];

	if (strlen(fileptr) > sizeof(tempname) - 3) {
		fprintf(stderr, "Pathname too long: %s\n", fileptr);
		exit_code = 1;
		return;
	}

	strcpy(tempname, fileptr);
	errno = 0;

	if (lstat(tempname, &infstat) == -1) {
		if (do_decomp) {
			switch (errno) {
			case ENOENT:	/* file doesn't exist */
				/*
				 * if the given name doesn't end with .Z, try appending one
				 * This is obviously the wrong thing to do if it's a
				 * directory, but it shouldn't do any harm.
				 */
				if (strcmp(tempname + strlen(tempname) - 2, ".Z") != 0) {
					strcat(tempname, ".Z");
					errno = 0;
					if (lstat(tempname, &infstat) == -1) {
						perror(tempname);
						exit_code = 1;
						return;
					}

					if ((infstat.st_mode & S_IFMT) != S_IFREG) {
						fprintf(stderr, "%s: Not a regular file.\n", tempname);
						exit_code = 1;
						return;
					}
				} else {
					perror(tempname);
					exit_code = 1;
					return;
				}

				break;

			default:
				perror(tempname);
				exit_code = 1;
				return;
			}
		} else {
			perror(tempname);
			exit_code = 1;
			return;
		}
	}

	switch (infstat.st_mode & S_IFMT) {
		case S_IFDIR:	/* directory */
			if (recursive)
				compdir(tempname);
			else if (!quiet)
				fprintf(stderr, "%s is a directory -- ignored\n", tempname);
			break;

		case S_IFREG:	/* regular file */
			if (do_decomp != 0) {
				/* DECOMPRESSION */
				if (!zcat_flg) {
					if (strcmp(tempname + strlen(tempname) - 2, ".Z") != 0) {
						if (!quiet)
							fprintf(stderr, "%s - no .Z suffix\n", tempname);
						return;
					}
				}

				strcpy(ofname, tempname);

				/* Strip off .Z suffix */
				if (strcmp(tempname + strlen(tempname) - 2, ".Z") == 0)
					ofname[strlen(tempname) - 2] = '\0';
			} else {
				/* COMPRESSION */
				if (!zcat_flg) {
					if (strcmp(tempname + strlen(tempname) - 2, ".Z") == 0) {
						fprintf(stderr, "%s: already has .Z suffix -- no change\n", tempname);
						return;
					}

					if (infstat.st_nlink > 1 && (!force)) {
						fprintf(stderr, "%s has %d other links: unchanged\n",
										tempname, (int)(infstat.st_nlink - 1));
						exit_code = 1;
						return;
					}
				}

				strcpy(ofname, tempname);
				strcat(ofname, ".Z");
			}

			if ((fdin = open(ifname = tempname, O_RDONLY|O_BINARY)) == -1) {
				perror(tempname);
				exit_code = 1;
				return;
			}

			if (zcat_flg == 0) {
				int c, s;
				struct stat statbuf, statbuf2;

				if (stat(ofname, &statbuf) == 0) {
					if ((s = strlen(ofname)) > 8) {
						c = ofname[s-1];
						ofname[s-1] = '\0';

						statbuf2 = statbuf;

						if (!stat(ofname, &statbuf2) &&
							statbuf.st_mode  == statbuf2.st_mode &&
							statbuf.st_ino   == statbuf2.st_ino &&
							statbuf.st_dev   == statbuf2.st_dev &&
							statbuf.st_uid   == statbuf2.st_uid &&
							statbuf.st_gid   == statbuf2.st_gid &&
							statbuf.st_size  == statbuf2.st_size &&
							statbuf.st_atime == statbuf2.st_atime &&
							statbuf.st_mtime == statbuf2.st_mtime &&
							statbuf.st_ctime == statbuf2.st_ctime)
						{
							fprintf(stderr, "%s: filename too long to tack on .Z\n", tempname);
							exit_code = 1;
							return;
						}

						ofname[s-1] = (char)c;
					}

					if (!force) {
						inbuf[0] = 'n';

						fprintf(stderr, "%s already exists.\n", ofname);

						if (fgnd_flag && isatty(0)) {
							fprintf(stderr, "Do you wish to overwrite %s (y or n)? ", ofname);
							fflush(stderr);

							if (read(0, inbuf, 1) > 0) {
								if (inbuf[0] != '\n') {
									do {
										if (read(0, inbuf+1, 1) <= 0) {
											perror("stdin");
											break;
										}
									} while (inbuf[1] != '\n');
								}
							} else
								perror("stdin");
						}

						if (inbuf[0] != 'y') {
							fprintf(stderr, "%s not overwritten\n", ofname);
							exit_code = 1;
							return;
						}
					}

					if (unlink(ofname)) {
						fprintf(stderr, "Can't remove old output file\n");
						perror(ofname);
						exit_code = 1;
						return;
					}
				}

				if ((fdout = open(ofname, O_WRONLY|O_CREAT|O_EXCL|O_BINARY, 0600)) == -1) {
					perror(tempname);
					return;
				}

				if ((s = strlen(ofname)) > 8) {
					if (fstat(fdout, &statbuf)) {
						fprintf(stderr, "Can't get status op output file\n");
						perror(ofname);
						exit_code = 1;
						return;
					}

					c = ofname[s-1];
					ofname[s-1] = '\0';
					statbuf2 = statbuf;

					if (!stat(ofname, &statbuf2) &&
						statbuf.st_mode  == statbuf2.st_mode &&
						statbuf.st_ino   == statbuf2.st_ino &&
						statbuf.st_dev   == statbuf2.st_dev &&
						statbuf.st_uid   == statbuf2.st_uid &&
						statbuf.st_gid   == statbuf2.st_gid &&
						statbuf.st_size  == statbuf2.st_size &&
						statbuf.st_atime == statbuf2.st_atime &&
						statbuf.st_mtime == statbuf2.st_mtime &&
						statbuf.st_ctime == statbuf2.st_ctime)
					{
						fprintf(stderr, "%s: filename too long to tack on .Z\n", tempname);

						if (unlink(ofname)) {
							fprintf(stderr, "can't remove bad output file\n");
							perror(ofname);
						}
						exit_code = 1;
						return;
					}

					ofname[s-1] = (char)c;
				}

				if (!quiet)
					fprintf(stderr, "%s: ", tempname);

				remove_ofname = 1;
			} else {
				fdout = 1;
				ofname[0] = '\0';
				remove_ofname = 0;
			}

			if (do_decomp == 0)
				compress(fdin, fdout);
			else
				decompress(fdin, fdout);

			close(fdin);

			if (fdout != 1 && close(fdout))
				write_error();

			if (bytes_in == 0) {
				if (remove_ofname) {
					if (unlink(ofname)) { /* Remove input file */
						fprintf(stderr, "\nunlink error (ignored) ");
						perror(ofname);
						exit_code = 1;
					}

					remove_ofname = 0;
				}
			} else if (zcat_flg == 0) {
				struct utimbuf timep;

				if (!do_decomp && bytes_out >= bytes_in && (!force)) {
					/* No compression: remove file.Z */
					if (!quiet)
						fprintf(stderr, "No compression -- %s unchanged\n", ifname);

					if (unlink(ofname)) {
						fprintf(stderr, "unlink error (ignored) ");
						perror(ofname);
					}

					remove_ofname = 0;
					exit_code = 2;
				} else {
					/* ***** Successful Compression ***** */
					if (!quiet) {
						fprintf(stderr, " -- replaced with %s", ofname);

						if (!do_decomp) {
							fprintf(stderr, " Compression: ");
							prratio(stderr, bytes_in-bytes_out, bytes_in);
						}

						fprintf(stderr, "\n");
					}

					timep.actime = infstat.st_atime;
					timep.modtime = infstat.st_mtime;

					if (utime(ofname, &timep)) {
						fprintf(stderr, "\nutime error (ignored) ");
						perror(ofname);
						exit_code = 1;
					}

					if (chmod(ofname, infstat.st_mode & 07777)) { /* Copy modes */
						fprintf(stderr, "\nchmod error (ignored) ");
						perror(ofname);
						exit_code = 1;
					}
					chown(ofname, infstat.st_uid, infstat.st_gid);	/* Copy ownership */
					remove_ofname = 0;

					if (unlink(ifname)) { /* Remove input file */
						fprintf(stderr, "\nunlink error (ignored) ");
						perror(ifname);
						exit_code = 1;
					}
				}
			}

			if (exit_code == -1)
				exit_code = 0;

			break;

		default:
			fprintf(stderr, "%s is not a directory or a regular file - ignored\n",
				tempname);
			break;
	}
}

void compdir(char *dir)
{
	struct dirent *dp;
	DIR *dirp;
	char nbuf[MAXPATHLEN];
	char *nptr = nbuf;

	dirp = opendir(dir);

	if (dirp == NULL) {
		printf("%s unreadable\n", dir); /* not stderr! */
		return;
	}

	/*
	 * WARNING: the following algorithm will occasionally cause
	 * compress to produce error warnings of the form "<filename>.Z
	 * already has .Z suffix - ignored". This occurs when the
	 * .Z output file is inserted into the directory below
	 * readdir's current pointer.
	 * These warnings are harmless but annoying. The alternative
	 * to allowing this would be to store the entire directory
	 * list in memory, then compress the entries in the stored
	 * list. Given the depth-first recursive algorithm used here,
	 * this could use up a tremendous amount of memory. I don't
	 * think it's worth it. -- Dave Mack
	 */

	while ((dp = readdir(dirp)) != NULL) {
		if (dp->d_ino == 0)
			continue;

		if (strcmp(dp->d_name, ".") == 0 || strcmp(dp->d_name, "..") == 0)
			continue;

		if ((strlen(dir)+strlen(dp->d_name)+1) < (MAXPATHLEN - 1)) {
			strcpy(nbuf, dir);
			strcat(nbuf, "/");
			strcat(nbuf, dp->d_name);
			comprexx(nptr);
		} else
			fprintf(stderr, "Pathname too long: %s/%s\n", dir, dp->d_name);
	}

	closedir(dirp);

	return;
}

/*
 * compress fdin to fdout
 *
 * Algorithm:  use open addressing double hashing (no chaining) on the
 * prefix code / next character combination.  We do a variant of Knuth's
 * algorithm D (vol. 3, sec. 6.4) along with G. Knott's relatively-prime
 * secondary probe.  Here, the modular division first probe is gives way
 * to a faster exclusive-or manipulation.  Also do block compression with
 * an adaptive reset, whereby the code table is cleared when the compression
 * ratio decreases, but after the table fills.  The variable-length output
 * codes are re-sized at this point, and a special CLEAR code is generated
 * for the decompressor.  Late addition:  construct the table according to
 * file size for noticeable speed improvement on small files.  Please direct
 * questions about this implementation to ames!jaw.
 */
void compress(int fdin, int fdout)
{
	long hp;
	int rpos;
	int outbits;
	int rlop;
	int rsize;
	int stcode;
	code_int free_ent;
	int boff;
	int n_bits;
	int ratio;
	long checkpoint;
	code_int extcode;
	union {
		long code;
		struct {
			char_type c;
			unsigned short ent;
		} e;
	} fcode;

	ratio = 0;
	checkpoint = CHECK_GAP;
	extcode = MAXCODE(n_bits = INIT_BITS)+1;
	stcode = 1;
	free_ent = FIRST;

	memset(outbuf, 0, sizeof(outbuf));
	bytes_out = 0; bytes_in = 0;
	outbuf[0] = MAGIC_1;
	outbuf[1] = MAGIC_2;
	outbuf[2] = (char)(maxbits | block_mode);
	boff = outbits = (3<<3);
	fcode.code = 0;

	clear_htab();

	while ((rsize = read(fdin, inbuf, IBUFSIZ)) > 0) {
		if (bytes_in == 0) {
			fcode.e.ent = inbuf[0];
			rpos = 1;
		} else
			rpos = 0;

		rlop = 0;

		do {
			if (free_ent >= extcode && fcode.e.ent < FIRST) {
				if (n_bits < maxbits) {
					boff = outbits = (outbits-1)+((n_bits<<3)-
					                 ((outbits-boff-1+(n_bits<<3))%(n_bits<<3)));
					if (++n_bits < maxbits)
						extcode = MAXCODE(n_bits)+1;
					else
						extcode = MAXCODE(n_bits);
				} else {
					extcode = MAXCODE(16)+OBUFSIZ;
					stcode = 0;
				}
			}

			if (!stcode && bytes_in >= checkpoint && fcode.e.ent < FIRST) {
				long int rat;

				checkpoint = bytes_in + CHECK_GAP;

				if (bytes_in > 0x007fffff) {
					/* shift will overflow */
					rat = (bytes_out+(outbits>>3)) >> 8;

					/* Don't divide by zero */
					if (rat == 0)
						rat = 0x7fffffff;
					else
						rat = bytes_in / rat;
				} else
					/* 8 fractional bits */
					rat = (bytes_in << 8) / (bytes_out+(outbits>>3));
				if (rat >= ratio)
					ratio = (int)rat;
				else {
					ratio = 0;
					clear_htab();
					output(outbuf, outbits, CLEAR, n_bits);
					boff = outbits = (outbits-1)+((n_bits<<3)-
					                 ((outbits-boff-1+(n_bits<<3))%(n_bits<<3)));
					extcode = MAXCODE(n_bits = INIT_BITS)+1;
					free_ent = FIRST;
					stcode = 1;
				}
			}

			if (outbits >= (OBUFSIZ<<3)) {
				if (write(fdout, outbuf, OBUFSIZ) != OBUFSIZ)
					write_error();

				outbits -= (OBUFSIZ<<3);
				boff = -(((OBUFSIZ<<3)-boff)%(n_bits<<3));
				bytes_out += OBUFSIZ;

				memcpy(outbuf, outbuf+OBUFSIZ, (outbits>>3)+1);
				memset(outbuf+(outbits>>3)+1, '\0', OBUFSIZ);
			}

			{
				int i = rsize-rlop;

				if ((code_int)i > extcode-free_ent)
					i = (int)(extcode-free_ent);

				if (i > ((sizeof(outbuf) - 32)*8 - outbits)/n_bits)
					i = ((sizeof(outbuf) - 32)*8 - outbits)/n_bits;

				if (!stcode && (long)i > checkpoint-bytes_in)
					i = (int)(checkpoint-bytes_in);

				rlop += i;
				bytes_in += i;
			}

			goto next;
hfound:		fcode.e.ent = codetabof(hp);
next:		if (rpos >= rlop)
				goto endlop;
next2:		fcode.e.c = inbuf[rpos++];

#ifndef FAST
			{
				code_int	i;
				hp = (((long)(fcode.e.c)) << (BITS-8)) ^ (long)(fcode.e.ent);

				if ((i = htabof(hp)) == fcode.code)
					goto hfound;

				if (i != -1) {
					long disp;

					disp = (HSIZE - hp)-1;	/* secondary hash (after G. Knott) */

					do {
						if ((hp -= disp) < 0)
							hp += HSIZE;

						if ((i = htabof(hp)) == fcode.code)
							goto hfound;
					} while (i != -1);
				}
			}
#else
			{
				long i, p;
				hp = ((((long)(fcode.e.c)) << (HBITS-8)) ^ (long)(fcode.e.ent));

				if ((i = htabof(hp)) == fcode.code)  goto hfound;
				if (i == -1)                         goto out;

				p = primetab[fcode.e.c];
lookup:			hp = (hp+p)&HMASK;
				if ((i = htabof(hp)) == fcode.code)  goto hfound;
				if (i == -1)                         goto out;
				hp = (hp+p)&HMASK;
				if ((i = htabof(hp)) == fcode.code)  goto hfound;
				if (i == -1)                         goto out;
				hp = (hp+p)&HMASK;
				if ((i = htabof(hp)) == fcode.code)  goto hfound;
				if (i == -1)                         goto out;
				goto lookup;
			}
out:		;
#endif
			output(outbuf, outbits, fcode.e.ent, n_bits);

			{
				long fc;
				fc = fcode.code;
				fcode.e.ent = fcode.e.c;

				if (stcode) {
					codetabof(hp) = (unsigned short)free_ent++;
					htabof(hp) = fc;
				}
			}

			goto next;

endlop:		if (fcode.e.ent >= FIRST && rpos < rsize)
				goto next2;

			if (rpos > rlop) {
				bytes_in += rpos-rlop;
				rlop = rpos;
			}
		} while (rlop < rsize);
	}

	if (rsize < 0)
		read_error();

	if (bytes_in > 0)
		output(outbuf, outbits, fcode.e.ent, n_bits);

	if (write(fdout, outbuf, (outbits+7)>>3) != (outbits+7)>>3)
		write_error();

	bytes_out += (outbits+7)>>3;

	return;
}

/*
 * Decompress stdin to stdout.  This routine adapts to the codes in the
 * file building the "string" table on-the-fly; requiring no table to
 * be stored in the compressed file.  The tables used herein are shared
 * with those of the compress() routine.  See the definitions above.
 */
void decompress(int fdin, int fdout)
{
	char_type *stackp;
	code_int code;
	int finchar;
	code_int oldcode;
	code_int incode;
	int inbits;
	int posbits;
	int outpos;
	int insize;
	int bitmask;
	code_int free_ent;
	code_int maxcode;
	code_int maxmaxcode;
	int n_bits;
	int rsize;

	bytes_in = 0;
	bytes_out = 0;
	insize = 0;

	while (insize < 3 && (rsize = read(fdin, inbuf+insize, IBUFSIZ)) > 0)
		insize += rsize;

	if (insize < 3 || inbuf[0] != MAGIC_1 || inbuf[1] != MAGIC_2) {
		if (rsize < 0)
			read_error();

		if (insize > 0) {
			fprintf(stderr, "%s: not in compressed format\n",
				(ifname[0] != '\0'? ifname : "stdin"));
			exit_code = 1;
		}

		return;
	}

	maxbits = inbuf[2] & BIT_MASK;
	block_mode = inbuf[2] & BLOCK_MODE;
	maxmaxcode = MAXCODE(maxbits);

	if (maxbits > BITS) {
		fprintf(stderr,
			"%s: compressed with %d bits, can only handle %d bits\n",
			(*ifname != '\0' ? ifname : "stdin"), maxbits, BITS);
		exit_code = 4;
		return;
	}

	bytes_in = insize;
	maxcode = MAXCODE(n_bits = INIT_BITS)-1;
	bitmask = (1<<n_bits)-1;
	oldcode = -1;
	finchar = 0;
	outpos = 0;
	posbits = 3<<3;

	free_ent = ((block_mode) ? FIRST : 256);

	/* As above, initialize the first
	 * 256 entries in the table. */
	clear_tab_prefixof();

	for (code = 255; code >= 0; --code)
		tab_suffixof(code) = (char_type)code;

	do {
resetbuf: ;
		{
			int i, e, o;

			e = insize-(o = (posbits>>3));

			for (i = 0; i < e; ++i)
				inbuf[i] = inbuf[i+o];

			insize = e;
			posbits = 0;
		}

		if (insize < sizeof(inbuf)-IBUFSIZ) {
			if ((rsize = read(fdin, inbuf+insize, IBUFSIZ)) < 0)
				read_error();

			insize += rsize;
		}

		inbits = ((rsize > 0) ? (insize - insize%n_bits)<<3 :
		          (insize<<3)-(n_bits-1));

		while (inbits > posbits) {
			if (free_ent > maxcode) {
				posbits = ((posbits-1) + ((n_bits<<3) -
				           (posbits-1+(n_bits<<3))%(n_bits<<3)));

				++n_bits;
				if (n_bits == maxbits)
					maxcode = maxmaxcode;
				else
					maxcode = MAXCODE(n_bits)-1;

				bitmask = (1<<n_bits)-1;
				goto resetbuf;
			}

			input(inbuf, posbits, code, n_bits, bitmask);

			if (oldcode == -1) {
				if (code >= 256) {
					fprintf(stderr, "oldcode:-1 code:%i\n", (int)(code));
					fprintf(stderr, "uncompress: corrupt input\n");
					abort_compress();
				}
				outbuf[outpos++] = (char_type)(finchar = (int)(oldcode = code));
				continue;
			}

			if (code == CLEAR && block_mode) {
				clear_tab_prefixof();
				free_ent = FIRST - 1;
				posbits = ((posbits-1) + ((n_bits<<3) -
				           (posbits-1+(n_bits<<3))%(n_bits<<3)));
				maxcode = MAXCODE(n_bits = INIT_BITS)-1;
				bitmask = (1<<n_bits)-1;
				goto resetbuf;
			}

			incode = code;
			stackp = de_stack;

			if (code >= free_ent) { /* Special case for KwKwK string.	*/
				if (code > free_ent) {
					char_type *p;

					posbits -= n_bits;
					p = &inbuf[posbits>>3];

					fprintf(stderr, "insize:%d posbits:%d inbuf:%02X %02X %02X %02X %02X (%d)\n", insize, posbits,
						p[-1], p[0], p[1], p[2], p[3], (posbits&07));
					fprintf(stderr, "uncompress: corrupt input\n");
					abort_compress();
				}

				*--stackp = (char_type)finchar;
				code = oldcode;
			}

			while ((cmp_code_int)code >= (cmp_code_int)256) {
				/* Generate output characters in reverse order */
				*--stackp = tab_suffixof(code);
				code = tab_prefixof(code);
			}

			*--stackp =	(char_type)(finchar = tab_suffixof(code));

			/* And put them out in forward order */
			{
				int	i;

				if (outpos+(i = (de_stack-stackp)) >= OBUFSIZ) {
					do {
						if (i > OBUFSIZ-outpos)
							i = OBUFSIZ-outpos;

						if (i > 0) {
							memcpy(outbuf+outpos, stackp, i);
							outpos += i;
						}

						if (outpos >= OBUFSIZ) {
							if (write(fdout, outbuf, outpos) != outpos)
								write_error();

							outpos = 0;
						}
						stackp+= i;
					} while ((i = (de_stack-stackp)) > 0);
				} else {
					memcpy(outbuf+outpos, stackp, i);
					outpos += i;
				}
			}

			/* Generate the new entry. */
			if ((code = free_ent) < maxmaxcode) {
				tab_prefixof(code) = (unsigned short)oldcode;
				tab_suffixof(code) = (char_type)finchar;
				free_ent = code+1;
			}

			oldcode = incode;	/* Remember previous code.	*/
		}

		bytes_in += rsize;
	} while (rsize > 0);

	if (outpos > 0 && write(fdout, outbuf, outpos) != outpos)
		write_error();
}

void read_error(void)
{
	fprintf(stderr, "\nread error on");
	perror((ifname[0] != '\0') ? ifname : "stdin");
	abort_compress();
}

void write_error(void)
{
	fprintf(stderr, "\nwrite error on");
	perror((ofname[0] != '\0') ? ofname : "stdout");
	abort_compress();
}

void abort_compress(void)
{
	if (remove_ofname)
		unlink(ofname);

	exit(1);
}

void prratio(FILE *stream, long int num, long int den)
{
	/* Doesn't need to be long */
	int q;

	if (den > 0) {
		if (num > 214748L)
			q = (int)(num/(den/10000L));	/* 2147483647/10000 */
		else
			q = (int)(10000L*num/den);		/* Long calculations, though */
	} else
		q = 10000;

	if (q < 0) {
		putc('-', stream);
		q = -q;
	}

	fprintf(stream, "%d.%02d%%", q / 100, q % 100);
}

void about(void)
{
	printf(
		"Compress version: (N)compress %s, compiled: %s\n"
		"\n"
		"Compile options:\n"
		"     "
#if BYTEORDER == 0000
		"!BYTEORDER?, "
#endif
#ifdef FAST
		"FAST, "
#endif
		"IBUFSIZ=%d, OBUFSIZ=%d, BITS=%d, MAXPATHLEN=%d\n"
		"\n"
		"Homepage:\n"
		"     http://ncompress.sourceforge.net/\n"
		"\n"
		"Author version 4.2.5 (Modernization):\n"
		"     Mike Frysinger  (vapier@gmail.com)\n"
		"\n"
		"Author version 4.2 (Speed improvement & source cleanup):\n"
		"     Peter Jannesen  (peter@ncs.nl)\n"
		"\n"
		"Author version 4.1 (Added recursive directory compress):\n"
		"     Dave Mack  (csu@alembic.acs.com)\n"
		"\n"
		"Authors version 4.0 (World release in 1985):\n"
		"     Spencer W. Thomas, Jim McKie, Steve Davies,\n"
		"     Ken Turkowski, James A. Woods, Joe Orost\n",
		VERSION, __DATE__, IBUFSIZ, OBUFSIZ, BITS, MAXPATHLEN
	);

	exit(0);
}
