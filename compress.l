.PU
.TH COMPRESS 1 local
.SH NAME
compress  \-  compress files
.SH SYNOPSIS
.B compress
.B [\-d]
[filename]
.br
.B uncompress
[filename]
.SH DESCRIPTION
Compresses the specified file or standard input.
The result is written to standard output.
Compressed files can be restored
to their original form by specifying the
.B \-d
option, or by running
.I uncompress.
.PP
.I Compress
uses the modified Lempel-Ziv algorithm described in
"A Technique for High Performance Data Compression",
Terry A. Welch,
.I IEEE Computer
Vol 17, No 6 (June 1984), pp 8-19.
.PP
The amount of compression obtained depends on the size of the
input file, and the distribution of character sequences.
Typically, text files, such as C programs,
are reduced to 40\-50% of their original size;
.SH BUGS
Even if the file gets bigger after compression, the output is still produced.
.SH SEE ALSO
Pack(l), Pcat(l), Unpack(l)
