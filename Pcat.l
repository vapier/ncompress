.TH PCAT 1 local
.SH NAME
Pcat \- expand files and write to standard output
.SH SYNOPSIS
.B Pcat
name .\|.\|.
.SH DESCRIPTION
.I Pcat\^
does for packed files what
.IR cat (1)
does for ordinary files.
The specified files are unpacked and written to the standard output.
Thus to view a packed file named
.IB name .Z
use:
.PP
.RS
pcat name.Z
.RE
or just:
.RS
pcat name
.PP
.RE
To create an unpacked copy, named
.IR nnn ,
of a packed file named
.IB name .Z
(without destroying
\f2name\^\fP\f3.Z\fP)
use the command:
.PP
.RS
Pcat name >nnn
.PP
.RE
Failure may occur if:
.PP
.RS
the file name (exclusive of the
.BR .Z )
has more than 12 characters;
.br
the file cannot be opened.
.RE
.SH EXAMPLE
.IP
Pcat file1
.PP
will display the packed file "file1.Z" for viewing in unpacked form.
