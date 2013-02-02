In order to use the win32 bareos regression scripts, it is important to have
some unix tools (such as sed, grep, and diff).  To make things simple, download
UnxUtils from http://sourceforge.net/projects/unxutils

Extract UnxUtils somewhere and add the the files in usr\local\wbin to $PATH.

Copy regress/win32 to a local directory on your system.

Set your sources directory in prototype.conf to a mapped drive or a local copy
of the bareos sources including windows binaries.

Run "config.cmd prototype.conf" from a command prompt in your regress/win32
directory followed by "make setup".

Tests are executed with "make test".
