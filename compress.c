/*
 * Define NO_UCHAR if "unsigned char" functions as signed char on your
 * machine.
 */
/* #define NO_UCHAR */
#define STATS
#undef STATS
#define DEBUG
#undef DEBUG
/* 
 * compress.c - File compression ala IEEE Computer June 1984.
 * 
 * Authors:	Spencer W. Thomas	(decvax!harpo!utah-cs!utah-gr!thomas)
 * 		Computer Science Dept.
 * 		University of Utah
 * 		Copyright (c) 1984 Spencer W. Thomas
 * 
 *		Jim McKie	(decvax!mcvax!jim)
 *
 *		Steve Davies	(decvax!vax135!petsd!peora!srd)
 *
 *		Ken Turkowski	(decvax!decwrl!turtlevax!ken)
 *
 *		Joe Orost	(decvax!vax135!petsd!joe)
 *
 * $Header: compress.c,v 2.0 84/08/28 22:00:00 joe Exp $
 * $Log:	compress.c,v $
 * Revision 2.0   84/08/28  22:00:00  petsd!joe
 * Add check for foreground before prompting user.  Insert maxbits into
 * compressed file.  Force file being uncompressed to end with ".Z".
 * Added "-c" flag and "zcat".  Prepared for release.
 *
 * Revision 1.10  84/08/24  18:28:00  turtlevax!ken
 * Will only compress regular files (no directories), added a magic number
 * header (plus an undocumented -n flag to handle old files without headers),
 * added -f flag to force overwriting of possibly existing destination file,
 * otherwise the user is prompted for a response.  Will tack on a .Z to a
 * filename if it doesn't have one when decompressing.  Will only replace
 * file if it was compressed.
 *
 * Revision 1.9  84/08/16  17:28:00  turtlevax!ken
 * Removed scanargs(), getopt(), added .Z extension and unlimited number of
 * filenames to compress.  Flags may be clustered (-Ddvb12) or separated
 * (-D -d -v -b 12), or combination thereof.  Modes and other status is
 * copied with copystat().  -O bug for 4.2 seems to have disappeared with
 * 1.8.
 *
 * Revision 1.8  84/08/09  23:15:00  joe
 * Made it compatible with vax version, installed jim's fixes/enhancements
 *
 * Revision 1.6  84/08/01  22:08:00  joe
 * Sped up algorithm significantly by sorting the compress chain.
 *
 * Revision 1.5  84/07/13  13:11:00  srd
 * Added C version of vax asm routines.  Changed structure to arrays to
 * save much memory.  Do unsigned compares where possible (faster on
 * Perkin-Elmer)
 *
 * Revision 1.4  84/07/05  03:11:11  thomas
 * Clean up the code a little and lint it.  (Lint complains about all
 * the regs used in the asm, but I'm not going to "fix" this.)
 * 
 * Revision 1.3  84/07/05  02:06:54  thomas
 * Minor fixes.
 * 
 * Revision 1.2  84/07/05  00:27:27  thomas
 * Add variable bit length output.
 * 

 */
static char rcs_ident[] = "$Header: compress.c,v 2.0 84/08/28 22:00:00 joe Exp $";

#include <stdio.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>

#define ARGVAL() (*++(*argv) || (--argc && *++argv))

unsigned char magic_header[] = { "\037\235" };	/* 1F 9D */
#define INIT_BITS 9			/* initial number of bits/code */
#define	BITS	16			/* maximum number of bits/code */
int n_bits;				/* number of bits/code */
int maxbits = BITS;			/* user settable max # bits/code */
long int maxcode;			/* maximum code, given n_bits */
long int maxmaxcode = 1 << BITS;	/* should NEVER generate this code */
#ifdef COMPATIBLE		/* But wrong! */
# define MAXCODE(n_bits)	(1 << (n_bits) - 1)
#else COMPATIBLE
# define MAXCODE(n_bits)	((1 << (n_bits)) - 1)
#endif COMPATIBLE

/* 
 * One code could conceivably represent (1<<BITS) characters, but
 * to get a code of length N requires an input string of at least
 * N*(N-1)/2 characters.  With 10000 chars in the stack, an input
 * file would have to contain a 50Mb string of a single character.
 * This seems unlikely.
 */
#define MAXSTACK    10000		/* size of output stack */

unsigned short tab_next[1<<BITS];	/* chain of entries with same prefix */
unsigned short tab_chain[1<<BITS];	/* chain prefixed with this entry */
unsigned short tab_prefix[1<<BITS];	/* prefix code for this entry */
	 char  tab_suffix[1<<BITS];	/* last char in this entry */

long int free_ent = 0;			/* first unused entry */
int exit_stat = 0;

long int getcode();

Usage() {
#ifdef DEBUG
fprintf(stderr,"Usage: compress [-dDvfc] [-b maxbits] [file ...]\n");
}
int debug = 0;
#else DEBUG
fprintf(stderr,"Usage: compress [-dfc] [-b maxbits] [file ...]\n");
}
#endif DEBUG
int nomagic = 0;	/* Use a 2 byte magic number header, unless old file */
int zcat_flg = 0;	/* Write output on stdout, suppress messages */

/*****************************************************************
 * TAG( main )
 * 
 * Algorithm from "A Technique for High Performance Data Compression",
 * Terry A. Welch, IEEE Computer Vol 17, No 6 (June 1984), pp 8-19.
 * 
 * Usage: compress [-d] [-c] [-f] [-b bits] [file ...]
 * Inputs:
 *	-d:	    If given, decompression is done instead.
 *
 *      -c:         Write output on stdout.
 *
 *      -b:         Limits the max number of bits/code.
 *
 *	-f:	    Forces output file to be generated, even if one already
 *		    exists; if -f is not used, the user will be prompted if
 *		    the stdin is a tty, otherwise, the output file will not
 *		    be overwritten.
 * 
 * 	file ...:   Files to be compressed.  If none specified, stdin
 *		    is used.
 * Outputs:
 *	file.Z:	    Compressed form of file with same mode, owner, and utimes
 * 	or stdout   (if stdin used as input)
 * 
 * Assumptions:
 *	When filenames are given, replaces with the compressed version
 *	(.Z suffix) only if the file decreased in size.
 * Algorithm:
 * 	Modified Lempel-Ziv method (LZW).  Basically finds common
 * substrings and replaces them with a variable size code.  This is
 * deterministic, and can be done on the fly.  Thus, the decompression
 * procedure needs no input table, but tracks the way the table was
 * built.
 */


main( argc, argv )
register int argc; char **argv;
{
    int decomp_flg = 0;
    int overwrite = 0;	/* Do not overwrite unless given -f flag */
    char ofname[100], tempname[100];
    char **filelist, **fileptr;
    char *cp, *rindex();
    struct stat statbuf;

#ifdef DEBUG
    int verbose = 0;
#endif DEBUG
#ifdef COMPATIBLE
    nomagic = 1;	/* Original didn't have a magic number */
#endif COMPATIBLE

    filelist = fileptr = (char **)(malloc(argc * sizeof(*argv)));
    *filelist = NULL;

    if((cp = rindex(argv[0], '/')) != 0) {
	cp++;
    } else {
	cp = argv[0];
    }
    if(strcmp(cp, "uncompress") == 0) {
	decomp_flg = 1;
    } else if(strcmp(cp, "zcat") == 0) {
	decomp_flg = 1;
	zcat_flg = 1;
    }

#ifdef BSD42
    /* 4.2BSD dependent - take it out if not */
    setlinebuf( stderr );
#endif BSD42

    /* Argument Processing
     * All flags are optional.
     * -D => debug
     * -d => decomp_flg
     * -v => verbose
     * -f => force overwrite of output file
     * -n => no header: useful to uncompress old files
     * -b maxbits => maxbits.  If -b is specified, then maxbits MUST be
     *	    given also.
     * -c => cat all output to stdout
     * if a string is left, must be an input filename.
     */
    for (argc--, argv++; argc > 0; argc--, argv++) {
	if (**argv == '-') {	/* A flag argument */
	    while (*++(*argv)) {	/* Process all flags in this arg */
		switch (**argv) {
#ifdef DEBUG
		    case 'D':
			debug = 1;
			break;
		    case 'v':
			verbose = 1;
			break;
#endif DEBUG
		    case 'd':
			decomp_flg = 1;
			break;
		    case 'f':
			overwrite = 1;
			break;
		    case 'n':
			nomagic = 1;
			break;
		    case 'b':
			if (!ARGVAL()) {
			    fprintf(stderr, "Missing maxbits\n");
			    Usage();
			    exit(1);
			}
			maxbits = atoi(*argv);
			goto nextarg;
		    case 'c':
			zcat_flg = 1;
			break;
		    default:
			fprintf(stderr, "Unknown flag: '%c'; ", **argv);
			Usage();
			exit(1);
		}
	    }
	}
	else {		/* Input file name */
	    *fileptr++ = *argv;	/* Build input file list */
	    *fileptr = NULL;
	    /* goto nextarg; */
	}
	nextarg: continue;
    }

    if (maxbits > BITS) maxbits = BITS;
    maxmaxcode = 1 << maxbits;

    if (*filelist != NULL) {
	for (fileptr = filelist; *fileptr; fileptr++) {
	    exit_stat = 0;
	    if (decomp_flg != 0) {			/* DECOMPRESSION */
		/* Check for .Z suffix */
		if (strcmp(*fileptr + strlen(*fileptr) - 2, ".Z") != 0) {
		    /* No .Z: tack one on */
		    strcpy(tempname, *fileptr);
		    strcat(tempname, ".Z");
		    *fileptr = tempname;
		}
		/* Open input file */
		if ((freopen(*fileptr, "r", stdin)) == NULL) {
			perror(*fileptr); continue;
		}
		/* Check the magic number */
		if (nomagic == 0) {
		    if ((getchar() != (magic_header[0] & 0xFF))
		     || (getchar() != (magic_header[1] & 0xFF))) {
			fprintf(stderr, "%s: not in compressed format\n",
			    *fileptr);
		    continue;
		    }
		    maxbits = getchar();	/* set -b from file */
		    maxmaxcode = 1 << maxbits;
		    if(maxbits > BITS) {
			fprintf(stderr, 
			"%s: compressed with %d bits, can only handle %d bits\n",
			*fileptr, maxbits, BITS);
			continue;
		    }
		}
		/* Generate output filename */
		strcpy(ofname, *fileptr);
		ofname[strlen(*fileptr) - 2] = '\0';  /* Strip off .Z */
	    }
	    else {					/* COMPRESSION */
		if (strcmp(*fileptr + strlen(*fileptr) - 2, ".Z") == 0) {
		    fprintf(stderr, "%s: already has .Z suffix -- no change\n",
			    *fileptr);
		    continue;
		}
		/* Open input file */
		if ((freopen(*fileptr, "r", stdin)) == NULL) {
		    perror(*fileptr); continue;
		}
		/* Generate output filename */
		strcpy(ofname, *fileptr);
#ifndef BSD42		/* Short filenames */
		if ((cp=rindex(ofname,'/')) != NULL)	cp++;
		else					cp = ofname;
		if (strlen(cp) > 12) {
		    fprintf(stderr,"%s: filename too long to tack on .Z\n",cp);
		    continue;
		}
#endif  BSD42		/* Long filenames allowed */
		strcat(ofname, ".Z");
	    }
	    /* Check for overwrite of existing file */
	    if (overwrite == 0 && zcat_flg == 0) {
		if (stat(ofname, &statbuf) == 0) {
		    char response[2];
		    response[0] = 'n';
		    fprintf(stderr, "%s already exists;", ofname);
		    if (foreground()) {
			fprintf(stderr, " do you wish to overwrite (y or n)? ",
			ofname);
			fflush(stderr);
			read(2, response, 2);
			while (response[1] != '\n') {
			    if (read(2, response+1, 1) < 0) {	/* Ack! */
				perror("stderr"); break;
			    }
			}
		    }
		    if (response[0] != 'y') {
			fprintf(stderr, "\tnot overwritten\n");
			continue;
		    }
		}
	    }
	    if(zcat_flg == 0) {		/* Open output file */
	        if (freopen(ofname, "w", stdout) == NULL) {
		    perror(ofname);
		    continue;
	        }
	        fprintf(stderr, "%s: ", *fileptr);
	    }

	    /* Actually do the compression/decompression */
	    if (decomp_flg == 0)	compress();
#ifndef DEBUG
	    else			decompress();
#else   DEBUG
	    else if (debug == 0)	decompress();
	    else			printcodes();
	    if (verbose)		dump_tab();
#endif DEBUG
	    if(zcat_flg == 0) {
	        copystat(*fileptr, ofname);	/* Copy stats */
	        putc('\n', stderr);
	    }
	}
    } else {		/* Standard input */
	if (decomp_flg == 0) {
		compress();
		putc('\n', stderr);
	} else {
	    /* Check the magic number */
	    if (nomagic == 0) {
		if ((getchar()!=(magic_header[0] & 0xFF)) 
		 || (getchar()!=(magic_header[1] & 0xFF))) {
		    fprintf(stderr, "stdin: not in compressed format\n");
		    exit(1);
		}
		maxbits = getchar();	/* set -b from file */
    		maxmaxcode = 1 << maxbits;
		if(maxbits > BITS) {
			fprintf(stderr, 
			"stdin: compressed with %d bits, can only handle %d bits\n",
			maxbits, BITS);
			exit(1);
		}
	    }
#ifndef DEBUG
	    decompress();
#else   DEBUG
	    if (debug == 0)	decompress();
	    else		printcodes();
	    if (verbose)	dump_tab();
#endif DEBUG
	}
    }

    exit(exit_stat);
}


/*****************************************************************
 * TAG( compress )
 * 
 * Actually does the compression.
 * Inputs:
 * 	Compresses file on stdin.
 * Outputs:
 * 	Writes compressed file to stdout.
 * Assumptions:
 *	[None]
 * Algorithm:
 * 	See above.
 */
static int offset;
long int bytes_out;			/* count how many are written */

compress() {
    register c;
    register long ent, n_ent;
    register long int in_count = 1;
#ifdef STATS
    int out_count = 0;
#endif STATS

    /* 
     * Initialize the compression table to contain all 8-bit values
     * initially.  These don't need to be chained, they can be looked
     * up directly.
     */
    offset = 0;
    bytes_out = 0;
    maxcode = MAXCODE(n_bits = INIT_BITS);
    for ( ent = 0; ent < 256; ent++ ) {
	tab_next[ent] = tab_chain[ent] = NULL;
	tab_suffix[ent] = ent;
    }
    free_ent = 256;

#ifndef COMPATIBLE
    if (nomagic == 0) {
	putchar(magic_header[0]); putchar(magic_header[1]);
	putchar((char)maxbits);
    }
#endif COMPATIBLE

    ent = getchar();		/* initial entry */
    while ( !feof( stdin ) && (c = getchar()) != (unsigned)EOF ) {
	in_count++;
	/* 
	 * Find the entry corresponding to the current entry suffixed
	 * with this char.  Since the entries are sorted, stop when
	 * the suffix >= c.
	 */
	for (n_ent = tab_chain[ent]; n_ent != NULL; n_ent = tab_next[n_ent])
	    if ( tab_suffix[n_ent] >= (unsigned)c )
		goto found;			/* found it */

not_found:
	/* 
	 * If no such entry, do 2 things:
	 * 1. Put out code for current prefix string.
	 * 2. Add the new string to the table.
	 *    If the table is full, just start over with current input
	 *    character.
	 */
	    output( (long)ent );
#ifdef STATS
	    out_count++;
#endif STATS
	    if ( (n_ent=free_ent) < maxmaxcode ) {
		/* Chain the new entry in 'c' order, so the aborted check
		   above works */

		register p_ent;

		tab_chain[n_ent] = NULL;
		tab_suffix[n_ent] = c;
		if((p_ent=tab_chain[ent]) == NULL ||
		   ((unsigned) c) < tab_suffix[p_ent]) {
			tab_next[n_ent] = p_ent;
			tab_chain[ent] = n_ent;
		} else {
			for(;;) {
				ent = tab_next[p_ent];
				if(ent == NULL ||
				   ((unsigned) c) < tab_suffix[ent]) break;
				p_ent = ent;
			}
			tab_next[n_ent] = ent;
			tab_next[p_ent] = n_ent;
		}
		free_ent = n_ent+1;
	    }
	    n_ent = c;
cont:
	ent = n_ent;
    }
    /* 
     * Put out the final code.
     */
    output( (long)ent );
#ifdef STATS
    out_count++;
#endif STATS
    output( -1L );			/* finish up output if necessary */

    /* 
     * Print out stats on stderr
     */
    if(zcat_flg == 0) {
#ifdef STATS
        fprintf( stderr,
	"%ld chars in, %ld codes (%ld bytes) out, compression factor %g\n",
		in_count, out_count, bytes_out,
		(double)in_count / (double)bytes_out );
        fprintf( stderr, "\tCompression as in compact: %5.2f%%\n",
		100.0 * ( in_count - bytes_out ) / (double) in_count );
        fprintf( stderr, "\tLargest code was %d (%d bits)\n", free_ent - 1, n_bits );
#else STATS
        fprintf( stderr, "Compression: %5.2f%%",
		100.0 * ( in_count - bytes_out ) / (double) in_count );
#endif STATS
    }
    if(bytes_out > in_count)	/* exit(2) if no savings */
	exit_stat = 2;
    return;

found: 
	/* either we found the entry, or we skipped past it */

	if(tab_suffix[n_ent] == (unsigned) c) {
		goto cont;
	} else {
		goto not_found;
	}
}


/*****************************************************************
 * TAG( output )
 * 
 * Output the given code.
 * Inputs:
 * 	code:	A n_bits-bit integer.  If == -1, then EOF.  This assumes
 *		that n_bits =< (long)wordsize - 1.
 * 
 * Outputs:
 * 	Outputs code to the file.
 * Assumptions:
 *	Chars are 8 bits long.
 * Algorithm:
 * 	Maintain a BITS character long buffer (so that 8 codes will
 * fit in it exactly).  Use the VAX insv instruction to insert each
 * code in turn.  When the buffer fills up empty it and start over.
 */

static char buf[BITS];

unsigned char lmask[9] = {0xff, 0xfe, 0xfc, 0xf8, 0xf0, 0xe0, 0xc0, 0x80, 0x00};
unsigned char rmask[9] = {0x00, 0x01, 0x03, 0x07, 0x0f, 0x1f, 0x3f, 0x7f, 0xff};

output( code )
long int  code;
{
#ifdef DEBUG
    static int col = 0;
#endif DEBUG

    /* 
     * On the VAX, it is important to have the register declarations
     * in exactly the order given, or the asm will break.
     */
    register int r_off = offset, bits= n_bits;
    register char * bp = buf;

    if ( code >= 0 ) {
#ifdef vax
	/* VAX DEPENDENT!! Implementation on other machines may be
	 * difficult.
	 * 
	 * Translation: Insert BITS bits from the argument starting at
	 * offset bits from the beginning of buf.
	 */
	0;	/* C compiler bug ?? */
	asm( "insv	4(ap),r11,r10,(r9)" );
#else not a vax
/* WARNING: byte/bit numbering on the vax is simulated by the following code
*/
	/* 
	 * Get to the first byte.
	 */
	bp += (r_off >> 3);
	r_off &= 7;
	/* 
	 * Since code is always >= 8 bits, only need to mask the first
	 * hunk on the left.
	 */
	*bp = (*bp & rmask[r_off]) | (code << r_off) & lmask[r_off];
	bp++;
	bits -= (8 - r_off);
	code >>= 8 - r_off;
	/* Get any 8 bit parts in the middle (<=1 for up to 16 bits). */
	if ( bits >= 8 ) {
	    *bp++ = code;
	    code >>= 8;
	    bits -= 8;
	}
	/* Last bits. */
	*bp = code;
#endif vax
	offset += n_bits;
	if ( offset == (n_bits << 3) ) {
	    if( fwrite( buf, 1, n_bits, stdout ) != n_bits)
		writeerr();
	    offset = 0;
	    bytes_out += n_bits;
	}
#ifdef DEBUG
	if ( debug )
	    fprintf( stderr, "%5d%c", code,
		    (col+=6) >= 74 ? (col = 0, '\n') : ' ' );
#endif DEBUG

	/* 
	 * If the next entry is going to be too big for the code size,
	 * then increase it, if possible.
	 */
	if ( free_ent > maxcode )
	{
	    /* 
	     * Write the whole buffer, because the input side won't
	     * discover the size increase until after it has read it.
	     */
	    if ( offset > 0 )
	    {
		if( fwrite( buf, 1, n_bits, stdout ) != n_bits)
			writeerr();
		bytes_out += n_bits;
	    }
	    offset = 0;
		
	    n_bits++;
	    if ( n_bits == maxbits )
		maxcode = maxmaxcode;
	    else
		maxcode = MAXCODE(n_bits);
#ifdef DEBUG
	    if ( debug ) {
		fprintf( stderr, "\nChange to %d bits\n", n_bits );
		col = 0;
	    }
#endif DEBUG
	}
    } else {
	/* 
	 * At EOF, write the rest of the buffer.
	 */
	if ( offset > 0 )
	    fwrite( buf, 1, (offset + 7) / 8, stdout );
	bytes_out += (offset + 7) / 8;
	offset = 0;
	fflush( stdout );
#ifdef DEBUG
	if ( debug )
	    fprintf( stderr, "\n" );
#endif DEBUG
	if( ferror( stdout ) )
		writeerr();
    }
}


/*****************************************************************
 * TAG( decompress )
 * 
 * Decompress the input file.
 * 
 * Inputs:
 *	Decompresses file on stdin.
 * Outputs:
 * 	Writes decompressed file to stdout.
 * Algorithm:
 * 	See article cited above.
 */

decompress() {
    register int stack_top = MAXSTACK;
    register long int code, oldcode, incode;
    register int finchar;
    char stack[MAXSTACK];

    /* 
     * As above, initialize the first 256 entries in the table.  
     */
    maxcode = MAXCODE(n_bits = INIT_BITS);
    for ( free_ent = 0; free_ent < 256; free_ent++ ) {
	tab_next[free_ent] = tab_chain[free_ent] = NULL;
	tab_prefix[free_ent] = 0;
	tab_suffix[free_ent] = free_ent;
    }

    finchar = oldcode = getcode();
    putchar( (char)finchar );		/* first code must be 8 bits = char */

    while ( ( code = getcode() ) != -1 ) {
	incode = code;
	/* 
	 * Special case for KwKwK string.
	 */
	if ( code >= free_ent ) {
	    stack[--stack_top] = finchar;
	    code = oldcode;
	}

	/* 
	 * Generate output characters in reverse order
	 */
	while ( ((unsigned long)code) >= ((unsigned long)256) ) {
	    stack[--stack_top] = tab_suffix[code];
	    code = tab_prefix[code];
	}
	stack[--stack_top] = finchar = tab_suffix[code];

	/* 
	 * And put them out in forward order
	 */
	if(fwrite( &stack[stack_top], 1, MAXSTACK - stack_top, stdout ) !=
	MAXSTACK - stack_top) writeerr();
	stack_top = MAXSTACK;

	/* 
	 * Generate the new entry.
	 */
	if ( (code=free_ent) < maxmaxcode ) {
	    tab_prefix[code] = oldcode;
	    tab_suffix[code] = finchar;
	    free_ent = code+1;
	}
	/* 
	 * Remember previous code.
	 */
	oldcode = incode;
    }
    fflush( stdout );
    if(ferror(stdout))
	writeerr();
}


/*****************************************************************
 * TAG( getcode )
 * 
 * Read one code from the standard input.  If EOF, return -1.
 * Inputs:
 * 	stdin
 * Outputs:
 * 	code or -1 is returned.
 * Assumptions:
 *	[None]
 * Algorithm:
 * 	For now, use scanf to read ascii input.
 */

long int
getcode() {
    /* 
     * On the VAX, it is important to have the register declarations
     * in exactly the order given, or the asm will break.
     */
    register long int code;
    static int offset = 0, size = 0;
    static unsigned char buf[BITS];
    register int r_off, bits;
    register unsigned char * bp = buf;

    if ( offset >= size || free_ent > maxcode ) {
	/* 
	 * If the next entry will be too big for the current code
	 * size, then we must increase the size.  This implies reading
	 * a new buffer full, too.
	 */
	if ( free_ent > maxcode ) {
	    n_bits++;
	    if ( n_bits == maxbits )
		maxcode = maxmaxcode;	/* won't get any bigger now */
	    else
		maxcode = MAXCODE(n_bits);
	}
	size = fread( buf, 1, n_bits, stdin );
	if ( size <= 0 )
	    return -1;			/* end of file */
	offset = 0;
	/* Round size down to integral number of codes */
	size = (size << 3) - (n_bits - 1);
    }
    r_off = offset;
    bits = n_bits;
#ifdef vax
    asm( "extzv   r10,r9,(r8),r11" );
#else not a vax
	/* 
	 * Get to the first byte.
	 */
	bp += (r_off >> 3);
	r_off &= 7;
	/* Get first part (low order bits) */
#ifdef NO_UCHAR
	code = ((*bp++ >> r_off) & rmask[8 - r_off]) & 0xff;
#else  NO_UCHAR
	code = (*bp++ >> r_off);
#endif NO_UCHAR
	bits -= (8 - r_off);
	r_off = 8 - r_off;		/* now, offset into code word */
	/* Get any 8 bit parts in the middle (<=1 for up to 16 bits). */
	if ( bits >= 8 ) {
#ifdef NO_UCHAR
	    code |= (*bp++ & 0xff) << r_off;
#else  NO_UCHAR
	    code |= *bp++ << r_off;
#endif NO_UCHAR
	    r_off += 8;
	    bits -= 8;
	}
	/* high order bits. */
	code |= (*bp & rmask[bits]) << r_off;
#endif vax
    offset += n_bits;

    return code;
}

char *
rindex(s, c)		/* For those who don't have it in libc.a */
register char *s, c;
{
	char *p;
	for (p = NULL; *s; s++)
	    if (*s == c)
		p = s;
	return(p);
}

#ifdef DEBUG
printcodes()
{
    /* 
     * Just print out codes from input file.  Mostly for debugging.
     */
    long int code;
    int col = 0, bits;

    bits = n_bits = INIT_BITS;
    maxcode = MAXCODE(n_bits);
    free_ent = 255;
    while ( ( code = getcode() ) >= 0 ) {
	if ( free_ent < maxmaxcode )
	    free_ent++;
	if ( bits != n_bits ) {
	    printf( "\nChange to %d bits\n", n_bits );
	    bits = n_bits;
	    col = 0;
	}
	printf( "%5d%c", code, (col+=6) >= 74 ? (col = 0, '\n') : ' ' );
    }
    putchar( '\n' );
    exit( 0 );
}

/*****************************************************************
 * TAG( dump_tab )
 * 
 * Dump the string table.
 * Inputs:
 *	[None]
 * Outputs:
 *	[None]
 * Assumptions:
 *	[None]
 * Algorithm:
 *	[None]
 */

dump_tab()
{
    register int i;
    register ent;
    char stack[4 * MAXSTACK];	/* \nnn makes it 4 times bigger */
    int stack_top = 4 * MAXSTACK;

    for ( i = 0; i < free_ent; i++ ) {
	ent = i;
	if ( isascii(tab_suffix[ent]) && isprint(tab_suffix[ent]) )
	    fprintf( stderr, "%5d: %5d/'%c'  \"",
			ent, tab_prefix[ent], tab_suffix[ent] );
	else
	    fprintf( stderr, "%5d: %5d/\\%03o \"",
			ent, tab_prefix[ent], tab_suffix[ent] );
	stack[--stack_top] = '\n';
	stack[--stack_top] = '"';
	for ( ; ent != NULL;
		ent = (ent >= 256 ? tab_prefix[ent] : NULL) ) {
	    if ( isascii(tab_suffix[ent]) && isprint(tab_suffix[ent]) )
		stack[--stack_top] = tab_suffix[ent];
	    else {
		switch( tab_suffix[ent] ) {
		case '\n': stack[--stack_top] = 'n'; break;
		case '\t': stack[--stack_top] = 't'; break;
		case '\b': stack[--stack_top] = 'b'; break;
		case '\f': stack[--stack_top] = 'f'; break;
		case '\r': stack[--stack_top] = 'r'; break;
		default:
		    stack[--stack_top] = '0' + tab_suffix[ent] % 8;
		    stack[--stack_top] = '0' + (tab_suffix[ent] / 8) % 8;
		    stack[--stack_top] = '0' + tab_suffix[ent] / 64;
		    break;
		}
		stack[--stack_top] = '\\';
	    }
	}
	fwrite( &stack[stack_top], 1, 4 * MAXSTACK - stack_top, stderr );
	stack_top = 4 * MAXSTACK;
    }
}
#endif DEBUG

/*****************************************************************
 * TAG( writeerr )
 * 
 * Exits with a message.  We only check for write errors often enough
 * to avoid a lot of "file system full" messages, not on every write.
 * ferror() check after fflush will catch any others (I trust).
 * 
 * Inputs:
 *	[None]
 * Outputs:
 *	[None]
 * Assumptions:
 *	[None]
 * Algorithm:
 *	[None]
 */

writeerr()
{
    perror( "goodbye, write error" );
    exit( 1 );
}

copystat(ifname, ofname)
char *ifname, *ofname;
{
    struct stat statbuf;
    int mode;
    time_t timep[2];

    fclose(stdout);
    if (stat(ifname, &statbuf)) {		/* Get stat on input file */
	perror(ifname);
	return;
    }
    if ((statbuf.st_mode & S_IFMT/*0170000*/) != S_IFREG/*0100000*/) {
	fprintf(stderr, " -- not a regular file: unchanged");
	exit_stat = 1;
    } else if (statbuf.st_nlink > 1) {
	fprintf(stderr, " -- has %d other links: unchanged",
		statbuf.st_nlink - 1);
	exit_stat = 1;
    } else if (exit_stat == 2) {	/* No compression: remove file.Z */
	fprintf(stderr, " -- file unchanged");
    } else {			/* ***** Successful Compression ***** */
	mode = statbuf.st_mode & 07777;
	if (chmod(ofname, mode))		/* Copy modes */
	    perror(ofname);
	chown(ofname, statbuf.st_uid, statbuf.st_gid);	/* Copy ownership */
	timep[0] = statbuf.st_atime;
	timep[1] = statbuf.st_mtime;
	utime(ofname, timep);	/* Update last accessed and modified times */
	if (unlink(ifname))	/* Remove input file */
	    perror(ifname);
	fprintf(stderr, " -- replaced with %s", ofname);
	return;		/* Successful return */
    }

    /* Unsuccessful return -- one of the tests failed */
    if (unlink(ofname))
	perror(ofname);
}
/*
 * This routine returns 1 if we are running in the foreground and stderr
 * is a tty.
 */
#include <signal.h>
foreground()
{
	register int (*tmp)();
	dummy();

	if((tmp = signal(SIGINT, dummy))) {	/* background? */
		signal(SIGINT, tmp);
		return(0);
	} else {			/* foreground */
		if(isatty(2)) {		/* and stderr is a tty */
			return(1);
		} else {
			return(0);
		}
	}
}
dummy()
{
	;
}
