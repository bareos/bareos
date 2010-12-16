AC_PREREQ(2.65)
AC_INIT([libdroplet], [0.1.0])
AC_CONFIG_SRCDIR([libdroplet])
AC_CONFIG_MACRO_DIR([m4])

AM_CONFIG_HEADER(config.h)
m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([yes])])

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

LT_PATH_LD
LT_INIT
AC_HEADER_STDC
AC_PROG_INSTALL
AC_PROG_LN_S

PKG_CHECK_MODULES(LIBXML, libxml-2.0)
PKG_CHECK_MODULES(OPENSSL, openssl)
AC_CHECK_LIB(readline, rl_abort, [READLINE_LIBS=-lreadline], [AC_MSG_ERROR([readline library missing])])
AC_SUBST([READLINE_LIBS])
AC_CHECK_LIB(m, sqrt, [LIBM_LIBS=-lm], [AC_MSG_ERROR([libm library missing])])
AC_SUBST([LIBM_LIBS])
AX_PTHREAD

AC_CONFIG_FILES([
Makefile
examples/Makefile
libdroplet/Makefile
tests/Makefile
tools/Makefile
])
AC_OUTPUT

