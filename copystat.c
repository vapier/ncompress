/*  File   : copymode.c
    Author : Richard A. O'Keefe & Joseph M. Orost
    Updated: 28 April 1984

    copystat FromFile ToFile1 ... ToFilen

    copies the mode (access permission) bits, the dates, and the owner (if
    possible)  from FromFile to ToFile1 ... ToFilen.  The exit code is
    0) all went well
    1) there were too few parameters
    2) stat(2) wasn't happy with FromFile
    3) chmod(2) wasn't happy with one or more of the ToFiles.
    All the ToFiles which can be changed will be changed, even
    if a file earlier in the list cannot be.  E.g. suppose
	copystat from a		# would succeed
	copystat from b		# would fail
	copystat from c		# would succeed
    then
	copystat from a b c	# will change a and c and will fail.

    This program is for use with filters.  A filter cannot make its
    output have the same protection as its input, because it has no
    access to the files as such, only to their contents.  Given a
    filter, to make something which preserves permissions, define
    Filter =
	a=$1
	b=$2
	shift;shift
	filter <$a >$b $*
	copystat $a $b
*/

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>

main(argc, argv)
    int argc;
    register char **argv;
    {
        struct stat statbuf;
	int mode;
	int errs = 0;

	if (argc < 3) {
	    fprintf(stderr, "Usage: copystat From To...\n");
	    exit(1);
	}
	if (stat(argv[1], &statbuf)) {
	    perror(argv[1]);
	    exit(2);
	}
	mode = statbuf.st_mode & 07777;
	argv++;
	while (*++argv) {
	    if (chmod(*argv, mode)) {
		perror(*argv);
		errs++;
	    }
	    chown(*argv, statbuf.st_uid, statbuf.st_gid);
	    utime(*argv, &statbuf.st_atime);
	}
	exit(errs ? 3 : 0);
    }
