.TH PACK 1 local
.SH NAME
Pack \- compress files
.SH SYNOPSIS
.B Pack
name .\|.\|.
.SH DESCRIPTION
.I Pack\^
attempts to store the specified files in a compressed form.
Wherever possible (and useful), each input file
.I name\^
is replaced by a packed file
.IB name .Z
with the same access modes, access and modified dates, and owner as those of
.IR name .
This is performed by running compress(l) and copystat(l) on each file.
If
.I Pack\^
is successful,
.I name\^
will be removed.
Packed files
can be restored to their original form using
.IR Unpack (l)
or
.IR Pcat (l).
.PP
The amount of compression obtained depends on the size of the
input file and the common substring frequency distribution.
.PP
No packing will occur if:
.PP
.RS
the filename ends in .Z;
.br
the file name has more than 12 characters;
.br
the file has links;
.br
the file is a directory;
.br
the file cannot be opened;
.br
no disk storage blocks will be saved by packing;
.br
a file called
.IB name .Z
already exists;
.br
the
.B .Z
file cannot be created;
.br
an I/O error occurred during processing.
.PP
.RE
The last segment of the file name must contain no more than 12
characters to allow space for the appended
.B .Z
extension.
Directories cannot be compressed.
.SH EXAMPLE
.IP
Pack file1
.PP
will pack file "file1" into "file1.Z" and removes "file1"
if packing is successful.
.SH "SEE ALSO"
Unpack(l), compress(l), Pcat(l)
