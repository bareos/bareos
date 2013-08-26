AC_PREREQ(2.59)
AC_INIT([libdroplet], [3.0.0])
AC_CONFIG_SRCDIR([libdroplet])
AC_CONFIG_MACRO_DIR([m4])

AM_CONFIG_HEADER(config.h)
m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([no])])

dnl to prevent error from autoreconf while crosscompiling
m4_pattern_allow(PKG_CONFIG_LIBDIR)

dnl libtool versioning
# +1 : 0 : +1  == adds new functions to the interface
# +1 : 0 : 0   == changes or removes functions (changes include both
#                 changes to the signature and the semantic)
#  ? :+1 : ?   == just internal changes
# CURRENT : REVISION : AGE
LIBDROPLET_SO_VERSION=0:0:0
AC_SUBST(LIBDROPLET_SO_VERSION)

AM_INIT_AUTOMAKE([dist-bzip2 no-dist-gzip subdir-objects])

AC_PROG_CC
AM_PROG_CC_C_O

AC_PROG_LIBTOOL
AC_HEADER_STDC
AC_PROG_INSTALL
AC_PROG_LN_S

AC_PROG_YACC

PKG_CHECK_MODULES(LIBXML, libxml-2.0)
PKG_CHECK_MODULES(OPENSSL, openssl)
PKG_CHECK_MODULES(JSON, json)
AC_SEARCH_LIBS([sqrt], [m], [], [AC_MSG_ERROR([libm library missing])])
AX_PTHREAD

AC_CONFIG_FILES([
Makefile
examples/Makefile
libdroplet/Makefile
droplet-3.0.pc
libdroplet.spec
])
AC_OUTPUT

