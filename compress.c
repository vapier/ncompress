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
 * $Header: compress.c,v 1.4 84/07/05 03:11:11 thomas Exp $
 * $Log:	compress.c,v $
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
static char rcs_ident[] = "$Header: compress.c,v 1.4 84/07/05 03:11:11 thomas Exp $";

#include "stdio.h"
#include "ctype.h"

#define	BITS	16			/* maximum number of bits/code */
int maxbits = BITS;			/* user settable max # bits/code */
long int maxmaxcode = 1 << BITS;
int n_bits = 9;				/* initial number of bits/code */
long int maxcode = 511;			/* 1 << n_bits - 1 */
long int bytes_out = 0;			/* count how many are written */

/* 
 * One code could conceivably represent (1<<BITS) characters, but
 * to get a code of length N requires an input string of at least
 * N*(N-1)/2 characters.  With 10000 chars in the stack, an input
 * file would have to contain a 50Mb string of a single character.
 * This seems unlikely.
 */
#define MAXSTACK    10000		/* size of output stack */

struct c_ent {
    struct c_ent * next,		/* chain of entries with same prefix */
		 * chain;		/* chain prefixed with this entry */
    unsigned int prefix : BITS,		/* prefix code for this entry */
		 code : BITS;		/* code for this entry */
    unsigned char suffix;		/* last char in this entry */
} c_tab[1<<BITS];			/* a table full of them */
long int free_ent = 0;			/* first unused entry */

long int getcode();

int debug = 0;

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
char **argv;
{
    FILE * infile;
    char * fname = NULL;
    int decompress_flg = 0;
    int verbose = 0;

    /* 4.2BSD dependent - take it out if not */
    setlinebuf( stderr );
#ifndef NO_SCANARGS
    if ( scanargs( argc, argv, "compress D%- d%- v%- b%-maxbits!d file%s",
		    &debug, &decompress_flg, &verbose,
		    &maxbits, &maxbits, &fname ) == 0 )
	exit( 1 );
#else
    /* 
     * Roll your own here using getopt or whatever.  All flags are
     * optional.
     * -D => debug
     * -d => decompress_flg
     * -v => verbose
     * -b maxbits => maxbits.  If -b is specified, then maxbits MUST be
     *	    given also.
     * if a string is left, must be filename.
     */
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

    if ( decompress_flg == 0 )
	compress( infile );
    else if ( debug == 0 )
	decompress( infile );
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
FILE *infile;
{
    int ic;
    unsigned char c;
    register struct c_ent * ent, * n_ent;
    long int in_count = 1;
#ifdef STATS
    int out_count = 0;
#endif

    /* 
     * Initialize the compression table to contain all 8-bit values
     * initially.  These don't need to be chained, they can be looked
     * up directly.
     */
    for ( free_ent = 0, ent = c_tab; free_ent < 256; free_ent++, ent++ )
    {
	ent->next = ent->chain = NULL;
	ent->prefix = 0;
	ent->code = free_ent;
	ent->suffix = free_ent;
    }

    c = getc( infile );
    ent = &c_tab[c];			/* initial entry */
    while ( !feof( infile ) && (ic = getc(infile)) != EOF )
    {
	in_count++;
	c = ic;
	/* 
	 * Find the entry corresponding to the current entry suffixed
	 * with this char.
	 */
	for ( n_ent = ent->chain; n_ent != NULL; n_ent = n_ent->next )
	    if ( n_ent->suffix == c )
		break;			/* found it */

	/* 
	 * If no such entry, do 2 things:
	 * 1. Put out code for current prefix string.
	 * 2. Add the new string to the table.
	 *    If the table is full, just start over with current input
	 *    character.
	 */
	if ( n_ent == NULL )
	{
	    output( (long int)ent->code );
#ifdef STATS
	    out_count++;
#endif
	    if ( free_ent < maxmaxcode )
	    {
		n_ent = &c_tab[free_ent];
		n_ent->chain = NULL;
		n_ent->prefix = ent->code;
		n_ent->suffix = c;
		n_ent->code = free_ent;
		n_ent->next = ent->chain;
		ent->chain = n_ent;
		free_ent++;
	    }
	    n_ent = &c_tab[c];
	}
	ent = n_ent;
    }
    /* 
     * Put out the final code.
     */
    output( (long int)ent->code );
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
    fprintf( stderr, "\tCompression: %5.2f%%\n",
		100.0 * ( in_count - bytes_out ) / (double) in_count );
#endif
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

output( code )
long int  code;
{
    static char buf[BITS];
    static int offset = 0;
    static int col = 0;
    /* 
     * On the VAX, it is important to have the register declarations
     * in exactly the order given, or the asm will break.
     */
    register int r_off = offset, bits= n_bits;
    register char * bp = buf;

    if ( code >= 0 )
    {
	/* VAX DEPENDENT!! Implementation on other machines may be
	 * difficult.
	 * 
	 * Translation: Insert BITS bits from the argument starting at
	 * offset bits from the beginning of buf.
	 */
#ifndef vax
	You lose
#endif
	asm( "insv	4(ap),r11,r10,(r9)" );
	offset += n_bits;
	if ( offset == n_bits*8 )
	{
	    fwrite( buf, 1, n_bits, stdout );
	    offset = 0;
	    bytes_out += n_bits;
	}
	if ( debug )
	    fprintf( stderr, "%5d%c", code,
		    (col+=6) >= 74 ? (col = 0, '\n') : ' ' );

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
	    if ( debug )
	    {
		fprintf( stderr, "\nChange to %d bits\n", n_bits );
		col = 0;
	    }
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
	if ( debug )
	    fprintf( stderr, "\n" );
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
FILE *infile;
{
    unsigned char stack[MAXSTACK];
    int stack_top = MAXSTACK;
    long int code, oldcode, incode;
    unsigned char finchar;
    register struct c_ent * ent;

    /* 
     * As above, initialize the first 256 entries in the table.  
     */
    for ( free_ent = 0; free_ent < 256; free_ent++ )
    {
	ent = &c_tab[free_ent];
	ent->next = ent->chain = NULL;
	ent->prefix = 0;
	ent->code = free_ent;
	ent->suffix = free_ent;
    }

    code = oldcode = getcode( infile );
    putchar( (char)code );		/* first code must be 8 bits = char */
    finchar = code;

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
	ent = &c_tab[code];
	while ( ent->code >= 256 )
	{
	    stack[--stack_top] = ent->suffix;
	    ent = &c_tab[ent->prefix];
	}
	stack[--stack_top] = finchar = ent->suffix;

	/* 
	 * And put them out in forward order
	 */
	fwrite( &stack[stack_top], 1, MAXSTACK - stack_top, stdout );
	stack_top = MAXSTACK;

	/* 
	 * Generate the new entry.
	 */
	if ( free_ent < maxmaxcode )
	{
	    ent = &c_tab[free_ent];
	    ent->prefix = oldcode;
	    ent->suffix = finchar;
	    ent->code = free_ent;
	    free_ent++;
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
    /* 
     * On the VAX, it is important to have the register declarations
     * in exactly the order given, or the asm will break.
     */
    register long int code;
    static int offset = 0, size = 0;
    static char buf[BITS];
    register int r_off, bits;
    register char * bp = buf;

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
	size = size * 8 - (n_bits - 1);
    }
    /* 
     * Get it into a register for the following VAX dependent code.
     */
    r_off = offset;
    bits = n_bits;
#ifndef vax
    You lose
#endif
    asm( "extzv   r10,r9,(r8),r11" );
    offset += n_bits;

    return code;
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
    register struct c_ent * ent;
    char stack[4 * MAXSTACK];	/* \nnn makes it 4 times bigger */
    int stack_top = 4 * MAXSTACK;

    for ( i = 0; i < free_ent; i++ )
    {
	ent = &c_tab[i];
	if ( isascii(ent->suffix) && isprint(ent->suffix) )
	    fprintf( stderr, "%5d: %5d/'%c'  \"",
			ent->code, ent->prefix, ent->suffix );
	else
	    fprintf( stderr, "%5d: %5d/\\%03o \"",
			ent->code, ent->prefix, ent->suffix );
	stack[--stack_top] = '\n';
	stack[--stack_top] = '"';
	for ( ; ent != NULL;
		ent = (ent->code >= 256 ? &c_tab[ent->prefix] : NULL) )
	{
	    if ( isascii(ent->suffix) && isprint(ent->suffix) )
		stack[--stack_top] = ent->suffix;
	    else
	    {
		switch( ent->suffix )
		{
		case '\n': stack[--stack_top] = 'n'; break;
		case '\t': stack[--stack_top] = 't'; break;
		case '\b': stack[--stack_top] = 'b'; break;
		case '\f': stack[--stack_top] = 'f'; break;
		case '\r': stack[--stack_top] = 'r'; break;
		default:
		    stack[--stack_top] = '0' + ent->suffix % 8;
		    stack[--stack_top] = '0' + (ent->suffix / 8) % 8;
		    stack[--stack_top] = '0' + ent->suffix / 64;
		    break;
		}
		stack[--stack_top] = '\\';
	    }
	}
	fwrite( &stack[stack_top], 1, 4 * MAXSTACK - stack_top, stderr );
	stack_top = 4 * MAXSTACK;
    }
}
