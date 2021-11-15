AC_PREREQ(2.59)
AC_INIT([libdroplet], [3.0.0])
AC_CONFIG_SRCDIR([libdroplet])
AC_CONFIG_MACRO_DIR([m4])

AM_CONFIG_HEADER(config.h)
m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([yes])])

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

PKG_CHECK_MODULES(CHECK, check >= 0.9.4)
dnl AM_PATH_CHECK(0.9.3) dnl Deprecated, but older systems may need this instead of the above

PKG_CHECK_MODULES(LIBXML, libxml-2.0)
PKG_CHECK_MODULES(OPENSSL, openssl)

AC_ARG_WITH([json-c-include-dir],
    [AS_HELP_STRING([--with-json-c-include-dir=<dirpath>], [Specify json-c include dir])
], [json_c_include_dir="$withval"])

AC_ARG_WITH([json-c-lib],
    [AS_HELP_STRING([--with-json-c-lib=<libpath>], [Specify linker cmd to link with json-c lib])
], [json_c_lib="$withval"])

AS_IF(
  [test "x$json_c_include_dir" = "x" -a "x$json_c_lib" = "x"],
    [PKG_CHECK_MODULES([JSON], [json-c],
      [AC_SUBST([JSON_PKG_NAME], [json-c])],
      [PKG_CHECK_MODULES([JSON], [json],
          [AC_SUBST([JSON_PKG_NAME], [json])]
      )]
    )],

  AS_IF(
    [test "x$json_c_include_dir" != "x" -a "x$json_c_lib"  = "x" -o \
          "x$json_c_include_dir"  = "x" -a "x$json_c_lib" != "x"],
      [AC_MSG_ERROR([Cannot not use just one option --with-json-c-include-dir or --with-json-c-lib])],

    [AC_SUBST([JSON_CFLAGS],[-I"$json_c_include_dir"])
     AC_SUBST([JSON_LIBS],[$json_c_lib])]
  )
)

AC_MSG_NOTICE("Using JSON cflags: $JSON_CFLAGS")
AC_MSG_NOTICE("Using JSON libs: $JSON_LIBS")

AC_SEARCH_LIBS([sqrt], [m], [], [AC_MSG_ERROR([libm library missing])])
AX_PTHREAD

AC_CHECK_HEADER([attr/xattr.h],[],AC_MSG_ERROR([libattr1 missing]))

AC_ARG_ENABLE(coverage, AS_HELP_STRING([--enable-coverage],
	     [Build with test coverage instrumentation]))
if test "$enable_coverage" = yes ; then
    if test "$GCC" = yes ; then
	AC_MSG_NOTICE([Adding --coverage to CFLAGS because --enable-coverage given]);
	CFLAGS="$CFLAGS --coverage"
	AC_MSG_NOTICE([Removing optimisation flags from CFLAGS because --enable-coverage given]);
	dnl autoconf uses [] as the m4 quoting characters
	dnl but that makes it really hard to build a regexp
	dnl so shove that aside temporarily
	changequote(%,%)
	CFLAGS=`echo " $CFLAGS " | sed -e 's/[ \t]-O[0-9]*[ \t]/ /g'`
	changequote([,])
    else
	AC_MSG_WARN([Compiler is not gcc, ignoring --enable-coverage]);
	enable_coverage=no
    fi
fi
AM_CONDITIONAL(COVERAGE,[test "$enable_coverage" = yes])

AC_CONFIG_FILES([
Makefile
examples/Makefile
libdroplet/Makefile
utests/Makefile
droplet-3.0.pc
libdroplet.spec
])

AC_OUTPUT
