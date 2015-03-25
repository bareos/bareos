Platform Support {#_PlatformChapter}
================

General
-------

This chapter describes the requirements for having a supported platform
(Operating System). In general, Bacula is quite portable. It supports 32
and 64 bit architectures as well as bigendian and littleendian machines.
For full support, the platform (Operating System) must implement POSIX
Unix system calls. However, for File daemon support only, a small
compatibility library can be written to support almost any architecture.

Currently Linux, FreeBSD, and Solaris are fully supported platforms,
which means that the code has been tested on those machines and passes a
full set of regression tests.

In addition, the Windows File daemon is supported on most versions of
Windows, and finally, there are a number of other platforms where the
File daemon (client) is known to run: NetBSD, OpenBSD, Mac OSX, SGI, ...

Requirements to become a Supported Platform
-------------------------------------------

As mentioned above, in order to become a fully supported platform, it
must support POSIX Unix system calls. In addition, the following
requirements must be met:

-   The principal developer (currently Kern) must have non-root ssh
    access to a test machine running the platform.

-   The ideal requirements and minimum requirements for this machine are
    given below.

-   There must be a defined platform champion who is normally a system
    administrator for the machine that is available. This person need
    not be a developer/programmer but must be familiar with system
    administration of the platform.

-   There must be at least one person designated who will run regression
    tests prior to each release. Releases occur approximately once every
    6 months, but can be more frequent. It takes at most a day’s effort
    to setup the regression scripts in the beginning, and after that,
    they can either be run daily or on demand before a release. Running
    the regression scripts involves only one or two command line
    commands and is fully automated.

-   Ideally there are one or more persons who will package each Bacula
    release.

-   Ideally there are one or more developers who can respond to and fix
    platform specific bugs.

Ideal requirements for a test machine:

-   The principal developer will have non-root ssh access to the test
    machine at all times.

-   The pricipal developer will have a root password.

-   The test machine will provide approximately 200 MB of disk space for
    continual use.

-   The test machine will have approximately 500 MB of free disk space
    for temporary use.

-   The test machine will run the most common version of the OS.

-   The test machine will have an autochanger of DDS-4 technology or
    later having two or more tapes.

-   The test machine will have MySQL and/or PostgreSQL database access
    for account “bacula” available.

-   The test machine will have sftp access.

-   The test machine will provide an smtp server.

Minimum requirements for a test machine:

-   The principal developer will have non-root ssh access to the test
    machine when requested approximately once a month.

-   The pricipal developer not have root access.

-   The test machine will provide approximately 80 MB of disk space for
    continual use.

-   The test machine will have approximately 300 MB of free disk space
    for temporary use.

-   The test machine will run the the OS.

-   The test machine will have a tape drive of DDS-4 technology or later
    that can be scheduled for access.

-   The test machine will not have MySQL and/or PostgreSQL database
    access.

-   The test machine will have no sftp access.

-   The test machine will provide no email access.

Bare bones test machine requirements:

-   The test machine is available only to a designated test person (your
    own machine).

-   The designated test person runs the regession tests on demand.

-   The test machine has a tape drive available.


