.TH UNPACK 1 local
.SH NAME
Unpack \- expand files 
.SH SYNOPSIS
.B Unpack
name .\|.\|.
.SH DESCRIPTION
.I Unpack\^
expands files created by
.IR Pack .
For each file
.I name\^
specified in the command, a search is made for a file called
.IB name .Z
(or just
.IR name ,
if
.I name\^
ends in
.BR .Z ).
This file is replaced by its expanded version by running compress(l) with
the '-d' option and copystat(l).
The new file has the
.B .Z
suffix stripped from its name, and has the same access modes,
access and modification dates, and owner as those of the packed file.
.PP
Failure may occur for the same reasons that it may in
.IR Pcat ,
as well as for the following:
.PP
.RS
a file with the ``unpacked'' name already exists;
.br
if the unpacked file cannot be created.
.PP
.RE
.SH EXAMPLE
.IP
Unpack file1
.PP
will replace the packed file "file1.Z" with its expanded version,
"file1".
.SH "SEE ALSO"
Pack(l), compress(l), Pact(l) 
