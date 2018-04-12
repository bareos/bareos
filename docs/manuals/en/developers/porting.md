Bareos Porting Notes
====================

This document is intended mostly for developers who wish to port Bareos
to a system that is not <span>**officially**</span> supported.

It is hoped that Bareos clients will eventually run on every imaginable
system that needs backing up. It is also hoped
that the Bareos Directory and Storage daemons will run on every system
capable of supporting them.

Porting Requirements
--------------------

In General, the following holds true:

-   Your compiler must provide support for 64 bit signed and unsigned
    integers.

-   You will need a recent copy of the <span>**autoconf**</span> tools
    loaded on your system (version 2.13 or later). The
    <span>**autoconf**</span> tools are used to build the configuration
    program, but are not part of the Bareos source distribution.

-   <span>**Bareos**</span> requires a good implementation of pthreads
    to work.

-   The source code has been written with portability in mind and is
    mostly POSIX compatible. Thus porting to any POSIX compatible
    operating system should be relatively easy.

Steps to Take for Porting
-------------------------

-   The first step is to ensure that you have version 2.13 or later of
    the <span>**autoconf**</span> tools loaded. You can skip this step,
    but making changes to the configuration program will be difficult or
    impossible.

-   The run a <span>**./configure**</span> command in the main source
    directory and examine the output. It should look something like the
    following:

        Configuration on Mon Oct 28 11:42:27 CET 2002:
          Host:                        i686-pc-linux-gnu -- redhat 7.3
          Bareos version:              1.27 (26 October 2002)
          Source code location:        .
          Install binaries:            /sbin
          Install config files:        /etc/bareos
          C Compiler:                  gcc
          C++ Compiler:                c++
          Compiler flags:              -g -O2
          Linker flags:
          Libraries:                   -lpthread
          Statically Linked Tools:     no
          Database found:              no
          Database type:               Internal
          Database lib:
          Job Output Email:            root@localhost
          Traceback Email:             root@localhost
          SMTP Host Address:           localhost
          Director Port                9101
          File daemon Port             9102
          Storage daemon Port          9103
          Working directory            /etc/bareos/working
          SQL binaries Directory
          Large file support:          yes
          readline support:            yes
          TCP Wrappers support:        no
          ZLIB support:                yes
          enable-smartalloc:           yes
          enable-gnome:                no
          gmp support:                 yes

    The details depend on your system. The first thing to check is that
    it properly identified your host on the <span>**Host:**</span> line.
    The first part (added in version 1.27) is the GNU four part
    identification of your system. The part after the – is your system
    and the system version. Generally, if your system is not yet
    supported, you must correct these.

-   If the <span>**./configure**</span> does not function properly, you
    must determine the cause and fix it. Generally, it will be because
    some required system routine is not available on your machine.

-   To correct problems with detection of your system type or with
    routines and libraries, you must edit the file
    <span>**<span>\<</span>bareos-src<span>\></span>/autoconf/configure.in**</span>.
    This is the “source” from which <span>**configure**</span> is built.
    In general, most of the changes for your system will be made in
    <span>**autoconf/aclocal.m4**</span> in the routine
    <span>**BA\_CHECK\_OPSYS**</span> or in the routine
    <span>**BA\_CHECK\_OPSYS\_DISTNAME**</span>. I have already added
    the necessary code for most systems, but if yours shows up as
    <span>**unknown**</span> you will need to make changes. Then as
    mentioned above, you will need to set a number of system dependent
    items in <span>**configure.in**</span> in the <span>**case**</span>
    statement at approximately line 1050 (depending on the Bareos
    release).

-   The items to in the case statement that corresponds to your system
    are the following:

    -   DISTVER – set to the version of your operating system. Typically
        some form of <span>**uname**</span> obtains it.

    -   TAPEDRIVE – the default tape drive. Not too important as the
        user can set it as an option.

    -   PSCMD – set to the <span>**ps**</span> command that will provide
        the PID in the first field and the program name in the second
        field. If this is not set properly, the <span>**bareos
        stop**</span> script will most likely not be able to stop Bareos
        in all cases.

    -   hostname – command to return the base host name (non-qualified)
        of your system. This is generally the machine name. Not too
        important as the user can correct this in his configuration
        file.

    -   CFLAGS – set any special compiler flags needed. Many systems
        need a special flag to make pthreads work. See cygwin for an
        example.

    -   LDFLAGS – set any special loader flags. See cygwin for an
        example.

    -   PTHREAD\_LIB – set for any special pthreads flags needed during
        linking. See freebsd as an example.

    -   lld – set so that a “long long int” will be properly edited in a
        printf() call.

    -   llu – set so that a “long long unsigned” will be properly edited
        in a printf() call.

    -   PFILES – set to add any files that you may define is your
        platform subdirectory. These files are used for installation of
        automatic system startup of Bareos daemons.

-   To rebuild a new version of <span>**configure**</span> from a
    changed <span>**autoconf/configure.in**</span> you enter
    <span>**make configure**</span> in the top level Bareos source
    directory. You must have done a ./configure prior to trying to
    rebuild the configure script or it will get into an infinite loop.

-   If the <span>**make configure**</span> gets into an infinite loop,
    ctl-c it, then do <span>**./configure**</span> (no options are
    necessary) and retry the <span>**make configure**</span>, which
    should now work.

-   To rebuild <span>**configure**</span> you will need to have
    <span>**autoconf**</span> version 2.57-3 or higher loaded. Older
    versions of autoconf will complain about unknown or bad options, and
    won’t work.

-   After you have a working <span>**configure**</span> script, you may
    need to make a few system dependent changes to the way Bareos works.
    Generally, these are done in <span>**src/baconfig.h**</span>. You
    can find a few examples of system dependent changes toward the end
    of this file. For example, on Irix systems, there is no definition
    for <span>**socklen\_t**</span>, so it is made in this file. If your
    system has structure alignment requirements, check the definition of
    BALIGN in this file. Currently, all Bareos allocated memory is
    aligned on a <span>**double**</span> boundary.

-   If you are having problems with Bareos’s type definitions, you might
    look at <span>**src/bc\_types.h**</span> where all the types such as
    <span>**uint32\_t**</span>, <span>**uint64\_t**</span>, etc. that
    Bareos uses are defined.


