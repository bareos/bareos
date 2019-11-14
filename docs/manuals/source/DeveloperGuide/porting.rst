Bareos Porting Notes
====================

This document is intended mostly for developers who wish to port Bareos
to a system that is not **officially** supported.

It is hoped that Bareos clients will eventually run on every imaginable
system that needs backing up. It is also hoped that the Bareos Directory
and Storage daemons will run on every system capable of supporting them.

Porting Requirements
--------------------

In General, the following holds true:

-  Your compiler must provide support for 64 bit signed and unsigned
   integers.

-  **Bareos** requires a good implementation of pthreads to work.

-  The source code has been written with portability in mind and is
   mostly POSIX compatible. Thus porting to any POSIX compatible
   operating system should be relatively easy.

