#define STATS
#undef STATS
#define DEBUG
#undef DEBUG
/* 
 * compress.c - File compression ala IEEE Computer June 1984.
 * 
 * Author:	Spencer W. Thomas
 * 		Computer Science Dept.
 * 		University of Utah
 * Date:	Wed Jul  4 1984
 * Copyright (c) 1984 Spencer W. Thomas
 * 
 * WARNING: due to cc -O bug (dealing with ext instruction), do not
 *	compile this program with -O (4.2bsd, at least).
 * 
 * $Header: compress.c,v 1.6 84/08/01 22:08:00 joe Exp $
 * $Log:	compress.c,v $
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
static char rcs_ident[] = "$Header: compress.c,v 1.6 84/08/01 22:08:00 joe Exp $";

#include "stdio.h"
#include "ctype.h"

#define	BITS	16			/* maximum number of bits/code */
int maxbits = BITS;			/* user settable max # bits/code */
long int maxmaxcode = 1 << BITS;
#define INIT_BITS 9
int n_bits = INIT_BITS;			/* initial number of bits/code */
long int maxcode = 1 << INIT_BITS - 1;	/* 1 << n_bits - 1 */
long int bytes_out = 0;			/* count how many are written */

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

long int getcode();

#ifdef DEBUG
int debug = 0;
#endif

#define NO_SCANARGS
/*****************************************************************
 * TAG( main )
 * 
 * Algorithm from "A Technique for High Performance Data Compression",
 * Terry A. Welch, IEEE Computer Vol 17, No 6 (June 1984), pp 8-19.
 * 
 * Usage: compress [-d] [file]
 * Inputs:
 *	-d:	    If given, decompression is done instead.
 * 
 * 	file:	    File to be compressed.  If none specified, stdin
 *		    is used.
 * Outputs:
 * 	stdout:	    Compressed form of file.
 * 
 * Assumptions:
 * 	File is better off compressed.  Goes ahead and compresses even
 * if the result is bigger.
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
    register FILE * infile;
    register char * fname = NULL;
    int decomp_flg = 0;
#ifdef DEBUG
    int verbose = 0;
#endif
	extern int optind;
	extern char *optarg;
	register c;

#ifdef BSD4.2
    /* 4.2BSD dependent - take it out if not */
    setlinebuf( stderr );
#endif
#ifndef NO_SCANARGS
    if ( scanargs( argc, argv, "compress D%- d%- v%- b%-maxbits!d file%s",
		    &debug, &decomp_flg, &verbose,
		    &maxbits, &maxbits, &fname ) == 0 )
	exit( 1 );
#else
    /* 
     * Roll your own here using getopt or whatever.  All flags are
     * optional.
     * -D => debug
     * -d => decomp_flg
     * -v => verbose
     * -b maxbits => maxbits.  If -b is specified, then maxbits MUST be
     *	    given also.
     * if a string is left, must be filename.
     */
#ifdef DEBUG
	while ((c=getopt(argc, argv, "Dvdb:")) != EOF)
#else
	while ((c=getopt(argc, argv, "db:")) != EOF)
#endif
		switch (c) {
#if DEBUG
		case 'D': debug = 1; break;
		case 'v': verbose = 1; break;
#endif
		case 'd': decomp_flg = 1; break;
		case 'b':
			maxbits = atoi(optarg);
			if (maxbits > 0)
				break;
		case '?':
#ifdef DEBUG
			fprintf(stderr,"usage: %s [-Ddv -b maxbits] [fname]\n",argv[0]);
#else
			fprintf(stderr,"usage: %s [-d] [-b maxbits] [fname]\n",argv[0]);
#endif
			exit( 1 );
		}
#if 0
	fprintf(stderr,"argc = %d optind = %d   %s\n", argc, optind, argv[optind]);
#endif
	if (argc > 1 && optind < argc)
		fname = argv[optind];
#endif

    if ( maxbits > BITS )
	maxbits = BITS;
    maxmaxcode = 1 << maxbits;

    if ( fname != NULL )
    {
	if ( ( infile = fopen( fname, "r" ) ) == NULL )
	{
	    perror( fname );
	    exit( 1 );
	}
    }
    else
	infile = stdin;

    if ( decomp_flg == 0 )
	compress( infile );
    else
#ifdef DEBUG
	if ( debug == 0 )
#endif
	decompress( infile );
#ifdef DEBUG
    else
    {
	/* 
	 * Just print out codes from input file.  Mostly for debugging.
	 */
	long int code;
	int col = 0, bits = n_bits;

	free_ent = 255;
	while ( ( code = getcode( infile ) ) >= 0 )
	{
	    if ( free_ent < maxmaxcode )
		free_ent++;
	    if ( bits != n_bits )
	    {
		printf( "\nChange to %d bits\n", n_bits );
		bits = n_bits;
		col = 0;
	    }
	    printf( "%5d%c", code, (col+=6) >= 74 ? (col = 0, '\n') : ' ' );
	}
	putchar( '\n' );
	exit( 0 );
    }

    /* If requested (verbose), dump string table */
    if ( verbose )
	dump_tab();
#endif
}


/*****************************************************************
 * TAG( compress )
 * 
 * Actually does the compression.
 * Inputs:
 * 	infile:	    File to compress.
 * Outputs:
 * 	Writes compressed file to stdout.
 * Assumptions:
 *	[None]
 * Algorithm:
 * 	See above.
 */

compress( infile )
register FILE *infile;
{
    register c;
    register int ent, n_ent;
    register long int in_count = 1;
#ifdef STATS
    int out_count = 0;
#endif

    /* 
     * Initialize the compression table to contain all 8-bit values
     * initially.  These don't need to be chained, they can be looked
     * up directly.
     */
    for ( ent = 0; ent < 256; ent++ )
    {
	tab_next[ent] = tab_chain[ent] = NULL;
	tab_suffix[ent] = ent;
    }

    free_ent = 256;

    ent = getc( infile );		/* initial entry */
    while ( !feof( infile ) && (c = getc(infile)) != (unsigned)EOF )
    {
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
#endif
	    if ( (n_ent=free_ent) < maxmaxcode )
	    {
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
#endif
    output( -1L );			/* finish up output if necessary */

    /* 
     * Print out stats on stderr
     */
#ifdef STATS
    fprintf( stderr,
	"%ld chars in, %ld codes (%ld bytes) out, compression factor %g\n",
		in_count, out_count, bytes_out,
		(double)in_count / (double)bytes_out );
    fprintf( stderr, "\tCompression as in compact: %5.2f%%\n",
		100.0 * ( in_count - bytes_out ) / (double) in_count );
    fprintf( stderr, "\tLargest code was %d (%d bits)\n", free_ent - 1, n_bits );
#else
    fprintf( stderr, "Compression: %5.2f%%\n",
		100.0 * ( in_count - bytes_out ) / (double) in_count );
#endif
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

#if vax
    static char buf[BITS];
#else
    static int buf[(BITS+sizeof(int)-sizeof(char))/sizeof(int)];
#endif
    static int offset = 0;

output( code )
long int  code;
{
#ifdef DEBUG
    static int col = 0;
#endif

#ifdef vax
    /* 
     * On the VAX, it is important to have the register declarations
     * in exactly the order given, or the asm will break.
     */
    register int r_off = offset, bits= n_bits;
    register char * bp = buf;
#endif

    if ( code >= 0 )
    {
#ifdef vax
	/* VAX DEPENDENT!! Implementation on other machines may be
	 * difficult.
	 * 
	 * Translation: Insert BITS bits from the argument starting at
	 * offset bits from the beginning of buf.
	 */
	asm( "insv	4(ap),r11,r10,(r9)" );
#else
	insert_bit(code);
#endif
	offset += n_bits;
	if ( offset == (n_bits << 3) )
	{
	    fwrite( buf, 1, n_bits, stdout );
	    offset = 0;
	    bytes_out += n_bits;
	}
#ifdef DEBUG
	if ( debug )
	    fprintf( stderr, "%5d%c", code,
		    (col+=6) >= 74 ? (col = 0, '\n') : ' ' );
#endif

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
		fwrite( buf, 1, n_bits, stdout );
		bytes_out += n_bits;
	    }
	    offset = 0;
		
	    n_bits++;
	    if ( n_bits == maxbits )
		maxcode = maxmaxcode;
	    else
		maxcode = (1 << n_bits) - 1;
#ifdef DEBUG
	    if ( debug )
	    {
		fprintf( stderr, "\nChange to %d bits\n", n_bits );
		col = 0;
	    }
#endif
	}
    }
    else
    {
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
#endif
    }
}


/*****************************************************************
 * TAG( decompress )
 * 
 * Decompress the input file.
 * 
 * Inputs:
 * 	infile:	    File to be decompressed.
 * Outputs:
 * 	Writes decompressed file to stdout.
 * Assumptions:
 * 	Input file was created with compress (could use a magic #, I
 *	guess).
 * Algorithm:
 * 	See article cited above.
 */

decompress( infile )
register FILE *infile;
{
    register int stack_top = MAXSTACK;
    register long int code, oldcode, incode;
    register int finchar;
    char stack[MAXSTACK];

    /* 
     * As above, initialize the first 256 entries in the table.  
     */
    for ( free_ent = 0; free_ent < 256; free_ent++ )
    {
	tab_next[free_ent] = tab_chain[free_ent] = NULL;
	tab_prefix[free_ent] = 0;
	tab_suffix[free_ent] = free_ent;
    }

    finchar = oldcode = getcode( infile );
    putchar( (char)finchar );		/* first code must be 8 bits = char */

    while ( ( code = getcode( infile ) ) != -1 )
    {
	incode = code;
	/* 
	 * Special case for KwKwK string.
	 */
	if ( code >= free_ent )
	{
	    stack[--stack_top] = finchar;
	    code = oldcode;
	}

	/* 
	 * Generate output characters in reverse order
	 */
	while ( ((unsigned long)code) >= ((unsigned long)256) )
	{
	    stack[--stack_top] = tab_suffix[code];
	    code = tab_prefix[code];
	}
	stack[--stack_top] = finchar = tab_suffix[code];

	/* 
	 * And put them out in forward order
	 */
	fwrite( &stack[stack_top], 1, MAXSTACK - stack_top, stdout );
	stack_top = MAXSTACK;

	/* 
	 * Generate the new entry.
	 */
	if ( (code=free_ent) < maxmaxcode )
	{
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
}


/*****************************************************************
 * TAG( getcode )
 * 
 * Read one code from the input file.  If EOF, return -1.
 * Inputs:
 * 	infile:	    Input file.
 * Outputs:
 * 	code or -1 is returned.
 * Assumptions:
 *	[None]
 * Algorithm:
 * 	For now, use scanf to read ascii input.
 */

long int
getcode( infile )
FILE *infile;
{
#ifdef vax
    /* 
     * On the VAX, it is important to have the register declarations
     * in exactly the order given, or the asm will break.
     */
    register long int code;
    static int offset = 0, size = 0;
    static char buf[BITS];
    register int r_off, bits;
    register char * bp = buf;
#else
    register long code;
    static int size = 0;
#endif

    if ( offset >= size || free_ent > maxcode )
    {
	/* 
	 * If the next entry will be too big for the current code
	 * size, then we must increase the size.  This implies reading
	 * a new buffer full, too.
	 */
	if ( free_ent > maxcode )
	{
	    n_bits++;
	    if ( n_bits == maxbits )
		maxcode = maxmaxcode;	/* won't get any bigger now */
	    else
		maxcode = (1 << n_bits) - 1;
	}
	size = fread( buf, 1, n_bits, infile );
	if ( size <= 0 )
	    return -1;			/* end of file */
	offset = 0;
	/* Round size down to integral number of codes */
	size = (size << 3) - (n_bits - 1);
    }
#ifdef vax
    /* 
     * Get it into a register for the following VAX dependent code.
     */
    r_off = offset;
    bits = n_bits;
    asm( "extzv   r10,r9,(r8),r11" );
#else
    code = fetch();
#endif
    offset += n_bits;

    return code;
}

#ifdef DEBUG

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

    for ( i = 0; i < free_ent; i++ )
    {
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
		ent = (ent >= 256 ? tab_prefix[ent] : NULL) )
	{
	    if ( isascii(tab_suffix[ent]) && isprint(tab_suffix[ent]) )
		stack[--stack_top] = tab_suffix[ent];
	    else
	    {
		switch( tab_suffix[ent] )
		{
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
#endif

static unsigned sizemask[] = { 0,
	0x00000001, 0x00000003, 0x00000007, 0x0000000F,
	0x0000001F, 0x0000003F, 0x0000007F, 0x000000FF,
	0x000001FF, 0x000003FF, 0x000007FF, 0x00000FFF,
	0x00001FFF, 0x00003FFF, 0x00007FFF, 0x0000FFFF,
	0x0001FFFF
	};

#undef DEBUG_INSERT

#define base buf

/* insert a value of "size" bits at "offset" bits from base */

insert_bit(value) register long int value; {

	register offs = offset;
	register size = n_bits;
	register  word, shift;

	word = offs >> 5;
	offs &= 31;

	if (((unsigned)offs + size) <= ((unsigned)32)) {
		shift = (32 - size) - offs;

#ifdef DEBUG_INSERT
fprintf(stderr,"offs=%2d size=%d value=%2x shift=%2d oword=%x",offs, size, value, shift, base[word]);
#endif
		base[word] = (base[word] & ~(sizemask[size] << shift)) |
				(value  << shift);
#ifdef DEBUG_INSERT
fprintf(stderr," nword=%x\n", base[word]);
#endif

	} else {

		register size1 = 32 - offs;

#ifdef DEBUG_INSERT
fprintf(stderr,"offs=%d size=%d value=%x oword1=%x",offs, size, value, base[word]);
#endif
		base[word] = (base[word] & ~sizemask[size1]) |
				((unsigned)value >> (size-size1));
		size -= size1;
		shift = 32 - size;
#ifdef DEBUG_INSERT
fprintf(stderr," nword1=%x oword2=%x", base[word], base[word+1]);
#endif
		base[word+1] = (base[word+1] & ~(sizemask[size] << shift)) |
				     (value << shift);
#ifdef DEBUG_INSERT
fprintf(stderr," nword2=%x\n", base[word+1]);
#endif
	}
}

#define DEBUG_FETCH
#undef DEBUG_FETCH

fetch() {

	register offs = offset;
	register size = n_bits;
	register word;

#ifdef slower
	word = offs / 32;
	offs %= 32;
#else
	word = offs >> 5;
	offs &= 31;
#endif
	if (((unsigned)offs + size) <= ((unsigned)32)) {

#ifdef DEBUG_FETCH
fprintf(stderr,"offs=%2d size=%d word=%x return=%x\n",offs, size, base[word], ((unsigned)base[word] >> ((32-size) - offs)) & sizemask[size]);
#endif
		return( ((unsigned)base[word] >> ((32-size) - offs)) & sizemask[size]);

	} else {
		register size2 = size - (32 - offs);

#ifdef DEBUG_FETCH
fprintf(stderr,"offs=%2d size=%d size2=%d word1=%x word2=%x return=%x\n", offs, size, size2, base[word], base[word+1], (((base[word] << size2) | ((unsigned)base[word+1] >> (32-size2))) & sizemask[size]) );
#endif
		return( ((base[word] << size2) | ((unsigned)base[word+1] >> (32-size2))) & sizemask[size]);
	}
}

