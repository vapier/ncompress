.TH COPYSTAT 1 local
.SH NAME
copystat \- copy stat info from file
.SH SYNOPSIS
.B copystat
FromFile ToFile ...
.SH DESCRIPTION
The mode, owner/group, along with the accessed and updated times of
each named
.I ToFile
is changed
to be exactly the same as that of the
.I FromFile.
.PP
The point of this command is that when you transform a file using
one or more filters, you would very often like the transformed
file to have the same access permissions, owner/group, and dates as the 
original.  But as
the filters never know what files they are working on (and indeed,
if you use pipes, there is no place to carry this information
from one stage to the next), it is not possible for the filters to
do this, and they create the transformed file with the same default
that output redirection would use.  With this command you can copy
the permissions, owner/group, and dates of the original file to the 
transformed file.
.PP
Only the owner of a file (or the super-user) may change its mode.
Only the super-user may change owner/group.
.SH EXAMPLES
.IP
crypt key <cypher >clear; copystat cypher clear
.PP
This is the case that copystat was created for.  The decrypted
file "clear" gets the same permissions as the encrypted file "crypt".
In addition, the ownership and the dates are set as in the original file,
causing it not no be touched.
.IP
split -250 bigfile piece
.br
copystat bigfile piece*
.PP
Splits a file into pieces and ensures that the pieces have the
same permissions, owner, and dates as the original file.  (Note that these must
be separate lines for filename expansion to work.)
.SH "SEE ALSO"
chmod(1),
ls(1),
chmod(2),
chown(2),
stat(2),
umask(2),
utime(2)
.SH BUGS
This should be another option ("-f FromFile") in chmod(1).
