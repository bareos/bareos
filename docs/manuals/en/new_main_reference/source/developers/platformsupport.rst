Platform Support
================

General
-------

This chapter describes the requirements for having a supported platform
(Operating System). In general, Bareos is quite portable. It supports 32
and 64 bit architectures as well as bigendian and littleendian machines.
For full support, the platform (Operating System) should implement POSIX
Unix system calls. However, for File daemon support only, a small
compatibility library can be written to support almost any architecture.

Requirements to become a Supported Platform
-------------------------------------------

As mentioned above, in order to become a fully supported platform, it
must support POSIX Unix system calls. In addition, the following
requirements must be met:
