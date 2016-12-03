# generated automatically by aclocal 1.15 -*- Autoconf -*-

# Copyright (C) 1996-2014 Free Software Foundation, Inc.

# This file is free software; the Free Software Foundation
# gives unlimited permission to copy and/or distribute it,
# with or without modifications, as long as this notice is preserved.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY, to the extent permitted by law; without
# even the implied warranty of MERCHANTABILITY or FITNESS FOR A
# PARTICULAR PURPOSE.

m4_ifndef([AC_CONFIG_MACRO_DIRS], [m4_defun([_AM_CONFIG_MACRO_DIRS], [])m4_defun([AC_CONFIG_MACRO_DIRS], [_AM_CONFIG_MACRO_DIRS($@)])])
# codeset.m4 serial 5 (gettext-0.18.2)
dnl Copyright (C) 2000-2002, 2006, 2008-2014, 2016 Free Software Foundation,
dnl Inc.
dnl This file is free software; the Free Software Foundation
dnl gives unlimited permission to copy and/or distribute it,
dnl with or without modifications, as long as this notice is preserved.

dnl From Bruno Haible.

AC_DEFUN([AM_LANGINFO_CODESET],
[
  AC_CACHE_CHECK([for nl_langinfo and CODESET], [am_cv_langinfo_codeset],
    [AC_LINK_IFELSE(
       [AC_LANG_PROGRAM(
          [[#include <langinfo.h>]],
          [[char* cs = nl_langinfo(CODESET); return !cs;]])],
       [am_cv_langinfo_codeset=yes],
       [am_cv_langinfo_codeset=no])
    ])
  if test $am_cv_langinfo_codeset = yes; then
    AC_DEFINE([HAVE_LANGINFO_CODESET], [1],
      [Define if you have <langinfo.h> and nl_langinfo(CODESET).])
  fi
])

dnl 'extern inline' a la ISO C99.

dnl Copyright 2012-2016 Free Software Foundation, Inc.
dnl This file is free software; the Free Software Foundation
dnl gives unlimited permission to copy and/or distribute it,
dnl with or without modifications, as long as this notice is preserved.

AC_DEFUN([gl_EXTERN_INLINE],
[
  AH_VERBATIM([extern_inline],
[/* Please see the Gnulib manual for how to use these macros.

   Suppress extern inline with HP-UX cc, as it appears to be broken; see
   <http://lists.gnu.org/archive/html/bug-texinfo/2013-02/msg00030.html>.

   Suppress extern inline with Sun C in standards-conformance mode, as it
   mishandles inline functions that call each other.  E.g., for 'inline void f
   (void) { } inline void g (void) { f (); }', c99 incorrectly complains
   'reference to static identifier "f" in extern inline function'.
   This bug was observed with Sun C 5.12 SunOS_i386 2011/11/16.

   Suppress extern inline (with or without __attribute__ ((__gnu_inline__)))
   on configurations that mistakenly use 'static inline' to implement
   functions or macros in standard C headers like <ctype.h>.  For example,
   if isdigit is mistakenly implemented via a static inline function,
   a program containing an extern inline function that calls isdigit
   may not work since the C standard prohibits extern inline functions
   from calling static functions.  This bug is known to occur on:

     OS X 10.8 and earlier; see:
     http://lists.gnu.org/archive/html/bug-gnulib/2012-12/msg00023.html

     DragonFly; see
     http://muscles.dragonflybsd.org/bulk/bleeding-edge-potential/latest-per-pkg/ah-tty-0.3.12.log

     FreeBSD; see:
     http://lists.gnu.org/archive/html/bug-gnulib/2014-07/msg00104.html

   OS X 10.9 has a macro __header_inline indicating the bug is fixed for C and
   for clang but remains for g++; see <http://trac.macports.org/ticket/41033>.
   Assume DragonFly and FreeBSD will be similar.  */
#if (((defined __APPLE__ && defined __MACH__) \
      || defined __DragonFly__ || defined __FreeBSD__) \
     && (defined __header_inline \
         ? (defined __cplusplus && defined __GNUC_STDC_INLINE__ \
            && ! defined __clang__) \
         : ((! defined _DONT_USE_CTYPE_INLINE_ \
             && (defined __GNUC__ || defined __cplusplus)) \
            || (defined _FORTIFY_SOURCE && 0 < _FORTIFY_SOURCE \
                && defined __GNUC__ && ! defined __cplusplus))))
# define _GL_EXTERN_INLINE_STDHEADER_BUG
#endif
#if ((__GNUC__ \
      ? defined __GNUC_STDC_INLINE__ && __GNUC_STDC_INLINE__ \
      : (199901L <= __STDC_VERSION__ \
         && !defined __HP_cc \
         && !defined __PGI \
         && !(defined __SUNPRO_C && __STDC__))) \
     && !defined _GL_EXTERN_INLINE_STDHEADER_BUG)
# define _GL_INLINE inline
# define _GL_EXTERN_INLINE extern inline
# define _GL_EXTERN_INLINE_IN_USE
#elif (2 < __GNUC__ + (7 <= __GNUC_MINOR__) && !defined __STRICT_ANSI__ \
       && !defined _GL_EXTERN_INLINE_STDHEADER_BUG)
# if defined __GNUC_GNU_INLINE__ && __GNUC_GNU_INLINE__
   /* __gnu_inline__ suppresses a GCC 4.2 diagnostic.  */
#  define _GL_INLINE extern inline __attribute__ ((__gnu_inline__))
# else
#  define _GL_INLINE extern inline
# endif
# define _GL_EXTERN_INLINE extern
# define _GL_EXTERN_INLINE_IN_USE
#else
# define _GL_INLINE static _GL_UNUSED
# define _GL_EXTERN_INLINE static _GL_UNUSED
#endif

/* In GCC 4.6 (inclusive) to 5.1 (exclusive),
   suppress bogus "no previous prototype for 'FOO'"
   and "no previous declaration for 'FOO'" diagnostics,
   when FOO is an inline function in the header; see
   <https://gcc.gnu.org/bugzilla/show_bug.cgi?id=54113> and
   <https://gcc.gnu.org/bugzilla/show_bug.cgi?id=63877>.  */
#if __GNUC__ == 4 && 6 <= __GNUC_MINOR__
# if defined __GNUC_STDC_INLINE__ && __GNUC_STDC_INLINE__
#  define _GL_INLINE_HEADER_CONST_PRAGMA
# else
#  define _GL_INLINE_HEADER_CONST_PRAGMA \
     _Pragma ("GCC diagnostic ignored \"-Wsuggest-attribute=const\"")
# endif
# define _GL_INLINE_HEADER_BEGIN \
    _Pragma ("GCC diagnostic push") \
    _Pragma ("GCC diagnostic ignored \"-Wmissing-prototypes\"") \
    _Pragma ("GCC diagnostic ignored \"-Wmissing-declarations\"") \
    _GL_INLINE_HEADER_CONST_PRAGMA
# define _GL_INLINE_HEADER_END \
    _Pragma ("GCC diagnostic pop")
#else
# define _GL_INLINE_HEADER_BEGIN
# define _GL_INLINE_HEADER_END
#endif])
])

# fcntl-o.m4 serial 4
dnl Copyright (C) 2006, 2009-2016 Free Software Foundation, Inc.
dnl This file is free software; the Free Software Foundation
dnl gives unlimited permission to copy and/or distribute it,
dnl with or without modifications, as long as this notice is preserved.

dnl Written by Paul Eggert.

# Test whether the flags O_NOATIME and O_NOFOLLOW actually work.
# Define HAVE_WORKING_O_NOATIME to 1 if O_NOATIME works, or to 0 otherwise.
# Define HAVE_WORKING_O_NOFOLLOW to 1 if O_NOFOLLOW works, or to 0 otherwise.
AC_DEFUN([gl_FCNTL_O_FLAGS],
[
  dnl Persuade glibc <fcntl.h> to define O_NOATIME and O_NOFOLLOW.
  dnl AC_USE_SYSTEM_EXTENSIONS was introduced in autoconf 2.60 and obsoletes
  dnl AC_GNU_SOURCE.
  m4_ifdef([AC_USE_SYSTEM_EXTENSIONS],
    [AC_REQUIRE([AC_USE_SYSTEM_EXTENSIONS])],
    [AC_REQUIRE([AC_GNU_SOURCE])])

  AC_CHECK_HEADERS_ONCE([unistd.h])
  AC_CHECK_FUNCS_ONCE([symlink])
  AC_CACHE_CHECK([for working fcntl.h], [gl_cv_header_working_fcntl_h],
    [AC_RUN_IFELSE(
       [AC_LANG_PROGRAM(
          [[#include <sys/types.h>
           #include <sys/stat.h>
           #if HAVE_UNISTD_H
           # include <unistd.h>
           #else /* on Windows with MSVC */
           # include <io.h>
           # include <stdlib.h>
           # defined sleep(n) _sleep ((n) * 1000)
           #endif
           #include <fcntl.h>
           #ifndef O_NOATIME
            #define O_NOATIME 0
           #endif
           #ifndef O_NOFOLLOW
            #define O_NOFOLLOW 0
           #endif
           static int const constants[] =
            {
              O_CREAT, O_EXCL, O_NOCTTY, O_TRUNC, O_APPEND,
              O_NONBLOCK, O_SYNC, O_ACCMODE, O_RDONLY, O_RDWR, O_WRONLY
            };
          ]],
          [[
            int result = !constants;
            #if HAVE_SYMLINK
            {
              static char const sym[] = "conftest.sym";
              if (symlink ("/dev/null", sym) != 0)
                result |= 2;
              else
                {
                  int fd = open (sym, O_WRONLY | O_NOFOLLOW | O_CREAT, 0);
                  if (fd >= 0)
                    {
                      close (fd);
                      result |= 4;
                    }
                }
              if (unlink (sym) != 0 || symlink (".", sym) != 0)
                result |= 2;
              else
                {
                  int fd = open (sym, O_RDONLY | O_NOFOLLOW);
                  if (fd >= 0)
                    {
                      close (fd);
                      result |= 4;
                    }
                }
              unlink (sym);
            }
            #endif
            {
              static char const file[] = "confdefs.h";
              int fd = open (file, O_RDONLY | O_NOATIME);
              if (fd < 0)
                result |= 8;
              else
                {
                  struct stat st0;
                  if (fstat (fd, &st0) != 0)
                    result |= 16;
                  else
                    {
                      char c;
                      sleep (1);
                      if (read (fd, &c, 1) != 1)
                        result |= 24;
                      else
                        {
                          if (close (fd) != 0)
                            result |= 32;
                          else
                            {
                              struct stat st1;
                              if (stat (file, &st1) != 0)
                                result |= 40;
                              else
                                if (st0.st_atime != st1.st_atime)
                                  result |= 64;
                            }
                        }
                    }
                }
            }
            return result;]])],
       [gl_cv_header_working_fcntl_h=yes],
       [case $? in #(
        4) gl_cv_header_working_fcntl_h='no (bad O_NOFOLLOW)';; #(
        64) gl_cv_header_working_fcntl_h='no (bad O_NOATIME)';; #(
        68) gl_cv_header_working_fcntl_h='no (bad O_NOATIME, O_NOFOLLOW)';; #(
         *) gl_cv_header_working_fcntl_h='no';;
        esac],
       [gl_cv_header_working_fcntl_h=cross-compiling])])

  case $gl_cv_header_working_fcntl_h in #(
  *O_NOATIME* | no | cross-compiling) ac_val=0;; #(
  *) ac_val=1;;
  esac
  AC_DEFINE_UNQUOTED([HAVE_WORKING_O_NOATIME], [$ac_val],
    [Define to 1 if O_NOATIME works.])

  case $gl_cv_header_working_fcntl_h in #(
  *O_NOFOLLOW* | no | cross-compiling) ac_val=0;; #(
  *) ac_val=1;;
  esac
  AC_DEFINE_UNQUOTED([HAVE_WORKING_O_NOFOLLOW], [$ac_val],
    [Define to 1 if O_NOFOLLOW works.])
])

# gettext.m4 serial 68 (gettext-0.19.8)
dnl Copyright (C) 1995-2014, 2016 Free Software Foundation, Inc.
dnl This file is free software; the Free Software Foundation
dnl gives unlimited permission to copy and/or distribute it,
dnl with or without modifications, as long as this notice is preserved.
dnl
dnl This file can be used in projects which are not available under
dnl the GNU General Public License or the GNU Library General Public
dnl License but which still want to provide support for the GNU gettext
dnl functionality.
dnl Please note that the actual code of the GNU gettext library is covered
dnl by the GNU Library General Public License, and the rest of the GNU
dnl gettext package is covered by the GNU General Public License.
dnl They are *not* in the public domain.

dnl Authors:
dnl   Ulrich Drepper <drepper@cygnus.com>, 1995-2000.
dnl   Bruno Haible <haible@clisp.cons.org>, 2000-2006, 2008-2010.

dnl Macro to add for using GNU gettext.

dnl Usage: AM_GNU_GETTEXT([INTLSYMBOL], [NEEDSYMBOL], [INTLDIR]).
dnl INTLSYMBOL can be one of 'external', 'no-libtool', 'use-libtool'. The
dnl    default (if it is not specified or empty) is 'no-libtool'.
dnl    INTLSYMBOL should be 'external' for packages with no intl directory,
dnl    and 'no-libtool' or 'use-libtool' for packages with an intl directory.
dnl    If INTLSYMBOL is 'use-libtool', then a libtool library
dnl    $(top_builddir)/intl/libintl.la will be created (shared and/or static,
dnl    depending on --{enable,disable}-{shared,static} and on the presence of
dnl    AM-DISABLE-SHARED). If INTLSYMBOL is 'no-libtool', a static library
dnl    $(top_builddir)/intl/libintl.a will be created.
dnl If NEEDSYMBOL is specified and is 'need-ngettext', then GNU gettext
dnl    implementations (in libc or libintl) without the ngettext() function
dnl    will be ignored.  If NEEDSYMBOL is specified and is
dnl    'need-formatstring-macros', then GNU gettext implementations that don't
dnl    support the ISO C 99 <inttypes.h> formatstring macros will be ignored.
dnl INTLDIR is used to find the intl libraries.  If empty,
dnl    the value '$(top_builddir)/intl/' is used.
dnl
dnl The result of the configuration is one of three cases:
dnl 1) GNU gettext, as included in the intl subdirectory, will be compiled
dnl    and used.
dnl    Catalog format: GNU --> install in $(datadir)
dnl    Catalog extension: .mo after installation, .gmo in source tree
dnl 2) GNU gettext has been found in the system's C library.
dnl    Catalog format: GNU --> install in $(datadir)
dnl    Catalog extension: .mo after installation, .gmo in source tree
dnl 3) No internationalization, always use English msgid.
dnl    Catalog format: none
dnl    Catalog extension: none
dnl If INTLSYMBOL is 'external', only cases 2 and 3 can occur.
dnl The use of .gmo is historical (it was needed to avoid overwriting the
dnl GNU format catalogs when building on a platform with an X/Open gettext),
dnl but we keep it in order not to force irrelevant filename changes on the
dnl maintainers.
dnl
AC_DEFUN([AM_GNU_GETTEXT],
[
  dnl Argument checking.
  ifelse([$1], [], , [ifelse([$1], [external], , [ifelse([$1], [no-libtool], , [ifelse([$1], [use-libtool], ,
    [errprint([ERROR: invalid first argument to AM_GNU_GETTEXT
])])])])])
  ifelse(ifelse([$1], [], [old])[]ifelse([$1], [no-libtool], [old]), [old],
    [AC_DIAGNOSE([obsolete], [Use of AM_GNU_GETTEXT without [external] argument is deprecated.])])
  ifelse([$2], [], , [ifelse([$2], [need-ngettext], , [ifelse([$2], [need-formatstring-macros], ,
    [errprint([ERROR: invalid second argument to AM_GNU_GETTEXT
])])])])
  define([gt_included_intl],
    ifelse([$1], [external],
      ifdef([AM_GNU_GETTEXT_][INTL_SUBDIR], [yes], [no]),
      [yes]))
  define([gt_libtool_suffix_prefix], ifelse([$1], [use-libtool], [l], []))
  gt_NEEDS_INIT
  AM_GNU_GETTEXT_NEED([$2])

  AC_REQUIRE([AM_PO_SUBDIRS])dnl
  ifelse(gt_included_intl, yes, [
    AC_REQUIRE([AM_INTL_SUBDIR])dnl
  ])

  dnl Prerequisites of AC_LIB_LINKFLAGS_BODY.
  AC_REQUIRE([AC_LIB_PREPARE_PREFIX])
  AC_REQUIRE([AC_LIB_RPATH])

  dnl Sometimes libintl requires libiconv, so first search for libiconv.
  dnl Ideally we would do this search only after the
  dnl      if test "$USE_NLS" = "yes"; then
  dnl        if { eval "gt_val=\$$gt_func_gnugettext_libc"; test "$gt_val" != "yes"; }; then
  dnl tests. But if configure.in invokes AM_ICONV after AM_GNU_GETTEXT
  dnl the configure script would need to contain the same shell code
  dnl again, outside any 'if'. There are two solutions:
  dnl - Invoke AM_ICONV_LINKFLAGS_BODY here, outside any 'if'.
  dnl - Control the expansions in more detail using AC_PROVIDE_IFELSE.
  dnl Since AC_PROVIDE_IFELSE is only in autoconf >= 2.52 and not
  dnl documented, we avoid it.
  ifelse(gt_included_intl, yes, , [
    AC_REQUIRE([AM_ICONV_LINKFLAGS_BODY])
  ])

  dnl Sometimes, on Mac OS X, libintl requires linking with CoreFoundation.
  gt_INTL_MACOSX

  dnl Set USE_NLS.
  AC_REQUIRE([AM_NLS])

  ifelse(gt_included_intl, yes, [
    BUILD_INCLUDED_LIBINTL=no
    USE_INCLUDED_LIBINTL=no
  ])
  LIBINTL=
  LTLIBINTL=
  POSUB=

  dnl Add a version number to the cache macros.
  case " $gt_needs " in
    *" need-formatstring-macros "*) gt_api_version=3 ;;
    *" need-ngettext "*) gt_api_version=2 ;;
    *) gt_api_version=1 ;;
  esac
  gt_func_gnugettext_libc="gt_cv_func_gnugettext${gt_api_version}_libc"
  gt_func_gnugettext_libintl="gt_cv_func_gnugettext${gt_api_version}_libintl"

  dnl If we use NLS figure out what method
  if test "$USE_NLS" = "yes"; then
    gt_use_preinstalled_gnugettext=no
    ifelse(gt_included_intl, yes, [
      AC_MSG_CHECKING([whether included gettext is requested])
      AC_ARG_WITH([included-gettext],
        [  --with-included-gettext use the GNU gettext library included here],
        nls_cv_force_use_gnu_gettext=$withval,
        nls_cv_force_use_gnu_gettext=no)
      AC_MSG_RESULT([$nls_cv_force_use_gnu_gettext])

      nls_cv_use_gnu_gettext="$nls_cv_force_use_gnu_gettext"
      if test "$nls_cv_force_use_gnu_gettext" != "yes"; then
    ])
        dnl User does not insist on using GNU NLS library.  Figure out what
        dnl to use.  If GNU gettext is available we use this.  Else we have
        dnl to fall back to GNU NLS library.

        if test $gt_api_version -ge 3; then
          gt_revision_test_code='
#ifndef __GNU_GETTEXT_SUPPORTED_REVISION
#define __GNU_GETTEXT_SUPPORTED_REVISION(major) ((major) == 0 ? 0 : -1)
#endif
changequote(,)dnl
typedef int array [2 * (__GNU_GETTEXT_SUPPORTED_REVISION(0) >= 1) - 1];
changequote([,])dnl
'
        else
          gt_revision_test_code=
        fi
        if test $gt_api_version -ge 2; then
          gt_expression_test_code=' + * ngettext ("", "", 0)'
        else
          gt_expression_test_code=
        fi

        AC_CACHE_CHECK([for GNU gettext in libc], [$gt_func_gnugettext_libc],
         [AC_LINK_IFELSE(
            [AC_LANG_PROGRAM(
               [[
#include <libintl.h>
#ifndef __GNU_GETTEXT_SUPPORTED_REVISION
extern int _nl_msg_cat_cntr;
extern int *_nl_domain_bindings;
#define __GNU_GETTEXT_SYMBOL_EXPRESSION (_nl_msg_cat_cntr + *_nl_domain_bindings)
#else
#define __GNU_GETTEXT_SYMBOL_EXPRESSION 0
#endif
$gt_revision_test_code
               ]],
               [[
bindtextdomain ("", "");
return * gettext ("")$gt_expression_test_code + __GNU_GETTEXT_SYMBOL_EXPRESSION
               ]])],
            [eval "$gt_func_gnugettext_libc=yes"],
            [eval "$gt_func_gnugettext_libc=no"])])

        if { eval "gt_val=\$$gt_func_gnugettext_libc"; test "$gt_val" != "yes"; }; then
          dnl Sometimes libintl requires libiconv, so first search for libiconv.
          ifelse(gt_included_intl, yes, , [
            AM_ICONV_LINK
          ])
          dnl Search for libintl and define LIBINTL, LTLIBINTL and INCINTL
          dnl accordingly. Don't use AC_LIB_LINKFLAGS_BODY([intl],[iconv])
          dnl because that would add "-liconv" to LIBINTL and LTLIBINTL
          dnl even if libiconv doesn't exist.
          AC_LIB_LINKFLAGS_BODY([intl])
          AC_CACHE_CHECK([for GNU gettext in libintl],
            [$gt_func_gnugettext_libintl],
           [gt_save_CPPFLAGS="$CPPFLAGS"
            CPPFLAGS="$CPPFLAGS $INCINTL"
            gt_save_LIBS="$LIBS"
            LIBS="$LIBS $LIBINTL"
            dnl Now see whether libintl exists and does not depend on libiconv.
            AC_LINK_IFELSE(
              [AC_LANG_PROGRAM(
                 [[
#include <libintl.h>
#ifndef __GNU_GETTEXT_SUPPORTED_REVISION
extern int _nl_msg_cat_cntr;
extern
#ifdef __cplusplus
"C"
#endif
const char *_nl_expand_alias (const char *);
#define __GNU_GETTEXT_SYMBOL_EXPRESSION (_nl_msg_cat_cntr + *_nl_expand_alias (""))
#else
#define __GNU_GETTEXT_SYMBOL_EXPRESSION 0
#endif
$gt_revision_test_code
                 ]],
                 [[
bindtextdomain ("", "");
return * gettext ("")$gt_expression_test_code + __GNU_GETTEXT_SYMBOL_EXPRESSION
                 ]])],
              [eval "$gt_func_gnugettext_libintl=yes"],
              [eval "$gt_func_gnugettext_libintl=no"])
            dnl Now see whether libintl exists and depends on libiconv.
            if { eval "gt_val=\$$gt_func_gnugettext_libintl"; test "$gt_val" != yes; } && test -n "$LIBICONV"; then
              LIBS="$LIBS $LIBICONV"
              AC_LINK_IFELSE(
                [AC_LANG_PROGRAM(
                   [[
#include <libintl.h>
#ifndef __GNU_GETTEXT_SUPPORTED_REVISION
extern int _nl_msg_cat_cntr;
extern
#ifdef __cplusplus
"C"
#endif
const char *_nl_expand_alias (const char *);
#define __GNU_GETTEXT_SYMBOL_EXPRESSION (_nl_msg_cat_cntr + *_nl_expand_alias (""))
#else
#define __GNU_GETTEXT_SYMBOL_EXPRESSION 0
#endif
$gt_revision_test_code
                   ]],
                   [[
bindtextdomain ("", "");
return * gettext ("")$gt_expression_test_code + __GNU_GETTEXT_SYMBOL_EXPRESSION
                   ]])],
                [LIBINTL="$LIBINTL $LIBICONV"
                 LTLIBINTL="$LTLIBINTL $LTLIBICONV"
                 eval "$gt_func_gnugettext_libintl=yes"
                ])
            fi
            CPPFLAGS="$gt_save_CPPFLAGS"
            LIBS="$gt_save_LIBS"])
        fi

        dnl If an already present or preinstalled GNU gettext() is found,
        dnl use it.  But if this macro is used in GNU gettext, and GNU
        dnl gettext is already preinstalled in libintl, we update this
        dnl libintl.  (Cf. the install rule in intl/Makefile.in.)
        if { eval "gt_val=\$$gt_func_gnugettext_libc"; test "$gt_val" = "yes"; } \
           || { { eval "gt_val=\$$gt_func_gnugettext_libintl"; test "$gt_val" = "yes"; } \
                && test "$PACKAGE" != gettext-runtime \
                && test "$PACKAGE" != gettext-tools; }; then
          gt_use_preinstalled_gnugettext=yes
        else
          dnl Reset the values set by searching for libintl.
          LIBINTL=
          LTLIBINTL=
          INCINTL=
        fi

    ifelse(gt_included_intl, yes, [
        if test "$gt_use_preinstalled_gnugettext" != "yes"; then
          dnl GNU gettext is not found in the C library.
          dnl Fall back on included GNU gettext library.
          nls_cv_use_gnu_gettext=yes
        fi
      fi

      if test "$nls_cv_use_gnu_gettext" = "yes"; then
        dnl Mark actions used to generate GNU NLS library.
        BUILD_INCLUDED_LIBINTL=yes
        USE_INCLUDED_LIBINTL=yes
        LIBINTL="ifelse([$3],[],\${top_builddir}/intl,[$3])/libintl.[]gt_libtool_suffix_prefix[]a $LIBICONV $LIBTHREAD"
        LTLIBINTL="ifelse([$3],[],\${top_builddir}/intl,[$3])/libintl.[]gt_libtool_suffix_prefix[]a $LTLIBICONV $LTLIBTHREAD"
        LIBS=`echo " $LIBS " | sed -e 's/ -lintl / /' -e 's/^ //' -e 's/ $//'`
      fi

      CATOBJEXT=
      if test "$gt_use_preinstalled_gnugettext" = "yes" \
         || test "$nls_cv_use_gnu_gettext" = "yes"; then
        dnl Mark actions to use GNU gettext tools.
        CATOBJEXT=.gmo
      fi
    ])

    if test -n "$INTL_MACOSX_LIBS"; then
      if test "$gt_use_preinstalled_gnugettext" = "yes" \
         || test "$nls_cv_use_gnu_gettext" = "yes"; then
        dnl Some extra flags are needed during linking.
        LIBINTL="$LIBINTL $INTL_MACOSX_LIBS"
        LTLIBINTL="$LTLIBINTL $INTL_MACOSX_LIBS"
      fi
    fi

    if test "$gt_use_preinstalled_gnugettext" = "yes" \
       || test "$nls_cv_use_gnu_gettext" = "yes"; then
      AC_DEFINE([ENABLE_NLS], [1],
        [Define to 1 if translation of program messages to the user's native language
   is requested.])
    else
      USE_NLS=no
    fi
  fi

  AC_MSG_CHECKING([whether to use NLS])
  AC_MSG_RESULT([$USE_NLS])
  if test "$USE_NLS" = "yes"; then
    AC_MSG_CHECKING([where the gettext function comes from])
    if test "$gt_use_preinstalled_gnugettext" = "yes"; then
      if { eval "gt_val=\$$gt_func_gnugettext_libintl"; test "$gt_val" = "yes"; }; then
        gt_source="external libintl"
      else
        gt_source="libc"
      fi
    else
      gt_source="included intl directory"
    fi
    AC_MSG_RESULT([$gt_source])
  fi

  if test "$USE_NLS" = "yes"; then

    if test "$gt_use_preinstalled_gnugettext" = "yes"; then
      if { eval "gt_val=\$$gt_func_gnugettext_libintl"; test "$gt_val" = "yes"; }; then
        AC_MSG_CHECKING([how to link with libintl])
        AC_MSG_RESULT([$LIBINTL])
        AC_LIB_APPENDTOVAR([CPPFLAGS], [$INCINTL])
      fi

      dnl For backward compatibility. Some packages may be using this.
      AC_DEFINE([HAVE_GETTEXT], [1],
       [Define if the GNU gettext() function is already present or preinstalled.])
      AC_DEFINE([HAVE_DCGETTEXT], [1],
       [Define if the GNU dcgettext() function is already present or preinstalled.])
    fi

    dnl We need to process the po/ directory.
    POSUB=po
  fi

  ifelse(gt_included_intl, yes, [
    dnl If this is used in GNU gettext we have to set BUILD_INCLUDED_LIBINTL
    dnl to 'yes' because some of the testsuite requires it.
    if test "$PACKAGE" = gettext-runtime || test "$PACKAGE" = gettext-tools; then
      BUILD_INCLUDED_LIBINTL=yes
    fi

    dnl Make all variables we use known to autoconf.
    AC_SUBST([BUILD_INCLUDED_LIBINTL])
    AC_SUBST([USE_INCLUDED_LIBINTL])
    AC_SUBST([CATOBJEXT])

    dnl For backward compatibility. Some configure.ins may be using this.
    nls_cv_header_intl=
    nls_cv_header_libgt=

    dnl For backward compatibility. Some Makefiles may be using this.
    DATADIRNAME=share
    AC_SUBST([DATADIRNAME])

    dnl For backward compatibility. Some Makefiles may be using this.
    INSTOBJEXT=.mo
    AC_SUBST([INSTOBJEXT])

    dnl For backward compatibility. Some Makefiles may be using this.
    GENCAT=gencat
    AC_SUBST([GENCAT])

    dnl For backward compatibility. Some Makefiles may be using this.
    INTLOBJS=
    if test "$USE_INCLUDED_LIBINTL" = yes; then
      INTLOBJS="\$(GETTOBJS)"
    fi
    AC_SUBST([INTLOBJS])

    dnl Enable libtool support if the surrounding package wishes it.
    INTL_LIBTOOL_SUFFIX_PREFIX=gt_libtool_suffix_prefix
    AC_SUBST([INTL_LIBTOOL_SUFFIX_PREFIX])
  ])

  dnl For backward compatibility. Some Makefiles may be using this.
  INTLLIBS="$LIBINTL"
  AC_SUBST([INTLLIBS])

  dnl Make all documented variables known to autoconf.
  AC_SUBST([LIBINTL])
  AC_SUBST([LTLIBINTL])
  AC_SUBST([POSUB])
])


dnl gt_NEEDS_INIT ensures that the gt_needs variable is initialized.
m4_define([gt_NEEDS_INIT],
[
  m4_divert_text([DEFAULTS], [gt_needs=])
  m4_define([gt_NEEDS_INIT], [])
])


dnl Usage: AM_GNU_GETTEXT_NEED([NEEDSYMBOL])
AC_DEFUN([AM_GNU_GETTEXT_NEED],
[
  m4_divert_text([INIT_PREPARE], [gt_needs="$gt_needs $1"])
])


dnl Usage: AM_GNU_GETTEXT_VERSION([gettext-version])
AC_DEFUN([AM_GNU_GETTEXT_VERSION], [])


dnl Usage: AM_GNU_GETTEXT_REQUIRE_VERSION([gettext-version])
AC_DEFUN([AM_GNU_GETTEXT_REQUIRE_VERSION], [])

# glibc2.m4 serial 3
dnl Copyright (C) 2000-2002, 2004, 2008, 2010-2016 Free Software Foundation,
dnl Inc.
dnl This file is free software; the Free Software Foundation
dnl gives unlimited permission to copy and/or distribute it,
dnl with or without modifications, as long as this notice is preserved.

# Test for the GNU C Library, version 2.0 or newer.
# From Bruno Haible.

AC_DEFUN([gt_GLIBC2],
  [
    AC_CACHE_CHECK([whether we are using the GNU C Library 2 or newer],
      [ac_cv_gnu_library_2],
      [AC_EGREP_CPP([Lucky GNU user],
        [
#include <features.h>
#ifdef __GNU_LIBRARY__
 #if (__GLIBC__ >= 2) && !defined __UCLIBC__
  Lucky GNU user
 #endif
#endif
        ],
        [ac_cv_gnu_library_2=yes],
        [ac_cv_gnu_library_2=no])
      ]
    )
    AC_SUBST([GLIBC2])
    GLIBC2="$ac_cv_gnu_library_2"
  ]
)

# glibc21.m4 serial 5
dnl Copyright (C) 2000-2002, 2004, 2008, 2010-2016 Free Software Foundation,
dnl Inc.
dnl This file is free software; the Free Software Foundation
dnl gives unlimited permission to copy and/or distribute it,
dnl with or without modifications, as long as this notice is preserved.

# Test for the GNU C Library, version 2.1 or newer, or uClibc.
# From Bruno Haible.

AC_DEFUN([gl_GLIBC21],
  [
    AC_CACHE_CHECK([whether we are using the GNU C Library >= 2.1 or uClibc],
      [ac_cv_gnu_library_2_1],
      [AC_EGREP_CPP([Lucky],
        [
#include <features.h>
#ifdef __GNU_LIBRARY__
 #if (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 1) || (__GLIBC__ > 2)
  Lucky GNU user
 #endif
#endif
#ifdef __UCLIBC__
 Lucky user
#endif
        ],
        [ac_cv_gnu_library_2_1=yes],
        [ac_cv_gnu_library_2_1=no])
      ]
    )
    AC_SUBST([GLIBC21])
    GLIBC21="$ac_cv_gnu_library_2_1"
  ]
)

# iconv.m4 serial 19 (gettext-0.18.2)
dnl Copyright (C) 2000-2002, 2007-2014, 2016 Free Software Foundation, Inc.
dnl This file is free software; the Free Software Foundation
dnl gives unlimited permission to copy and/or distribute it,
dnl with or without modifications, as long as this notice is preserved.

dnl From Bruno Haible.

AC_DEFUN([AM_ICONV_LINKFLAGS_BODY],
[
  dnl Prerequisites of AC_LIB_LINKFLAGS_BODY.
  AC_REQUIRE([AC_LIB_PREPARE_PREFIX])
  AC_REQUIRE([AC_LIB_RPATH])

  dnl Search for libiconv and define LIBICONV, LTLIBICONV and INCICONV
  dnl accordingly.
  AC_LIB_LINKFLAGS_BODY([iconv])
])

AC_DEFUN([AM_ICONV_LINK],
[
  dnl Some systems have iconv in libc, some have it in libiconv (OSF/1 and
  dnl those with the standalone portable GNU libiconv installed).
  AC_REQUIRE([AC_CANONICAL_HOST]) dnl for cross-compiles

  dnl Search for libiconv and define LIBICONV, LTLIBICONV and INCICONV
  dnl accordingly.
  AC_REQUIRE([AM_ICONV_LINKFLAGS_BODY])

  dnl Add $INCICONV to CPPFLAGS before performing the following checks,
  dnl because if the user has installed libiconv and not disabled its use
  dnl via --without-libiconv-prefix, he wants to use it. The first
  dnl AC_LINK_IFELSE will then fail, the second AC_LINK_IFELSE will succeed.
  am_save_CPPFLAGS="$CPPFLAGS"
  AC_LIB_APPENDTOVAR([CPPFLAGS], [$INCICONV])

  AC_CACHE_CHECK([for iconv], [am_cv_func_iconv], [
    am_cv_func_iconv="no, consider installing GNU libiconv"
    am_cv_lib_iconv=no
    AC_LINK_IFELSE(
      [AC_LANG_PROGRAM(
         [[
#include <stdlib.h>
#include <iconv.h>
         ]],
         [[iconv_t cd = iconv_open("","");
           iconv(cd,NULL,NULL,NULL,NULL);
           iconv_close(cd);]])],
      [am_cv_func_iconv=yes])
    if test "$am_cv_func_iconv" != yes; then
      am_save_LIBS="$LIBS"
      LIBS="$LIBS $LIBICONV"
      AC_LINK_IFELSE(
        [AC_LANG_PROGRAM(
           [[
#include <stdlib.h>
#include <iconv.h>
           ]],
           [[iconv_t cd = iconv_open("","");
             iconv(cd,NULL,NULL,NULL,NULL);
             iconv_close(cd);]])],
        [am_cv_lib_iconv=yes]
        [am_cv_func_iconv=yes])
      LIBS="$am_save_LIBS"
    fi
  ])
  if test "$am_cv_func_iconv" = yes; then
    AC_CACHE_CHECK([for working iconv], [am_cv_func_iconv_works], [
      dnl This tests against bugs in AIX 5.1, AIX 6.1..7.1, HP-UX 11.11,
      dnl Solaris 10.
      am_save_LIBS="$LIBS"
      if test $am_cv_lib_iconv = yes; then
        LIBS="$LIBS $LIBICONV"
      fi
      am_cv_func_iconv_works=no
      for ac_iconv_const in '' 'const'; do
        AC_RUN_IFELSE(
          [AC_LANG_PROGRAM(
             [[
#include <iconv.h>
#include <string.h>

#ifndef ICONV_CONST
# define ICONV_CONST $ac_iconv_const
#endif
             ]],
             [[int result = 0;
  /* Test against AIX 5.1 bug: Failures are not distinguishable from successful
     returns.  */
  {
    iconv_t cd_utf8_to_88591 = iconv_open ("ISO8859-1", "UTF-8");
    if (cd_utf8_to_88591 != (iconv_t)(-1))
      {
        static ICONV_CONST char input[] = "\342\202\254"; /* EURO SIGN */
        char buf[10];
        ICONV_CONST char *inptr = input;
        size_t inbytesleft = strlen (input);
        char *outptr = buf;
        size_t outbytesleft = sizeof (buf);
        size_t res = iconv (cd_utf8_to_88591,
                            &inptr, &inbytesleft,
                            &outptr, &outbytesleft);
        if (res == 0)
          result |= 1;
        iconv_close (cd_utf8_to_88591);
      }
  }
  /* Test against Solaris 10 bug: Failures are not distinguishable from
     successful returns.  */
  {
    iconv_t cd_ascii_to_88591 = iconv_open ("ISO8859-1", "646");
    if (cd_ascii_to_88591 != (iconv_t)(-1))
      {
        static ICONV_CONST char input[] = "\263";
        char buf[10];
        ICONV_CONST char *inptr = input;
        size_t inbytesleft = strlen (input);
        char *outptr = buf;
        size_t outbytesleft = sizeof (buf);
        size_t res = iconv (cd_ascii_to_88591,
                            &inptr, &inbytesleft,
                            &outptr, &outbytesleft);
        if (res == 0)
          result |= 2;
        iconv_close (cd_ascii_to_88591);
      }
  }
  /* Test against AIX 6.1..7.1 bug: Buffer overrun.  */
  {
    iconv_t cd_88591_to_utf8 = iconv_open ("UTF-8", "ISO-8859-1");
    if (cd_88591_to_utf8 != (iconv_t)(-1))
      {
        static ICONV_CONST char input[] = "\304";
        static char buf[2] = { (char)0xDE, (char)0xAD };
        ICONV_CONST char *inptr = input;
        size_t inbytesleft = 1;
        char *outptr = buf;
        size_t outbytesleft = 1;
        size_t res = iconv (cd_88591_to_utf8,
                            &inptr, &inbytesleft,
                            &outptr, &outbytesleft);
        if (res != (size_t)(-1) || outptr - buf > 1 || buf[1] != (char)0xAD)
          result |= 4;
        iconv_close (cd_88591_to_utf8);
      }
  }
#if 0 /* This bug could be worked around by the caller.  */
  /* Test against HP-UX 11.11 bug: Positive return value instead of 0.  */
  {
    iconv_t cd_88591_to_utf8 = iconv_open ("utf8", "iso88591");
    if (cd_88591_to_utf8 != (iconv_t)(-1))
      {
        static ICONV_CONST char input[] = "\304rger mit b\366sen B\374bchen ohne Augenma\337";
        char buf[50];
        ICONV_CONST char *inptr = input;
        size_t inbytesleft = strlen (input);
        char *outptr = buf;
        size_t outbytesleft = sizeof (buf);
        size_t res = iconv (cd_88591_to_utf8,
                            &inptr, &inbytesleft,
                            &outptr, &outbytesleft);
        if ((int)res > 0)
          result |= 8;
        iconv_close (cd_88591_to_utf8);
      }
  }
#endif
  /* Test against HP-UX 11.11 bug: No converter from EUC-JP to UTF-8 is
     provided.  */
  if (/* Try standardized names.  */
      iconv_open ("UTF-8", "EUC-JP") == (iconv_t)(-1)
      /* Try IRIX, OSF/1 names.  */
      && iconv_open ("UTF-8", "eucJP") == (iconv_t)(-1)
      /* Try AIX names.  */
      && iconv_open ("UTF-8", "IBM-eucJP") == (iconv_t)(-1)
      /* Try HP-UX names.  */
      && iconv_open ("utf8", "eucJP") == (iconv_t)(-1))
    result |= 16;
  return result;
]])],
          [am_cv_func_iconv_works=yes], ,
          [case "$host_os" in
             aix* | hpux*) am_cv_func_iconv_works="guessing no" ;;
             *)            am_cv_func_iconv_works="guessing yes" ;;
           esac])
        test "$am_cv_func_iconv_works" = no || break
      done
      LIBS="$am_save_LIBS"
    ])
    case "$am_cv_func_iconv_works" in
      *no) am_func_iconv=no am_cv_lib_iconv=no ;;
      *)   am_func_iconv=yes ;;
    esac
  else
    am_func_iconv=no am_cv_lib_iconv=no
  fi
  if test "$am_func_iconv" = yes; then
    AC_DEFINE([HAVE_ICONV], [1],
      [Define if you have the iconv() function and it works.])
  fi
  if test "$am_cv_lib_iconv" = yes; then
    AC_MSG_CHECKING([how to link with libiconv])
    AC_MSG_RESULT([$LIBICONV])
  else
    dnl If $LIBICONV didn't lead to a usable library, we don't need $INCICONV
    dnl either.
    CPPFLAGS="$am_save_CPPFLAGS"
    LIBICONV=
    LTLIBICONV=
  fi
  AC_SUBST([LIBICONV])
  AC_SUBST([LTLIBICONV])
])

dnl Define AM_ICONV using AC_DEFUN_ONCE for Autoconf >= 2.64, in order to
dnl avoid warnings like
dnl "warning: AC_REQUIRE: `AM_ICONV' was expanded before it was required".
dnl This is tricky because of the way 'aclocal' is implemented:
dnl - It requires defining an auxiliary macro whose name ends in AC_DEFUN.
dnl   Otherwise aclocal's initial scan pass would miss the macro definition.
dnl - It requires a line break inside the AC_DEFUN_ONCE and AC_DEFUN expansions.
dnl   Otherwise aclocal would emit many "Use of uninitialized value $1"
dnl   warnings.
m4_define([gl_iconv_AC_DEFUN],
  m4_version_prereq([2.64],
    [[AC_DEFUN_ONCE(
        [$1], [$2])]],
    [m4_ifdef([gl_00GNULIB],
       [[AC_DEFUN_ONCE(
           [$1], [$2])]],
       [[AC_DEFUN(
           [$1], [$2])]])]))
gl_iconv_AC_DEFUN([AM_ICONV],
[
  AM_ICONV_LINK
  if test "$am_cv_func_iconv" = yes; then
    AC_MSG_CHECKING([for iconv declaration])
    AC_CACHE_VAL([am_cv_proto_iconv], [
      AC_COMPILE_IFELSE(
        [AC_LANG_PROGRAM(
           [[
#include <stdlib.h>
#include <iconv.h>
extern
#ifdef __cplusplus
"C"
#endif
#if defined(__STDC__) || defined(_MSC_VER) || defined(__cplusplus)
size_t iconv (iconv_t cd, char * *inbuf, size_t *inbytesleft, char * *outbuf, size_t *outbytesleft);
#else
size_t iconv();
#endif
           ]],
           [[]])],
        [am_cv_proto_iconv_arg1=""],
        [am_cv_proto_iconv_arg1="const"])
      am_cv_proto_iconv="extern size_t iconv (iconv_t cd, $am_cv_proto_iconv_arg1 char * *inbuf, size_t *inbytesleft, char * *outbuf, size_t *outbytesleft);"])
    am_cv_proto_iconv=`echo "[$]am_cv_proto_iconv" | tr -s ' ' | sed -e 's/( /(/'`
    AC_MSG_RESULT([
         $am_cv_proto_iconv])
    AC_DEFINE_UNQUOTED([ICONV_CONST], [$am_cv_proto_iconv_arg1],
      [Define as const if the declaration of iconv() needs const.])
    dnl Also substitute ICONV_CONST in the gnulib generated <iconv.h>.
    m4_ifdef([gl_ICONV_H_DEFAULTS],
      [AC_REQUIRE([gl_ICONV_H_DEFAULTS])
       if test -n "$am_cv_proto_iconv_arg1"; then
         ICONV_CONST="const"
       fi
      ])
  fi
])

# intdiv0.m4 serial 6 (gettext-0.18.2)
dnl Copyright (C) 2002, 2007-2008, 2010-2016 Free Software Foundation, Inc.
dnl This file is free software; the Free Software Foundation
dnl gives unlimited permission to copy and/or distribute it,
dnl with or without modifications, as long as this notice is preserved.

dnl From Bruno Haible.

AC_DEFUN([gt_INTDIV0],
[
  AC_REQUIRE([AC_PROG_CC])dnl
  AC_REQUIRE([AC_CANONICAL_HOST])dnl

  AC_CACHE_CHECK([whether integer division by zero raises SIGFPE],
    gt_cv_int_divbyzero_sigfpe,
    [
      gt_cv_int_divbyzero_sigfpe=
changequote(,)dnl
      case "$host_os" in
        macos* | darwin[6-9]* | darwin[1-9][0-9]*)
          # On Mac OS X 10.2 or newer, just assume the same as when cross-
          # compiling. If we were to perform the real test, 1 Crash Report
          # dialog window would pop up.
          case "$host_cpu" in
            i[34567]86 | x86_64)
              gt_cv_int_divbyzero_sigfpe="guessing yes" ;;
          esac
          ;;
      esac
changequote([,])dnl
      if test -z "$gt_cv_int_divbyzero_sigfpe"; then
        AC_RUN_IFELSE(
          [AC_LANG_SOURCE([[
#include <stdlib.h>
#include <signal.h>

static void
sigfpe_handler (int sig)
{
  /* Exit with code 0 if SIGFPE, with code 1 if any other signal.  */
  _exit (sig != SIGFPE);
}

int x = 1;
int y = 0;
int z;
int nan;

int main ()
{
  signal (SIGFPE, sigfpe_handler);
/* IRIX and AIX (when "xlc -qcheck" is used) yield signal SIGTRAP.  */
#if (defined (__sgi) || defined (_AIX)) && defined (SIGTRAP)
  signal (SIGTRAP, sigfpe_handler);
#endif
/* Linux/SPARC yields signal SIGILL.  */
#if defined (__sparc__) && defined (__linux__)
  signal (SIGILL, sigfpe_handler);
#endif

  z = x / y;
  nan = y / y;
  exit (2);
}
]])],
          [gt_cv_int_divbyzero_sigfpe=yes],
          [gt_cv_int_divbyzero_sigfpe=no],
          [
            # Guess based on the CPU.
changequote(,)dnl
            case "$host_cpu" in
              alpha* | i[34567]86 | x86_64 | m68k | s390*)
                gt_cv_int_divbyzero_sigfpe="guessing yes";;
              *)
                gt_cv_int_divbyzero_sigfpe="guessing no";;
            esac
changequote([,])dnl
          ])
      fi
    ])
  case "$gt_cv_int_divbyzero_sigfpe" in
    *yes) value=1;;
    *) value=0;;
  esac
  AC_DEFINE_UNQUOTED([INTDIV0_RAISES_SIGFPE], [$value],
    [Define if integer division by zero raises signal SIGFPE.])
])

# intl.m4 serial 29 (gettext-0.19)
dnl Copyright (C) 1995-2014, 2016 Free Software Foundation, Inc.
dnl This file is free software; the Free Software Foundation
dnl gives unlimited permission to copy and/or distribute it,
dnl with or without modifications, as long as this notice is preserved.
dnl
dnl This file can be used in projects which are not available under
dnl the GNU General Public License or the GNU Library General Public
dnl License but which still want to provide support for the GNU gettext
dnl functionality.
dnl Please note that the actual code of the GNU gettext library is covered
dnl by the GNU Library General Public License, and the rest of the GNU
dnl gettext package is covered by the GNU General Public License.
dnl They are *not* in the public domain.

dnl Authors:
dnl   Ulrich Drepper <drepper@cygnus.com>, 1995-2000.
dnl   Bruno Haible <haible@clisp.cons.org>, 2000-2009.

AC_PREREQ([2.60])

dnl Checks for all prerequisites of the intl subdirectory,
dnl except for INTL_LIBTOOL_SUFFIX_PREFIX (and possibly LIBTOOL), INTLOBJS,
dnl            USE_INCLUDED_LIBINTL, BUILD_INCLUDED_LIBINTL.
AC_DEFUN([AM_INTL_SUBDIR],
[
  AC_REQUIRE([AC_PROG_INSTALL])dnl
  AC_REQUIRE([AC_PROG_MKDIR_P])dnl
  AC_REQUIRE([AC_PROG_CC])dnl
  AC_REQUIRE([AC_CANONICAL_HOST])dnl
  AC_REQUIRE([gt_GLIBC2])dnl
  AC_REQUIRE([AC_PROG_RANLIB])dnl
  AC_REQUIRE([gl_VISIBILITY])dnl
  AC_REQUIRE([gt_INTL_SUBDIR_CORE])dnl
  AC_REQUIRE([AC_TYPE_LONG_LONG_INT])dnl
  AC_REQUIRE([gt_TYPE_WCHAR_T])dnl
  AC_REQUIRE([gt_TYPE_WINT_T])dnl
  AC_REQUIRE([gl_AC_HEADER_INTTYPES_H])
  AC_REQUIRE([gt_TYPE_INTMAX_T])
  AC_REQUIRE([gt_PRINTF_POSIX])
  AC_REQUIRE([gl_GLIBC21])dnl
  AC_REQUIRE([gl_XSIZE])dnl
  AC_REQUIRE([gl_FCNTL_O_FLAGS])dnl
  AC_REQUIRE([gt_INTL_MACOSX])dnl
  AC_REQUIRE([gl_EXTERN_INLINE])dnl
  AC_REQUIRE([gt_GL_ATTRIBUTE])dnl

  dnl Support for automake's --enable-silent-rules.
  case "$enable_silent_rules" in
    yes) INTL_DEFAULT_VERBOSITY=0;;
    no)  INTL_DEFAULT_VERBOSITY=1;;
    *)   INTL_DEFAULT_VERBOSITY=1;;
  esac
  AC_SUBST([INTL_DEFAULT_VERBOSITY])

  AC_CHECK_TYPE([ptrdiff_t], ,
    [AC_DEFINE([ptrdiff_t], [long],
       [Define as the type of the result of subtracting two pointers, if the system doesn't define it.])
    ])
  AC_CHECK_HEADERS([features.h stddef.h stdlib.h string.h])
  AC_CHECK_FUNCS([asprintf fwprintf newlocale putenv setenv setlocale \
    snprintf strnlen wcslen wcsnlen mbrtowc wcrtomb])

  dnl Use the _snprintf function only if it is declared (because on NetBSD it
  dnl is defined as a weak alias of snprintf; we prefer to use the latter).
  AC_CHECK_DECLS([_snprintf, _snwprintf], , , [#include <stdio.h>])

  dnl Use the *_unlocked functions only if they are declared.
  dnl (because some of them were defined without being declared in Solaris
  dnl 2.5.1 but were removed in Solaris 2.6, whereas we want binaries built
  dnl on Solaris 2.5.1 to run on Solaris 2.6).
  AC_CHECK_DECLS([getc_unlocked], , , [#include <stdio.h>])

  case $gt_cv_func_printf_posix in
    *yes) HAVE_POSIX_PRINTF=1 ;;
    *) HAVE_POSIX_PRINTF=0 ;;
  esac
  AC_SUBST([HAVE_POSIX_PRINTF])
  if test "$ac_cv_func_asprintf" = yes; then
    HAVE_ASPRINTF=1
  else
    HAVE_ASPRINTF=0
  fi
  AC_SUBST([HAVE_ASPRINTF])
  if test "$ac_cv_func_snprintf" = yes; then
    HAVE_SNPRINTF=1
  else
    HAVE_SNPRINTF=0
  fi
  AC_SUBST([HAVE_SNPRINTF])
  if test "$ac_cv_func_newlocale" = yes; then
    HAVE_NEWLOCALE=1
  else
    HAVE_NEWLOCALE=0
  fi
  AC_SUBST([HAVE_NEWLOCALE])
  if test "$ac_cv_func_wprintf" = yes; then
    HAVE_WPRINTF=1
  else
    HAVE_WPRINTF=0
  fi
  AC_SUBST([HAVE_WPRINTF])

  AM_LANGINFO_CODESET
  gt_LC_MESSAGES

  dnl Compilation on mingw and Cygwin needs special Makefile rules, because
  dnl 1. when we install a shared library, we must arrange to export
  dnl    auxiliary pointer variables for every exported variable,
  dnl 2. when we install a shared library and a static library simultaneously,
  dnl    the include file specifies __declspec(dllimport) and therefore we
  dnl    must arrange to define the auxiliary pointer variables for the
  dnl    exported variables _also_ in the static library.
  if test "$enable_shared" = yes; then
    case "$host_os" in
      mingw* | cygwin*) is_woe32dll=yes ;;
      *) is_woe32dll=no ;;
    esac
  else
    is_woe32dll=no
  fi
  WOE32DLL=$is_woe32dll
  AC_SUBST([WOE32DLL])

  dnl On mingw and Cygwin, we can activate special Makefile rules which add
  dnl version information to the shared libraries and executables.
  case "$host_os" in
    mingw* | cygwin*) is_woe32=yes ;;
    *) is_woe32=no ;;
  esac
  WOE32=$is_woe32
  AC_SUBST([WOE32])
  if test $WOE32 = yes; then
    dnl Check for a program that compiles Windows resource files.
    AC_CHECK_TOOL([WINDRES], [windres])
  fi

  dnl Determine whether when creating a library, "-lc" should be passed to
  dnl libtool or not. On many platforms, it is required for the libtool option
  dnl -no-undefined to work. On HP-UX, however, the -lc - stored by libtool
  dnl in the *.la files - makes it impossible to create multithreaded programs,
  dnl because libtool also reorders the -lc to come before the -pthread, and
  dnl this disables pthread_create() <http://docs.hp.com/en/1896/pthreads.html>.
  case "$host_os" in
    hpux*) LTLIBC="" ;;
    *)     LTLIBC="-lc" ;;
  esac
  AC_SUBST([LTLIBC])

  dnl Rename some macros and functions used for locking.
  AH_BOTTOM([
#define __libc_lock_t                   gl_lock_t
#define __libc_lock_define              gl_lock_define
#define __libc_lock_define_initialized  gl_lock_define_initialized
#define __libc_lock_init                gl_lock_init
#define __libc_lock_lock                gl_lock_lock
#define __libc_lock_unlock              gl_lock_unlock
#define __libc_lock_recursive_t                   gl_recursive_lock_t
#define __libc_lock_define_recursive              gl_recursive_lock_define
#define __libc_lock_define_initialized_recursive  gl_recursive_lock_define_initialized
#define __libc_lock_init_recursive                gl_recursive_lock_init
#define __libc_lock_lock_recursive                gl_recursive_lock_lock
#define __libc_lock_unlock_recursive              gl_recursive_lock_unlock
#define glthread_in_use  libintl_thread_in_use
#define glthread_lock_init_func     libintl_lock_init_func
#define glthread_lock_lock_func     libintl_lock_lock_func
#define glthread_lock_unlock_func   libintl_lock_unlock_func
#define glthread_lock_destroy_func  libintl_lock_destroy_func
#define glthread_rwlock_init_multithreaded     libintl_rwlock_init_multithreaded
#define glthread_rwlock_init_func              libintl_rwlock_init_func
#define glthread_rwlock_rdlock_multithreaded   libintl_rwlock_rdlock_multithreaded
#define glthread_rwlock_rdlock_func            libintl_rwlock_rdlock_func
#define glthread_rwlock_wrlock_multithreaded   libintl_rwlock_wrlock_multithreaded
#define glthread_rwlock_wrlock_func            libintl_rwlock_wrlock_func
#define glthread_rwlock_unlock_multithreaded   libintl_rwlock_unlock_multithreaded
#define glthread_rwlock_unlock_func            libintl_rwlock_unlock_func
#define glthread_rwlock_destroy_multithreaded  libintl_rwlock_destroy_multithreaded
#define glthread_rwlock_destroy_func           libintl_rwlock_destroy_func
#define glthread_recursive_lock_init_multithreaded     libintl_recursive_lock_init_multithreaded
#define glthread_recursive_lock_init_func              libintl_recursive_lock_init_func
#define glthread_recursive_lock_lock_multithreaded     libintl_recursive_lock_lock_multithreaded
#define glthread_recursive_lock_lock_func              libintl_recursive_lock_lock_func
#define glthread_recursive_lock_unlock_multithreaded   libintl_recursive_lock_unlock_multithreaded
#define glthread_recursive_lock_unlock_func            libintl_recursive_lock_unlock_func
#define glthread_recursive_lock_destroy_multithreaded  libintl_recursive_lock_destroy_multithreaded
#define glthread_recursive_lock_destroy_func           libintl_recursive_lock_destroy_func
#define glthread_once_func            libintl_once_func
#define glthread_once_singlethreaded  libintl_once_singlethreaded
#define glthread_once_multithreaded   libintl_once_multithreaded
])
])


dnl Checks for the core files of the intl subdirectory:
dnl   dcigettext.c
dnl   eval-plural.h
dnl   explodename.c
dnl   finddomain.c
dnl   gettextP.h
dnl   gmo.h
dnl   hash-string.h hash-string.c
dnl   l10nflist.c
dnl   libgnuintl.h.in (except the *printf stuff)
dnl   loadinfo.h
dnl   loadmsgcat.c
dnl   localealias.c
dnl   log.c
dnl   plural-exp.h plural-exp.c
dnl   plural.y
dnl Used by libglocale.
AC_DEFUN([gt_INTL_SUBDIR_CORE],
[
  AC_REQUIRE([AC_C_INLINE])dnl
  AC_REQUIRE([AC_TYPE_SIZE_T])dnl
  AC_REQUIRE([gl_AC_HEADER_STDINT_H])
  AC_REQUIRE([AC_FUNC_ALLOCA])dnl
  AC_REQUIRE([AC_FUNC_MMAP])dnl
  AC_REQUIRE([gt_INTDIV0])dnl
  AC_REQUIRE([gl_AC_TYPE_UINTMAX_T])dnl
  AC_REQUIRE([gt_INTTYPES_PRI])dnl
  AC_REQUIRE([gl_LOCK])dnl

  AC_LINK_IFELSE(
    [AC_LANG_PROGRAM(
       [[int foo (int a) { a = __builtin_expect (a, 10); return a == 10 ? 0 : 1; }]],
       [[]])],
    [AC_DEFINE([HAVE_BUILTIN_EXPECT], [1],
       [Define to 1 if the compiler understands __builtin_expect.])])

  AC_CHECK_HEADERS([argz.h inttypes.h limits.h unistd.h sys/param.h])
  AC_CHECK_FUNCS([getcwd getegid geteuid getgid getuid mempcpy munmap \
    stpcpy strcasecmp strdup strtoul tsearch uselocale argz_count \
    argz_stringify argz_next __fsetlocking])

  dnl Solaris 12 provides getlocalename_l, while Illumos doesn't have
  dnl it nor the equivalent.
  if test $ac_cv_func_uselocale = yes; then
    AC_CHECK_FUNCS([getlocalename_l])
  fi

  dnl Use the *_unlocked functions only if they are declared.
  dnl (because some of them were defined without being declared in Solaris
  dnl 2.5.1 but were removed in Solaris 2.6, whereas we want binaries built
  dnl on Solaris 2.5.1 to run on Solaris 2.6).
  AC_CHECK_DECLS([feof_unlocked, fgets_unlocked], , , [#include <stdio.h>])

  AM_ICONV

  dnl intl/plural.c is generated from intl/plural.y. It requires bison,
  dnl because plural.y uses bison specific features. It requires at least
  dnl bison-2.7 for %define api.pure.
  dnl bison is only needed for the maintainer (who touches plural.y). But in
  dnl order to avoid separate Makefiles or --enable-maintainer-mode, we put
  dnl the rule in general Makefile. Now, some people carelessly touch the
  dnl files or have a broken "make" program, hence the plural.c rule will
  dnl sometimes fire. To avoid an error, defines BISON to ":" if it is not
  dnl present or too old.
  AC_CHECK_PROGS([INTLBISON], [bison])
  if test -z "$INTLBISON"; then
    ac_verc_fail=yes
  else
    dnl Found it, now check the version.
    AC_MSG_CHECKING([version of bison])
changequote(<<,>>)dnl
    ac_prog_version=`$INTLBISON --version 2>&1 | sed -n 's/^.*GNU Bison.* \([0-9]*\.[0-9.]*\).*$/\1/p'`
    case $ac_prog_version in
      '') ac_prog_version="v. ?.??, bad"; ac_verc_fail=yes;;
      2.[7-9]* | [3-9].*)
changequote([,])dnl
         ac_prog_version="$ac_prog_version, ok"; ac_verc_fail=no;;
      *) ac_prog_version="$ac_prog_version, bad"; ac_verc_fail=yes;;
    esac
    AC_MSG_RESULT([$ac_prog_version])
  fi
  if test $ac_verc_fail = yes; then
    INTLBISON=:
  fi
])

dnl Copies _GL_UNUSED and _GL_ATTRIBUTE_PURE definitions from
dnl gnulib-common.m4 as a fallback, if the project isn't using Gnulib.
AC_DEFUN([gt_GL_ATTRIBUTE], [
  m4_ifndef([gl_[]COMMON],
    AH_VERBATIM([gt_gl_attribute],
[/* Define as a marker that can be attached to declarations that might not
    be used.  This helps to reduce warnings, such as from
    GCC -Wunused-parameter.  */
#ifndef _GL_UNUSED
# if __GNUC__ >= 3 || (__GNUC__ == 2 && __GNUC_MINOR__ >= 7)
#  define _GL_UNUSED __attribute__ ((__unused__))
# else
#  define _GL_UNUSED
# endif
#endif

/* The __pure__ attribute was added in gcc 2.96.  */
#ifndef _GL_ATTRIBUTE_PURE
# if __GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ >= 96)
#  define _GL_ATTRIBUTE_PURE __attribute__ ((__pure__))
# else
#  define _GL_ATTRIBUTE_PURE /* empty */
# endif
#endif
]))])

# intlmacosx.m4 serial 5 (gettext-0.18.2)
dnl Copyright (C) 2004-2014, 2016 Free Software Foundation, Inc.
dnl This file is free software; the Free Software Foundation
dnl gives unlimited permission to copy and/or distribute it,
dnl with or without modifications, as long as this notice is preserved.
dnl
dnl This file can be used in projects which are not available under
dnl the GNU General Public License or the GNU Library General Public
dnl License but which still want to provide support for the GNU gettext
dnl functionality.
dnl Please note that the actual code of the GNU gettext library is covered
dnl by the GNU Library General Public License, and the rest of the GNU
dnl gettext package is covered by the GNU General Public License.
dnl They are *not* in the public domain.

dnl Checks for special options needed on Mac OS X.
dnl Defines INTL_MACOSX_LIBS.
AC_DEFUN([gt_INTL_MACOSX],
[
  dnl Check for API introduced in Mac OS X 10.2.
  AC_CACHE_CHECK([for CFPreferencesCopyAppValue],
    [gt_cv_func_CFPreferencesCopyAppValue],
    [gt_save_LIBS="$LIBS"
     LIBS="$LIBS -Wl,-framework -Wl,CoreFoundation"
     AC_LINK_IFELSE(
       [AC_LANG_PROGRAM(
          [[#include <CoreFoundation/CFPreferences.h>]],
          [[CFPreferencesCopyAppValue(NULL, NULL)]])],
       [gt_cv_func_CFPreferencesCopyAppValue=yes],
       [gt_cv_func_CFPreferencesCopyAppValue=no])
     LIBS="$gt_save_LIBS"])
  if test $gt_cv_func_CFPreferencesCopyAppValue = yes; then
    AC_DEFINE([HAVE_CFPREFERENCESCOPYAPPVALUE], [1],
      [Define to 1 if you have the Mac OS X function CFPreferencesCopyAppValue in the CoreFoundation framework.])
  fi
  dnl Check for API introduced in Mac OS X 10.3.
  AC_CACHE_CHECK([for CFLocaleCopyCurrent], [gt_cv_func_CFLocaleCopyCurrent],
    [gt_save_LIBS="$LIBS"
     LIBS="$LIBS -Wl,-framework -Wl,CoreFoundation"
     AC_LINK_IFELSE(
       [AC_LANG_PROGRAM(
          [[#include <CoreFoundation/CFLocale.h>]],
          [[CFLocaleCopyCurrent();]])],
       [gt_cv_func_CFLocaleCopyCurrent=yes],
       [gt_cv_func_CFLocaleCopyCurrent=no])
     LIBS="$gt_save_LIBS"])
  if test $gt_cv_func_CFLocaleCopyCurrent = yes; then
    AC_DEFINE([HAVE_CFLOCALECOPYCURRENT], [1],
      [Define to 1 if you have the Mac OS X function CFLocaleCopyCurrent in the CoreFoundation framework.])
  fi
  INTL_MACOSX_LIBS=
  if test $gt_cv_func_CFPreferencesCopyAppValue = yes || test $gt_cv_func_CFLocaleCopyCurrent = yes; then
    INTL_MACOSX_LIBS="-Wl,-framework -Wl,CoreFoundation"
  fi
  AC_SUBST([INTL_MACOSX_LIBS])
])

# intmax.m4 serial 6 (gettext-0.18.2)
dnl Copyright (C) 2002-2005, 2008-2016 Free Software Foundation, Inc.
dnl This file is free software; the Free Software Foundation
dnl gives unlimited permission to copy and/or distribute it,
dnl with or without modifications, as long as this notice is preserved.

dnl From Bruno Haible.
dnl Test whether the system has the 'intmax_t' type, but don't attempt to
dnl find a replacement if it is lacking.

AC_DEFUN([gt_TYPE_INTMAX_T],
[
  AC_REQUIRE([gl_AC_HEADER_INTTYPES_H])
  AC_REQUIRE([gl_AC_HEADER_STDINT_H])
  AC_CACHE_CHECK([for intmax_t], [gt_cv_c_intmax_t],
    [AC_COMPILE_IFELSE(
       [AC_LANG_PROGRAM(
          [[
#include <stddef.h>
#include <stdlib.h>
#if HAVE_STDINT_H_WITH_UINTMAX
#include <stdint.h>
#endif
#if HAVE_INTTYPES_H_WITH_UINTMAX
#include <inttypes.h>
#endif
          ]],
          [[intmax_t x = -1;
            return !x;]])],
       [gt_cv_c_intmax_t=yes],
       [gt_cv_c_intmax_t=no])])
  if test $gt_cv_c_intmax_t = yes; then
    AC_DEFINE([HAVE_INTMAX_T], [1],
      [Define if you have the 'intmax_t' type in <stdint.h> or <inttypes.h>.])
  fi
])

# inttypes-pri.m4 serial 7 (gettext-0.18.2)
dnl Copyright (C) 1997-2002, 2006, 2008-2016 Free Software Foundation, Inc.
dnl This file is free software; the Free Software Foundation
dnl gives unlimited permission to copy and/or distribute it,
dnl with or without modifications, as long as this notice is preserved.

dnl From Bruno Haible.

AC_PREREQ([2.53])

# Define PRI_MACROS_BROKEN if <inttypes.h> exists and defines the PRI*
# macros to non-string values.  This is the case on AIX 4.3.3.

AC_DEFUN([gt_INTTYPES_PRI],
[
  AC_CHECK_HEADERS([inttypes.h])
  if test $ac_cv_header_inttypes_h = yes; then
    AC_CACHE_CHECK([whether the inttypes.h PRIxNN macros are broken],
      [gt_cv_inttypes_pri_broken],
      [
        AC_COMPILE_IFELSE(
          [AC_LANG_PROGRAM(
             [[
#include <inttypes.h>
#ifdef PRId32
char *p = PRId32;
#endif
             ]],
             [[]])],
          [gt_cv_inttypes_pri_broken=no],
          [gt_cv_inttypes_pri_broken=yes])
      ])
  fi
  if test "$gt_cv_inttypes_pri_broken" = yes; then
    AC_DEFINE_UNQUOTED([PRI_MACROS_BROKEN], [1],
      [Define if <inttypes.h> exists and defines unusable PRI* macros.])
    PRI_MACROS_BROKEN=1
  else
    PRI_MACROS_BROKEN=0
  fi
  AC_SUBST([PRI_MACROS_BROKEN])
])

# inttypes_h.m4 serial 10
dnl Copyright (C) 1997-2004, 2006, 2008-2016 Free Software Foundation, Inc.
dnl This file is free software; the Free Software Foundation
dnl gives unlimited permission to copy and/or distribute it,
dnl with or without modifications, as long as this notice is preserved.

dnl From Paul Eggert.

# Define HAVE_INTTYPES_H_WITH_UINTMAX if <inttypes.h> exists,
# doesn't clash with <sys/types.h>, and declares uintmax_t.

AC_DEFUN([gl_AC_HEADER_INTTYPES_H],
[
  AC_CACHE_CHECK([for inttypes.h], [gl_cv_header_inttypes_h],
    [AC_COMPILE_IFELSE(
       [AC_LANG_PROGRAM(
          [[
#include <sys/types.h>
#include <inttypes.h>
          ]],
          [[uintmax_t i = (uintmax_t) -1; return !i;]])],
       [gl_cv_header_inttypes_h=yes],
       [gl_cv_header_inttypes_h=no])])
  if test $gl_cv_header_inttypes_h = yes; then
    AC_DEFINE_UNQUOTED([HAVE_INTTYPES_H_WITH_UINTMAX], [1],
      [Define if <inttypes.h> exists, doesn't clash with <sys/types.h>,
       and declares uintmax_t. ])
  fi
])

# lcmessage.m4 serial 7 (gettext-0.18.2)
dnl Copyright (C) 1995-2002, 2004-2005, 2008-2014, 2016 Free Software
dnl Foundation, Inc.
dnl This file is free software; the Free Software Foundation
dnl gives unlimited permission to copy and/or distribute it,
dnl with or without modifications, as long as this notice is preserved.
dnl
dnl This file can be used in projects which are not available under
dnl the GNU General Public License or the GNU Library General Public
dnl License but which still want to provide support for the GNU gettext
dnl functionality.
dnl Please note that the actual code of the GNU gettext library is covered
dnl by the GNU Library General Public License, and the rest of the GNU
dnl gettext package is covered by the GNU General Public License.
dnl They are *not* in the public domain.

dnl Authors:
dnl   Ulrich Drepper <drepper@cygnus.com>, 1995.

# Check whether LC_MESSAGES is available in <locale.h>.

AC_DEFUN([gt_LC_MESSAGES],
[
  AC_CACHE_CHECK([for LC_MESSAGES], [gt_cv_val_LC_MESSAGES],
    [AC_LINK_IFELSE(
       [AC_LANG_PROGRAM(
          [[#include <locale.h>]],
          [[return LC_MESSAGES]])],
       [gt_cv_val_LC_MESSAGES=yes],
       [gt_cv_val_LC_MESSAGES=no])])
  if test $gt_cv_val_LC_MESSAGES = yes; then
    AC_DEFINE([HAVE_LC_MESSAGES], [1],
      [Define if your <locale.h> file defines LC_MESSAGES.])
  fi
])

# lib-ld.m4 serial 6
dnl Copyright (C) 1996-2003, 2009-2016 Free Software Foundation, Inc.
dnl This file is free software; the Free Software Foundation
dnl gives unlimited permission to copy and/or distribute it,
dnl with or without modifications, as long as this notice is preserved.

dnl Subroutines of libtool.m4,
dnl with replacements s/_*LT_PATH/AC_LIB_PROG/ and s/lt_/acl_/ to avoid
dnl collision with libtool.m4.

dnl From libtool-2.4. Sets the variable with_gnu_ld to yes or no.
AC_DEFUN([AC_LIB_PROG_LD_GNU],
[AC_CACHE_CHECK([if the linker ($LD) is GNU ld], [acl_cv_prog_gnu_ld],
[# I'd rather use --version here, but apparently some GNU lds only accept -v.
case `$LD -v 2>&1 </dev/null` in
*GNU* | *'with BFD'*)
  acl_cv_prog_gnu_ld=yes
  ;;
*)
  acl_cv_prog_gnu_ld=no
  ;;
esac])
with_gnu_ld=$acl_cv_prog_gnu_ld
])

dnl From libtool-2.4. Sets the variable LD.
AC_DEFUN([AC_LIB_PROG_LD],
[AC_REQUIRE([AC_PROG_CC])dnl
AC_REQUIRE([AC_CANONICAL_HOST])dnl

AC_ARG_WITH([gnu-ld],
    [AS_HELP_STRING([--with-gnu-ld],
        [assume the C compiler uses GNU ld [default=no]])],
    [test "$withval" = no || with_gnu_ld=yes],
    [with_gnu_ld=no])dnl

# Prepare PATH_SEPARATOR.
# The user is always right.
if test "${PATH_SEPARATOR+set}" != set; then
  # Determine PATH_SEPARATOR by trying to find /bin/sh in a PATH which
  # contains only /bin. Note that ksh looks also at the FPATH variable,
  # so we have to set that as well for the test.
  PATH_SEPARATOR=:
  (PATH='/bin;/bin'; FPATH=$PATH; sh -c :) >/dev/null 2>&1 \
    && { (PATH='/bin:/bin'; FPATH=$PATH; sh -c :) >/dev/null 2>&1 \
           || PATH_SEPARATOR=';'
       }
fi

ac_prog=ld
if test "$GCC" = yes; then
  # Check if gcc -print-prog-name=ld gives a path.
  AC_MSG_CHECKING([for ld used by $CC])
  case $host in
  *-*-mingw*)
    # gcc leaves a trailing carriage return which upsets mingw
    ac_prog=`($CC -print-prog-name=ld) 2>&5 | tr -d '\015'` ;;
  *)
    ac_prog=`($CC -print-prog-name=ld) 2>&5` ;;
  esac
  case $ac_prog in
    # Accept absolute paths.
    [[\\/]]* | ?:[[\\/]]*)
      re_direlt='/[[^/]][[^/]]*/\.\./'
      # Canonicalize the pathname of ld
      ac_prog=`echo "$ac_prog"| sed 's%\\\\%/%g'`
      while echo "$ac_prog" | grep "$re_direlt" > /dev/null 2>&1; do
        ac_prog=`echo $ac_prog| sed "s%$re_direlt%/%"`
      done
      test -z "$LD" && LD="$ac_prog"
      ;;
  "")
    # If it fails, then pretend we aren't using GCC.
    ac_prog=ld
    ;;
  *)
    # If it is relative, then search for the first ld in PATH.
    with_gnu_ld=unknown
    ;;
  esac
elif test "$with_gnu_ld" = yes; then
  AC_MSG_CHECKING([for GNU ld])
else
  AC_MSG_CHECKING([for non-GNU ld])
fi
AC_CACHE_VAL([acl_cv_path_LD],
[if test -z "$LD"; then
  acl_save_ifs="$IFS"; IFS=$PATH_SEPARATOR
  for ac_dir in $PATH; do
    IFS="$acl_save_ifs"
    test -z "$ac_dir" && ac_dir=.
    if test -f "$ac_dir/$ac_prog" || test -f "$ac_dir/$ac_prog$ac_exeext"; then
      acl_cv_path_LD="$ac_dir/$ac_prog"
      # Check to see if the program is GNU ld.  I'd rather use --version,
      # but apparently some variants of GNU ld only accept -v.
      # Break only if it was the GNU/non-GNU ld that we prefer.
      case `"$acl_cv_path_LD" -v 2>&1 </dev/null` in
      *GNU* | *'with BFD'*)
        test "$with_gnu_ld" != no && break
        ;;
      *)
        test "$with_gnu_ld" != yes && break
        ;;
      esac
    fi
  done
  IFS="$acl_save_ifs"
else
  acl_cv_path_LD="$LD" # Let the user override the test with a path.
fi])
LD="$acl_cv_path_LD"
if test -n "$LD"; then
  AC_MSG_RESULT([$LD])
else
  AC_MSG_RESULT([no])
fi
test -z "$LD" && AC_MSG_ERROR([no acceptable ld found in \$PATH])
AC_LIB_PROG_LD_GNU
])

# lib-link.m4 serial 26 (gettext-0.18.2)
dnl Copyright (C) 2001-2016 Free Software Foundation, Inc.
dnl This file is free software; the Free Software Foundation
dnl gives unlimited permission to copy and/or distribute it,
dnl with or without modifications, as long as this notice is preserved.

dnl From Bruno Haible.

AC_PREREQ([2.54])

dnl AC_LIB_LINKFLAGS(name [, dependencies]) searches for libname and
dnl the libraries corresponding to explicit and implicit dependencies.
dnl Sets and AC_SUBSTs the LIB${NAME} and LTLIB${NAME} variables and
dnl augments the CPPFLAGS variable.
dnl Sets and AC_SUBSTs the LIB${NAME}_PREFIX variable to nonempty if libname
dnl was found in ${LIB${NAME}_PREFIX}/$acl_libdirstem.
AC_DEFUN([AC_LIB_LINKFLAGS],
[
  AC_REQUIRE([AC_LIB_PREPARE_PREFIX])
  AC_REQUIRE([AC_LIB_RPATH])
  pushdef([Name],[m4_translit([$1],[./+-], [____])])
  pushdef([NAME],[m4_translit([$1],[abcdefghijklmnopqrstuvwxyz./+-],
                                   [ABCDEFGHIJKLMNOPQRSTUVWXYZ____])])
  AC_CACHE_CHECK([how to link with lib[]$1], [ac_cv_lib[]Name[]_libs], [
    AC_LIB_LINKFLAGS_BODY([$1], [$2])
    ac_cv_lib[]Name[]_libs="$LIB[]NAME"
    ac_cv_lib[]Name[]_ltlibs="$LTLIB[]NAME"
    ac_cv_lib[]Name[]_cppflags="$INC[]NAME"
    ac_cv_lib[]Name[]_prefix="$LIB[]NAME[]_PREFIX"
  ])
  LIB[]NAME="$ac_cv_lib[]Name[]_libs"
  LTLIB[]NAME="$ac_cv_lib[]Name[]_ltlibs"
  INC[]NAME="$ac_cv_lib[]Name[]_cppflags"
  LIB[]NAME[]_PREFIX="$ac_cv_lib[]Name[]_prefix"
  AC_LIB_APPENDTOVAR([CPPFLAGS], [$INC]NAME)
  AC_SUBST([LIB]NAME)
  AC_SUBST([LTLIB]NAME)
  AC_SUBST([LIB]NAME[_PREFIX])
  dnl Also set HAVE_LIB[]NAME so that AC_LIB_HAVE_LINKFLAGS can reuse the
  dnl results of this search when this library appears as a dependency.
  HAVE_LIB[]NAME=yes
  popdef([NAME])
  popdef([Name])
])

dnl AC_LIB_HAVE_LINKFLAGS(name, dependencies, includes, testcode, [missing-message])
dnl searches for libname and the libraries corresponding to explicit and
dnl implicit dependencies, together with the specified include files and
dnl the ability to compile and link the specified testcode. The missing-message
dnl defaults to 'no' and may contain additional hints for the user.
dnl If found, it sets and AC_SUBSTs HAVE_LIB${NAME}=yes and the LIB${NAME}
dnl and LTLIB${NAME} variables and augments the CPPFLAGS variable, and
dnl #defines HAVE_LIB${NAME} to 1. Otherwise, it sets and AC_SUBSTs
dnl HAVE_LIB${NAME}=no and LIB${NAME} and LTLIB${NAME} to empty.
dnl Sets and AC_SUBSTs the LIB${NAME}_PREFIX variable to nonempty if libname
dnl was found in ${LIB${NAME}_PREFIX}/$acl_libdirstem.
AC_DEFUN([AC_LIB_HAVE_LINKFLAGS],
[
  AC_REQUIRE([AC_LIB_PREPARE_PREFIX])
  AC_REQUIRE([AC_LIB_RPATH])
  pushdef([Name],[m4_translit([$1],[./+-], [____])])
  pushdef([NAME],[m4_translit([$1],[abcdefghijklmnopqrstuvwxyz./+-],
                                   [ABCDEFGHIJKLMNOPQRSTUVWXYZ____])])

  dnl Search for lib[]Name and define LIB[]NAME, LTLIB[]NAME and INC[]NAME
  dnl accordingly.
  AC_LIB_LINKFLAGS_BODY([$1], [$2])

  dnl Add $INC[]NAME to CPPFLAGS before performing the following checks,
  dnl because if the user has installed lib[]Name and not disabled its use
  dnl via --without-lib[]Name-prefix, he wants to use it.
  ac_save_CPPFLAGS="$CPPFLAGS"
  AC_LIB_APPENDTOVAR([CPPFLAGS], [$INC]NAME)

  AC_CACHE_CHECK([for lib[]$1], [ac_cv_lib[]Name], [
    ac_save_LIBS="$LIBS"
    dnl If $LIB[]NAME contains some -l options, add it to the end of LIBS,
    dnl because these -l options might require -L options that are present in
    dnl LIBS. -l options benefit only from the -L options listed before it.
    dnl Otherwise, add it to the front of LIBS, because it may be a static
    dnl library that depends on another static library that is present in LIBS.
    dnl Static libraries benefit only from the static libraries listed after
    dnl it.
    case " $LIB[]NAME" in
      *" -l"*) LIBS="$LIBS $LIB[]NAME" ;;
      *)       LIBS="$LIB[]NAME $LIBS" ;;
    esac
    AC_LINK_IFELSE(
      [AC_LANG_PROGRAM([[$3]], [[$4]])],
      [ac_cv_lib[]Name=yes],
      [ac_cv_lib[]Name='m4_if([$5], [], [no], [[$5]])'])
    LIBS="$ac_save_LIBS"
  ])
  if test "$ac_cv_lib[]Name" = yes; then
    HAVE_LIB[]NAME=yes
    AC_DEFINE([HAVE_LIB]NAME, 1, [Define if you have the lib][$1 library.])
    AC_MSG_CHECKING([how to link with lib[]$1])
    AC_MSG_RESULT([$LIB[]NAME])
  else
    HAVE_LIB[]NAME=no
    dnl If $LIB[]NAME didn't lead to a usable library, we don't need
    dnl $INC[]NAME either.
    CPPFLAGS="$ac_save_CPPFLAGS"
    LIB[]NAME=
    LTLIB[]NAME=
    LIB[]NAME[]_PREFIX=
  fi
  AC_SUBST([HAVE_LIB]NAME)
  AC_SUBST([LIB]NAME)
  AC_SUBST([LTLIB]NAME)
  AC_SUBST([LIB]NAME[_PREFIX])
  popdef([NAME])
  popdef([Name])
])

dnl Determine the platform dependent parameters needed to use rpath:
dnl   acl_libext,
dnl   acl_shlibext,
dnl   acl_libname_spec,
dnl   acl_library_names_spec,
dnl   acl_hardcode_libdir_flag_spec,
dnl   acl_hardcode_libdir_separator,
dnl   acl_hardcode_direct,
dnl   acl_hardcode_minus_L.
AC_DEFUN([AC_LIB_RPATH],
[
  dnl Tell automake >= 1.10 to complain if config.rpath is missing.
  m4_ifdef([AC_REQUIRE_AUX_FILE], [AC_REQUIRE_AUX_FILE([config.rpath])])
  AC_REQUIRE([AC_PROG_CC])                dnl we use $CC, $GCC, $LDFLAGS
  AC_REQUIRE([AC_LIB_PROG_LD])            dnl we use $LD, $with_gnu_ld
  AC_REQUIRE([AC_CANONICAL_HOST])         dnl we use $host
  AC_REQUIRE([AC_CONFIG_AUX_DIR_DEFAULT]) dnl we use $ac_aux_dir
  AC_CACHE_CHECK([for shared library run path origin], [acl_cv_rpath], [
    CC="$CC" GCC="$GCC" LDFLAGS="$LDFLAGS" LD="$LD" with_gnu_ld="$with_gnu_ld" \
    ${CONFIG_SHELL-/bin/sh} "$ac_aux_dir/config.rpath" "$host" > conftest.sh
    . ./conftest.sh
    rm -f ./conftest.sh
    acl_cv_rpath=done
  ])
  wl="$acl_cv_wl"
  acl_libext="$acl_cv_libext"
  acl_shlibext="$acl_cv_shlibext"
  acl_libname_spec="$acl_cv_libname_spec"
  acl_library_names_spec="$acl_cv_library_names_spec"
  acl_hardcode_libdir_flag_spec="$acl_cv_hardcode_libdir_flag_spec"
  acl_hardcode_libdir_separator="$acl_cv_hardcode_libdir_separator"
  acl_hardcode_direct="$acl_cv_hardcode_direct"
  acl_hardcode_minus_L="$acl_cv_hardcode_minus_L"
  dnl Determine whether the user wants rpath handling at all.
  AC_ARG_ENABLE([rpath],
    [  --disable-rpath         do not hardcode runtime library paths],
    :, enable_rpath=yes)
])

dnl AC_LIB_FROMPACKAGE(name, package)
dnl declares that libname comes from the given package. The configure file
dnl will then not have a --with-libname-prefix option but a
dnl --with-package-prefix option. Several libraries can come from the same
dnl package. This declaration must occur before an AC_LIB_LINKFLAGS or similar
dnl macro call that searches for libname.
AC_DEFUN([AC_LIB_FROMPACKAGE],
[
  pushdef([NAME],[m4_translit([$1],[abcdefghijklmnopqrstuvwxyz./+-],
                                   [ABCDEFGHIJKLMNOPQRSTUVWXYZ____])])
  define([acl_frompackage_]NAME, [$2])
  popdef([NAME])
  pushdef([PACK],[$2])
  pushdef([PACKUP],[m4_translit(PACK,[abcdefghijklmnopqrstuvwxyz./+-],
                                     [ABCDEFGHIJKLMNOPQRSTUVWXYZ____])])
  define([acl_libsinpackage_]PACKUP,
    m4_ifdef([acl_libsinpackage_]PACKUP, [m4_defn([acl_libsinpackage_]PACKUP)[, ]],)[lib$1])
  popdef([PACKUP])
  popdef([PACK])
])

dnl AC_LIB_LINKFLAGS_BODY(name [, dependencies]) searches for libname and
dnl the libraries corresponding to explicit and implicit dependencies.
dnl Sets the LIB${NAME}, LTLIB${NAME} and INC${NAME} variables.
dnl Also, sets the LIB${NAME}_PREFIX variable to nonempty if libname was found
dnl in ${LIB${NAME}_PREFIX}/$acl_libdirstem.
AC_DEFUN([AC_LIB_LINKFLAGS_BODY],
[
  AC_REQUIRE([AC_LIB_PREPARE_MULTILIB])
  pushdef([NAME],[m4_translit([$1],[abcdefghijklmnopqrstuvwxyz./+-],
                                   [ABCDEFGHIJKLMNOPQRSTUVWXYZ____])])
  pushdef([PACK],[m4_ifdef([acl_frompackage_]NAME, [acl_frompackage_]NAME, lib[$1])])
  pushdef([PACKUP],[m4_translit(PACK,[abcdefghijklmnopqrstuvwxyz./+-],
                                     [ABCDEFGHIJKLMNOPQRSTUVWXYZ____])])
  pushdef([PACKLIBS],[m4_ifdef([acl_frompackage_]NAME, [acl_libsinpackage_]PACKUP, lib[$1])])
  dnl Autoconf >= 2.61 supports dots in --with options.
  pushdef([P_A_C_K],[m4_if(m4_version_compare(m4_defn([m4_PACKAGE_VERSION]),[2.61]),[-1],[m4_translit(PACK,[.],[_])],PACK)])
  dnl By default, look in $includedir and $libdir.
  use_additional=yes
  AC_LIB_WITH_FINAL_PREFIX([
    eval additional_includedir=\"$includedir\"
    eval additional_libdir=\"$libdir\"
  ])
  AC_ARG_WITH(P_A_C_K[-prefix],
[[  --with-]]P_A_C_K[[-prefix[=DIR]  search for ]PACKLIBS[ in DIR/include and DIR/lib
  --without-]]P_A_C_K[[-prefix     don't search for ]PACKLIBS[ in includedir and libdir]],
[
    if test "X$withval" = "Xno"; then
      use_additional=no
    else
      if test "X$withval" = "X"; then
        AC_LIB_WITH_FINAL_PREFIX([
          eval additional_includedir=\"$includedir\"
          eval additional_libdir=\"$libdir\"
        ])
      else
        additional_includedir="$withval/include"
        additional_libdir="$withval/$acl_libdirstem"
        if test "$acl_libdirstem2" != "$acl_libdirstem" \
           && ! test -d "$withval/$acl_libdirstem"; then
          additional_libdir="$withval/$acl_libdirstem2"
        fi
      fi
    fi
])
  dnl Search the library and its dependencies in $additional_libdir and
  dnl $LDFLAGS. Using breadth-first-seach.
  LIB[]NAME=
  LTLIB[]NAME=
  INC[]NAME=
  LIB[]NAME[]_PREFIX=
  dnl HAVE_LIB${NAME} is an indicator that LIB${NAME}, LTLIB${NAME} have been
  dnl computed. So it has to be reset here.
  HAVE_LIB[]NAME=
  rpathdirs=
  ltrpathdirs=
  names_already_handled=
  names_next_round='$1 $2'
  while test -n "$names_next_round"; do
    names_this_round="$names_next_round"
    names_next_round=
    for name in $names_this_round; do
      already_handled=
      for n in $names_already_handled; do
        if test "$n" = "$name"; then
          already_handled=yes
          break
        fi
      done
      if test -z "$already_handled"; then
        names_already_handled="$names_already_handled $name"
        dnl See if it was already located by an earlier AC_LIB_LINKFLAGS
        dnl or AC_LIB_HAVE_LINKFLAGS call.
        uppername=`echo "$name" | sed -e 'y|abcdefghijklmnopqrstuvwxyz./+-|ABCDEFGHIJKLMNOPQRSTUVWXYZ____|'`
        eval value=\"\$HAVE_LIB$uppername\"
        if test -n "$value"; then
          if test "$value" = yes; then
            eval value=\"\$LIB$uppername\"
            test -z "$value" || LIB[]NAME="${LIB[]NAME}${LIB[]NAME:+ }$value"
            eval value=\"\$LTLIB$uppername\"
            test -z "$value" || LTLIB[]NAME="${LTLIB[]NAME}${LTLIB[]NAME:+ }$value"
          else
            dnl An earlier call to AC_LIB_HAVE_LINKFLAGS has determined
            dnl that this library doesn't exist. So just drop it.
            :
          fi
        else
          dnl Search the library lib$name in $additional_libdir and $LDFLAGS
          dnl and the already constructed $LIBNAME/$LTLIBNAME.
          found_dir=
          found_la=
          found_so=
          found_a=
          eval libname=\"$acl_libname_spec\"    # typically: libname=lib$name
          if test -n "$acl_shlibext"; then
            shrext=".$acl_shlibext"             # typically: shrext=.so
          else
            shrext=
          fi
          if test $use_additional = yes; then
            dir="$additional_libdir"
            dnl The same code as in the loop below:
            dnl First look for a shared library.
            if test -n "$acl_shlibext"; then
              if test -f "$dir/$libname$shrext"; then
                found_dir="$dir"
                found_so="$dir/$libname$shrext"
              else
                if test "$acl_library_names_spec" = '$libname$shrext$versuffix'; then
                  ver=`(cd "$dir" && \
                        for f in "$libname$shrext".*; do echo "$f"; done \
                        | sed -e "s,^$libname$shrext\\\\.,," \
                        | sort -t '.' -n -r -k1,1 -k2,2 -k3,3 -k4,4 -k5,5 \
                        | sed 1q ) 2>/dev/null`
                  if test -n "$ver" && test -f "$dir/$libname$shrext.$ver"; then
                    found_dir="$dir"
                    found_so="$dir/$libname$shrext.$ver"
                  fi
                else
                  eval library_names=\"$acl_library_names_spec\"
                  for f in $library_names; do
                    if test -f "$dir/$f"; then
                      found_dir="$dir"
                      found_so="$dir/$f"
                      break
                    fi
                  done
                fi
              fi
            fi
            dnl Then look for a static library.
            if test "X$found_dir" = "X"; then
              if test -f "$dir/$libname.$acl_libext"; then
                found_dir="$dir"
                found_a="$dir/$libname.$acl_libext"
              fi
            fi
            if test "X$found_dir" != "X"; then
              if test -f "$dir/$libname.la"; then
                found_la="$dir/$libname.la"
              fi
            fi
          fi
          if test "X$found_dir" = "X"; then
            for x in $LDFLAGS $LTLIB[]NAME; do
              AC_LIB_WITH_FINAL_PREFIX([eval x=\"$x\"])
              case "$x" in
                -L*)
                  dir=`echo "X$x" | sed -e 's/^X-L//'`
                  dnl First look for a shared library.
                  if test -n "$acl_shlibext"; then
                    if test -f "$dir/$libname$shrext"; then
                      found_dir="$dir"
                      found_so="$dir/$libname$shrext"
                    else
                      if test "$acl_library_names_spec" = '$libname$shrext$versuffix'; then
                        ver=`(cd "$dir" && \
                              for f in "$libname$shrext".*; do echo "$f"; done \
                              | sed -e "s,^$libname$shrext\\\\.,," \
                              | sort -t '.' -n -r -k1,1 -k2,2 -k3,3 -k4,4 -k5,5 \
                              | sed 1q ) 2>/dev/null`
                        if test -n "$ver" && test -f "$dir/$libname$shrext.$ver"; then
                          found_dir="$dir"
                          found_so="$dir/$libname$shrext.$ver"
                        fi
                      else
                        eval library_names=\"$acl_library_names_spec\"
                        for f in $library_names; do
                          if test -f "$dir/$f"; then
                            found_dir="$dir"
                            found_so="$dir/$f"
                            break
                          fi
                        done
                      fi
                    fi
                  fi
                  dnl Then look for a static library.
                  if test "X$found_dir" = "X"; then
                    if test -f "$dir/$libname.$acl_libext"; then
                      found_dir="$dir"
                      found_a="$dir/$libname.$acl_libext"
                    fi
                  fi
                  if test "X$found_dir" != "X"; then
                    if test -f "$dir/$libname.la"; then
                      found_la="$dir/$libname.la"
                    fi
                  fi
                  ;;
              esac
              if test "X$found_dir" != "X"; then
                break
              fi
            done
          fi
          if test "X$found_dir" != "X"; then
            dnl Found the library.
            LTLIB[]NAME="${LTLIB[]NAME}${LTLIB[]NAME:+ }-L$found_dir -l$name"
            if test "X$found_so" != "X"; then
              dnl Linking with a shared library. We attempt to hardcode its
              dnl directory into the executable's runpath, unless it's the
              dnl standard /usr/lib.
              if test "$enable_rpath" = no \
                 || test "X$found_dir" = "X/usr/$acl_libdirstem" \
                 || test "X$found_dir" = "X/usr/$acl_libdirstem2"; then
                dnl No hardcoding is needed.
                LIB[]NAME="${LIB[]NAME}${LIB[]NAME:+ }$found_so"
              else
                dnl Use an explicit option to hardcode DIR into the resulting
                dnl binary.
                dnl Potentially add DIR to ltrpathdirs.
                dnl The ltrpathdirs will be appended to $LTLIBNAME at the end.
                haveit=
                for x in $ltrpathdirs; do
                  if test "X$x" = "X$found_dir"; then
                    haveit=yes
                    break
                  fi
                done
                if test -z "$haveit"; then
                  ltrpathdirs="$ltrpathdirs $found_dir"
                fi
                dnl The hardcoding into $LIBNAME is system dependent.
                if test "$acl_hardcode_direct" = yes; then
                  dnl Using DIR/libNAME.so during linking hardcodes DIR into the
                  dnl resulting binary.
                  LIB[]NAME="${LIB[]NAME}${LIB[]NAME:+ }$found_so"
                else
                  if test -n "$acl_hardcode_libdir_flag_spec" && test "$acl_hardcode_minus_L" = no; then
                    dnl Use an explicit option to hardcode DIR into the resulting
                    dnl binary.
                    LIB[]NAME="${LIB[]NAME}${LIB[]NAME:+ }$found_so"
                    dnl Potentially add DIR to rpathdirs.
                    dnl The rpathdirs will be appended to $LIBNAME at the end.
                    haveit=
                    for x in $rpathdirs; do
                      if test "X$x" = "X$found_dir"; then
                        haveit=yes
                        break
                      fi
                    done
                    if test -z "$haveit"; then
                      rpathdirs="$rpathdirs $found_dir"
                    fi
                  else
                    dnl Rely on "-L$found_dir".
                    dnl But don't add it if it's already contained in the LDFLAGS
                    dnl or the already constructed $LIBNAME
                    haveit=
                    for x in $LDFLAGS $LIB[]NAME; do
                      AC_LIB_WITH_FINAL_PREFIX([eval x=\"$x\"])
                      if test "X$x" = "X-L$found_dir"; then
                        haveit=yes
                        break
                      fi
                    done
                    if test -z "$haveit"; then
                      LIB[]NAME="${LIB[]NAME}${LIB[]NAME:+ }-L$found_dir"
                    fi
                    if test "$acl_hardcode_minus_L" != no; then
                      dnl FIXME: Not sure whether we should use
                      dnl "-L$found_dir -l$name" or "-L$found_dir $found_so"
                      dnl here.
                      LIB[]NAME="${LIB[]NAME}${LIB[]NAME:+ }$found_so"
                    else
                      dnl We cannot use $acl_hardcode_runpath_var and LD_RUN_PATH
                      dnl here, because this doesn't fit in flags passed to the
                      dnl compiler. So give up. No hardcoding. This affects only
                      dnl very old systems.
                      dnl FIXME: Not sure whether we should use
                      dnl "-L$found_dir -l$name" or "-L$found_dir $found_so"
                      dnl here.
                      LIB[]NAME="${LIB[]NAME}${LIB[]NAME:+ }-l$name"
                    fi
                  fi
                fi
              fi
            else
              if test "X$found_a" != "X"; then
                dnl Linking with a static library.
                LIB[]NAME="${LIB[]NAME}${LIB[]NAME:+ }$found_a"
              else
                dnl We shouldn't come here, but anyway it's good to have a
                dnl fallback.
                LIB[]NAME="${LIB[]NAME}${LIB[]NAME:+ }-L$found_dir -l$name"
              fi
            fi
            dnl Assume the include files are nearby.
            additional_includedir=
            case "$found_dir" in
              */$acl_libdirstem | */$acl_libdirstem/)
                basedir=`echo "X$found_dir" | sed -e 's,^X,,' -e "s,/$acl_libdirstem/"'*$,,'`
                if test "$name" = '$1'; then
                  LIB[]NAME[]_PREFIX="$basedir"
                fi
                additional_includedir="$basedir/include"
                ;;
              */$acl_libdirstem2 | */$acl_libdirstem2/)
                basedir=`echo "X$found_dir" | sed -e 's,^X,,' -e "s,/$acl_libdirstem2/"'*$,,'`
                if test "$name" = '$1'; then
                  LIB[]NAME[]_PREFIX="$basedir"
                fi
                additional_includedir="$basedir/include"
                ;;
            esac
            if test "X$additional_includedir" != "X"; then
              dnl Potentially add $additional_includedir to $INCNAME.
              dnl But don't add it
              dnl   1. if it's the standard /usr/include,
              dnl   2. if it's /usr/local/include and we are using GCC on Linux,
              dnl   3. if it's already present in $CPPFLAGS or the already
              dnl      constructed $INCNAME,
              dnl   4. if it doesn't exist as a directory.
              if test "X$additional_includedir" != "X/usr/include"; then
                haveit=
                if test "X$additional_includedir" = "X/usr/local/include"; then
                  if test -n "$GCC"; then
                    case $host_os in
                      linux* | gnu* | k*bsd*-gnu) haveit=yes;;
                    esac
                  fi
                fi
                if test -z "$haveit"; then
                  for x in $CPPFLAGS $INC[]NAME; do
                    AC_LIB_WITH_FINAL_PREFIX([eval x=\"$x\"])
                    if test "X$x" = "X-I$additional_includedir"; then
                      haveit=yes
                      break
                    fi
                  done
                  if test -z "$haveit"; then
                    if test -d "$additional_includedir"; then
                      dnl Really add $additional_includedir to $INCNAME.
                      INC[]NAME="${INC[]NAME}${INC[]NAME:+ }-I$additional_includedir"
                    fi
                  fi
                fi
              fi
            fi
            dnl Look for dependencies.
            if test -n "$found_la"; then
              dnl Read the .la file. It defines the variables
              dnl dlname, library_names, old_library, dependency_libs, current,
              dnl age, revision, installed, dlopen, dlpreopen, libdir.
              save_libdir="$libdir"
              case "$found_la" in
                */* | *\\*) . "$found_la" ;;
                *) . "./$found_la" ;;
              esac
              libdir="$save_libdir"
              dnl We use only dependency_libs.
              for dep in $dependency_libs; do
                case "$dep" in
                  -L*)
                    additional_libdir=`echo "X$dep" | sed -e 's/^X-L//'`
                    dnl Potentially add $additional_libdir to $LIBNAME and $LTLIBNAME.
                    dnl But don't add it
                    dnl   1. if it's the standard /usr/lib,
                    dnl   2. if it's /usr/local/lib and we are using GCC on Linux,
                    dnl   3. if it's already present in $LDFLAGS or the already
                    dnl      constructed $LIBNAME,
                    dnl   4. if it doesn't exist as a directory.
                    if test "X$additional_libdir" != "X/usr/$acl_libdirstem" \
                       && test "X$additional_libdir" != "X/usr/$acl_libdirstem2"; then
                      haveit=
                      if test "X$additional_libdir" = "X/usr/local/$acl_libdirstem" \
                         || test "X$additional_libdir" = "X/usr/local/$acl_libdirstem2"; then
                        if test -n "$GCC"; then
                          case $host_os in
                            linux* | gnu* | k*bsd*-gnu) haveit=yes;;
                          esac
                        fi
                      fi
                      if test -z "$haveit"; then
                        haveit=
                        for x in $LDFLAGS $LIB[]NAME; do
                          AC_LIB_WITH_FINAL_PREFIX([eval x=\"$x\"])
                          if test "X$x" = "X-L$additional_libdir"; then
                            haveit=yes
                            break
                          fi
                        done
                        if test -z "$haveit"; then
                          if test -d "$additional_libdir"; then
                            dnl Really add $additional_libdir to $LIBNAME.
                            LIB[]NAME="${LIB[]NAME}${LIB[]NAME:+ }-L$additional_libdir"
                          fi
                        fi
                        haveit=
                        for x in $LDFLAGS $LTLIB[]NAME; do
                          AC_LIB_WITH_FINAL_PREFIX([eval x=\"$x\"])
                          if test "X$x" = "X-L$additional_libdir"; then
                            haveit=yes
                            break
                          fi
                        done
                        if test -z "$haveit"; then
                          if test -d "$additional_libdir"; then
                            dnl Really add $additional_libdir to $LTLIBNAME.
                            LTLIB[]NAME="${LTLIB[]NAME}${LTLIB[]NAME:+ }-L$additional_libdir"
                          fi
                        fi
                      fi
                    fi
                    ;;
                  -R*)
                    dir=`echo "X$dep" | sed -e 's/^X-R//'`
                    if test "$enable_rpath" != no; then
                      dnl Potentially add DIR to rpathdirs.
                      dnl The rpathdirs will be appended to $LIBNAME at the end.
                      haveit=
                      for x in $rpathdirs; do
                        if test "X$x" = "X$dir"; then
                          haveit=yes
                          break
                        fi
                      done
                      if test -z "$haveit"; then
                        rpathdirs="$rpathdirs $dir"
                      fi
                      dnl Potentially add DIR to ltrpathdirs.
                      dnl The ltrpathdirs will be appended to $LTLIBNAME at the end.
                      haveit=
                      for x in $ltrpathdirs; do
                        if test "X$x" = "X$dir"; then
                          haveit=yes
                          break
                        fi
                      done
                      if test -z "$haveit"; then
                        ltrpathdirs="$ltrpathdirs $dir"
                      fi
                    fi
                    ;;
                  -l*)
                    dnl Handle this in the next round.
                    names_next_round="$names_next_round "`echo "X$dep" | sed -e 's/^X-l//'`
                    ;;
                  *.la)
                    dnl Handle this in the next round. Throw away the .la's
                    dnl directory; it is already contained in a preceding -L
                    dnl option.
                    names_next_round="$names_next_round "`echo "X$dep" | sed -e 's,^X.*/,,' -e 's,^lib,,' -e 's,\.la$,,'`
                    ;;
                  *)
                    dnl Most likely an immediate library name.
                    LIB[]NAME="${LIB[]NAME}${LIB[]NAME:+ }$dep"
                    LTLIB[]NAME="${LTLIB[]NAME}${LTLIB[]NAME:+ }$dep"
                    ;;
                esac
              done
            fi
          else
            dnl Didn't find the library; assume it is in the system directories
            dnl known to the linker and runtime loader. (All the system
            dnl directories known to the linker should also be known to the
            dnl runtime loader, otherwise the system is severely misconfigured.)
            LIB[]NAME="${LIB[]NAME}${LIB[]NAME:+ }-l$name"
            LTLIB[]NAME="${LTLIB[]NAME}${LTLIB[]NAME:+ }-l$name"
          fi
        fi
      fi
    done
  done
  if test "X$rpathdirs" != "X"; then
    if test -n "$acl_hardcode_libdir_separator"; then
      dnl Weird platform: only the last -rpath option counts, the user must
      dnl pass all path elements in one option. We can arrange that for a
      dnl single library, but not when more than one $LIBNAMEs are used.
      alldirs=
      for found_dir in $rpathdirs; do
        alldirs="${alldirs}${alldirs:+$acl_hardcode_libdir_separator}$found_dir"
      done
      dnl Note: acl_hardcode_libdir_flag_spec uses $libdir and $wl.
      acl_save_libdir="$libdir"
      libdir="$alldirs"
      eval flag=\"$acl_hardcode_libdir_flag_spec\"
      libdir="$acl_save_libdir"
      LIB[]NAME="${LIB[]NAME}${LIB[]NAME:+ }$flag"
    else
      dnl The -rpath options are cumulative.
      for found_dir in $rpathdirs; do
        acl_save_libdir="$libdir"
        libdir="$found_dir"
        eval flag=\"$acl_hardcode_libdir_flag_spec\"
        libdir="$acl_save_libdir"
        LIB[]NAME="${LIB[]NAME}${LIB[]NAME:+ }$flag"
      done
    fi
  fi
  if test "X$ltrpathdirs" != "X"; then
    dnl When using libtool, the option that works for both libraries and
    dnl executables is -R. The -R options are cumulative.
    for found_dir in $ltrpathdirs; do
      LTLIB[]NAME="${LTLIB[]NAME}${LTLIB[]NAME:+ }-R$found_dir"
    done
  fi
  popdef([P_A_C_K])
  popdef([PACKLIBS])
  popdef([PACKUP])
  popdef([PACK])
  popdef([NAME])
])

dnl AC_LIB_APPENDTOVAR(VAR, CONTENTS) appends the elements of CONTENTS to VAR,
dnl unless already present in VAR.
dnl Works only for CPPFLAGS, not for LIB* variables because that sometimes
dnl contains two or three consecutive elements that belong together.
AC_DEFUN([AC_LIB_APPENDTOVAR],
[
  for element in [$2]; do
    haveit=
    for x in $[$1]; do
      AC_LIB_WITH_FINAL_PREFIX([eval x=\"$x\"])
      if test "X$x" = "X$element"; then
        haveit=yes
        break
      fi
    done
    if test -z "$haveit"; then
      [$1]="${[$1]}${[$1]:+ }$element"
    fi
  done
])

dnl For those cases where a variable contains several -L and -l options
dnl referring to unknown libraries and directories, this macro determines the
dnl necessary additional linker options for the runtime path.
dnl AC_LIB_LINKFLAGS_FROM_LIBS([LDADDVAR], [LIBSVALUE], [USE-LIBTOOL])
dnl sets LDADDVAR to linker options needed together with LIBSVALUE.
dnl If USE-LIBTOOL evaluates to non-empty, linking with libtool is assumed,
dnl otherwise linking without libtool is assumed.
AC_DEFUN([AC_LIB_LINKFLAGS_FROM_LIBS],
[
  AC_REQUIRE([AC_LIB_RPATH])
  AC_REQUIRE([AC_LIB_PREPARE_MULTILIB])
  $1=
  if test "$enable_rpath" != no; then
    if test -n "$acl_hardcode_libdir_flag_spec" && test "$acl_hardcode_minus_L" = no; then
      dnl Use an explicit option to hardcode directories into the resulting
      dnl binary.
      rpathdirs=
      next=
      for opt in $2; do
        if test -n "$next"; then
          dir="$next"
          dnl No need to hardcode the standard /usr/lib.
          if test "X$dir" != "X/usr/$acl_libdirstem" \
             && test "X$dir" != "X/usr/$acl_libdirstem2"; then
            rpathdirs="$rpathdirs $dir"
          fi
          next=
        else
          case $opt in
            -L) next=yes ;;
            -L*) dir=`echo "X$opt" | sed -e 's,^X-L,,'`
                 dnl No need to hardcode the standard /usr/lib.
                 if test "X$dir" != "X/usr/$acl_libdirstem" \
                    && test "X$dir" != "X/usr/$acl_libdirstem2"; then
                   rpathdirs="$rpathdirs $dir"
                 fi
                 next= ;;
            *) next= ;;
          esac
        fi
      done
      if test "X$rpathdirs" != "X"; then
        if test -n ""$3""; then
          dnl libtool is used for linking. Use -R options.
          for dir in $rpathdirs; do
            $1="${$1}${$1:+ }-R$dir"
          done
        else
          dnl The linker is used for linking directly.
          if test -n "$acl_hardcode_libdir_separator"; then
            dnl Weird platform: only the last -rpath option counts, the user
            dnl must pass all path elements in one option.
            alldirs=
            for dir in $rpathdirs; do
              alldirs="${alldirs}${alldirs:+$acl_hardcode_libdir_separator}$dir"
            done
            acl_save_libdir="$libdir"
            libdir="$alldirs"
            eval flag=\"$acl_hardcode_libdir_flag_spec\"
            libdir="$acl_save_libdir"
            $1="$flag"
          else
            dnl The -rpath options are cumulative.
            for dir in $rpathdirs; do
              acl_save_libdir="$libdir"
              libdir="$dir"
              eval flag=\"$acl_hardcode_libdir_flag_spec\"
              libdir="$acl_save_libdir"
              $1="${$1}${$1:+ }$flag"
            done
          fi
        fi
      fi
    fi
  fi
  AC_SUBST([$1])
])

# lib-prefix.m4 serial 7 (gettext-0.18)
dnl Copyright (C) 2001-2005, 2008-2016 Free Software Foundation, Inc.
dnl This file is free software; the Free Software Foundation
dnl gives unlimited permission to copy and/or distribute it,
dnl with or without modifications, as long as this notice is preserved.

dnl From Bruno Haible.

dnl AC_LIB_ARG_WITH is synonymous to AC_ARG_WITH in autoconf-2.13, and
dnl similar to AC_ARG_WITH in autoconf 2.52...2.57 except that is doesn't
dnl require excessive bracketing.
ifdef([AC_HELP_STRING],
[AC_DEFUN([AC_LIB_ARG_WITH], [AC_ARG_WITH([$1],[[$2]],[$3],[$4])])],
[AC_DEFUN([AC_][LIB_ARG_WITH], [AC_ARG_WITH([$1],[$2],[$3],[$4])])])

dnl AC_LIB_PREFIX adds to the CPPFLAGS and LDFLAGS the flags that are needed
dnl to access previously installed libraries. The basic assumption is that
dnl a user will want packages to use other packages he previously installed
dnl with the same --prefix option.
dnl This macro is not needed if only AC_LIB_LINKFLAGS is used to locate
dnl libraries, but is otherwise very convenient.
AC_DEFUN([AC_LIB_PREFIX],
[
  AC_BEFORE([$0], [AC_LIB_LINKFLAGS])
  AC_REQUIRE([AC_PROG_CC])
  AC_REQUIRE([AC_CANONICAL_HOST])
  AC_REQUIRE([AC_LIB_PREPARE_MULTILIB])
  AC_REQUIRE([AC_LIB_PREPARE_PREFIX])
  dnl By default, look in $includedir and $libdir.
  use_additional=yes
  AC_LIB_WITH_FINAL_PREFIX([
    eval additional_includedir=\"$includedir\"
    eval additional_libdir=\"$libdir\"
  ])
  AC_LIB_ARG_WITH([lib-prefix],
[  --with-lib-prefix[=DIR] search for libraries in DIR/include and DIR/lib
  --without-lib-prefix    don't search for libraries in includedir and libdir],
[
    if test "X$withval" = "Xno"; then
      use_additional=no
    else
      if test "X$withval" = "X"; then
        AC_LIB_WITH_FINAL_PREFIX([
          eval additional_includedir=\"$includedir\"
          eval additional_libdir=\"$libdir\"
        ])
      else
        additional_includedir="$withval/include"
        additional_libdir="$withval/$acl_libdirstem"
      fi
    fi
])
  if test $use_additional = yes; then
    dnl Potentially add $additional_includedir to $CPPFLAGS.
    dnl But don't add it
    dnl   1. if it's the standard /usr/include,
    dnl   2. if it's already present in $CPPFLAGS,
    dnl   3. if it's /usr/local/include and we are using GCC on Linux,
    dnl   4. if it doesn't exist as a directory.
    if test "X$additional_includedir" != "X/usr/include"; then
      haveit=
      for x in $CPPFLAGS; do
        AC_LIB_WITH_FINAL_PREFIX([eval x=\"$x\"])
        if test "X$x" = "X-I$additional_includedir"; then
          haveit=yes
          break
        fi
      done
      if test -z "$haveit"; then
        if test "X$additional_includedir" = "X/usr/local/include"; then
          if test -n "$GCC"; then
            case $host_os in
              linux* | gnu* | k*bsd*-gnu) haveit=yes;;
            esac
          fi
        fi
        if test -z "$haveit"; then
          if test -d "$additional_includedir"; then
            dnl Really add $additional_includedir to $CPPFLAGS.
            CPPFLAGS="${CPPFLAGS}${CPPFLAGS:+ }-I$additional_includedir"
          fi
        fi
      fi
    fi
    dnl Potentially add $additional_libdir to $LDFLAGS.
    dnl But don't add it
    dnl   1. if it's the standard /usr/lib,
    dnl   2. if it's already present in $LDFLAGS,
    dnl   3. if it's /usr/local/lib and we are using GCC on Linux,
    dnl   4. if it doesn't exist as a directory.
    if test "X$additional_libdir" != "X/usr/$acl_libdirstem"; then
      haveit=
      for x in $LDFLAGS; do
        AC_LIB_WITH_FINAL_PREFIX([eval x=\"$x\"])
        if test "X$x" = "X-L$additional_libdir"; then
          haveit=yes
          break
        fi
      done
      if test -z "$haveit"; then
        if test "X$additional_libdir" = "X/usr/local/$acl_libdirstem"; then
          if test -n "$GCC"; then
            case $host_os in
              linux*) haveit=yes;;
            esac
          fi
        fi
        if test -z "$haveit"; then
          if test -d "$additional_libdir"; then
            dnl Really add $additional_libdir to $LDFLAGS.
            LDFLAGS="${LDFLAGS}${LDFLAGS:+ }-L$additional_libdir"
          fi
        fi
      fi
    fi
  fi
])

dnl AC_LIB_PREPARE_PREFIX creates variables acl_final_prefix,
dnl acl_final_exec_prefix, containing the values to which $prefix and
dnl $exec_prefix will expand at the end of the configure script.
AC_DEFUN([AC_LIB_PREPARE_PREFIX],
[
  dnl Unfortunately, prefix and exec_prefix get only finally determined
  dnl at the end of configure.
  if test "X$prefix" = "XNONE"; then
    acl_final_prefix="$ac_default_prefix"
  else
    acl_final_prefix="$prefix"
  fi
  if test "X$exec_prefix" = "XNONE"; then
    acl_final_exec_prefix='${prefix}'
  else
    acl_final_exec_prefix="$exec_prefix"
  fi
  acl_save_prefix="$prefix"
  prefix="$acl_final_prefix"
  eval acl_final_exec_prefix=\"$acl_final_exec_prefix\"
  prefix="$acl_save_prefix"
])

dnl AC_LIB_WITH_FINAL_PREFIX([statement]) evaluates statement, with the
dnl variables prefix and exec_prefix bound to the values they will have
dnl at the end of the configure script.
AC_DEFUN([AC_LIB_WITH_FINAL_PREFIX],
[
  acl_save_prefix="$prefix"
  prefix="$acl_final_prefix"
  acl_save_exec_prefix="$exec_prefix"
  exec_prefix="$acl_final_exec_prefix"
  $1
  exec_prefix="$acl_save_exec_prefix"
  prefix="$acl_save_prefix"
])

dnl AC_LIB_PREPARE_MULTILIB creates
dnl - a variable acl_libdirstem, containing the basename of the libdir, either
dnl   "lib" or "lib64" or "lib/64",
dnl - a variable acl_libdirstem2, as a secondary possible value for
dnl   acl_libdirstem, either the same as acl_libdirstem or "lib/sparcv9" or
dnl   "lib/amd64".
AC_DEFUN([AC_LIB_PREPARE_MULTILIB],
[
  dnl There is no formal standard regarding lib and lib64.
  dnl On glibc systems, the current practice is that on a system supporting
  dnl 32-bit and 64-bit instruction sets or ABIs, 64-bit libraries go under
  dnl $prefix/lib64 and 32-bit libraries go under $prefix/lib. We determine
  dnl the compiler's default mode by looking at the compiler's library search
  dnl path. If at least one of its elements ends in /lib64 or points to a
  dnl directory whose absolute pathname ends in /lib64, we assume a 64-bit ABI.
  dnl Otherwise we use the default, namely "lib".
  dnl On Solaris systems, the current practice is that on a system supporting
  dnl 32-bit and 64-bit instruction sets or ABIs, 64-bit libraries go under
  dnl $prefix/lib/64 (which is a symlink to either $prefix/lib/sparcv9 or
  dnl $prefix/lib/amd64) and 32-bit libraries go under $prefix/lib.
  AC_REQUIRE([AC_CANONICAL_HOST])
  acl_libdirstem=lib
  acl_libdirstem2=
  case "$host_os" in
    solaris*)
      dnl See Solaris 10 Software Developer Collection > Solaris 64-bit Developer's Guide > The Development Environment
      dnl <http://docs.sun.com/app/docs/doc/816-5138/dev-env?l=en&a=view>.
      dnl "Portable Makefiles should refer to any library directories using the 64 symbolic link."
      dnl But we want to recognize the sparcv9 or amd64 subdirectory also if the
      dnl symlink is missing, so we set acl_libdirstem2 too.
      AC_CACHE_CHECK([for 64-bit host], [gl_cv_solaris_64bit],
        [AC_EGREP_CPP([sixtyfour bits], [
#ifdef _LP64
sixtyfour bits
#endif
           ], [gl_cv_solaris_64bit=yes], [gl_cv_solaris_64bit=no])
        ])
      if test $gl_cv_solaris_64bit = yes; then
        acl_libdirstem=lib/64
        case "$host_cpu" in
          sparc*)        acl_libdirstem2=lib/sparcv9 ;;
          i*86 | x86_64) acl_libdirstem2=lib/amd64 ;;
        esac
      fi
      ;;
    *)
      searchpath=`(LC_ALL=C $CC -print-search-dirs) 2>/dev/null | sed -n -e 's,^libraries: ,,p' | sed -e 's,^=,,'`
      if test -n "$searchpath"; then
        acl_save_IFS="${IFS= 	}"; IFS=":"
        for searchdir in $searchpath; do
          if test -d "$searchdir"; then
            case "$searchdir" in
              */lib64/ | */lib64 ) acl_libdirstem=lib64 ;;
              */../ | */.. )
                # Better ignore directories of this form. They are misleading.
                ;;
              *) searchdir=`cd "$searchdir" && pwd`
                 case "$searchdir" in
                   */lib64 ) acl_libdirstem=lib64 ;;
                 esac ;;
            esac
          fi
        done
        IFS="$acl_save_IFS"
      fi
      ;;
  esac
  test -n "$acl_libdirstem2" || acl_libdirstem2="$acl_libdirstem"
])

# libtool.m4 - Configure libtool for the host system. -*-Autoconf-*-
#
#   Copyright (C) 1996-2001, 2003-2015 Free Software Foundation, Inc.
#   Written by Gordon Matzigkeit, 1996
#
# This file is free software; the Free Software Foundation gives
# unlimited permission to copy and/or distribute it, with or without
# modifications, as long as this notice is preserved.

m4_define([_LT_COPYING], [dnl
# Copyright (C) 2014 Free Software Foundation, Inc.
# This is free software; see the source for copying conditions.  There is NO
# warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

# GNU Libtool is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of of the License, or
# (at your option) any later version.
#
# As a special exception to the GNU General Public License, if you
# distribute this file as part of a program or library that is built
# using GNU Libtool, you may include this file under the  same
# distribution terms that you use for the rest of that program.
#
# GNU Libtool is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
])

# serial 58 LT_INIT


# LT_PREREQ(VERSION)
# ------------------
# Complain and exit if this libtool version is less that VERSION.
m4_defun([LT_PREREQ],
[m4_if(m4_version_compare(m4_defn([LT_PACKAGE_VERSION]), [$1]), -1,
       [m4_default([$3],
		   [m4_fatal([Libtool version $1 or higher is required],
		             63)])],
       [$2])])


# _LT_CHECK_BUILDDIR
# ------------------
# Complain if the absolute build directory name contains unusual characters
m4_defun([_LT_CHECK_BUILDDIR],
[case `pwd` in
  *\ * | *\	*)
    AC_MSG_WARN([Libtool does not cope well with whitespace in `pwd`]) ;;
esac
])


# LT_INIT([OPTIONS])
# ------------------
AC_DEFUN([LT_INIT],
[AC_PREREQ([2.62])dnl We use AC_PATH_PROGS_FEATURE_CHECK
AC_REQUIRE([AC_CONFIG_AUX_DIR_DEFAULT])dnl
AC_BEFORE([$0], [LT_LANG])dnl
AC_BEFORE([$0], [LT_OUTPUT])dnl
AC_BEFORE([$0], [LTDL_INIT])dnl
m4_require([_LT_CHECK_BUILDDIR])dnl

dnl Autoconf doesn't catch unexpanded LT_ macros by default:
m4_pattern_forbid([^_?LT_[A-Z_]+$])dnl
m4_pattern_allow([^(_LT_EOF|LT_DLGLOBAL|LT_DLLAZY_OR_NOW|LT_MULTI_MODULE)$])dnl
dnl aclocal doesn't pull ltoptions.m4, ltsugar.m4, or ltversion.m4
dnl unless we require an AC_DEFUNed macro:
AC_REQUIRE([LTOPTIONS_VERSION])dnl
AC_REQUIRE([LTSUGAR_VERSION])dnl
AC_REQUIRE([LTVERSION_VERSION])dnl
AC_REQUIRE([LTOBSOLETE_VERSION])dnl
m4_require([_LT_PROG_LTMAIN])dnl

_LT_SHELL_INIT([SHELL=${CONFIG_SHELL-/bin/sh}])

dnl Parse OPTIONS
_LT_SET_OPTIONS([$0], [$1])

# This can be used to rebuild libtool when needed
LIBTOOL_DEPS=$ltmain

# Always use our own libtool.
LIBTOOL='$(SHELL) $(top_builddir)/libtool'
AC_SUBST(LIBTOOL)dnl

_LT_SETUP

# Only expand once:
m4_define([LT_INIT])
])# LT_INIT

# Old names:
AU_ALIAS([AC_PROG_LIBTOOL], [LT_INIT])
AU_ALIAS([AM_PROG_LIBTOOL], [LT_INIT])
dnl aclocal-1.4 backwards compatibility:
dnl AC_DEFUN([AC_PROG_LIBTOOL], [])
dnl AC_DEFUN([AM_PROG_LIBTOOL], [])


# _LT_PREPARE_CC_BASENAME
# -----------------------
m4_defun([_LT_PREPARE_CC_BASENAME], [
# Calculate cc_basename.  Skip known compiler wrappers and cross-prefix.
func_cc_basename ()
{
    for cc_temp in @S|@*""; do
      case $cc_temp in
        compile | *[[\\/]]compile | ccache | *[[\\/]]ccache ) ;;
        distcc | *[[\\/]]distcc | purify | *[[\\/]]purify ) ;;
        \-*) ;;
        *) break;;
      esac
    done
    func_cc_basename_result=`$ECHO "$cc_temp" | $SED "s%.*/%%; s%^$host_alias-%%"`
}
])# _LT_PREPARE_CC_BASENAME


# _LT_CC_BASENAME(CC)
# -------------------
# It would be clearer to call AC_REQUIREs from _LT_PREPARE_CC_BASENAME,
# but that macro is also expanded into generated libtool script, which
# arranges for $SED and $ECHO to be set by different means.
m4_defun([_LT_CC_BASENAME],
[m4_require([_LT_PREPARE_CC_BASENAME])dnl
AC_REQUIRE([_LT_DECL_SED])dnl
AC_REQUIRE([_LT_PROG_ECHO_BACKSLASH])dnl
func_cc_basename $1
cc_basename=$func_cc_basename_result
])


# _LT_FILEUTILS_DEFAULTS
# ----------------------
# It is okay to use these file commands and assume they have been set
# sensibly after 'm4_require([_LT_FILEUTILS_DEFAULTS])'.
m4_defun([_LT_FILEUTILS_DEFAULTS],
[: ${CP="cp -f"}
: ${MV="mv -f"}
: ${RM="rm -f"}
])# _LT_FILEUTILS_DEFAULTS


# _LT_SETUP
# ---------
m4_defun([_LT_SETUP],
[AC_REQUIRE([AC_CANONICAL_HOST])dnl
AC_REQUIRE([AC_CANONICAL_BUILD])dnl
AC_REQUIRE([_LT_PREPARE_SED_QUOTE_VARS])dnl
AC_REQUIRE([_LT_PROG_ECHO_BACKSLASH])dnl

_LT_DECL([], [PATH_SEPARATOR], [1], [The PATH separator for the build system])dnl
dnl
_LT_DECL([], [host_alias], [0], [The host system])dnl
_LT_DECL([], [host], [0])dnl
_LT_DECL([], [host_os], [0])dnl
dnl
_LT_DECL([], [build_alias], [0], [The build system])dnl
_LT_DECL([], [build], [0])dnl
_LT_DECL([], [build_os], [0])dnl
dnl
AC_REQUIRE([AC_PROG_CC])dnl
AC_REQUIRE([LT_PATH_LD])dnl
AC_REQUIRE([LT_PATH_NM])dnl
dnl
AC_REQUIRE([AC_PROG_LN_S])dnl
test -z "$LN_S" && LN_S="ln -s"
_LT_DECL([], [LN_S], [1], [Whether we need soft or hard links])dnl
dnl
AC_REQUIRE([LT_CMD_MAX_LEN])dnl
_LT_DECL([objext], [ac_objext], [0], [Object file suffix (normally "o")])dnl
_LT_DECL([], [exeext], [0], [Executable file suffix (normally "")])dnl
dnl
m4_require([_LT_FILEUTILS_DEFAULTS])dnl
m4_require([_LT_CHECK_SHELL_FEATURES])dnl
m4_require([_LT_PATH_CONVERSION_FUNCTIONS])dnl
m4_require([_LT_CMD_RELOAD])dnl
m4_require([_LT_CHECK_MAGIC_METHOD])dnl
m4_require([_LT_CHECK_SHAREDLIB_FROM_LINKLIB])dnl
m4_require([_LT_CMD_OLD_ARCHIVE])dnl
m4_require([_LT_CMD_GLOBAL_SYMBOLS])dnl
m4_require([_LT_WITH_SYSROOT])dnl
m4_require([_LT_CMD_TRUNCATE])dnl

_LT_CONFIG_LIBTOOL_INIT([
# See if we are running on zsh, and set the options that allow our
# commands through without removal of \ escapes INIT.
if test -n "\${ZSH_VERSION+set}"; then
   setopt NO_GLOB_SUBST
fi
])
if test -n "${ZSH_VERSION+set}"; then
   setopt NO_GLOB_SUBST
fi

_LT_CHECK_OBJDIR

m4_require([_LT_TAG_COMPILER])dnl

case $host_os in
aix3*)
  # AIX sometimes has problems with the GCC collect2 program.  For some
  # reason, if we set the COLLECT_NAMES environment variable, the problems
  # vanish in a puff of smoke.
  if test set != "${COLLECT_NAMES+set}"; then
    COLLECT_NAMES=
    export COLLECT_NAMES
  fi
  ;;
esac

# Global variables:
ofile=libtool
can_build_shared=yes

# All known linkers require a '.a' archive for static linking (except MSVC,
# which needs '.lib').
libext=a

with_gnu_ld=$lt_cv_prog_gnu_ld

old_CC=$CC
old_CFLAGS=$CFLAGS

# Set sane defaults for various variables
test -z "$CC" && CC=cc
test -z "$LTCC" && LTCC=$CC
test -z "$LTCFLAGS" && LTCFLAGS=$CFLAGS
test -z "$LD" && LD=ld
test -z "$ac_objext" && ac_objext=o

_LT_CC_BASENAME([$compiler])

# Only perform the check for file, if the check method requires it
test -z "$MAGIC_CMD" && MAGIC_CMD=file
case $deplibs_check_method in
file_magic*)
  if test "$file_magic_cmd" = '$MAGIC_CMD'; then
    _LT_PATH_MAGIC
  fi
  ;;
esac

# Use C for the default configuration in the libtool script
LT_SUPPORTED_TAG([CC])
_LT_LANG_C_CONFIG
_LT_LANG_DEFAULT_CONFIG
_LT_CONFIG_COMMANDS
])# _LT_SETUP


# _LT_PREPARE_SED_QUOTE_VARS
# --------------------------
# Define a few sed substitution that help us do robust quoting.
m4_defun([_LT_PREPARE_SED_QUOTE_VARS],
[# Backslashify metacharacters that are still active within
# double-quoted strings.
sed_quote_subst='s/\([["`$\\]]\)/\\\1/g'

# Same as above, but do not quote variable references.
double_quote_subst='s/\([["`\\]]\)/\\\1/g'

# Sed substitution to delay expansion of an escaped shell variable in a
# double_quote_subst'ed string.
delay_variable_subst='s/\\\\\\\\\\\$/\\\\\\$/g'

# Sed substitution to delay expansion of an escaped single quote.
delay_single_quote_subst='s/'\''/'\'\\\\\\\'\''/g'

# Sed substitution to avoid accidental globbing in evaled expressions
no_glob_subst='s/\*/\\\*/g'
])

# _LT_PROG_LTMAIN
# ---------------
# Note that this code is called both from 'configure', and 'config.status'
# now that we use AC_CONFIG_COMMANDS to generate libtool.  Notably,
# 'config.status' has no value for ac_aux_dir unless we are using Automake,
# so we pass a copy along to make sure it has a sensible value anyway.
m4_defun([_LT_PROG_LTMAIN],
[m4_ifdef([AC_REQUIRE_AUX_FILE], [AC_REQUIRE_AUX_FILE([ltmain.sh])])dnl
_LT_CONFIG_LIBTOOL_INIT([ac_aux_dir='$ac_aux_dir'])
ltmain=$ac_aux_dir/ltmain.sh
])# _LT_PROG_LTMAIN



# So that we can recreate a full libtool script including additional
# tags, we accumulate the chunks of code to send to AC_CONFIG_COMMANDS
# in macros and then make a single call at the end using the 'libtool'
# label.


# _LT_CONFIG_LIBTOOL_INIT([INIT-COMMANDS])
# ----------------------------------------
# Register INIT-COMMANDS to be passed to AC_CONFIG_COMMANDS later.
m4_define([_LT_CONFIG_LIBTOOL_INIT],
[m4_ifval([$1],
          [m4_append([_LT_OUTPUT_LIBTOOL_INIT],
                     [$1
])])])

# Initialize.
m4_define([_LT_OUTPUT_LIBTOOL_INIT])


# _LT_CONFIG_LIBTOOL([COMMANDS])
# ------------------------------
# Register COMMANDS to be passed to AC_CONFIG_COMMANDS later.
m4_define([_LT_CONFIG_LIBTOOL],
[m4_ifval([$1],
          [m4_append([_LT_OUTPUT_LIBTOOL_COMMANDS],
                     [$1
])])])

# Initialize.
m4_define([_LT_OUTPUT_LIBTOOL_COMMANDS])


# _LT_CONFIG_SAVE_COMMANDS([COMMANDS], [INIT_COMMANDS])
# -----------------------------------------------------
m4_defun([_LT_CONFIG_SAVE_COMMANDS],
[_LT_CONFIG_LIBTOOL([$1])
_LT_CONFIG_LIBTOOL_INIT([$2])
])


# _LT_FORMAT_COMMENT([COMMENT])
# -----------------------------
# Add leading comment marks to the start of each line, and a trailing
# full-stop to the whole comment if one is not present already.
m4_define([_LT_FORMAT_COMMENT],
[m4_ifval([$1], [
m4_bpatsubst([m4_bpatsubst([$1], [^ *], [# ])],
              [['`$\]], [\\\&])]m4_bmatch([$1], [[!?.]$], [], [.])
)])





# _LT_DECL([CONFIGNAME], VARNAME, VALUE, [DESCRIPTION], [IS-TAGGED?])
# -------------------------------------------------------------------
# CONFIGNAME is the name given to the value in the libtool script.
# VARNAME is the (base) name used in the configure script.
# VALUE may be 0, 1 or 2 for a computed quote escaped value based on
# VARNAME.  Any other value will be used directly.
m4_define([_LT_DECL],
[lt_if_append_uniq([lt_decl_varnames], [$2], [, ],
    [lt_dict_add_subkey([lt_decl_dict], [$2], [libtool_name],
	[m4_ifval([$1], [$1], [$2])])
    lt_dict_add_subkey([lt_decl_dict], [$2], [value], [$3])
    m4_ifval([$4],
	[lt_dict_add_subkey([lt_decl_dict], [$2], [description], [$4])])
    lt_dict_add_subkey([lt_decl_dict], [$2],
	[tagged?], [m4_ifval([$5], [yes], [no])])])
])


# _LT_TAGDECL([CONFIGNAME], VARNAME, VALUE, [DESCRIPTION])
# --------------------------------------------------------
m4_define([_LT_TAGDECL], [_LT_DECL([$1], [$2], [$3], [$4], [yes])])


# lt_decl_tag_varnames([SEPARATOR], [VARNAME1...])
# ------------------------------------------------
m4_define([lt_decl_tag_varnames],
[_lt_decl_filter([tagged?], [yes], $@)])


# _lt_decl_filter(SUBKEY, VALUE, [SEPARATOR], [VARNAME1..])
# ---------------------------------------------------------
m4_define([_lt_decl_filter],
[m4_case([$#],
  [0], [m4_fatal([$0: too few arguments: $#])],
  [1], [m4_fatal([$0: too few arguments: $#: $1])],
  [2], [lt_dict_filter([lt_decl_dict], [$1], [$2], [], lt_decl_varnames)],
  [3], [lt_dict_filter([lt_decl_dict], [$1], [$2], [$3], lt_decl_varnames)],
  [lt_dict_filter([lt_decl_dict], $@)])[]dnl
])


# lt_decl_quote_varnames([SEPARATOR], [VARNAME1...])
# --------------------------------------------------
m4_define([lt_decl_quote_varnames],
[_lt_decl_filter([value], [1], $@)])


# lt_decl_dquote_varnames([SEPARATOR], [VARNAME1...])
# ---------------------------------------------------
m4_define([lt_decl_dquote_varnames],
[_lt_decl_filter([value], [2], $@)])


# lt_decl_varnames_tagged([SEPARATOR], [VARNAME1...])
# ---------------------------------------------------
m4_define([lt_decl_varnames_tagged],
[m4_assert([$# <= 2])dnl
_$0(m4_quote(m4_default([$1], [[, ]])),
    m4_ifval([$2], [[$2]], [m4_dquote(lt_decl_tag_varnames)]),
    m4_split(m4_normalize(m4_quote(_LT_TAGS)), [ ]))])
m4_define([_lt_decl_varnames_tagged],
[m4_ifval([$3], [lt_combine([$1], [$2], [_], $3)])])


# lt_decl_all_varnames([SEPARATOR], [VARNAME1...])
# ------------------------------------------------
m4_define([lt_decl_all_varnames],
[_$0(m4_quote(m4_default([$1], [[, ]])),
     m4_if([$2], [],
	   m4_quote(lt_decl_varnames),
	m4_quote(m4_shift($@))))[]dnl
])
m4_define([_lt_decl_all_varnames],
[lt_join($@, lt_decl_varnames_tagged([$1],
			lt_decl_tag_varnames([[, ]], m4_shift($@))))dnl
])


# _LT_CONFIG_STATUS_DECLARE([VARNAME])
# ------------------------------------
# Quote a variable value, and forward it to 'config.status' so that its
# declaration there will have the same value as in 'configure'.  VARNAME
# must have a single quote delimited value for this to work.
m4_define([_LT_CONFIG_STATUS_DECLARE],
[$1='`$ECHO "$][$1" | $SED "$delay_single_quote_subst"`'])


# _LT_CONFIG_STATUS_DECLARATIONS
# ------------------------------
# We delimit libtool config variables with single quotes, so when
# we write them to config.status, we have to be sure to quote all
# embedded single quotes properly.  In configure, this macro expands
# each variable declared with _LT_DECL (and _LT_TAGDECL) into:
#
#    <var>='`$ECHO "$<var>" | $SED "$delay_single_quote_subst"`'
m4_defun([_LT_CONFIG_STATUS_DECLARATIONS],
[m4_foreach([_lt_var], m4_quote(lt_decl_all_varnames),
    [m4_n([_LT_CONFIG_STATUS_DECLARE(_lt_var)])])])


# _LT_LIBTOOL_TAGS
# ----------------
# Output comment and list of tags supported by the script
m4_defun([_LT_LIBTOOL_TAGS],
[_LT_FORMAT_COMMENT([The names of the tagged configurations supported by this script])dnl
available_tags='_LT_TAGS'dnl
])


# _LT_LIBTOOL_DECLARE(VARNAME, [TAG])
# -----------------------------------
# Extract the dictionary values for VARNAME (optionally with TAG) and
# expand to a commented shell variable setting:
#
#    # Some comment about what VAR is for.
#    visible_name=$lt_internal_name
m4_define([_LT_LIBTOOL_DECLARE],
[_LT_FORMAT_COMMENT(m4_quote(lt_dict_fetch([lt_decl_dict], [$1],
					   [description])))[]dnl
m4_pushdef([_libtool_name],
    m4_quote(lt_dict_fetch([lt_decl_dict], [$1], [libtool_name])))[]dnl
m4_case(m4_quote(lt_dict_fetch([lt_decl_dict], [$1], [value])),
    [0], [_libtool_name=[$]$1],
    [1], [_libtool_name=$lt_[]$1],
    [2], [_libtool_name=$lt_[]$1],
    [_libtool_name=lt_dict_fetch([lt_decl_dict], [$1], [value])])[]dnl
m4_ifval([$2], [_$2])[]m4_popdef([_libtool_name])[]dnl
])


# _LT_LIBTOOL_CONFIG_VARS
# -----------------------
# Produce commented declarations of non-tagged libtool config variables
# suitable for insertion in the LIBTOOL CONFIG section of the 'libtool'
# script.  Tagged libtool config variables (even for the LIBTOOL CONFIG
# section) are produced by _LT_LIBTOOL_TAG_VARS.
m4_defun([_LT_LIBTOOL_CONFIG_VARS],
[m4_foreach([_lt_var],
    m4_quote(_lt_decl_filter([tagged?], [no], [], lt_decl_varnames)),
    [m4_n([_LT_LIBTOOL_DECLARE(_lt_var)])])])


# _LT_LIBTOOL_TAG_VARS(TAG)
# -------------------------
m4_define([_LT_LIBTOOL_TAG_VARS],
[m4_foreach([_lt_var], m4_quote(lt_decl_tag_varnames),
    [m4_n([_LT_LIBTOOL_DECLARE(_lt_var, [$1])])])])


# _LT_TAGVAR(VARNAME, [TAGNAME])
# ------------------------------
m4_define([_LT_TAGVAR], [m4_ifval([$2], [$1_$2], [$1])])


# _LT_CONFIG_COMMANDS
# -------------------
# Send accumulated output to $CONFIG_STATUS.  Thanks to the lists of
# variables for single and double quote escaping we saved from calls
# to _LT_DECL, we can put quote escaped variables declarations
# into 'config.status', and then the shell code to quote escape them in
# for loops in 'config.status'.  Finally, any additional code accumulated
# from calls to _LT_CONFIG_LIBTOOL_INIT is expanded.
m4_defun([_LT_CONFIG_COMMANDS],
[AC_PROVIDE_IFELSE([LT_OUTPUT],
	dnl If the libtool generation code has been placed in $CONFIG_LT,
	dnl instead of duplicating it all over again into config.status,
	dnl then we will have config.status run $CONFIG_LT later, so it
	dnl needs to know what name is stored there:
        [AC_CONFIG_COMMANDS([libtool],
            [$SHELL $CONFIG_LT || AS_EXIT(1)], [CONFIG_LT='$CONFIG_LT'])],
    dnl If the libtool generation code is destined for config.status,
    dnl expand the accumulated commands and init code now:
    [AC_CONFIG_COMMANDS([libtool],
        [_LT_OUTPUT_LIBTOOL_COMMANDS], [_LT_OUTPUT_LIBTOOL_COMMANDS_INIT])])
])#_LT_CONFIG_COMMANDS


# Initialize.
m4_define([_LT_OUTPUT_LIBTOOL_COMMANDS_INIT],
[

# The HP-UX ksh and POSIX shell print the target directory to stdout
# if CDPATH is set.
(unset CDPATH) >/dev/null 2>&1 && unset CDPATH

sed_quote_subst='$sed_quote_subst'
double_quote_subst='$double_quote_subst'
delay_variable_subst='$delay_variable_subst'
_LT_CONFIG_STATUS_DECLARATIONS
LTCC='$LTCC'
LTCFLAGS='$LTCFLAGS'
compiler='$compiler_DEFAULT'

# A function that is used when there is no print builtin or printf.
func_fallback_echo ()
{
  eval 'cat <<_LTECHO_EOF
\$[]1
_LTECHO_EOF'
}

# Quote evaled strings.
for var in lt_decl_all_varnames([[ \
]], lt_decl_quote_varnames); do
    case \`eval \\\\\$ECHO \\\\""\\\\\$\$var"\\\\"\` in
    *[[\\\\\\\`\\"\\\$]]*)
      eval "lt_\$var=\\\\\\"\\\`\\\$ECHO \\"\\\$\$var\\" | \\\$SED \\"\\\$sed_quote_subst\\"\\\`\\\\\\"" ## exclude from sc_prohibit_nested_quotes
      ;;
    *)
      eval "lt_\$var=\\\\\\"\\\$\$var\\\\\\""
      ;;
    esac
done

# Double-quote double-evaled strings.
for var in lt_decl_all_varnames([[ \
]], lt_decl_dquote_varnames); do
    case \`eval \\\\\$ECHO \\\\""\\\\\$\$var"\\\\"\` in
    *[[\\\\\\\`\\"\\\$]]*)
      eval "lt_\$var=\\\\\\"\\\`\\\$ECHO \\"\\\$\$var\\" | \\\$SED -e \\"\\\$double_quote_subst\\" -e \\"\\\$sed_quote_subst\\" -e \\"\\\$delay_variable_subst\\"\\\`\\\\\\"" ## exclude from sc_prohibit_nested_quotes
      ;;
    *)
      eval "lt_\$var=\\\\\\"\\\$\$var\\\\\\""
      ;;
    esac
done

_LT_OUTPUT_LIBTOOL_INIT
])

# _LT_GENERATED_FILE_INIT(FILE, [COMMENT])
# ------------------------------------
# Generate a child script FILE with all initialization necessary to
# reuse the environment learned by the parent script, and make the
# file executable.  If COMMENT is supplied, it is inserted after the
# '#!' sequence but before initialization text begins.  After this
# macro, additional text can be appended to FILE to form the body of
# the child script.  The macro ends with non-zero status if the
# file could not be fully written (such as if the disk is full).
m4_ifdef([AS_INIT_GENERATED],
[m4_defun([_LT_GENERATED_FILE_INIT],[AS_INIT_GENERATED($@)])],
[m4_defun([_LT_GENERATED_FILE_INIT],
[m4_require([AS_PREPARE])]dnl
[m4_pushdef([AS_MESSAGE_LOG_FD])]dnl
[lt_write_fail=0
cat >$1 <<_ASEOF || lt_write_fail=1
#! $SHELL
# Generated by $as_me.
$2
SHELL=\${CONFIG_SHELL-$SHELL}
export SHELL
_ASEOF
cat >>$1 <<\_ASEOF || lt_write_fail=1
AS_SHELL_SANITIZE
_AS_PREPARE
exec AS_MESSAGE_FD>&1
_ASEOF
test 0 = "$lt_write_fail" && chmod +x $1[]dnl
m4_popdef([AS_MESSAGE_LOG_FD])])])# _LT_GENERATED_FILE_INIT

# LT_OUTPUT
# ---------
# This macro allows early generation of the libtool script (before
# AC_OUTPUT is called), incase it is used in configure for compilation
# tests.
AC_DEFUN([LT_OUTPUT],
[: ${CONFIG_LT=./config.lt}
AC_MSG_NOTICE([creating $CONFIG_LT])
_LT_GENERATED_FILE_INIT(["$CONFIG_LT"],
[# Run this file to recreate a libtool stub with the current configuration.])

cat >>"$CONFIG_LT" <<\_LTEOF
lt_cl_silent=false
exec AS_MESSAGE_LOG_FD>>config.log
{
  echo
  AS_BOX([Running $as_me.])
} >&AS_MESSAGE_LOG_FD

lt_cl_help="\
'$as_me' creates a local libtool stub from the current configuration,
for use in further configure time tests before the real libtool is
generated.

Usage: $[0] [[OPTIONS]]

  -h, --help      print this help, then exit
  -V, --version   print version number, then exit
  -q, --quiet     do not print progress messages
  -d, --debug     don't remove temporary files

Report bugs to <bug-libtool@gnu.org>."

lt_cl_version="\
m4_ifset([AC_PACKAGE_NAME], [AC_PACKAGE_NAME ])config.lt[]dnl
m4_ifset([AC_PACKAGE_VERSION], [ AC_PACKAGE_VERSION])
configured by $[0], generated by m4_PACKAGE_STRING.

Copyright (C) 2011 Free Software Foundation, Inc.
This config.lt script is free software; the Free Software Foundation
gives unlimited permision to copy, distribute and modify it."

while test 0 != $[#]
do
  case $[1] in
    --version | --v* | -V )
      echo "$lt_cl_version"; exit 0 ;;
    --help | --h* | -h )
      echo "$lt_cl_help"; exit 0 ;;
    --debug | --d* | -d )
      debug=: ;;
    --quiet | --q* | --silent | --s* | -q )
      lt_cl_silent=: ;;

    -*) AC_MSG_ERROR([unrecognized option: $[1]
Try '$[0] --help' for more information.]) ;;

    *) AC_MSG_ERROR([unrecognized argument: $[1]
Try '$[0] --help' for more information.]) ;;
  esac
  shift
done

if $lt_cl_silent; then
  exec AS_MESSAGE_FD>/dev/null
fi
_LTEOF

cat >>"$CONFIG_LT" <<_LTEOF
_LT_OUTPUT_LIBTOOL_COMMANDS_INIT
_LTEOF

cat >>"$CONFIG_LT" <<\_LTEOF
AC_MSG_NOTICE([creating $ofile])
_LT_OUTPUT_LIBTOOL_COMMANDS
AS_EXIT(0)
_LTEOF
chmod +x "$CONFIG_LT"

# configure is writing to config.log, but config.lt does its own redirection,
# appending to config.log, which fails on DOS, as config.log is still kept
# open by configure.  Here we exec the FD to /dev/null, effectively closing
# config.log, so it can be properly (re)opened and appended to by config.lt.
lt_cl_success=:
test yes = "$silent" &&
  lt_config_lt_args="$lt_config_lt_args --quiet"
exec AS_MESSAGE_LOG_FD>/dev/null
$SHELL "$CONFIG_LT" $lt_config_lt_args || lt_cl_success=false
exec AS_MESSAGE_LOG_FD>>config.log
$lt_cl_success || AS_EXIT(1)
])# LT_OUTPUT


# _LT_CONFIG(TAG)
# ---------------
# If TAG is the built-in tag, create an initial libtool script with a
# default configuration from the untagged config vars.  Otherwise add code
# to config.status for appending the configuration named by TAG from the
# matching tagged config vars.
m4_defun([_LT_CONFIG],
[m4_require([_LT_FILEUTILS_DEFAULTS])dnl
_LT_CONFIG_SAVE_COMMANDS([
  m4_define([_LT_TAG], m4_if([$1], [], [C], [$1]))dnl
  m4_if(_LT_TAG, [C], [
    # See if we are running on zsh, and set the options that allow our
    # commands through without removal of \ escapes.
    if test -n "${ZSH_VERSION+set}"; then
      setopt NO_GLOB_SUBST
    fi

    cfgfile=${ofile}T
    trap "$RM \"$cfgfile\"; exit 1" 1 2 15
    $RM "$cfgfile"

    cat <<_LT_EOF >> "$cfgfile"
#! $SHELL
# Generated automatically by $as_me ($PACKAGE) $VERSION
# NOTE: Changes made to this file will be lost: look at ltmain.sh.

# Provide generalized library-building support services.
# Written by Gordon Matzigkeit, 1996

_LT_COPYING
_LT_LIBTOOL_TAGS

# Configured defaults for sys_lib_dlsearch_path munging.
: \${LT_SYS_LIBRARY_PATH="$configure_time_lt_sys_library_path"}

# ### BEGIN LIBTOOL CONFIG
_LT_LIBTOOL_CONFIG_VARS
_LT_LIBTOOL_TAG_VARS
# ### END LIBTOOL CONFIG

_LT_EOF

    cat <<'_LT_EOF' >> "$cfgfile"

# ### BEGIN FUNCTIONS SHARED WITH CONFIGURE

_LT_PREPARE_MUNGE_PATH_LIST
_LT_PREPARE_CC_BASENAME

# ### END FUNCTIONS SHARED WITH CONFIGURE

_LT_EOF

  case $host_os in
  aix3*)
    cat <<\_LT_EOF >> "$cfgfile"
# AIX sometimes has problems with the GCC collect2 program.  For some
# reason, if we set the COLLECT_NAMES environment variable, the problems
# vanish in a puff of smoke.
if test set != "${COLLECT_NAMES+set}"; then
  COLLECT_NAMES=
  export COLLECT_NAMES
fi
_LT_EOF
    ;;
  esac

  _LT_PROG_LTMAIN

  # We use sed instead of cat because bash on DJGPP gets confused if
  # if finds mixed CR/LF and LF-only lines.  Since sed operates in
  # text mode, it properly converts lines to CR/LF.  This bash problem
  # is reportedly fixed, but why not run on old versions too?
  sed '$q' "$ltmain" >> "$cfgfile" \
     || (rm -f "$cfgfile"; exit 1)

   mv -f "$cfgfile" "$ofile" ||
    (rm -f "$ofile" && cp "$cfgfile" "$ofile" && rm -f "$cfgfile")
  chmod +x "$ofile"
],
[cat <<_LT_EOF >> "$ofile"

dnl Unfortunately we have to use $1 here, since _LT_TAG is not expanded
dnl in a comment (ie after a #).
# ### BEGIN LIBTOOL TAG CONFIG: $1
_LT_LIBTOOL_TAG_VARS(_LT_TAG)
# ### END LIBTOOL TAG CONFIG: $1
_LT_EOF
])dnl /m4_if
],
[m4_if([$1], [], [
    PACKAGE='$PACKAGE'
    VERSION='$VERSION'
    RM='$RM'
    ofile='$ofile'], [])
])dnl /_LT_CONFIG_SAVE_COMMANDS
])# _LT_CONFIG


# LT_SUPPORTED_TAG(TAG)
# ---------------------
# Trace this macro to discover what tags are supported by the libtool
# --tag option, using:
#    autoconf --trace 'LT_SUPPORTED_TAG:$1'
AC_DEFUN([LT_SUPPORTED_TAG], [])


# C support is built-in for now
m4_define([_LT_LANG_C_enabled], [])
m4_define([_LT_TAGS], [])


# LT_LANG(LANG)
# -------------
# Enable libtool support for the given language if not already enabled.
AC_DEFUN([LT_LANG],
[AC_BEFORE([$0], [LT_OUTPUT])dnl
m4_case([$1],
  [C],			[_LT_LANG(C)],
  [C++],		[_LT_LANG(CXX)],
  [Go],			[_LT_LANG(GO)],
  [Java],		[_LT_LANG(GCJ)],
  [Fortran 77],		[_LT_LANG(F77)],
  [Fortran],		[_LT_LANG(FC)],
  [Windows Resource],	[_LT_LANG(RC)],
  [m4_ifdef([_LT_LANG_]$1[_CONFIG],
    [_LT_LANG($1)],
    [m4_fatal([$0: unsupported language: "$1"])])])dnl
])# LT_LANG


# _LT_LANG(LANGNAME)
# ------------------
m4_defun([_LT_LANG],
[m4_ifdef([_LT_LANG_]$1[_enabled], [],
  [LT_SUPPORTED_TAG([$1])dnl
  m4_append([_LT_TAGS], [$1 ])dnl
  m4_define([_LT_LANG_]$1[_enabled], [])dnl
  _LT_LANG_$1_CONFIG($1)])dnl
])# _LT_LANG


m4_ifndef([AC_PROG_GO], [
# NOTE: This macro has been submitted for inclusion into   #
#  GNU Autoconf as AC_PROG_GO.  When it is available in    #
#  a released version of Autoconf we should remove this    #
#  macro and use it instead.                               #
m4_defun([AC_PROG_GO],
[AC_LANG_PUSH(Go)dnl
AC_ARG_VAR([GOC],     [Go compiler command])dnl
AC_ARG_VAR([GOFLAGS], [Go compiler flags])dnl
_AC_ARG_VAR_LDFLAGS()dnl
AC_CHECK_TOOL(GOC, gccgo)
if test -z "$GOC"; then
  if test -n "$ac_tool_prefix"; then
    AC_CHECK_PROG(GOC, [${ac_tool_prefix}gccgo], [${ac_tool_prefix}gccgo])
  fi
fi
if test -z "$GOC"; then
  AC_CHECK_PROG(GOC, gccgo, gccgo, false)
fi
])#m4_defun
])#m4_ifndef


# _LT_LANG_DEFAULT_CONFIG
# -----------------------
m4_defun([_LT_LANG_DEFAULT_CONFIG],
[AC_PROVIDE_IFELSE([AC_PROG_CXX],
  [LT_LANG(CXX)],
  [m4_define([AC_PROG_CXX], defn([AC_PROG_CXX])[LT_LANG(CXX)])])

AC_PROVIDE_IFELSE([AC_PROG_F77],
  [LT_LANG(F77)],
  [m4_define([AC_PROG_F77], defn([AC_PROG_F77])[LT_LANG(F77)])])

AC_PROVIDE_IFELSE([AC_PROG_FC],
  [LT_LANG(FC)],
  [m4_define([AC_PROG_FC], defn([AC_PROG_FC])[LT_LANG(FC)])])

dnl The call to [A][M_PROG_GCJ] is quoted like that to stop aclocal
dnl pulling things in needlessly.
AC_PROVIDE_IFELSE([AC_PROG_GCJ],
  [LT_LANG(GCJ)],
  [AC_PROVIDE_IFELSE([A][M_PROG_GCJ],
    [LT_LANG(GCJ)],
    [AC_PROVIDE_IFELSE([LT_PROG_GCJ],
      [LT_LANG(GCJ)],
      [m4_ifdef([AC_PROG_GCJ],
	[m4_define([AC_PROG_GCJ], defn([AC_PROG_GCJ])[LT_LANG(GCJ)])])
       m4_ifdef([A][M_PROG_GCJ],
	[m4_define([A][M_PROG_GCJ], defn([A][M_PROG_GCJ])[LT_LANG(GCJ)])])
       m4_ifdef([LT_PROG_GCJ],
	[m4_define([LT_PROG_GCJ], defn([LT_PROG_GCJ])[LT_LANG(GCJ)])])])])])

AC_PROVIDE_IFELSE([AC_PROG_GO],
  [LT_LANG(GO)],
  [m4_define([AC_PROG_GO], defn([AC_PROG_GO])[LT_LANG(GO)])])

AC_PROVIDE_IFELSE([LT_PROG_RC],
  [LT_LANG(RC)],
  [m4_define([LT_PROG_RC], defn([LT_PROG_RC])[LT_LANG(RC)])])
])# _LT_LANG_DEFAULT_CONFIG

# Obsolete macros:
AU_DEFUN([AC_LIBTOOL_CXX], [LT_LANG(C++)])
AU_DEFUN([AC_LIBTOOL_F77], [LT_LANG(Fortran 77)])
AU_DEFUN([AC_LIBTOOL_FC], [LT_LANG(Fortran)])
AU_DEFUN([AC_LIBTOOL_GCJ], [LT_LANG(Java)])
AU_DEFUN([AC_LIBTOOL_RC], [LT_LANG(Windows Resource)])
dnl aclocal-1.4 backwards compatibility:
dnl AC_DEFUN([AC_LIBTOOL_CXX], [])
dnl AC_DEFUN([AC_LIBTOOL_F77], [])
dnl AC_DEFUN([AC_LIBTOOL_FC], [])
dnl AC_DEFUN([AC_LIBTOOL_GCJ], [])
dnl AC_DEFUN([AC_LIBTOOL_RC], [])


# _LT_TAG_COMPILER
# ----------------
m4_defun([_LT_TAG_COMPILER],
[AC_REQUIRE([AC_PROG_CC])dnl

_LT_DECL([LTCC], [CC], [1], [A C compiler])dnl
_LT_DECL([LTCFLAGS], [CFLAGS], [1], [LTCC compiler flags])dnl
_LT_TAGDECL([CC], [compiler], [1], [A language specific compiler])dnl
_LT_TAGDECL([with_gcc], [GCC], [0], [Is the compiler the GNU compiler?])dnl

# If no C compiler was specified, use CC.
LTCC=${LTCC-"$CC"}

# If no C compiler flags were specified, use CFLAGS.
LTCFLAGS=${LTCFLAGS-"$CFLAGS"}

# Allow CC to be a program name with arguments.
compiler=$CC
])# _LT_TAG_COMPILER


# _LT_COMPILER_BOILERPLATE
# ------------------------
# Check for compiler boilerplate output or warnings with
# the simple compiler test code.
m4_defun([_LT_COMPILER_BOILERPLATE],
[m4_require([_LT_DECL_SED])dnl
ac_outfile=conftest.$ac_objext
echo "$lt_simple_compile_test_code" >conftest.$ac_ext
eval "$ac_compile" 2>&1 >/dev/null | $SED '/^$/d; /^ *+/d' >conftest.err
_lt_compiler_boilerplate=`cat conftest.err`
$RM conftest*
])# _LT_COMPILER_BOILERPLATE


# _LT_LINKER_BOILERPLATE
# ----------------------
# Check for linker boilerplate output or warnings with
# the simple link test code.
m4_defun([_LT_LINKER_BOILERPLATE],
[m4_require([_LT_DECL_SED])dnl
ac_outfile=conftest.$ac_objext
echo "$lt_simple_link_test_code" >conftest.$ac_ext
eval "$ac_link" 2>&1 >/dev/null | $SED '/^$/d; /^ *+/d' >conftest.err
_lt_linker_boilerplate=`cat conftest.err`
$RM -r conftest*
])# _LT_LINKER_BOILERPLATE

# _LT_REQUIRED_DARWIN_CHECKS
# -------------------------
m4_defun_once([_LT_REQUIRED_DARWIN_CHECKS],[
  case $host_os in
    rhapsody* | darwin*)
    AC_CHECK_TOOL([DSYMUTIL], [dsymutil], [:])
    AC_CHECK_TOOL([NMEDIT], [nmedit], [:])
    AC_CHECK_TOOL([LIPO], [lipo], [:])
    AC_CHECK_TOOL([OTOOL], [otool], [:])
    AC_CHECK_TOOL([OTOOL64], [otool64], [:])
    _LT_DECL([], [DSYMUTIL], [1],
      [Tool to manipulate archived DWARF debug symbol files on Mac OS X])
    _LT_DECL([], [NMEDIT], [1],
      [Tool to change global to local symbols on Mac OS X])
    _LT_DECL([], [LIPO], [1],
      [Tool to manipulate fat objects and archives on Mac OS X])
    _LT_DECL([], [OTOOL], [1],
      [ldd/readelf like tool for Mach-O binaries on Mac OS X])
    _LT_DECL([], [OTOOL64], [1],
      [ldd/readelf like tool for 64 bit Mach-O binaries on Mac OS X 10.4])

    AC_CACHE_CHECK([for -single_module linker flag],[lt_cv_apple_cc_single_mod],
      [lt_cv_apple_cc_single_mod=no
      if test -z "$LT_MULTI_MODULE"; then
	# By default we will add the -single_module flag. You can override
	# by either setting the environment variable LT_MULTI_MODULE
	# non-empty at configure time, or by adding -multi_module to the
	# link flags.
	rm -rf libconftest.dylib*
	echo "int foo(void){return 1;}" > conftest.c
	echo "$LTCC $LTCFLAGS $LDFLAGS -o libconftest.dylib \
-dynamiclib -Wl,-single_module conftest.c" >&AS_MESSAGE_LOG_FD
	$LTCC $LTCFLAGS $LDFLAGS -o libconftest.dylib \
	  -dynamiclib -Wl,-single_module conftest.c 2>conftest.err
        _lt_result=$?
	# If there is a non-empty error log, and "single_module"
	# appears in it, assume the flag caused a linker warning
        if test -s conftest.err && $GREP single_module conftest.err; then
	  cat conftest.err >&AS_MESSAGE_LOG_FD
	# Otherwise, if the output was created with a 0 exit code from
	# the compiler, it worked.
	elif test -f libconftest.dylib && test 0 = "$_lt_result"; then
	  lt_cv_apple_cc_single_mod=yes
	else
	  cat conftest.err >&AS_MESSAGE_LOG_FD
	fi
	rm -rf libconftest.dylib*
	rm -f conftest.*
      fi])

    AC_CACHE_CHECK([for -exported_symbols_list linker flag],
      [lt_cv_ld_exported_symbols_list],
      [lt_cv_ld_exported_symbols_list=no
      save_LDFLAGS=$LDFLAGS
      echo "_main" > conftest.sym
      LDFLAGS="$LDFLAGS -Wl,-exported_symbols_list,conftest.sym"
      AC_LINK_IFELSE([AC_LANG_PROGRAM([],[])],
	[lt_cv_ld_exported_symbols_list=yes],
	[lt_cv_ld_exported_symbols_list=no])
	LDFLAGS=$save_LDFLAGS
    ])

    AC_CACHE_CHECK([for -force_load linker flag],[lt_cv_ld_force_load],
      [lt_cv_ld_force_load=no
      cat > conftest.c << _LT_EOF
int forced_loaded() { return 2;}
_LT_EOF
      echo "$LTCC $LTCFLAGS -c -o conftest.o conftest.c" >&AS_MESSAGE_LOG_FD
      $LTCC $LTCFLAGS -c -o conftest.o conftest.c 2>&AS_MESSAGE_LOG_FD
      echo "$AR cru libconftest.a conftest.o" >&AS_MESSAGE_LOG_FD
      $AR cru libconftest.a conftest.o 2>&AS_MESSAGE_LOG_FD
      echo "$RANLIB libconftest.a" >&AS_MESSAGE_LOG_FD
      $RANLIB libconftest.a 2>&AS_MESSAGE_LOG_FD
      cat > conftest.c << _LT_EOF
int main() { return 0;}
_LT_EOF
      echo "$LTCC $LTCFLAGS $LDFLAGS -o conftest conftest.c -Wl,-force_load,./libconftest.a" >&AS_MESSAGE_LOG_FD
      $LTCC $LTCFLAGS $LDFLAGS -o conftest conftest.c -Wl,-force_load,./libconftest.a 2>conftest.err
      _lt_result=$?
      if test -s conftest.err && $GREP force_load conftest.err; then
	cat conftest.err >&AS_MESSAGE_LOG_FD
      elif test -f conftest && test 0 = "$_lt_result" && $GREP forced_load conftest >/dev/null 2>&1; then
	lt_cv_ld_force_load=yes
      else
	cat conftest.err >&AS_MESSAGE_LOG_FD
      fi
        rm -f conftest.err libconftest.a conftest conftest.c
        rm -rf conftest.dSYM
    ])
    case $host_os in
    rhapsody* | darwin1.[[012]])
      _lt_dar_allow_undefined='$wl-undefined ${wl}suppress' ;;
    darwin1.*)
      _lt_dar_allow_undefined='$wl-flat_namespace $wl-undefined ${wl}suppress' ;;
    darwin*) # darwin 5.x on
      # if running on 10.5 or later, the deployment target defaults
      # to the OS version, if on x86, and 10.4, the deployment
      # target defaults to 10.4. Don't you love it?
      case ${MACOSX_DEPLOYMENT_TARGET-10.0},$host in
	10.0,*86*-darwin8*|10.0,*-darwin[[91]]*)
	  _lt_dar_allow_undefined='$wl-undefined ${wl}dynamic_lookup' ;;
	10.[[012]][[,.]]*)
	  _lt_dar_allow_undefined='$wl-flat_namespace $wl-undefined ${wl}suppress' ;;
	10.*)
	  _lt_dar_allow_undefined='$wl-undefined ${wl}dynamic_lookup' ;;
      esac
    ;;
  esac
    if test yes = "$lt_cv_apple_cc_single_mod"; then
      _lt_dar_single_mod='$single_module'
    fi
    if test yes = "$lt_cv_ld_exported_symbols_list"; then
      _lt_dar_export_syms=' $wl-exported_symbols_list,$output_objdir/$libname-symbols.expsym'
    else
      _lt_dar_export_syms='~$NMEDIT -s $output_objdir/$libname-symbols.expsym $lib'
    fi
    if test : != "$DSYMUTIL" && test no = "$lt_cv_ld_force_load"; then
      _lt_dsymutil='~$DSYMUTIL $lib || :'
    else
      _lt_dsymutil=
    fi
    ;;
  esac
])


# _LT_DARWIN_LINKER_FEATURES([TAG])
# ---------------------------------
# Checks for linker and compiler features on darwin
m4_defun([_LT_DARWIN_LINKER_FEATURES],
[
  m4_require([_LT_REQUIRED_DARWIN_CHECKS])
  _LT_TAGVAR(archive_cmds_need_lc, $1)=no
  _LT_TAGVAR(hardcode_direct, $1)=no
  _LT_TAGVAR(hardcode_automatic, $1)=yes
  _LT_TAGVAR(hardcode_shlibpath_var, $1)=unsupported
  if test yes = "$lt_cv_ld_force_load"; then
    _LT_TAGVAR(whole_archive_flag_spec, $1)='`for conv in $convenience\"\"; do test  -n \"$conv\" && new_convenience=\"$new_convenience $wl-force_load,$conv\"; done; func_echo_all \"$new_convenience\"`'
    m4_case([$1], [F77], [_LT_TAGVAR(compiler_needs_object, $1)=yes],
                  [FC],  [_LT_TAGVAR(compiler_needs_object, $1)=yes])
  else
    _LT_TAGVAR(whole_archive_flag_spec, $1)=''
  fi
  _LT_TAGVAR(link_all_deplibs, $1)=yes
  _LT_TAGVAR(allow_undefined_flag, $1)=$_lt_dar_allow_undefined
  case $cc_basename in
     ifort*|nagfor*) _lt_dar_can_shared=yes ;;
     *) _lt_dar_can_shared=$GCC ;;
  esac
  if test yes = "$_lt_dar_can_shared"; then
    output_verbose_link_cmd=func_echo_all
    _LT_TAGVAR(archive_cmds, $1)="\$CC -dynamiclib \$allow_undefined_flag -o \$lib \$libobjs \$deplibs \$compiler_flags -install_name \$rpath/\$soname \$verstring $_lt_dar_single_mod$_lt_dsymutil"
    _LT_TAGVAR(module_cmds, $1)="\$CC \$allow_undefined_flag -o \$lib -bundle \$libobjs \$deplibs \$compiler_flags$_lt_dsymutil"
    _LT_TAGVAR(archive_expsym_cmds, $1)="sed 's|^|_|' < \$export_symbols > \$output_objdir/\$libname-symbols.expsym~\$CC -dynamiclib \$allow_undefined_flag -o \$lib \$libobjs \$deplibs \$compiler_flags -install_name \$rpath/\$soname \$verstring $_lt_dar_single_mod$_lt_dar_export_syms$_lt_dsymutil"
    _LT_TAGVAR(module_expsym_cmds, $1)="sed -e 's|^|_|' < \$export_symbols > \$output_objdir/\$libname-symbols.expsym~\$CC \$allow_undefined_flag -o \$lib -bundle \$libobjs \$deplibs \$compiler_flags$_lt_dar_export_syms$_lt_dsymutil"
    m4_if([$1], [CXX],
[   if test yes != "$lt_cv_apple_cc_single_mod"; then
      _LT_TAGVAR(archive_cmds, $1)="\$CC -r -keep_private_externs -nostdlib -o \$lib-master.o \$libobjs~\$CC -dynamiclib \$allow_undefined_flag -o \$lib \$lib-master.o \$deplibs \$compiler_flags -install_name \$rpath/\$soname \$verstring$_lt_dsymutil"
      _LT_TAGVAR(archive_expsym_cmds, $1)="sed 's|^|_|' < \$export_symbols > \$output_objdir/\$libname-symbols.expsym~\$CC -r -keep_private_externs -nostdlib -o \$lib-master.o \$libobjs~\$CC -dynamiclib \$allow_undefined_flag -o \$lib \$lib-master.o \$deplibs \$compiler_flags -install_name \$rpath/\$soname \$verstring$_lt_dar_export_syms$_lt_dsymutil"
    fi
],[])
  else
  _LT_TAGVAR(ld_shlibs, $1)=no
  fi
])

# _LT_SYS_MODULE_PATH_AIX([TAGNAME])
# ----------------------------------
# Links a minimal program and checks the executable
# for the system default hardcoded library path. In most cases,
# this is /usr/lib:/lib, but when the MPI compilers are used
# the location of the communication and MPI libs are included too.
# If we don't find anything, use the default library path according
# to the aix ld manual.
# Store the results from the different compilers for each TAGNAME.
# Allow to override them for all tags through lt_cv_aix_libpath.
m4_defun([_LT_SYS_MODULE_PATH_AIX],
[m4_require([_LT_DECL_SED])dnl
if test set = "${lt_cv_aix_libpath+set}"; then
  aix_libpath=$lt_cv_aix_libpath
else
  AC_CACHE_VAL([_LT_TAGVAR([lt_cv_aix_libpath_], [$1])],
  [AC_LINK_IFELSE([AC_LANG_PROGRAM],[
  lt_aix_libpath_sed='[
      /Import File Strings/,/^$/ {
	  /^0/ {
	      s/^0  *\([^ ]*\) *$/\1/
	      p
	  }
      }]'
  _LT_TAGVAR([lt_cv_aix_libpath_], [$1])=`dump -H conftest$ac_exeext 2>/dev/null | $SED -n -e "$lt_aix_libpath_sed"`
  # Check for a 64-bit object if we didn't find anything.
  if test -z "$_LT_TAGVAR([lt_cv_aix_libpath_], [$1])"; then
    _LT_TAGVAR([lt_cv_aix_libpath_], [$1])=`dump -HX64 conftest$ac_exeext 2>/dev/null | $SED -n -e "$lt_aix_libpath_sed"`
  fi],[])
  if test -z "$_LT_TAGVAR([lt_cv_aix_libpath_], [$1])"; then
    _LT_TAGVAR([lt_cv_aix_libpath_], [$1])=/usr/lib:/lib
  fi
  ])
  aix_libpath=$_LT_TAGVAR([lt_cv_aix_libpath_], [$1])
fi
])# _LT_SYS_MODULE_PATH_AIX


# _LT_SHELL_INIT(ARG)
# -------------------
m4_define([_LT_SHELL_INIT],
[m4_divert_text([M4SH-INIT], [$1
])])# _LT_SHELL_INIT



# _LT_PROG_ECHO_BACKSLASH
# -----------------------
# Find how we can fake an echo command that does not interpret backslash.
# In particular, with Autoconf 2.60 or later we add some code to the start
# of the generated configure script that will find a shell with a builtin
# printf (that we can use as an echo command).
m4_defun([_LT_PROG_ECHO_BACKSLASH],
[ECHO='\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\'
ECHO=$ECHO$ECHO$ECHO$ECHO$ECHO
ECHO=$ECHO$ECHO$ECHO$ECHO$ECHO$ECHO

AC_MSG_CHECKING([how to print strings])
# Test print first, because it will be a builtin if present.
if test "X`( print -r -- -n ) 2>/dev/null`" = X-n && \
   test "X`print -r -- $ECHO 2>/dev/null`" = "X$ECHO"; then
  ECHO='print -r --'
elif test "X`printf %s $ECHO 2>/dev/null`" = "X$ECHO"; then
  ECHO='printf %s\n'
else
  # Use this function as a fallback that always works.
  func_fallback_echo ()
  {
    eval 'cat <<_LTECHO_EOF
$[]1
_LTECHO_EOF'
  }
  ECHO='func_fallback_echo'
fi

# func_echo_all arg...
# Invoke $ECHO with all args, space-separated.
func_echo_all ()
{
    $ECHO "$*"
}

case $ECHO in
  printf*) AC_MSG_RESULT([printf]) ;;
  print*) AC_MSG_RESULT([print -r]) ;;
  *) AC_MSG_RESULT([cat]) ;;
esac

m4_ifdef([_AS_DETECT_SUGGESTED],
[_AS_DETECT_SUGGESTED([
  test -n "${ZSH_VERSION+set}${BASH_VERSION+set}" || (
    ECHO='\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\'
    ECHO=$ECHO$ECHO$ECHO$ECHO$ECHO
    ECHO=$ECHO$ECHO$ECHO$ECHO$ECHO$ECHO
    PATH=/empty FPATH=/empty; export PATH FPATH
    test "X`printf %s $ECHO`" = "X$ECHO" \
      || test "X`print -r -- $ECHO`" = "X$ECHO" )])])

_LT_DECL([], [SHELL], [1], [Shell to use when invoking shell scripts])
_LT_DECL([], [ECHO], [1], [An echo program that protects backslashes])
])# _LT_PROG_ECHO_BACKSLASH


# _LT_WITH_SYSROOT
# ----------------
AC_DEFUN([_LT_WITH_SYSROOT],
[AC_MSG_CHECKING([for sysroot])
AC_ARG_WITH([sysroot],
[AS_HELP_STRING([--with-sysroot@<:@=DIR@:>@],
  [Search for dependent libraries within DIR (or the compiler's sysroot
   if not specified).])],
[], [with_sysroot=no])

dnl lt_sysroot will always be passed unquoted.  We quote it here
dnl in case the user passed a directory name.
lt_sysroot=
case $with_sysroot in #(
 yes)
   if test yes = "$GCC"; then
     lt_sysroot=`$CC --print-sysroot 2>/dev/null`
   fi
   ;; #(
 /*)
   lt_sysroot=`echo "$with_sysroot" | sed -e "$sed_quote_subst"`
   ;; #(
 no|'')
   ;; #(
 *)
   AC_MSG_RESULT([$with_sysroot])
   AC_MSG_ERROR([The sysroot must be an absolute path.])
   ;;
esac

 AC_MSG_RESULT([${lt_sysroot:-no}])
_LT_DECL([], [lt_sysroot], [0], [The root where to search for ]dnl
[dependent libraries, and where our libraries should be installed.])])

# _LT_ENABLE_LOCK
# ---------------
m4_defun([_LT_ENABLE_LOCK],
[AC_ARG_ENABLE([libtool-lock],
  [AS_HELP_STRING([--disable-libtool-lock],
    [avoid locking (might break parallel builds)])])
test no = "$enable_libtool_lock" || enable_libtool_lock=yes

# Some flags need to be propagated to the compiler or linker for good
# libtool support.
case $host in
ia64-*-hpux*)
  # Find out what ABI is being produced by ac_compile, and set mode
  # options accordingly.
  echo 'int i;' > conftest.$ac_ext
  if AC_TRY_EVAL(ac_compile); then
    case `/usr/bin/file conftest.$ac_objext` in
      *ELF-32*)
	HPUX_IA64_MODE=32
	;;
      *ELF-64*)
	HPUX_IA64_MODE=64
	;;
    esac
  fi
  rm -rf conftest*
  ;;
*-*-irix6*)
  # Find out what ABI is being produced by ac_compile, and set linker
  # options accordingly.
  echo '[#]line '$LINENO' "configure"' > conftest.$ac_ext
  if AC_TRY_EVAL(ac_compile); then
    if test yes = "$lt_cv_prog_gnu_ld"; then
      case `/usr/bin/file conftest.$ac_objext` in
	*32-bit*)
	  LD="${LD-ld} -melf32bsmip"
	  ;;
	*N32*)
	  LD="${LD-ld} -melf32bmipn32"
	  ;;
	*64-bit*)
	  LD="${LD-ld} -melf64bmip"
	;;
      esac
    else
      case `/usr/bin/file conftest.$ac_objext` in
	*32-bit*)
	  LD="${LD-ld} -32"
	  ;;
	*N32*)
	  LD="${LD-ld} -n32"
	  ;;
	*64-bit*)
	  LD="${LD-ld} -64"
	  ;;
      esac
    fi
  fi
  rm -rf conftest*
  ;;

mips64*-*linux*)
  # Find out what ABI is being produced by ac_compile, and set linker
  # options accordingly.
  echo '[#]line '$LINENO' "configure"' > conftest.$ac_ext
  if AC_TRY_EVAL(ac_compile); then
    emul=elf
    case `/usr/bin/file conftest.$ac_objext` in
      *32-bit*)
	emul="${emul}32"
	;;
      *64-bit*)
	emul="${emul}64"
	;;
    esac
    case `/usr/bin/file conftest.$ac_objext` in
      *MSB*)
	emul="${emul}btsmip"
	;;
      *LSB*)
	emul="${emul}ltsmip"
	;;
    esac
    case `/usr/bin/file conftest.$ac_objext` in
      *N32*)
	emul="${emul}n32"
	;;
    esac
    LD="${LD-ld} -m $emul"
  fi
  rm -rf conftest*
  ;;

x86_64-*kfreebsd*-gnu|x86_64-*linux*|powerpc*-*linux*| \
s390*-*linux*|s390*-*tpf*|sparc*-*linux*)
  # Find out what ABI is being produced by ac_compile, and set linker
  # options accordingly.  Note that the listed cases only cover the
  # situations where additional linker options are needed (such as when
  # doing 32-bit compilation for a host where ld defaults to 64-bit, or
  # vice versa); the common cases where no linker options are needed do
  # not appear in the list.
  echo 'int i;' > conftest.$ac_ext
  if AC_TRY_EVAL(ac_compile); then
    case `/usr/bin/file conftest.o` in
      *32-bit*)
	case $host in
	  x86_64-*kfreebsd*-gnu)
	    LD="${LD-ld} -m elf_i386_fbsd"
	    ;;
	  x86_64-*linux*)
	    case `/usr/bin/file conftest.o` in
	      *x86-64*)
		LD="${LD-ld} -m elf32_x86_64"
		;;
	      *)
		LD="${LD-ld} -m elf_i386"
		;;
	    esac
	    ;;
	  powerpc64le-*linux*)
	    LD="${LD-ld} -m elf32lppclinux"
	    ;;
	  powerpc64-*linux*)
	    LD="${LD-ld} -m elf32ppclinux"
	    ;;
	  s390x-*linux*)
	    LD="${LD-ld} -m elf_s390"
	    ;;
	  sparc64-*linux*)
	    LD="${LD-ld} -m elf32_sparc"
	    ;;
	esac
	;;
      *64-bit*)
	case $host in
	  x86_64-*kfreebsd*-gnu)
	    LD="${LD-ld} -m elf_x86_64_fbsd"
	    ;;
	  x86_64-*linux*)
	    LD="${LD-ld} -m elf_x86_64"
	    ;;
	  powerpcle-*linux*)
	    LD="${LD-ld} -m elf64lppc"
	    ;;
	  powerpc-*linux*)
	    LD="${LD-ld} -m elf64ppc"
	    ;;
	  s390*-*linux*|s390*-*tpf*)
	    LD="${LD-ld} -m elf64_s390"
	    ;;
	  sparc*-*linux*)
	    LD="${LD-ld} -m elf64_sparc"
	    ;;
	esac
	;;
    esac
  fi
  rm -rf conftest*
  ;;

*-*-sco3.2v5*)
  # On SCO OpenServer 5, we need -belf to get full-featured binaries.
  SAVE_CFLAGS=$CFLAGS
  CFLAGS="$CFLAGS -belf"
  AC_CACHE_CHECK([whether the C compiler needs -belf], lt_cv_cc_needs_belf,
    [AC_LANG_PUSH(C)
     AC_LINK_IFELSE([AC_LANG_PROGRAM([[]],[[]])],[lt_cv_cc_needs_belf=yes],[lt_cv_cc_needs_belf=no])
     AC_LANG_POP])
  if test yes != "$lt_cv_cc_needs_belf"; then
    # this is probably gcc 2.8.0, egcs 1.0 or newer; no need for -belf
    CFLAGS=$SAVE_CFLAGS
  fi
  ;;
*-*solaris*)
  # Find out what ABI is being produced by ac_compile, and set linker
  # options accordingly.
  echo 'int i;' > conftest.$ac_ext
  if AC_TRY_EVAL(ac_compile); then
    case `/usr/bin/file conftest.o` in
    *64-bit*)
      case $lt_cv_prog_gnu_ld in
      yes*)
        case $host in
        i?86-*-solaris*|x86_64-*-solaris*)
          LD="${LD-ld} -m elf_x86_64"
          ;;
        sparc*-*-solaris*)
          LD="${LD-ld} -m elf64_sparc"
          ;;
        esac
        # GNU ld 2.21 introduced _sol2 emulations.  Use them if available.
        if ${LD-ld} -V | grep _sol2 >/dev/null 2>&1; then
          LD=${LD-ld}_sol2
        fi
        ;;
      *)
	if ${LD-ld} -64 -r -o conftest2.o conftest.o >/dev/null 2>&1; then
	  LD="${LD-ld} -64"
	fi
	;;
      esac
      ;;
    esac
  fi
  rm -rf conftest*
  ;;
esac

need_locks=$enable_libtool_lock
])# _LT_ENABLE_LOCK


# _LT_PROG_AR
# -----------
m4_defun([_LT_PROG_AR],
[AC_CHECK_TOOLS(AR, [ar], false)
: ${AR=ar}
: ${AR_FLAGS=cru}
_LT_DECL([], [AR], [1], [The archiver])
_LT_DECL([], [AR_FLAGS], [1], [Flags to create an archive])

AC_CACHE_CHECK([for archiver @FILE support], [lt_cv_ar_at_file],
  [lt_cv_ar_at_file=no
   AC_COMPILE_IFELSE([AC_LANG_PROGRAM],
     [echo conftest.$ac_objext > conftest.lst
      lt_ar_try='$AR $AR_FLAGS libconftest.a @conftest.lst >&AS_MESSAGE_LOG_FD'
      AC_TRY_EVAL([lt_ar_try])
      if test 0 -eq "$ac_status"; then
	# Ensure the archiver fails upon bogus file names.
	rm -f conftest.$ac_objext libconftest.a
	AC_TRY_EVAL([lt_ar_try])
	if test 0 -ne "$ac_status"; then
          lt_cv_ar_at_file=@
        fi
      fi
      rm -f conftest.* libconftest.a
     ])
  ])

if test no = "$lt_cv_ar_at_file"; then
  archiver_list_spec=
else
  archiver_list_spec=$lt_cv_ar_at_file
fi
_LT_DECL([], [archiver_list_spec], [1],
  [How to feed a file listing to the archiver])
])# _LT_PROG_AR


# _LT_CMD_OLD_ARCHIVE
# -------------------
m4_defun([_LT_CMD_OLD_ARCHIVE],
[_LT_PROG_AR

AC_CHECK_TOOL(STRIP, strip, :)
test -z "$STRIP" && STRIP=:
_LT_DECL([], [STRIP], [1], [A symbol stripping program])

AC_CHECK_TOOL(RANLIB, ranlib, :)
test -z "$RANLIB" && RANLIB=:
_LT_DECL([], [RANLIB], [1],
    [Commands used to install an old-style archive])

# Determine commands to create old-style static archives.
old_archive_cmds='$AR $AR_FLAGS $oldlib$oldobjs'
old_postinstall_cmds='chmod 644 $oldlib'
old_postuninstall_cmds=

if test -n "$RANLIB"; then
  case $host_os in
  bitrig* | openbsd*)
    old_postinstall_cmds="$old_postinstall_cmds~\$RANLIB -t \$tool_oldlib"
    ;;
  *)
    old_postinstall_cmds="$old_postinstall_cmds~\$RANLIB \$tool_oldlib"
    ;;
  esac
  old_archive_cmds="$old_archive_cmds~\$RANLIB \$tool_oldlib"
fi

case $host_os in
  darwin*)
    lock_old_archive_extraction=yes ;;
  *)
    lock_old_archive_extraction=no ;;
esac
_LT_DECL([], [old_postinstall_cmds], [2])
_LT_DECL([], [old_postuninstall_cmds], [2])
_LT_TAGDECL([], [old_archive_cmds], [2],
    [Commands used to build an old-style archive])
_LT_DECL([], [lock_old_archive_extraction], [0],
    [Whether to use a lock for old archive extraction])
])# _LT_CMD_OLD_ARCHIVE


# _LT_COMPILER_OPTION(MESSAGE, VARIABLE-NAME, FLAGS,
#		[OUTPUT-FILE], [ACTION-SUCCESS], [ACTION-FAILURE])
# ----------------------------------------------------------------
# Check whether the given compiler option works
AC_DEFUN([_LT_COMPILER_OPTION],
[m4_require([_LT_FILEUTILS_DEFAULTS])dnl
m4_require([_LT_DECL_SED])dnl
AC_CACHE_CHECK([$1], [$2],
  [$2=no
   m4_if([$4], , [ac_outfile=conftest.$ac_objext], [ac_outfile=$4])
   echo "$lt_simple_compile_test_code" > conftest.$ac_ext
   lt_compiler_flag="$3"  ## exclude from sc_useless_quotes_in_assignment
   # Insert the option either (1) after the last *FLAGS variable, or
   # (2) before a word containing "conftest.", or (3) at the end.
   # Note that $ac_compile itself does not contain backslashes and begins
   # with a dollar sign (not a hyphen), so the echo should work correctly.
   # The option is referenced via a variable to avoid confusing sed.
   lt_compile=`echo "$ac_compile" | $SED \
   -e 's:.*FLAGS}\{0,1\} :&$lt_compiler_flag :; t' \
   -e 's: [[^ ]]*conftest\.: $lt_compiler_flag&:; t' \
   -e 's:$: $lt_compiler_flag:'`
   (eval echo "\"\$as_me:$LINENO: $lt_compile\"" >&AS_MESSAGE_LOG_FD)
   (eval "$lt_compile" 2>conftest.err)
   ac_status=$?
   cat conftest.err >&AS_MESSAGE_LOG_FD
   echo "$as_me:$LINENO: \$? = $ac_status" >&AS_MESSAGE_LOG_FD
   if (exit $ac_status) && test -s "$ac_outfile"; then
     # The compiler can only warn and ignore the option if not recognized
     # So say no if there are warnings other than the usual output.
     $ECHO "$_lt_compiler_boilerplate" | $SED '/^$/d' >conftest.exp
     $SED '/^$/d; /^ *+/d' conftest.err >conftest.er2
     if test ! -s conftest.er2 || diff conftest.exp conftest.er2 >/dev/null; then
       $2=yes
     fi
   fi
   $RM conftest*
])

if test yes = "[$]$2"; then
    m4_if([$5], , :, [$5])
else
    m4_if([$6], , :, [$6])
fi
])# _LT_COMPILER_OPTION

# Old name:
AU_ALIAS([AC_LIBTOOL_COMPILER_OPTION], [_LT_COMPILER_OPTION])
dnl aclocal-1.4 backwards compatibility:
dnl AC_DEFUN([AC_LIBTOOL_COMPILER_OPTION], [])


# _LT_LINKER_OPTION(MESSAGE, VARIABLE-NAME, FLAGS,
#                  [ACTION-SUCCESS], [ACTION-FAILURE])
# ----------------------------------------------------
# Check whether the given linker option works
AC_DEFUN([_LT_LINKER_OPTION],
[m4_require([_LT_FILEUTILS_DEFAULTS])dnl
m4_require([_LT_DECL_SED])dnl
AC_CACHE_CHECK([$1], [$2],
  [$2=no
   save_LDFLAGS=$LDFLAGS
   LDFLAGS="$LDFLAGS $3"
   echo "$lt_simple_link_test_code" > conftest.$ac_ext
   if (eval $ac_link 2>conftest.err) && test -s conftest$ac_exeext; then
     # The linker can only warn and ignore the option if not recognized
     # So say no if there are warnings
     if test -s conftest.err; then
       # Append any errors to the config.log.
       cat conftest.err 1>&AS_MESSAGE_LOG_FD
       $ECHO "$_lt_linker_boilerplate" | $SED '/^$/d' > conftest.exp
       $SED '/^$/d; /^ *+/d' conftest.err >conftest.er2
       if diff conftest.exp conftest.er2 >/dev/null; then
         $2=yes
       fi
     else
       $2=yes
     fi
   fi
   $RM -r conftest*
   LDFLAGS=$save_LDFLAGS
])

if test yes = "[$]$2"; then
    m4_if([$4], , :, [$4])
else
    m4_if([$5], , :, [$5])
fi
])# _LT_LINKER_OPTION

# Old name:
AU_ALIAS([AC_LIBTOOL_LINKER_OPTION], [_LT_LINKER_OPTION])
dnl aclocal-1.4 backwards compatibility:
dnl AC_DEFUN([AC_LIBTOOL_LINKER_OPTION], [])


# LT_CMD_MAX_LEN
#---------------
AC_DEFUN([LT_CMD_MAX_LEN],
[AC_REQUIRE([AC_CANONICAL_HOST])dnl
# find the maximum length of command line arguments
AC_MSG_CHECKING([the maximum length of command line arguments])
AC_CACHE_VAL([lt_cv_sys_max_cmd_len], [dnl
  i=0
  teststring=ABCD

  case $build_os in
  msdosdjgpp*)
    # On DJGPP, this test can blow up pretty badly due to problems in libc
    # (any single argument exceeding 2000 bytes causes a buffer overrun
    # during glob expansion).  Even if it were fixed, the result of this
    # check would be larger than it should be.
    lt_cv_sys_max_cmd_len=12288;    # 12K is about right
    ;;

  gnu*)
    # Under GNU Hurd, this test is not required because there is
    # no limit to the length of command line arguments.
    # Libtool will interpret -1 as no limit whatsoever
    lt_cv_sys_max_cmd_len=-1;
    ;;

  cygwin* | mingw* | cegcc*)
    # On Win9x/ME, this test blows up -- it succeeds, but takes
    # about 5 minutes as the teststring grows exponentially.
    # Worse, since 9x/ME are not pre-emptively multitasking,
    # you end up with a "frozen" computer, even though with patience
    # the test eventually succeeds (with a max line length of 256k).
    # Instead, let's just punt: use the minimum linelength reported by
    # all of the supported platforms: 8192 (on NT/2K/XP).
    lt_cv_sys_max_cmd_len=8192;
    ;;

  mint*)
    # On MiNT this can take a long time and run out of memory.
    lt_cv_sys_max_cmd_len=8192;
    ;;

  amigaos*)
    # On AmigaOS with pdksh, this test takes hours, literally.
    # So we just punt and use a minimum line length of 8192.
    lt_cv_sys_max_cmd_len=8192;
    ;;

  bitrig* | darwin* | dragonfly* | freebsd* | netbsd* | openbsd*)
    # This has been around since 386BSD, at least.  Likely further.
    if test -x /sbin/sysctl; then
      lt_cv_sys_max_cmd_len=`/sbin/sysctl -n kern.argmax`
    elif test -x /usr/sbin/sysctl; then
      lt_cv_sys_max_cmd_len=`/usr/sbin/sysctl -n kern.argmax`
    else
      lt_cv_sys_max_cmd_len=65536	# usable default for all BSDs
    fi
    # And add a safety zone
    lt_cv_sys_max_cmd_len=`expr $lt_cv_sys_max_cmd_len \/ 4`
    lt_cv_sys_max_cmd_len=`expr $lt_cv_sys_max_cmd_len \* 3`
    ;;

  interix*)
    # We know the value 262144 and hardcode it with a safety zone (like BSD)
    lt_cv_sys_max_cmd_len=196608
    ;;

  os2*)
    # The test takes a long time on OS/2.
    lt_cv_sys_max_cmd_len=8192
    ;;

  osf*)
    # Dr. Hans Ekkehard Plesser reports seeing a kernel panic running configure
    # due to this test when exec_disable_arg_limit is 1 on Tru64. It is not
    # nice to cause kernel panics so lets avoid the loop below.
    # First set a reasonable default.
    lt_cv_sys_max_cmd_len=16384
    #
    if test -x /sbin/sysconfig; then
      case `/sbin/sysconfig -q proc exec_disable_arg_limit` in
        *1*) lt_cv_sys_max_cmd_len=-1 ;;
      esac
    fi
    ;;
  sco3.2v5*)
    lt_cv_sys_max_cmd_len=102400
    ;;
  sysv5* | sco5v6* | sysv4.2uw2*)
    kargmax=`grep ARG_MAX /etc/conf/cf.d/stune 2>/dev/null`
    if test -n "$kargmax"; then
      lt_cv_sys_max_cmd_len=`echo $kargmax | sed 's/.*[[	 ]]//'`
    else
      lt_cv_sys_max_cmd_len=32768
    fi
    ;;
  *)
    lt_cv_sys_max_cmd_len=`(getconf ARG_MAX) 2> /dev/null`
    if test -n "$lt_cv_sys_max_cmd_len" && \
       test undefined != "$lt_cv_sys_max_cmd_len"; then
      lt_cv_sys_max_cmd_len=`expr $lt_cv_sys_max_cmd_len \/ 4`
      lt_cv_sys_max_cmd_len=`expr $lt_cv_sys_max_cmd_len \* 3`
    else
      # Make teststring a little bigger before we do anything with it.
      # a 1K string should be a reasonable start.
      for i in 1 2 3 4 5 6 7 8; do
        teststring=$teststring$teststring
      done
      SHELL=${SHELL-${CONFIG_SHELL-/bin/sh}}
      # If test is not a shell built-in, we'll probably end up computing a
      # maximum length that is only half of the actual maximum length, but
      # we can't tell.
      while { test X`env echo "$teststring$teststring" 2>/dev/null` \
	         = "X$teststring$teststring"; } >/dev/null 2>&1 &&
	      test 17 != "$i" # 1/2 MB should be enough
      do
        i=`expr $i + 1`
        teststring=$teststring$teststring
      done
      # Only check the string length outside the loop.
      lt_cv_sys_max_cmd_len=`expr "X$teststring" : ".*" 2>&1`
      teststring=
      # Add a significant safety factor because C++ compilers can tack on
      # massive amounts of additional arguments before passing them to the
      # linker.  It appears as though 1/2 is a usable value.
      lt_cv_sys_max_cmd_len=`expr $lt_cv_sys_max_cmd_len \/ 2`
    fi
    ;;
  esac
])
if test -n "$lt_cv_sys_max_cmd_len"; then
  AC_MSG_RESULT($lt_cv_sys_max_cmd_len)
else
  AC_MSG_RESULT(none)
fi
max_cmd_len=$lt_cv_sys_max_cmd_len
_LT_DECL([], [max_cmd_len], [0],
    [What is the maximum length of a command?])
])# LT_CMD_MAX_LEN

# Old name:
AU_ALIAS([AC_LIBTOOL_SYS_MAX_CMD_LEN], [LT_CMD_MAX_LEN])
dnl aclocal-1.4 backwards compatibility:
dnl AC_DEFUN([AC_LIBTOOL_SYS_MAX_CMD_LEN], [])


# _LT_HEADER_DLFCN
# ----------------
m4_defun([_LT_HEADER_DLFCN],
[AC_CHECK_HEADERS([dlfcn.h], [], [], [AC_INCLUDES_DEFAULT])dnl
])# _LT_HEADER_DLFCN


# _LT_TRY_DLOPEN_SELF (ACTION-IF-TRUE, ACTION-IF-TRUE-W-USCORE,
#                      ACTION-IF-FALSE, ACTION-IF-CROSS-COMPILING)
# ----------------------------------------------------------------
m4_defun([_LT_TRY_DLOPEN_SELF],
[m4_require([_LT_HEADER_DLFCN])dnl
if test yes = "$cross_compiling"; then :
  [$4]
else
  lt_dlunknown=0; lt_dlno_uscore=1; lt_dlneed_uscore=2
  lt_status=$lt_dlunknown
  cat > conftest.$ac_ext <<_LT_EOF
[#line $LINENO "configure"
#include "confdefs.h"

#if HAVE_DLFCN_H
#include <dlfcn.h>
#endif

#include <stdio.h>

#ifdef RTLD_GLOBAL
#  define LT_DLGLOBAL		RTLD_GLOBAL
#else
#  ifdef DL_GLOBAL
#    define LT_DLGLOBAL		DL_GLOBAL
#  else
#    define LT_DLGLOBAL		0
#  endif
#endif

/* We may have to define LT_DLLAZY_OR_NOW in the command line if we
   find out it does not work in some platform. */
#ifndef LT_DLLAZY_OR_NOW
#  ifdef RTLD_LAZY
#    define LT_DLLAZY_OR_NOW		RTLD_LAZY
#  else
#    ifdef DL_LAZY
#      define LT_DLLAZY_OR_NOW		DL_LAZY
#    else
#      ifdef RTLD_NOW
#        define LT_DLLAZY_OR_NOW	RTLD_NOW
#      else
#        ifdef DL_NOW
#          define LT_DLLAZY_OR_NOW	DL_NOW
#        else
#          define LT_DLLAZY_OR_NOW	0
#        endif
#      endif
#    endif
#  endif
#endif

/* When -fvisibility=hidden is used, assume the code has been annotated
   correspondingly for the symbols needed.  */
#if defined __GNUC__ && (((__GNUC__ == 3) && (__GNUC_MINOR__ >= 3)) || (__GNUC__ > 3))
int fnord () __attribute__((visibility("default")));
#endif

int fnord () { return 42; }
int main ()
{
  void *self = dlopen (0, LT_DLGLOBAL|LT_DLLAZY_OR_NOW);
  int status = $lt_dlunknown;

  if (self)
    {
      if (dlsym (self,"fnord"))       status = $lt_dlno_uscore;
      else
        {
	  if (dlsym( self,"_fnord"))  status = $lt_dlneed_uscore;
          else puts (dlerror ());
	}
      /* dlclose (self); */
    }
  else
    puts (dlerror ());

  return status;
}]
_LT_EOF
  if AC_TRY_EVAL(ac_link) && test -s "conftest$ac_exeext" 2>/dev/null; then
    (./conftest; exit; ) >&AS_MESSAGE_LOG_FD 2>/dev/null
    lt_status=$?
    case x$lt_status in
      x$lt_dlno_uscore) $1 ;;
      x$lt_dlneed_uscore) $2 ;;
      x$lt_dlunknown|x*) $3 ;;
    esac
  else :
    # compilation failed
    $3
  fi
fi
rm -fr conftest*
])# _LT_TRY_DLOPEN_SELF


# LT_SYS_DLOPEN_SELF
# ------------------
AC_DEFUN([LT_SYS_DLOPEN_SELF],
[m4_require([_LT_HEADER_DLFCN])dnl
if test yes != "$enable_dlopen"; then
  enable_dlopen=unknown
  enable_dlopen_self=unknown
  enable_dlopen_self_static=unknown
else
  lt_cv_dlopen=no
  lt_cv_dlopen_libs=

  case $host_os in
  beos*)
    lt_cv_dlopen=load_add_on
    lt_cv_dlopen_libs=
    lt_cv_dlopen_self=yes
    ;;

  mingw* | pw32* | cegcc*)
    lt_cv_dlopen=LoadLibrary
    lt_cv_dlopen_libs=
    ;;

  cygwin*)
    lt_cv_dlopen=dlopen
    lt_cv_dlopen_libs=
    ;;

  darwin*)
    # if libdl is installed we need to link against it
    AC_CHECK_LIB([dl], [dlopen],
		[lt_cv_dlopen=dlopen lt_cv_dlopen_libs=-ldl],[
    lt_cv_dlopen=dyld
    lt_cv_dlopen_libs=
    lt_cv_dlopen_self=yes
    ])
    ;;

  tpf*)
    # Don't try to run any link tests for TPF.  We know it's impossible
    # because TPF is a cross-compiler, and we know how we open DSOs.
    lt_cv_dlopen=dlopen
    lt_cv_dlopen_libs=
    lt_cv_dlopen_self=no
    ;;

  *)
    AC_CHECK_FUNC([shl_load],
	  [lt_cv_dlopen=shl_load],
      [AC_CHECK_LIB([dld], [shl_load],
	    [lt_cv_dlopen=shl_load lt_cv_dlopen_libs=-ldld],
	[AC_CHECK_FUNC([dlopen],
	      [lt_cv_dlopen=dlopen],
	  [AC_CHECK_LIB([dl], [dlopen],
		[lt_cv_dlopen=dlopen lt_cv_dlopen_libs=-ldl],
	    [AC_CHECK_LIB([svld], [dlopen],
		  [lt_cv_dlopen=dlopen lt_cv_dlopen_libs=-lsvld],
	      [AC_CHECK_LIB([dld], [dld_link],
		    [lt_cv_dlopen=dld_link lt_cv_dlopen_libs=-ldld])
	      ])
	    ])
	  ])
	])
      ])
    ;;
  esac

  if test no = "$lt_cv_dlopen"; then
    enable_dlopen=no
  else
    enable_dlopen=yes
  fi

  case $lt_cv_dlopen in
  dlopen)
    save_CPPFLAGS=$CPPFLAGS
    test yes = "$ac_cv_header_dlfcn_h" && CPPFLAGS="$CPPFLAGS -DHAVE_DLFCN_H"

    save_LDFLAGS=$LDFLAGS
    wl=$lt_prog_compiler_wl eval LDFLAGS=\"\$LDFLAGS $export_dynamic_flag_spec\"

    save_LIBS=$LIBS
    LIBS="$lt_cv_dlopen_libs $LIBS"

    AC_CACHE_CHECK([whether a program can dlopen itself],
	  lt_cv_dlopen_self, [dnl
	  _LT_TRY_DLOPEN_SELF(
	    lt_cv_dlopen_self=yes, lt_cv_dlopen_self=yes,
	    lt_cv_dlopen_self=no, lt_cv_dlopen_self=cross)
    ])

    if test yes = "$lt_cv_dlopen_self"; then
      wl=$lt_prog_compiler_wl eval LDFLAGS=\"\$LDFLAGS $lt_prog_compiler_static\"
      AC_CACHE_CHECK([whether a statically linked program can dlopen itself],
	  lt_cv_dlopen_self_static, [dnl
	  _LT_TRY_DLOPEN_SELF(
	    lt_cv_dlopen_self_static=yes, lt_cv_dlopen_self_static=yes,
	    lt_cv_dlopen_self_static=no,  lt_cv_dlopen_self_static=cross)
      ])
    fi

    CPPFLAGS=$save_CPPFLAGS
    LDFLAGS=$save_LDFLAGS
    LIBS=$save_LIBS
    ;;
  esac

  case $lt_cv_dlopen_self in
  yes|no) enable_dlopen_self=$lt_cv_dlopen_self ;;
  *) enable_dlopen_self=unknown ;;
  esac

  case $lt_cv_dlopen_self_static in
  yes|no) enable_dlopen_self_static=$lt_cv_dlopen_self_static ;;
  *) enable_dlopen_self_static=unknown ;;
  esac
fi
_LT_DECL([dlopen_support], [enable_dlopen], [0],
	 [Whether dlopen is supported])
_LT_DECL([dlopen_self], [enable_dlopen_self], [0],
	 [Whether dlopen of programs is supported])
_LT_DECL([dlopen_self_static], [enable_dlopen_self_static], [0],
	 [Whether dlopen of statically linked programs is supported])
])# LT_SYS_DLOPEN_SELF

# Old name:
AU_ALIAS([AC_LIBTOOL_DLOPEN_SELF], [LT_SYS_DLOPEN_SELF])
dnl aclocal-1.4 backwards compatibility:
dnl AC_DEFUN([AC_LIBTOOL_DLOPEN_SELF], [])


# _LT_COMPILER_C_O([TAGNAME])
# ---------------------------
# Check to see if options -c and -o are simultaneously supported by compiler.
# This macro does not hard code the compiler like AC_PROG_CC_C_O.
m4_defun([_LT_COMPILER_C_O],
[m4_require([_LT_DECL_SED])dnl
m4_require([_LT_FILEUTILS_DEFAULTS])dnl
m4_require([_LT_TAG_COMPILER])dnl
AC_CACHE_CHECK([if $compiler supports -c -o file.$ac_objext],
  [_LT_TAGVAR(lt_cv_prog_compiler_c_o, $1)],
  [_LT_TAGVAR(lt_cv_prog_compiler_c_o, $1)=no
   $RM -r conftest 2>/dev/null
   mkdir conftest
   cd conftest
   mkdir out
   echo "$lt_simple_compile_test_code" > conftest.$ac_ext

   lt_compiler_flag="-o out/conftest2.$ac_objext"
   # Insert the option either (1) after the last *FLAGS variable, or
   # (2) before a word containing "conftest.", or (3) at the end.
   # Note that $ac_compile itself does not contain backslashes and begins
   # with a dollar sign (not a hyphen), so the echo should work correctly.
   lt_compile=`echo "$ac_compile" | $SED \
   -e 's:.*FLAGS}\{0,1\} :&$lt_compiler_flag :; t' \
   -e 's: [[^ ]]*conftest\.: $lt_compiler_flag&:; t' \
   -e 's:$: $lt_compiler_flag:'`
   (eval echo "\"\$as_me:$LINENO: $lt_compile\"" >&AS_MESSAGE_LOG_FD)
   (eval "$lt_compile" 2>out/conftest.err)
   ac_status=$?
   cat out/conftest.err >&AS_MESSAGE_LOG_FD
   echo "$as_me:$LINENO: \$? = $ac_status" >&AS_MESSAGE_LOG_FD
   if (exit $ac_status) && test -s out/conftest2.$ac_objext
   then
     # The compiler can only warn and ignore the option if not recognized
     # So say no if there are warnings
     $ECHO "$_lt_compiler_boilerplate" | $SED '/^$/d' > out/conftest.exp
     $SED '/^$/d; /^ *+/d' out/conftest.err >out/conftest.er2
     if test ! -s out/conftest.er2 || diff out/conftest.exp out/conftest.er2 >/dev/null; then
       _LT_TAGVAR(lt_cv_prog_compiler_c_o, $1)=yes
     fi
   fi
   chmod u+w . 2>&AS_MESSAGE_LOG_FD
   $RM conftest*
   # SGI C++ compiler will create directory out/ii_files/ for
   # template instantiation
   test -d out/ii_files && $RM out/ii_files/* && rmdir out/ii_files
   $RM out/* && rmdir out
   cd ..
   $RM -r conftest
   $RM conftest*
])
_LT_TAGDECL([compiler_c_o], [lt_cv_prog_compiler_c_o], [1],
	[Does compiler simultaneously support -c and -o options?])
])# _LT_COMPILER_C_O


# _LT_COMPILER_FILE_LOCKS([TAGNAME])
# ----------------------------------
# Check to see if we can do hard links to lock some files if needed
m4_defun([_LT_COMPILER_FILE_LOCKS],
[m4_require([_LT_ENABLE_LOCK])dnl
m4_require([_LT_FILEUTILS_DEFAULTS])dnl
_LT_COMPILER_C_O([$1])

hard_links=nottested
if test no = "$_LT_TAGVAR(lt_cv_prog_compiler_c_o, $1)" && test no != "$need_locks"; then
  # do not overwrite the value of need_locks provided by the user
  AC_MSG_CHECKING([if we can lock with hard links])
  hard_links=yes
  $RM conftest*
  ln conftest.a conftest.b 2>/dev/null && hard_links=no
  touch conftest.a
  ln conftest.a conftest.b 2>&5 || hard_links=no
  ln conftest.a conftest.b 2>/dev/null && hard_links=no
  AC_MSG_RESULT([$hard_links])
  if test no = "$hard_links"; then
    AC_MSG_WARN(['$CC' does not support '-c -o', so 'make -j' may be unsafe])
    need_locks=warn
  fi
else
  need_locks=no
fi
_LT_DECL([], [need_locks], [1], [Must we lock files when doing compilation?])
])# _LT_COMPILER_FILE_LOCKS


# _LT_CHECK_OBJDIR
# ----------------
m4_defun([_LT_CHECK_OBJDIR],
[AC_CACHE_CHECK([for objdir], [lt_cv_objdir],
[rm -f .libs 2>/dev/null
mkdir .libs 2>/dev/null
if test -d .libs; then
  lt_cv_objdir=.libs
else
  # MS-DOS does not allow filenames that begin with a dot.
  lt_cv_objdir=_libs
fi
rmdir .libs 2>/dev/null])
objdir=$lt_cv_objdir
_LT_DECL([], [objdir], [0],
         [The name of the directory that contains temporary libtool files])dnl
m4_pattern_allow([LT_OBJDIR])dnl
AC_DEFINE_UNQUOTED([LT_OBJDIR], "$lt_cv_objdir/",
  [Define to the sub-directory where libtool stores uninstalled libraries.])
])# _LT_CHECK_OBJDIR


# _LT_LINKER_HARDCODE_LIBPATH([TAGNAME])
# --------------------------------------
# Check hardcoding attributes.
m4_defun([_LT_LINKER_HARDCODE_LIBPATH],
[AC_MSG_CHECKING([how to hardcode library paths into programs])
_LT_TAGVAR(hardcode_action, $1)=
if test -n "$_LT_TAGVAR(hardcode_libdir_flag_spec, $1)" ||
   test -n "$_LT_TAGVAR(runpath_var, $1)" ||
   test yes = "$_LT_TAGVAR(hardcode_automatic, $1)"; then

  # We can hardcode non-existent directories.
  if test no != "$_LT_TAGVAR(hardcode_direct, $1)" &&
     # If the only mechanism to avoid hardcoding is shlibpath_var, we
     # have to relink, otherwise we might link with an installed library
     # when we should be linking with a yet-to-be-installed one
     ## test no != "$_LT_TAGVAR(hardcode_shlibpath_var, $1)" &&
     test no != "$_LT_TAGVAR(hardcode_minus_L, $1)"; then
    # Linking always hardcodes the temporary library directory.
    _LT_TAGVAR(hardcode_action, $1)=relink
  else
    # We can link without hardcoding, and we can hardcode nonexisting dirs.
    _LT_TAGVAR(hardcode_action, $1)=immediate
  fi
else
  # We cannot hardcode anything, or else we can only hardcode existing
  # directories.
  _LT_TAGVAR(hardcode_action, $1)=unsupported
fi
AC_MSG_RESULT([$_LT_TAGVAR(hardcode_action, $1)])

if test relink = "$_LT_TAGVAR(hardcode_action, $1)" ||
   test yes = "$_LT_TAGVAR(inherit_rpath, $1)"; then
  # Fast installation is not supported
  enable_fast_install=no
elif test yes = "$shlibpath_overrides_runpath" ||
     test no = "$enable_shared"; then
  # Fast installation is not necessary
  enable_fast_install=needless
fi
_LT_TAGDECL([], [hardcode_action], [0],
    [How to hardcode a shared library path into an executable])
])# _LT_LINKER_HARDCODE_LIBPATH


# _LT_CMD_STRIPLIB
# ----------------
m4_defun([_LT_CMD_STRIPLIB],
[m4_require([_LT_DECL_EGREP])
striplib=
old_striplib=
AC_MSG_CHECKING([whether stripping libraries is possible])
if test -n "$STRIP" && $STRIP -V 2>&1 | $GREP "GNU strip" >/dev/null; then
  test -z "$old_striplib" && old_striplib="$STRIP --strip-debug"
  test -z "$striplib" && striplib="$STRIP --strip-unneeded"
  AC_MSG_RESULT([yes])
else
# FIXME - insert some real tests, host_os isn't really good enough
  case $host_os in
  darwin*)
    if test -n "$STRIP"; then
      striplib="$STRIP -x"
      old_striplib="$STRIP -S"
      AC_MSG_RESULT([yes])
    else
      AC_MSG_RESULT([no])
    fi
    ;;
  *)
    AC_MSG_RESULT([no])
    ;;
  esac
fi
_LT_DECL([], [old_striplib], [1], [Commands to strip libraries])
_LT_DECL([], [striplib], [1])
])# _LT_CMD_STRIPLIB


# _LT_PREPARE_MUNGE_PATH_LIST
# ---------------------------
# Make sure func_munge_path_list() is defined correctly.
m4_defun([_LT_PREPARE_MUNGE_PATH_LIST],
[[# func_munge_path_list VARIABLE PATH
# -----------------------------------
# VARIABLE is name of variable containing _space_ separated list of
# directories to be munged by the contents of PATH, which is string
# having a format:
# "DIR[:DIR]:"
#       string "DIR[ DIR]" will be prepended to VARIABLE
# ":DIR[:DIR]"
#       string "DIR[ DIR]" will be appended to VARIABLE
# "DIRP[:DIRP]::[DIRA:]DIRA"
#       string "DIRP[ DIRP]" will be prepended to VARIABLE and string
#       "DIRA[ DIRA]" will be appended to VARIABLE
# "DIR[:DIR]"
#       VARIABLE will be replaced by "DIR[ DIR]"
func_munge_path_list ()
{
    case x@S|@2 in
    x)
        ;;
    *:)
        eval @S|@1=\"`$ECHO @S|@2 | $SED 's/:/ /g'` \@S|@@S|@1\"
        ;;
    x:*)
        eval @S|@1=\"\@S|@@S|@1 `$ECHO @S|@2 | $SED 's/:/ /g'`\"
        ;;
    *::*)
        eval @S|@1=\"\@S|@@S|@1\ `$ECHO @S|@2 | $SED -e 's/.*:://' -e 's/:/ /g'`\"
        eval @S|@1=\"`$ECHO @S|@2 | $SED -e 's/::.*//' -e 's/:/ /g'`\ \@S|@@S|@1\"
        ;;
    *)
        eval @S|@1=\"`$ECHO @S|@2 | $SED 's/:/ /g'`\"
        ;;
    esac
}
]])# _LT_PREPARE_PATH_LIST


# _LT_SYS_DYNAMIC_LINKER([TAG])
# -----------------------------
# PORTME Fill in your ld.so characteristics
m4_defun([_LT_SYS_DYNAMIC_LINKER],
[AC_REQUIRE([AC_CANONICAL_HOST])dnl
m4_require([_LT_DECL_EGREP])dnl
m4_require([_LT_FILEUTILS_DEFAULTS])dnl
m4_require([_LT_DECL_OBJDUMP])dnl
m4_require([_LT_DECL_SED])dnl
m4_require([_LT_CHECK_SHELL_FEATURES])dnl
m4_require([_LT_PREPARE_MUNGE_PATH_LIST])dnl
AC_MSG_CHECKING([dynamic linker characteristics])
m4_if([$1],
	[], [
if test yes = "$GCC"; then
  case $host_os in
    darwin*) lt_awk_arg='/^libraries:/,/LR/' ;;
    *) lt_awk_arg='/^libraries:/' ;;
  esac
  case $host_os in
    mingw* | cegcc*) lt_sed_strip_eq='s|=\([[A-Za-z]]:\)|\1|g' ;;
    *) lt_sed_strip_eq='s|=/|/|g' ;;
  esac
  lt_search_path_spec=`$CC -print-search-dirs | awk $lt_awk_arg | $SED -e "s/^libraries://" -e $lt_sed_strip_eq`
  case $lt_search_path_spec in
  *\;*)
    # if the path contains ";" then we assume it to be the separator
    # otherwise default to the standard path separator (i.e. ":") - it is
    # assumed that no part of a normal pathname contains ";" but that should
    # okay in the real world where ";" in dirpaths is itself problematic.
    lt_search_path_spec=`$ECHO "$lt_search_path_spec" | $SED 's/;/ /g'`
    ;;
  *)
    lt_search_path_spec=`$ECHO "$lt_search_path_spec" | $SED "s/$PATH_SEPARATOR/ /g"`
    ;;
  esac
  # Ok, now we have the path, separated by spaces, we can step through it
  # and add multilib dir if necessary...
  lt_tmp_lt_search_path_spec=
  lt_multi_os_dir=/`$CC $CPPFLAGS $CFLAGS $LDFLAGS -print-multi-os-directory 2>/dev/null`
  # ...but if some path component already ends with the multilib dir we assume
  # that all is fine and trust -print-search-dirs as is (GCC 4.2? or newer).
  case "$lt_multi_os_dir; $lt_search_path_spec " in
  "/; "* | "/.; "* | "/./; "* | *"$lt_multi_os_dir "* | *"$lt_multi_os_dir/ "*)
    lt_multi_os_dir=
    ;;
  esac
  for lt_sys_path in $lt_search_path_spec; do
    if test -d "$lt_sys_path$lt_multi_os_dir"; then
      lt_tmp_lt_search_path_spec="$lt_tmp_lt_search_path_spec $lt_sys_path$lt_multi_os_dir"
    elif test -n "$lt_multi_os_dir"; then
      test -d "$lt_sys_path" && \
	lt_tmp_lt_search_path_spec="$lt_tmp_lt_search_path_spec $lt_sys_path"
    fi
  done
  lt_search_path_spec=`$ECHO "$lt_tmp_lt_search_path_spec" | awk '
BEGIN {RS = " "; FS = "/|\n";} {
  lt_foo = "";
  lt_count = 0;
  for (lt_i = NF; lt_i > 0; lt_i--) {
    if ($lt_i != "" && $lt_i != ".") {
      if ($lt_i == "..") {
        lt_count++;
      } else {
        if (lt_count == 0) {
          lt_foo = "/" $lt_i lt_foo;
        } else {
          lt_count--;
        }
      }
    }
  }
  if (lt_foo != "") { lt_freq[[lt_foo]]++; }
  if (lt_freq[[lt_foo]] == 1) { print lt_foo; }
}'`
  # AWK program above erroneously prepends '/' to C:/dos/paths
  # for these hosts.
  case $host_os in
    mingw* | cegcc*) lt_search_path_spec=`$ECHO "$lt_search_path_spec" |\
      $SED 's|/\([[A-Za-z]]:\)|\1|g'` ;;
  esac
  sys_lib_search_path_spec=`$ECHO "$lt_search_path_spec" | $lt_NL2SP`
else
  sys_lib_search_path_spec="/lib /usr/lib /usr/local/lib"
fi])
library_names_spec=
libname_spec='lib$name'
soname_spec=
shrext_cmds=.so
postinstall_cmds=
postuninstall_cmds=
finish_cmds=
finish_eval=
shlibpath_var=
shlibpath_overrides_runpath=unknown
version_type=none
dynamic_linker="$host_os ld.so"
sys_lib_dlsearch_path_spec="/lib /usr/lib"
need_lib_prefix=unknown
hardcode_into_libs=no

# when you set need_version to no, make sure it does not cause -set_version
# flags to be left without arguments
need_version=unknown

AC_ARG_VAR([LT_SYS_LIBRARY_PATH],
[User-defined run-time library search path.])

case $host_os in
aix3*)
  version_type=linux # correct to gnu/linux during the next big refactor
  library_names_spec='$libname$release$shared_ext$versuffix $libname.a'
  shlibpath_var=LIBPATH

  # AIX 3 has no versioning support, so we append a major version to the name.
  soname_spec='$libname$release$shared_ext$major'
  ;;

aix[[4-9]]*)
  version_type=linux # correct to gnu/linux during the next big refactor
  need_lib_prefix=no
  need_version=no
  hardcode_into_libs=yes
  if test ia64 = "$host_cpu"; then
    # AIX 5 supports IA64
    library_names_spec='$libname$release$shared_ext$major $libname$release$shared_ext$versuffix $libname$shared_ext'
    shlibpath_var=LD_LIBRARY_PATH
  else
    # With GCC up to 2.95.x, collect2 would create an import file
    # for dependence libraries.  The import file would start with
    # the line '#! .'.  This would cause the generated library to
    # depend on '.', always an invalid library.  This was fixed in
    # development snapshots of GCC prior to 3.0.
    case $host_os in
      aix4 | aix4.[[01]] | aix4.[[01]].*)
      if { echo '#if __GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ >= 97)'
	   echo ' yes '
	   echo '#endif'; } | $CC -E - | $GREP yes > /dev/null; then
	:
      else
	can_build_shared=no
      fi
      ;;
    esac
    # Using Import Files as archive members, it is possible to support
    # filename-based versioning of shared library archives on AIX. While
    # this would work for both with and without runtime linking, it will
    # prevent static linking of such archives. So we do filename-based
    # shared library versioning with .so extension only, which is used
    # when both runtime linking and shared linking is enabled.
    # Unfortunately, runtime linking may impact performance, so we do
    # not want this to be the default eventually. Also, we use the
    # versioned .so libs for executables only if there is the -brtl
    # linker flag in LDFLAGS as well, or --with-aix-soname=svr4 only.
    # To allow for filename-based versioning support, we need to create
    # libNAME.so.V as an archive file, containing:
    # *) an Import File, referring to the versioned filename of the
    #    archive as well as the shared archive member, telling the
    #    bitwidth (32 or 64) of that shared object, and providing the
    #    list of exported symbols of that shared object, eventually
    #    decorated with the 'weak' keyword
    # *) the shared object with the F_LOADONLY flag set, to really avoid
    #    it being seen by the linker.
    # At run time we better use the real file rather than another symlink,
    # but for link time we create the symlink libNAME.so -> libNAME.so.V

    case $with_aix_soname,$aix_use_runtimelinking in
    # AIX (on Power*) has no versioning support, so currently we cannot hardcode correct
    # soname into executable. Probably we can add versioning support to
    # collect2, so additional links can be useful in future.
    aix,yes) # traditional libtool
      dynamic_linker='AIX unversionable lib.so'
      # If using run time linking (on AIX 4.2 or later) use lib<name>.so
      # instead of lib<name>.a to let people know that these are not
      # typical AIX shared libraries.
      library_names_spec='$libname$release$shared_ext$versuffix $libname$release$shared_ext$major $libname$shared_ext'
      ;;
    aix,no) # traditional AIX only
      dynamic_linker='AIX lib.a[(]lib.so.V[)]'
      # We preserve .a as extension for shared libraries through AIX4.2
      # and later when we are not doing run time linking.
      library_names_spec='$libname$release.a $libname.a'
      soname_spec='$libname$release$shared_ext$major'
      ;;
    svr4,*) # full svr4 only
      dynamic_linker="AIX lib.so.V[(]$shared_archive_member_spec.o[)]"
      library_names_spec='$libname$release$shared_ext$major $libname$shared_ext'
      # We do not specify a path in Import Files, so LIBPATH fires.
      shlibpath_overrides_runpath=yes
      ;;
    *,yes) # both, prefer svr4
      dynamic_linker="AIX lib.so.V[(]$shared_archive_member_spec.o[)], lib.a[(]lib.so.V[)]"
      library_names_spec='$libname$release$shared_ext$major $libname$shared_ext'
      # unpreferred sharedlib libNAME.a needs extra handling
      postinstall_cmds='test -n "$linkname" || linkname="$realname"~func_stripname "" ".so" "$linkname"~$install_shared_prog "$dir/$func_stripname_result.$libext" "$destdir/$func_stripname_result.$libext"~test -z "$tstripme" || test -z "$striplib" || $striplib "$destdir/$func_stripname_result.$libext"'
      postuninstall_cmds='for n in $library_names $old_library; do :; done~func_stripname "" ".so" "$n"~test "$func_stripname_result" = "$n" || func_append rmfiles " $odir/$func_stripname_result.$libext"'
      # We do not specify a path in Import Files, so LIBPATH fires.
      shlibpath_overrides_runpath=yes
      ;;
    *,no) # both, prefer aix
      dynamic_linker="AIX lib.a[(]lib.so.V[)], lib.so.V[(]$shared_archive_member_spec.o[)]"
      library_names_spec='$libname$release.a $libname.a'
      soname_spec='$libname$release$shared_ext$major'
      # unpreferred sharedlib libNAME.so.V and symlink libNAME.so need extra handling
      postinstall_cmds='test -z "$dlname" || $install_shared_prog $dir/$dlname $destdir/$dlname~test -z "$tstripme" || test -z "$striplib" || $striplib $destdir/$dlname~test -n "$linkname" || linkname=$realname~func_stripname "" ".a" "$linkname"~(cd "$destdir" && $LN_S -f $dlname $func_stripname_result.so)'
      postuninstall_cmds='test -z "$dlname" || func_append rmfiles " $odir/$dlname"~for n in $old_library $library_names; do :; done~func_stripname "" ".a" "$n"~func_append rmfiles " $odir/$func_stripname_result.so"'
      ;;
    esac
    shlibpath_var=LIBPATH
  fi
  ;;

amigaos*)
  case $host_cpu in
  powerpc)
    # Since July 2007 AmigaOS4 officially supports .so libraries.
    # When compiling the executable, add -use-dynld -Lsobjs: to the compileline.
    library_names_spec='$libname$release$shared_ext$versuffix $libname$release$shared_ext$major $libname$shared_ext'
    ;;
  m68k)
    library_names_spec='$libname.ixlibrary $libname.a'
    # Create ${libname}_ixlibrary.a entries in /sys/libs.
    finish_eval='for lib in `ls $libdir/*.ixlibrary 2>/dev/null`; do libname=`func_echo_all "$lib" | $SED '\''s%^.*/\([[^/]]*\)\.ixlibrary$%\1%'\''`; $RM /sys/libs/${libname}_ixlibrary.a; $show "cd /sys/libs && $LN_S $lib ${libname}_ixlibrary.a"; cd /sys/libs && $LN_S $lib ${libname}_ixlibrary.a || exit 1; done'
    ;;
  esac
  ;;

beos*)
  library_names_spec='$libname$shared_ext'
  dynamic_linker="$host_os ld.so"
  shlibpath_var=LIBRARY_PATH
  ;;

bsdi[[45]]*)
  version_type=linux # correct to gnu/linux during the next big refactor
  need_version=no
  library_names_spec='$libname$release$shared_ext$versuffix $libname$release$shared_ext$major $libname$shared_ext'
  soname_spec='$libname$release$shared_ext$major'
  finish_cmds='PATH="\$PATH:/sbin" ldconfig $libdir'
  shlibpath_var=LD_LIBRARY_PATH
  sys_lib_search_path_spec="/shlib /usr/lib /usr/X11/lib /usr/contrib/lib /lib /usr/local/lib"
  sys_lib_dlsearch_path_spec="/shlib /usr/lib /usr/local/lib"
  # the default ld.so.conf also contains /usr/contrib/lib and
  # /usr/X11R6/lib (/usr/X11 is a link to /usr/X11R6), but let us allow
  # libtool to hard-code these into programs
  ;;

cygwin* | mingw* | pw32* | cegcc*)
  version_type=windows
  shrext_cmds=.dll
  need_version=no
  need_lib_prefix=no

  case $GCC,$cc_basename in
  yes,*)
    # gcc
    library_names_spec='$libname.dll.a'
    # DLL is installed to $(libdir)/../bin by postinstall_cmds
    postinstall_cmds='base_file=`basename \$file`~
      dlpath=`$SHELL 2>&1 -c '\''. $dir/'\''\$base_file'\''i; echo \$dlname'\''`~
      dldir=$destdir/`dirname \$dlpath`~
      test -d \$dldir || mkdir -p \$dldir~
      $install_prog $dir/$dlname \$dldir/$dlname~
      chmod a+x \$dldir/$dlname~
      if test -n '\''$stripme'\'' && test -n '\''$striplib'\''; then
        eval '\''$striplib \$dldir/$dlname'\'' || exit \$?;
      fi'
    postuninstall_cmds='dldll=`$SHELL 2>&1 -c '\''. $file; echo \$dlname'\''`~
      dlpath=$dir/\$dldll~
       $RM \$dlpath'
    shlibpath_overrides_runpath=yes

    case $host_os in
    cygwin*)
      # Cygwin DLLs use 'cyg' prefix rather than 'lib'
      soname_spec='`echo $libname | sed -e 's/^lib/cyg/'``echo $release | $SED -e 's/[[.]]/-/g'`$versuffix$shared_ext'
m4_if([$1], [],[
      sys_lib_search_path_spec="$sys_lib_search_path_spec /usr/lib/w32api"])
      ;;
    mingw* | cegcc*)
      # MinGW DLLs use traditional 'lib' prefix
      soname_spec='$libname`echo $release | $SED -e 's/[[.]]/-/g'`$versuffix$shared_ext'
      ;;
    pw32*)
      # pw32 DLLs use 'pw' prefix rather than 'lib'
      library_names_spec='`echo $libname | sed -e 's/^lib/pw/'``echo $release | $SED -e 's/[[.]]/-/g'`$versuffix$shared_ext'
      ;;
    esac
    dynamic_linker='Win32 ld.exe'
    ;;

  *,cl*)
    # Native MSVC
    libname_spec='$name'
    soname_spec='$libname`echo $release | $SED -e 's/[[.]]/-/g'`$versuffix$shared_ext'
    library_names_spec='$libname.dll.lib'

    case $build_os in
    mingw*)
      sys_lib_search_path_spec=
      lt_save_ifs=$IFS
      IFS=';'
      for lt_path in $LIB
      do
        IFS=$lt_save_ifs
        # Let DOS variable expansion print the short 8.3 style file name.
        lt_path=`cd "$lt_path" 2>/dev/null && cmd //C "for %i in (".") do @echo %~si"`
        sys_lib_search_path_spec="$sys_lib_search_path_spec $lt_path"
      done
      IFS=$lt_save_ifs
      # Convert to MSYS style.
      sys_lib_search_path_spec=`$ECHO "$sys_lib_search_path_spec" | sed -e 's|\\\\|/|g' -e 's| \\([[a-zA-Z]]\\):| /\\1|g' -e 's|^ ||'`
      ;;
    cygwin*)
      # Convert to unix form, then to dos form, then back to unix form
      # but this time dos style (no spaces!) so that the unix form looks
      # like /cygdrive/c/PROGRA~1:/cygdr...
      sys_lib_search_path_spec=`cygpath --path --unix "$LIB"`
      sys_lib_search_path_spec=`cygpath --path --dos "$sys_lib_search_path_spec" 2>/dev/null`
      sys_lib_search_path_spec=`cygpath --path --unix "$sys_lib_search_path_spec" | $SED -e "s/$PATH_SEPARATOR/ /g"`
      ;;
    *)
      sys_lib_search_path_spec=$LIB
      if $ECHO "$sys_lib_search_path_spec" | [$GREP ';[c-zC-Z]:/' >/dev/null]; then
        # It is most probably a Windows format PATH.
        sys_lib_search_path_spec=`$ECHO "$sys_lib_search_path_spec" | $SED -e 's/;/ /g'`
      else
        sys_lib_search_path_spec=`$ECHO "$sys_lib_search_path_spec" | $SED -e "s/$PATH_SEPARATOR/ /g"`
      fi
      # FIXME: find the short name or the path components, as spaces are
      # common. (e.g. "Program Files" -> "PROGRA~1")
      ;;
    esac

    # DLL is installed to $(libdir)/../bin by postinstall_cmds
    postinstall_cmds='base_file=`basename \$file`~
      dlpath=`$SHELL 2>&1 -c '\''. $dir/'\''\$base_file'\''i; echo \$dlname'\''`~
      dldir=$destdir/`dirname \$dlpath`~
      test -d \$dldir || mkdir -p \$dldir~
      $install_prog $dir/$dlname \$dldir/$dlname'
    postuninstall_cmds='dldll=`$SHELL 2>&1 -c '\''. $file; echo \$dlname'\''`~
      dlpath=$dir/\$dldll~
       $RM \$dlpath'
    shlibpath_overrides_runpath=yes
    dynamic_linker='Win32 link.exe'
    ;;

  *)
    # Assume MSVC wrapper
    library_names_spec='$libname`echo $release | $SED -e 's/[[.]]/-/g'`$versuffix$shared_ext $libname.lib'
    dynamic_linker='Win32 ld.exe'
    ;;
  esac
  # FIXME: first we should search . and the directory the executable is in
  shlibpath_var=PATH
  ;;

darwin* | rhapsody*)
  dynamic_linker="$host_os dyld"
  version_type=darwin
  need_lib_prefix=no
  need_version=no
  library_names_spec='$libname$release$major$shared_ext $libname$shared_ext'
  soname_spec='$libname$release$major$shared_ext'
  shlibpath_overrides_runpath=yes
  shlibpath_var=DYLD_LIBRARY_PATH
  shrext_cmds='`test .$module = .yes && echo .so || echo .dylib`'
m4_if([$1], [],[
  sys_lib_search_path_spec="$sys_lib_search_path_spec /usr/local/lib"])
  sys_lib_dlsearch_path_spec='/usr/local/lib /lib /usr/lib'
  ;;

dgux*)
  version_type=linux # correct to gnu/linux during the next big refactor
  need_lib_prefix=no
  need_version=no
  library_names_spec='$libname$release$shared_ext$versuffix $libname$release$shared_ext$major $libname$shared_ext'
  soname_spec='$libname$release$shared_ext$major'
  shlibpath_var=LD_LIBRARY_PATH
  ;;

freebsd* | dragonfly*)
  # DragonFly does not have aout.  When/if they implement a new
  # versioning mechanism, adjust this.
  if test -x /usr/bin/objformat; then
    objformat=`/usr/bin/objformat`
  else
    case $host_os in
    freebsd[[23]].*) objformat=aout ;;
    *) objformat=elf ;;
    esac
  fi
  version_type=freebsd-$objformat
  case $version_type in
    freebsd-elf*)
      library_names_spec='$libname$release$shared_ext$versuffix $libname$release$shared_ext$major $libname$shared_ext'
      soname_spec='$libname$release$shared_ext$major'
      need_version=no
      need_lib_prefix=no
      ;;
    freebsd-*)
      library_names_spec='$libname$release$shared_ext$versuffix $libname$shared_ext$versuffix'
      need_version=yes
      ;;
  esac
  shlibpath_var=LD_LIBRARY_PATH
  case $host_os in
  freebsd2.*)
    shlibpath_overrides_runpath=yes
    ;;
  freebsd3.[[01]]* | freebsdelf3.[[01]]*)
    shlibpath_overrides_runpath=yes
    hardcode_into_libs=yes
    ;;
  freebsd3.[[2-9]]* | freebsdelf3.[[2-9]]* | \
  freebsd4.[[0-5]] | freebsdelf4.[[0-5]] | freebsd4.1.1 | freebsdelf4.1.1)
    shlibpath_overrides_runpath=no
    hardcode_into_libs=yes
    ;;
  *) # from 4.6 on, and DragonFly
    shlibpath_overrides_runpath=yes
    hardcode_into_libs=yes
    ;;
  esac
  ;;

haiku*)
  version_type=linux # correct to gnu/linux during the next big refactor
  need_lib_prefix=no
  need_version=no
  dynamic_linker="$host_os runtime_loader"
  library_names_spec='$libname$release$shared_ext$versuffix $libname$release$shared_ext$major $libname$shared_ext'
  soname_spec='$libname$release$shared_ext$major'
  shlibpath_var=LIBRARY_PATH
  shlibpath_overrides_runpath=no
  sys_lib_dlsearch_path_spec='/boot/home/config/lib /boot/common/lib /boot/system/lib'
  hardcode_into_libs=yes
  ;;

hpux9* | hpux10* | hpux11*)
  # Give a soname corresponding to the major version so that dld.sl refuses to
  # link against other versions.
  version_type=sunos
  need_lib_prefix=no
  need_version=no
  case $host_cpu in
  ia64*)
    shrext_cmds='.so'
    hardcode_into_libs=yes
    dynamic_linker="$host_os dld.so"
    shlibpath_var=LD_LIBRARY_PATH
    shlibpath_overrides_runpath=yes # Unless +noenvvar is specified.
    library_names_spec='$libname$release$shared_ext$versuffix $libname$release$shared_ext$major $libname$shared_ext'
    soname_spec='$libname$release$shared_ext$major'
    if test 32 = "$HPUX_IA64_MODE"; then
      sys_lib_search_path_spec="/usr/lib/hpux32 /usr/local/lib/hpux32 /usr/local/lib"
      sys_lib_dlsearch_path_spec=/usr/lib/hpux32
    else
      sys_lib_search_path_spec="/usr/lib/hpux64 /usr/local/lib/hpux64"
      sys_lib_dlsearch_path_spec=/usr/lib/hpux64
    fi
    ;;
  hppa*64*)
    shrext_cmds='.sl'
    hardcode_into_libs=yes
    dynamic_linker="$host_os dld.sl"
    shlibpath_var=LD_LIBRARY_PATH # How should we handle SHLIB_PATH
    shlibpath_overrides_runpath=yes # Unless +noenvvar is specified.
    library_names_spec='$libname$release$shared_ext$versuffix $libname$release$shared_ext$major $libname$shared_ext'
    soname_spec='$libname$release$shared_ext$major'
    sys_lib_search_path_spec="/usr/lib/pa20_64 /usr/ccs/lib/pa20_64"
    sys_lib_dlsearch_path_spec=$sys_lib_search_path_spec
    ;;
  *)
    shrext_cmds='.sl'
    dynamic_linker="$host_os dld.sl"
    shlibpath_var=SHLIB_PATH
    shlibpath_overrides_runpath=no # +s is required to enable SHLIB_PATH
    library_names_spec='$libname$release$shared_ext$versuffix $libname$release$shared_ext$major $libname$shared_ext'
    soname_spec='$libname$release$shared_ext$major'
    ;;
  esac
  # HP-UX runs *really* slowly unless shared libraries are mode 555, ...
  postinstall_cmds='chmod 555 $lib'
  # or fails outright, so override atomically:
  install_override_mode=555
  ;;

interix[[3-9]]*)
  version_type=linux # correct to gnu/linux during the next big refactor
  need_lib_prefix=no
  need_version=no
  library_names_spec='$libname$release$shared_ext$versuffix $libname$release$shared_ext$major $libname$shared_ext'
  soname_spec='$libname$release$shared_ext$major'
  dynamic_linker='Interix 3.x ld.so.1 (PE, like ELF)'
  shlibpath_var=LD_LIBRARY_PATH
  shlibpath_overrides_runpath=no
  hardcode_into_libs=yes
  ;;

irix5* | irix6* | nonstopux*)
  case $host_os in
    nonstopux*) version_type=nonstopux ;;
    *)
	if test yes = "$lt_cv_prog_gnu_ld"; then
		version_type=linux # correct to gnu/linux during the next big refactor
	else
		version_type=irix
	fi ;;
  esac
  need_lib_prefix=no
  need_version=no
  soname_spec='$libname$release$shared_ext$major'
  library_names_spec='$libname$release$shared_ext$versuffix $libname$release$shared_ext$major $libname$release$shared_ext $libname$shared_ext'
  case $host_os in
  irix5* | nonstopux*)
    libsuff= shlibsuff=
    ;;
  *)
    case $LD in # libtool.m4 will add one of these switches to LD
    *-32|*"-32 "|*-melf32bsmip|*"-melf32bsmip ")
      libsuff= shlibsuff= libmagic=32-bit;;
    *-n32|*"-n32 "|*-melf32bmipn32|*"-melf32bmipn32 ")
      libsuff=32 shlibsuff=N32 libmagic=N32;;
    *-64|*"-64 "|*-melf64bmip|*"-melf64bmip ")
      libsuff=64 shlibsuff=64 libmagic=64-bit;;
    *) libsuff= shlibsuff= libmagic=never-match;;
    esac
    ;;
  esac
  shlibpath_var=LD_LIBRARY${shlibsuff}_PATH
  shlibpath_overrides_runpath=no
  sys_lib_search_path_spec="/usr/lib$libsuff /lib$libsuff /usr/local/lib$libsuff"
  sys_lib_dlsearch_path_spec="/usr/lib$libsuff /lib$libsuff"
  hardcode_into_libs=yes
  ;;

# No shared lib support for Linux oldld, aout, or coff.
linux*oldld* | linux*aout* | linux*coff*)
  dynamic_linker=no
  ;;

linux*android*)
  version_type=none # Android doesn't support versioned libraries.
  need_lib_prefix=no
  need_version=no
  library_names_spec='$libname$release$shared_ext'
  soname_spec='$libname$release$shared_ext'
  finish_cmds=
  shlibpath_var=LD_LIBRARY_PATH
  shlibpath_overrides_runpath=yes

  # This implies no fast_install, which is unacceptable.
  # Some rework will be needed to allow for fast_install
  # before this can be enabled.
  hardcode_into_libs=yes

  dynamic_linker='Android linker'
  # Don't embed -rpath directories since the linker doesn't support them.
  _LT_TAGVAR(hardcode_libdir_flag_spec, $1)='-L$libdir'
  ;;

# This must be glibc/ELF.
linux* | k*bsd*-gnu | kopensolaris*-gnu | gnu*)
  version_type=linux # correct to gnu/linux during the next big refactor
  need_lib_prefix=no
  need_version=no
  library_names_spec='$libname$release$shared_ext$versuffix $libname$release$shared_ext$major $libname$shared_ext'
  soname_spec='$libname$release$shared_ext$major'
  finish_cmds='PATH="\$PATH:/sbin" ldconfig -n $libdir'
  shlibpath_var=LD_LIBRARY_PATH
  shlibpath_overrides_runpath=no

  # Some binutils ld are patched to set DT_RUNPATH
  AC_CACHE_VAL([lt_cv_shlibpath_overrides_runpath],
    [lt_cv_shlibpath_overrides_runpath=no
    save_LDFLAGS=$LDFLAGS
    save_libdir=$libdir
    eval "libdir=/foo; wl=\"$_LT_TAGVAR(lt_prog_compiler_wl, $1)\"; \
	 LDFLAGS=\"\$LDFLAGS $_LT_TAGVAR(hardcode_libdir_flag_spec, $1)\""
    AC_LINK_IFELSE([AC_LANG_PROGRAM([],[])],
      [AS_IF([ ($OBJDUMP -p conftest$ac_exeext) 2>/dev/null | grep "RUNPATH.*$libdir" >/dev/null],
	 [lt_cv_shlibpath_overrides_runpath=yes])])
    LDFLAGS=$save_LDFLAGS
    libdir=$save_libdir
    ])
  shlibpath_overrides_runpath=$lt_cv_shlibpath_overrides_runpath

  # This implies no fast_install, which is unacceptable.
  # Some rework will be needed to allow for fast_install
  # before this can be enabled.
  hardcode_into_libs=yes

  # Ideally, we could use ldconfig to report *all* directores which are
  # searched for libraries, however this is still not possible.  Aside from not
  # being certain /sbin/ldconfig is available, command
  # 'ldconfig -N -X -v | grep ^/' on 64bit Fedora does not report /usr/lib64,
  # even though it is searched at run-time.  Try to do the best guess by
  # appending ld.so.conf contents (and includes) to the search path.
  if test -f /etc/ld.so.conf; then
    lt_ld_extra=`awk '/^include / { system(sprintf("cd /etc; cat %s 2>/dev/null", \[$]2)); skip = 1; } { if (!skip) print \[$]0; skip = 0; }' < /etc/ld.so.conf | $SED -e 's/#.*//;/^[	 ]*hwcap[	 ]/d;s/[:,	]/ /g;s/=[^=]*$//;s/=[^= ]* / /g;s/"//g;/^$/d' | tr '\n' ' '`
    sys_lib_dlsearch_path_spec="/lib /usr/lib $lt_ld_extra"
  fi

  # We used to test for /lib/ld.so.1 and disable shared libraries on
  # powerpc, because MkLinux only supported shared libraries with the
  # GNU dynamic linker.  Since this was broken with cross compilers,
  # most powerpc-linux boxes support dynamic linking these days and
  # people can always --disable-shared, the test was removed, and we
  # assume the GNU/Linux dynamic linker is in use.
  dynamic_linker='GNU/Linux ld.so'
  ;;

netbsdelf*-gnu)
  version_type=linux
  need_lib_prefix=no
  need_version=no
  library_names_spec='${libname}${release}${shared_ext}$versuffix ${libname}${release}${shared_ext}$major ${libname}${shared_ext}'
  soname_spec='${libname}${release}${shared_ext}$major'
  shlibpath_var=LD_LIBRARY_PATH
  shlibpath_overrides_runpath=no
  hardcode_into_libs=yes
  dynamic_linker='NetBSD ld.elf_so'
  ;;

netbsd*)
  version_type=sunos
  need_lib_prefix=no
  need_version=no
  if echo __ELF__ | $CC -E - | $GREP __ELF__ >/dev/null; then
    library_names_spec='$libname$release$shared_ext$versuffix $libname$shared_ext$versuffix'
    finish_cmds='PATH="\$PATH:/sbin" ldconfig -m $libdir'
    dynamic_linker='NetBSD (a.out) ld.so'
  else
    library_names_spec='$libname$release$shared_ext$versuffix $libname$release$shared_ext$major $libname$shared_ext'
    soname_spec='$libname$release$shared_ext$major'
    dynamic_linker='NetBSD ld.elf_so'
  fi
  shlibpath_var=LD_LIBRARY_PATH
  shlibpath_overrides_runpath=yes
  hardcode_into_libs=yes
  ;;

newsos6)
  version_type=linux # correct to gnu/linux during the next big refactor
  library_names_spec='$libname$release$shared_ext$versuffix $libname$release$shared_ext$major $libname$shared_ext'
  shlibpath_var=LD_LIBRARY_PATH
  shlibpath_overrides_runpath=yes
  ;;

*nto* | *qnx*)
  version_type=qnx
  need_lib_prefix=no
  need_version=no
  library_names_spec='$libname$release$shared_ext$versuffix $libname$release$shared_ext$major $libname$shared_ext'
  soname_spec='$libname$release$shared_ext$major'
  shlibpath_var=LD_LIBRARY_PATH
  shlibpath_overrides_runpath=no
  hardcode_into_libs=yes
  dynamic_linker='ldqnx.so'
  ;;

openbsd* | bitrig*)
  version_type=sunos
  sys_lib_dlsearch_path_spec=/usr/lib
  need_lib_prefix=no
  if test -z "`echo __ELF__ | $CC -E - | $GREP __ELF__`"; then
    need_version=no
  else
    need_version=yes
  fi
  library_names_spec='$libname$release$shared_ext$versuffix $libname$shared_ext$versuffix'
  finish_cmds='PATH="\$PATH:/sbin" ldconfig -m $libdir'
  shlibpath_var=LD_LIBRARY_PATH
  shlibpath_overrides_runpath=yes
  ;;

os2*)
  libname_spec='$name'
  version_type=windows
  shrext_cmds=.dll
  need_version=no
  need_lib_prefix=no
  # OS/2 can only load a DLL with a base name of 8 characters or less.
  soname_spec='`test -n "$os2dllname" && libname="$os2dllname";
    v=$($ECHO $release$versuffix | tr -d .-);
    n=$($ECHO $libname | cut -b -$((8 - ${#v})) | tr . _);
    $ECHO $n$v`$shared_ext'
  library_names_spec='${libname}_dll.$libext'
  dynamic_linker='OS/2 ld.exe'
  shlibpath_var=BEGINLIBPATH
  sys_lib_search_path_spec="/lib /usr/lib /usr/local/lib"
  sys_lib_dlsearch_path_spec=$sys_lib_search_path_spec
  postinstall_cmds='base_file=`basename \$file`~
    dlpath=`$SHELL 2>&1 -c '\''. $dir/'\''\$base_file'\''i; $ECHO \$dlname'\''`~
    dldir=$destdir/`dirname \$dlpath`~
    test -d \$dldir || mkdir -p \$dldir~
    $install_prog $dir/$dlname \$dldir/$dlname~
    chmod a+x \$dldir/$dlname~
    if test -n '\''$stripme'\'' && test -n '\''$striplib'\''; then
      eval '\''$striplib \$dldir/$dlname'\'' || exit \$?;
    fi'
  postuninstall_cmds='dldll=`$SHELL 2>&1 -c '\''. $file; $ECHO \$dlname'\''`~
    dlpath=$dir/\$dldll~
    $RM \$dlpath'
  ;;

osf3* | osf4* | osf5*)
  version_type=osf
  need_lib_prefix=no
  need_version=no
  soname_spec='$libname$release$shared_ext$major'
  library_names_spec='$libname$release$shared_ext$versuffix $libname$release$shared_ext$major $libname$shared_ext'
  shlibpath_var=LD_LIBRARY_PATH
  sys_lib_search_path_spec="/usr/shlib /usr/ccs/lib /usr/lib/cmplrs/cc /usr/lib /usr/local/lib /var/shlib"
  sys_lib_dlsearch_path_spec=$sys_lib_search_path_spec
  ;;

rdos*)
  dynamic_linker=no
  ;;

solaris*)
  version_type=linux # correct to gnu/linux during the next big refactor
  need_lib_prefix=no
  need_version=no
  library_names_spec='$libname$release$shared_ext$versuffix $libname$release$shared_ext$major $libname$shared_ext'
  soname_spec='$libname$release$shared_ext$major'
  shlibpath_var=LD_LIBRARY_PATH
  shlibpath_overrides_runpath=yes
  hardcode_into_libs=yes
  # ldd complains unless libraries are executable
  postinstall_cmds='chmod +x $lib'
  ;;

sunos4*)
  version_type=sunos
  library_names_spec='$libname$release$shared_ext$versuffix $libname$shared_ext$versuffix'
  finish_cmds='PATH="\$PATH:/usr/etc" ldconfig $libdir'
  shlibpath_var=LD_LIBRARY_PATH
  shlibpath_overrides_runpath=yes
  if test yes = "$with_gnu_ld"; then
    need_lib_prefix=no
  fi
  need_version=yes
  ;;

sysv4 | sysv4.3*)
  version_type=linux # correct to gnu/linux during the next big refactor
  library_names_spec='$libname$release$shared_ext$versuffix $libname$release$shared_ext$major $libname$shared_ext'
  soname_spec='$libname$release$shared_ext$major'
  shlibpath_var=LD_LIBRARY_PATH
  case $host_vendor in
    sni)
      shlibpath_overrides_runpath=no
      need_lib_prefix=no
      runpath_var=LD_RUN_PATH
      ;;
    siemens)
      need_lib_prefix=no
      ;;
    motorola)
      need_lib_prefix=no
      need_version=no
      shlibpath_overrides_runpath=no
      sys_lib_search_path_spec='/lib /usr/lib /usr/ccs/lib'
      ;;
  esac
  ;;

sysv4*MP*)
  if test -d /usr/nec; then
    version_type=linux # correct to gnu/linux during the next big refactor
    library_names_spec='$libname$shared_ext.$versuffix $libname$shared_ext.$major $libname$shared_ext'
    soname_spec='$libname$shared_ext.$major'
    shlibpath_var=LD_LIBRARY_PATH
  fi
  ;;

sysv5* | sco3.2v5* | sco5v6* | unixware* | OpenUNIX* | sysv4*uw2*)
  version_type=sco
  need_lib_prefix=no
  need_version=no
  library_names_spec='$libname$release$shared_ext$versuffix $libname$release$shared_ext $libname$shared_ext'
  soname_spec='$libname$release$shared_ext$major'
  shlibpath_var=LD_LIBRARY_PATH
  shlibpath_overrides_runpath=yes
  hardcode_into_libs=yes
  if test yes = "$with_gnu_ld"; then
    sys_lib_search_path_spec='/usr/local/lib /usr/gnu/lib /usr/ccs/lib /usr/lib /lib'
  else
    sys_lib_search_path_spec='/usr/ccs/lib /usr/lib'
    case $host_os in
      sco3.2v5*)
        sys_lib_search_path_spec="$sys_lib_search_path_spec /lib"
	;;
    esac
  fi
  sys_lib_dlsearch_path_spec='/usr/lib'
  ;;

tpf*)
  # TPF is a cross-target only.  Preferred cross-host = GNU/Linux.
  version_type=linux # correct to gnu/linux during the next big refactor
  need_lib_prefix=no
  need_version=no
  library_names_spec='$libname$release$shared_ext$versuffix $libname$release$shared_ext$major $libname$shared_ext'
  shlibpath_var=LD_LIBRARY_PATH
  shlibpath_overrides_runpath=no
  hardcode_into_libs=yes
  ;;

uts4*)
  version_type=linux # correct to gnu/linux during the next big refactor
  library_names_spec='$libname$release$shared_ext$versuffix $libname$release$shared_ext$major $libname$shared_ext'
  soname_spec='$libname$release$shared_ext$major'
  shlibpath_var=LD_LIBRARY_PATH
  ;;

*)
  dynamic_linker=no
  ;;
esac
AC_MSG_RESULT([$dynamic_linker])
test no = "$dynamic_linker" && can_build_shared=no

variables_saved_for_relink="PATH $shlibpath_var $runpath_var"
if test yes = "$GCC"; then
  variables_saved_for_relink="$variables_saved_for_relink GCC_EXEC_PREFIX COMPILER_PATH LIBRARY_PATH"
fi

if test set = "${lt_cv_sys_lib_search_path_spec+set}"; then
  sys_lib_search_path_spec=$lt_cv_sys_lib_search_path_spec
fi

if test set = "${lt_cv_sys_lib_dlsearch_path_spec+set}"; then
  sys_lib_dlsearch_path_spec=$lt_cv_sys_lib_dlsearch_path_spec
fi

# remember unaugmented sys_lib_dlsearch_path content for libtool script decls...
configure_time_dlsearch_path=$sys_lib_dlsearch_path_spec

# ... but it needs LT_SYS_LIBRARY_PATH munging for other configure-time code
func_munge_path_list sys_lib_dlsearch_path_spec "$LT_SYS_LIBRARY_PATH"

# to be used as default LT_SYS_LIBRARY_PATH value in generated libtool
configure_time_lt_sys_library_path=$LT_SYS_LIBRARY_PATH

_LT_DECL([], [variables_saved_for_relink], [1],
    [Variables whose values should be saved in libtool wrapper scripts and
    restored at link time])
_LT_DECL([], [need_lib_prefix], [0],
    [Do we need the "lib" prefix for modules?])
_LT_DECL([], [need_version], [0], [Do we need a version for libraries?])
_LT_DECL([], [version_type], [0], [Library versioning type])
_LT_DECL([], [runpath_var], [0],  [Shared library runtime path variable])
_LT_DECL([], [shlibpath_var], [0],[Shared library path variable])
_LT_DECL([], [shlibpath_overrides_runpath], [0],
    [Is shlibpath searched before the hard-coded library search path?])
_LT_DECL([], [libname_spec], [1], [Format of library name prefix])
_LT_DECL([], [library_names_spec], [1],
    [[List of archive names.  First name is the real one, the rest are links.
    The last name is the one that the linker finds with -lNAME]])
_LT_DECL([], [soname_spec], [1],
    [[The coded name of the library, if different from the real name]])
_LT_DECL([], [install_override_mode], [1],
    [Permission mode override for installation of shared libraries])
_LT_DECL([], [postinstall_cmds], [2],
    [Command to use after installation of a shared archive])
_LT_DECL([], [postuninstall_cmds], [2],
    [Command to use after uninstallation of a shared archive])
_LT_DECL([], [finish_cmds], [2],
    [Commands used to finish a libtool library installation in a directory])
_LT_DECL([], [finish_eval], [1],
    [[As "finish_cmds", except a single script fragment to be evaled but
    not shown]])
_LT_DECL([], [hardcode_into_libs], [0],
    [Whether we should hardcode library paths into libraries])
_LT_DECL([], [sys_lib_search_path_spec], [2],
    [Compile-time system search path for libraries])
_LT_DECL([sys_lib_dlsearch_path_spec], [configure_time_dlsearch_path], [2],
    [Detected run-time system search path for libraries])
_LT_DECL([], [configure_time_lt_sys_library_path], [2],
    [Explicit LT_SYS_LIBRARY_PATH set during ./configure time])
])# _LT_SYS_DYNAMIC_LINKER


# _LT_PATH_TOOL_PREFIX(TOOL)
# --------------------------
# find a file program that can recognize shared library
AC_DEFUN([_LT_PATH_TOOL_PREFIX],
[m4_require([_LT_DECL_EGREP])dnl
AC_MSG_CHECKING([for $1])
AC_CACHE_VAL(lt_cv_path_MAGIC_CMD,
[case $MAGIC_CMD in
[[\\/*] |  ?:[\\/]*])
  lt_cv_path_MAGIC_CMD=$MAGIC_CMD # Let the user override the test with a path.
  ;;
*)
  lt_save_MAGIC_CMD=$MAGIC_CMD
  lt_save_ifs=$IFS; IFS=$PATH_SEPARATOR
dnl $ac_dummy forces splitting on constant user-supplied paths.
dnl POSIX.2 word splitting is done only on the output of word expansions,
dnl not every word.  This closes a longstanding sh security hole.
  ac_dummy="m4_if([$2], , $PATH, [$2])"
  for ac_dir in $ac_dummy; do
    IFS=$lt_save_ifs
    test -z "$ac_dir" && ac_dir=.
    if test -f "$ac_dir/$1"; then
      lt_cv_path_MAGIC_CMD=$ac_dir/"$1"
      if test -n "$file_magic_test_file"; then
	case $deplibs_check_method in
	"file_magic "*)
	  file_magic_regex=`expr "$deplibs_check_method" : "file_magic \(.*\)"`
	  MAGIC_CMD=$lt_cv_path_MAGIC_CMD
	  if eval $file_magic_cmd \$file_magic_test_file 2> /dev/null |
	    $EGREP "$file_magic_regex" > /dev/null; then
	    :
	  else
	    cat <<_LT_EOF 1>&2

*** Warning: the command libtool uses to detect shared libraries,
*** $file_magic_cmd, produces output that libtool cannot recognize.
*** The result is that libtool may fail to recognize shared libraries
*** as such.  This will affect the creation of libtool libraries that
*** depend on shared libraries, but programs linked with such libtool
*** libraries will work regardless of this problem.  Nevertheless, you
*** may want to report the problem to your system manager and/or to
*** bug-libtool@gnu.org

_LT_EOF
	  fi ;;
	esac
      fi
      break
    fi
  done
  IFS=$lt_save_ifs
  MAGIC_CMD=$lt_save_MAGIC_CMD
  ;;
esac])
MAGIC_CMD=$lt_cv_path_MAGIC_CMD
if test -n "$MAGIC_CMD"; then
  AC_MSG_RESULT($MAGIC_CMD)
else
  AC_MSG_RESULT(no)
fi
_LT_DECL([], [MAGIC_CMD], [0],
	 [Used to examine libraries when file_magic_cmd begins with "file"])dnl
])# _LT_PATH_TOOL_PREFIX

# Old name:
AU_ALIAS([AC_PATH_TOOL_PREFIX], [_LT_PATH_TOOL_PREFIX])
dnl aclocal-1.4 backwards compatibility:
dnl AC_DEFUN([AC_PATH_TOOL_PREFIX], [])


# _LT_PATH_MAGIC
# --------------
# find a file program that can recognize a shared library
m4_defun([_LT_PATH_MAGIC],
[_LT_PATH_TOOL_PREFIX(${ac_tool_prefix}file, /usr/bin$PATH_SEPARATOR$PATH)
if test -z "$lt_cv_path_MAGIC_CMD"; then
  if test -n "$ac_tool_prefix"; then
    _LT_PATH_TOOL_PREFIX(file, /usr/bin$PATH_SEPARATOR$PATH)
  else
    MAGIC_CMD=:
  fi
fi
])# _LT_PATH_MAGIC


# LT_PATH_LD
# ----------
# find the pathname to the GNU or non-GNU linker
AC_DEFUN([LT_PATH_LD],
[AC_REQUIRE([AC_PROG_CC])dnl
AC_REQUIRE([AC_CANONICAL_HOST])dnl
AC_REQUIRE([AC_CANONICAL_BUILD])dnl
m4_require([_LT_DECL_SED])dnl
m4_require([_LT_DECL_EGREP])dnl
m4_require([_LT_PROG_ECHO_BACKSLASH])dnl

AC_ARG_WITH([gnu-ld],
    [AS_HELP_STRING([--with-gnu-ld],
	[assume the C compiler uses GNU ld @<:@default=no@:>@])],
    [test no = "$withval" || with_gnu_ld=yes],
    [with_gnu_ld=no])dnl

ac_prog=ld
if test yes = "$GCC"; then
  # Check if gcc -print-prog-name=ld gives a path.
  AC_MSG_CHECKING([for ld used by $CC])
  case $host in
  *-*-mingw*)
    # gcc leaves a trailing carriage return, which upsets mingw
    ac_prog=`($CC -print-prog-name=ld) 2>&5 | tr -d '\015'` ;;
  *)
    ac_prog=`($CC -print-prog-name=ld) 2>&5` ;;
  esac
  case $ac_prog in
    # Accept absolute paths.
    [[\\/]]* | ?:[[\\/]]*)
      re_direlt='/[[^/]][[^/]]*/\.\./'
      # Canonicalize the pathname of ld
      ac_prog=`$ECHO "$ac_prog"| $SED 's%\\\\%/%g'`
      while $ECHO "$ac_prog" | $GREP "$re_direlt" > /dev/null 2>&1; do
	ac_prog=`$ECHO $ac_prog| $SED "s%$re_direlt%/%"`
      done
      test -z "$LD" && LD=$ac_prog
      ;;
  "")
    # If it fails, then pretend we aren't using GCC.
    ac_prog=ld
    ;;
  *)
    # If it is relative, then search for the first ld in PATH.
    with_gnu_ld=unknown
    ;;
  esac
elif test yes = "$with_gnu_ld"; then
  AC_MSG_CHECKING([for GNU ld])
else
  AC_MSG_CHECKING([for non-GNU ld])
fi
AC_CACHE_VAL(lt_cv_path_LD,
[if test -z "$LD"; then
  lt_save_ifs=$IFS; IFS=$PATH_SEPARATOR
  for ac_dir in $PATH; do
    IFS=$lt_save_ifs
    test -z "$ac_dir" && ac_dir=.
    if test -f "$ac_dir/$ac_prog" || test -f "$ac_dir/$ac_prog$ac_exeext"; then
      lt_cv_path_LD=$ac_dir/$ac_prog
      # Check to see if the program is GNU ld.  I'd rather use --version,
      # but apparently some variants of GNU ld only accept -v.
      # Break only if it was the GNU/non-GNU ld that we prefer.
      case `"$lt_cv_path_LD" -v 2>&1 </dev/null` in
      *GNU* | *'with BFD'*)
	test no != "$with_gnu_ld" && break
	;;
      *)
	test yes != "$with_gnu_ld" && break
	;;
      esac
    fi
  done
  IFS=$lt_save_ifs
else
  lt_cv_path_LD=$LD # Let the user override the test with a path.
fi])
LD=$lt_cv_path_LD
if test -n "$LD"; then
  AC_MSG_RESULT($LD)
else
  AC_MSG_RESULT(no)
fi
test -z "$LD" && AC_MSG_ERROR([no acceptable ld found in \$PATH])
_LT_PATH_LD_GNU
AC_SUBST([LD])

_LT_TAGDECL([], [LD], [1], [The linker used to build libraries])
])# LT_PATH_LD

# Old names:
AU_ALIAS([AM_PROG_LD], [LT_PATH_LD])
AU_ALIAS([AC_PROG_LD], [LT_PATH_LD])
dnl aclocal-1.4 backwards compatibility:
dnl AC_DEFUN([AM_PROG_LD], [])
dnl AC_DEFUN([AC_PROG_LD], [])


# _LT_PATH_LD_GNU
#- --------------
m4_defun([_LT_PATH_LD_GNU],
[AC_CACHE_CHECK([if the linker ($LD) is GNU ld], lt_cv_prog_gnu_ld,
[# I'd rather use --version here, but apparently some GNU lds only accept -v.
case `$LD -v 2>&1 </dev/null` in
*GNU* | *'with BFD'*)
  lt_cv_prog_gnu_ld=yes
  ;;
*)
  lt_cv_prog_gnu_ld=no
  ;;
esac])
with_gnu_ld=$lt_cv_prog_gnu_ld
])# _LT_PATH_LD_GNU


# _LT_CMD_RELOAD
# --------------
# find reload flag for linker
#   -- PORTME Some linkers may need a different reload flag.
m4_defun([_LT_CMD_RELOAD],
[AC_CACHE_CHECK([for $LD option to reload object files],
  lt_cv_ld_reload_flag,
  [lt_cv_ld_reload_flag='-r'])
reload_flag=$lt_cv_ld_reload_flag
case $reload_flag in
"" | " "*) ;;
*) reload_flag=" $reload_flag" ;;
esac
reload_cmds='$LD$reload_flag -o $output$reload_objs'
case $host_os in
  cygwin* | mingw* | pw32* | cegcc*)
    if test yes != "$GCC"; then
      reload_cmds=false
    fi
    ;;
  darwin*)
    if test yes = "$GCC"; then
      reload_cmds='$LTCC $LTCFLAGS -nostdlib $wl-r -o $output$reload_objs'
    else
      reload_cmds='$LD$reload_flag -o $output$reload_objs'
    fi
    ;;
esac
_LT_TAGDECL([], [reload_flag], [1], [How to create reloadable object files])dnl
_LT_TAGDECL([], [reload_cmds], [2])dnl
])# _LT_CMD_RELOAD


# _LT_PATH_DD
# -----------
# find a working dd
m4_defun([_LT_PATH_DD],
[AC_CACHE_CHECK([for a working dd], [ac_cv_path_lt_DD],
[printf 0123456789abcdef0123456789abcdef >conftest.i
cat conftest.i conftest.i >conftest2.i
: ${lt_DD:=$DD}
AC_PATH_PROGS_FEATURE_CHECK([lt_DD], [dd],
[if "$ac_path_lt_DD" bs=32 count=1 <conftest2.i >conftest.out 2>/dev/null; then
  cmp -s conftest.i conftest.out \
  && ac_cv_path_lt_DD="$ac_path_lt_DD" ac_path_lt_DD_found=:
fi])
rm -f conftest.i conftest2.i conftest.out])
])# _LT_PATH_DD


# _LT_CMD_TRUNCATE
# ----------------
# find command to truncate a binary pipe
m4_defun([_LT_CMD_TRUNCATE],
[m4_require([_LT_PATH_DD])
AC_CACHE_CHECK([how to truncate binary pipes], [lt_cv_truncate_bin],
[printf 0123456789abcdef0123456789abcdef >conftest.i
cat conftest.i conftest.i >conftest2.i
lt_cv_truncate_bin=
if "$ac_cv_path_lt_DD" bs=32 count=1 <conftest2.i >conftest.out 2>/dev/null; then
  cmp -s conftest.i conftest.out \
  && lt_cv_truncate_bin="$ac_cv_path_lt_DD bs=4096 count=1"
fi
rm -f conftest.i conftest2.i conftest.out
test -z "$lt_cv_truncate_bin" && lt_cv_truncate_bin="$SED -e 4q"])
_LT_DECL([lt_truncate_bin], [lt_cv_truncate_bin], [1],
  [Command to truncate a binary pipe])
])# _LT_CMD_TRUNCATE


# _LT_CHECK_MAGIC_METHOD
# ----------------------
# how to check for library dependencies
#  -- PORTME fill in with the dynamic library characteristics
m4_defun([_LT_CHECK_MAGIC_METHOD],
[m4_require([_LT_DECL_EGREP])
m4_require([_LT_DECL_OBJDUMP])
AC_CACHE_CHECK([how to recognize dependent libraries],
lt_cv_deplibs_check_method,
[lt_cv_file_magic_cmd='$MAGIC_CMD'
lt_cv_file_magic_test_file=
lt_cv_deplibs_check_method='unknown'
# Need to set the preceding variable on all platforms that support
# interlibrary dependencies.
# 'none' -- dependencies not supported.
# 'unknown' -- same as none, but documents that we really don't know.
# 'pass_all' -- all dependencies passed with no checks.
# 'test_compile' -- check by making test program.
# 'file_magic [[regex]]' -- check by looking for files in library path
# that responds to the $file_magic_cmd with a given extended regex.
# If you have 'file' or equivalent on your system and you're not sure
# whether 'pass_all' will *always* work, you probably want this one.

case $host_os in
aix[[4-9]]*)
  lt_cv_deplibs_check_method=pass_all
  ;;

beos*)
  lt_cv_deplibs_check_method=pass_all
  ;;

bsdi[[45]]*)
  lt_cv_deplibs_check_method='file_magic ELF [[0-9]][[0-9]]*-bit [[ML]]SB (shared object|dynamic lib)'
  lt_cv_file_magic_cmd='/usr/bin/file -L'
  lt_cv_file_magic_test_file=/shlib/libc.so
  ;;

cygwin*)
  # func_win32_libid is a shell function defined in ltmain.sh
  lt_cv_deplibs_check_method='file_magic ^x86 archive import|^x86 DLL'
  lt_cv_file_magic_cmd='func_win32_libid'
  ;;

mingw* | pw32*)
  # Base MSYS/MinGW do not provide the 'file' command needed by
  # func_win32_libid shell function, so use a weaker test based on 'objdump',
  # unless we find 'file', for example because we are cross-compiling.
  if ( file / ) >/dev/null 2>&1; then
    lt_cv_deplibs_check_method='file_magic ^x86 archive import|^x86 DLL'
    lt_cv_file_magic_cmd='func_win32_libid'
  else
    # Keep this pattern in sync with the one in func_win32_libid.
    lt_cv_deplibs_check_method='file_magic file format (pei*-i386(.*architecture: i386)?|pe-arm-wince|pe-x86-64)'
    lt_cv_file_magic_cmd='$OBJDUMP -f'
  fi
  ;;

cegcc*)
  # use the weaker test based on 'objdump'. See mingw*.
  lt_cv_deplibs_check_method='file_magic file format pe-arm-.*little(.*architecture: arm)?'
  lt_cv_file_magic_cmd='$OBJDUMP -f'
  ;;

darwin* | rhapsody*)
  lt_cv_deplibs_check_method=pass_all
  ;;

freebsd* | dragonfly*)
  if echo __ELF__ | $CC -E - | $GREP __ELF__ > /dev/null; then
    case $host_cpu in
    i*86 )
      # Not sure whether the presence of OpenBSD here was a mistake.
      # Let's accept both of them until this is cleared up.
      lt_cv_deplibs_check_method='file_magic (FreeBSD|OpenBSD|DragonFly)/i[[3-9]]86 (compact )?demand paged shared library'
      lt_cv_file_magic_cmd=/usr/bin/file
      lt_cv_file_magic_test_file=`echo /usr/lib/libc.so.*`
      ;;
    esac
  else
    lt_cv_deplibs_check_method=pass_all
  fi
  ;;

haiku*)
  lt_cv_deplibs_check_method=pass_all
  ;;

hpux10.20* | hpux11*)
  lt_cv_file_magic_cmd=/usr/bin/file
  case $host_cpu in
  ia64*)
    lt_cv_deplibs_check_method='file_magic (s[[0-9]][[0-9]][[0-9]]|ELF-[[0-9]][[0-9]]) shared object file - IA64'
    lt_cv_file_magic_test_file=/usr/lib/hpux32/libc.so
    ;;
  hppa*64*)
    [lt_cv_deplibs_check_method='file_magic (s[0-9][0-9][0-9]|ELF[ -][0-9][0-9])(-bit)?( [LM]SB)? shared object( file)?[, -]* PA-RISC [0-9]\.[0-9]']
    lt_cv_file_magic_test_file=/usr/lib/pa20_64/libc.sl
    ;;
  *)
    lt_cv_deplibs_check_method='file_magic (s[[0-9]][[0-9]][[0-9]]|PA-RISC[[0-9]]\.[[0-9]]) shared library'
    lt_cv_file_magic_test_file=/usr/lib/libc.sl
    ;;
  esac
  ;;

interix[[3-9]]*)
  # PIC code is broken on Interix 3.x, that's why |\.a not |_pic\.a here
  lt_cv_deplibs_check_method='match_pattern /lib[[^/]]+(\.so|\.a)$'
  ;;

irix5* | irix6* | nonstopux*)
  case $LD in
  *-32|*"-32 ") libmagic=32-bit;;
  *-n32|*"-n32 ") libmagic=N32;;
  *-64|*"-64 ") libmagic=64-bit;;
  *) libmagic=never-match;;
  esac
  lt_cv_deplibs_check_method=pass_all
  ;;

# This must be glibc/ELF.
linux* | k*bsd*-gnu | kopensolaris*-gnu | gnu*)
  lt_cv_deplibs_check_method=pass_all
  ;;

netbsd* | netbsdelf*-gnu)
  if echo __ELF__ | $CC -E - | $GREP __ELF__ > /dev/null; then
    lt_cv_deplibs_check_method='match_pattern /lib[[^/]]+(\.so\.[[0-9]]+\.[[0-9]]+|_pic\.a)$'
  else
    lt_cv_deplibs_check_method='match_pattern /lib[[^/]]+(\.so|_pic\.a)$'
  fi
  ;;

newos6*)
  lt_cv_deplibs_check_method='file_magic ELF [[0-9]][[0-9]]*-bit [[ML]]SB (executable|dynamic lib)'
  lt_cv_file_magic_cmd=/usr/bin/file
  lt_cv_file_magic_test_file=/usr/lib/libnls.so
  ;;

*nto* | *qnx*)
  lt_cv_deplibs_check_method=pass_all
  ;;

openbsd* | bitrig*)
  if test -z "`echo __ELF__ | $CC -E - | $GREP __ELF__`"; then
    lt_cv_deplibs_check_method='match_pattern /lib[[^/]]+(\.so\.[[0-9]]+\.[[0-9]]+|\.so|_pic\.a)$'
  else
    lt_cv_deplibs_check_method='match_pattern /lib[[^/]]+(\.so\.[[0-9]]+\.[[0-9]]+|_pic\.a)$'
  fi
  ;;

osf3* | osf4* | osf5*)
  lt_cv_deplibs_check_method=pass_all
  ;;

rdos*)
  lt_cv_deplibs_check_method=pass_all
  ;;

solaris*)
  lt_cv_deplibs_check_method=pass_all
  ;;

sysv5* | sco3.2v5* | sco5v6* | unixware* | OpenUNIX* | sysv4*uw2*)
  lt_cv_deplibs_check_method=pass_all
  ;;

sysv4 | sysv4.3*)
  case $host_vendor in
  motorola)
    lt_cv_deplibs_check_method='file_magic ELF [[0-9]][[0-9]]*-bit [[ML]]SB (shared object|dynamic lib) M[[0-9]][[0-9]]* Version [[0-9]]'
    lt_cv_file_magic_test_file=`echo /usr/lib/libc.so*`
    ;;
  ncr)
    lt_cv_deplibs_check_method=pass_all
    ;;
  sequent)
    lt_cv_file_magic_cmd='/bin/file'
    lt_cv_deplibs_check_method='file_magic ELF [[0-9]][[0-9]]*-bit [[LM]]SB (shared object|dynamic lib )'
    ;;
  sni)
    lt_cv_file_magic_cmd='/bin/file'
    lt_cv_deplibs_check_method="file_magic ELF [[0-9]][[0-9]]*-bit [[LM]]SB dynamic lib"
    lt_cv_file_magic_test_file=/lib/libc.so
    ;;
  siemens)
    lt_cv_deplibs_check_method=pass_all
    ;;
  pc)
    lt_cv_deplibs_check_method=pass_all
    ;;
  esac
  ;;

tpf*)
  lt_cv_deplibs_check_method=pass_all
  ;;
os2*)
  lt_cv_deplibs_check_method=pass_all
  ;;
esac
])

file_magic_glob=
want_nocaseglob=no
if test "$build" = "$host"; then
  case $host_os in
  mingw* | pw32*)
    if ( shopt | grep nocaseglob ) >/dev/null 2>&1; then
      want_nocaseglob=yes
    else
      file_magic_glob=`echo aAbBcCdDeEfFgGhHiIjJkKlLmMnNoOpPqQrRsStTuUvVwWxXyYzZ | $SED -e "s/\(..\)/s\/[[\1]]\/[[\1]]\/g;/g"`
    fi
    ;;
  esac
fi

file_magic_cmd=$lt_cv_file_magic_cmd
deplibs_check_method=$lt_cv_deplibs_check_method
test -z "$deplibs_check_method" && deplibs_check_method=unknown

_LT_DECL([], [deplibs_check_method], [1],
    [Method to check whether dependent libraries are shared objects])
_LT_DECL([], [file_magic_cmd], [1],
    [Command to use when deplibs_check_method = "file_magic"])
_LT_DECL([], [file_magic_glob], [1],
    [How to find potential files when deplibs_check_method = "file_magic"])
_LT_DECL([], [want_nocaseglob], [1],
    [Find potential files using nocaseglob when deplibs_check_method = "file_magic"])
])# _LT_CHECK_MAGIC_METHOD


# LT_PATH_NM
# ----------
# find the pathname to a BSD- or MS-compatible name lister
AC_DEFUN([LT_PATH_NM],
[AC_REQUIRE([AC_PROG_CC])dnl
AC_CACHE_CHECK([for BSD- or MS-compatible name lister (nm)], lt_cv_path_NM,
[if test -n "$NM"; then
  # Let the user override the test.
  lt_cv_path_NM=$NM
else
  lt_nm_to_check=${ac_tool_prefix}nm
  if test -n "$ac_tool_prefix" && test "$build" = "$host"; then
    lt_nm_to_check="$lt_nm_to_check nm"
  fi
  for lt_tmp_nm in $lt_nm_to_check; do
    lt_save_ifs=$IFS; IFS=$PATH_SEPARATOR
    for ac_dir in $PATH /usr/ccs/bin/elf /usr/ccs/bin /usr/ucb /bin; do
      IFS=$lt_save_ifs
      test -z "$ac_dir" && ac_dir=.
      tmp_nm=$ac_dir/$lt_tmp_nm
      if test -f "$tmp_nm" || test -f "$tmp_nm$ac_exeext"; then
	# Check to see if the nm accepts a BSD-compat flag.
	# Adding the 'sed 1q' prevents false positives on HP-UX, which says:
	#   nm: unknown option "B" ignored
	# Tru64's nm complains that /dev/null is an invalid object file
	# MSYS converts /dev/null to NUL, MinGW nm treats NUL as empty
	case $build_os in
	mingw*) lt_bad_file=conftest.nm/nofile ;;
	*) lt_bad_file=/dev/null ;;
	esac
	case `"$tmp_nm" -B $lt_bad_file 2>&1 | sed '1q'` in
	*$lt_bad_file* | *'Invalid file or object type'*)
	  lt_cv_path_NM="$tmp_nm -B"
	  break 2
	  ;;
	*)
	  case `"$tmp_nm" -p /dev/null 2>&1 | sed '1q'` in
	  */dev/null*)
	    lt_cv_path_NM="$tmp_nm -p"
	    break 2
	    ;;
	  *)
	    lt_cv_path_NM=${lt_cv_path_NM="$tmp_nm"} # keep the first match, but
	    continue # so that we can try to find one that supports BSD flags
	    ;;
	  esac
	  ;;
	esac
      fi
    done
    IFS=$lt_save_ifs
  done
  : ${lt_cv_path_NM=no}
fi])
if test no != "$lt_cv_path_NM"; then
  NM=$lt_cv_path_NM
else
  # Didn't find any BSD compatible name lister, look for dumpbin.
  if test -n "$DUMPBIN"; then :
    # Let the user override the test.
  else
    AC_CHECK_TOOLS(DUMPBIN, [dumpbin "link -dump"], :)
    case `$DUMPBIN -symbols -headers /dev/null 2>&1 | sed '1q'` in
    *COFF*)
      DUMPBIN="$DUMPBIN -symbols -headers"
      ;;
    *)
      DUMPBIN=:
      ;;
    esac
  fi
  AC_SUBST([DUMPBIN])
  if test : != "$DUMPBIN"; then
    NM=$DUMPBIN
  fi
fi
test -z "$NM" && NM=nm
AC_SUBST([NM])
_LT_DECL([], [NM], [1], [A BSD- or MS-compatible name lister])dnl

AC_CACHE_CHECK([the name lister ($NM) interface], [lt_cv_nm_interface],
  [lt_cv_nm_interface="BSD nm"
  echo "int some_variable = 0;" > conftest.$ac_ext
  (eval echo "\"\$as_me:$LINENO: $ac_compile\"" >&AS_MESSAGE_LOG_FD)
  (eval "$ac_compile" 2>conftest.err)
  cat conftest.err >&AS_MESSAGE_LOG_FD
  (eval echo "\"\$as_me:$LINENO: $NM \\\"conftest.$ac_objext\\\"\"" >&AS_MESSAGE_LOG_FD)
  (eval "$NM \"conftest.$ac_objext\"" 2>conftest.err > conftest.out)
  cat conftest.err >&AS_MESSAGE_LOG_FD
  (eval echo "\"\$as_me:$LINENO: output\"" >&AS_MESSAGE_LOG_FD)
  cat conftest.out >&AS_MESSAGE_LOG_FD
  if $GREP 'External.*some_variable' conftest.out > /dev/null; then
    lt_cv_nm_interface="MS dumpbin"
  fi
  rm -f conftest*])
])# LT_PATH_NM

# Old names:
AU_ALIAS([AM_PROG_NM], [LT_PATH_NM])
AU_ALIAS([AC_PROG_NM], [LT_PATH_NM])
dnl aclocal-1.4 backwards compatibility:
dnl AC_DEFUN([AM_PROG_NM], [])
dnl AC_DEFUN([AC_PROG_NM], [])

# _LT_CHECK_SHAREDLIB_FROM_LINKLIB
# --------------------------------
# how to determine the name of the shared library
# associated with a specific link library.
#  -- PORTME fill in with the dynamic library characteristics
m4_defun([_LT_CHECK_SHAREDLIB_FROM_LINKLIB],
[m4_require([_LT_DECL_EGREP])
m4_require([_LT_DECL_OBJDUMP])
m4_require([_LT_DECL_DLLTOOL])
AC_CACHE_CHECK([how to associate runtime and link libraries],
lt_cv_sharedlib_from_linklib_cmd,
[lt_cv_sharedlib_from_linklib_cmd='unknown'

case $host_os in
cygwin* | mingw* | pw32* | cegcc*)
  # two different shell functions defined in ltmain.sh;
  # decide which one to use based on capabilities of $DLLTOOL
  case `$DLLTOOL --help 2>&1` in
  *--identify-strict*)
    lt_cv_sharedlib_from_linklib_cmd=func_cygming_dll_for_implib
    ;;
  *)
    lt_cv_sharedlib_from_linklib_cmd=func_cygming_dll_for_implib_fallback
    ;;
  esac
  ;;
*)
  # fallback: assume linklib IS sharedlib
  lt_cv_sharedlib_from_linklib_cmd=$ECHO
  ;;
esac
])
sharedlib_from_linklib_cmd=$lt_cv_sharedlib_from_linklib_cmd
test -z "$sharedlib_from_linklib_cmd" && sharedlib_from_linklib_cmd=$ECHO

_LT_DECL([], [sharedlib_from_linklib_cmd], [1],
    [Command to associate shared and link libraries])
])# _LT_CHECK_SHAREDLIB_FROM_LINKLIB


# _LT_PATH_MANIFEST_TOOL
# ----------------------
# locate the manifest tool
m4_defun([_LT_PATH_MANIFEST_TOOL],
[AC_CHECK_TOOL(MANIFEST_TOOL, mt, :)
test -z "$MANIFEST_TOOL" && MANIFEST_TOOL=mt
AC_CACHE_CHECK([if $MANIFEST_TOOL is a manifest tool], [lt_cv_path_mainfest_tool],
  [lt_cv_path_mainfest_tool=no
  echo "$as_me:$LINENO: $MANIFEST_TOOL '-?'" >&AS_MESSAGE_LOG_FD
  $MANIFEST_TOOL '-?' 2>conftest.err > conftest.out
  cat conftest.err >&AS_MESSAGE_LOG_FD
  if $GREP 'Manifest Tool' conftest.out > /dev/null; then
    lt_cv_path_mainfest_tool=yes
  fi
  rm -f conftest*])
if test yes != "$lt_cv_path_mainfest_tool"; then
  MANIFEST_TOOL=:
fi
_LT_DECL([], [MANIFEST_TOOL], [1], [Manifest tool])dnl
])# _LT_PATH_MANIFEST_TOOL


# _LT_DLL_DEF_P([FILE])
# ---------------------
# True iff FILE is a Windows DLL '.def' file.
# Keep in sync with func_dll_def_p in the libtool script
AC_DEFUN([_LT_DLL_DEF_P],
[dnl
  test DEF = "`$SED -n dnl
    -e '\''s/^[[	 ]]*//'\'' dnl Strip leading whitespace
    -e '\''/^\(;.*\)*$/d'\'' dnl      Delete empty lines and comments
    -e '\''s/^\(EXPORTS\|LIBRARY\)\([[	 ]].*\)*$/DEF/p'\'' dnl
    -e q dnl                          Only consider the first "real" line
    $1`" dnl
])# _LT_DLL_DEF_P


# LT_LIB_M
# --------
# check for math library
AC_DEFUN([LT_LIB_M],
[AC_REQUIRE([AC_CANONICAL_HOST])dnl
LIBM=
case $host in
*-*-beos* | *-*-cegcc* | *-*-cygwin* | *-*-haiku* | *-*-pw32* | *-*-darwin*)
  # These system don't have libm, or don't need it
  ;;
*-ncr-sysv4.3*)
  AC_CHECK_LIB(mw, _mwvalidcheckl, LIBM=-lmw)
  AC_CHECK_LIB(m, cos, LIBM="$LIBM -lm")
  ;;
*)
  AC_CHECK_LIB(m, cos, LIBM=-lm)
  ;;
esac
AC_SUBST([LIBM])
])# LT_LIB_M

# Old name:
AU_ALIAS([AC_CHECK_LIBM], [LT_LIB_M])
dnl aclocal-1.4 backwards compatibility:
dnl AC_DEFUN([AC_CHECK_LIBM], [])


# _LT_COMPILER_NO_RTTI([TAGNAME])
# -------------------------------
m4_defun([_LT_COMPILER_NO_RTTI],
[m4_require([_LT_TAG_COMPILER])dnl

_LT_TAGVAR(lt_prog_compiler_no_builtin_flag, $1)=

if test yes = "$GCC"; then
  case $cc_basename in
  nvcc*)
    _LT_TAGVAR(lt_prog_compiler_no_builtin_flag, $1)=' -Xcompiler -fno-builtin' ;;
  *)
    _LT_TAGVAR(lt_prog_compiler_no_builtin_flag, $1)=' -fno-builtin' ;;
  esac

  _LT_COMPILER_OPTION([if $compiler supports -fno-rtti -fno-exceptions],
    lt_cv_prog_compiler_rtti_exceptions,
    [-fno-rtti -fno-exceptions], [],
    [_LT_TAGVAR(lt_prog_compiler_no_builtin_flag, $1)="$_LT_TAGVAR(lt_prog_compiler_no_builtin_flag, $1) -fno-rtti -fno-exceptions"])
fi
_LT_TAGDECL([no_builtin_flag], [lt_prog_compiler_no_builtin_flag], [1],
	[Compiler flag to turn off builtin functions])
])# _LT_COMPILER_NO_RTTI


# _LT_CMD_GLOBAL_SYMBOLS
# ----------------------
m4_defun([_LT_CMD_GLOBAL_SYMBOLS],
[AC_REQUIRE([AC_CANONICAL_HOST])dnl
AC_REQUIRE([AC_PROG_CC])dnl
AC_REQUIRE([AC_PROG_AWK])dnl
AC_REQUIRE([LT_PATH_NM])dnl
AC_REQUIRE([LT_PATH_LD])dnl
m4_require([_LT_DECL_SED])dnl
m4_require([_LT_DECL_EGREP])dnl
m4_require([_LT_TAG_COMPILER])dnl

# Check for command to grab the raw symbol name followed by C symbol from nm.
AC_MSG_CHECKING([command to parse $NM output from $compiler object])
AC_CACHE_VAL([lt_cv_sys_global_symbol_pipe],
[
# These are sane defaults that work on at least a few old systems.
# [They come from Ultrix.  What could be older than Ultrix?!! ;)]

# Character class describing NM global symbol codes.
symcode='[[BCDEGRST]]'

# Regexp to match symbols that can be accessed directly from C.
sympat='\([[_A-Za-z]][[_A-Za-z0-9]]*\)'

# Define system-specific variables.
case $host_os in
aix*)
  symcode='[[BCDT]]'
  ;;
cygwin* | mingw* | pw32* | cegcc*)
  symcode='[[ABCDGISTW]]'
  ;;
hpux*)
  if test ia64 = "$host_cpu"; then
    symcode='[[ABCDEGRST]]'
  fi
  ;;
irix* | nonstopux*)
  symcode='[[BCDEGRST]]'
  ;;
osf*)
  symcode='[[BCDEGQRST]]'
  ;;
solaris*)
  symcode='[[BDRT]]'
  ;;
sco3.2v5*)
  symcode='[[DT]]'
  ;;
sysv4.2uw2*)
  symcode='[[DT]]'
  ;;
sysv5* | sco5v6* | unixware* | OpenUNIX*)
  symcode='[[ABDT]]'
  ;;
sysv4)
  symcode='[[DFNSTU]]'
  ;;
esac

# If we're using GNU nm, then use its standard symbol codes.
case `$NM -V 2>&1` in
*GNU* | *'with BFD'*)
  symcode='[[ABCDGIRSTW]]' ;;
esac

if test "$lt_cv_nm_interface" = "MS dumpbin"; then
  # Gets list of data symbols to import.
  lt_cv_sys_global_symbol_to_import="sed -n -e 's/^I .* \(.*\)$/\1/p'"
  # Adjust the below global symbol transforms to fixup imported variables.
  lt_cdecl_hook=" -e 's/^I .* \(.*\)$/extern __declspec(dllimport) char \1;/p'"
  lt_c_name_hook=" -e 's/^I .* \(.*\)$/  {\"\1\", (void *) 0},/p'"
  lt_c_name_lib_hook="\
  -e 's/^I .* \(lib.*\)$/  {\"\1\", (void *) 0},/p'\
  -e 's/^I .* \(.*\)$/  {\"lib\1\", (void *) 0},/p'"
else
  # Disable hooks by default.
  lt_cv_sys_global_symbol_to_import=
  lt_cdecl_hook=
  lt_c_name_hook=
  lt_c_name_lib_hook=
fi

# Transform an extracted symbol line into a proper C declaration.
# Some systems (esp. on ia64) link data and code symbols differently,
# so use this general approach.
lt_cv_sys_global_symbol_to_cdecl="sed -n"\
$lt_cdecl_hook\
" -e 's/^T .* \(.*\)$/extern int \1();/p'"\
" -e 's/^$symcode$symcode* .* \(.*\)$/extern char \1;/p'"

# Transform an extracted symbol line into symbol name and symbol address
lt_cv_sys_global_symbol_to_c_name_address="sed -n"\
$lt_c_name_hook\
" -e 's/^: \(.*\) .*$/  {\"\1\", (void *) 0},/p'"\
" -e 's/^$symcode$symcode* .* \(.*\)$/  {\"\1\", (void *) \&\1},/p'"

# Transform an extracted symbol line into symbol name with lib prefix and
# symbol address.
lt_cv_sys_global_symbol_to_c_name_address_lib_prefix="sed -n"\
$lt_c_name_lib_hook\
" -e 's/^: \(.*\) .*$/  {\"\1\", (void *) 0},/p'"\
" -e 's/^$symcode$symcode* .* \(lib.*\)$/  {\"\1\", (void *) \&\1},/p'"\
" -e 's/^$symcode$symcode* .* \(.*\)$/  {\"lib\1\", (void *) \&\1},/p'"

# Handle CRLF in mingw tool chain
opt_cr=
case $build_os in
mingw*)
  opt_cr=`$ECHO 'x\{0,1\}' | tr x '\015'` # option cr in regexp
  ;;
esac

# Try without a prefix underscore, then with it.
for ac_symprfx in "" "_"; do

  # Transform symcode, sympat, and symprfx into a raw symbol and a C symbol.
  symxfrm="\\1 $ac_symprfx\\2 \\2"

  # Write the raw and C identifiers.
  if test "$lt_cv_nm_interface" = "MS dumpbin"; then
    # Fake it for dumpbin and say T for any non-static function,
    # D for any global variable and I for any imported variable.
    # Also find C++ and __fastcall symbols from MSVC++,
    # which start with @ or ?.
    lt_cv_sys_global_symbol_pipe="$AWK ['"\
"     {last_section=section; section=\$ 3};"\
"     /^COFF SYMBOL TABLE/{for(i in hide) delete hide[i]};"\
"     /Section length .*#relocs.*(pick any)/{hide[last_section]=1};"\
"     /^ *Symbol name *: /{split(\$ 0,sn,\":\"); si=substr(sn[2],2)};"\
"     /^ *Type *: code/{print \"T\",si,substr(si,length(prfx))};"\
"     /^ *Type *: data/{print \"I\",si,substr(si,length(prfx))};"\
"     \$ 0!~/External *\|/{next};"\
"     / 0+ UNDEF /{next}; / UNDEF \([^|]\)*()/{next};"\
"     {if(hide[section]) next};"\
"     {f=\"D\"}; \$ 0~/\(\).*\|/{f=\"T\"};"\
"     {split(\$ 0,a,/\||\r/); split(a[2],s)};"\
"     s[1]~/^[@?]/{print f,s[1],s[1]; next};"\
"     s[1]~prfx {split(s[1],t,\"@\"); print f,t[1],substr(t[1],length(prfx))}"\
"     ' prfx=^$ac_symprfx]"
  else
    lt_cv_sys_global_symbol_pipe="sed -n -e 's/^.*[[	 ]]\($symcode$symcode*\)[[	 ]][[	 ]]*$ac_symprfx$sympat$opt_cr$/$symxfrm/p'"
  fi
  lt_cv_sys_global_symbol_pipe="$lt_cv_sys_global_symbol_pipe | sed '/ __gnu_lto/d'"

  # Check to see that the pipe works correctly.
  pipe_works=no

  rm -f conftest*
  cat > conftest.$ac_ext <<_LT_EOF
#ifdef __cplusplus
extern "C" {
#endif
char nm_test_var;
void nm_test_func(void);
void nm_test_func(void){}
#ifdef __cplusplus
}
#endif
int main(){nm_test_var='a';nm_test_func();return(0);}
_LT_EOF

  if AC_TRY_EVAL(ac_compile); then
    # Now try to grab the symbols.
    nlist=conftest.nm
    if AC_TRY_EVAL(NM conftest.$ac_objext \| "$lt_cv_sys_global_symbol_pipe" \> $nlist) && test -s "$nlist"; then
      # Try sorting and uniquifying the output.
      if sort "$nlist" | uniq > "$nlist"T; then
	mv -f "$nlist"T "$nlist"
      else
	rm -f "$nlist"T
      fi

      # Make sure that we snagged all the symbols we need.
      if $GREP ' nm_test_var$' "$nlist" >/dev/null; then
	if $GREP ' nm_test_func$' "$nlist" >/dev/null; then
	  cat <<_LT_EOF > conftest.$ac_ext
/* Keep this code in sync between libtool.m4, ltmain, lt_system.h, and tests.  */
#if defined _WIN32 || defined __CYGWIN__ || defined _WIN32_WCE
/* DATA imports from DLLs on WIN32 can't be const, because runtime
   relocations are performed -- see ld's documentation on pseudo-relocs.  */
# define LT@&t@_DLSYM_CONST
#elif defined __osf__
/* This system does not cope well with relocations in const data.  */
# define LT@&t@_DLSYM_CONST
#else
# define LT@&t@_DLSYM_CONST const
#endif

#ifdef __cplusplus
extern "C" {
#endif

_LT_EOF
	  # Now generate the symbol file.
	  eval "$lt_cv_sys_global_symbol_to_cdecl"' < "$nlist" | $GREP -v main >> conftest.$ac_ext'

	  cat <<_LT_EOF >> conftest.$ac_ext

/* The mapping between symbol names and symbols.  */
LT@&t@_DLSYM_CONST struct {
  const char *name;
  void       *address;
}
lt__PROGRAM__LTX_preloaded_symbols[[]] =
{
  { "@PROGRAM@", (void *) 0 },
_LT_EOF
	  $SED "s/^$symcode$symcode* .* \(.*\)$/  {\"\1\", (void *) \&\1},/" < "$nlist" | $GREP -v main >> conftest.$ac_ext
	  cat <<\_LT_EOF >> conftest.$ac_ext
  {0, (void *) 0}
};

/* This works around a problem in FreeBSD linker */
#ifdef FREEBSD_WORKAROUND
static const void *lt_preloaded_setup() {
  return lt__PROGRAM__LTX_preloaded_symbols;
}
#endif

#ifdef __cplusplus
}
#endif
_LT_EOF
	  # Now try linking the two files.
	  mv conftest.$ac_objext conftstm.$ac_objext
	  lt_globsym_save_LIBS=$LIBS
	  lt_globsym_save_CFLAGS=$CFLAGS
	  LIBS=conftstm.$ac_objext
	  CFLAGS="$CFLAGS$_LT_TAGVAR(lt_prog_compiler_no_builtin_flag, $1)"
	  if AC_TRY_EVAL(ac_link) && test -s conftest$ac_exeext; then
	    pipe_works=yes
	  fi
	  LIBS=$lt_globsym_save_LIBS
	  CFLAGS=$lt_globsym_save_CFLAGS
	else
	  echo "cannot find nm_test_func in $nlist" >&AS_MESSAGE_LOG_FD
	fi
      else
	echo "cannot find nm_test_var in $nlist" >&AS_MESSAGE_LOG_FD
      fi
    else
      echo "cannot run $lt_cv_sys_global_symbol_pipe" >&AS_MESSAGE_LOG_FD
    fi
  else
    echo "$progname: failed program was:" >&AS_MESSAGE_LOG_FD
    cat conftest.$ac_ext >&5
  fi
  rm -rf conftest* conftst*

  # Do not use the global_symbol_pipe unless it works.
  if test yes = "$pipe_works"; then
    break
  else
    lt_cv_sys_global_symbol_pipe=
  fi
done
])
if test -z "$lt_cv_sys_global_symbol_pipe"; then
  lt_cv_sys_global_symbol_to_cdecl=
fi
if test -z "$lt_cv_sys_global_symbol_pipe$lt_cv_sys_global_symbol_to_cdecl"; then
  AC_MSG_RESULT(failed)
else
  AC_MSG_RESULT(ok)
fi

# Response file support.
if test "$lt_cv_nm_interface" = "MS dumpbin"; then
  nm_file_list_spec='@'
elif $NM --help 2>/dev/null | grep '[[@]]FILE' >/dev/null; then
  nm_file_list_spec='@'
fi

_LT_DECL([global_symbol_pipe], [lt_cv_sys_global_symbol_pipe], [1],
    [Take the output of nm and produce a listing of raw symbols and C names])
_LT_DECL([global_symbol_to_cdecl], [lt_cv_sys_global_symbol_to_cdecl], [1],
    [Transform the output of nm in a proper C declaration])
_LT_DECL([global_symbol_to_import], [lt_cv_sys_global_symbol_to_import], [1],
    [Transform the output of nm into a list of symbols to manually relocate])
_LT_DECL([global_symbol_to_c_name_address],
    [lt_cv_sys_global_symbol_to_c_name_address], [1],
    [Transform the output of nm in a C name address pair])
_LT_DECL([global_symbol_to_c_name_address_lib_prefix],
    [lt_cv_sys_global_symbol_to_c_name_address_lib_prefix], [1],
    [Transform the output of nm in a C name address pair when lib prefix is needed])
_LT_DECL([nm_interface], [lt_cv_nm_interface], [1],
    [The name lister interface])
_LT_DECL([], [nm_file_list_spec], [1],
    [Specify filename containing input files for $NM])
]) # _LT_CMD_GLOBAL_SYMBOLS


# _LT_COMPILER_PIC([TAGNAME])
# ---------------------------
m4_defun([_LT_COMPILER_PIC],
[m4_require([_LT_TAG_COMPILER])dnl
_LT_TAGVAR(lt_prog_compiler_wl, $1)=
_LT_TAGVAR(lt_prog_compiler_pic, $1)=
_LT_TAGVAR(lt_prog_compiler_static, $1)=

m4_if([$1], [CXX], [
  # C++ specific cases for pic, static, wl, etc.
  if test yes = "$GXX"; then
    _LT_TAGVAR(lt_prog_compiler_wl, $1)='-Wl,'
    _LT_TAGVAR(lt_prog_compiler_static, $1)='-static'

    case $host_os in
    aix*)
      # All AIX code is PIC.
      if test ia64 = "$host_cpu"; then
	# AIX 5 now supports IA64 processor
	_LT_TAGVAR(lt_prog_compiler_static, $1)='-Bstatic'
      fi
      _LT_TAGVAR(lt_prog_compiler_pic, $1)='-fPIC'
      ;;

    amigaos*)
      case $host_cpu in
      powerpc)
            # see comment about AmigaOS4 .so support
            _LT_TAGVAR(lt_prog_compiler_pic, $1)='-fPIC'
        ;;
      m68k)
            # FIXME: we need at least 68020 code to build shared libraries, but
            # adding the '-m68020' flag to GCC prevents building anything better,
            # like '-m68040'.
            _LT_TAGVAR(lt_prog_compiler_pic, $1)='-m68020 -resident32 -malways-restore-a4'
        ;;
      esac
      ;;

    beos* | irix5* | irix6* | nonstopux* | osf3* | osf4* | osf5*)
      # PIC is the default for these OSes.
      ;;
    mingw* | cygwin* | os2* | pw32* | cegcc*)
      # This hack is so that the source file can tell whether it is being
      # built for inclusion in a dll (and should export symbols for example).
      # Although the cygwin gcc ignores -fPIC, still need this for old-style
      # (--disable-auto-import) libraries
      m4_if([$1], [GCJ], [],
	[_LT_TAGVAR(lt_prog_compiler_pic, $1)='-DDLL_EXPORT'])
      case $host_os in
      os2*)
	_LT_TAGVAR(lt_prog_compiler_static, $1)='$wl-static'
	;;
      esac
      ;;
    darwin* | rhapsody*)
      # PIC is the default on this platform
      # Common symbols not allowed in MH_DYLIB files
      _LT_TAGVAR(lt_prog_compiler_pic, $1)='-fno-common'
      ;;
    *djgpp*)
      # DJGPP does not support shared libraries at all
      _LT_TAGVAR(lt_prog_compiler_pic, $1)=
      ;;
    haiku*)
      # PIC is the default for Haiku.
      # The "-static" flag exists, but is broken.
      _LT_TAGVAR(lt_prog_compiler_static, $1)=
      ;;
    interix[[3-9]]*)
      # Interix 3.x gcc -fpic/-fPIC options generate broken code.
      # Instead, we relocate shared libraries at runtime.
      ;;
    sysv4*MP*)
      if test -d /usr/nec; then
	_LT_TAGVAR(lt_prog_compiler_pic, $1)=-Kconform_pic
      fi
      ;;
    hpux*)
      # PIC is the default for 64-bit PA HP-UX, but not for 32-bit
      # PA HP-UX.  On IA64 HP-UX, PIC is the default but the pic flag
      # sets the default TLS model and affects inlining.
      case $host_cpu in
      hppa*64*)
	;;
      *)
	_LT_TAGVAR(lt_prog_compiler_pic, $1)='-fPIC'
	;;
      esac
      ;;
    *qnx* | *nto*)
      # QNX uses GNU C++, but need to define -shared option too, otherwise
      # it will coredump.
      _LT_TAGVAR(lt_prog_compiler_pic, $1)='-fPIC -shared'
      ;;
    *)
      _LT_TAGVAR(lt_prog_compiler_pic, $1)='-fPIC'
      ;;
    esac
  else
    case $host_os in
      aix[[4-9]]*)
	# All AIX code is PIC.
	if test ia64 = "$host_cpu"; then
	  # AIX 5 now supports IA64 processor
	  _LT_TAGVAR(lt_prog_compiler_static, $1)='-Bstatic'
	else
	  _LT_TAGVAR(lt_prog_compiler_static, $1)='-bnso -bI:/lib/syscalls.exp'
	fi
	;;
      chorus*)
	case $cc_basename in
	cxch68*)
	  # Green Hills C++ Compiler
	  # _LT_TAGVAR(lt_prog_compiler_static, $1)="--no_auto_instantiation -u __main -u __premain -u _abort -r $COOL_DIR/lib/libOrb.a $MVME_DIR/lib/CC/libC.a $MVME_DIR/lib/classix/libcx.s.a"
	  ;;
	esac
	;;
      mingw* | cygwin* | os2* | pw32* | cegcc*)
	# This hack is so that the source file can tell whether it is being
	# built for inclusion in a dll (and should export symbols for example).
	m4_if([$1], [GCJ], [],
	  [_LT_TAGVAR(lt_prog_compiler_pic, $1)='-DDLL_EXPORT'])
	;;
      dgux*)
	case $cc_basename in
	  ec++*)
	    _LT_TAGVAR(lt_prog_compiler_pic, $1)='-KPIC'
	    ;;
	  ghcx*)
	    # Green Hills C++ Compiler
	    _LT_TAGVAR(lt_prog_compiler_pic, $1)='-pic'
	    ;;
	  *)
	    ;;
	esac
	;;
      freebsd* | dragonfly*)
	# FreeBSD uses GNU C++
	;;
      hpux9* | hpux10* | hpux11*)
	case $cc_basename in
	  CC*)
	    _LT_TAGVAR(lt_prog_compiler_wl, $1)='-Wl,'
	    _LT_TAGVAR(lt_prog_compiler_static, $1)='$wl-a ${wl}archive'
	    if test ia64 != "$host_cpu"; then
	      _LT_TAGVAR(lt_prog_compiler_pic, $1)='+Z'
	    fi
	    ;;
	  aCC*)
	    _LT_TAGVAR(lt_prog_compiler_wl, $1)='-Wl,'
	    _LT_TAGVAR(lt_prog_compiler_static, $1)='$wl-a ${wl}archive'
	    case $host_cpu in
	    hppa*64*|ia64*)
	      # +Z the default
	      ;;
	    *)
	      _LT_TAGVAR(lt_prog_compiler_pic, $1)='+Z'
	      ;;
	    esac
	    ;;
	  *)
	    ;;
	esac
	;;
      interix*)
	# This is c89, which is MS Visual C++ (no shared libs)
	# Anyone wants to do a port?
	;;
      irix5* | irix6* | nonstopux*)
	case $cc_basename in
	  CC*)
	    _LT_TAGVAR(lt_prog_compiler_wl, $1)='-Wl,'
	    _LT_TAGVAR(lt_prog_compiler_static, $1)='-non_shared'
	    # CC pic flag -KPIC is the default.
	    ;;
	  *)
	    ;;
	esac
	;;
      linux* | k*bsd*-gnu | kopensolaris*-gnu | gnu*)
	case $cc_basename in
	  KCC*)
	    # KAI C++ Compiler
	    _LT_TAGVAR(lt_prog_compiler_wl, $1)='--backend -Wl,'
	    _LT_TAGVAR(lt_prog_compiler_pic, $1)='-fPIC'
	    ;;
	  ecpc* )
	    # old Intel C++ for x86_64, which still supported -KPIC.
	    _LT_TAGVAR(lt_prog_compiler_wl, $1)='-Wl,'
	    _LT_TAGVAR(lt_prog_compiler_pic, $1)='-KPIC'
	    _LT_TAGVAR(lt_prog_compiler_static, $1)='-static'
	    ;;
	  icpc* )
	    # Intel C++, used to be incompatible with GCC.
	    # ICC 10 doesn't accept -KPIC any more.
	    _LT_TAGVAR(lt_prog_compiler_wl, $1)='-Wl,'
	    _LT_TAGVAR(lt_prog_compiler_pic, $1)='-fPIC'
	    _LT_TAGVAR(lt_prog_compiler_static, $1)='-static'
	    ;;
	  pgCC* | pgcpp*)
	    # Portland Group C++ compiler
	    _LT_TAGVAR(lt_prog_compiler_wl, $1)='-Wl,'
	    _LT_TAGVAR(lt_prog_compiler_pic, $1)='-fpic'
	    _LT_TAGVAR(lt_prog_compiler_static, $1)='-Bstatic'
	    ;;
	  cxx*)
	    # Compaq C++
	    # Make sure the PIC flag is empty.  It appears that all Alpha
	    # Linux and Compaq Tru64 Unix objects are PIC.
	    _LT_TAGVAR(lt_prog_compiler_pic, $1)=
	    _LT_TAGVAR(lt_prog_compiler_static, $1)='-non_shared'
	    ;;
	  xlc* | xlC* | bgxl[[cC]]* | mpixl[[cC]]*)
	    # IBM XL 8.0, 9.0 on PPC and BlueGene
	    _LT_TAGVAR(lt_prog_compiler_wl, $1)='-Wl,'
	    _LT_TAGVAR(lt_prog_compiler_pic, $1)='-qpic'
	    _LT_TAGVAR(lt_prog_compiler_static, $1)='-qstaticlink'
	    ;;
	  *)
	    case `$CC -V 2>&1 | sed 5q` in
	    *Sun\ C*)
	      # Sun C++ 5.9
	      _LT_TAGVAR(lt_prog_compiler_pic, $1)='-KPIC'
	      _LT_TAGVAR(lt_prog_compiler_static, $1)='-Bstatic'
	      _LT_TAGVAR(lt_prog_compiler_wl, $1)='-Qoption ld '
	      ;;
	    esac
	    ;;
	esac
	;;
      lynxos*)
	;;
      m88k*)
	;;
      mvs*)
	case $cc_basename in
	  cxx*)
	    _LT_TAGVAR(lt_prog_compiler_pic, $1)='-W c,exportall'
	    ;;
	  *)
	    ;;
	esac
	;;
      netbsd* | netbsdelf*-gnu)
	;;
      *qnx* | *nto*)
        # QNX uses GNU C++, but need to define -shared option too, otherwise
        # it will coredump.
        _LT_TAGVAR(lt_prog_compiler_pic, $1)='-fPIC -shared'
        ;;
      osf3* | osf4* | osf5*)
	case $cc_basename in
	  KCC*)
	    _LT_TAGVAR(lt_prog_compiler_wl, $1)='--backend -Wl,'
	    ;;
	  RCC*)
	    # Rational C++ 2.4.1
	    _LT_TAGVAR(lt_prog_compiler_pic, $1)='-pic'
	    ;;
	  cxx*)
	    # Digital/Compaq C++
	    _LT_TAGVAR(lt_prog_compiler_wl, $1)='-Wl,'
	    # Make sure the PIC flag is empty.  It appears that all Alpha
	    # Linux and Compaq Tru64 Unix objects are PIC.
	    _LT_TAGVAR(lt_prog_compiler_pic, $1)=
	    _LT_TAGVAR(lt_prog_compiler_static, $1)='-non_shared'
	    ;;
	  *)
	    ;;
	esac
	;;
      psos*)
	;;
      solaris*)
	case $cc_basename in
	  CC* | sunCC*)
	    # Sun C++ 4.2, 5.x and Centerline C++
	    _LT_TAGVAR(lt_prog_compiler_pic, $1)='-KPIC'
	    _LT_TAGVAR(lt_prog_compiler_static, $1)='-Bstatic'
	    _LT_TAGVAR(lt_prog_compiler_wl, $1)='-Qoption ld '
	    ;;
	  gcx*)
	    # Green Hills C++ Compiler
	    _LT_TAGVAR(lt_prog_compiler_pic, $1)='-PIC'
	    ;;
	  *)
	    ;;
	esac
	;;
      sunos4*)
	case $cc_basename in
	  CC*)
	    # Sun C++ 4.x
	    _LT_TAGVAR(lt_prog_compiler_pic, $1)='-pic'
	    _LT_TAGVAR(lt_prog_compiler_static, $1)='-Bstatic'
	    ;;
	  lcc*)
	    # Lucid
	    _LT_TAGVAR(lt_prog_compiler_pic, $1)='-pic'
	    ;;
	  *)
	    ;;
	esac
	;;
      sysv5* | unixware* | sco3.2v5* | sco5v6* | OpenUNIX*)
	case $cc_basename in
	  CC*)
	    _LT_TAGVAR(lt_prog_compiler_wl, $1)='-Wl,'
	    _LT_TAGVAR(lt_prog_compiler_pic, $1)='-KPIC'
	    _LT_TAGVAR(lt_prog_compiler_static, $1)='-Bstatic'
	    ;;
	esac
	;;
      tandem*)
	case $cc_basename in
	  NCC*)
	    # NonStop-UX NCC 3.20
	    _LT_TAGVAR(lt_prog_compiler_pic, $1)='-KPIC'
	    ;;
	  *)
	    ;;
	esac
	;;
      vxworks*)
	;;
      *)
	_LT_TAGVAR(lt_prog_compiler_can_build_shared, $1)=no
	;;
    esac
  fi
],
[
  if test yes = "$GCC"; then
    _LT_TAGVAR(lt_prog_compiler_wl, $1)='-Wl,'
    _LT_TAGVAR(lt_prog_compiler_static, $1)='-static'

    case $host_os in
      aix*)
      # All AIX code is PIC.
      if test ia64 = "$host_cpu"; then
	# AIX 5 now supports IA64 processor
	_LT_TAGVAR(lt_prog_compiler_static, $1)='-Bstatic'
      fi
      _LT_TAGVAR(lt_prog_compiler_pic, $1)='-fPIC'
      ;;

    amigaos*)
      case $host_cpu in
      powerpc)
            # see comment about AmigaOS4 .so support
            _LT_TAGVAR(lt_prog_compiler_pic, $1)='-fPIC'
        ;;
      m68k)
            # FIXME: we need at least 68020 code to build shared libraries, but
            # adding the '-m68020' flag to GCC prevents building anything better,
            # like '-m68040'.
            _LT_TAGVAR(lt_prog_compiler_pic, $1)='-m68020 -resident32 -malways-restore-a4'
        ;;
      esac
      ;;

    beos* | irix5* | irix6* | nonstopux* | osf3* | osf4* | osf5*)
      # PIC is the default for these OSes.
      ;;

    mingw* | cygwin* | pw32* | os2* | cegcc*)
      # This hack is so that the source file can tell whether it is being
      # built for inclusion in a dll (and should export symbols for example).
      # Although the cygwin gcc ignores -fPIC, still need this for old-style
      # (--disable-auto-import) libraries
      m4_if([$1], [GCJ], [],
	[_LT_TAGVAR(lt_prog_compiler_pic, $1)='-DDLL_EXPORT'])
      case $host_os in
      os2*)
	_LT_TAGVAR(lt_prog_compiler_static, $1)='$wl-static'
	;;
      esac
      ;;

    darwin* | rhapsody*)
      # PIC is the default on this platform
      # Common symbols not allowed in MH_DYLIB files
      _LT_TAGVAR(lt_prog_compiler_pic, $1)='-fno-common'
      ;;

    haiku*)
      # PIC is the default for Haiku.
      # The "-static" flag exists, but is broken.
      _LT_TAGVAR(lt_prog_compiler_static, $1)=
      ;;

    hpux*)
      # PIC is the default for 64-bit PA HP-UX, but not for 32-bit
      # PA HP-UX.  On IA64 HP-UX, PIC is the default but the pic flag
      # sets the default TLS model and affects inlining.
      case $host_cpu in
      hppa*64*)
	# +Z the default
	;;
      *)
	_LT_TAGVAR(lt_prog_compiler_pic, $1)='-fPIC'
	;;
      esac
      ;;

    interix[[3-9]]*)
      # Interix 3.x gcc -fpic/-fPIC options generate broken code.
      # Instead, we relocate shared libraries at runtime.
      ;;

    msdosdjgpp*)
      # Just because we use GCC doesn't mean we suddenly get shared libraries
      # on systems that don't support them.
      _LT_TAGVAR(lt_prog_compiler_can_build_shared, $1)=no
      enable_shared=no
      ;;

    *nto* | *qnx*)
      # QNX uses GNU C++, but need to define -shared option too, otherwise
      # it will coredump.
      _LT_TAGVAR(lt_prog_compiler_pic, $1)='-fPIC -shared'
      ;;

    sysv4*MP*)
      if test -d /usr/nec; then
	_LT_TAGVAR(lt_prog_compiler_pic, $1)=-Kconform_pic
      fi
      ;;

    *)
      _LT_TAGVAR(lt_prog_compiler_pic, $1)='-fPIC'
      ;;
    esac

    case $cc_basename in
    nvcc*) # Cuda Compiler Driver 2.2
      _LT_TAGVAR(lt_prog_compiler_wl, $1)='-Xlinker '
      if test -n "$_LT_TAGVAR(lt_prog_compiler_pic, $1)"; then
        _LT_TAGVAR(lt_prog_compiler_pic, $1)="-Xcompiler $_LT_TAGVAR(lt_prog_compiler_pic, $1)"
      fi
      ;;
    esac
  else
    # PORTME Check for flag to pass linker flags through the system compiler.
    case $host_os in
    aix*)
      _LT_TAGVAR(lt_prog_compiler_wl, $1)='-Wl,'
      if test ia64 = "$host_cpu"; then
	# AIX 5 now supports IA64 processor
	_LT_TAGVAR(lt_prog_compiler_static, $1)='-Bstatic'
      else
	_LT_TAGVAR(lt_prog_compiler_static, $1)='-bnso -bI:/lib/syscalls.exp'
      fi
      ;;

    darwin* | rhapsody*)
      # PIC is the default on this platform
      # Common symbols not allowed in MH_DYLIB files
      _LT_TAGVAR(lt_prog_compiler_pic, $1)='-fno-common'
      case $cc_basename in
      nagfor*)
        # NAG Fortran compiler
        _LT_TAGVAR(lt_prog_compiler_wl, $1)='-Wl,-Wl,,'
        _LT_TAGVAR(lt_prog_compiler_pic, $1)='-PIC'
        _LT_TAGVAR(lt_prog_compiler_static, $1)='-Bstatic'
        ;;
      esac
      ;;

    mingw* | cygwin* | pw32* | os2* | cegcc*)
      # This hack is so that the source file can tell whether it is being
      # built for inclusion in a dll (and should export symbols for example).
      m4_if([$1], [GCJ], [],
	[_LT_TAGVAR(lt_prog_compiler_pic, $1)='-DDLL_EXPORT'])
      case $host_os in
      os2*)
	_LT_TAGVAR(lt_prog_compiler_static, $1)='$wl-static'
	;;
      esac
      ;;

    hpux9* | hpux10* | hpux11*)
      _LT_TAGVAR(lt_prog_compiler_wl, $1)='-Wl,'
      # PIC is the default for IA64 HP-UX and 64-bit HP-UX, but
      # not for PA HP-UX.
      case $host_cpu in
      hppa*64*|ia64*)
	# +Z the default
	;;
      *)
	_LT_TAGVAR(lt_prog_compiler_pic, $1)='+Z'
	;;
      esac
      # Is there a better lt_prog_compiler_static that works with the bundled CC?
      _LT_TAGVAR(lt_prog_compiler_static, $1)='$wl-a ${wl}archive'
      ;;

    irix5* | irix6* | nonstopux*)
      _LT_TAGVAR(lt_prog_compiler_wl, $1)='-Wl,'
      # PIC (with -KPIC) is the default.
      _LT_TAGVAR(lt_prog_compiler_static, $1)='-non_shared'
      ;;

    linux* | k*bsd*-gnu | kopensolaris*-gnu | gnu*)
      case $cc_basename in
      # old Intel for x86_64, which still supported -KPIC.
      ecc*)
	_LT_TAGVAR(lt_prog_compiler_wl, $1)='-Wl,'
	_LT_TAGVAR(lt_prog_compiler_pic, $1)='-KPIC'
	_LT_TAGVAR(lt_prog_compiler_static, $1)='-static'
        ;;
      # icc used to be incompatible with GCC.
      # ICC 10 doesn't accept -KPIC any more.
      icc* | ifort*)
	_LT_TAGVAR(lt_prog_compiler_wl, $1)='-Wl,'
	_LT_TAGVAR(lt_prog_compiler_pic, $1)='-fPIC'
	_LT_TAGVAR(lt_prog_compiler_static, $1)='-static'
        ;;
      # Lahey Fortran 8.1.
      lf95*)
	_LT_TAGVAR(lt_prog_compiler_wl, $1)='-Wl,'
	_LT_TAGVAR(lt_prog_compiler_pic, $1)='--shared'
	_LT_TAGVAR(lt_prog_compiler_static, $1)='--static'
	;;
      nagfor*)
	# NAG Fortran compiler
	_LT_TAGVAR(lt_prog_compiler_wl, $1)='-Wl,-Wl,,'
	_LT_TAGVAR(lt_prog_compiler_pic, $1)='-PIC'
	_LT_TAGVAR(lt_prog_compiler_static, $1)='-Bstatic'
	;;
      tcc*)
	# Fabrice Bellard et al's Tiny C Compiler
	_LT_TAGVAR(lt_prog_compiler_wl, $1)='-Wl,'
	_LT_TAGVAR(lt_prog_compiler_pic, $1)='-fPIC'
	_LT_TAGVAR(lt_prog_compiler_static, $1)='-static'
	;;
      pgcc* | pgf77* | pgf90* | pgf95* | pgfortran*)
        # Portland Group compilers (*not* the Pentium gcc compiler,
	# which looks to be a dead project)
	_LT_TAGVAR(lt_prog_compiler_wl, $1)='-Wl,'
	_LT_TAGVAR(lt_prog_compiler_pic, $1)='-fpic'
	_LT_TAGVAR(lt_prog_compiler_static, $1)='-Bstatic'
        ;;
      ccc*)
        _LT_TAGVAR(lt_prog_compiler_wl, $1)='-Wl,'
        # All Alpha code is PIC.
        _LT_TAGVAR(lt_prog_compiler_static, $1)='-non_shared'
        ;;
      xl* | bgxl* | bgf* | mpixl*)
	# IBM XL C 8.0/Fortran 10.1, 11.1 on PPC and BlueGene
	_LT_TAGVAR(lt_prog_compiler_wl, $1)='-Wl,'
	_LT_TAGVAR(lt_prog_compiler_pic, $1)='-qpic'
	_LT_TAGVAR(lt_prog_compiler_static, $1)='-qstaticlink'
	;;
      *)
	case `$CC -V 2>&1 | sed 5q` in
	*Sun\ Ceres\ Fortran* | *Sun*Fortran*\ [[1-7]].* | *Sun*Fortran*\ 8.[[0-3]]*)
	  # Sun Fortran 8.3 passes all unrecognized flags to the linker
	  _LT_TAGVAR(lt_prog_compiler_pic, $1)='-KPIC'
	  _LT_TAGVAR(lt_prog_compiler_static, $1)='-Bstatic'
	  _LT_TAGVAR(lt_prog_compiler_wl, $1)=''
	  ;;
	*Sun\ F* | *Sun*Fortran*)
	  _LT_TAGVAR(lt_prog_compiler_pic, $1)='-KPIC'
	  _LT_TAGVAR(lt_prog_compiler_static, $1)='-Bstatic'
	  _LT_TAGVAR(lt_prog_compiler_wl, $1)='-Qoption ld '
	  ;;
	*Sun\ C*)
	  # Sun C 5.9
	  _LT_TAGVAR(lt_prog_compiler_pic, $1)='-KPIC'
	  _LT_TAGVAR(lt_prog_compiler_static, $1)='-Bstatic'
	  _LT_TAGVAR(lt_prog_compiler_wl, $1)='-Wl,'
	  ;;
        *Intel*\ [[CF]]*Compiler*)
	  _LT_TAGVAR(lt_prog_compiler_wl, $1)='-Wl,'
	  _LT_TAGVAR(lt_prog_compiler_pic, $1)='-fPIC'
	  _LT_TAGVAR(lt_prog_compiler_static, $1)='-static'
	  ;;
	*Portland\ Group*)
	  _LT_TAGVAR(lt_prog_compiler_wl, $1)='-Wl,'
	  _LT_TAGVAR(lt_prog_compiler_pic, $1)='-fpic'
	  _LT_TAGVAR(lt_prog_compiler_static, $1)='-Bstatic'
	  ;;
	esac
	;;
      esac
      ;;

    newsos6)
      _LT_TAGVAR(lt_prog_compiler_pic, $1)='-KPIC'
      _LT_TAGVAR(lt_prog_compiler_static, $1)='-Bstatic'
      ;;

    *nto* | *qnx*)
      # QNX uses GNU C++, but need to define -shared option too, otherwise
      # it will coredump.
      _LT_TAGVAR(lt_prog_compiler_pic, $1)='-fPIC -shared'
      ;;

    osf3* | osf4* | osf5*)
      _LT_TAGVAR(lt_prog_compiler_wl, $1)='-Wl,'
      # All OSF/1 code is PIC.
      _LT_TAGVAR(lt_prog_compiler_static, $1)='-non_shared'
      ;;

    rdos*)
      _LT_TAGVAR(lt_prog_compiler_static, $1)='-non_shared'
      ;;

    solaris*)
      _LT_TAGVAR(lt_prog_compiler_pic, $1)='-KPIC'
      _LT_TAGVAR(lt_prog_compiler_static, $1)='-Bstatic'
      case $cc_basename in
      f77* | f90* | f95* | sunf77* | sunf90* | sunf95*)
	_LT_TAGVAR(lt_prog_compiler_wl, $1)='-Qoption ld ';;
      *)
	_LT_TAGVAR(lt_prog_compiler_wl, $1)='-Wl,';;
      esac
      ;;

    sunos4*)
      _LT_TAGVAR(lt_prog_compiler_wl, $1)='-Qoption ld '
      _LT_TAGVAR(lt_prog_compiler_pic, $1)='-PIC'
      _LT_TAGVAR(lt_prog_compiler_static, $1)='-Bstatic'
      ;;

    sysv4 | sysv4.2uw2* | sysv4.3*)
      _LT_TAGVAR(lt_prog_compiler_wl, $1)='-Wl,'
      _LT_TAGVAR(lt_prog_compiler_pic, $1)='-KPIC'
      _LT_TAGVAR(lt_prog_compiler_static, $1)='-Bstatic'
      ;;

    sysv4*MP*)
      if test -d /usr/nec; then
	_LT_TAGVAR(lt_prog_compiler_pic, $1)='-Kconform_pic'
	_LT_TAGVAR(lt_prog_compiler_static, $1)='-Bstatic'
      fi
      ;;

    sysv5* | unixware* | sco3.2v5* | sco5v6* | OpenUNIX*)
      _LT_TAGVAR(lt_prog_compiler_wl, $1)='-Wl,'
      _LT_TAGVAR(lt_prog_compiler_pic, $1)='-KPIC'
      _LT_TAGVAR(lt_prog_compiler_static, $1)='-Bstatic'
      ;;

    unicos*)
      _LT_TAGVAR(lt_prog_compiler_wl, $1)='-Wl,'
      _LT_TAGVAR(lt_prog_compiler_can_build_shared, $1)=no
      ;;

    uts4*)
      _LT_TAGVAR(lt_prog_compiler_pic, $1)='-pic'
      _LT_TAGVAR(lt_prog_compiler_static, $1)='-Bstatic'
      ;;

    *)
      _LT_TAGVAR(lt_prog_compiler_can_build_shared, $1)=no
      ;;
    esac
  fi
])
case $host_os in
  # For platforms that do not support PIC, -DPIC is meaningless:
  *djgpp*)
    _LT_TAGVAR(lt_prog_compiler_pic, $1)=
    ;;
  *)
    _LT_TAGVAR(lt_prog_compiler_pic, $1)="$_LT_TAGVAR(lt_prog_compiler_pic, $1)@&t@m4_if([$1],[],[ -DPIC],[m4_if([$1],[CXX],[ -DPIC],[])])"
    ;;
esac

AC_CACHE_CHECK([for $compiler option to produce PIC],
  [_LT_TAGVAR(lt_cv_prog_compiler_pic, $1)],
  [_LT_TAGVAR(lt_cv_prog_compiler_pic, $1)=$_LT_TAGVAR(lt_prog_compiler_pic, $1)])
_LT_TAGVAR(lt_prog_compiler_pic, $1)=$_LT_TAGVAR(lt_cv_prog_compiler_pic, $1)

#
# Check to make sure the PIC flag actually works.
#
if test -n "$_LT_TAGVAR(lt_prog_compiler_pic, $1)"; then
  _LT_COMPILER_OPTION([if $compiler PIC flag $_LT_TAGVAR(lt_prog_compiler_pic, $1) works],
    [_LT_TAGVAR(lt_cv_prog_compiler_pic_works, $1)],
    [$_LT_TAGVAR(lt_prog_compiler_pic, $1)@&t@m4_if([$1],[],[ -DPIC],[m4_if([$1],[CXX],[ -DPIC],[])])], [],
    [case $_LT_TAGVAR(lt_prog_compiler_pic, $1) in
     "" | " "*) ;;
     *) _LT_TAGVAR(lt_prog_compiler_pic, $1)=" $_LT_TAGVAR(lt_prog_compiler_pic, $1)" ;;
     esac],
    [_LT_TAGVAR(lt_prog_compiler_pic, $1)=
     _LT_TAGVAR(lt_prog_compiler_can_build_shared, $1)=no])
fi
_LT_TAGDECL([pic_flag], [lt_prog_compiler_pic], [1],
	[Additional compiler flags for building library objects])

_LT_TAGDECL([wl], [lt_prog_compiler_wl], [1],
	[How to pass a linker flag through the compiler])
#
# Check to make sure the static flag actually works.
#
wl=$_LT_TAGVAR(lt_prog_compiler_wl, $1) eval lt_tmp_static_flag=\"$_LT_TAGVAR(lt_prog_compiler_static, $1)\"
_LT_LINKER_OPTION([if $compiler static flag $lt_tmp_static_flag works],
  _LT_TAGVAR(lt_cv_prog_compiler_static_works, $1),
  $lt_tmp_static_flag,
  [],
  [_LT_TAGVAR(lt_prog_compiler_static, $1)=])
_LT_TAGDECL([link_static_flag], [lt_prog_compiler_static], [1],
	[Compiler flag to prevent dynamic linking])
])# _LT_COMPILER_PIC


# _LT_LINKER_SHLIBS([TAGNAME])
# ----------------------------
# See if the linker supports building shared libraries.
m4_defun([_LT_LINKER_SHLIBS],
[AC_REQUIRE([LT_PATH_LD])dnl
AC_REQUIRE([LT_PATH_NM])dnl
m4_require([_LT_PATH_MANIFEST_TOOL])dnl
m4_require([_LT_FILEUTILS_DEFAULTS])dnl
m4_require([_LT_DECL_EGREP])dnl
m4_require([_LT_DECL_SED])dnl
m4_require([_LT_CMD_GLOBAL_SYMBOLS])dnl
m4_require([_LT_TAG_COMPILER])dnl
AC_MSG_CHECKING([whether the $compiler linker ($LD) supports shared libraries])
m4_if([$1], [CXX], [
  _LT_TAGVAR(export_symbols_cmds, $1)='$NM $libobjs $convenience | $global_symbol_pipe | $SED '\''s/.* //'\'' | sort | uniq > $export_symbols'
  _LT_TAGVAR(exclude_expsyms, $1)=['_GLOBAL_OFFSET_TABLE_|_GLOBAL__F[ID]_.*']
  case $host_os in
  aix[[4-9]]*)
    # If we're using GNU nm, then we don't want the "-C" option.
    # -C means demangle to GNU nm, but means don't demangle to AIX nm.
    # Without the "-l" option, or with the "-B" option, AIX nm treats
    # weak defined symbols like other global defined symbols, whereas
    # GNU nm marks them as "W".
    # While the 'weak' keyword is ignored in the Export File, we need
    # it in the Import File for the 'aix-soname' feature, so we have
    # to replace the "-B" option with "-P" for AIX nm.
    if $NM -V 2>&1 | $GREP 'GNU' > /dev/null; then
      _LT_TAGVAR(export_symbols_cmds, $1)='$NM -Bpg $libobjs $convenience | awk '\''{ if (((\$ 2 == "T") || (\$ 2 == "D") || (\$ 2 == "B") || (\$ 2 == "W")) && ([substr](\$ 3,1,1) != ".")) { if (\$ 2 == "W") { print \$ 3 " weak" } else { print \$ 3 } } }'\'' | sort -u > $export_symbols'
    else
      _LT_TAGVAR(export_symbols_cmds, $1)='`func_echo_all $NM | $SED -e '\''s/B\([[^B]]*\)$/P\1/'\''` -PCpgl $libobjs $convenience | awk '\''{ if (((\$ 2 == "T") || (\$ 2 == "D") || (\$ 2 == "B") || (\$ 2 == "W") || (\$ 2 == "V") || (\$ 2 == "Z")) && ([substr](\$ 1,1,1) != ".")) { if ((\$ 2 == "W") || (\$ 2 == "V") || (\$ 2 == "Z")) { print \$ 1 " weak" } else { print \$ 1 } } }'\'' | sort -u > $export_symbols'
    fi
    ;;
  pw32*)
    _LT_TAGVAR(export_symbols_cmds, $1)=$ltdll_cmds
    ;;
  cygwin* | mingw* | cegcc*)
    case $cc_basename in
    cl*)
      _LT_TAGVAR(exclude_expsyms, $1)='_NULL_IMPORT_DESCRIPTOR|_IMPORT_DESCRIPTOR_.*'
      ;;
    *)
      _LT_TAGVAR(export_symbols_cmds, $1)='$NM $libobjs $convenience | $global_symbol_pipe | $SED -e '\''/^[[BCDGRS]][[ ]]/s/.*[[ ]]\([[^ ]]*\)/\1 DATA/;s/^.*[[ ]]__nm__\([[^ ]]*\)[[ ]][[^ ]]*/\1 DATA/;/^I[[ ]]/d;/^[[AITW]][[ ]]/s/.* //'\'' | sort | uniq > $export_symbols'
      _LT_TAGVAR(exclude_expsyms, $1)=['[_]+GLOBAL_OFFSET_TABLE_|[_]+GLOBAL__[FID]_.*|[_]+head_[A-Za-z0-9_]+_dll|[A-Za-z0-9_]+_dll_iname']
      ;;
    esac
    ;;
  linux* | k*bsd*-gnu | gnu*)
    _LT_TAGVAR(link_all_deplibs, $1)=no
    ;;
  *)
    _LT_TAGVAR(export_symbols_cmds, $1)='$NM $libobjs $convenience | $global_symbol_pipe | $SED '\''s/.* //'\'' | sort | uniq > $export_symbols'
    ;;
  esac
], [
  runpath_var=
  _LT_TAGVAR(allow_undefined_flag, $1)=
  _LT_TAGVAR(always_export_symbols, $1)=no
  _LT_TAGVAR(archive_cmds, $1)=
  _LT_TAGVAR(archive_expsym_cmds, $1)=
  _LT_TAGVAR(compiler_needs_object, $1)=no
  _LT_TAGVAR(enable_shared_with_static_runtimes, $1)=no
  _LT_TAGVAR(export_dynamic_flag_spec, $1)=
  _LT_TAGVAR(export_symbols_cmds, $1)='$NM $libobjs $convenience | $global_symbol_pipe | $SED '\''s/.* //'\'' | sort | uniq > $export_symbols'
  _LT_TAGVAR(hardcode_automatic, $1)=no
  _LT_TAGVAR(hardcode_direct, $1)=no
  _LT_TAGVAR(hardcode_direct_absolute, $1)=no
  _LT_TAGVAR(hardcode_libdir_flag_spec, $1)=
  _LT_TAGVAR(hardcode_libdir_separator, $1)=
  _LT_TAGVAR(hardcode_minus_L, $1)=no
  _LT_TAGVAR(hardcode_shlibpath_var, $1)=unsupported
  _LT_TAGVAR(inherit_rpath, $1)=no
  _LT_TAGVAR(link_all_deplibs, $1)=unknown
  _LT_TAGVAR(module_cmds, $1)=
  _LT_TAGVAR(module_expsym_cmds, $1)=
  _LT_TAGVAR(old_archive_from_new_cmds, $1)=
  _LT_TAGVAR(old_archive_from_expsyms_cmds, $1)=
  _LT_TAGVAR(thread_safe_flag_spec, $1)=
  _LT_TAGVAR(whole_archive_flag_spec, $1)=
  # include_expsyms should be a list of space-separated symbols to be *always*
  # included in the symbol list
  _LT_TAGVAR(include_expsyms, $1)=
  # exclude_expsyms can be an extended regexp of symbols to exclude
  # it will be wrapped by ' (' and ')$', so one must not match beginning or
  # end of line.  Example: 'a|bc|.*d.*' will exclude the symbols 'a' and 'bc',
  # as well as any symbol that contains 'd'.
  _LT_TAGVAR(exclude_expsyms, $1)=['_GLOBAL_OFFSET_TABLE_|_GLOBAL__F[ID]_.*']
  # Although _GLOBAL_OFFSET_TABLE_ is a valid symbol C name, most a.out
  # platforms (ab)use it in PIC code, but their linkers get confused if
  # the symbol is explicitly referenced.  Since portable code cannot
  # rely on this symbol name, it's probably fine to never include it in
  # preloaded symbol tables.
  # Exclude shared library initialization/finalization symbols.
dnl Note also adjust exclude_expsyms for C++ above.
  extract_expsyms_cmds=

  case $host_os in
  cygwin* | mingw* | pw32* | cegcc*)
    # FIXME: the MSVC++ port hasn't been tested in a loooong time
    # When not using gcc, we currently assume that we are using
    # Microsoft Visual C++.
    if test yes != "$GCC"; then
      with_gnu_ld=no
    fi
    ;;
  interix*)
    # we just hope/assume this is gcc and not c89 (= MSVC++)
    with_gnu_ld=yes
    ;;
  openbsd* | bitrig*)
    with_gnu_ld=no
    ;;
  linux* | k*bsd*-gnu | gnu*)
    _LT_TAGVAR(link_all_deplibs, $1)=no
    ;;
  esac

  _LT_TAGVAR(ld_shlibs, $1)=yes

  # On some targets, GNU ld is compatible enough with the native linker
  # that we're better off using the native interface for both.
  lt_use_gnu_ld_interface=no
  if test yes = "$with_gnu_ld"; then
    case $host_os in
      aix*)
	# The AIX port of GNU ld has always aspired to compatibility
	# with the native linker.  However, as the warning in the GNU ld
	# block says, versions before 2.19.5* couldn't really create working
	# shared libraries, regardless of the interface used.
	case `$LD -v 2>&1` in
	  *\ \(GNU\ Binutils\)\ 2.19.5*) ;;
	  *\ \(GNU\ Binutils\)\ 2.[[2-9]]*) ;;
	  *\ \(GNU\ Binutils\)\ [[3-9]]*) ;;
	  *)
	    lt_use_gnu_ld_interface=yes
	    ;;
	esac
	;;
      *)
	lt_use_gnu_ld_interface=yes
	;;
    esac
  fi

  if test yes = "$lt_use_gnu_ld_interface"; then
    # If archive_cmds runs LD, not CC, wlarc should be empty
    wlarc='$wl'

    # Set some defaults for GNU ld with shared library support. These
    # are reset later if shared libraries are not supported. Putting them
    # here allows them to be overridden if necessary.
    runpath_var=LD_RUN_PATH
    _LT_TAGVAR(hardcode_libdir_flag_spec, $1)='$wl-rpath $wl$libdir'
    _LT_TAGVAR(export_dynamic_flag_spec, $1)='$wl--export-dynamic'
    # ancient GNU ld didn't support --whole-archive et. al.
    if $LD --help 2>&1 | $GREP 'no-whole-archive' > /dev/null; then
      _LT_TAGVAR(whole_archive_flag_spec, $1)=$wlarc'--whole-archive$convenience '$wlarc'--no-whole-archive'
    else
      _LT_TAGVAR(whole_archive_flag_spec, $1)=
    fi
    supports_anon_versioning=no
    case `$LD -v | $SED -e 's/([^)]\+)\s\+//' 2>&1` in
      *GNU\ gold*) supports_anon_versioning=yes ;;
      *\ [[01]].* | *\ 2.[[0-9]].* | *\ 2.10.*) ;; # catch versions < 2.11
      *\ 2.11.93.0.2\ *) supports_anon_versioning=yes ;; # RH7.3 ...
      *\ 2.11.92.0.12\ *) supports_anon_versioning=yes ;; # Mandrake 8.2 ...
      *\ 2.11.*) ;; # other 2.11 versions
      *) supports_anon_versioning=yes ;;
    esac

    # See if GNU ld supports shared libraries.
    case $host_os in
    aix[[3-9]]*)
      # On AIX/PPC, the GNU linker is very broken
      if test ia64 != "$host_cpu"; then
	_LT_TAGVAR(ld_shlibs, $1)=no
	cat <<_LT_EOF 1>&2

*** Warning: the GNU linker, at least up to release 2.19, is reported
*** to be unable to reliably create shared libraries on AIX.
*** Therefore, libtool is disabling shared libraries support.  If you
*** really care for shared libraries, you may want to install binutils
*** 2.20 or above, or modify your PATH so that a non-GNU linker is found.
*** You will then need to restart the configuration process.

_LT_EOF
      fi
      ;;

    amigaos*)
      case $host_cpu in
      powerpc)
            # see comment about AmigaOS4 .so support
            _LT_TAGVAR(archive_cmds, $1)='$CC -shared $libobjs $deplibs $compiler_flags $wl-soname $wl$soname -o $lib'
            _LT_TAGVAR(archive_expsym_cmds, $1)=''
        ;;
      m68k)
            _LT_TAGVAR(archive_cmds, $1)='$RM $output_objdir/a2ixlibrary.data~$ECHO "#define NAME $libname" > $output_objdir/a2ixlibrary.data~$ECHO "#define LIBRARY_ID 1" >> $output_objdir/a2ixlibrary.data~$ECHO "#define VERSION $major" >> $output_objdir/a2ixlibrary.data~$ECHO "#define REVISION $revision" >> $output_objdir/a2ixlibrary.data~$AR $AR_FLAGS $lib $libobjs~$RANLIB $lib~(cd $output_objdir && a2ixlibrary -32)'
            _LT_TAGVAR(hardcode_libdir_flag_spec, $1)='-L$libdir'
            _LT_TAGVAR(hardcode_minus_L, $1)=yes
        ;;
      esac
      ;;

    beos*)
      if $LD --help 2>&1 | $GREP ': supported targets:.* elf' > /dev/null; then
	_LT_TAGVAR(allow_undefined_flag, $1)=unsupported
	# Joseph Beckenbach <jrb3@best.com> says some releases of gcc
	# support --undefined.  This deserves some investigation.  FIXME
	_LT_TAGVAR(archive_cmds, $1)='$CC -nostart $libobjs $deplibs $compiler_flags $wl-soname $wl$soname -o $lib'
      else
	_LT_TAGVAR(ld_shlibs, $1)=no
      fi
      ;;

    cygwin* | mingw* | pw32* | cegcc*)
      # _LT_TAGVAR(hardcode_libdir_flag_spec, $1) is actually meaningless,
      # as there is no search path for DLLs.
      _LT_TAGVAR(hardcode_libdir_flag_spec, $1)='-L$libdir'
      _LT_TAGVAR(export_dynamic_flag_spec, $1)='$wl--export-all-symbols'
      _LT_TAGVAR(allow_undefined_flag, $1)=unsupported
      _LT_TAGVAR(always_export_symbols, $1)=no
      _LT_TAGVAR(enable_shared_with_static_runtimes, $1)=yes
      _LT_TAGVAR(export_symbols_cmds, $1)='$NM $libobjs $convenience | $global_symbol_pipe | $SED -e '\''/^[[BCDGRS]][[ ]]/s/.*[[ ]]\([[^ ]]*\)/\1 DATA/;s/^.*[[ ]]__nm__\([[^ ]]*\)[[ ]][[^ ]]*/\1 DATA/;/^I[[ ]]/d;/^[[AITW]][[ ]]/s/.* //'\'' | sort | uniq > $export_symbols'
      _LT_TAGVAR(exclude_expsyms, $1)=['[_]+GLOBAL_OFFSET_TABLE_|[_]+GLOBAL__[FID]_.*|[_]+head_[A-Za-z0-9_]+_dll|[A-Za-z0-9_]+_dll_iname']

      if $LD --help 2>&1 | $GREP 'auto-import' > /dev/null; then
        _LT_TAGVAR(archive_cmds, $1)='$CC -shared $libobjs $deplibs $compiler_flags -o $output_objdir/$soname $wl--enable-auto-image-base -Xlinker --out-implib -Xlinker $lib'
	# If the export-symbols file already is a .def file, use it as
	# is; otherwise, prepend EXPORTS...
	_LT_TAGVAR(archive_expsym_cmds, $1)='if _LT_DLL_DEF_P([$export_symbols]); then
          cp $export_symbols $output_objdir/$soname.def;
        else
          echo EXPORTS > $output_objdir/$soname.def;
          cat $export_symbols >> $output_objdir/$soname.def;
        fi~
        $CC -shared $output_objdir/$soname.def $libobjs $deplibs $compiler_flags -o $output_objdir/$soname $wl--enable-auto-image-base -Xlinker --out-implib -Xlinker $lib'
      else
	_LT_TAGVAR(ld_shlibs, $1)=no
      fi
      ;;

    haiku*)
      _LT_TAGVAR(archive_cmds, $1)='$CC -shared $libobjs $deplibs $compiler_flags $wl-soname $wl$soname -o $lib'
      _LT_TAGVAR(link_all_deplibs, $1)=yes
      ;;

    os2*)
      _LT_TAGVAR(hardcode_libdir_flag_spec, $1)='-L$libdir'
      _LT_TAGVAR(hardcode_minus_L, $1)=yes
      _LT_TAGVAR(allow_undefined_flag, $1)=unsupported
      shrext_cmds=.dll
      _LT_TAGVAR(archive_cmds, $1)='$ECHO "LIBRARY ${soname%$shared_ext} INITINSTANCE TERMINSTANCE" > $output_objdir/$libname.def~
	$ECHO "DESCRIPTION \"$libname\"" >> $output_objdir/$libname.def~
	$ECHO "DATA MULTIPLE NONSHARED" >> $output_objdir/$libname.def~
	$ECHO EXPORTS >> $output_objdir/$libname.def~
	emxexp $libobjs | $SED /"_DLL_InitTerm"/d >> $output_objdir/$libname.def~
	$CC -Zdll -Zcrtdll -o $output_objdir/$soname $libobjs $deplibs $compiler_flags $output_objdir/$libname.def~
	emximp -o $lib $output_objdir/$libname.def'
      _LT_TAGVAR(archive_expsym_cmds, $1)='$ECHO "LIBRARY ${soname%$shared_ext} INITINSTANCE TERMINSTANCE" > $output_objdir/$libname.def~
	$ECHO "DESCRIPTION \"$libname\"" >> $output_objdir/$libname.def~
	$ECHO "DATA MULTIPLE NONSHARED" >> $output_objdir/$libname.def~
	$ECHO EXPORTS >> $output_objdir/$libname.def~
	prefix_cmds="$SED"~
	if test EXPORTS = "`$SED 1q $export_symbols`"; then
	  prefix_cmds="$prefix_cmds -e 1d";
	fi~
	prefix_cmds="$prefix_cmds -e \"s/^\(.*\)$/_\1/g\""~
	cat $export_symbols | $prefix_cmds >> $output_objdir/$libname.def~
	$CC -Zdll -Zcrtdll -o $output_objdir/$soname $libobjs $deplibs $compiler_flags $output_objdir/$libname.def~
	emximp -o $lib $output_objdir/$libname.def'
      _LT_TAGVAR(old_archive_From_new_cmds, $1)='emximp -o $output_objdir/${libname}_dll.a $output_objdir/$libname.def'
      _LT_TAGVAR(enable_shared_with_static_runtimes, $1)=yes
      ;;

    interix[[3-9]]*)
      _LT_TAGVAR(hardcode_direct, $1)=no
      _LT_TAGVAR(hardcode_shlibpath_var, $1)=no
      _LT_TAGVAR(hardcode_libdir_flag_spec, $1)='$wl-rpath,$libdir'
      _LT_TAGVAR(export_dynamic_flag_spec, $1)='$wl-E'
      # Hack: On Interix 3.x, we cannot compile PIC because of a broken gcc.
      # Instead, shared libraries are loaded at an image base (0x10000000 by
      # default) and relocated if they conflict, which is a slow very memory
      # consuming and fragmenting process.  To avoid this, we pick a random,
      # 256 KiB-aligned image base between 0x50000000 and 0x6FFC0000 at link
      # time.  Moving up from 0x10000000 also allows more sbrk(2) space.
      _LT_TAGVAR(archive_cmds, $1)='$CC -shared $pic_flag $libobjs $deplibs $compiler_flags $wl-h,$soname $wl--image-base,`expr ${RANDOM-$$} % 4096 / 2 \* 262144 + 1342177280` -o $lib'
      _LT_TAGVAR(archive_expsym_cmds, $1)='sed "s|^|_|" $export_symbols >$output_objdir/$soname.expsym~$CC -shared $pic_flag $libobjs $deplibs $compiler_flags $wl-h,$soname $wl--retain-symbols-file,$output_objdir/$soname.expsym $wl--image-base,`expr ${RANDOM-$$} % 4096 / 2 \* 262144 + 1342177280` -o $lib'
      ;;

    gnu* | linux* | tpf* | k*bsd*-gnu | kopensolaris*-gnu)
      tmp_diet=no
      if test linux-dietlibc = "$host_os"; then
	case $cc_basename in
	  diet\ *) tmp_diet=yes;;	# linux-dietlibc with static linking (!diet-dyn)
	esac
      fi
      if $LD --help 2>&1 | $EGREP ': supported targets:.* elf' > /dev/null \
	 && test no = "$tmp_diet"
      then
	tmp_addflag=' $pic_flag'
	tmp_sharedflag='-shared'
	case $cc_basename,$host_cpu in
        pgcc*)				# Portland Group C compiler
	  _LT_TAGVAR(whole_archive_flag_spec, $1)='$wl--whole-archive`for conv in $convenience\"\"; do test  -n \"$conv\" && new_convenience=\"$new_convenience,$conv\"; done; func_echo_all \"$new_convenience\"` $wl--no-whole-archive'
	  tmp_addflag=' $pic_flag'
	  ;;
	pgf77* | pgf90* | pgf95* | pgfortran*)
					# Portland Group f77 and f90 compilers
	  _LT_TAGVAR(whole_archive_flag_spec, $1)='$wl--whole-archive`for conv in $convenience\"\"; do test  -n \"$conv\" && new_convenience=\"$new_convenience,$conv\"; done; func_echo_all \"$new_convenience\"` $wl--no-whole-archive'
	  tmp_addflag=' $pic_flag -Mnomain' ;;
	ecc*,ia64* | icc*,ia64*)	# Intel C compiler on ia64
	  tmp_addflag=' -i_dynamic' ;;
	efc*,ia64* | ifort*,ia64*)	# Intel Fortran compiler on ia64
	  tmp_addflag=' -i_dynamic -nofor_main' ;;
	ifc* | ifort*)			# Intel Fortran compiler
	  tmp_addflag=' -nofor_main' ;;
	lf95*)				# Lahey Fortran 8.1
	  _LT_TAGVAR(whole_archive_flag_spec, $1)=
	  tmp_sharedflag='--shared' ;;
        nagfor*)                        # NAGFOR 5.3
          tmp_sharedflag='-Wl,-shared' ;;
	xl[[cC]]* | bgxl[[cC]]* | mpixl[[cC]]*) # IBM XL C 8.0 on PPC (deal with xlf below)
	  tmp_sharedflag='-qmkshrobj'
	  tmp_addflag= ;;
	nvcc*)	# Cuda Compiler Driver 2.2
	  _LT_TAGVAR(whole_archive_flag_spec, $1)='$wl--whole-archive`for conv in $convenience\"\"; do test  -n \"$conv\" && new_convenience=\"$new_convenience,$conv\"; done; func_echo_all \"$new_convenience\"` $wl--no-whole-archive'
	  _LT_TAGVAR(compiler_needs_object, $1)=yes
	  ;;
	esac
	case `$CC -V 2>&1 | sed 5q` in
	*Sun\ C*)			# Sun C 5.9
	  _LT_TAGVAR(whole_archive_flag_spec, $1)='$wl--whole-archive`new_convenience=; for conv in $convenience\"\"; do test -z \"$conv\" || new_convenience=\"$new_convenience,$conv\"; done; func_echo_all \"$new_convenience\"` $wl--no-whole-archive'
	  _LT_TAGVAR(compiler_needs_object, $1)=yes
	  tmp_sharedflag='-G' ;;
	*Sun\ F*)			# Sun Fortran 8.3
	  tmp_sharedflag='-G' ;;
	esac
	_LT_TAGVAR(archive_cmds, $1)='$CC '"$tmp_sharedflag""$tmp_addflag"' $libobjs $deplibs $compiler_flags $wl-soname $wl$soname -o $lib'

        if test yes = "$supports_anon_versioning"; then
          _LT_TAGVAR(archive_expsym_cmds, $1)='echo "{ global:" > $output_objdir/$libname.ver~
            cat $export_symbols | sed -e "s/\(.*\)/\1;/" >> $output_objdir/$libname.ver~
            echo "local: *; };" >> $output_objdir/$libname.ver~
            $CC '"$tmp_sharedflag""$tmp_addflag"' $libobjs $deplibs $compiler_flags $wl-soname $wl$soname $wl-version-script $wl$output_objdir/$libname.ver -o $lib'
        fi

	case $cc_basename in
	tcc*)
	  _LT_TAGVAR(export_dynamic_flag_spec, $1)='-rdynamic'
	  ;;
	xlf* | bgf* | bgxlf* | mpixlf*)
	  # IBM XL Fortran 10.1 on PPC cannot create shared libs itself
	  _LT_TAGVAR(whole_archive_flag_spec, $1)='--whole-archive$convenience --no-whole-archive'
	  _LT_TAGVAR(hardcode_libdir_flag_spec, $1)='$wl-rpath $wl$libdir'
	  _LT_TAGVAR(archive_cmds, $1)='$LD -shared $libobjs $deplibs $linker_flags -soname $soname -o $lib'
	  if test yes = "$supports_anon_versioning"; then
	    _LT_TAGVAR(archive_expsym_cmds, $1)='echo "{ global:" > $output_objdir/$libname.ver~
              cat $export_symbols | sed -e "s/\(.*\)/\1;/" >> $output_objdir/$libname.ver~
              echo "local: *; };" >> $output_objdir/$libname.ver~
              $LD -shared $libobjs $deplibs $linker_flags -soname $soname -version-script $output_objdir/$libname.ver -o $lib'
	  fi
	  ;;
	esac
      else
        _LT_TAGVAR(ld_shlibs, $1)=no
      fi
      ;;

    netbsd* | netbsdelf*-gnu)
      if echo __ELF__ | $CC -E - | $GREP __ELF__ >/dev/null; then
	_LT_TAGVAR(archive_cmds, $1)='$LD -Bshareable $libobjs $deplibs $linker_flags -o $lib'
	wlarc=
      else
	_LT_TAGVAR(archive_cmds, $1)='$CC -shared $pic_flag $libobjs $deplibs $compiler_flags $wl-soname $wl$soname -o $lib'
	_LT_TAGVAR(archive_expsym_cmds, $1)='$CC -shared $pic_flag $libobjs $deplibs $compiler_flags $wl-soname $wl$soname $wl-retain-symbols-file $wl$export_symbols -o $lib'
      fi
      ;;

    solaris*)
      if $LD -v 2>&1 | $GREP 'BFD 2\.8' > /dev/null; then
	_LT_TAGVAR(ld_shlibs, $1)=no
	cat <<_LT_EOF 1>&2

*** Warning: The releases 2.8.* of the GNU linker cannot reliably
*** create shared libraries on Solaris systems.  Therefore, libtool
*** is disabling shared libraries support.  We urge you to upgrade GNU
*** binutils to release 2.9.1 or newer.  Another option is to modify
*** your PATH or compiler configuration so that the native linker is
*** used, and then restart.

_LT_EOF
      elif $LD --help 2>&1 | $GREP ': supported targets:.* elf' > /dev/null; then
	_LT_TAGVAR(archive_cmds, $1)='$CC -shared $pic_flag $libobjs $deplibs $compiler_flags $wl-soname $wl$soname -o $lib'
	_LT_TAGVAR(archive_expsym_cmds, $1)='$CC -shared $pic_flag $libobjs $deplibs $compiler_flags $wl-soname $wl$soname $wl-retain-symbols-file $wl$export_symbols -o $lib'
      else
	_LT_TAGVAR(ld_shlibs, $1)=no
      fi
      ;;

    sysv5* | sco3.2v5* | sco5v6* | unixware* | OpenUNIX*)
      case `$LD -v 2>&1` in
        *\ [[01]].* | *\ 2.[[0-9]].* | *\ 2.1[[0-5]].*)
	_LT_TAGVAR(ld_shlibs, $1)=no
	cat <<_LT_EOF 1>&2

*** Warning: Releases of the GNU linker prior to 2.16.91.0.3 cannot
*** reliably create shared libraries on SCO systems.  Therefore, libtool
*** is disabling shared libraries support.  We urge you to upgrade GNU
*** binutils to release 2.16.91.0.3 or newer.  Another option is to modify
*** your PATH or compiler configuration so that the native linker is
*** used, and then restart.

_LT_EOF
	;;
	*)
	  # For security reasons, it is highly recommended that you always
	  # use absolute paths for naming shared libraries, and exclude the
	  # DT_RUNPATH tag from executables and libraries.  But doing so
	  # requires that you compile everything twice, which is a pain.
	  if $LD --help 2>&1 | $GREP ': supported targets:.* elf' > /dev/null; then
	    _LT_TAGVAR(hardcode_libdir_flag_spec, $1)='$wl-rpath $wl$libdir'
	    _LT_TAGVAR(archive_cmds, $1)='$CC -shared $libobjs $deplibs $compiler_flags $wl-soname $wl$soname -o $lib'
	    _LT_TAGVAR(archive_expsym_cmds, $1)='$CC -shared $libobjs $deplibs $compiler_flags $wl-soname $wl$soname $wl-retain-symbols-file $wl$export_symbols -o $lib'
	  else
	    _LT_TAGVAR(ld_shlibs, $1)=no
	  fi
	;;
      esac
      ;;

    sunos4*)
      _LT_TAGVAR(archive_cmds, $1)='$LD -assert pure-text -Bshareable -o $lib $libobjs $deplibs $linker_flags'
      wlarc=
      _LT_TAGVAR(hardcode_direct, $1)=yes
      _LT_TAGVAR(hardcode_shlibpath_var, $1)=no
      ;;

    *)
      if $LD --help 2>&1 | $GREP ': supported targets:.* elf' > /dev/null; then
	_LT_TAGVAR(archive_cmds, $1)='$CC -shared $pic_flag $libobjs $deplibs $compiler_flags $wl-soname $wl$soname -o $lib'
	_LT_TAGVAR(archive_expsym_cmds, $1)='$CC -shared $pic_flag $libobjs $deplibs $compiler_flags $wl-soname $wl$soname $wl-retain-symbols-file $wl$export_symbols -o $lib'
      else
	_LT_TAGVAR(ld_shlibs, $1)=no
      fi
      ;;
    esac

    if test no = "$_LT_TAGVAR(ld_shlibs, $1)"; then
      runpath_var=
      _LT_TAGVAR(hardcode_libdir_flag_spec, $1)=
      _LT_TAGVAR(export_dynamic_flag_spec, $1)=
      _LT_TAGVAR(whole_archive_flag_spec, $1)=
    fi
  else
    # PORTME fill in a description of your system's linker (not GNU ld)
    case $host_os in
    aix3*)
      _LT_TAGVAR(allow_undefined_flag, $1)=unsupported
      _LT_TAGVAR(always_export_symbols, $1)=yes
      _LT_TAGVAR(archive_expsym_cmds, $1)='$LD -o $output_objdir/$soname $libobjs $deplibs $linker_flags -bE:$export_symbols -T512 -H512 -bM:SRE~$AR $AR_FLAGS $lib $output_objdir/$soname'
      # Note: this linker hardcodes the directories in LIBPATH if there
      # are no directories specified by -L.
      _LT_TAGVAR(hardcode_minus_L, $1)=yes
      if test yes = "$GCC" && test -z "$lt_prog_compiler_static"; then
	# Neither direct hardcoding nor static linking is supported with a
	# broken collect2.
	_LT_TAGVAR(hardcode_direct, $1)=unsupported
      fi
      ;;

    aix[[4-9]]*)
      if test ia64 = "$host_cpu"; then
	# On IA64, the linker does run time linking by default, so we don't
	# have to do anything special.
	aix_use_runtimelinking=no
	exp_sym_flag='-Bexport'
	no_entry_flag=
      else
	# If we're using GNU nm, then we don't want the "-C" option.
	# -C means demangle to GNU nm, but means don't demangle to AIX nm.
	# Without the "-l" option, or with the "-B" option, AIX nm treats
	# weak defined symbols like other global defined symbols, whereas
	# GNU nm marks them as "W".
	# While the 'weak' keyword is ignored in the Export File, we need
	# it in the Import File for the 'aix-soname' feature, so we have
	# to replace the "-B" option with "-P" for AIX nm.
	if $NM -V 2>&1 | $GREP 'GNU' > /dev/null; then
	  _LT_TAGVAR(export_symbols_cmds, $1)='$NM -Bpg $libobjs $convenience | awk '\''{ if (((\$ 2 == "T") || (\$ 2 == "D") || (\$ 2 == "B") || (\$ 2 == "W")) && ([substr](\$ 3,1,1) != ".")) { if (\$ 2 == "W") { print \$ 3 " weak" } else { print \$ 3 } } }'\'' | sort -u > $export_symbols'
	else
	  _LT_TAGVAR(export_symbols_cmds, $1)='`func_echo_all $NM | $SED -e '\''s/B\([[^B]]*\)$/P\1/'\''` -PCpgl $libobjs $convenience | awk '\''{ if (((\$ 2 == "T") || (\$ 2 == "D") || (\$ 2 == "B") || (\$ 2 == "W") || (\$ 2 == "V") || (\$ 2 == "Z")) && ([substr](\$ 1,1,1) != ".")) { if ((\$ 2 == "W") || (\$ 2 == "V") || (\$ 2 == "Z")) { print \$ 1 " weak" } else { print \$ 1 } } }'\'' | sort -u > $export_symbols'
	fi
	aix_use_runtimelinking=no

	# Test if we are trying to use run time linking or normal
	# AIX style linking. If -brtl is somewhere in LDFLAGS, we
	# have runtime linking enabled, and use it for executables.
	# For shared libraries, we enable/disable runtime linking
	# depending on the kind of the shared library created -
	# when "with_aix_soname,aix_use_runtimelinking" is:
	# "aix,no"   lib.a(lib.so.V) shared, rtl:no,  for executables
	# "aix,yes"  lib.so          shared, rtl:yes, for executables
	#            lib.a           static archive
	# "both,no"  lib.so.V(shr.o) shared, rtl:yes
	#            lib.a(lib.so.V) shared, rtl:no,  for executables
	# "both,yes" lib.so.V(shr.o) shared, rtl:yes, for executables
	#            lib.a(lib.so.V) shared, rtl:no
	# "svr4,*"   lib.so.V(shr.o) shared, rtl:yes, for executables
	#            lib.a           static archive
	case $host_os in aix4.[[23]]|aix4.[[23]].*|aix[[5-9]]*)
	  for ld_flag in $LDFLAGS; do
	  if (test x-brtl = "x$ld_flag" || test x-Wl,-brtl = "x$ld_flag"); then
	    aix_use_runtimelinking=yes
	    break
	  fi
	  done
	  if test svr4,no = "$with_aix_soname,$aix_use_runtimelinking"; then
	    # With aix-soname=svr4, we create the lib.so.V shared archives only,
	    # so we don't have lib.a shared libs to link our executables.
	    # We have to force runtime linking in this case.
	    aix_use_runtimelinking=yes
	    LDFLAGS="$LDFLAGS -Wl,-brtl"
	  fi
	  ;;
	esac

	exp_sym_flag='-bexport'
	no_entry_flag='-bnoentry'
      fi

      # When large executables or shared objects are built, AIX ld can
      # have problems creating the table of contents.  If linking a library
      # or program results in "error TOC overflow" add -mminimal-toc to
      # CXXFLAGS/CFLAGS for g++/gcc.  In the cases where that is not
      # enough to fix the problem, add -Wl,-bbigtoc to LDFLAGS.

      _LT_TAGVAR(archive_cmds, $1)=''
      _LT_TAGVAR(hardcode_direct, $1)=yes
      _LT_TAGVAR(hardcode_direct_absolute, $1)=yes
      _LT_TAGVAR(hardcode_libdir_separator, $1)=':'
      _LT_TAGVAR(link_all_deplibs, $1)=yes
      _LT_TAGVAR(file_list_spec, $1)='$wl-f,'
      case $with_aix_soname,$aix_use_runtimelinking in
      aix,*) ;; # traditional, no import file
      svr4,* | *,yes) # use import file
	# The Import File defines what to hardcode.
	_LT_TAGVAR(hardcode_direct, $1)=no
	_LT_TAGVAR(hardcode_direct_absolute, $1)=no
	;;
      esac

      if test yes = "$GCC"; then
	case $host_os in aix4.[[012]]|aix4.[[012]].*)
	# We only want to do this on AIX 4.2 and lower, the check
	# below for broken collect2 doesn't work under 4.3+
	  collect2name=`$CC -print-prog-name=collect2`
	  if test -f "$collect2name" &&
	   strings "$collect2name" | $GREP resolve_lib_name >/dev/null
	  then
	  # We have reworked collect2
	  :
	  else
	  # We have old collect2
	  _LT_TAGVAR(hardcode_direct, $1)=unsupported
	  # It fails to find uninstalled libraries when the uninstalled
	  # path is not listed in the libpath.  Setting hardcode_minus_L
	  # to unsupported forces relinking
	  _LT_TAGVAR(hardcode_minus_L, $1)=yes
	  _LT_TAGVAR(hardcode_libdir_flag_spec, $1)='-L$libdir'
	  _LT_TAGVAR(hardcode_libdir_separator, $1)=
	  fi
	  ;;
	esac
	shared_flag='-shared'
	if test yes = "$aix_use_runtimelinking"; then
	  shared_flag="$shared_flag "'$wl-G'
	fi
	# Need to ensure runtime linking is disabled for the traditional
	# shared library, or the linker may eventually find shared libraries
	# /with/ Import File - we do not want to mix them.
	shared_flag_aix='-shared'
	shared_flag_svr4='-shared $wl-G'
      else
	# not using gcc
	if test ia64 = "$host_cpu"; then
	# VisualAge C++, Version 5.5 for AIX 5L for IA-64, Beta 3 Release
	# chokes on -Wl,-G. The following line is correct:
	  shared_flag='-G'
	else
	  if test yes = "$aix_use_runtimelinking"; then
	    shared_flag='$wl-G'
	  else
	    shared_flag='$wl-bM:SRE'
	  fi
	  shared_flag_aix='$wl-bM:SRE'
	  shared_flag_svr4='$wl-G'
	fi
      fi

      _LT_TAGVAR(export_dynamic_flag_spec, $1)='$wl-bexpall'
      # It seems that -bexpall does not export symbols beginning with
      # underscore (_), so it is better to generate a list of symbols to export.
      _LT_TAGVAR(always_export_symbols, $1)=yes
      if test aix,yes = "$with_aix_soname,$aix_use_runtimelinking"; then
	# Warning - without using the other runtime loading flags (-brtl),
	# -berok will link without error, but may produce a broken library.
	_LT_TAGVAR(allow_undefined_flag, $1)='-berok'
        # Determine the default libpath from the value encoded in an
        # empty executable.
        _LT_SYS_MODULE_PATH_AIX([$1])
        _LT_TAGVAR(hardcode_libdir_flag_spec, $1)='$wl-blibpath:$libdir:'"$aix_libpath"
        _LT_TAGVAR(archive_expsym_cmds, $1)='$CC -o $output_objdir/$soname $libobjs $deplibs $wl'$no_entry_flag' $compiler_flags `if test -n "$allow_undefined_flag"; then func_echo_all "$wl$allow_undefined_flag"; else :; fi` $wl'$exp_sym_flag:\$export_symbols' '$shared_flag
      else
	if test ia64 = "$host_cpu"; then
	  _LT_TAGVAR(hardcode_libdir_flag_spec, $1)='$wl-R $libdir:/usr/lib:/lib'
	  _LT_TAGVAR(allow_undefined_flag, $1)="-z nodefs"
	  _LT_TAGVAR(archive_expsym_cmds, $1)="\$CC $shared_flag"' -o $output_objdir/$soname $libobjs $deplibs '"\$wl$no_entry_flag"' $compiler_flags $wl$allow_undefined_flag '"\$wl$exp_sym_flag:\$export_symbols"
	else
	 # Determine the default libpath from the value encoded in an
	 # empty executable.
	 _LT_SYS_MODULE_PATH_AIX([$1])
	 _LT_TAGVAR(hardcode_libdir_flag_spec, $1)='$wl-blibpath:$libdir:'"$aix_libpath"
	  # Warning - without using the other run time loading flags,
	  # -berok will link without error, but may produce a broken library.
	  _LT_TAGVAR(no_undefined_flag, $1)=' $wl-bernotok'
	  _LT_TAGVAR(allow_undefined_flag, $1)=' $wl-berok'
	  if test yes = "$with_gnu_ld"; then
	    # We only use this code for GNU lds that support --whole-archive.
	    _LT_TAGVAR(whole_archive_flag_spec, $1)='$wl--whole-archive$convenience $wl--no-whole-archive'
	  else
	    # Exported symbols can be pulled into shared objects from archives
	    _LT_TAGVAR(whole_archive_flag_spec, $1)='$convenience'
	  fi
	  _LT_TAGVAR(archive_cmds_need_lc, $1)=yes
	  _LT_TAGVAR(archive_expsym_cmds, $1)='$RM -r $output_objdir/$realname.d~$MKDIR $output_objdir/$realname.d'
	  # -brtl affects multiple linker settings, -berok does not and is overridden later
	  compiler_flags_filtered='`func_echo_all "$compiler_flags " | $SED -e "s%-brtl\\([[, ]]\\)%-berok\\1%g"`'
	  if test svr4 != "$with_aix_soname"; then
	    # This is similar to how AIX traditionally builds its shared libraries.
	    _LT_TAGVAR(archive_expsym_cmds, $1)="$_LT_TAGVAR(archive_expsym_cmds, $1)"'~$CC '$shared_flag_aix' -o $output_objdir/$realname.d/$soname $libobjs $deplibs $wl-bnoentry '$compiler_flags_filtered'$wl-bE:$export_symbols$allow_undefined_flag~$AR $AR_FLAGS $output_objdir/$libname$release.a $output_objdir/$realname.d/$soname'
	  fi
	  if test aix != "$with_aix_soname"; then
	    _LT_TAGVAR(archive_expsym_cmds, $1)="$_LT_TAGVAR(archive_expsym_cmds, $1)"'~$CC '$shared_flag_svr4' -o $output_objdir/$realname.d/$shared_archive_member_spec.o $libobjs $deplibs $wl-bnoentry '$compiler_flags_filtered'$wl-bE:$export_symbols$allow_undefined_flag~$STRIP -e $output_objdir/$realname.d/$shared_archive_member_spec.o~( func_echo_all "#! $soname($shared_archive_member_spec.o)"; if test shr_64 = "$shared_archive_member_spec"; then func_echo_all "# 64"; else func_echo_all "# 32"; fi; cat $export_symbols ) > $output_objdir/$realname.d/$shared_archive_member_spec.imp~$AR $AR_FLAGS $output_objdir/$soname $output_objdir/$realname.d/$shared_archive_member_spec.o $output_objdir/$realname.d/$shared_archive_member_spec.imp'
	  else
	    # used by -dlpreopen to get the symbols
	    _LT_TAGVAR(archive_expsym_cmds, $1)="$_LT_TAGVAR(archive_expsym_cmds, $1)"'~$MV  $output_objdir/$realname.d/$soname $output_objdir'
	  fi
	  _LT_TAGVAR(archive_expsym_cmds, $1)="$_LT_TAGVAR(archive_expsym_cmds, $1)"'~$RM -r $output_objdir/$realname.d'
	fi
      fi
      ;;

    amigaos*)
      case $host_cpu in
      powerpc)
            # see comment about AmigaOS4 .so support
            _LT_TAGVAR(archive_cmds, $1)='$CC -shared $libobjs $deplibs $compiler_flags $wl-soname $wl$soname -o $lib'
            _LT_TAGVAR(archive_expsym_cmds, $1)=''
        ;;
      m68k)
            _LT_TAGVAR(archive_cmds, $1)='$RM $output_objdir/a2ixlibrary.data~$ECHO "#define NAME $libname" > $output_objdir/a2ixlibrary.data~$ECHO "#define LIBRARY_ID 1" >> $output_objdir/a2ixlibrary.data~$ECHO "#define VERSION $major" >> $output_objdir/a2ixlibrary.data~$ECHO "#define REVISION $revision" >> $output_objdir/a2ixlibrary.data~$AR $AR_FLAGS $lib $libobjs~$RANLIB $lib~(cd $output_objdir && a2ixlibrary -32)'
            _LT_TAGVAR(hardcode_libdir_flag_spec, $1)='-L$libdir'
            _LT_TAGVAR(hardcode_minus_L, $1)=yes
        ;;
      esac
      ;;

    bsdi[[45]]*)
      _LT_TAGVAR(export_dynamic_flag_spec, $1)=-rdynamic
      ;;

    cygwin* | mingw* | pw32* | cegcc*)
      # When not using gcc, we currently assume that we are using
      # Microsoft Visual C++.
      # hardcode_libdir_flag_spec is actually meaningless, as there is
      # no search path for DLLs.
      case $cc_basename in
      cl*)
	# Native MSVC
	_LT_TAGVAR(hardcode_libdir_flag_spec, $1)=' '
	_LT_TAGVAR(allow_undefined_flag, $1)=unsupported
	_LT_TAGVAR(always_export_symbols, $1)=yes
	_LT_TAGVAR(file_list_spec, $1)='@'
	# Tell ltmain to make .lib files, not .a files.
	libext=lib
	# Tell ltmain to make .dll files, not .so files.
	shrext_cmds=.dll
	# FIXME: Setting linknames here is a bad hack.
	_LT_TAGVAR(archive_cmds, $1)='$CC -o $output_objdir/$soname $libobjs $compiler_flags $deplibs -Wl,-DLL,-IMPLIB:"$tool_output_objdir$libname.dll.lib"~linknames='
	_LT_TAGVAR(archive_expsym_cmds, $1)='if _LT_DLL_DEF_P([$export_symbols]); then
            cp "$export_symbols" "$output_objdir/$soname.def";
            echo "$tool_output_objdir$soname.def" > "$output_objdir/$soname.exp";
          else
            $SED -e '\''s/^/-link -EXPORT:/'\'' < $export_symbols > $output_objdir/$soname.exp;
          fi~
          $CC -o $tool_output_objdir$soname $libobjs $compiler_flags $deplibs "@$tool_output_objdir$soname.exp" -Wl,-DLL,-IMPLIB:"$tool_output_objdir$libname.dll.lib"~
          linknames='
	# The linker will not automatically build a static lib if we build a DLL.
	# _LT_TAGVAR(old_archive_from_new_cmds, $1)='true'
	_LT_TAGVAR(enable_shared_with_static_runtimes, $1)=yes
	_LT_TAGVAR(exclude_expsyms, $1)='_NULL_IMPORT_DESCRIPTOR|_IMPORT_DESCRIPTOR_.*'
	_LT_TAGVAR(export_symbols_cmds, $1)='$NM $libobjs $convenience | $global_symbol_pipe | $SED -e '\''/^[[BCDGRS]][[ ]]/s/.*[[ ]]\([[^ ]]*\)/\1,DATA/'\'' | $SED -e '\''/^[[AITW]][[ ]]/s/.*[[ ]]//'\'' | sort | uniq > $export_symbols'
	# Don't use ranlib
	_LT_TAGVAR(old_postinstall_cmds, $1)='chmod 644 $oldlib'
	_LT_TAGVAR(postlink_cmds, $1)='lt_outputfile="@OUTPUT@"~
          lt_tool_outputfile="@TOOL_OUTPUT@"~
          case $lt_outputfile in
            *.exe|*.EXE) ;;
            *)
              lt_outputfile=$lt_outputfile.exe
              lt_tool_outputfile=$lt_tool_outputfile.exe
              ;;
          esac~
          if test : != "$MANIFEST_TOOL" && test -f "$lt_outputfile.manifest"; then
            $MANIFEST_TOOL -manifest "$lt_tool_outputfile.manifest" -outputresource:"$lt_tool_outputfile" || exit 1;
            $RM "$lt_outputfile.manifest";
          fi'
	;;
      *)
	# Assume MSVC wrapper
	_LT_TAGVAR(hardcode_libdir_flag_spec, $1)=' '
	_LT_TAGVAR(allow_undefined_flag, $1)=unsupported
	# Tell ltmain to make .lib files, not .a files.
	libext=lib
	# Tell ltmain to make .dll files, not .so files.
	shrext_cmds=.dll
	# FIXME: Setting linknames here is a bad hack.
	_LT_TAGVAR(archive_cmds, $1)='$CC -o $lib $libobjs $compiler_flags `func_echo_all "$deplibs" | $SED '\''s/ -lc$//'\''` -link -dll~linknames='
	# The linker will automatically build a .lib file if we build a DLL.
	_LT_TAGVAR(old_archive_from_new_cmds, $1)='true'
	# FIXME: Should let the user specify the lib program.
	_LT_TAGVAR(old_archive_cmds, $1)='lib -OUT:$oldlib$oldobjs$old_deplibs'
	_LT_TAGVAR(enable_shared_with_static_runtimes, $1)=yes
	;;
      esac
      ;;

    darwin* | rhapsody*)
      _LT_DARWIN_LINKER_FEATURES($1)
      ;;

    dgux*)
      _LT_TAGVAR(archive_cmds, $1)='$LD -G -h $soname -o $lib $libobjs $deplibs $linker_flags'
      _LT_TAGVAR(hardcode_libdir_flag_spec, $1)='-L$libdir'
      _LT_TAGVAR(hardcode_shlibpath_var, $1)=no
      ;;

    # FreeBSD 2.2.[012] allows us to include c++rt0.o to get C++ constructor
    # support.  Future versions do this automatically, but an explicit c++rt0.o
    # does not break anything, and helps significantly (at the cost of a little
    # extra space).
    freebsd2.2*)
      _LT_TAGVAR(archive_cmds, $1)='$LD -Bshareable -o $lib $libobjs $deplibs $linker_flags /usr/lib/c++rt0.o'
      _LT_TAGVAR(hardcode_libdir_flag_spec, $1)='-R$libdir'
      _LT_TAGVAR(hardcode_direct, $1)=yes
      _LT_TAGVAR(hardcode_shlibpath_var, $1)=no
      ;;

    # Unfortunately, older versions of FreeBSD 2 do not have this feature.
    freebsd2.*)
      _LT_TAGVAR(archive_cmds, $1)='$LD -Bshareable -o $lib $libobjs $deplibs $linker_flags'
      _LT_TAGVAR(hardcode_direct, $1)=yes
      _LT_TAGVAR(hardcode_minus_L, $1)=yes
      _LT_TAGVAR(hardcode_shlibpath_var, $1)=no
      ;;

    # FreeBSD 3 and greater uses gcc -shared to do shared libraries.
    freebsd* | dragonfly*)
      _LT_TAGVAR(archive_cmds, $1)='$CC -shared $pic_flag -o $lib $libobjs $deplibs $compiler_flags'
      _LT_TAGVAR(hardcode_libdir_flag_spec, $1)='-R$libdir'
      _LT_TAGVAR(hardcode_direct, $1)=yes
      _LT_TAGVAR(hardcode_shlibpath_var, $1)=no
      ;;

    hpux9*)
      if test yes = "$GCC"; then
	_LT_TAGVAR(archive_cmds, $1)='$RM $output_objdir/$soname~$CC -shared $pic_flag $wl+b $wl$install_libdir -o $output_objdir/$soname $libobjs $deplibs $compiler_flags~test "x$output_objdir/$soname" = "x$lib" || mv $output_objdir/$soname $lib'
      else
	_LT_TAGVAR(archive_cmds, $1)='$RM $output_objdir/$soname~$LD -b +b $install_libdir -o $output_objdir/$soname $libobjs $deplibs $linker_flags~test "x$output_objdir/$soname" = "x$lib" || mv $output_objdir/$soname $lib'
      fi
      _LT_TAGVAR(hardcode_libdir_flag_spec, $1)='$wl+b $wl$libdir'
      _LT_TAGVAR(hardcode_libdir_separator, $1)=:
      _LT_TAGVAR(hardcode_direct, $1)=yes

      # hardcode_minus_L: Not really in the search PATH,
      # but as the default location of the library.
      _LT_TAGVAR(hardcode_minus_L, $1)=yes
      _LT_TAGVAR(export_dynamic_flag_spec, $1)='$wl-E'
      ;;

    hpux10*)
      if test yes,no = "$GCC,$with_gnu_ld"; then
	_LT_TAGVAR(archive_cmds, $1)='$CC -shared $pic_flag $wl+h $wl$soname $wl+b $wl$install_libdir -o $lib $libobjs $deplibs $compiler_flags'
      else
	_LT_TAGVAR(archive_cmds, $1)='$LD -b +h $soname +b $install_libdir -o $lib $libobjs $deplibs $linker_flags'
      fi
      if test no = "$with_gnu_ld"; then
	_LT_TAGVAR(hardcode_libdir_flag_spec, $1)='$wl+b $wl$libdir'
	_LT_TAGVAR(hardcode_libdir_separator, $1)=:
	_LT_TAGVAR(hardcode_direct, $1)=yes
	_LT_TAGVAR(hardcode_direct_absolute, $1)=yes
	_LT_TAGVAR(export_dynamic_flag_spec, $1)='$wl-E'
	# hardcode_minus_L: Not really in the search PATH,
	# but as the default location of the library.
	_LT_TAGVAR(hardcode_minus_L, $1)=yes
      fi
      ;;

    hpux11*)
      if test yes,no = "$GCC,$with_gnu_ld"; then
	case $host_cpu in
	hppa*64*)
	  _LT_TAGVAR(archive_cmds, $1)='$CC -shared $wl+h $wl$soname -o $lib $libobjs $deplibs $compiler_flags'
	  ;;
	ia64*)
	  _LT_TAGVAR(archive_cmds, $1)='$CC -shared $pic_flag $wl+h $wl$soname $wl+nodefaultrpath -o $lib $libobjs $deplibs $compiler_flags'
	  ;;
	*)
	  _LT_TAGVAR(archive_cmds, $1)='$CC -shared $pic_flag $wl+h $wl$soname $wl+b $wl$install_libdir -o $lib $libobjs $deplibs $compiler_flags'
	  ;;
	esac
      else
	case $host_cpu in
	hppa*64*)
	  _LT_TAGVAR(archive_cmds, $1)='$CC -b $wl+h $wl$soname -o $lib $libobjs $deplibs $compiler_flags'
	  ;;
	ia64*)
	  _LT_TAGVAR(archive_cmds, $1)='$CC -b $wl+h $wl$soname $wl+nodefaultrpath -o $lib $libobjs $deplibs $compiler_flags'
	  ;;
	*)
	m4_if($1, [], [
	  # Older versions of the 11.00 compiler do not understand -b yet
	  # (HP92453-01 A.11.01.20 doesn't, HP92453-01 B.11.X.35175-35176.GP does)
	  _LT_LINKER_OPTION([if $CC understands -b],
	    _LT_TAGVAR(lt_cv_prog_compiler__b, $1), [-b],
	    [_LT_TAGVAR(archive_cmds, $1)='$CC -b $wl+h $wl$soname $wl+b $wl$install_libdir -o $lib $libobjs $deplibs $compiler_flags'],
	    [_LT_TAGVAR(archive_cmds, $1)='$LD -b +h $soname +b $install_libdir -o $lib $libobjs $deplibs $linker_flags'])],
	  [_LT_TAGVAR(archive_cmds, $1)='$CC -b $wl+h $wl$soname $wl+b $wl$install_libdir -o $lib $libobjs $deplibs $compiler_flags'])
	  ;;
	esac
      fi
      if test no = "$with_gnu_ld"; then
	_LT_TAGVAR(hardcode_libdir_flag_spec, $1)='$wl+b $wl$libdir'
	_LT_TAGVAR(hardcode_libdir_separator, $1)=:

	case $host_cpu in
	hppa*64*|ia64*)
	  _LT_TAGVAR(hardcode_direct, $1)=no
	  _LT_TAGVAR(hardcode_shlibpath_var, $1)=no
	  ;;
	*)
	  _LT_TAGVAR(hardcode_direct, $1)=yes
	  _LT_TAGVAR(hardcode_direct_absolute, $1)=yes
	  _LT_TAGVAR(export_dynamic_flag_spec, $1)='$wl-E'

	  # hardcode_minus_L: Not really in the search PATH,
	  # but as the default location of the library.
	  _LT_TAGVAR(hardcode_minus_L, $1)=yes
	  ;;
	esac
      fi
      ;;

    irix5* | irix6* | nonstopux*)
      if test yes = "$GCC"; then
	_LT_TAGVAR(archive_cmds, $1)='$CC -shared $pic_flag $libobjs $deplibs $compiler_flags $wl-soname $wl$soname `test -n "$verstring" && func_echo_all "$wl-set_version $wl$verstring"` $wl-update_registry $wl$output_objdir/so_locations -o $lib'
	# Try to use the -exported_symbol ld option, if it does not
	# work, assume that -exports_file does not work either and
	# implicitly export all symbols.
	# This should be the same for all languages, so no per-tag cache variable.
	AC_CACHE_CHECK([whether the $host_os linker accepts -exported_symbol],
	  [lt_cv_irix_exported_symbol],
	  [save_LDFLAGS=$LDFLAGS
	   LDFLAGS="$LDFLAGS -shared $wl-exported_symbol ${wl}foo $wl-update_registry $wl/dev/null"
	   AC_LINK_IFELSE(
	     [AC_LANG_SOURCE(
	        [AC_LANG_CASE([C], [[int foo (void) { return 0; }]],
			      [C++], [[int foo (void) { return 0; }]],
			      [Fortran 77], [[
      subroutine foo
      end]],
			      [Fortran], [[
      subroutine foo
      end]])])],
	      [lt_cv_irix_exported_symbol=yes],
	      [lt_cv_irix_exported_symbol=no])
           LDFLAGS=$save_LDFLAGS])
	if test yes = "$lt_cv_irix_exported_symbol"; then
          _LT_TAGVAR(archive_expsym_cmds, $1)='$CC -shared $pic_flag $libobjs $deplibs $compiler_flags $wl-soname $wl$soname `test -n "$verstring" && func_echo_all "$wl-set_version $wl$verstring"` $wl-update_registry $wl$output_objdir/so_locations $wl-exports_file $wl$export_symbols -o $lib'
	fi
	_LT_TAGVAR(link_all_deplibs, $1)=no
      else
	_LT_TAGVAR(archive_cmds, $1)='$CC -shared $libobjs $deplibs $compiler_flags -soname $soname `test -n "$verstring" && func_echo_all "-set_version $verstring"` -update_registry $output_objdir/so_locations -o $lib'
	_LT_TAGVAR(archive_expsym_cmds, $1)='$CC -shared $libobjs $deplibs $compiler_flags -soname $soname `test -n "$verstring" && func_echo_all "-set_version $verstring"` -update_registry $output_objdir/so_locations -exports_file $export_symbols -o $lib'
      fi
      _LT_TAGVAR(archive_cmds_need_lc, $1)='no'
      _LT_TAGVAR(hardcode_libdir_flag_spec, $1)='$wl-rpath $wl$libdir'
      _LT_TAGVAR(hardcode_libdir_separator, $1)=:
      _LT_TAGVAR(inherit_rpath, $1)=yes
      _LT_TAGVAR(link_all_deplibs, $1)=yes
      ;;

    linux*)
      case $cc_basename in
      tcc*)
	# Fabrice Bellard et al's Tiny C Compiler
	_LT_TAGVAR(ld_shlibs, $1)=yes
	_LT_TAGVAR(archive_cmds, $1)='$CC -shared $pic_flag -o $lib $libobjs $deplibs $compiler_flags'
	;;
      esac
      ;;

    netbsd* | netbsdelf*-gnu)
      if echo __ELF__ | $CC -E - | $GREP __ELF__ >/dev/null; then
	_LT_TAGVAR(archive_cmds, $1)='$LD -Bshareable -o $lib $libobjs $deplibs $linker_flags'  # a.out
      else
	_LT_TAGVAR(archive_cmds, $1)='$LD -shared -o $lib $libobjs $deplibs $linker_flags'      # ELF
      fi
      _LT_TAGVAR(hardcode_libdir_flag_spec, $1)='-R$libdir'
      _LT_TAGVAR(hardcode_direct, $1)=yes
      _LT_TAGVAR(hardcode_shlibpath_var, $1)=no
      ;;

    newsos6)
      _LT_TAGVAR(archive_cmds, $1)='$LD -G -h $soname -o $lib $libobjs $deplibs $linker_flags'
      _LT_TAGVAR(hardcode_direct, $1)=yes
      _LT_TAGVAR(hardcode_libdir_flag_spec, $1)='$wl-rpath $wl$libdir'
      _LT_TAGVAR(hardcode_libdir_separator, $1)=:
      _LT_TAGVAR(hardcode_shlibpath_var, $1)=no
      ;;

    *nto* | *qnx*)
      ;;

    openbsd* | bitrig*)
      if test -f /usr/libexec/ld.so; then
	_LT_TAGVAR(hardcode_direct, $1)=yes
	_LT_TAGVAR(hardcode_shlibpath_var, $1)=no
	_LT_TAGVAR(hardcode_direct_absolute, $1)=yes
	if test -z "`echo __ELF__ | $CC -E - | $GREP __ELF__`"; then
	  _LT_TAGVAR(archive_cmds, $1)='$CC -shared $pic_flag -o $lib $libobjs $deplibs $compiler_flags'
	  _LT_TAGVAR(archive_expsym_cmds, $1)='$CC -shared $pic_flag -o $lib $libobjs $deplibs $compiler_flags $wl-retain-symbols-file,$export_symbols'
	  _LT_TAGVAR(hardcode_libdir_flag_spec, $1)='$wl-rpath,$libdir'
	  _LT_TAGVAR(export_dynamic_flag_spec, $1)='$wl-E'
	else
	  _LT_TAGVAR(archive_cmds, $1)='$CC -shared $pic_flag -o $lib $libobjs $deplibs $compiler_flags'
	  _LT_TAGVAR(hardcode_libdir_flag_spec, $1)='$wl-rpath,$libdir'
	fi
      else
	_LT_TAGVAR(ld_shlibs, $1)=no
      fi
      ;;

    os2*)
      _LT_TAGVAR(hardcode_libdir_flag_spec, $1)='-L$libdir'
      _LT_TAGVAR(hardcode_minus_L, $1)=yes
      _LT_TAGVAR(allow_undefined_flag, $1)=unsupported
      shrext_cmds=.dll
      _LT_TAGVAR(archive_cmds, $1)='$ECHO "LIBRARY ${soname%$shared_ext} INITINSTANCE TERMINSTANCE" > $output_objdir/$libname.def~
	$ECHO "DESCRIPTION \"$libname\"" >> $output_objdir/$libname.def~
	$ECHO "DATA MULTIPLE NONSHARED" >> $output_objdir/$libname.def~
	$ECHO EXPORTS >> $output_objdir/$libname.def~
	emxexp $libobjs | $SED /"_DLL_InitTerm"/d >> $output_objdir/$libname.def~
	$CC -Zdll -Zcrtdll -o $output_objdir/$soname $libobjs $deplibs $compiler_flags $output_objdir/$libname.def~
	emximp -o $lib $output_objdir/$libname.def'
      _LT_TAGVAR(archive_expsym_cmds, $1)='$ECHO "LIBRARY ${soname%$shared_ext} INITINSTANCE TERMINSTANCE" > $output_objdir/$libname.def~
	$ECHO "DESCRIPTION \"$libname\"" >> $output_objdir/$libname.def~
	$ECHO "DATA MULTIPLE NONSHARED" >> $output_objdir/$libname.def~
	$ECHO EXPORTS >> $output_objdir/$libname.def~
	prefix_cmds="$SED"~
	if test EXPORTS = "`$SED 1q $export_symbols`"; then
	  prefix_cmds="$prefix_cmds -e 1d";
	fi~
	prefix_cmds="$prefix_cmds -e \"s/^\(.*\)$/_\1/g\""~
	cat $export_symbols | $prefix_cmds >> $output_objdir/$libname.def~
	$CC -Zdll -Zcrtdll -o $output_objdir/$soname $libobjs $deplibs $compiler_flags $output_objdir/$libname.def~
	emximp -o $lib $output_objdir/$libname.def'
      _LT_TAGVAR(old_archive_From_new_cmds, $1)='emximp -o $output_objdir/${libname}_dll.a $output_objdir/$libname.def'
      _LT_TAGVAR(enable_shared_with_static_runtimes, $1)=yes
      ;;

    osf3*)
      if test yes = "$GCC"; then
	_LT_TAGVAR(allow_undefined_flag, $1)=' $wl-expect_unresolved $wl\*'
	_LT_TAGVAR(archive_cmds, $1)='$CC -shared$allow_undefined_flag $libobjs $deplibs $compiler_flags $wl-soname $wl$soname `test -n "$verstring" && func_echo_all "$wl-set_version $wl$verstring"` $wl-update_registry $wl$output_objdir/so_locations -o $lib'
      else
	_LT_TAGVAR(allow_undefined_flag, $1)=' -expect_unresolved \*'
	_LT_TAGVAR(archive_cmds, $1)='$CC -shared$allow_undefined_flag $libobjs $deplibs $compiler_flags -soname $soname `test -n "$verstring" && func_echo_all "-set_version $verstring"` -update_registry $output_objdir/so_locations -o $lib'
      fi
      _LT_TAGVAR(archive_cmds_need_lc, $1)='no'
      _LT_TAGVAR(hardcode_libdir_flag_spec, $1)='$wl-rpath $wl$libdir'
      _LT_TAGVAR(hardcode_libdir_separator, $1)=:
      ;;

    osf4* | osf5*)	# as osf3* with the addition of -msym flag
      if test yes = "$GCC"; then
	_LT_TAGVAR(allow_undefined_flag, $1)=' $wl-expect_unresolved $wl\*'
	_LT_TAGVAR(archive_cmds, $1)='$CC -shared$allow_undefined_flag $pic_flag $libobjs $deplibs $compiler_flags $wl-msym $wl-soname $wl$soname `test -n "$verstring" && func_echo_all "$wl-set_version $wl$verstring"` $wl-update_registry $wl$output_objdir/so_locations -o $lib'
	_LT_TAGVAR(hardcode_libdir_flag_spec, $1)='$wl-rpath $wl$libdir'
      else
	_LT_TAGVAR(allow_undefined_flag, $1)=' -expect_unresolved \*'
	_LT_TAGVAR(archive_cmds, $1)='$CC -shared$allow_undefined_flag $libobjs $deplibs $compiler_flags -msym -soname $soname `test -n "$verstring" && func_echo_all "-set_version $verstring"` -update_registry $output_objdir/so_locations -o $lib'
	_LT_TAGVAR(archive_expsym_cmds, $1)='for i in `cat $export_symbols`; do printf "%s %s\\n" -exported_symbol "\$i" >> $lib.exp; done; printf "%s\\n" "-hidden">> $lib.exp~
          $CC -shared$allow_undefined_flag $wl-input $wl$lib.exp $compiler_flags $libobjs $deplibs -soname $soname `test -n "$verstring" && $ECHO "-set_version $verstring"` -update_registry $output_objdir/so_locations -o $lib~$RM $lib.exp'

	# Both c and cxx compiler support -rpath directly
	_LT_TAGVAR(hardcode_libdir_flag_spec, $1)='-rpath $libdir'
      fi
      _LT_TAGVAR(archive_cmds_need_lc, $1)='no'
      _LT_TAGVAR(hardcode_libdir_separator, $1)=:
      ;;

    solaris*)
      _LT_TAGVAR(no_undefined_flag, $1)=' -z defs'
      if test yes = "$GCC"; then
	wlarc='$wl'
	_LT_TAGVAR(archive_cmds, $1)='$CC -shared $pic_flag $wl-z ${wl}text $wl-h $wl$soname -o $lib $libobjs $deplibs $compiler_flags'
	_LT_TAGVAR(archive_expsym_cmds, $1)='echo "{ global:" > $lib.exp~cat $export_symbols | $SED -e "s/\(.*\)/\1;/" >> $lib.exp~echo "local: *; };" >> $lib.exp~
          $CC -shared $pic_flag $wl-z ${wl}text $wl-M $wl$lib.exp $wl-h $wl$soname -o $lib $libobjs $deplibs $compiler_flags~$RM $lib.exp'
      else
	case `$CC -V 2>&1` in
	*"Compilers 5.0"*)
	  wlarc=''
	  _LT_TAGVAR(archive_cmds, $1)='$LD -G$allow_undefined_flag -h $soname -o $lib $libobjs $deplibs $linker_flags'
	  _LT_TAGVAR(archive_expsym_cmds, $1)='echo "{ global:" > $lib.exp~cat $export_symbols | $SED -e "s/\(.*\)/\1;/" >> $lib.exp~echo "local: *; };" >> $lib.exp~
            $LD -G$allow_undefined_flag -M $lib.exp -h $soname -o $lib $libobjs $deplibs $linker_flags~$RM $lib.exp'
	  ;;
	*)
	  wlarc='$wl'
	  _LT_TAGVAR(archive_cmds, $1)='$CC -G$allow_undefined_flag -h $soname -o $lib $libobjs $deplibs $compiler_flags'
	  _LT_TAGVAR(archive_expsym_cmds, $1)='echo "{ global:" > $lib.exp~cat $export_symbols | $SED -e "s/\(.*\)/\1;/" >> $lib.exp~echo "local: *; };" >> $lib.exp~
            $CC -G$allow_undefined_flag -M $lib.exp -h $soname -o $lib $libobjs $deplibs $compiler_flags~$RM $lib.exp'
	  ;;
	esac
      fi
      _LT_TAGVAR(hardcode_libdir_flag_spec, $1)='-R$libdir'
      _LT_TAGVAR(hardcode_shlibpath_var, $1)=no
      case $host_os in
      solaris2.[[0-5]] | solaris2.[[0-5]].*) ;;
      *)
	# The compiler driver will combine and reorder linker options,
	# but understands '-z linker_flag'.  GCC discards it without '$wl',
	# but is careful enough not to reorder.
	# Supported since Solaris 2.6 (maybe 2.5.1?)
	if test yes = "$GCC"; then
	  _LT_TAGVAR(whole_archive_flag_spec, $1)='$wl-z ${wl}allextract$convenience $wl-z ${wl}defaultextract'
	else
	  _LT_TAGVAR(whole_archive_flag_spec, $1)='-z allextract$convenience -z defaultextract'
	fi
	;;
      esac
      _LT_TAGVAR(link_all_deplibs, $1)=yes
      ;;

    sunos4*)
      if test sequent = "$host_vendor"; then
	# Use $CC to link under sequent, because it throws in some extra .o
	# files that make .init and .fini sections work.
	_LT_TAGVAR(archive_cmds, $1)='$CC -G $wl-h $soname -o $lib $libobjs $deplibs $compiler_flags'
      else
	_LT_TAGVAR(archive_cmds, $1)='$LD -assert pure-text -Bstatic -o $lib $libobjs $deplibs $linker_flags'
      fi
      _LT_TAGVAR(hardcode_libdir_flag_spec, $1)='-L$libdir'
      _LT_TAGVAR(hardcode_direct, $1)=yes
      _LT_TAGVAR(hardcode_minus_L, $1)=yes
      _LT_TAGVAR(hardcode_shlibpath_var, $1)=no
      ;;

    sysv4)
      case $host_vendor in
	sni)
	  _LT_TAGVAR(archive_cmds, $1)='$LD -G -h $soname -o $lib $libobjs $deplibs $linker_flags'
	  _LT_TAGVAR(hardcode_direct, $1)=yes # is this really true???
	;;
	siemens)
	  ## LD is ld it makes a PLAMLIB
	  ## CC just makes a GrossModule.
	  _LT_TAGVAR(archive_cmds, $1)='$LD -G -o $lib $libobjs $deplibs $linker_flags'
	  _LT_TAGVAR(reload_cmds, $1)='$CC -r -o $output$reload_objs'
	  _LT_TAGVAR(hardcode_direct, $1)=no
        ;;
	motorola)
	  _LT_TAGVAR(archive_cmds, $1)='$LD -G -h $soname -o $lib $libobjs $deplibs $linker_flags'
	  _LT_TAGVAR(hardcode_direct, $1)=no #Motorola manual says yes, but my tests say they lie
	;;
      esac
      runpath_var='LD_RUN_PATH'
      _LT_TAGVAR(hardcode_shlibpath_var, $1)=no
      ;;

    sysv4.3*)
      _LT_TAGVAR(archive_cmds, $1)='$LD -G -h $soname -o $lib $libobjs $deplibs $linker_flags'
      _LT_TAGVAR(hardcode_shlibpath_var, $1)=no
      _LT_TAGVAR(export_dynamic_flag_spec, $1)='-Bexport'
      ;;

    sysv4*MP*)
      if test -d /usr/nec; then
	_LT_TAGVAR(archive_cmds, $1)='$LD -G -h $soname -o $lib $libobjs $deplibs $linker_flags'
	_LT_TAGVAR(hardcode_shlibpath_var, $1)=no
	runpath_var=LD_RUN_PATH
	hardcode_runpath_var=yes
	_LT_TAGVAR(ld_shlibs, $1)=yes
      fi
      ;;

    sysv4*uw2* | sysv5OpenUNIX* | sysv5UnixWare7.[[01]].[[10]]* | unixware7* | sco3.2v5.0.[[024]]*)
      _LT_TAGVAR(no_undefined_flag, $1)='$wl-z,text'
      _LT_TAGVAR(archive_cmds_need_lc, $1)=no
      _LT_TAGVAR(hardcode_shlibpath_var, $1)=no
      runpath_var='LD_RUN_PATH'

      if test yes = "$GCC"; then
	_LT_TAGVAR(archive_cmds, $1)='$CC -shared $wl-h,$soname -o $lib $libobjs $deplibs $compiler_flags'
	_LT_TAGVAR(archive_expsym_cmds, $1)='$CC -shared $wl-Bexport:$export_symbols $wl-h,$soname -o $lib $libobjs $deplibs $compiler_flags'
      else
	_LT_TAGVAR(archive_cmds, $1)='$CC -G $wl-h,$soname -o $lib $libobjs $deplibs $compiler_flags'
	_LT_TAGVAR(archive_expsym_cmds, $1)='$CC -G $wl-Bexport:$export_symbols $wl-h,$soname -o $lib $libobjs $deplibs $compiler_flags'
      fi
      ;;

    sysv5* | sco3.2v5* | sco5v6*)
      # Note: We CANNOT use -z defs as we might desire, because we do not
      # link with -lc, and that would cause any symbols used from libc to
      # always be unresolved, which means just about no library would
      # ever link correctly.  If we're not using GNU ld we use -z text
      # though, which does catch some bad symbols but isn't as heavy-handed
      # as -z defs.
      _LT_TAGVAR(no_undefined_flag, $1)='$wl-z,text'
      _LT_TAGVAR(allow_undefined_flag, $1)='$wl-z,nodefs'
      _LT_TAGVAR(archive_cmds_need_lc, $1)=no
      _LT_TAGVAR(hardcode_shlibpath_var, $1)=no
      _LT_TAGVAR(hardcode_libdir_flag_spec, $1)='$wl-R,$libdir'
      _LT_TAGVAR(hardcode_libdir_separator, $1)=':'
      _LT_TAGVAR(link_all_deplibs, $1)=yes
      _LT_TAGVAR(export_dynamic_flag_spec, $1)='$wl-Bexport'
      runpath_var='LD_RUN_PATH'

      if test yes = "$GCC"; then
	_LT_TAGVAR(archive_cmds, $1)='$CC -shared $wl-h,$soname -o $lib $libobjs $deplibs $compiler_flags'
	_LT_TAGVAR(archive_expsym_cmds, $1)='$CC -shared $wl-Bexport:$export_symbols $wl-h,$soname -o $lib $libobjs $deplibs $compiler_flags'
      else
	_LT_TAGVAR(archive_cmds, $1)='$CC -G $wl-h,$soname -o $lib $libobjs $deplibs $compiler_flags'
	_LT_TAGVAR(archive_expsym_cmds, $1)='$CC -G $wl-Bexport:$export_symbols $wl-h,$soname -o $lib $libobjs $deplibs $compiler_flags'
      fi
      ;;

    uts4*)
      _LT_TAGVAR(archive_cmds, $1)='$LD -G -h $soname -o $lib $libobjs $deplibs $linker_flags'
      _LT_TAGVAR(hardcode_libdir_flag_spec, $1)='-L$libdir'
      _LT_TAGVAR(hardcode_shlibpath_var, $1)=no
      ;;

    *)
      _LT_TAGVAR(ld_shlibs, $1)=no
      ;;
    esac

    if test sni = "$host_vendor"; then
      case $host in
      sysv4 | sysv4.2uw2* | sysv4.3* | sysv5*)
	_LT_TAGVAR(export_dynamic_flag_spec, $1)='$wl-Blargedynsym'
	;;
      esac
    fi
  fi
])
AC_MSG_RESULT([$_LT_TAGVAR(ld_shlibs, $1)])
test no = "$_LT_TAGVAR(ld_shlibs, $1)" && can_build_shared=no

_LT_TAGVAR(with_gnu_ld, $1)=$with_gnu_ld

_LT_DECL([], [libext], [0], [Old archive suffix (normally "a")])dnl
_LT_DECL([], [shrext_cmds], [1], [Shared library suffix (normally ".so")])dnl
_LT_DECL([], [extract_expsyms_cmds], [2],
    [The commands to extract the exported symbol list from a shared archive])

#
# Do we need to explicitly link libc?
#
case "x$_LT_TAGVAR(archive_cmds_need_lc, $1)" in
x|xyes)
  # Assume -lc should be added
  _LT_TAGVAR(archive_cmds_need_lc, $1)=yes

  if test yes,yes = "$GCC,$enable_shared"; then
    case $_LT_TAGVAR(archive_cmds, $1) in
    *'~'*)
      # FIXME: we may have to deal with multi-command sequences.
      ;;
    '$CC '*)
      # Test whether the compiler implicitly links with -lc since on some
      # systems, -lgcc has to come before -lc. If gcc already passes -lc
      # to ld, don't add -lc before -lgcc.
      AC_CACHE_CHECK([whether -lc should be explicitly linked in],
	[lt_cv_]_LT_TAGVAR(archive_cmds_need_lc, $1),
	[$RM conftest*
	echo "$lt_simple_compile_test_code" > conftest.$ac_ext

	if AC_TRY_EVAL(ac_compile) 2>conftest.err; then
	  soname=conftest
	  lib=conftest
	  libobjs=conftest.$ac_objext
	  deplibs=
	  wl=$_LT_TAGVAR(lt_prog_compiler_wl, $1)
	  pic_flag=$_LT_TAGVAR(lt_prog_compiler_pic, $1)
	  compiler_flags=-v
	  linker_flags=-v
	  verstring=
	  output_objdir=.
	  libname=conftest
	  lt_save_allow_undefined_flag=$_LT_TAGVAR(allow_undefined_flag, $1)
	  _LT_TAGVAR(allow_undefined_flag, $1)=
	  if AC_TRY_EVAL(_LT_TAGVAR(archive_cmds, $1) 2\>\&1 \| $GREP \" -lc \" \>/dev/null 2\>\&1)
	  then
	    lt_cv_[]_LT_TAGVAR(archive_cmds_need_lc, $1)=no
	  else
	    lt_cv_[]_LT_TAGVAR(archive_cmds_need_lc, $1)=yes
	  fi
	  _LT_TAGVAR(allow_undefined_flag, $1)=$lt_save_allow_undefined_flag
	else
	  cat conftest.err 1>&5
	fi
	$RM conftest*
	])
      _LT_TAGVAR(archive_cmds_need_lc, $1)=$lt_cv_[]_LT_TAGVAR(archive_cmds_need_lc, $1)
      ;;
    esac
  fi
  ;;
esac

_LT_TAGDECL([build_libtool_need_lc], [archive_cmds_need_lc], [0],
    [Whether or not to add -lc for building shared libraries])
_LT_TAGDECL([allow_libtool_libs_with_static_runtimes],
    [enable_shared_with_static_runtimes], [0],
    [Whether or not to disallow shared libs when runtime libs are static])
_LT_TAGDECL([], [export_dynamic_flag_spec], [1],
    [Compiler flag to allow reflexive dlopens])
_LT_TAGDECL([], [whole_archive_flag_spec], [1],
    [Compiler flag to generate shared objects directly from archives])
_LT_TAGDECL([], [compiler_needs_object], [1],
    [Whether the compiler copes with passing no objects directly])
_LT_TAGDECL([], [old_archive_from_new_cmds], [2],
    [Create an old-style archive from a shared archive])
_LT_TAGDECL([], [old_archive_from_expsyms_cmds], [2],
    [Create a temporary old-style archive to link instead of a shared archive])
_LT_TAGDECL([], [archive_cmds], [2], [Commands used to build a shared archive])
_LT_TAGDECL([], [archive_expsym_cmds], [2])
_LT_TAGDECL([], [module_cmds], [2],
    [Commands used to build a loadable module if different from building
    a shared archive.])
_LT_TAGDECL([], [module_expsym_cmds], [2])
_LT_TAGDECL([], [with_gnu_ld], [1],
    [Whether we are building with GNU ld or not])
_LT_TAGDECL([], [allow_undefined_flag], [1],
    [Flag that allows shared libraries with undefined symbols to be built])
_LT_TAGDECL([], [no_undefined_flag], [1],
    [Flag that enforces no undefined symbols])
_LT_TAGDECL([], [hardcode_libdir_flag_spec], [1],
    [Flag to hardcode $libdir into a binary during linking.
    This must work even if $libdir does not exist])
_LT_TAGDECL([], [hardcode_libdir_separator], [1],
    [Whether we need a single "-rpath" flag with a separated argument])
_LT_TAGDECL([], [hardcode_direct], [0],
    [Set to "yes" if using DIR/libNAME$shared_ext during linking hardcodes
    DIR into the resulting binary])
_LT_TAGDECL([], [hardcode_direct_absolute], [0],
    [Set to "yes" if using DIR/libNAME$shared_ext during linking hardcodes
    DIR into the resulting binary and the resulting library dependency is
    "absolute", i.e impossible to change by setting $shlibpath_var if the
    library is relocated])
_LT_TAGDECL([], [hardcode_minus_L], [0],
    [Set to "yes" if using the -LDIR flag during linking hardcodes DIR
    into the resulting binary])
_LT_TAGDECL([], [hardcode_shlibpath_var], [0],
    [Set to "yes" if using SHLIBPATH_VAR=DIR during linking hardcodes DIR
    into the resulting binary])
_LT_TAGDECL([], [hardcode_automatic], [0],
    [Set to "yes" if building a shared library automatically hardcodes DIR
    into the library and all subsequent libraries and executables linked
    against it])
_LT_TAGDECL([], [inherit_rpath], [0],
    [Set to yes if linker adds runtime paths of dependent libraries
    to runtime path list])
_LT_TAGDECL([], [link_all_deplibs], [0],
    [Whether libtool must link a program against all its dependency libraries])
_LT_TAGDECL([], [always_export_symbols], [0],
    [Set to "yes" if exported symbols are required])
_LT_TAGDECL([], [export_symbols_cmds], [2],
    [The commands to list exported symbols])
_LT_TAGDECL([], [exclude_expsyms], [1],
    [Symbols that should not be listed in the preloaded symbols])
_LT_TAGDECL([], [include_expsyms], [1],
    [Symbols that must always be exported])
_LT_TAGDECL([], [prelink_cmds], [2],
    [Commands necessary for linking programs (against libraries) with templates])
_LT_TAGDECL([], [postlink_cmds], [2],
    [Commands necessary for finishing linking programs])
_LT_TAGDECL([], [file_list_spec], [1],
    [Specify filename containing input files])
dnl FIXME: Not yet implemented
dnl _LT_TAGDECL([], [thread_safe_flag_spec], [1],
dnl    [Compiler flag to generate thread safe objects])
])# _LT_LINKER_SHLIBS


# _LT_LANG_C_CONFIG([TAG])
# ------------------------
# Ensure that the configuration variables for a C compiler are suitably
# defined.  These variables are subsequently used by _LT_CONFIG to write
# the compiler configuration to 'libtool'.
m4_defun([_LT_LANG_C_CONFIG],
[m4_require([_LT_DECL_EGREP])dnl
lt_save_CC=$CC
AC_LANG_PUSH(C)

# Source file extension for C test sources.
ac_ext=c

# Object file extension for compiled C test sources.
objext=o
_LT_TAGVAR(objext, $1)=$objext

# Code to be used in simple compile tests
lt_simple_compile_test_code="int some_variable = 0;"

# Code to be used in simple link tests
lt_simple_link_test_code='int main(){return(0);}'

_LT_TAG_COMPILER
# Save the default compiler, since it gets overwritten when the other
# tags are being tested, and _LT_TAGVAR(compiler, []) is a NOP.
compiler_DEFAULT=$CC

# save warnings/boilerplate of simple test code
_LT_COMPILER_BOILERPLATE
_LT_LINKER_BOILERPLATE

if test -n "$compiler"; then
  _LT_COMPILER_NO_RTTI($1)
  _LT_COMPILER_PIC($1)
  _LT_COMPILER_C_O($1)
  _LT_COMPILER_FILE_LOCKS($1)
  _LT_LINKER_SHLIBS($1)
  _LT_SYS_DYNAMIC_LINKER($1)
  _LT_LINKER_HARDCODE_LIBPATH($1)
  LT_SYS_DLOPEN_SELF
  _LT_CMD_STRIPLIB

  # Report what library types will actually be built
  AC_MSG_CHECKING([if libtool supports shared libraries])
  AC_MSG_RESULT([$can_build_shared])

  AC_MSG_CHECKING([whether to build shared libraries])
  test no = "$can_build_shared" && enable_shared=no

  # On AIX, shared libraries and static libraries use the same namespace, and
  # are all built from PIC.
  case $host_os in
  aix3*)
    test yes = "$enable_shared" && enable_static=no
    if test -n "$RANLIB"; then
      archive_cmds="$archive_cmds~\$RANLIB \$lib"
      postinstall_cmds='$RANLIB $lib'
    fi
    ;;

  aix[[4-9]]*)
    if test ia64 != "$host_cpu"; then
      case $enable_shared,$with_aix_soname,$aix_use_runtimelinking in
      yes,aix,yes) ;;			# shared object as lib.so file only
      yes,svr4,*) ;;			# shared object as lib.so archive member only
      yes,*) enable_static=no ;;	# shared object in lib.a archive as well
      esac
    fi
    ;;
  esac
  AC_MSG_RESULT([$enable_shared])

  AC_MSG_CHECKING([whether to build static libraries])
  # Make sure either enable_shared or enable_static is yes.
  test yes = "$enable_shared" || enable_static=yes
  AC_MSG_RESULT([$enable_static])

  _LT_CONFIG($1)
fi
AC_LANG_POP
CC=$lt_save_CC
])# _LT_LANG_C_CONFIG


# _LT_LANG_CXX_CONFIG([TAG])
# --------------------------
# Ensure that the configuration variables for a C++ compiler are suitably
# defined.  These variables are subsequently used by _LT_CONFIG to write
# the compiler configuration to 'libtool'.
m4_defun([_LT_LANG_CXX_CONFIG],
[m4_require([_LT_FILEUTILS_DEFAULTS])dnl
m4_require([_LT_DECL_EGREP])dnl
m4_require([_LT_PATH_MANIFEST_TOOL])dnl
if test -n "$CXX" && ( test no != "$CXX" &&
    ( (test g++ = "$CXX" && `g++ -v >/dev/null 2>&1` ) ||
    (test g++ != "$CXX"))); then
  AC_PROG_CXXCPP
else
  _lt_caught_CXX_error=yes
fi

AC_LANG_PUSH(C++)
_LT_TAGVAR(archive_cmds_need_lc, $1)=no
_LT_TAGVAR(allow_undefined_flag, $1)=
_LT_TAGVAR(always_export_symbols, $1)=no
_LT_TAGVAR(archive_expsym_cmds, $1)=
_LT_TAGVAR(compiler_needs_object, $1)=no
_LT_TAGVAR(export_dynamic_flag_spec, $1)=
_LT_TAGVAR(hardcode_direct, $1)=no
_LT_TAGVAR(hardcode_direct_absolute, $1)=no
_LT_TAGVAR(hardcode_libdir_flag_spec, $1)=
_LT_TAGVAR(hardcode_libdir_separator, $1)=
_LT_TAGVAR(hardcode_minus_L, $1)=no
_LT_TAGVAR(hardcode_shlibpath_var, $1)=unsupported
_LT_TAGVAR(hardcode_automatic, $1)=no
_LT_TAGVAR(inherit_rpath, $1)=no
_LT_TAGVAR(module_cmds, $1)=
_LT_TAGVAR(module_expsym_cmds, $1)=
_LT_TAGVAR(link_all_deplibs, $1)=unknown
_LT_TAGVAR(old_archive_cmds, $1)=$old_archive_cmds
_LT_TAGVAR(reload_flag, $1)=$reload_flag
_LT_TAGVAR(reload_cmds, $1)=$reload_cmds
_LT_TAGVAR(no_undefined_flag, $1)=
_LT_TAGVAR(whole_archive_flag_spec, $1)=
_LT_TAGVAR(enable_shared_with_static_runtimes, $1)=no

# Source file extension for C++ test sources.
ac_ext=cpp

# Object file extension for compiled C++ test sources.
objext=o
_LT_TAGVAR(objext, $1)=$objext

# No sense in running all these tests if we already determined that
# the CXX compiler isn't working.  Some variables (like enable_shared)
# are currently assumed to apply to all compilers on this platform,
# and will be corrupted by setting them based on a non-working compiler.
if test yes != "$_lt_caught_CXX_error"; then
  # Code to be used in simple compile tests
  lt_simple_compile_test_code="int some_variable = 0;"

  # Code to be used in simple link tests
  lt_simple_link_test_code='int main(int, char *[[]]) { return(0); }'

  # ltmain only uses $CC for tagged configurations so make sure $CC is set.
  _LT_TAG_COMPILER

  # save warnings/boilerplate of simple test code
  _LT_COMPILER_BOILERPLATE
  _LT_LINKER_BOILERPLATE

  # Allow CC to be a program name with arguments.
  lt_save_CC=$CC
  lt_save_CFLAGS=$CFLAGS
  lt_save_LD=$LD
  lt_save_GCC=$GCC
  GCC=$GXX
  lt_save_with_gnu_ld=$with_gnu_ld
  lt_save_path_LD=$lt_cv_path_LD
  if test -n "${lt_cv_prog_gnu_ldcxx+set}"; then
    lt_cv_prog_gnu_ld=$lt_cv_prog_gnu_ldcxx
  else
    $as_unset lt_cv_prog_gnu_ld
  fi
  if test -n "${lt_cv_path_LDCXX+set}"; then
    lt_cv_path_LD=$lt_cv_path_LDCXX
  else
    $as_unset lt_cv_path_LD
  fi
  test -z "${LDCXX+set}" || LD=$LDCXX
  CC=${CXX-"c++"}
  CFLAGS=$CXXFLAGS
  compiler=$CC
  _LT_TAGVAR(compiler, $1)=$CC
  _LT_CC_BASENAME([$compiler])

  if test -n "$compiler"; then
    # We don't want -fno-exception when compiling C++ code, so set the
    # no_builtin_flag separately
    if test yes = "$GXX"; then
      _LT_TAGVAR(lt_prog_compiler_no_builtin_flag, $1)=' -fno-builtin'
    else
      _LT_TAGVAR(lt_prog_compiler_no_builtin_flag, $1)=
    fi

    if test yes = "$GXX"; then
      # Set up default GNU C++ configuration

      LT_PATH_LD

      # Check if GNU C++ uses GNU ld as the underlying linker, since the
      # archiving commands below assume that GNU ld is being used.
      if test yes = "$with_gnu_ld"; then
        _LT_TAGVAR(archive_cmds, $1)='$CC $pic_flag -shared -nostdlib $predep_objects $libobjs $deplibs $postdep_objects $compiler_flags $wl-soname $wl$soname -o $lib'
        _LT_TAGVAR(archive_expsym_cmds, $1)='$CC $pic_flag -shared -nostdlib $predep_objects $libobjs $deplibs $postdep_objects $compiler_flags $wl-soname $wl$soname $wl-retain-symbols-file $wl$export_symbols -o $lib'

        _LT_TAGVAR(hardcode_libdir_flag_spec, $1)='$wl-rpath $wl$libdir'
        _LT_TAGVAR(export_dynamic_flag_spec, $1)='$wl--export-dynamic'

        # If archive_cmds runs LD, not CC, wlarc should be empty
        # XXX I think wlarc can be eliminated in ltcf-cxx, but I need to
        #     investigate it a little bit more. (MM)
        wlarc='$wl'

        # ancient GNU ld didn't support --whole-archive et. al.
        if eval "`$CC -print-prog-name=ld` --help 2>&1" |
	  $GREP 'no-whole-archive' > /dev/null; then
          _LT_TAGVAR(whole_archive_flag_spec, $1)=$wlarc'--whole-archive$convenience '$wlarc'--no-whole-archive'
        else
          _LT_TAGVAR(whole_archive_flag_spec, $1)=
        fi
      else
        with_gnu_ld=no
        wlarc=

        # A generic and very simple default shared library creation
        # command for GNU C++ for the case where it uses the native
        # linker, instead of GNU ld.  If possible, this setting should
        # overridden to take advantage of the native linker features on
        # the platform it is being used on.
        _LT_TAGVAR(archive_cmds, $1)='$CC -shared -nostdlib $predep_objects $libobjs $deplibs $postdep_objects $compiler_flags -o $lib'
      fi

      # Commands to make compiler produce verbose output that lists
      # what "hidden" libraries, object files and flags are used when
      # linking a shared library.
      output_verbose_link_cmd='$CC -shared $CFLAGS -v conftest.$objext 2>&1 | $GREP -v "^Configured with:" | $GREP "\-L"'

    else
      GXX=no
      with_gnu_ld=no
      wlarc=
    fi

    # PORTME: fill in a description of your system's C++ link characteristics
    AC_MSG_CHECKING([whether the $compiler linker ($LD) supports shared libraries])
    _LT_TAGVAR(ld_shlibs, $1)=yes
    case $host_os in
      aix3*)
        # FIXME: insert proper C++ library support
        _LT_TAGVAR(ld_shlibs, $1)=no
        ;;
      aix[[4-9]]*)
        if test ia64 = "$host_cpu"; then
          # On IA64, the linker does run time linking by default, so we don't
          # have to do anything special.
          aix_use_runtimelinking=no
          exp_sym_flag='-Bexport'
          no_entry_flag=
        else
          aix_use_runtimelinking=no

          # Test if we are trying to use run time linking or normal
          # AIX style linking. If -brtl is somewhere in LDFLAGS, we
          # have runtime linking enabled, and use it for executables.
          # For shared libraries, we enable/disable runtime linking
          # depending on the kind of the shared library created -
          # when "with_aix_soname,aix_use_runtimelinking" is:
          # "aix,no"   lib.a(lib.so.V) shared, rtl:no,  for executables
          # "aix,yes"  lib.so          shared, rtl:yes, for executables
          #            lib.a           static archive
          # "both,no"  lib.so.V(shr.o) shared, rtl:yes
          #            lib.a(lib.so.V) shared, rtl:no,  for executables
          # "both,yes" lib.so.V(shr.o) shared, rtl:yes, for executables
          #            lib.a(lib.so.V) shared, rtl:no
          # "svr4,*"   lib.so.V(shr.o) shared, rtl:yes, for executables
          #            lib.a           static archive
          case $host_os in aix4.[[23]]|aix4.[[23]].*|aix[[5-9]]*)
	    for ld_flag in $LDFLAGS; do
	      case $ld_flag in
	      *-brtl*)
	        aix_use_runtimelinking=yes
	        break
	        ;;
	      esac
	    done
	    if test svr4,no = "$with_aix_soname,$aix_use_runtimelinking"; then
	      # With aix-soname=svr4, we create the lib.so.V shared archives only,
	      # so we don't have lib.a shared libs to link our executables.
	      # We have to force runtime linking in this case.
	      aix_use_runtimelinking=yes
	      LDFLAGS="$LDFLAGS -Wl,-brtl"
	    fi
	    ;;
          esac

          exp_sym_flag='-bexport'
          no_entry_flag='-bnoentry'
        fi

        # When large executables or shared objects are built, AIX ld can
        # have problems creating the table of contents.  If linking a library
        # or program results in "error TOC overflow" add -mminimal-toc to
        # CXXFLAGS/CFLAGS for g++/gcc.  In the cases where that is not
        # enough to fix the problem, add -Wl,-bbigtoc to LDFLAGS.

        _LT_TAGVAR(archive_cmds, $1)=''
        _LT_TAGVAR(hardcode_direct, $1)=yes
        _LT_TAGVAR(hardcode_direct_absolute, $1)=yes
        _LT_TAGVAR(hardcode_libdir_separator, $1)=':'
        _LT_TAGVAR(link_all_deplibs, $1)=yes
        _LT_TAGVAR(file_list_spec, $1)='$wl-f,'
        case $with_aix_soname,$aix_use_runtimelinking in
        aix,*) ;;	# no import file
        svr4,* | *,yes) # use import file
          # The Import File defines what to hardcode.
          _LT_TAGVAR(hardcode_direct, $1)=no
          _LT_TAGVAR(hardcode_direct_absolute, $1)=no
          ;;
        esac

        if test yes = "$GXX"; then
          case $host_os in aix4.[[012]]|aix4.[[012]].*)
          # We only want to do this on AIX 4.2 and lower, the check
          # below for broken collect2 doesn't work under 4.3+
	  collect2name=`$CC -print-prog-name=collect2`
	  if test -f "$collect2name" &&
	     strings "$collect2name" | $GREP resolve_lib_name >/dev/null
	  then
	    # We have reworked collect2
	    :
	  else
	    # We have old collect2
	    _LT_TAGVAR(hardcode_direct, $1)=unsupported
	    # It fails to find uninstalled libraries when the uninstalled
	    # path is not listed in the libpath.  Setting hardcode_minus_L
	    # to unsupported forces relinking
	    _LT_TAGVAR(hardcode_minus_L, $1)=yes
	    _LT_TAGVAR(hardcode_libdir_flag_spec, $1)='-L$libdir'
	    _LT_TAGVAR(hardcode_libdir_separator, $1)=
	  fi
          esac
          shared_flag='-shared'
	  if test yes = "$aix_use_runtimelinking"; then
	    shared_flag=$shared_flag' $wl-G'
	  fi
	  # Need to ensure runtime linking is disabled for the traditional
	  # shared library, or the linker may eventually find shared libraries
	  # /with/ Import File - we do not want to mix them.
	  shared_flag_aix='-shared'
	  shared_flag_svr4='-shared $wl-G'
        else
          # not using gcc
          if test ia64 = "$host_cpu"; then
	  # VisualAge C++, Version 5.5 for AIX 5L for IA-64, Beta 3 Release
	  # chokes on -Wl,-G. The following line is correct:
	  shared_flag='-G'
          else
	    if test yes = "$aix_use_runtimelinking"; then
	      shared_flag='$wl-G'
	    else
	      shared_flag='$wl-bM:SRE'
	    fi
	    shared_flag_aix='$wl-bM:SRE'
	    shared_flag_svr4='$wl-G'
          fi
        fi

        _LT_TAGVAR(export_dynamic_flag_spec, $1)='$wl-bexpall'
        # It seems that -bexpall does not export symbols beginning with
        # underscore (_), so it is better to generate a list of symbols to
	# export.
        _LT_TAGVAR(always_export_symbols, $1)=yes
	if test aix,yes = "$with_aix_soname,$aix_use_runtimelinking"; then
          # Warning - without using the other runtime loading flags (-brtl),
          # -berok will link without error, but may produce a broken library.
          # The "-G" linker flag allows undefined symbols.
          _LT_TAGVAR(no_undefined_flag, $1)='-bernotok'
          # Determine the default libpath from the value encoded in an empty
          # executable.
          _LT_SYS_MODULE_PATH_AIX([$1])
          _LT_TAGVAR(hardcode_libdir_flag_spec, $1)='$wl-blibpath:$libdir:'"$aix_libpath"

          _LT_TAGVAR(archive_expsym_cmds, $1)='$CC -o $output_objdir/$soname $libobjs $deplibs $wl'$no_entry_flag' $compiler_flags `if test -n "$allow_undefined_flag"; then func_echo_all "$wl$allow_undefined_flag"; else :; fi` $wl'$exp_sym_flag:\$export_symbols' '$shared_flag
        else
          if test ia64 = "$host_cpu"; then
	    _LT_TAGVAR(hardcode_libdir_flag_spec, $1)='$wl-R $libdir:/usr/lib:/lib'
	    _LT_TAGVAR(allow_undefined_flag, $1)="-z nodefs"
	    _LT_TAGVAR(archive_expsym_cmds, $1)="\$CC $shared_flag"' -o $output_objdir/$soname $libobjs $deplibs '"\$wl$no_entry_flag"' $compiler_flags $wl$allow_undefined_flag '"\$wl$exp_sym_flag:\$export_symbols"
          else
	    # Determine the default libpath from the value encoded in an
	    # empty executable.
	    _LT_SYS_MODULE_PATH_AIX([$1])
	    _LT_TAGVAR(hardcode_libdir_flag_spec, $1)='$wl-blibpath:$libdir:'"$aix_libpath"
	    # Warning - without using the other run time loading flags,
	    # -berok will link without error, but may produce a broken library.
	    _LT_TAGVAR(no_undefined_flag, $1)=' $wl-bernotok'
	    _LT_TAGVAR(allow_undefined_flag, $1)=' $wl-berok'
	    if test yes = "$with_gnu_ld"; then
	      # We only use this code for GNU lds that support --whole-archive.
	      _LT_TAGVAR(whole_archive_flag_spec, $1)='$wl--whole-archive$convenience $wl--no-whole-archive'
	    else
	      # Exported symbols can be pulled into shared objects from archives
	      _LT_TAGVAR(whole_archive_flag_spec, $1)='$convenience'
	    fi
	    _LT_TAGVAR(archive_cmds_need_lc, $1)=yes
	    _LT_TAGVAR(archive_expsym_cmds, $1)='$RM -r $output_objdir/$realname.d~$MKDIR $output_objdir/$realname.d'
	    # -brtl affects multiple linker settings, -berok does not and is overridden later
	    compiler_flags_filtered='`func_echo_all "$compiler_flags " | $SED -e "s%-brtl\\([[, ]]\\)%-berok\\1%g"`'
	    if test svr4 != "$with_aix_soname"; then
	      # This is similar to how AIX traditionally builds its shared
	      # libraries. Need -bnortl late, we may have -brtl in LDFLAGS.
	      _LT_TAGVAR(archive_expsym_cmds, $1)="$_LT_TAGVAR(archive_expsym_cmds, $1)"'~$CC '$shared_flag_aix' -o $output_objdir/$realname.d/$soname $libobjs $deplibs $wl-bnoentry '$compiler_flags_filtered'$wl-bE:$export_symbols$allow_undefined_flag~$AR $AR_FLAGS $output_objdir/$libname$release.a $output_objdir/$realname.d/$soname'
	    fi
	    if test aix != "$with_aix_soname"; then
	      _LT_TAGVAR(archive_expsym_cmds, $1)="$_LT_TAGVAR(archive_expsym_cmds, $1)"'~$CC '$shared_flag_svr4' -o $output_objdir/$realname.d/$shared_archive_member_spec.o $libobjs $deplibs $wl-bnoentry '$compiler_flags_filtered'$wl-bE:$export_symbols$allow_undefined_flag~$STRIP -e $output_objdir/$realname.d/$shared_archive_member_spec.o~( func_echo_all "#! $soname($shared_archive_member_spec.o)"; if test shr_64 = "$shared_archive_member_spec"; then func_echo_all "# 64"; else func_echo_all "# 32"; fi; cat $export_symbols ) > $output_objdir/$realname.d/$shared_archive_member_spec.imp~$AR $AR_FLAGS $output_objdir/$soname $output_objdir/$realname.d/$shared_archive_member_spec.o $output_objdir/$realname.d/$shared_archive_member_spec.imp'
	    else
	      # used by -dlpreopen to get the symbols
	      _LT_TAGVAR(archive_expsym_cmds, $1)="$_LT_TAGVAR(archive_expsym_cmds, $1)"'~$MV  $output_objdir/$realname.d/$soname $output_objdir'
	    fi
	    _LT_TAGVAR(archive_expsym_cmds, $1)="$_LT_TAGVAR(archive_expsym_cmds, $1)"'~$RM -r $output_objdir/$realname.d'
          fi
        fi
        ;;

      beos*)
	if $LD --help 2>&1 | $GREP ': supported targets:.* elf' > /dev/null; then
	  _LT_TAGVAR(allow_undefined_flag, $1)=unsupported
	  # Joseph Beckenbach <jrb3@best.com> says some releases of gcc
	  # support --undefined.  This deserves some investigation.  FIXME
	  _LT_TAGVAR(archive_cmds, $1)='$CC -nostart $libobjs $deplibs $compiler_flags $wl-soname $wl$soname -o $lib'
	else
	  _LT_TAGVAR(ld_shlibs, $1)=no
	fi
	;;

      chorus*)
        case $cc_basename in
          *)
	  # FIXME: insert proper C++ library support
	  _LT_TAGVAR(ld_shlibs, $1)=no
	  ;;
        esac
        ;;

      cygwin* | mingw* | pw32* | cegcc*)
	case $GXX,$cc_basename in
	,cl* | no,cl*)
	  # Native MSVC
	  # hardcode_libdir_flag_spec is actually meaningless, as there is
	  # no search path for DLLs.
	  _LT_TAGVAR(hardcode_libdir_flag_spec, $1)=' '
	  _LT_TAGVAR(allow_undefined_flag, $1)=unsupported
	  _LT_TAGVAR(always_export_symbols, $1)=yes
	  _LT_TAGVAR(file_list_spec, $1)='@'
	  # Tell ltmain to make .lib files, not .a files.
	  libext=lib
	  # Tell ltmain to make .dll files, not .so files.
	  shrext_cmds=.dll
	  # FIXME: Setting linknames here is a bad hack.
	  _LT_TAGVAR(archive_cmds, $1)='$CC -o $output_objdir/$soname $libobjs $compiler_flags $deplibs -Wl,-DLL,-IMPLIB:"$tool_output_objdir$libname.dll.lib"~linknames='
	  _LT_TAGVAR(archive_expsym_cmds, $1)='if _LT_DLL_DEF_P([$export_symbols]); then
              cp "$export_symbols" "$output_objdir/$soname.def";
              echo "$tool_output_objdir$soname.def" > "$output_objdir/$soname.exp";
            else
              $SED -e '\''s/^/-link -EXPORT:/'\'' < $export_symbols > $output_objdir/$soname.exp;
            fi~
            $CC -o $tool_output_objdir$soname $libobjs $compiler_flags $deplibs "@$tool_output_objdir$soname.exp" -Wl,-DLL,-IMPLIB:"$tool_output_objdir$libname.dll.lib"~
            linknames='
	  # The linker will not automatically build a static lib if we build a DLL.
	  # _LT_TAGVAR(old_archive_from_new_cmds, $1)='true'
	  _LT_TAGVAR(enable_shared_with_static_runtimes, $1)=yes
	  # Don't use ranlib
	  _LT_TAGVAR(old_postinstall_cmds, $1)='chmod 644 $oldlib'
	  _LT_TAGVAR(postlink_cmds, $1)='lt_outputfile="@OUTPUT@"~
            lt_tool_outputfile="@TOOL_OUTPUT@"~
            case $lt_outputfile in
              *.exe|*.EXE) ;;
              *)
                lt_outputfile=$lt_outputfile.exe
                lt_tool_outputfile=$lt_tool_outputfile.exe
                ;;
            esac~
            func_to_tool_file "$lt_outputfile"~
            if test : != "$MANIFEST_TOOL" && test -f "$lt_outputfile.manifest"; then
              $MANIFEST_TOOL -manifest "$lt_tool_outputfile.manifest" -outputresource:"$lt_tool_outputfile" || exit 1;
              $RM "$lt_outputfile.manifest";
            fi'
	  ;;
	*)
	  # g++
	  # _LT_TAGVAR(hardcode_libdir_flag_spec, $1) is actually meaningless,
	  # as there is no search path for DLLs.
	  _LT_TAGVAR(hardcode_libdir_flag_spec, $1)='-L$libdir'
	  _LT_TAGVAR(export_dynamic_flag_spec, $1)='$wl--export-all-symbols'
	  _LT_TAGVAR(allow_undefined_flag, $1)=unsupported
	  _LT_TAGVAR(always_export_symbols, $1)=no
	  _LT_TAGVAR(enable_shared_with_static_runtimes, $1)=yes

	  if $LD --help 2>&1 | $GREP 'auto-import' > /dev/null; then
	    _LT_TAGVAR(archive_cmds, $1)='$CC -shared -nostdlib $predep_objects $libobjs $deplibs $postdep_objects $compiler_flags -o $output_objdir/$soname $wl--enable-auto-image-base -Xlinker --out-implib -Xlinker $lib'
	    # If the export-symbols file already is a .def file, use it as
	    # is; otherwise, prepend EXPORTS...
	    _LT_TAGVAR(archive_expsym_cmds, $1)='if _LT_DLL_DEF_P([$export_symbols]); then
              cp $export_symbols $output_objdir/$soname.def;
            else
              echo EXPORTS > $output_objdir/$soname.def;
              cat $export_symbols >> $output_objdir/$soname.def;
            fi~
            $CC -shared -nostdlib $output_objdir/$soname.def $predep_objects $libobjs $deplibs $postdep_objects $compiler_flags -o $output_objdir/$soname $wl--enable-auto-image-base -Xlinker --out-implib -Xlinker $lib'
	  else
	    _LT_TAGVAR(ld_shlibs, $1)=no
	  fi
	  ;;
	esac
	;;
      darwin* | rhapsody*)
        _LT_DARWIN_LINKER_FEATURES($1)
	;;

      os2*)
	_LT_TAGVAR(hardcode_libdir_flag_spec, $1)='-L$libdir'
	_LT_TAGVAR(hardcode_minus_L, $1)=yes
	_LT_TAGVAR(allow_undefined_flag, $1)=unsupported
	shrext_cmds=.dll
	_LT_TAGVAR(archive_cmds, $1)='$ECHO "LIBRARY ${soname%$shared_ext} INITINSTANCE TERMINSTANCE" > $output_objdir/$libname.def~
	  $ECHO "DESCRIPTION \"$libname\"" >> $output_objdir/$libname.def~
	  $ECHO "DATA MULTIPLE NONSHARED" >> $output_objdir/$libname.def~
	  $ECHO EXPORTS >> $output_objdir/$libname.def~
	  emxexp $libobjs | $SED /"_DLL_InitTerm"/d >> $output_objdir/$libname.def~
	  $CC -Zdll -Zcrtdll -o $output_objdir/$soname $libobjs $deplibs $compiler_flags $output_objdir/$libname.def~
	  emximp -o $lib $output_objdir/$libname.def'
	_LT_TAGVAR(archive_expsym_cmds, $1)='$ECHO "LIBRARY ${soname%$shared_ext} INITINSTANCE TERMINSTANCE" > $output_objdir/$libname.def~
	  $ECHO "DESCRIPTION \"$libname\"" >> $output_objdir/$libname.def~
	  $ECHO "DATA MULTIPLE NONSHARED" >> $output_objdir/$libname.def~
	  $ECHO EXPORTS >> $output_objdir/$libname.def~
	  prefix_cmds="$SED"~
	  if test EXPORTS = "`$SED 1q $export_symbols`"; then
	    prefix_cmds="$prefix_cmds -e 1d";
	  fi~
	  prefix_cmds="$prefix_cmds -e \"s/^\(.*\)$/_\1/g\""~
	  cat $export_symbols | $prefix_cmds >> $output_objdir/$libname.def~
	  $CC -Zdll -Zcrtdll -o $output_objdir/$soname $libobjs $deplibs $compiler_flags $output_objdir/$libname.def~
	  emximp -o $lib $output_objdir/$libname.def'
	_LT_TAGVAR(old_archive_From_new_cmds, $1)='emximp -o $output_objdir/${libname}_dll.a $output_objdir/$libname.def'
	_LT_TAGVAR(enable_shared_with_static_runtimes, $1)=yes
	;;

      dgux*)
        case $cc_basename in
          ec++*)
	    # FIXME: insert proper C++ library support
	    _LT_TAGVAR(ld_shlibs, $1)=no
	    ;;
          ghcx*)
	    # Green Hills C++ Compiler
	    # FIXME: insert proper C++ library support
	    _LT_TAGVAR(ld_shlibs, $1)=no
	    ;;
          *)
	    # FIXME: insert proper C++ library support
	    _LT_TAGVAR(ld_shlibs, $1)=no
	    ;;
        esac
        ;;

      freebsd2.*)
        # C++ shared libraries reported to be fairly broken before
	# switch to ELF
        _LT_TAGVAR(ld_shlibs, $1)=no
        ;;

      freebsd-elf*)
        _LT_TAGVAR(archive_cmds_need_lc, $1)=no
        ;;

      freebsd* | dragonfly*)
        # FreeBSD 3 and later use GNU C++ and GNU ld with standard ELF
        # conventions
        _LT_TAGVAR(ld_shlibs, $1)=yes
        ;;

      haiku*)
        _LT_TAGVAR(archive_cmds, $1)='$CC -shared $libobjs $deplibs $compiler_flags $wl-soname $wl$soname -o $lib'
        _LT_TAGVAR(link_all_deplibs, $1)=yes
        ;;

      hpux9*)
        _LT_TAGVAR(hardcode_libdir_flag_spec, $1)='$wl+b $wl$libdir'
        _LT_TAGVAR(hardcode_libdir_separator, $1)=:
        _LT_TAGVAR(export_dynamic_flag_spec, $1)='$wl-E'
        _LT_TAGVAR(hardcode_direct, $1)=yes
        _LT_TAGVAR(hardcode_minus_L, $1)=yes # Not in the search PATH,
				             # but as the default
				             # location of the library.

        case $cc_basename in
          CC*)
            # FIXME: insert proper C++ library support
            _LT_TAGVAR(ld_shlibs, $1)=no
            ;;
          aCC*)
            _LT_TAGVAR(archive_cmds, $1)='$RM $output_objdir/$soname~$CC -b $wl+b $wl$install_libdir -o $output_objdir/$soname $predep_objects $libobjs $deplibs $postdep_objects $compiler_flags~test "x$output_objdir/$soname" = "x$lib" || mv $output_objdir/$soname $lib'
            # Commands to make compiler produce verbose output that lists
            # what "hidden" libraries, object files and flags are used when
            # linking a shared library.
            #
            # There doesn't appear to be a way to prevent this compiler from
            # explicitly linking system object files so we need to strip them
            # from the output so that they don't get included in the library
            # dependencies.
            output_verbose_link_cmd='templist=`($CC -b $CFLAGS -v conftest.$objext 2>&1) | $EGREP "\-L"`; list= ; for z in $templist; do case $z in conftest.$objext) list="$list $z";; *.$objext);; *) list="$list $z";;esac; done; func_echo_all "$list"'
            ;;
          *)
            if test yes = "$GXX"; then
              _LT_TAGVAR(archive_cmds, $1)='$RM $output_objdir/$soname~$CC -shared -nostdlib $pic_flag $wl+b $wl$install_libdir -o $output_objdir/$soname $predep_objects $libobjs $deplibs $postdep_objects $compiler_flags~test "x$output_objdir/$soname" = "x$lib" || mv $output_objdir/$soname $lib'
            else
              # FIXME: insert proper C++ library support
              _LT_TAGVAR(ld_shlibs, $1)=no
            fi
            ;;
        esac
        ;;

      hpux10*|hpux11*)
        if test no = "$with_gnu_ld"; then
	  _LT_TAGVAR(hardcode_libdir_flag_spec, $1)='$wl+b $wl$libdir'
	  _LT_TAGVAR(hardcode_libdir_separator, $1)=:

          case $host_cpu in
            hppa*64*|ia64*)
              ;;
            *)
	      _LT_TAGVAR(export_dynamic_flag_spec, $1)='$wl-E'
              ;;
          esac
        fi
        case $host_cpu in
          hppa*64*|ia64*)
            _LT_TAGVAR(hardcode_direct, $1)=no
            _LT_TAGVAR(hardcode_shlibpath_var, $1)=no
            ;;
          *)
            _LT_TAGVAR(hardcode_direct, $1)=yes
            _LT_TAGVAR(hardcode_direct_absolute, $1)=yes
            _LT_TAGVAR(hardcode_minus_L, $1)=yes # Not in the search PATH,
					         # but as the default
					         # location of the library.
            ;;
        esac

        case $cc_basename in
          CC*)
	    # FIXME: insert proper C++ library support
	    _LT_TAGVAR(ld_shlibs, $1)=no
	    ;;
          aCC*)
	    case $host_cpu in
	      hppa*64*)
	        _LT_TAGVAR(archive_cmds, $1)='$CC -b $wl+h $wl$soname -o $lib $predep_objects $libobjs $deplibs $postdep_objects $compiler_flags'
	        ;;
	      ia64*)
	        _LT_TAGVAR(archive_cmds, $1)='$CC -b $wl+h $wl$soname $wl+nodefaultrpath -o $lib $predep_objects $libobjs $deplibs $postdep_objects $compiler_flags'
	        ;;
	      *)
	        _LT_TAGVAR(archive_cmds, $1)='$CC -b $wl+h $wl$soname $wl+b $wl$install_libdir -o $lib $predep_objects $libobjs $deplibs $postdep_objects $compiler_flags'
	        ;;
	    esac
	    # Commands to make compiler produce verbose output that lists
	    # what "hidden" libraries, object files and flags are used when
	    # linking a shared library.
	    #
	    # There doesn't appear to be a way to prevent this compiler from
	    # explicitly linking system object files so we need to strip them
	    # from the output so that they don't get included in the library
	    # dependencies.
	    output_verbose_link_cmd='templist=`($CC -b $CFLAGS -v conftest.$objext 2>&1) | $GREP "\-L"`; list= ; for z in $templist; do case $z in conftest.$objext) list="$list $z";; *.$objext);; *) list="$list $z";;esac; done; func_echo_all "$list"'
	    ;;
          *)
	    if test yes = "$GXX"; then
	      if test no = "$with_gnu_ld"; then
	        case $host_cpu in
	          hppa*64*)
	            _LT_TAGVAR(archive_cmds, $1)='$CC -shared -nostdlib -fPIC $wl+h $wl$soname -o $lib $predep_objects $libobjs $deplibs $postdep_objects $compiler_flags'
	            ;;
	          ia64*)
	            _LT_TAGVAR(archive_cmds, $1)='$CC -shared -nostdlib $pic_flag $wl+h $wl$soname $wl+nodefaultrpath -o $lib $predep_objects $libobjs $deplibs $postdep_objects $compiler_flags'
	            ;;
	          *)
	            _LT_TAGVAR(archive_cmds, $1)='$CC -shared -nostdlib $pic_flag $wl+h $wl$soname $wl+b $wl$install_libdir -o $lib $predep_objects $libobjs $deplibs $postdep_objects $compiler_flags'
	            ;;
	        esac
	      fi
	    else
	      # FIXME: insert proper C++ library support
	      _LT_TAGVAR(ld_shlibs, $1)=no
	    fi
	    ;;
        esac
        ;;

      interix[[3-9]]*)
	_LT_TAGVAR(hardcode_direct, $1)=no
	_LT_TAGVAR(hardcode_shlibpath_var, $1)=no
	_LT_TAGVAR(hardcode_libdir_flag_spec, $1)='$wl-rpath,$libdir'
	_LT_TAGVAR(export_dynamic_flag_spec, $1)='$wl-E'
	# Hack: On Interix 3.x, we cannot compile PIC because of a broken gcc.
	# Instead, shared libraries are loaded at an image base (0x10000000 by
	# default) and relocated if they conflict, which is a slow very memory
	# consuming and fragmenting process.  To avoid this, we pick a random,
	# 256 KiB-aligned image base between 0x50000000 and 0x6FFC0000 at link
	# time.  Moving up from 0x10000000 also allows more sbrk(2) space.
	_LT_TAGVAR(archive_cmds, $1)='$CC -shared $pic_flag $libobjs $deplibs $compiler_flags $wl-h,$soname $wl--image-base,`expr ${RANDOM-$$} % 4096 / 2 \* 262144 + 1342177280` -o $lib'
	_LT_TAGVAR(archive_expsym_cmds, $1)='sed "s|^|_|" $export_symbols >$output_objdir/$soname.expsym~$CC -shared $pic_flag $libobjs $deplibs $compiler_flags $wl-h,$soname $wl--retain-symbols-file,$output_objdir/$soname.expsym $wl--image-base,`expr ${RANDOM-$$} % 4096 / 2 \* 262144 + 1342177280` -o $lib'
	;;
      irix5* | irix6*)
        case $cc_basename in
          CC*)
	    # SGI C++
	    _LT_TAGVAR(archive_cmds, $1)='$CC -shared -all -multigot $predep_objects $libobjs $deplibs $postdep_objects $compiler_flags -soname $soname `test -n "$verstring" && func_echo_all "-set_version $verstring"` -update_registry $output_objdir/so_locations -o $lib'

	    # Archives containing C++ object files must be created using
	    # "CC -ar", where "CC" is the IRIX C++ compiler.  This is
	    # necessary to make sure instantiated templates are included
	    # in the archive.
	    _LT_TAGVAR(old_archive_cmds, $1)='$CC -ar -WR,-u -o $oldlib $oldobjs'
	    ;;
          *)
	    if test yes = "$GXX"; then
	      if test no = "$with_gnu_ld"; then
	        _LT_TAGVAR(archive_cmds, $1)='$CC -shared $pic_flag -nostdlib $predep_objects $libobjs $deplibs $postdep_objects $compiler_flags $wl-soname $wl$soname `test -n "$verstring" && func_echo_all "$wl-set_version $wl$verstring"` $wl-update_registry $wl$output_objdir/so_locations -o $lib'
	      else
	        _LT_TAGVAR(archive_cmds, $1)='$CC -shared $pic_flag -nostdlib $predep_objects $libobjs $deplibs $postdep_objects $compiler_flags $wl-soname $wl$soname `test -n "$verstring" && func_echo_all "$wl-set_version $wl$verstring"` -o $lib'
	      fi
	    fi
	    _LT_TAGVAR(link_all_deplibs, $1)=yes
	    ;;
        esac
        _LT_TAGVAR(hardcode_libdir_flag_spec, $1)='$wl-rpath $wl$libdir'
        _LT_TAGVAR(hardcode_libdir_separator, $1)=:
        _LT_TAGVAR(inherit_rpath, $1)=yes
        ;;

      linux* | k*bsd*-gnu | kopensolaris*-gnu | gnu*)
        case $cc_basename in
          KCC*)
	    # Kuck and Associates, Inc. (KAI) C++ Compiler

	    # KCC will only create a shared library if the output file
	    # ends with ".so" (or ".sl" for HP-UX), so rename the library
	    # to its proper name (with version) after linking.
	    _LT_TAGVAR(archive_cmds, $1)='tempext=`echo $shared_ext | $SED -e '\''s/\([[^()0-9A-Za-z{}]]\)/\\\\\1/g'\''`; templib=`echo $lib | $SED -e "s/\$tempext\..*/.so/"`; $CC $predep_objects $libobjs $deplibs $postdep_objects $compiler_flags --soname $soname -o \$templib; mv \$templib $lib'
	    _LT_TAGVAR(archive_expsym_cmds, $1)='tempext=`echo $shared_ext | $SED -e '\''s/\([[^()0-9A-Za-z{}]]\)/\\\\\1/g'\''`; templib=`echo $lib | $SED -e "s/\$tempext\..*/.so/"`; $CC $predep_objects $libobjs $deplibs $postdep_objects $compiler_flags --soname $soname -o \$templib $wl-retain-symbols-file,$export_symbols; mv \$templib $lib'
	    # Commands to make compiler produce verbose output that lists
	    # what "hidden" libraries, object files and flags are used when
	    # linking a shared library.
	    #
	    # There doesn't appear to be a way to prevent this compiler from
	    # explicitly linking system object files so we need to strip them
	    # from the output so that they don't get included in the library
	    # dependencies.
	    output_verbose_link_cmd='templist=`$CC $CFLAGS -v conftest.$objext -o libconftest$shared_ext 2>&1 | $GREP "ld"`; rm -f libconftest$shared_ext; list= ; for z in $templist; do case $z in conftest.$objext) list="$list $z";; *.$objext);; *) list="$list $z";;esac; done; func_echo_all "$list"'

	    _LT_TAGVAR(hardcode_libdir_flag_spec, $1)='$wl-rpath,$libdir'
	    _LT_TAGVAR(export_dynamic_flag_spec, $1)='$wl--export-dynamic'

	    # Archives containing C++ object files must be created using
	    # "CC -Bstatic", where "CC" is the KAI C++ compiler.
	    _LT_TAGVAR(old_archive_cmds, $1)='$CC -Bstatic -o $oldlib $oldobjs'
	    ;;
	  icpc* | ecpc* )
	    # Intel C++
	    with_gnu_ld=yes
	    # version 8.0 and above of icpc choke on multiply defined symbols
	    # if we add $predep_objects and $postdep_objects, however 7.1 and
	    # earlier do not add the objects themselves.
	    case `$CC -V 2>&1` in
	      *"Version 7."*)
	        _LT_TAGVAR(archive_cmds, $1)='$CC -shared $predep_objects $libobjs $deplibs $postdep_objects $compiler_flags $wl-soname $wl$soname -o $lib'
		_LT_TAGVAR(archive_expsym_cmds, $1)='$CC -shared $predep_objects $libobjs $deplibs $postdep_objects $compiler_flags $wl-soname $wl$soname $wl-retain-symbols-file $wl$export_symbols -o $lib'
		;;
	      *)  # Version 8.0 or newer
	        tmp_idyn=
	        case $host_cpu in
		  ia64*) tmp_idyn=' -i_dynamic';;
		esac
	        _LT_TAGVAR(archive_cmds, $1)='$CC -shared'"$tmp_idyn"' $libobjs $deplibs $compiler_flags $wl-soname $wl$soname -o $lib'
		_LT_TAGVAR(archive_expsym_cmds, $1)='$CC -shared'"$tmp_idyn"' $libobjs $deplibs $compiler_flags $wl-soname $wl$soname $wl-retain-symbols-file $wl$export_symbols -o $lib'
		;;
	    esac
	    _LT_TAGVAR(archive_cmds_need_lc, $1)=no
	    _LT_TAGVAR(hardcode_libdir_flag_spec, $1)='$wl-rpath,$libdir'
	    _LT_TAGVAR(export_dynamic_flag_spec, $1)='$wl--export-dynamic'
	    _LT_TAGVAR(whole_archive_flag_spec, $1)='$wl--whole-archive$convenience $wl--no-whole-archive'
	    ;;
          pgCC* | pgcpp*)
            # Portland Group C++ compiler
	    case `$CC -V` in
	    *pgCC\ [[1-5]].* | *pgcpp\ [[1-5]].*)
	      _LT_TAGVAR(prelink_cmds, $1)='tpldir=Template.dir~
               rm -rf $tpldir~
               $CC --prelink_objects --instantiation_dir $tpldir $objs $libobjs $compile_deplibs~
               compile_command="$compile_command `find $tpldir -name \*.o | sort | $NL2SP`"'
	      _LT_TAGVAR(old_archive_cmds, $1)='tpldir=Template.dir~
                rm -rf $tpldir~
                $CC --prelink_objects --instantiation_dir $tpldir $oldobjs$old_deplibs~
                $AR $AR_FLAGS $oldlib$oldobjs$old_deplibs `find $tpldir -name \*.o | sort | $NL2SP`~
                $RANLIB $oldlib'
	      _LT_TAGVAR(archive_cmds, $1)='tpldir=Template.dir~
                rm -rf $tpldir~
                $CC --prelink_objects --instantiation_dir $tpldir $predep_objects $libobjs $deplibs $convenience $postdep_objects~
                $CC -shared $pic_flag $predep_objects $libobjs $deplibs `find $tpldir -name \*.o | sort | $NL2SP` $postdep_objects $compiler_flags $wl-soname $wl$soname -o $lib'
	      _LT_TAGVAR(archive_expsym_cmds, $1)='tpldir=Template.dir~
                rm -rf $tpldir~
                $CC --prelink_objects --instantiation_dir $tpldir $predep_objects $libobjs $deplibs $convenience $postdep_objects~
                $CC -shared $pic_flag $predep_objects $libobjs $deplibs `find $tpldir -name \*.o | sort | $NL2SP` $postdep_objects $compiler_flags $wl-soname $wl$soname $wl-retain-symbols-file $wl$export_symbols -o $lib'
	      ;;
	    *) # Version 6 and above use weak symbols
	      _LT_TAGVAR(archive_cmds, $1)='$CC -shared $pic_flag $predep_objects $libobjs $deplibs $postdep_objects $compiler_flags $wl-soname $wl$soname -o $lib'
	      _LT_TAGVAR(archive_expsym_cmds, $1)='$CC -shared $pic_flag $predep_objects $libobjs $deplibs $postdep_objects $compiler_flags $wl-soname $wl$soname $wl-retain-symbols-file $wl$export_symbols -o $lib'
	      ;;
	    esac

	    _LT_TAGVAR(hardcode_libdir_flag_spec, $1)='$wl--rpath $wl$libdir'
	    _LT_TAGVAR(export_dynamic_flag_spec, $1)='$wl--export-dynamic'
	    _LT_TAGVAR(whole_archive_flag_spec, $1)='$wl--whole-archive`for conv in $convenience\"\"; do test  -n \"$conv\" && new_convenience=\"$new_convenience,$conv\"; done; func_echo_all \"$new_convenience\"` $wl--no-whole-archive'
            ;;
	  cxx*)
	    # Compaq C++
	    _LT_TAGVAR(archive_cmds, $1)='$CC -shared $predep_objects $libobjs $deplibs $postdep_objects $compiler_flags $wl-soname $wl$soname -o $lib'
	    _LT_TAGVAR(archive_expsym_cmds, $1)='$CC -shared $predep_objects $libobjs $deplibs $postdep_objects $compiler_flags $wl-soname $wl$soname  -o $lib $wl-retain-symbols-file $wl$export_symbols'

	    runpath_var=LD_RUN_PATH
	    _LT_TAGVAR(hardcode_libdir_flag_spec, $1)='-rpath $libdir'
	    _LT_TAGVAR(hardcode_libdir_separator, $1)=:

	    # Commands to make compiler produce verbose output that lists
	    # what "hidden" libraries, object files and flags are used when
	    # linking a shared library.
	    #
	    # There doesn't appear to be a way to prevent this compiler from
	    # explicitly linking system object files so we need to strip them
	    # from the output so that they don't get included in the library
	    # dependencies.
	    output_verbose_link_cmd='templist=`$CC -shared $CFLAGS -v conftest.$objext 2>&1 | $GREP "ld"`; templist=`func_echo_all "$templist" | $SED "s/\(^.*ld.*\)\( .*ld .*$\)/\1/"`; list= ; for z in $templist; do case $z in conftest.$objext) list="$list $z";; *.$objext);; *) list="$list $z";;esac; done; func_echo_all "X$list" | $Xsed'
	    ;;
	  xl* | mpixl* | bgxl*)
	    # IBM XL 8.0 on PPC, with GNU ld
	    _LT_TAGVAR(hardcode_libdir_flag_spec, $1)='$wl-rpath $wl$libdir'
	    _LT_TAGVAR(export_dynamic_flag_spec, $1)='$wl--export-dynamic'
	    _LT_TAGVAR(archive_cmds, $1)='$CC -qmkshrobj $libobjs $deplibs $compiler_flags $wl-soname $wl$soname -o $lib'
	    if test yes = "$supports_anon_versioning"; then
	      _LT_TAGVAR(archive_expsym_cmds, $1)='echo "{ global:" > $output_objdir/$libname.ver~
                cat $export_symbols | sed -e "s/\(.*\)/\1;/" >> $output_objdir/$libname.ver~
                echo "local: *; };" >> $output_objdir/$libname.ver~
                $CC -qmkshrobj $libobjs $deplibs $compiler_flags $wl-soname $wl$soname $wl-version-script $wl$output_objdir/$libname.ver -o $lib'
	    fi
	    ;;
	  *)
	    case `$CC -V 2>&1 | sed 5q` in
	    *Sun\ C*)
	      # Sun C++ 5.9
	      _LT_TAGVAR(no_undefined_flag, $1)=' -zdefs'
	      _LT_TAGVAR(archive_cmds, $1)='$CC -G$allow_undefined_flag -h$soname -o $lib $predep_objects $libobjs $deplibs $postdep_objects $compiler_flags'
	      _LT_TAGVAR(archive_expsym_cmds, $1)='$CC -G$allow_undefined_flag -h$soname -o $lib $predep_objects $libobjs $deplibs $postdep_objects $compiler_flags $wl-retain-symbols-file $wl$export_symbols'
	      _LT_TAGVAR(hardcode_libdir_flag_spec, $1)='-R$libdir'
	      _LT_TAGVAR(whole_archive_flag_spec, $1)='$wl--whole-archive`new_convenience=; for conv in $convenience\"\"; do test -z \"$conv\" || new_convenience=\"$new_convenience,$conv\"; done; func_echo_all \"$new_convenience\"` $wl--no-whole-archive'
	      _LT_TAGVAR(compiler_needs_object, $1)=yes

	      # Not sure whether something based on
	      # $CC $CFLAGS -v conftest.$objext -o libconftest$shared_ext 2>&1
	      # would be better.
	      output_verbose_link_cmd='func_echo_all'

	      # Archives containing C++ object files must be created using
	      # "CC -xar", where "CC" is the Sun C++ compiler.  This is
	      # necessary to make sure instantiated templates are included
	      # in the archive.
	      _LT_TAGVAR(old_archive_cmds, $1)='$CC -xar -o $oldlib $oldobjs'
	      ;;
	    esac
	    ;;
	esac
	;;

      lynxos*)
        # FIXME: insert proper C++ library support
	_LT_TAGVAR(ld_shlibs, $1)=no
	;;

      m88k*)
        # FIXME: insert proper C++ library support
        _LT_TAGVAR(ld_shlibs, $1)=no
	;;

      mvs*)
        case $cc_basename in
          cxx*)
	    # FIXME: insert proper C++ library support
	    _LT_TAGVAR(ld_shlibs, $1)=no
	    ;;
	  *)
	    # FIXME: insert proper C++ library support
	    _LT_TAGVAR(ld_shlibs, $1)=no
	    ;;
	esac
	;;

      netbsd*)
        if echo __ELF__ | $CC -E - | $GREP __ELF__ >/dev/null; then
	  _LT_TAGVAR(archive_cmds, $1)='$LD -Bshareable  -o $lib $predep_objects $libobjs $deplibs $postdep_objects $linker_flags'
	  wlarc=
	  _LT_TAGVAR(hardcode_libdir_flag_spec, $1)='-R$libdir'
	  _LT_TAGVAR(hardcode_direct, $1)=yes
	  _LT_TAGVAR(hardcode_shlibpath_var, $1)=no
	fi
	# Workaround some broken pre-1.5 toolchains
	output_verbose_link_cmd='$CC -shared $CFLAGS -v conftest.$objext 2>&1 | $GREP conftest.$objext | $SED -e "s:-lgcc -lc -lgcc::"'
	;;

      *nto* | *qnx*)
        _LT_TAGVAR(ld_shlibs, $1)=yes
	;;

      openbsd* | bitrig*)
	if test -f /usr/libexec/ld.so; then
	  _LT_TAGVAR(hardcode_direct, $1)=yes
	  _LT_TAGVAR(hardcode_shlibpath_var, $1)=no
	  _LT_TAGVAR(hardcode_direct_absolute, $1)=yes
	  _LT_TAGVAR(archive_cmds, $1)='$CC -shared $pic_flag $predep_objects $libobjs $deplibs $postdep_objects $compiler_flags -o $lib'
	  _LT_TAGVAR(hardcode_libdir_flag_spec, $1)='$wl-rpath,$libdir'
	  if test -z "`echo __ELF__ | $CC -E - | grep __ELF__`"; then
	    _LT_TAGVAR(archive_expsym_cmds, $1)='$CC -shared $pic_flag $predep_objects $libobjs $deplibs $postdep_objects $compiler_flags $wl-retain-symbols-file,$export_symbols -o $lib'
	    _LT_TAGVAR(export_dynamic_flag_spec, $1)='$wl-E'
	    _LT_TAGVAR(whole_archive_flag_spec, $1)=$wlarc'--whole-archive$convenience '$wlarc'--no-whole-archive'
	  fi
	  output_verbose_link_cmd=func_echo_all
	else
	  _LT_TAGVAR(ld_shlibs, $1)=no
	fi
	;;

      osf3* | osf4* | osf5*)
        case $cc_basename in
          KCC*)
	    # Kuck and Associates, Inc. (KAI) C++ Compiler

	    # KCC will only create a shared library if the output file
	    # ends with ".so" (or ".sl" for HP-UX), so rename the library
	    # to its proper name (with version) after linking.
	    _LT_TAGVAR(archive_cmds, $1)='tempext=`echo $shared_ext | $SED -e '\''s/\([[^()0-9A-Za-z{}]]\)/\\\\\1/g'\''`; templib=`echo "$lib" | $SED -e "s/\$tempext\..*/.so/"`; $CC $predep_objects $libobjs $deplibs $postdep_objects $compiler_flags --soname $soname -o \$templib; mv \$templib $lib'

	    _LT_TAGVAR(hardcode_libdir_flag_spec, $1)='$wl-rpath,$libdir'
	    _LT_TAGVAR(hardcode_libdir_separator, $1)=:

	    # Archives containing C++ object files must be created using
	    # the KAI C++ compiler.
	    case $host in
	      osf3*) _LT_TAGVAR(old_archive_cmds, $1)='$CC -Bstatic -o $oldlib $oldobjs' ;;
	      *) _LT_TAGVAR(old_archive_cmds, $1)='$CC -o $oldlib $oldobjs' ;;
	    esac
	    ;;
          RCC*)
	    # Rational C++ 2.4.1
	    # FIXME: insert proper C++ library support
	    _LT_TAGVAR(ld_shlibs, $1)=no
	    ;;
          cxx*)
	    case $host in
	      osf3*)
	        _LT_TAGVAR(allow_undefined_flag, $1)=' $wl-expect_unresolved $wl\*'
	        _LT_TAGVAR(archive_cmds, $1)='$CC -shared$allow_undefined_flag $predep_objects $libobjs $deplibs $postdep_objects $compiler_flags $wl-soname $soname `test -n "$verstring" && func_echo_all "$wl-set_version $verstring"` -update_registry $output_objdir/so_locations -o $lib'
	        _LT_TAGVAR(hardcode_libdir_flag_spec, $1)='$wl-rpath $wl$libdir'
		;;
	      *)
	        _LT_TAGVAR(allow_undefined_flag, $1)=' -expect_unresolved \*'
	        _LT_TAGVAR(archive_cmds, $1)='$CC -shared$allow_undefined_flag $predep_objects $libobjs $deplibs $postdep_objects $compiler_flags -msym -soname $soname `test -n "$verstring" && func_echo_all "-set_version $verstring"` -update_registry $output_objdir/so_locations -o $lib'
	        _LT_TAGVAR(archive_expsym_cmds, $1)='for i in `cat $export_symbols`; do printf "%s %s\\n" -exported_symbol "\$i" >> $lib.exp; done~
                  echo "-hidden">> $lib.exp~
                  $CC -shared$allow_undefined_flag $predep_objects $libobjs $deplibs $postdep_objects $compiler_flags -msym -soname $soname $wl-input $wl$lib.exp  `test -n "$verstring" && $ECHO "-set_version $verstring"` -update_registry $output_objdir/so_locations -o $lib~
                  $RM $lib.exp'
	        _LT_TAGVAR(hardcode_libdir_flag_spec, $1)='-rpath $libdir'
		;;
	    esac

	    _LT_TAGVAR(hardcode_libdir_separator, $1)=:

	    # Commands to make compiler produce verbose output that lists
	    # what "hidden" libraries, object files and flags are used when
	    # linking a shared library.
	    #
	    # There doesn't appear to be a way to prevent this compiler from
	    # explicitly linking system object files so we need to strip them
	    # from the output so that they don't get included in the library
	    # dependencies.
	    output_verbose_link_cmd='templist=`$CC -shared $CFLAGS -v conftest.$objext 2>&1 | $GREP "ld" | $GREP -v "ld:"`; templist=`func_echo_all "$templist" | $SED "s/\(^.*ld.*\)\( .*ld.*$\)/\1/"`; list= ; for z in $templist; do case $z in conftest.$objext) list="$list $z";; *.$objext);; *) list="$list $z";;esac; done; func_echo_all "$list"'
	    ;;
	  *)
	    if test yes,no = "$GXX,$with_gnu_ld"; then
	      _LT_TAGVAR(allow_undefined_flag, $1)=' $wl-expect_unresolved $wl\*'
	      case $host in
	        osf3*)
	          _LT_TAGVAR(archive_cmds, $1)='$CC -shared -nostdlib $allow_undefined_flag $predep_objects $libobjs $deplibs $postdep_objects $compiler_flags $wl-soname $wl$soname `test -n "$verstring" && func_echo_all "$wl-set_version $wl$verstring"` $wl-update_registry $wl$output_objdir/so_locations -o $lib'
		  ;;
	        *)
	          _LT_TAGVAR(archive_cmds, $1)='$CC -shared $pic_flag -nostdlib $allow_undefined_flag $predep_objects $libobjs $deplibs $postdep_objects $compiler_flags $wl-msym $wl-soname $wl$soname `test -n "$verstring" && func_echo_all "$wl-set_version $wl$verstring"` $wl-update_registry $wl$output_objdir/so_locations -o $lib'
		  ;;
	      esac

	      _LT_TAGVAR(hardcode_libdir_flag_spec, $1)='$wl-rpath $wl$libdir'
	      _LT_TAGVAR(hardcode_libdir_separator, $1)=:

	      # Commands to make compiler produce verbose output that lists
	      # what "hidden" libraries, object files and flags are used when
	      # linking a shared library.
	      output_verbose_link_cmd='$CC -shared $CFLAGS -v conftest.$objext 2>&1 | $GREP -v "^Configured with:" | $GREP "\-L"'

	    else
	      # FIXME: insert proper C++ library support
	      _LT_TAGVAR(ld_shlibs, $1)=no
	    fi
	    ;;
        esac
        ;;

      psos*)
        # FIXME: insert proper C++ library support
        _LT_TAGVAR(ld_shlibs, $1)=no
        ;;

      sunos4*)
        case $cc_basename in
          CC*)
	    # Sun C++ 4.x
	    # FIXME: insert proper C++ library support
	    _LT_TAGVAR(ld_shlibs, $1)=no
	    ;;
          lcc*)
	    # Lucid
	    # FIXME: insert proper C++ library support
	    _LT_TAGVAR(ld_shlibs, $1)=no
	    ;;
          *)
	    # FIXME: insert proper C++ library support
	    _LT_TAGVAR(ld_shlibs, $1)=no
	    ;;
        esac
        ;;

      solaris*)
        case $cc_basename in
          CC* | sunCC*)
	    # Sun C++ 4.2, 5.x and Centerline C++
            _LT_TAGVAR(archive_cmds_need_lc,$1)=yes
	    _LT_TAGVAR(no_undefined_flag, $1)=' -zdefs'
	    _LT_TAGVAR(archive_cmds, $1)='$CC -G$allow_undefined_flag -h$soname -o $lib $predep_objects $libobjs $deplibs $postdep_objects $compiler_flags'
	    _LT_TAGVAR(archive_expsym_cmds, $1)='echo "{ global:" > $lib.exp~cat $export_symbols | $SED -e "s/\(.*\)/\1;/" >> $lib.exp~echo "local: *; };" >> $lib.exp~
              $CC -G$allow_undefined_flag $wl-M $wl$lib.exp -h$soname -o $lib $predep_objects $libobjs $deplibs $postdep_objects $compiler_flags~$RM $lib.exp'

	    _LT_TAGVAR(hardcode_libdir_flag_spec, $1)='-R$libdir'
	    _LT_TAGVAR(hardcode_shlibpath_var, $1)=no
	    case $host_os in
	      solaris2.[[0-5]] | solaris2.[[0-5]].*) ;;
	      *)
		# The compiler driver will combine and reorder linker options,
		# but understands '-z linker_flag'.
	        # Supported since Solaris 2.6 (maybe 2.5.1?)
		_LT_TAGVAR(whole_archive_flag_spec, $1)='-z allextract$convenience -z defaultextract'
	        ;;
	    esac
	    _LT_TAGVAR(link_all_deplibs, $1)=yes

	    output_verbose_link_cmd='func_echo_all'

	    # Archives containing C++ object files must be created using
	    # "CC -xar", where "CC" is the Sun C++ compiler.  This is
	    # necessary to make sure instantiated templates are included
	    # in the archive.
	    _LT_TAGVAR(old_archive_cmds, $1)='$CC -xar -o $oldlib $oldobjs'
	    ;;
          gcx*)
	    # Green Hills C++ Compiler
	    _LT_TAGVAR(archive_cmds, $1)='$CC -shared $predep_objects $libobjs $deplibs $postdep_objects $compiler_flags $wl-h $wl$soname -o $lib'

	    # The C++ compiler must be used to create the archive.
	    _LT_TAGVAR(old_archive_cmds, $1)='$CC $LDFLAGS -archive -o $oldlib $oldobjs'
	    ;;
          *)
	    # GNU C++ compiler with Solaris linker
	    if test yes,no = "$GXX,$with_gnu_ld"; then
	      _LT_TAGVAR(no_undefined_flag, $1)=' $wl-z ${wl}defs'
	      if $CC --version | $GREP -v '^2\.7' > /dev/null; then
	        _LT_TAGVAR(archive_cmds, $1)='$CC -shared $pic_flag -nostdlib $predep_objects $libobjs $deplibs $postdep_objects $compiler_flags $wl-h $wl$soname -o $lib'
	        _LT_TAGVAR(archive_expsym_cmds, $1)='echo "{ global:" > $lib.exp~cat $export_symbols | $SED -e "s/\(.*\)/\1;/" >> $lib.exp~echo "local: *; };" >> $lib.exp~
                  $CC -shared $pic_flag -nostdlib $wl-M $wl$lib.exp $wl-h $wl$soname -o $lib $predep_objects $libobjs $deplibs $postdep_objects $compiler_flags~$RM $lib.exp'

	        # Commands to make compiler produce verbose output that lists
	        # what "hidden" libraries, object files and flags are used when
	        # linking a shared library.
	        output_verbose_link_cmd='$CC -shared $CFLAGS -v conftest.$objext 2>&1 | $GREP -v "^Configured with:" | $GREP "\-L"'
	      else
	        # g++ 2.7 appears to require '-G' NOT '-shared' on this
	        # platform.
	        _LT_TAGVAR(archive_cmds, $1)='$CC -G -nostdlib $predep_objects $libobjs $deplibs $postdep_objects $compiler_flags $wl-h $wl$soname -o $lib'
	        _LT_TAGVAR(archive_expsym_cmds, $1)='echo "{ global:" > $lib.exp~cat $export_symbols | $SED -e "s/\(.*\)/\1;/" >> $lib.exp~echo "local: *; };" >> $lib.exp~
                  $CC -G -nostdlib $wl-M $wl$lib.exp $wl-h $wl$soname -o $lib $predep_objects $libobjs $deplibs $postdep_objects $compiler_flags~$RM $lib.exp'

	        # Commands to make compiler produce verbose output that lists
	        # what "hidden" libraries, object files and flags are used when
	        # linking a shared library.
	        output_verbose_link_cmd='$CC -G $CFLAGS -v conftest.$objext 2>&1 | $GREP -v "^Configured with:" | $GREP "\-L"'
	      fi

	      _LT_TAGVAR(hardcode_libdir_flag_spec, $1)='$wl-R $wl$libdir'
	      case $host_os in
		solaris2.[[0-5]] | solaris2.[[0-5]].*) ;;
		*)
		  _LT_TAGVAR(whole_archive_flag_spec, $1)='$wl-z ${wl}allextract$convenience $wl-z ${wl}defaultextract'
		  ;;
	      esac
	    fi
	    ;;
        esac
        ;;

    sysv4*uw2* | sysv5OpenUNIX* | sysv5UnixWare7.[[01]].[[10]]* | unixware7* | sco3.2v5.0.[[024]]*)
      _LT_TAGVAR(no_undefined_flag, $1)='$wl-z,text'
      _LT_TAGVAR(archive_cmds_need_lc, $1)=no
      _LT_TAGVAR(hardcode_shlibpath_var, $1)=no
      runpath_var='LD_RUN_PATH'

      case $cc_basename in
        CC*)
	  _LT_TAGVAR(archive_cmds, $1)='$CC -G $wl-h,$soname -o $lib $libobjs $deplibs $compiler_flags'
	  _LT_TAGVAR(archive_expsym_cmds, $1)='$CC -G $wl-Bexport:$export_symbols $wl-h,$soname -o $lib $libobjs $deplibs $compiler_flags'
	  ;;
	*)
	  _LT_TAGVAR(archive_cmds, $1)='$CC -shared $wl-h,$soname -o $lib $libobjs $deplibs $compiler_flags'
	  _LT_TAGVAR(archive_expsym_cmds, $1)='$CC -shared $wl-Bexport:$export_symbols $wl-h,$soname -o $lib $libobjs $deplibs $compiler_flags'
	  ;;
      esac
      ;;

      sysv5* | sco3.2v5* | sco5v6*)
	# Note: We CANNOT use -z defs as we might desire, because we do not
	# link with -lc, and that would cause any symbols used from libc to
	# always be unresolved, which means just about no library would
	# ever link correctly.  If we're not using GNU ld we use -z text
	# though, which does catch some bad symbols but isn't as heavy-handed
	# as -z defs.
	_LT_TAGVAR(no_undefined_flag, $1)='$wl-z,text'
	_LT_TAGVAR(allow_undefined_flag, $1)='$wl-z,nodefs'
	_LT_TAGVAR(archive_cmds_need_lc, $1)=no
	_LT_TAGVAR(hardcode_shlibpath_var, $1)=no
	_LT_TAGVAR(hardcode_libdir_flag_spec, $1)='$wl-R,$libdir'
	_LT_TAGVAR(hardcode_libdir_separator, $1)=':'
	_LT_TAGVAR(link_all_deplibs, $1)=yes
	_LT_TAGVAR(export_dynamic_flag_spec, $1)='$wl-Bexport'
	runpath_var='LD_RUN_PATH'

	case $cc_basename in
          CC*)
	    _LT_TAGVAR(archive_cmds, $1)='$CC -G $wl-h,$soname -o $lib $libobjs $deplibs $compiler_flags'
	    _LT_TAGVAR(archive_expsym_cmds, $1)='$CC -G $wl-Bexport:$export_symbols $wl-h,$soname -o $lib $libobjs $deplibs $compiler_flags'
	    _LT_TAGVAR(old_archive_cmds, $1)='$CC -Tprelink_objects $oldobjs~
              '"$_LT_TAGVAR(old_archive_cmds, $1)"
	    _LT_TAGVAR(reload_cmds, $1)='$CC -Tprelink_objects $reload_objs~
              '"$_LT_TAGVAR(reload_cmds, $1)"
	    ;;
	  *)
	    _LT_TAGVAR(archive_cmds, $1)='$CC -shared $wl-h,$soname -o $lib $libobjs $deplibs $compiler_flags'
	    _LT_TAGVAR(archive_expsym_cmds, $1)='$CC -shared $wl-Bexport:$export_symbols $wl-h,$soname -o $lib $libobjs $deplibs $compiler_flags'
	    ;;
	esac
      ;;

      tandem*)
        case $cc_basename in
          NCC*)
	    # NonStop-UX NCC 3.20
	    # FIXME: insert proper C++ library support
	    _LT_TAGVAR(ld_shlibs, $1)=no
	    ;;
          *)
	    # FIXME: insert proper C++ library support
	    _LT_TAGVAR(ld_shlibs, $1)=no
	    ;;
        esac
        ;;

      vxworks*)
        # FIXME: insert proper C++ library support
        _LT_TAGVAR(ld_shlibs, $1)=no
        ;;

      *)
        # FIXME: insert proper C++ library support
        _LT_TAGVAR(ld_shlibs, $1)=no
        ;;
    esac

    AC_MSG_RESULT([$_LT_TAGVAR(ld_shlibs, $1)])
    test no = "$_LT_TAGVAR(ld_shlibs, $1)" && can_build_shared=no

    _LT_TAGVAR(GCC, $1)=$GXX
    _LT_TAGVAR(LD, $1)=$LD

    ## CAVEAT EMPTOR:
    ## There is no encapsulation within the following macros, do not change
    ## the running order or otherwise move them around unless you know exactly
    ## what you are doing...
    _LT_SYS_HIDDEN_LIBDEPS($1)
    _LT_COMPILER_PIC($1)
    _LT_COMPILER_C_O($1)
    _LT_COMPILER_FILE_LOCKS($1)
    _LT_LINKER_SHLIBS($1)
    _LT_SYS_DYNAMIC_LINKER($1)
    _LT_LINKER_HARDCODE_LIBPATH($1)

    _LT_CONFIG($1)
  fi # test -n "$compiler"

  CC=$lt_save_CC
  CFLAGS=$lt_save_CFLAGS
  LDCXX=$LD
  LD=$lt_save_LD
  GCC=$lt_save_GCC
  with_gnu_ld=$lt_save_with_gnu_ld
  lt_cv_path_LDCXX=$lt_cv_path_LD
  lt_cv_path_LD=$lt_save_path_LD
  lt_cv_prog_gnu_ldcxx=$lt_cv_prog_gnu_ld
  lt_cv_prog_gnu_ld=$lt_save_with_gnu_ld
fi # test yes != "$_lt_caught_CXX_error"

AC_LANG_POP
])# _LT_LANG_CXX_CONFIG


# _LT_FUNC_STRIPNAME_CNF
# ----------------------
# func_stripname_cnf prefix suffix name
# strip PREFIX and SUFFIX off of NAME.
# PREFIX and SUFFIX must not contain globbing or regex special
# characters, hashes, percent signs, but SUFFIX may contain a leading
# dot (in which case that matches only a dot).
#
# This function is identical to the (non-XSI) version of func_stripname,
# except this one can be used by m4 code that may be executed by configure,
# rather than the libtool script.
m4_defun([_LT_FUNC_STRIPNAME_CNF],[dnl
AC_REQUIRE([_LT_DECL_SED])
AC_REQUIRE([_LT_PROG_ECHO_BACKSLASH])
func_stripname_cnf ()
{
  case @S|@2 in
  .*) func_stripname_result=`$ECHO "@S|@3" | $SED "s%^@S|@1%%; s%\\\\@S|@2\$%%"`;;
  *)  func_stripname_result=`$ECHO "@S|@3" | $SED "s%^@S|@1%%; s%@S|@2\$%%"`;;
  esac
} # func_stripname_cnf
])# _LT_FUNC_STRIPNAME_CNF


# _LT_SYS_HIDDEN_LIBDEPS([TAGNAME])
# ---------------------------------
# Figure out "hidden" library dependencies from verbose
# compiler output when linking a shared library.
# Parse the compiler output and extract the necessary
# objects, libraries and library flags.
m4_defun([_LT_SYS_HIDDEN_LIBDEPS],
[m4_require([_LT_FILEUTILS_DEFAULTS])dnl
AC_REQUIRE([_LT_FUNC_STRIPNAME_CNF])dnl
# Dependencies to place before and after the object being linked:
_LT_TAGVAR(predep_objects, $1)=
_LT_TAGVAR(postdep_objects, $1)=
_LT_TAGVAR(predeps, $1)=
_LT_TAGVAR(postdeps, $1)=
_LT_TAGVAR(compiler_lib_search_path, $1)=

dnl we can't use the lt_simple_compile_test_code here,
dnl because it contains code intended for an executable,
dnl not a library.  It's possible we should let each
dnl tag define a new lt_????_link_test_code variable,
dnl but it's only used here...
m4_if([$1], [], [cat > conftest.$ac_ext <<_LT_EOF
int a;
void foo (void) { a = 0; }
_LT_EOF
], [$1], [CXX], [cat > conftest.$ac_ext <<_LT_EOF
class Foo
{
public:
  Foo (void) { a = 0; }
private:
  int a;
};
_LT_EOF
], [$1], [F77], [cat > conftest.$ac_ext <<_LT_EOF
      subroutine foo
      implicit none
      integer*4 a
      a=0
      return
      end
_LT_EOF
], [$1], [FC], [cat > conftest.$ac_ext <<_LT_EOF
      subroutine foo
      implicit none
      integer a
      a=0
      return
      end
_LT_EOF
], [$1], [GCJ], [cat > conftest.$ac_ext <<_LT_EOF
public class foo {
  private int a;
  public void bar (void) {
    a = 0;
  }
};
_LT_EOF
], [$1], [GO], [cat > conftest.$ac_ext <<_LT_EOF
package foo
func foo() {
}
_LT_EOF
])

_lt_libdeps_save_CFLAGS=$CFLAGS
case "$CC $CFLAGS " in #(
*\ -flto*\ *) CFLAGS="$CFLAGS -fno-lto" ;;
*\ -fwhopr*\ *) CFLAGS="$CFLAGS -fno-whopr" ;;
*\ -fuse-linker-plugin*\ *) CFLAGS="$CFLAGS -fno-use-linker-plugin" ;;
esac

dnl Parse the compiler output and extract the necessary
dnl objects, libraries and library flags.
if AC_TRY_EVAL(ac_compile); then
  # Parse the compiler output and extract the necessary
  # objects, libraries and library flags.

  # Sentinel used to keep track of whether or not we are before
  # the conftest object file.
  pre_test_object_deps_done=no

  for p in `eval "$output_verbose_link_cmd"`; do
    case $prev$p in

    -L* | -R* | -l*)
       # Some compilers place space between "-{L,R}" and the path.
       # Remove the space.
       if test x-L = "$p" ||
          test x-R = "$p"; then
	 prev=$p
	 continue
       fi

       # Expand the sysroot to ease extracting the directories later.
       if test -z "$prev"; then
         case $p in
         -L*) func_stripname_cnf '-L' '' "$p"; prev=-L; p=$func_stripname_result ;;
         -R*) func_stripname_cnf '-R' '' "$p"; prev=-R; p=$func_stripname_result ;;
         -l*) func_stripname_cnf '-l' '' "$p"; prev=-l; p=$func_stripname_result ;;
         esac
       fi
       case $p in
       =*) func_stripname_cnf '=' '' "$p"; p=$lt_sysroot$func_stripname_result ;;
       esac
       if test no = "$pre_test_object_deps_done"; then
	 case $prev in
	 -L | -R)
	   # Internal compiler library paths should come after those
	   # provided the user.  The postdeps already come after the
	   # user supplied libs so there is no need to process them.
	   if test -z "$_LT_TAGVAR(compiler_lib_search_path, $1)"; then
	     _LT_TAGVAR(compiler_lib_search_path, $1)=$prev$p
	   else
	     _LT_TAGVAR(compiler_lib_search_path, $1)="${_LT_TAGVAR(compiler_lib_search_path, $1)} $prev$p"
	   fi
	   ;;
	 # The "-l" case would never come before the object being
	 # linked, so don't bother handling this case.
	 esac
       else
	 if test -z "$_LT_TAGVAR(postdeps, $1)"; then
	   _LT_TAGVAR(postdeps, $1)=$prev$p
	 else
	   _LT_TAGVAR(postdeps, $1)="${_LT_TAGVAR(postdeps, $1)} $prev$p"
	 fi
       fi
       prev=
       ;;

    *.lto.$objext) ;; # Ignore GCC LTO objects
    *.$objext)
       # This assumes that the test object file only shows up
       # once in the compiler output.
       if test "$p" = "conftest.$objext"; then
	 pre_test_object_deps_done=yes
	 continue
       fi

       if test no = "$pre_test_object_deps_done"; then
	 if test -z "$_LT_TAGVAR(predep_objects, $1)"; then
	   _LT_TAGVAR(predep_objects, $1)=$p
	 else
	   _LT_TAGVAR(predep_objects, $1)="$_LT_TAGVAR(predep_objects, $1) $p"
	 fi
       else
	 if test -z "$_LT_TAGVAR(postdep_objects, $1)"; then
	   _LT_TAGVAR(postdep_objects, $1)=$p
	 else
	   _LT_TAGVAR(postdep_objects, $1)="$_LT_TAGVAR(postdep_objects, $1) $p"
	 fi
       fi
       ;;

    *) ;; # Ignore the rest.

    esac
  done

  # Clean up.
  rm -f a.out a.exe
else
  echo "libtool.m4: error: problem compiling $1 test program"
fi

$RM -f confest.$objext
CFLAGS=$_lt_libdeps_save_CFLAGS

# PORTME: override above test on systems where it is broken
m4_if([$1], [CXX],
[case $host_os in
interix[[3-9]]*)
  # Interix 3.5 installs completely hosed .la files for C++, so rather than
  # hack all around it, let's just trust "g++" to DTRT.
  _LT_TAGVAR(predep_objects,$1)=
  _LT_TAGVAR(postdep_objects,$1)=
  _LT_TAGVAR(postdeps,$1)=
  ;;
esac
])

case " $_LT_TAGVAR(postdeps, $1) " in
*" -lc "*) _LT_TAGVAR(archive_cmds_need_lc, $1)=no ;;
esac
 _LT_TAGVAR(compiler_lib_search_dirs, $1)=
if test -n "${_LT_TAGVAR(compiler_lib_search_path, $1)}"; then
 _LT_TAGVAR(compiler_lib_search_dirs, $1)=`echo " ${_LT_TAGVAR(compiler_lib_search_path, $1)}" | $SED -e 's! -L! !g' -e 's!^ !!'`
fi
_LT_TAGDECL([], [compiler_lib_search_dirs], [1],
    [The directories searched by this compiler when creating a shared library])
_LT_TAGDECL([], [predep_objects], [1],
    [Dependencies to place before and after the objects being linked to
    create a shared library])
_LT_TAGDECL([], [postdep_objects], [1])
_LT_TAGDECL([], [predeps], [1])
_LT_TAGDECL([], [postdeps], [1])
_LT_TAGDECL([], [compiler_lib_search_path], [1],
    [The library search path used internally by the compiler when linking
    a shared library])
])# _LT_SYS_HIDDEN_LIBDEPS


# _LT_LANG_F77_CONFIG([TAG])
# --------------------------
# Ensure that the configuration variables for a Fortran 77 compiler are
# suitably defined.  These variables are subsequently used by _LT_CONFIG
# to write the compiler configuration to 'libtool'.
m4_defun([_LT_LANG_F77_CONFIG],
[AC_LANG_PUSH(Fortran 77)
if test -z "$F77" || test no = "$F77"; then
  _lt_disable_F77=yes
fi

_LT_TAGVAR(archive_cmds_need_lc, $1)=no
_LT_TAGVAR(allow_undefined_flag, $1)=
_LT_TAGVAR(always_export_symbols, $1)=no
_LT_TAGVAR(archive_expsym_cmds, $1)=
_LT_TAGVAR(export_dynamic_flag_spec, $1)=
_LT_TAGVAR(hardcode_direct, $1)=no
_LT_TAGVAR(hardcode_direct_absolute, $1)=no
_LT_TAGVAR(hardcode_libdir_flag_spec, $1)=
_LT_TAGVAR(hardcode_libdir_separator, $1)=
_LT_TAGVAR(hardcode_minus_L, $1)=no
_LT_TAGVAR(hardcode_automatic, $1)=no
_LT_TAGVAR(inherit_rpath, $1)=no
_LT_TAGVAR(module_cmds, $1)=
_LT_TAGVAR(module_expsym_cmds, $1)=
_LT_TAGVAR(link_all_deplibs, $1)=unknown
_LT_TAGVAR(old_archive_cmds, $1)=$old_archive_cmds
_LT_TAGVAR(reload_flag, $1)=$reload_flag
_LT_TAGVAR(reload_cmds, $1)=$reload_cmds
_LT_TAGVAR(no_undefined_flag, $1)=
_LT_TAGVAR(whole_archive_flag_spec, $1)=
_LT_TAGVAR(enable_shared_with_static_runtimes, $1)=no

# Source file extension for f77 test sources.
ac_ext=f

# Object file extension for compiled f77 test sources.
objext=o
_LT_TAGVAR(objext, $1)=$objext

# No sense in running all these tests if we already determined that
# the F77 compiler isn't working.  Some variables (like enable_shared)
# are currently assumed to apply to all compilers on this platform,
# and will be corrupted by setting them based on a non-working compiler.
if test yes != "$_lt_disable_F77"; then
  # Code to be used in simple compile tests
  lt_simple_compile_test_code="\
      subroutine t
      return
      end
"

  # Code to be used in simple link tests
  lt_simple_link_test_code="\
      program t
      end
"

  # ltmain only uses $CC for tagged configurations so make sure $CC is set.
  _LT_TAG_COMPILER

  # save warnings/boilerplate of simple test code
  _LT_COMPILER_BOILERPLATE
  _LT_LINKER_BOILERPLATE

  # Allow CC to be a program name with arguments.
  lt_save_CC=$CC
  lt_save_GCC=$GCC
  lt_save_CFLAGS=$CFLAGS
  CC=${F77-"f77"}
  CFLAGS=$FFLAGS
  compiler=$CC
  _LT_TAGVAR(compiler, $1)=$CC
  _LT_CC_BASENAME([$compiler])
  GCC=$G77
  if test -n "$compiler"; then
    AC_MSG_CHECKING([if libtool supports shared libraries])
    AC_MSG_RESULT([$can_build_shared])

    AC_MSG_CHECKING([whether to build shared libraries])
    test no = "$can_build_shared" && enable_shared=no

    # On AIX, shared libraries and static libraries use the same namespace, and
    # are all built from PIC.
    case $host_os in
      aix3*)
        test yes = "$enable_shared" && enable_static=no
        if test -n "$RANLIB"; then
          archive_cmds="$archive_cmds~\$RANLIB \$lib"
          postinstall_cmds='$RANLIB $lib'
        fi
        ;;
      aix[[4-9]]*)
	if test ia64 != "$host_cpu"; then
	  case $enable_shared,$with_aix_soname,$aix_use_runtimelinking in
	  yes,aix,yes) ;;		# shared object as lib.so file only
	  yes,svr4,*) ;;		# shared object as lib.so archive member only
	  yes,*) enable_static=no ;;	# shared object in lib.a archive as well
	  esac
	fi
        ;;
    esac
    AC_MSG_RESULT([$enable_shared])

    AC_MSG_CHECKING([whether to build static libraries])
    # Make sure either enable_shared or enable_static is yes.
    test yes = "$enable_shared" || enable_static=yes
    AC_MSG_RESULT([$enable_static])

    _LT_TAGVAR(GCC, $1)=$G77
    _LT_TAGVAR(LD, $1)=$LD

    ## CAVEAT EMPTOR:
    ## There is no encapsulation within the following macros, do not change
    ## the running order or otherwise move them around unless you know exactly
    ## what you are doing...
    _LT_COMPILER_PIC($1)
    _LT_COMPILER_C_O($1)
    _LT_COMPILER_FILE_LOCKS($1)
    _LT_LINKER_SHLIBS($1)
    _LT_SYS_DYNAMIC_LINKER($1)
    _LT_LINKER_HARDCODE_LIBPATH($1)

    _LT_CONFIG($1)
  fi # test -n "$compiler"

  GCC=$lt_save_GCC
  CC=$lt_save_CC
  CFLAGS=$lt_save_CFLAGS
fi # test yes != "$_lt_disable_F77"

AC_LANG_POP
])# _LT_LANG_F77_CONFIG


# _LT_LANG_FC_CONFIG([TAG])
# -------------------------
# Ensure that the configuration variables for a Fortran compiler are
# suitably defined.  These variables are subsequently used by _LT_CONFIG
# to write the compiler configuration to 'libtool'.
m4_defun([_LT_LANG_FC_CONFIG],
[AC_LANG_PUSH(Fortran)

if test -z "$FC" || test no = "$FC"; then
  _lt_disable_FC=yes
fi

_LT_TAGVAR(archive_cmds_need_lc, $1)=no
_LT_TAGVAR(allow_undefined_flag, $1)=
_LT_TAGVAR(always_export_symbols, $1)=no
_LT_TAGVAR(archive_expsym_cmds, $1)=
_LT_TAGVAR(export_dynamic_flag_spec, $1)=
_LT_TAGVAR(hardcode_direct, $1)=no
_LT_TAGVAR(hardcode_direct_absolute, $1)=no
_LT_TAGVAR(hardcode_libdir_flag_spec, $1)=
_LT_TAGVAR(hardcode_libdir_separator, $1)=
_LT_TAGVAR(hardcode_minus_L, $1)=no
_LT_TAGVAR(hardcode_automatic, $1)=no
_LT_TAGVAR(inherit_rpath, $1)=no
_LT_TAGVAR(module_cmds, $1)=
_LT_TAGVAR(module_expsym_cmds, $1)=
_LT_TAGVAR(link_all_deplibs, $1)=unknown
_LT_TAGVAR(old_archive_cmds, $1)=$old_archive_cmds
_LT_TAGVAR(reload_flag, $1)=$reload_flag
_LT_TAGVAR(reload_cmds, $1)=$reload_cmds
_LT_TAGVAR(no_undefined_flag, $1)=
_LT_TAGVAR(whole_archive_flag_spec, $1)=
_LT_TAGVAR(enable_shared_with_static_runtimes, $1)=no

# Source file extension for fc test sources.
ac_ext=${ac_fc_srcext-f}

# Object file extension for compiled fc test sources.
objext=o
_LT_TAGVAR(objext, $1)=$objext

# No sense in running all these tests if we already determined that
# the FC compiler isn't working.  Some variables (like enable_shared)
# are currently assumed to apply to all compilers on this platform,
# and will be corrupted by setting them based on a non-working compiler.
if test yes != "$_lt_disable_FC"; then
  # Code to be used in simple compile tests
  lt_simple_compile_test_code="\
      subroutine t
      return
      end
"

  # Code to be used in simple link tests
  lt_simple_link_test_code="\
      program t
      end
"

  # ltmain only uses $CC for tagged configurations so make sure $CC is set.
  _LT_TAG_COMPILER

  # save warnings/boilerplate of simple test code
  _LT_COMPILER_BOILERPLATE
  _LT_LINKER_BOILERPLATE

  # Allow CC to be a program name with arguments.
  lt_save_CC=$CC
  lt_save_GCC=$GCC
  lt_save_CFLAGS=$CFLAGS
  CC=${FC-"f95"}
  CFLAGS=$FCFLAGS
  compiler=$CC
  GCC=$ac_cv_fc_compiler_gnu

  _LT_TAGVAR(compiler, $1)=$CC
  _LT_CC_BASENAME([$compiler])

  if test -n "$compiler"; then
    AC_MSG_CHECKING([if libtool supports shared libraries])
    AC_MSG_RESULT([$can_build_shared])

    AC_MSG_CHECKING([whether to build shared libraries])
    test no = "$can_build_shared" && enable_shared=no

    # On AIX, shared libraries and static libraries use the same namespace, and
    # are all built from PIC.
    case $host_os in
      aix3*)
        test yes = "$enable_shared" && enable_static=no
        if test -n "$RANLIB"; then
          archive_cmds="$archive_cmds~\$RANLIB \$lib"
          postinstall_cmds='$RANLIB $lib'
        fi
        ;;
      aix[[4-9]]*)
	if test ia64 != "$host_cpu"; then
	  case $enable_shared,$with_aix_soname,$aix_use_runtimelinking in
	  yes,aix,yes) ;;		# shared object as lib.so file only
	  yes,svr4,*) ;;		# shared object as lib.so archive member only
	  yes,*) enable_static=no ;;	# shared object in lib.a archive as well
	  esac
	fi
        ;;
    esac
    AC_MSG_RESULT([$enable_shared])

    AC_MSG_CHECKING([whether to build static libraries])
    # Make sure either enable_shared or enable_static is yes.
    test yes = "$enable_shared" || enable_static=yes
    AC_MSG_RESULT([$enable_static])

    _LT_TAGVAR(GCC, $1)=$ac_cv_fc_compiler_gnu
    _LT_TAGVAR(LD, $1)=$LD

    ## CAVEAT EMPTOR:
    ## There is no encapsulation within the following macros, do not change
    ## the running order or otherwise move them around unless you know exactly
    ## what you are doing...
    _LT_SYS_HIDDEN_LIBDEPS($1)
    _LT_COMPILER_PIC($1)
    _LT_COMPILER_C_O($1)
    _LT_COMPILER_FILE_LOCKS($1)
    _LT_LINKER_SHLIBS($1)
    _LT_SYS_DYNAMIC_LINKER($1)
    _LT_LINKER_HARDCODE_LIBPATH($1)

    _LT_CONFIG($1)
  fi # test -n "$compiler"

  GCC=$lt_save_GCC
  CC=$lt_save_CC
  CFLAGS=$lt_save_CFLAGS
fi # test yes != "$_lt_disable_FC"

AC_LANG_POP
])# _LT_LANG_FC_CONFIG


# _LT_LANG_GCJ_CONFIG([TAG])
# --------------------------
# Ensure that the configuration variables for the GNU Java Compiler compiler
# are suitably defined.  These variables are subsequently used by _LT_CONFIG
# to write the compiler configuration to 'libtool'.
m4_defun([_LT_LANG_GCJ_CONFIG],
[AC_REQUIRE([LT_PROG_GCJ])dnl
AC_LANG_SAVE

# Source file extension for Java test sources.
ac_ext=java

# Object file extension for compiled Java test sources.
objext=o
_LT_TAGVAR(objext, $1)=$objext

# Code to be used in simple compile tests
lt_simple_compile_test_code="class foo {}"

# Code to be used in simple link tests
lt_simple_link_test_code='public class conftest { public static void main(String[[]] argv) {}; }'

# ltmain only uses $CC for tagged configurations so make sure $CC is set.
_LT_TAG_COMPILER

# save warnings/boilerplate of simple test code
_LT_COMPILER_BOILERPLATE
_LT_LINKER_BOILERPLATE

# Allow CC to be a program name with arguments.
lt_save_CC=$CC
lt_save_CFLAGS=$CFLAGS
lt_save_GCC=$GCC
GCC=yes
CC=${GCJ-"gcj"}
CFLAGS=$GCJFLAGS
compiler=$CC
_LT_TAGVAR(compiler, $1)=$CC
_LT_TAGVAR(LD, $1)=$LD
_LT_CC_BASENAME([$compiler])

# GCJ did not exist at the time GCC didn't implicitly link libc in.
_LT_TAGVAR(archive_cmds_need_lc, $1)=no

_LT_TAGVAR(old_archive_cmds, $1)=$old_archive_cmds
_LT_TAGVAR(reload_flag, $1)=$reload_flag
_LT_TAGVAR(reload_cmds, $1)=$reload_cmds

if test -n "$compiler"; then
  _LT_COMPILER_NO_RTTI($1)
  _LT_COMPILER_PIC($1)
  _LT_COMPILER_C_O($1)
  _LT_COMPILER_FILE_LOCKS($1)
  _LT_LINKER_SHLIBS($1)
  _LT_LINKER_HARDCODE_LIBPATH($1)

  _LT_CONFIG($1)
fi

AC_LANG_RESTORE

GCC=$lt_save_GCC
CC=$lt_save_CC
CFLAGS=$lt_save_CFLAGS
])# _LT_LANG_GCJ_CONFIG


# _LT_LANG_GO_CONFIG([TAG])
# --------------------------
# Ensure that the configuration variables for the GNU Go compiler
# are suitably defined.  These variables are subsequently used by _LT_CONFIG
# to write the compiler configuration to 'libtool'.
m4_defun([_LT_LANG_GO_CONFIG],
[AC_REQUIRE([LT_PROG_GO])dnl
AC_LANG_SAVE

# Source file extension for Go test sources.
ac_ext=go

# Object file extension for compiled Go test sources.
objext=o
_LT_TAGVAR(objext, $1)=$objext

# Code to be used in simple compile tests
lt_simple_compile_test_code="package main; func main() { }"

# Code to be used in simple link tests
lt_simple_link_test_code='package main; func main() { }'

# ltmain only uses $CC for tagged configurations so make sure $CC is set.
_LT_TAG_COMPILER

# save warnings/boilerplate of simple test code
_LT_COMPILER_BOILERPLATE
_LT_LINKER_BOILERPLATE

# Allow CC to be a program name with arguments.
lt_save_CC=$CC
lt_save_CFLAGS=$CFLAGS
lt_save_GCC=$GCC
GCC=yes
CC=${GOC-"gccgo"}
CFLAGS=$GOFLAGS
compiler=$CC
_LT_TAGVAR(compiler, $1)=$CC
_LT_TAGVAR(LD, $1)=$LD
_LT_CC_BASENAME([$compiler])

# Go did not exist at the time GCC didn't implicitly link libc in.
_LT_TAGVAR(archive_cmds_need_lc, $1)=no

_LT_TAGVAR(old_archive_cmds, $1)=$old_archive_cmds
_LT_TAGVAR(reload_flag, $1)=$reload_flag
_LT_TAGVAR(reload_cmds, $1)=$reload_cmds

if test -n "$compiler"; then
  _LT_COMPILER_NO_RTTI($1)
  _LT_COMPILER_PIC($1)
  _LT_COMPILER_C_O($1)
  _LT_COMPILER_FILE_LOCKS($1)
  _LT_LINKER_SHLIBS($1)
  _LT_LINKER_HARDCODE_LIBPATH($1)

  _LT_CONFIG($1)
fi

AC_LANG_RESTORE

GCC=$lt_save_GCC
CC=$lt_save_CC
CFLAGS=$lt_save_CFLAGS
])# _LT_LANG_GO_CONFIG


# _LT_LANG_RC_CONFIG([TAG])
# -------------------------
# Ensure that the configuration variables for the Windows resource compiler
# are suitably defined.  These variables are subsequently used by _LT_CONFIG
# to write the compiler configuration to 'libtool'.
m4_defun([_LT_LANG_RC_CONFIG],
[AC_REQUIRE([LT_PROG_RC])dnl
AC_LANG_SAVE

# Source file extension for RC test sources.
ac_ext=rc

# Object file extension for compiled RC test sources.
objext=o
_LT_TAGVAR(objext, $1)=$objext

# Code to be used in simple compile tests
lt_simple_compile_test_code='sample MENU { MENUITEM "&Soup", 100, CHECKED }'

# Code to be used in simple link tests
lt_simple_link_test_code=$lt_simple_compile_test_code

# ltmain only uses $CC for tagged configurations so make sure $CC is set.
_LT_TAG_COMPILER

# save warnings/boilerplate of simple test code
_LT_COMPILER_BOILERPLATE
_LT_LINKER_BOILERPLATE

# Allow CC to be a program name with arguments.
lt_save_CC=$CC
lt_save_CFLAGS=$CFLAGS
lt_save_GCC=$GCC
GCC=
CC=${RC-"windres"}
CFLAGS=
compiler=$CC
_LT_TAGVAR(compiler, $1)=$CC
_LT_CC_BASENAME([$compiler])
_LT_TAGVAR(lt_cv_prog_compiler_c_o, $1)=yes

if test -n "$compiler"; then
  :
  _LT_CONFIG($1)
fi

GCC=$lt_save_GCC
AC_LANG_RESTORE
CC=$lt_save_CC
CFLAGS=$lt_save_CFLAGS
])# _LT_LANG_RC_CONFIG


# LT_PROG_GCJ
# -----------
AC_DEFUN([LT_PROG_GCJ],
[m4_ifdef([AC_PROG_GCJ], [AC_PROG_GCJ],
  [m4_ifdef([A][M_PROG_GCJ], [A][M_PROG_GCJ],
    [AC_CHECK_TOOL(GCJ, gcj,)
      test set = "${GCJFLAGS+set}" || GCJFLAGS="-g -O2"
      AC_SUBST(GCJFLAGS)])])[]dnl
])

# Old name:
AU_ALIAS([LT_AC_PROG_GCJ], [LT_PROG_GCJ])
dnl aclocal-1.4 backwards compatibility:
dnl AC_DEFUN([LT_AC_PROG_GCJ], [])


# LT_PROG_GO
# ----------
AC_DEFUN([LT_PROG_GO],
[AC_CHECK_TOOL(GOC, gccgo,)
])


# LT_PROG_RC
# ----------
AC_DEFUN([LT_PROG_RC],
[AC_CHECK_TOOL(RC, windres,)
])

# Old name:
AU_ALIAS([LT_AC_PROG_RC], [LT_PROG_RC])
dnl aclocal-1.4 backwards compatibility:
dnl AC_DEFUN([LT_AC_PROG_RC], [])


# _LT_DECL_EGREP
# --------------
# If we don't have a new enough Autoconf to choose the best grep
# available, choose the one first in the user's PATH.
m4_defun([_LT_DECL_EGREP],
[AC_REQUIRE([AC_PROG_EGREP])dnl
AC_REQUIRE([AC_PROG_FGREP])dnl
test -z "$GREP" && GREP=grep
_LT_DECL([], [GREP], [1], [A grep program that handles long lines])
_LT_DECL([], [EGREP], [1], [An ERE matcher])
_LT_DECL([], [FGREP], [1], [A literal string matcher])
dnl Non-bleeding-edge autoconf doesn't subst GREP, so do it here too
AC_SUBST([GREP])
])


# _LT_DECL_OBJDUMP
# --------------
# If we don't have a new enough Autoconf to choose the best objdump
# available, choose the one first in the user's PATH.
m4_defun([_LT_DECL_OBJDUMP],
[AC_CHECK_TOOL(OBJDUMP, objdump, false)
test -z "$OBJDUMP" && OBJDUMP=objdump
_LT_DECL([], [OBJDUMP], [1], [An object symbol dumper])
AC_SUBST([OBJDUMP])
])

# _LT_DECL_DLLTOOL
# ----------------
# Ensure DLLTOOL variable is set.
m4_defun([_LT_DECL_DLLTOOL],
[AC_CHECK_TOOL(DLLTOOL, dlltool, false)
test -z "$DLLTOOL" && DLLTOOL=dlltool
_LT_DECL([], [DLLTOOL], [1], [DLL creation program])
AC_SUBST([DLLTOOL])
])

# _LT_DECL_SED
# ------------
# Check for a fully-functional sed program, that truncates
# as few characters as possible.  Prefer GNU sed if found.
m4_defun([_LT_DECL_SED],
[AC_PROG_SED
test -z "$SED" && SED=sed
Xsed="$SED -e 1s/^X//"
_LT_DECL([], [SED], [1], [A sed program that does not truncate output])
_LT_DECL([], [Xsed], ["\$SED -e 1s/^X//"],
    [Sed that helps us avoid accidentally triggering echo(1) options like -n])
])# _LT_DECL_SED

m4_ifndef([AC_PROG_SED], [
# NOTE: This macro has been submitted for inclusion into   #
#  GNU Autoconf as AC_PROG_SED.  When it is available in   #
#  a released version of Autoconf we should remove this    #
#  macro and use it instead.                               #

m4_defun([AC_PROG_SED],
[AC_MSG_CHECKING([for a sed that does not truncate output])
AC_CACHE_VAL(lt_cv_path_SED,
[# Loop through the user's path and test for sed and gsed.
# Then use that list of sed's as ones to test for truncation.
as_save_IFS=$IFS; IFS=$PATH_SEPARATOR
for as_dir in $PATH
do
  IFS=$as_save_IFS
  test -z "$as_dir" && as_dir=.
  for lt_ac_prog in sed gsed; do
    for ac_exec_ext in '' $ac_executable_extensions; do
      if $as_executable_p "$as_dir/$lt_ac_prog$ac_exec_ext"; then
        lt_ac_sed_list="$lt_ac_sed_list $as_dir/$lt_ac_prog$ac_exec_ext"
      fi
    done
  done
done
IFS=$as_save_IFS
lt_ac_max=0
lt_ac_count=0
# Add /usr/xpg4/bin/sed as it is typically found on Solaris
# along with /bin/sed that truncates output.
for lt_ac_sed in $lt_ac_sed_list /usr/xpg4/bin/sed; do
  test ! -f "$lt_ac_sed" && continue
  cat /dev/null > conftest.in
  lt_ac_count=0
  echo $ECHO_N "0123456789$ECHO_C" >conftest.in
  # Check for GNU sed and select it if it is found.
  if "$lt_ac_sed" --version 2>&1 < /dev/null | grep 'GNU' > /dev/null; then
    lt_cv_path_SED=$lt_ac_sed
    break
  fi
  while true; do
    cat conftest.in conftest.in >conftest.tmp
    mv conftest.tmp conftest.in
    cp conftest.in conftest.nl
    echo >>conftest.nl
    $lt_ac_sed -e 's/a$//' < conftest.nl >conftest.out || break
    cmp -s conftest.out conftest.nl || break
    # 10000 chars as input seems more than enough
    test 10 -lt "$lt_ac_count" && break
    lt_ac_count=`expr $lt_ac_count + 1`
    if test "$lt_ac_count" -gt "$lt_ac_max"; then
      lt_ac_max=$lt_ac_count
      lt_cv_path_SED=$lt_ac_sed
    fi
  done
done
])
SED=$lt_cv_path_SED
AC_SUBST([SED])
AC_MSG_RESULT([$SED])
])#AC_PROG_SED
])#m4_ifndef

# Old name:
AU_ALIAS([LT_AC_PROG_SED], [AC_PROG_SED])
dnl aclocal-1.4 backwards compatibility:
dnl AC_DEFUN([LT_AC_PROG_SED], [])


# _LT_CHECK_SHELL_FEATURES
# ------------------------
# Find out whether the shell is Bourne or XSI compatible,
# or has some other useful features.
m4_defun([_LT_CHECK_SHELL_FEATURES],
[if ( (MAIL=60; unset MAIL) || exit) >/dev/null 2>&1; then
  lt_unset=unset
else
  lt_unset=false
fi
_LT_DECL([], [lt_unset], [0], [whether the shell understands "unset"])dnl

# test EBCDIC or ASCII
case `echo X|tr X '\101'` in
 A) # ASCII based system
    # \n is not interpreted correctly by Solaris 8 /usr/ucb/tr
  lt_SP2NL='tr \040 \012'
  lt_NL2SP='tr \015\012 \040\040'
  ;;
 *) # EBCDIC based system
  lt_SP2NL='tr \100 \n'
  lt_NL2SP='tr \r\n \100\100'
  ;;
esac
_LT_DECL([SP2NL], [lt_SP2NL], [1], [turn spaces into newlines])dnl
_LT_DECL([NL2SP], [lt_NL2SP], [1], [turn newlines into spaces])dnl
])# _LT_CHECK_SHELL_FEATURES


# _LT_PATH_CONVERSION_FUNCTIONS
# -----------------------------
# Determine what file name conversion functions should be used by
# func_to_host_file (and, implicitly, by func_to_host_path).  These are needed
# for certain cross-compile configurations and native mingw.
m4_defun([_LT_PATH_CONVERSION_FUNCTIONS],
[AC_REQUIRE([AC_CANONICAL_HOST])dnl
AC_REQUIRE([AC_CANONICAL_BUILD])dnl
AC_MSG_CHECKING([how to convert $build file names to $host format])
AC_CACHE_VAL(lt_cv_to_host_file_cmd,
[case $host in
  *-*-mingw* )
    case $build in
      *-*-mingw* ) # actually msys
        lt_cv_to_host_file_cmd=func_convert_file_msys_to_w32
        ;;
      *-*-cygwin* )
        lt_cv_to_host_file_cmd=func_convert_file_cygwin_to_w32
        ;;
      * ) # otherwise, assume *nix
        lt_cv_to_host_file_cmd=func_convert_file_nix_to_w32
        ;;
    esac
    ;;
  *-*-cygwin* )
    case $build in
      *-*-mingw* ) # actually msys
        lt_cv_to_host_file_cmd=func_convert_file_msys_to_cygwin
        ;;
      *-*-cygwin* )
        lt_cv_to_host_file_cmd=func_convert_file_noop
        ;;
      * ) # otherwise, assume *nix
        lt_cv_to_host_file_cmd=func_convert_file_nix_to_cygwin
        ;;
    esac
    ;;
  * ) # unhandled hosts (and "normal" native builds)
    lt_cv_to_host_file_cmd=func_convert_file_noop
    ;;
esac
])
to_host_file_cmd=$lt_cv_to_host_file_cmd
AC_MSG_RESULT([$lt_cv_to_host_file_cmd])
_LT_DECL([to_host_file_cmd], [lt_cv_to_host_file_cmd],
         [0], [convert $build file names to $host format])dnl

AC_MSG_CHECKING([how to convert $build file names to toolchain format])
AC_CACHE_VAL(lt_cv_to_tool_file_cmd,
[#assume ordinary cross tools, or native build.
lt_cv_to_tool_file_cmd=func_convert_file_noop
case $host in
  *-*-mingw* )
    case $build in
      *-*-mingw* ) # actually msys
        lt_cv_to_tool_file_cmd=func_convert_file_msys_to_w32
        ;;
    esac
    ;;
esac
])
to_tool_file_cmd=$lt_cv_to_tool_file_cmd
AC_MSG_RESULT([$lt_cv_to_tool_file_cmd])
_LT_DECL([to_tool_file_cmd], [lt_cv_to_tool_file_cmd],
         [0], [convert $build files to toolchain format])dnl
])# _LT_PATH_CONVERSION_FUNCTIONS

# lock.m4 serial 13 (gettext-0.18.2)
dnl Copyright (C) 2005-2016 Free Software Foundation, Inc.
dnl This file is free software; the Free Software Foundation
dnl gives unlimited permission to copy and/or distribute it,
dnl with or without modifications, as long as this notice is preserved.

dnl From Bruno Haible.

AC_DEFUN([gl_LOCK],
[
  AC_REQUIRE([gl_THREADLIB])
  if test "$gl_threads_api" = posix; then
    # OSF/1 4.0 and Mac OS X 10.1 lack the pthread_rwlock_t type and the
    # pthread_rwlock_* functions.
    AC_CHECK_TYPE([pthread_rwlock_t],
      [AC_DEFINE([HAVE_PTHREAD_RWLOCK], [1],
         [Define if the POSIX multithreading library has read/write locks.])],
      [],
      [#include <pthread.h>])
    # glibc defines PTHREAD_MUTEX_RECURSIVE as enum, not as a macro.
    AC_COMPILE_IFELSE([
      AC_LANG_PROGRAM(
        [[#include <pthread.h>]],
        [[
#if __FreeBSD__ == 4
error "No, in FreeBSD 4.0 recursive mutexes actually don't work."
#elif (defined __ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__ \
       && __ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__ < 1070)
error "No, in Mac OS X < 10.7 recursive mutexes actually don't work."
#else
int x = (int)PTHREAD_MUTEX_RECURSIVE;
return !x;
#endif
        ]])],
      [AC_DEFINE([HAVE_PTHREAD_MUTEX_RECURSIVE], [1],
         [Define if the <pthread.h> defines PTHREAD_MUTEX_RECURSIVE.])])
  fi
  gl_PREREQ_LOCK
])

# Prerequisites of lib/glthread/lock.c.
AC_DEFUN([gl_PREREQ_LOCK], [:])

# longlong.m4 serial 17
dnl Copyright (C) 1999-2007, 2009-2016 Free Software Foundation, Inc.
dnl This file is free software; the Free Software Foundation
dnl gives unlimited permission to copy and/or distribute it,
dnl with or without modifications, as long as this notice is preserved.

dnl From Paul Eggert.

# Define HAVE_LONG_LONG_INT if 'long long int' works.
# This fixes a bug in Autoconf 2.61, and can be faster
# than what's in Autoconf 2.62 through 2.68.

# Note: If the type 'long long int' exists but is only 32 bits large
# (as on some very old compilers), HAVE_LONG_LONG_INT will not be
# defined. In this case you can treat 'long long int' like 'long int'.

AC_DEFUN([AC_TYPE_LONG_LONG_INT],
[
  AC_REQUIRE([AC_TYPE_UNSIGNED_LONG_LONG_INT])
  AC_CACHE_CHECK([for long long int], [ac_cv_type_long_long_int],
     [ac_cv_type_long_long_int=yes
      if test "x${ac_cv_prog_cc_c99-no}" = xno; then
        ac_cv_type_long_long_int=$ac_cv_type_unsigned_long_long_int
        if test $ac_cv_type_long_long_int = yes; then
          dnl Catch a bug in Tandem NonStop Kernel (OSS) cc -O circa 2004.
          dnl If cross compiling, assume the bug is not important, since
          dnl nobody cross compiles for this platform as far as we know.
          AC_RUN_IFELSE(
            [AC_LANG_PROGRAM(
               [[@%:@include <limits.h>
                 @%:@ifndef LLONG_MAX
                 @%:@ define HALF \
                          (1LL << (sizeof (long long int) * CHAR_BIT - 2))
                 @%:@ define LLONG_MAX (HALF - 1 + HALF)
                 @%:@endif]],
               [[long long int n = 1;
                 int i;
                 for (i = 0; ; i++)
                   {
                     long long int m = n << i;
                     if (m >> i != n)
                       return 1;
                     if (LLONG_MAX / 2 < m)
                       break;
                   }
                 return 0;]])],
            [],
            [ac_cv_type_long_long_int=no],
            [:])
        fi
      fi])
  if test $ac_cv_type_long_long_int = yes; then
    AC_DEFINE([HAVE_LONG_LONG_INT], [1],
      [Define to 1 if the system has the type 'long long int'.])
  fi
])

# Define HAVE_UNSIGNED_LONG_LONG_INT if 'unsigned long long int' works.
# This fixes a bug in Autoconf 2.61, and can be faster
# than what's in Autoconf 2.62 through 2.68.

# Note: If the type 'unsigned long long int' exists but is only 32 bits
# large (as on some very old compilers), AC_TYPE_UNSIGNED_LONG_LONG_INT
# will not be defined. In this case you can treat 'unsigned long long int'
# like 'unsigned long int'.

AC_DEFUN([AC_TYPE_UNSIGNED_LONG_LONG_INT],
[
  AC_CACHE_CHECK([for unsigned long long int],
    [ac_cv_type_unsigned_long_long_int],
    [ac_cv_type_unsigned_long_long_int=yes
     if test "x${ac_cv_prog_cc_c99-no}" = xno; then
       AC_LINK_IFELSE(
         [_AC_TYPE_LONG_LONG_SNIPPET],
         [],
         [ac_cv_type_unsigned_long_long_int=no])
     fi])
  if test $ac_cv_type_unsigned_long_long_int = yes; then
    AC_DEFINE([HAVE_UNSIGNED_LONG_LONG_INT], [1],
      [Define to 1 if the system has the type 'unsigned long long int'.])
  fi
])

# Expands to a C program that can be used to test for simultaneous support
# of 'long long' and 'unsigned long long'. We don't want to say that
# 'long long' is available if 'unsigned long long' is not, or vice versa,
# because too many programs rely on the symmetry between signed and unsigned
# integer types (excluding 'bool').
AC_DEFUN([_AC_TYPE_LONG_LONG_SNIPPET],
[
  AC_LANG_PROGRAM(
    [[/* For now, do not test the preprocessor; as of 2007 there are too many
         implementations with broken preprocessors.  Perhaps this can
         be revisited in 2012.  In the meantime, code should not expect
         #if to work with literals wider than 32 bits.  */
      /* Test literals.  */
      long long int ll = 9223372036854775807ll;
      long long int nll = -9223372036854775807LL;
      unsigned long long int ull = 18446744073709551615ULL;
      /* Test constant expressions.   */
      typedef int a[((-9223372036854775807LL < 0 && 0 < 9223372036854775807ll)
                     ? 1 : -1)];
      typedef int b[(18446744073709551615ULL <= (unsigned long long int) -1
                     ? 1 : -1)];
      int i = 63;]],
    [[/* Test availability of runtime routines for shift and division.  */
      long long int llmax = 9223372036854775807ll;
      unsigned long long int ullmax = 18446744073709551615ull;
      return ((ll << 63) | (ll >> 63) | (ll < i) | (ll > i)
              | (llmax / ll) | (llmax % ll)
              | (ull << 63) | (ull >> 63) | (ull << i) | (ull >> i)
              | (ullmax / ull) | (ullmax % ull));]])
])

# ltdl.m4 - Configure ltdl for the target system. -*-Autoconf-*-
#
#   Copyright (C) 1999-2008, 2011-2015 Free Software Foundation, Inc.
#   Written by Thomas Tanner, 1999
#
# This file is free software; the Free Software Foundation gives
# unlimited permission to copy and/or distribute it, with or without
# modifications, as long as this notice is preserved.

# serial 20 LTDL_INIT

# LT_CONFIG_LTDL_DIR(DIRECTORY, [LTDL-MODE])
# ------------------------------------------
# DIRECTORY contains the libltdl sources.  It is okay to call this
# function multiple times, as long as the same DIRECTORY is always given.
AC_DEFUN([LT_CONFIG_LTDL_DIR],
[AC_BEFORE([$0], [LTDL_INIT])
_$0($*)
])# LT_CONFIG_LTDL_DIR

# We break this out into a separate macro, so that we can call it safely
# internally without being caught accidentally by the sed scan in libtoolize.
m4_defun([_LT_CONFIG_LTDL_DIR],
[dnl remove trailing slashes
m4_pushdef([_ARG_DIR], m4_bpatsubst([$1], [/*$]))
m4_case(_LTDL_DIR,
	[], [dnl only set lt_ltdl_dir if _ARG_DIR is not simply '.'
	     m4_if(_ARG_DIR, [.],
	             [],
		 [m4_define([_LTDL_DIR], _ARG_DIR)
	          _LT_SHELL_INIT([lt_ltdl_dir=']_ARG_DIR['])])],
    [m4_if(_ARG_DIR, _LTDL_DIR,
	    [],
	[m4_fatal([multiple libltdl directories: ']_LTDL_DIR[', ']_ARG_DIR['])])])
m4_popdef([_ARG_DIR])
])# _LT_CONFIG_LTDL_DIR

# Initialise:
m4_define([_LTDL_DIR], [])


# _LT_BUILD_PREFIX
# ----------------
# If Autoconf is new enough, expand to '$(top_build_prefix)', otherwise
# to '$(top_builddir)/'.
m4_define([_LT_BUILD_PREFIX],
[m4_ifdef([AC_AUTOCONF_VERSION],
   [m4_if(m4_version_compare(m4_defn([AC_AUTOCONF_VERSION]), [2.62]),
	  [-1], [m4_ifdef([_AC_HAVE_TOP_BUILD_PREFIX],
			  [$(top_build_prefix)],
			  [$(top_builddir)/])],
	  [$(top_build_prefix)])],
   [$(top_builddir)/])[]dnl
])


# LTDL_CONVENIENCE
# ----------------
# sets LIBLTDL to the link flags for the libltdl convenience library and
# LTDLINCL to the include flags for the libltdl header and adds
# --enable-ltdl-convenience to the configure arguments.  Note that
# AC_CONFIG_SUBDIRS is not called here.  LIBLTDL will be prefixed with
# '$(top_build_prefix)' if available, otherwise with '$(top_builddir)/',
# and LTDLINCL will be prefixed with '$(top_srcdir)/' (note the single
# quotes!).  If your package is not flat and you're not using automake,
# define top_build_prefix, top_builddir, and top_srcdir appropriately
# in your Makefiles.
AC_DEFUN([LTDL_CONVENIENCE],
[AC_BEFORE([$0], [LTDL_INIT])dnl
dnl Although the argument is deprecated and no longer documented,
dnl LTDL_CONVENIENCE used to take a DIRECTORY orgument, if we have one
dnl here make sure it is the same as any other declaration of libltdl's
dnl location!  This also ensures lt_ltdl_dir is set when configure.ac is
dnl not yet using an explicit LT_CONFIG_LTDL_DIR.
m4_ifval([$1], [_LT_CONFIG_LTDL_DIR([$1])])dnl
_$0()
])# LTDL_CONVENIENCE

# AC_LIBLTDL_CONVENIENCE accepted a directory argument in older libtools,
# now we have LT_CONFIG_LTDL_DIR:
AU_DEFUN([AC_LIBLTDL_CONVENIENCE],
[_LT_CONFIG_LTDL_DIR([m4_default([$1], [libltdl])])
_LTDL_CONVENIENCE])

dnl aclocal-1.4 backwards compatibility:
dnl AC_DEFUN([AC_LIBLTDL_CONVENIENCE], [])


# _LTDL_CONVENIENCE
# -----------------
# Code shared by LTDL_CONVENIENCE and LTDL_INIT([convenience]).
m4_defun([_LTDL_CONVENIENCE],
[case $enable_ltdl_convenience in
  no) AC_MSG_ERROR([this package needs a convenience libltdl]) ;;
  "") enable_ltdl_convenience=yes
      ac_configure_args="$ac_configure_args --enable-ltdl-convenience" ;;
esac
LIBLTDL='_LT_BUILD_PREFIX'"${lt_ltdl_dir+$lt_ltdl_dir/}libltdlc.la"
LTDLDEPS=$LIBLTDL
LTDLINCL='-I$(top_srcdir)'"${lt_ltdl_dir+/$lt_ltdl_dir}"

AC_SUBST([LIBLTDL])
AC_SUBST([LTDLDEPS])
AC_SUBST([LTDLINCL])

# For backwards non-gettext consistent compatibility...
INCLTDL=$LTDLINCL
AC_SUBST([INCLTDL])
])# _LTDL_CONVENIENCE


# LTDL_INSTALLABLE
# ----------------
# sets LIBLTDL to the link flags for the libltdl installable library
# and LTDLINCL to the include flags for the libltdl header and adds
# --enable-ltdl-install to the configure arguments.  Note that
# AC_CONFIG_SUBDIRS is not called from here.  If an installed libltdl
# is not found, LIBLTDL will be prefixed with '$(top_build_prefix)' if
# available, otherwise with '$(top_builddir)/', and LTDLINCL will be
# prefixed with '$(top_srcdir)/' (note the single quotes!).  If your
# package is not flat and you're not using automake, define top_build_prefix,
# top_builddir, and top_srcdir appropriately in your Makefiles.
# In the future, this macro may have to be called after LT_INIT.
AC_DEFUN([LTDL_INSTALLABLE],
[AC_BEFORE([$0], [LTDL_INIT])dnl
dnl Although the argument is deprecated and no longer documented,
dnl LTDL_INSTALLABLE used to take a DIRECTORY orgument, if we have one
dnl here make sure it is the same as any other declaration of libltdl's
dnl location!  This also ensures lt_ltdl_dir is set when configure.ac is
dnl not yet using an explicit LT_CONFIG_LTDL_DIR.
m4_ifval([$1], [_LT_CONFIG_LTDL_DIR([$1])])dnl
_$0()
])# LTDL_INSTALLABLE

# AC_LIBLTDL_INSTALLABLE accepted a directory argument in older libtools,
# now we have LT_CONFIG_LTDL_DIR:
AU_DEFUN([AC_LIBLTDL_INSTALLABLE],
[_LT_CONFIG_LTDL_DIR([m4_default([$1], [libltdl])])
_LTDL_INSTALLABLE])

dnl aclocal-1.4 backwards compatibility:
dnl AC_DEFUN([AC_LIBLTDL_INSTALLABLE], [])


# _LTDL_INSTALLABLE
# -----------------
# Code shared by LTDL_INSTALLABLE and LTDL_INIT([installable]).
m4_defun([_LTDL_INSTALLABLE],
[if test -f "$prefix/lib/libltdl.la"; then
  lt_save_LDFLAGS=$LDFLAGS
  LDFLAGS="-L$prefix/lib $LDFLAGS"
  AC_CHECK_LIB([ltdl], [lt_dlinit], [lt_lib_ltdl=yes])
  LDFLAGS=$lt_save_LDFLAGS
  if test yes = "${lt_lib_ltdl-no}"; then
    if test yes != "$enable_ltdl_install"; then
      # Don't overwrite $prefix/lib/libltdl.la without --enable-ltdl-install
      AC_MSG_WARN([not overwriting libltdl at $prefix, force with '--enable-ltdl-install'])
      enable_ltdl_install=no
    fi
  elif test no = "$enable_ltdl_install"; then
    AC_MSG_WARN([libltdl not installed, but installation disabled])
  fi
fi

# If configure.ac declared an installable ltdl, and the user didn't override
# with --disable-ltdl-install, we will install the shipped libltdl.
case $enable_ltdl_install in
  no) ac_configure_args="$ac_configure_args --enable-ltdl-install=no"
      LIBLTDL=-lltdl
      LTDLDEPS=
      LTDLINCL=
      ;;
  *)  enable_ltdl_install=yes
      ac_configure_args="$ac_configure_args --enable-ltdl-install"
      LIBLTDL='_LT_BUILD_PREFIX'"${lt_ltdl_dir+$lt_ltdl_dir/}libltdl.la"
      LTDLDEPS=$LIBLTDL
      LTDLINCL='-I$(top_srcdir)'"${lt_ltdl_dir+/$lt_ltdl_dir}"
      ;;
esac

AC_SUBST([LIBLTDL])
AC_SUBST([LTDLDEPS])
AC_SUBST([LTDLINCL])

# For backwards non-gettext consistent compatibility...
INCLTDL=$LTDLINCL
AC_SUBST([INCLTDL])
])# LTDL_INSTALLABLE


# _LTDL_MODE_DISPATCH
# -------------------
m4_define([_LTDL_MODE_DISPATCH],
[dnl If _LTDL_DIR is '.', then we are configuring libltdl itself:
m4_if(_LTDL_DIR, [],
	[],
    dnl if _LTDL_MODE was not set already, the default value is 'subproject':
    [m4_case(m4_default(_LTDL_MODE, [subproject]),
	  [subproject], [AC_CONFIG_SUBDIRS(_LTDL_DIR)
			  _LT_SHELL_INIT([lt_dlopen_dir=$lt_ltdl_dir])],
	  [nonrecursive], [_LT_SHELL_INIT([lt_dlopen_dir=$lt_ltdl_dir; lt_libobj_prefix=$lt_ltdl_dir/])],
	  [recursive], [],
	[m4_fatal([unknown libltdl mode: ]_LTDL_MODE)])])dnl
dnl Be careful not to expand twice:
m4_define([$0], [])
])# _LTDL_MODE_DISPATCH


# _LT_LIBOBJ(MODULE_NAME)
# -----------------------
# Like AC_LIBOBJ, except that MODULE_NAME goes into _LT_LIBOBJS instead
# of into LIBOBJS.
AC_DEFUN([_LT_LIBOBJ], [
  m4_pattern_allow([^_LT_LIBOBJS$])
  _LT_LIBOBJS="$_LT_LIBOBJS $1.$ac_objext"
])# _LT_LIBOBJS


# LTDL_INIT([OPTIONS])
# --------------------
# Clients of libltdl can use this macro to allow the installer to
# choose between a shipped copy of the ltdl sources or a preinstalled
# version of the library.  If the shipped ltdl sources are not in a
# subdirectory named libltdl, the directory name must be given by
# LT_CONFIG_LTDL_DIR.
AC_DEFUN([LTDL_INIT],
[dnl Parse OPTIONS
_LT_SET_OPTIONS([$0], [$1])

dnl We need to keep our own list of libobjs separate from our parent project,
dnl and the easiest way to do that is redefine the AC_LIBOBJs macro while
dnl we look for our own LIBOBJs.
m4_pushdef([AC_LIBOBJ], m4_defn([_LT_LIBOBJ]))
m4_pushdef([AC_LIBSOURCES])

dnl If not otherwise defined, default to the 1.5.x compatible subproject mode:
m4_if(_LTDL_MODE, [],
        [m4_define([_LTDL_MODE], m4_default([$2], [subproject]))
        m4_if([-1], [m4_bregexp(_LTDL_MODE, [\(subproject\|\(non\)?recursive\)])],
                [m4_fatal([unknown libltdl mode: ]_LTDL_MODE)])])

AC_ARG_WITH([included_ltdl],
    [AS_HELP_STRING([--with-included-ltdl],
                    [use the GNU ltdl sources included here])])

if test yes != "$with_included_ltdl"; then
  # We are not being forced to use the included libltdl sources, so
  # decide whether there is a useful installed version we can use.
  AC_CHECK_HEADER([ltdl.h],
      [AC_CHECK_DECL([lt_dlinterface_register],
	   [AC_CHECK_LIB([ltdl], [lt_dladvise_preload],
	       [with_included_ltdl=no],
	       [with_included_ltdl=yes])],
	   [with_included_ltdl=yes],
	   [AC_INCLUDES_DEFAULT
	    #include <ltdl.h>])],
      [with_included_ltdl=yes],
      [AC_INCLUDES_DEFAULT]
  )
fi

dnl If neither LT_CONFIG_LTDL_DIR, LTDL_CONVENIENCE nor LTDL_INSTALLABLE
dnl was called yet, then for old times' sake, we assume libltdl is in an
dnl eponymous directory:
AC_PROVIDE_IFELSE([LT_CONFIG_LTDL_DIR], [], [_LT_CONFIG_LTDL_DIR([libltdl])])

AC_ARG_WITH([ltdl_include],
    [AS_HELP_STRING([--with-ltdl-include=DIR],
                    [use the ltdl headers installed in DIR])])

if test -n "$with_ltdl_include"; then
  if test -f "$with_ltdl_include/ltdl.h"; then :
  else
    AC_MSG_ERROR([invalid ltdl include directory: '$with_ltdl_include'])
  fi
else
  with_ltdl_include=no
fi

AC_ARG_WITH([ltdl_lib],
    [AS_HELP_STRING([--with-ltdl-lib=DIR],
                    [use the libltdl.la installed in DIR])])

if test -n "$with_ltdl_lib"; then
  if test -f "$with_ltdl_lib/libltdl.la"; then :
  else
    AC_MSG_ERROR([invalid ltdl library directory: '$with_ltdl_lib'])
  fi
else
  with_ltdl_lib=no
fi

case ,$with_included_ltdl,$with_ltdl_include,$with_ltdl_lib, in
  ,yes,no,no,)
	m4_case(m4_default(_LTDL_TYPE, [convenience]),
	    [convenience], [_LTDL_CONVENIENCE],
	    [installable], [_LTDL_INSTALLABLE],
	  [m4_fatal([unknown libltdl build type: ]_LTDL_TYPE)])
	;;
  ,no,no,no,)
	# If the included ltdl is not to be used, then use the
	# preinstalled libltdl we found.
	AC_DEFINE([HAVE_LTDL], [1],
	  [Define this if a modern libltdl is already installed])
	LIBLTDL=-lltdl
	LTDLDEPS=
	LTDLINCL=
	;;
  ,no*,no,*)
	AC_MSG_ERROR(['--with-ltdl-include' and '--with-ltdl-lib' options must be used together])
	;;
  *)	with_included_ltdl=no
	LIBLTDL="-L$with_ltdl_lib -lltdl"
	LTDLDEPS=
	LTDLINCL=-I$with_ltdl_include
	;;
esac
INCLTDL=$LTDLINCL

# Report our decision...
AC_MSG_CHECKING([where to find libltdl headers])
AC_MSG_RESULT([$LTDLINCL])
AC_MSG_CHECKING([where to find libltdl library])
AC_MSG_RESULT([$LIBLTDL])

_LTDL_SETUP

dnl restore autoconf definition.
m4_popdef([AC_LIBOBJ])
m4_popdef([AC_LIBSOURCES])

AC_CONFIG_COMMANDS_PRE([
    _ltdl_libobjs=
    _ltdl_ltlibobjs=
    if test -n "$_LT_LIBOBJS"; then
      # Remove the extension.
      _lt_sed_drop_objext='s/\.o$//;s/\.obj$//'
      for i in `for i in $_LT_LIBOBJS; do echo "$i"; done | sed "$_lt_sed_drop_objext" | sort -u`; do
        _ltdl_libobjs="$_ltdl_libobjs $lt_libobj_prefix$i.$ac_objext"
        _ltdl_ltlibobjs="$_ltdl_ltlibobjs $lt_libobj_prefix$i.lo"
      done
    fi
    AC_SUBST([ltdl_LIBOBJS], [$_ltdl_libobjs])
    AC_SUBST([ltdl_LTLIBOBJS], [$_ltdl_ltlibobjs])
])

# Only expand once:
m4_define([LTDL_INIT])
])# LTDL_INIT

# Old names:
AU_DEFUN([AC_LIB_LTDL], [LTDL_INIT($@)])
AU_DEFUN([AC_WITH_LTDL], [LTDL_INIT($@)])
AU_DEFUN([LT_WITH_LTDL], [LTDL_INIT($@)])
dnl aclocal-1.4 backwards compatibility:
dnl AC_DEFUN([AC_LIB_LTDL], [])
dnl AC_DEFUN([AC_WITH_LTDL], [])
dnl AC_DEFUN([LT_WITH_LTDL], [])


# _LTDL_SETUP
# -----------
# Perform all the checks necessary for compilation of the ltdl objects
#  -- including compiler checks and header checks.  This is a public
# interface  mainly for the benefit of libltdl's own configure.ac, most
# other users should call LTDL_INIT instead.
AC_DEFUN([_LTDL_SETUP],
[AC_REQUIRE([AC_PROG_CC])dnl
AC_REQUIRE([LT_SYS_MODULE_EXT])dnl
AC_REQUIRE([LT_SYS_MODULE_PATH])dnl
AC_REQUIRE([LT_SYS_DLSEARCH_PATH])dnl
AC_REQUIRE([LT_LIB_DLLOAD])dnl
AC_REQUIRE([LT_SYS_SYMBOL_USCORE])dnl
AC_REQUIRE([LT_FUNC_DLSYM_USCORE])dnl
AC_REQUIRE([LT_SYS_DLOPEN_DEPLIBS])dnl
AC_REQUIRE([LT_FUNC_ARGZ])dnl

m4_require([_LT_CHECK_OBJDIR])dnl
m4_require([_LT_HEADER_DLFCN])dnl
m4_require([_LT_CHECK_DLPREOPEN])dnl
m4_require([_LT_DECL_SED])dnl

dnl Don't require this, or it will be expanded earlier than the code
dnl that sets the variables it relies on:
_LT_ENABLE_INSTALL

dnl _LTDL_MODE specific code must be called at least once:
_LTDL_MODE_DISPATCH

# In order that ltdl.c can compile, find out the first AC_CONFIG_HEADERS
# the user used.  This is so that ltdl.h can pick up the parent projects
# config.h file, The first file in AC_CONFIG_HEADERS must contain the
# definitions required by ltdl.c.
# FIXME: Remove use of undocumented AC_LIST_HEADERS (2.59 compatibility).
AC_CONFIG_COMMANDS_PRE([dnl
m4_pattern_allow([^LT_CONFIG_H$])dnl
m4_ifset([AH_HEADER],
    [LT_CONFIG_H=AH_HEADER],
    [m4_ifset([AC_LIST_HEADERS],
	    [LT_CONFIG_H=`echo "AC_LIST_HEADERS" | $SED 's|^[[      ]]*||;s|[[ :]].*$||'`],
	[])])])
AC_SUBST([LT_CONFIG_H])

AC_CHECK_HEADERS([unistd.h dl.h sys/dl.h dld.h mach-o/dyld.h dirent.h],
	[], [], [AC_INCLUDES_DEFAULT])

AC_CHECK_FUNCS([closedir opendir readdir], [], [AC_LIBOBJ([lt__dirent])])
AC_CHECK_FUNCS([strlcat strlcpy], [], [AC_LIBOBJ([lt__strl])])

m4_pattern_allow([LT_LIBEXT])dnl
AC_DEFINE_UNQUOTED([LT_LIBEXT],["$libext"],[The archive extension])

name=
eval "lt_libprefix=\"$libname_spec\""
m4_pattern_allow([LT_LIBPREFIX])dnl
AC_DEFINE_UNQUOTED([LT_LIBPREFIX],["$lt_libprefix"],[The archive prefix])

name=ltdl
eval "LTDLOPEN=\"$libname_spec\""
AC_SUBST([LTDLOPEN])
])# _LTDL_SETUP


# _LT_ENABLE_INSTALL
# ------------------
m4_define([_LT_ENABLE_INSTALL],
[AC_ARG_ENABLE([ltdl-install],
    [AS_HELP_STRING([--enable-ltdl-install], [install libltdl])])

case ,$enable_ltdl_install,$enable_ltdl_convenience in
  *yes*) ;;
  *) enable_ltdl_convenience=yes ;;
esac

m4_ifdef([AM_CONDITIONAL],
[AM_CONDITIONAL(INSTALL_LTDL, test no != "${enable_ltdl_install-no}")
 AM_CONDITIONAL(CONVENIENCE_LTDL, test no != "${enable_ltdl_convenience-no}")])
])# _LT_ENABLE_INSTALL


# LT_SYS_DLOPEN_DEPLIBS
# ---------------------
AC_DEFUN([LT_SYS_DLOPEN_DEPLIBS],
[AC_REQUIRE([AC_CANONICAL_HOST])dnl
AC_CACHE_CHECK([whether deplibs are loaded by dlopen],
  [lt_cv_sys_dlopen_deplibs],
  [# PORTME does your system automatically load deplibs for dlopen?
  # or its logical equivalent (e.g. shl_load for HP-UX < 11)
  # For now, we just catch OSes we know something about -- in the
  # future, we'll try test this programmatically.
  lt_cv_sys_dlopen_deplibs=unknown
  case $host_os in
  aix3*|aix4.1.*|aix4.2.*)
    # Unknown whether this is true for these versions of AIX, but
    # we want this 'case' here to explicitly catch those versions.
    lt_cv_sys_dlopen_deplibs=unknown
    ;;
  aix[[4-9]]*)
    lt_cv_sys_dlopen_deplibs=yes
    ;;
  amigaos*)
    case $host_cpu in
    powerpc)
      lt_cv_sys_dlopen_deplibs=no
      ;;
    esac
    ;;
  bitrig*)
    lt_cv_sys_dlopen_deplibs=yes
    ;;
  darwin*)
    # Assuming the user has installed a libdl from somewhere, this is true
    # If you are looking for one http://www.opendarwin.org/projects/dlcompat
    lt_cv_sys_dlopen_deplibs=yes
    ;;
  freebsd* | dragonfly*)
    lt_cv_sys_dlopen_deplibs=yes
    ;;
  gnu* | linux* | k*bsd*-gnu | kopensolaris*-gnu)
    # GNU and its variants, using gnu ld.so (Glibc)
    lt_cv_sys_dlopen_deplibs=yes
    ;;
  hpux10*|hpux11*)
    lt_cv_sys_dlopen_deplibs=yes
    ;;
  interix*)
    lt_cv_sys_dlopen_deplibs=yes
    ;;
  irix[[12345]]*|irix6.[[01]]*)
    # Catch all versions of IRIX before 6.2, and indicate that we don't
    # know how it worked for any of those versions.
    lt_cv_sys_dlopen_deplibs=unknown
    ;;
  irix*)
    # The case above catches anything before 6.2, and it's known that
    # at 6.2 and later dlopen does load deplibs.
    lt_cv_sys_dlopen_deplibs=yes
    ;;
  netbsd* | netbsdelf*-gnu)
    lt_cv_sys_dlopen_deplibs=yes
    ;;
  openbsd*)
    lt_cv_sys_dlopen_deplibs=yes
    ;;
  osf[[1234]]*)
    # dlopen did load deplibs (at least at 4.x), but until the 5.x series,
    # it did *not* use an RPATH in a shared library to find objects the
    # library depends on, so we explicitly say 'no'.
    lt_cv_sys_dlopen_deplibs=no
    ;;
  osf5.0|osf5.0a|osf5.1)
    # dlopen *does* load deplibs and with the right loader patch applied
    # it even uses RPATH in a shared library to search for shared objects
    # that the library depends on, but there's no easy way to know if that
    # patch is installed.  Since this is the case, all we can really
    # say is unknown -- it depends on the patch being installed.  If
    # it is, this changes to 'yes'.  Without it, it would be 'no'.
    lt_cv_sys_dlopen_deplibs=unknown
    ;;
  osf*)
    # the two cases above should catch all versions of osf <= 5.1.  Read
    # the comments above for what we know about them.
    # At > 5.1, deplibs are loaded *and* any RPATH in a shared library
    # is used to find them so we can finally say 'yes'.
    lt_cv_sys_dlopen_deplibs=yes
    ;;
  qnx*)
    lt_cv_sys_dlopen_deplibs=yes
    ;;
  solaris*)
    lt_cv_sys_dlopen_deplibs=yes
    ;;
  sysv5* | sco3.2v5* | sco5v6* | unixware* | OpenUNIX* | sysv4*uw2*)
    libltdl_cv_sys_dlopen_deplibs=yes
    ;;
  esac
  ])
if test yes != "$lt_cv_sys_dlopen_deplibs"; then
 AC_DEFINE([LTDL_DLOPEN_DEPLIBS], [1],
    [Define if the OS needs help to load dependent libraries for dlopen().])
fi
])# LT_SYS_DLOPEN_DEPLIBS

# Old name:
AU_ALIAS([AC_LTDL_SYS_DLOPEN_DEPLIBS], [LT_SYS_DLOPEN_DEPLIBS])
dnl aclocal-1.4 backwards compatibility:
dnl AC_DEFUN([AC_LTDL_SYS_DLOPEN_DEPLIBS], [])


# LT_SYS_MODULE_EXT
# -----------------
AC_DEFUN([LT_SYS_MODULE_EXT],
[m4_require([_LT_SYS_DYNAMIC_LINKER])dnl
AC_CACHE_CHECK([what extension is used for runtime loadable modules],
  [libltdl_cv_shlibext],
[
module=yes
eval libltdl_cv_shlibext=$shrext_cmds
module=no
eval libltdl_cv_shrext=$shrext_cmds
  ])
if test -n "$libltdl_cv_shlibext"; then
  m4_pattern_allow([LT_MODULE_EXT])dnl
  AC_DEFINE_UNQUOTED([LT_MODULE_EXT], ["$libltdl_cv_shlibext"],
    [Define to the extension used for runtime loadable modules, say, ".so".])
fi
if test "$libltdl_cv_shrext" != "$libltdl_cv_shlibext"; then
  m4_pattern_allow([LT_SHARED_EXT])dnl
  AC_DEFINE_UNQUOTED([LT_SHARED_EXT], ["$libltdl_cv_shrext"],
    [Define to the shared library suffix, say, ".dylib".])
fi
if test -n "$shared_archive_member_spec"; then
  m4_pattern_allow([LT_SHARED_LIB_MEMBER])dnl
  AC_DEFINE_UNQUOTED([LT_SHARED_LIB_MEMBER], ["($shared_archive_member_spec.o)"],
    [Define to the shared archive member specification, say "(shr.o)".])
fi
])# LT_SYS_MODULE_EXT

# Old name:
AU_ALIAS([AC_LTDL_SHLIBEXT], [LT_SYS_MODULE_EXT])
dnl aclocal-1.4 backwards compatibility:
dnl AC_DEFUN([AC_LTDL_SHLIBEXT], [])


# LT_SYS_MODULE_PATH
# ------------------
AC_DEFUN([LT_SYS_MODULE_PATH],
[m4_require([_LT_SYS_DYNAMIC_LINKER])dnl
AC_CACHE_CHECK([what variable specifies run-time module search path],
  [lt_cv_module_path_var], [lt_cv_module_path_var=$shlibpath_var])
if test -n "$lt_cv_module_path_var"; then
  m4_pattern_allow([LT_MODULE_PATH_VAR])dnl
  AC_DEFINE_UNQUOTED([LT_MODULE_PATH_VAR], ["$lt_cv_module_path_var"],
    [Define to the name of the environment variable that determines the run-time module search path.])
fi
])# LT_SYS_MODULE_PATH

# Old name:
AU_ALIAS([AC_LTDL_SHLIBPATH], [LT_SYS_MODULE_PATH])
dnl aclocal-1.4 backwards compatibility:
dnl AC_DEFUN([AC_LTDL_SHLIBPATH], [])


# LT_SYS_DLSEARCH_PATH
# --------------------
AC_DEFUN([LT_SYS_DLSEARCH_PATH],
[m4_require([_LT_SYS_DYNAMIC_LINKER])dnl
AC_CACHE_CHECK([for the default library search path],
  [lt_cv_sys_dlsearch_path],
  [lt_cv_sys_dlsearch_path=$sys_lib_dlsearch_path_spec])
if test -n "$lt_cv_sys_dlsearch_path"; then
  sys_dlsearch_path=
  for dir in $lt_cv_sys_dlsearch_path; do
    if test -z "$sys_dlsearch_path"; then
      sys_dlsearch_path=$dir
    else
      sys_dlsearch_path=$sys_dlsearch_path$PATH_SEPARATOR$dir
    fi
  done
  m4_pattern_allow([LT_DLSEARCH_PATH])dnl
  AC_DEFINE_UNQUOTED([LT_DLSEARCH_PATH], ["$sys_dlsearch_path"],
    [Define to the system default library search path.])
fi
])# LT_SYS_DLSEARCH_PATH

# Old name:
AU_ALIAS([AC_LTDL_SYSSEARCHPATH], [LT_SYS_DLSEARCH_PATH])
dnl aclocal-1.4 backwards compatibility:
dnl AC_DEFUN([AC_LTDL_SYSSEARCHPATH], [])


# _LT_CHECK_DLPREOPEN
# -------------------
m4_defun([_LT_CHECK_DLPREOPEN],
[m4_require([_LT_CMD_GLOBAL_SYMBOLS])dnl
AC_CACHE_CHECK([whether libtool supports -dlopen/-dlpreopen],
  [libltdl_cv_preloaded_symbols],
  [if test -n "$lt_cv_sys_global_symbol_pipe"; then
    libltdl_cv_preloaded_symbols=yes
  else
    libltdl_cv_preloaded_symbols=no
  fi
  ])
if test yes = "$libltdl_cv_preloaded_symbols"; then
  AC_DEFINE([HAVE_PRELOADED_SYMBOLS], [1],
    [Define if libtool can extract symbol lists from object files.])
fi
])# _LT_CHECK_DLPREOPEN


# LT_LIB_DLLOAD
# -------------
AC_DEFUN([LT_LIB_DLLOAD],
[m4_pattern_allow([^LT_DLLOADERS$])
LT_DLLOADERS=
AC_SUBST([LT_DLLOADERS])

AC_LANG_PUSH([C])
lt_dlload_save_LIBS=$LIBS

LIBADD_DLOPEN=
AC_SEARCH_LIBS([dlopen], [dl],
	[AC_DEFINE([HAVE_LIBDL], [1],
		   [Define if you have the libdl library or equivalent.])
	if test "$ac_cv_search_dlopen" != "none required"; then
	  LIBADD_DLOPEN=-ldl
	fi
	libltdl_cv_lib_dl_dlopen=yes
	LT_DLLOADERS="$LT_DLLOADERS ${lt_dlopen_dir+$lt_dlopen_dir/}dlopen.la"],
    [AC_LINK_IFELSE([AC_LANG_PROGRAM([[#if HAVE_DLFCN_H
#  include <dlfcn.h>
#endif
    ]], [[dlopen(0, 0);]])],
	    [AC_DEFINE([HAVE_LIBDL], [1],
		       [Define if you have the libdl library or equivalent.])
	    libltdl_cv_func_dlopen=yes
	    LT_DLLOADERS="$LT_DLLOADERS ${lt_dlopen_dir+$lt_dlopen_dir/}dlopen.la"],
	[AC_CHECK_LIB([svld], [dlopen],
		[AC_DEFINE([HAVE_LIBDL], [1],
			 [Define if you have the libdl library or equivalent.])
	        LIBADD_DLOPEN=-lsvld libltdl_cv_func_dlopen=yes
		LT_DLLOADERS="$LT_DLLOADERS ${lt_dlopen_dir+$lt_dlopen_dir/}dlopen.la"])])])
if test yes = "$libltdl_cv_func_dlopen" || test yes = "$libltdl_cv_lib_dl_dlopen"
then
  lt_save_LIBS=$LIBS
  LIBS="$LIBS $LIBADD_DLOPEN"
  AC_CHECK_FUNCS([dlerror])
  LIBS=$lt_save_LIBS
fi
AC_SUBST([LIBADD_DLOPEN])

LIBADD_SHL_LOAD=
AC_CHECK_FUNC([shl_load],
	[AC_DEFINE([HAVE_SHL_LOAD], [1],
		   [Define if you have the shl_load function.])
	LT_DLLOADERS="$LT_DLLOADERS ${lt_dlopen_dir+$lt_dlopen_dir/}shl_load.la"],
    [AC_CHECK_LIB([dld], [shl_load],
	    [AC_DEFINE([HAVE_SHL_LOAD], [1],
		       [Define if you have the shl_load function.])
	    LT_DLLOADERS="$LT_DLLOADERS ${lt_dlopen_dir+$lt_dlopen_dir/}shl_load.la"
	    LIBADD_SHL_LOAD=-ldld])])
AC_SUBST([LIBADD_SHL_LOAD])

case $host_os in
darwin[[1567]].*)
# We only want this for pre-Mac OS X 10.4.
  AC_CHECK_FUNC([_dyld_func_lookup],
	[AC_DEFINE([HAVE_DYLD], [1],
		   [Define if you have the _dyld_func_lookup function.])
	LT_DLLOADERS="$LT_DLLOADERS ${lt_dlopen_dir+$lt_dlopen_dir/}dyld.la"])
  ;;
beos*)
  LT_DLLOADERS="$LT_DLLOADERS ${lt_dlopen_dir+$lt_dlopen_dir/}load_add_on.la"
  ;;
cygwin* | mingw* | pw32*)
  AC_CHECK_DECLS([cygwin_conv_path], [], [], [[#include <sys/cygwin.h>]])
  LT_DLLOADERS="$LT_DLLOADERS ${lt_dlopen_dir+$lt_dlopen_dir/}loadlibrary.la"
  ;;
esac

AC_CHECK_LIB([dld], [dld_link],
	[AC_DEFINE([HAVE_DLD], [1],
		   [Define if you have the GNU dld library.])
		LT_DLLOADERS="$LT_DLLOADERS ${lt_dlopen_dir+$lt_dlopen_dir/}dld_link.la"])
AC_SUBST([LIBADD_DLD_LINK])

m4_pattern_allow([^LT_DLPREOPEN$])
LT_DLPREOPEN=
if test -n "$LT_DLLOADERS"
then
  for lt_loader in $LT_DLLOADERS; do
    LT_DLPREOPEN="$LT_DLPREOPEN-dlpreopen $lt_loader "
  done
  AC_DEFINE([HAVE_LIBDLLOADER], [1],
            [Define if libdlloader will be built on this platform])
fi
AC_SUBST([LT_DLPREOPEN])

dnl This isn't used anymore, but set it for backwards compatibility
LIBADD_DL="$LIBADD_DLOPEN $LIBADD_SHL_LOAD"
AC_SUBST([LIBADD_DL])

LIBS=$lt_dlload_save_LIBS
AC_LANG_POP
])# LT_LIB_DLLOAD

# Old name:
AU_ALIAS([AC_LTDL_DLLIB], [LT_LIB_DLLOAD])
dnl aclocal-1.4 backwards compatibility:
dnl AC_DEFUN([AC_LTDL_DLLIB], [])


# LT_SYS_SYMBOL_USCORE
# --------------------
# does the compiler prefix global symbols with an underscore?
AC_DEFUN([LT_SYS_SYMBOL_USCORE],
[m4_require([_LT_CMD_GLOBAL_SYMBOLS])dnl
AC_CACHE_CHECK([for _ prefix in compiled symbols],
  [lt_cv_sys_symbol_underscore],
  [lt_cv_sys_symbol_underscore=no
  cat > conftest.$ac_ext <<_LT_EOF
void nm_test_func(){}
int main(){nm_test_func;return 0;}
_LT_EOF
  if AC_TRY_EVAL(ac_compile); then
    # Now try to grab the symbols.
    ac_nlist=conftest.nm
    if AC_TRY_EVAL(NM conftest.$ac_objext \| $lt_cv_sys_global_symbol_pipe \> $ac_nlist) && test -s "$ac_nlist"; then
      # See whether the symbols have a leading underscore.
      if grep '^. _nm_test_func' "$ac_nlist" >/dev/null; then
        lt_cv_sys_symbol_underscore=yes
      else
        if grep '^. nm_test_func ' "$ac_nlist" >/dev/null; then
	  :
        else
	  echo "configure: cannot find nm_test_func in $ac_nlist" >&AS_MESSAGE_LOG_FD
        fi
      fi
    else
      echo "configure: cannot run $lt_cv_sys_global_symbol_pipe" >&AS_MESSAGE_LOG_FD
    fi
  else
    echo "configure: failed program was:" >&AS_MESSAGE_LOG_FD
    cat conftest.c >&AS_MESSAGE_LOG_FD
  fi
  rm -rf conftest*
  ])
  sys_symbol_underscore=$lt_cv_sys_symbol_underscore
  AC_SUBST([sys_symbol_underscore])
])# LT_SYS_SYMBOL_USCORE

# Old name:
AU_ALIAS([AC_LTDL_SYMBOL_USCORE], [LT_SYS_SYMBOL_USCORE])
dnl aclocal-1.4 backwards compatibility:
dnl AC_DEFUN([AC_LTDL_SYMBOL_USCORE], [])


# LT_FUNC_DLSYM_USCORE
# --------------------
AC_DEFUN([LT_FUNC_DLSYM_USCORE],
[AC_REQUIRE([_LT_COMPILER_PIC])dnl	for lt_prog_compiler_wl
AC_REQUIRE([LT_SYS_SYMBOL_USCORE])dnl	for lt_cv_sys_symbol_underscore
AC_REQUIRE([LT_SYS_MODULE_EXT])dnl	for libltdl_cv_shlibext
if test yes = "$lt_cv_sys_symbol_underscore"; then
  if test yes = "$libltdl_cv_func_dlopen" || test yes = "$libltdl_cv_lib_dl_dlopen"; then
    AC_CACHE_CHECK([whether we have to add an underscore for dlsym],
      [libltdl_cv_need_uscore],
      [libltdl_cv_need_uscore=unknown
      dlsym_uscore_save_LIBS=$LIBS
      LIBS="$LIBS $LIBADD_DLOPEN"
      libname=conftmod # stay within 8.3 filename limits!
      cat >$libname.$ac_ext <<_LT_EOF
[#line $LINENO "configure"
#include "confdefs.h"
/* When -fvisibility=hidden is used, assume the code has been annotated
   correspondingly for the symbols needed.  */
#if defined __GNUC__ && (((__GNUC__ == 3) && (__GNUC_MINOR__ >= 3)) || (__GNUC__ > 3))
int fnord () __attribute__((visibility("default")));
#endif
int fnord () { return 42; }]
_LT_EOF

      # ltfn_module_cmds module_cmds
      # Execute tilde-delimited MODULE_CMDS with environment primed for
      # $module_cmds or $archive_cmds type content.
      ltfn_module_cmds ()
      {( # subshell avoids polluting parent global environment
          module_cmds_save_ifs=$IFS; IFS='~'
          for cmd in @S|@1; do
            IFS=$module_cmds_save_ifs
            libobjs=$libname.$ac_objext; lib=$libname$libltdl_cv_shlibext
            rpath=/not-exists; soname=$libname$libltdl_cv_shlibext; output_objdir=.
            major=; versuffix=; verstring=; deplibs=
            ECHO=echo; wl=$lt_prog_compiler_wl; allow_undefined_flag=
            eval $cmd
          done
          IFS=$module_cmds_save_ifs
      )}

      # Compile a loadable module using libtool macro expansion results.
      $CC $pic_flag -c $libname.$ac_ext
      ltfn_module_cmds "${module_cmds:-$archive_cmds}"

      # Try to fetch fnord with dlsym().
      libltdl_dlunknown=0; libltdl_dlnouscore=1; libltdl_dluscore=2
      cat >conftest.$ac_ext <<_LT_EOF
[#line $LINENO "configure"
#include "confdefs.h"
#if HAVE_DLFCN_H
#include <dlfcn.h>
#endif
#include <stdio.h>
#ifndef RTLD_GLOBAL
#  ifdef DL_GLOBAL
#    define RTLD_GLOBAL DL_GLOBAL
#  else
#    define RTLD_GLOBAL 0
#  endif
#endif
#ifndef RTLD_NOW
#  ifdef DL_NOW
#    define RTLD_NOW DL_NOW
#  else
#    define RTLD_NOW 0
#  endif
#endif
int main () {
  void *handle = dlopen ("`pwd`/$libname$libltdl_cv_shlibext", RTLD_GLOBAL|RTLD_NOW);
  int status = $libltdl_dlunknown;
  if (handle) {
    if (dlsym (handle, "fnord"))
      status = $libltdl_dlnouscore;
    else {
      if (dlsym (handle, "_fnord"))
        status = $libltdl_dluscore;
      else
	puts (dlerror ());
    }
    dlclose (handle);
  } else
    puts (dlerror ());
  return status;
}]
_LT_EOF
      if AC_TRY_EVAL(ac_link) && test -s "conftest$ac_exeext" 2>/dev/null; then
        (./conftest; exit; ) >&AS_MESSAGE_LOG_FD 2>/dev/null
        libltdl_status=$?
        case x$libltdl_status in
          x$libltdl_dlnouscore) libltdl_cv_need_uscore=no ;;
	  x$libltdl_dluscore) libltdl_cv_need_uscore=yes ;;
	  x*) libltdl_cv_need_uscore=unknown ;;
        esac
      fi
      rm -rf conftest* $libname*
      LIBS=$dlsym_uscore_save_LIBS
    ])
  fi
fi

if test yes = "$libltdl_cv_need_uscore"; then
  AC_DEFINE([NEED_USCORE], [1],
    [Define if dlsym() requires a leading underscore in symbol names.])
fi
])# LT_FUNC_DLSYM_USCORE

# Old name:
AU_ALIAS([AC_LTDL_DLSYM_USCORE], [LT_FUNC_DLSYM_USCORE])
dnl aclocal-1.4 backwards compatibility:
dnl AC_DEFUN([AC_LTDL_DLSYM_USCORE], [])

# Helper functions for option handling.                    -*- Autoconf -*-
#
#   Copyright (C) 2004-2005, 2007-2009, 2011-2015 Free Software
#   Foundation, Inc.
#   Written by Gary V. Vaughan, 2004
#
# This file is free software; the Free Software Foundation gives
# unlimited permission to copy and/or distribute it, with or without
# modifications, as long as this notice is preserved.

# serial 8 ltoptions.m4

# This is to help aclocal find these macros, as it can't see m4_define.
AC_DEFUN([LTOPTIONS_VERSION], [m4_if([1])])


# _LT_MANGLE_OPTION(MACRO-NAME, OPTION-NAME)
# ------------------------------------------
m4_define([_LT_MANGLE_OPTION],
[[_LT_OPTION_]m4_bpatsubst($1__$2, [[^a-zA-Z0-9_]], [_])])


# _LT_SET_OPTION(MACRO-NAME, OPTION-NAME)
# ---------------------------------------
# Set option OPTION-NAME for macro MACRO-NAME, and if there is a
# matching handler defined, dispatch to it.  Other OPTION-NAMEs are
# saved as a flag.
m4_define([_LT_SET_OPTION],
[m4_define(_LT_MANGLE_OPTION([$1], [$2]))dnl
m4_ifdef(_LT_MANGLE_DEFUN([$1], [$2]),
        _LT_MANGLE_DEFUN([$1], [$2]),
    [m4_warning([Unknown $1 option '$2'])])[]dnl
])


# _LT_IF_OPTION(MACRO-NAME, OPTION-NAME, IF-SET, [IF-NOT-SET])
# ------------------------------------------------------------
# Execute IF-SET if OPTION is set, IF-NOT-SET otherwise.
m4_define([_LT_IF_OPTION],
[m4_ifdef(_LT_MANGLE_OPTION([$1], [$2]), [$3], [$4])])


# _LT_UNLESS_OPTIONS(MACRO-NAME, OPTION-LIST, IF-NOT-SET)
# -------------------------------------------------------
# Execute IF-NOT-SET unless all options in OPTION-LIST for MACRO-NAME
# are set.
m4_define([_LT_UNLESS_OPTIONS],
[m4_foreach([_LT_Option], m4_split(m4_normalize([$2])),
	    [m4_ifdef(_LT_MANGLE_OPTION([$1], _LT_Option),
		      [m4_define([$0_found])])])[]dnl
m4_ifdef([$0_found], [m4_undefine([$0_found])], [$3
])[]dnl
])


# _LT_SET_OPTIONS(MACRO-NAME, OPTION-LIST)
# ----------------------------------------
# OPTION-LIST is a space-separated list of Libtool options associated
# with MACRO-NAME.  If any OPTION has a matching handler declared with
# LT_OPTION_DEFINE, dispatch to that macro; otherwise complain about
# the unknown option and exit.
m4_defun([_LT_SET_OPTIONS],
[# Set options
m4_foreach([_LT_Option], m4_split(m4_normalize([$2])),
    [_LT_SET_OPTION([$1], _LT_Option)])

m4_if([$1],[LT_INIT],[
  dnl
  dnl Simply set some default values (i.e off) if boolean options were not
  dnl specified:
  _LT_UNLESS_OPTIONS([LT_INIT], [dlopen], [enable_dlopen=no
  ])
  _LT_UNLESS_OPTIONS([LT_INIT], [win32-dll], [enable_win32_dll=no
  ])
  dnl
  dnl If no reference was made to various pairs of opposing options, then
  dnl we run the default mode handler for the pair.  For example, if neither
  dnl 'shared' nor 'disable-shared' was passed, we enable building of shared
  dnl archives by default:
  _LT_UNLESS_OPTIONS([LT_INIT], [shared disable-shared], [_LT_ENABLE_SHARED])
  _LT_UNLESS_OPTIONS([LT_INIT], [static disable-static], [_LT_ENABLE_STATIC])
  _LT_UNLESS_OPTIONS([LT_INIT], [pic-only no-pic], [_LT_WITH_PIC])
  _LT_UNLESS_OPTIONS([LT_INIT], [fast-install disable-fast-install],
		   [_LT_ENABLE_FAST_INSTALL])
  _LT_UNLESS_OPTIONS([LT_INIT], [aix-soname=aix aix-soname=both aix-soname=svr4],
		   [_LT_WITH_AIX_SONAME([aix])])
  ])
])# _LT_SET_OPTIONS



# _LT_MANGLE_DEFUN(MACRO-NAME, OPTION-NAME)
# -----------------------------------------
m4_define([_LT_MANGLE_DEFUN],
[[_LT_OPTION_DEFUN_]m4_bpatsubst(m4_toupper([$1__$2]), [[^A-Z0-9_]], [_])])


# LT_OPTION_DEFINE(MACRO-NAME, OPTION-NAME, CODE)
# -----------------------------------------------
m4_define([LT_OPTION_DEFINE],
[m4_define(_LT_MANGLE_DEFUN([$1], [$2]), [$3])[]dnl
])# LT_OPTION_DEFINE


# dlopen
# ------
LT_OPTION_DEFINE([LT_INIT], [dlopen], [enable_dlopen=yes
])

AU_DEFUN([AC_LIBTOOL_DLOPEN],
[_LT_SET_OPTION([LT_INIT], [dlopen])
AC_DIAGNOSE([obsolete],
[$0: Remove this warning and the call to _LT_SET_OPTION when you
put the 'dlopen' option into LT_INIT's first parameter.])
])

dnl aclocal-1.4 backwards compatibility:
dnl AC_DEFUN([AC_LIBTOOL_DLOPEN], [])


# win32-dll
# ---------
# Declare package support for building win32 dll's.
LT_OPTION_DEFINE([LT_INIT], [win32-dll],
[enable_win32_dll=yes

case $host in
*-*-cygwin* | *-*-mingw* | *-*-pw32* | *-*-cegcc*)
  AC_CHECK_TOOL(AS, as, false)
  AC_CHECK_TOOL(DLLTOOL, dlltool, false)
  AC_CHECK_TOOL(OBJDUMP, objdump, false)
  ;;
esac

test -z "$AS" && AS=as
_LT_DECL([], [AS],      [1], [Assembler program])dnl

test -z "$DLLTOOL" && DLLTOOL=dlltool
_LT_DECL([], [DLLTOOL], [1], [DLL creation program])dnl

test -z "$OBJDUMP" && OBJDUMP=objdump
_LT_DECL([], [OBJDUMP], [1], [Object dumper program])dnl
])# win32-dll

AU_DEFUN([AC_LIBTOOL_WIN32_DLL],
[AC_REQUIRE([AC_CANONICAL_HOST])dnl
_LT_SET_OPTION([LT_INIT], [win32-dll])
AC_DIAGNOSE([obsolete],
[$0: Remove this warning and the call to _LT_SET_OPTION when you
put the 'win32-dll' option into LT_INIT's first parameter.])
])

dnl aclocal-1.4 backwards compatibility:
dnl AC_DEFUN([AC_LIBTOOL_WIN32_DLL], [])


# _LT_ENABLE_SHARED([DEFAULT])
# ----------------------------
# implement the --enable-shared flag, and supports the 'shared' and
# 'disable-shared' LT_INIT options.
# DEFAULT is either 'yes' or 'no'.  If omitted, it defaults to 'yes'.
m4_define([_LT_ENABLE_SHARED],
[m4_define([_LT_ENABLE_SHARED_DEFAULT], [m4_if($1, no, no, yes)])dnl
AC_ARG_ENABLE([shared],
    [AS_HELP_STRING([--enable-shared@<:@=PKGS@:>@],
	[build shared libraries @<:@default=]_LT_ENABLE_SHARED_DEFAULT[@:>@])],
    [p=${PACKAGE-default}
    case $enableval in
    yes) enable_shared=yes ;;
    no) enable_shared=no ;;
    *)
      enable_shared=no
      # Look at the argument we got.  We use all the common list separators.
      lt_save_ifs=$IFS; IFS=$IFS$PATH_SEPARATOR,
      for pkg in $enableval; do
	IFS=$lt_save_ifs
	if test "X$pkg" = "X$p"; then
	  enable_shared=yes
	fi
      done
      IFS=$lt_save_ifs
      ;;
    esac],
    [enable_shared=]_LT_ENABLE_SHARED_DEFAULT)

    _LT_DECL([build_libtool_libs], [enable_shared], [0],
	[Whether or not to build shared libraries])
])# _LT_ENABLE_SHARED

LT_OPTION_DEFINE([LT_INIT], [shared], [_LT_ENABLE_SHARED([yes])])
LT_OPTION_DEFINE([LT_INIT], [disable-shared], [_LT_ENABLE_SHARED([no])])

# Old names:
AC_DEFUN([AC_ENABLE_SHARED],
[_LT_SET_OPTION([LT_INIT], m4_if([$1], [no], [disable-])[shared])
])

AC_DEFUN([AC_DISABLE_SHARED],
[_LT_SET_OPTION([LT_INIT], [disable-shared])
])

AU_DEFUN([AM_ENABLE_SHARED], [AC_ENABLE_SHARED($@)])
AU_DEFUN([AM_DISABLE_SHARED], [AC_DISABLE_SHARED($@)])

dnl aclocal-1.4 backwards compatibility:
dnl AC_DEFUN([AM_ENABLE_SHARED], [])
dnl AC_DEFUN([AM_DISABLE_SHARED], [])



# _LT_ENABLE_STATIC([DEFAULT])
# ----------------------------
# implement the --enable-static flag, and support the 'static' and
# 'disable-static' LT_INIT options.
# DEFAULT is either 'yes' or 'no'.  If omitted, it defaults to 'yes'.
m4_define([_LT_ENABLE_STATIC],
[m4_define([_LT_ENABLE_STATIC_DEFAULT], [m4_if($1, no, no, yes)])dnl
AC_ARG_ENABLE([static],
    [AS_HELP_STRING([--enable-static@<:@=PKGS@:>@],
	[build static libraries @<:@default=]_LT_ENABLE_STATIC_DEFAULT[@:>@])],
    [p=${PACKAGE-default}
    case $enableval in
    yes) enable_static=yes ;;
    no) enable_static=no ;;
    *)
     enable_static=no
      # Look at the argument we got.  We use all the common list separators.
      lt_save_ifs=$IFS; IFS=$IFS$PATH_SEPARATOR,
      for pkg in $enableval; do
	IFS=$lt_save_ifs
	if test "X$pkg" = "X$p"; then
	  enable_static=yes
	fi
      done
      IFS=$lt_save_ifs
      ;;
    esac],
    [enable_static=]_LT_ENABLE_STATIC_DEFAULT)

    _LT_DECL([build_old_libs], [enable_static], [0],
	[Whether or not to build static libraries])
])# _LT_ENABLE_STATIC

LT_OPTION_DEFINE([LT_INIT], [static], [_LT_ENABLE_STATIC([yes])])
LT_OPTION_DEFINE([LT_INIT], [disable-static], [_LT_ENABLE_STATIC([no])])

# Old names:
AC_DEFUN([AC_ENABLE_STATIC],
[_LT_SET_OPTION([LT_INIT], m4_if([$1], [no], [disable-])[static])
])

AC_DEFUN([AC_DISABLE_STATIC],
[_LT_SET_OPTION([LT_INIT], [disable-static])
])

AU_DEFUN([AM_ENABLE_STATIC], [AC_ENABLE_STATIC($@)])
AU_DEFUN([AM_DISABLE_STATIC], [AC_DISABLE_STATIC($@)])

dnl aclocal-1.4 backwards compatibility:
dnl AC_DEFUN([AM_ENABLE_STATIC], [])
dnl AC_DEFUN([AM_DISABLE_STATIC], [])



# _LT_ENABLE_FAST_INSTALL([DEFAULT])
# ----------------------------------
# implement the --enable-fast-install flag, and support the 'fast-install'
# and 'disable-fast-install' LT_INIT options.
# DEFAULT is either 'yes' or 'no'.  If omitted, it defaults to 'yes'.
m4_define([_LT_ENABLE_FAST_INSTALL],
[m4_define([_LT_ENABLE_FAST_INSTALL_DEFAULT], [m4_if($1, no, no, yes)])dnl
AC_ARG_ENABLE([fast-install],
    [AS_HELP_STRING([--enable-fast-install@<:@=PKGS@:>@],
    [optimize for fast installation @<:@default=]_LT_ENABLE_FAST_INSTALL_DEFAULT[@:>@])],
    [p=${PACKAGE-default}
    case $enableval in
    yes) enable_fast_install=yes ;;
    no) enable_fast_install=no ;;
    *)
      enable_fast_install=no
      # Look at the argument we got.  We use all the common list separators.
      lt_save_ifs=$IFS; IFS=$IFS$PATH_SEPARATOR,
      for pkg in $enableval; do
	IFS=$lt_save_ifs
	if test "X$pkg" = "X$p"; then
	  enable_fast_install=yes
	fi
      done
      IFS=$lt_save_ifs
      ;;
    esac],
    [enable_fast_install=]_LT_ENABLE_FAST_INSTALL_DEFAULT)

_LT_DECL([fast_install], [enable_fast_install], [0],
	 [Whether or not to optimize for fast installation])dnl
])# _LT_ENABLE_FAST_INSTALL

LT_OPTION_DEFINE([LT_INIT], [fast-install], [_LT_ENABLE_FAST_INSTALL([yes])])
LT_OPTION_DEFINE([LT_INIT], [disable-fast-install], [_LT_ENABLE_FAST_INSTALL([no])])

# Old names:
AU_DEFUN([AC_ENABLE_FAST_INSTALL],
[_LT_SET_OPTION([LT_INIT], m4_if([$1], [no], [disable-])[fast-install])
AC_DIAGNOSE([obsolete],
[$0: Remove this warning and the call to _LT_SET_OPTION when you put
the 'fast-install' option into LT_INIT's first parameter.])
])

AU_DEFUN([AC_DISABLE_FAST_INSTALL],
[_LT_SET_OPTION([LT_INIT], [disable-fast-install])
AC_DIAGNOSE([obsolete],
[$0: Remove this warning and the call to _LT_SET_OPTION when you put
the 'disable-fast-install' option into LT_INIT's first parameter.])
])

dnl aclocal-1.4 backwards compatibility:
dnl AC_DEFUN([AC_ENABLE_FAST_INSTALL], [])
dnl AC_DEFUN([AM_DISABLE_FAST_INSTALL], [])


# _LT_WITH_AIX_SONAME([DEFAULT])
# ----------------------------------
# implement the --with-aix-soname flag, and support the `aix-soname=aix'
# and `aix-soname=both' and `aix-soname=svr4' LT_INIT options. DEFAULT
# is either `aix', `both' or `svr4'.  If omitted, it defaults to `aix'.
m4_define([_LT_WITH_AIX_SONAME],
[m4_define([_LT_WITH_AIX_SONAME_DEFAULT], [m4_if($1, svr4, svr4, m4_if($1, both, both, aix))])dnl
shared_archive_member_spec=
case $host,$enable_shared in
power*-*-aix[[5-9]]*,yes)
  AC_MSG_CHECKING([which variant of shared library versioning to provide])
  AC_ARG_WITH([aix-soname],
    [AS_HELP_STRING([--with-aix-soname=aix|svr4|both],
      [shared library versioning (aka "SONAME") variant to provide on AIX, @<:@default=]_LT_WITH_AIX_SONAME_DEFAULT[@:>@.])],
    [case $withval in
    aix|svr4|both)
      ;;
    *)
      AC_MSG_ERROR([Unknown argument to --with-aix-soname])
      ;;
    esac
    lt_cv_with_aix_soname=$with_aix_soname],
    [AC_CACHE_VAL([lt_cv_with_aix_soname],
      [lt_cv_with_aix_soname=]_LT_WITH_AIX_SONAME_DEFAULT)
    with_aix_soname=$lt_cv_with_aix_soname])
  AC_MSG_RESULT([$with_aix_soname])
  if test aix != "$with_aix_soname"; then
    # For the AIX way of multilib, we name the shared archive member
    # based on the bitwidth used, traditionally 'shr.o' or 'shr_64.o',
    # and 'shr.imp' or 'shr_64.imp', respectively, for the Import File.
    # Even when GNU compilers ignore OBJECT_MODE but need '-maix64' flag,
    # the AIX toolchain works better with OBJECT_MODE set (default 32).
    if test 64 = "${OBJECT_MODE-32}"; then
      shared_archive_member_spec=shr_64
    else
      shared_archive_member_spec=shr
    fi
  fi
  ;;
*)
  with_aix_soname=aix
  ;;
esac

_LT_DECL([], [shared_archive_member_spec], [0],
    [Shared archive member basename, for filename based shared library versioning on AIX])dnl
])# _LT_WITH_AIX_SONAME

LT_OPTION_DEFINE([LT_INIT], [aix-soname=aix], [_LT_WITH_AIX_SONAME([aix])])
LT_OPTION_DEFINE([LT_INIT], [aix-soname=both], [_LT_WITH_AIX_SONAME([both])])
LT_OPTION_DEFINE([LT_INIT], [aix-soname=svr4], [_LT_WITH_AIX_SONAME([svr4])])


# _LT_WITH_PIC([MODE])
# --------------------
# implement the --with-pic flag, and support the 'pic-only' and 'no-pic'
# LT_INIT options.
# MODE is either 'yes' or 'no'.  If omitted, it defaults to 'both'.
m4_define([_LT_WITH_PIC],
[AC_ARG_WITH([pic],
    [AS_HELP_STRING([--with-pic@<:@=PKGS@:>@],
	[try to use only PIC/non-PIC objects @<:@default=use both@:>@])],
    [lt_p=${PACKAGE-default}
    case $withval in
    yes|no) pic_mode=$withval ;;
    *)
      pic_mode=default
      # Look at the argument we got.  We use all the common list separators.
      lt_save_ifs=$IFS; IFS=$IFS$PATH_SEPARATOR,
      for lt_pkg in $withval; do
	IFS=$lt_save_ifs
	if test "X$lt_pkg" = "X$lt_p"; then
	  pic_mode=yes
	fi
      done
      IFS=$lt_save_ifs
      ;;
    esac],
    [pic_mode=m4_default([$1], [default])])

_LT_DECL([], [pic_mode], [0], [What type of objects to build])dnl
])# _LT_WITH_PIC

LT_OPTION_DEFINE([LT_INIT], [pic-only], [_LT_WITH_PIC([yes])])
LT_OPTION_DEFINE([LT_INIT], [no-pic], [_LT_WITH_PIC([no])])

# Old name:
AU_DEFUN([AC_LIBTOOL_PICMODE],
[_LT_SET_OPTION([LT_INIT], [pic-only])
AC_DIAGNOSE([obsolete],
[$0: Remove this warning and the call to _LT_SET_OPTION when you
put the 'pic-only' option into LT_INIT's first parameter.])
])

dnl aclocal-1.4 backwards compatibility:
dnl AC_DEFUN([AC_LIBTOOL_PICMODE], [])


m4_define([_LTDL_MODE], [])
LT_OPTION_DEFINE([LTDL_INIT], [nonrecursive],
		 [m4_define([_LTDL_MODE], [nonrecursive])])
LT_OPTION_DEFINE([LTDL_INIT], [recursive],
		 [m4_define([_LTDL_MODE], [recursive])])
LT_OPTION_DEFINE([LTDL_INIT], [subproject],
		 [m4_define([_LTDL_MODE], [subproject])])

m4_define([_LTDL_TYPE], [])
LT_OPTION_DEFINE([LTDL_INIT], [installable],
		 [m4_define([_LTDL_TYPE], [installable])])
LT_OPTION_DEFINE([LTDL_INIT], [convenience],
		 [m4_define([_LTDL_TYPE], [convenience])])

# ltsugar.m4 -- libtool m4 base layer.                         -*-Autoconf-*-
#
# Copyright (C) 2004-2005, 2007-2008, 2011-2015 Free Software
# Foundation, Inc.
# Written by Gary V. Vaughan, 2004
#
# This file is free software; the Free Software Foundation gives
# unlimited permission to copy and/or distribute it, with or without
# modifications, as long as this notice is preserved.

# serial 6 ltsugar.m4

# This is to help aclocal find these macros, as it can't see m4_define.
AC_DEFUN([LTSUGAR_VERSION], [m4_if([0.1])])


# lt_join(SEP, ARG1, [ARG2...])
# -----------------------------
# Produce ARG1SEPARG2...SEPARGn, omitting [] arguments and their
# associated separator.
# Needed until we can rely on m4_join from Autoconf 2.62, since all earlier
# versions in m4sugar had bugs.
m4_define([lt_join],
[m4_if([$#], [1], [],
       [$#], [2], [[$2]],
       [m4_if([$2], [], [], [[$2]_])$0([$1], m4_shift(m4_shift($@)))])])
m4_define([_lt_join],
[m4_if([$#$2], [2], [],
       [m4_if([$2], [], [], [[$1$2]])$0([$1], m4_shift(m4_shift($@)))])])


# lt_car(LIST)
# lt_cdr(LIST)
# ------------
# Manipulate m4 lists.
# These macros are necessary as long as will still need to support
# Autoconf-2.59, which quotes differently.
m4_define([lt_car], [[$1]])
m4_define([lt_cdr],
[m4_if([$#], 0, [m4_fatal([$0: cannot be called without arguments])],
       [$#], 1, [],
       [m4_dquote(m4_shift($@))])])
m4_define([lt_unquote], $1)


# lt_append(MACRO-NAME, STRING, [SEPARATOR])
# ------------------------------------------
# Redefine MACRO-NAME to hold its former content plus 'SEPARATOR''STRING'.
# Note that neither SEPARATOR nor STRING are expanded; they are appended
# to MACRO-NAME as is (leaving the expansion for when MACRO-NAME is invoked).
# No SEPARATOR is output if MACRO-NAME was previously undefined (different
# than defined and empty).
#
# This macro is needed until we can rely on Autoconf 2.62, since earlier
# versions of m4sugar mistakenly expanded SEPARATOR but not STRING.
m4_define([lt_append],
[m4_define([$1],
	   m4_ifdef([$1], [m4_defn([$1])[$3]])[$2])])



# lt_combine(SEP, PREFIX-LIST, INFIX, SUFFIX1, [SUFFIX2...])
# ----------------------------------------------------------
# Produce a SEP delimited list of all paired combinations of elements of
# PREFIX-LIST with SUFFIX1 through SUFFIXn.  Each element of the list
# has the form PREFIXmINFIXSUFFIXn.
# Needed until we can rely on m4_combine added in Autoconf 2.62.
m4_define([lt_combine],
[m4_if(m4_eval([$# > 3]), [1],
       [m4_pushdef([_Lt_sep], [m4_define([_Lt_sep], m4_defn([lt_car]))])]]dnl
[[m4_foreach([_Lt_prefix], [$2],
	     [m4_foreach([_Lt_suffix],
		]m4_dquote(m4_dquote(m4_shift(m4_shift(m4_shift($@)))))[,
	[_Lt_sep([$1])[]m4_defn([_Lt_prefix])[$3]m4_defn([_Lt_suffix])])])])])


# lt_if_append_uniq(MACRO-NAME, VARNAME, [SEPARATOR], [UNIQ], [NOT-UNIQ])
# -----------------------------------------------------------------------
# Iff MACRO-NAME does not yet contain VARNAME, then append it (delimited
# by SEPARATOR if supplied) and expand UNIQ, else NOT-UNIQ.
m4_define([lt_if_append_uniq],
[m4_ifdef([$1],
	  [m4_if(m4_index([$3]m4_defn([$1])[$3], [$3$2$3]), [-1],
		 [lt_append([$1], [$2], [$3])$4],
		 [$5])],
	  [lt_append([$1], [$2], [$3])$4])])


# lt_dict_add(DICT, KEY, VALUE)
# -----------------------------
m4_define([lt_dict_add],
[m4_define([$1($2)], [$3])])


# lt_dict_add_subkey(DICT, KEY, SUBKEY, VALUE)
# --------------------------------------------
m4_define([lt_dict_add_subkey],
[m4_define([$1($2:$3)], [$4])])


# lt_dict_fetch(DICT, KEY, [SUBKEY])
# ----------------------------------
m4_define([lt_dict_fetch],
[m4_ifval([$3],
	m4_ifdef([$1($2:$3)], [m4_defn([$1($2:$3)])]),
    m4_ifdef([$1($2)], [m4_defn([$1($2)])]))])


# lt_if_dict_fetch(DICT, KEY, [SUBKEY], VALUE, IF-TRUE, [IF-FALSE])
# -----------------------------------------------------------------
m4_define([lt_if_dict_fetch],
[m4_if(lt_dict_fetch([$1], [$2], [$3]), [$4],
	[$5],
    [$6])])


# lt_dict_filter(DICT, [SUBKEY], VALUE, [SEPARATOR], KEY, [...])
# --------------------------------------------------------------
m4_define([lt_dict_filter],
[m4_if([$5], [], [],
  [lt_join(m4_quote(m4_default([$4], [[, ]])),
           lt_unquote(m4_split(m4_normalize(m4_foreach(_Lt_key, lt_car([m4_shiftn(4, $@)]),
		      [lt_if_dict_fetch([$1], _Lt_key, [$2], [$3], [_Lt_key ])])))))])[]dnl
])

# ltversion.m4 -- version numbers			-*- Autoconf -*-
#
#   Copyright (C) 2004, 2011-2015 Free Software Foundation, Inc.
#   Written by Scott James Remnant, 2004
#
# This file is free software; the Free Software Foundation gives
# unlimited permission to copy and/or distribute it, with or without
# modifications, as long as this notice is preserved.

# @configure_input@

# serial 4179 ltversion.m4
# This file is part of GNU Libtool

m4_define([LT_PACKAGE_VERSION], [2.4.6])
m4_define([LT_PACKAGE_REVISION], [2.4.6])

AC_DEFUN([LTVERSION_VERSION],
[macro_version='2.4.6'
macro_revision='2.4.6'
_LT_DECL(, macro_version, 0, [Which release of libtool.m4 was used?])
_LT_DECL(, macro_revision, 0)
])

# lt~obsolete.m4 -- aclocal satisfying obsolete definitions.    -*-Autoconf-*-
#
#   Copyright (C) 2004-2005, 2007, 2009, 2011-2015 Free Software
#   Foundation, Inc.
#   Written by Scott James Remnant, 2004.
#
# This file is free software; the Free Software Foundation gives
# unlimited permission to copy and/or distribute it, with or without
# modifications, as long as this notice is preserved.

# serial 5 lt~obsolete.m4

# These exist entirely to fool aclocal when bootstrapping libtool.
#
# In the past libtool.m4 has provided macros via AC_DEFUN (or AU_DEFUN),
# which have later been changed to m4_define as they aren't part of the
# exported API, or moved to Autoconf or Automake where they belong.
#
# The trouble is, aclocal is a bit thick.  It'll see the old AC_DEFUN
# in /usr/share/aclocal/libtool.m4 and remember it, then when it sees us
# using a macro with the same name in our local m4/libtool.m4 it'll
# pull the old libtool.m4 in (it doesn't see our shiny new m4_define
# and doesn't know about Autoconf macros at all.)
#
# So we provide this file, which has a silly filename so it's always
# included after everything else.  This provides aclocal with the
# AC_DEFUNs it wants, but when m4 processes it, it doesn't do anything
# because those macros already exist, or will be overwritten later.
# We use AC_DEFUN over AU_DEFUN for compatibility with aclocal-1.6.
#
# Anytime we withdraw an AC_DEFUN or AU_DEFUN, remember to add it here.
# Yes, that means every name once taken will need to remain here until
# we give up compatibility with versions before 1.7, at which point
# we need to keep only those names which we still refer to.

# This is to help aclocal find these macros, as it can't see m4_define.
AC_DEFUN([LTOBSOLETE_VERSION], [m4_if([1])])

m4_ifndef([AC_LIBTOOL_LINKER_OPTION],	[AC_DEFUN([AC_LIBTOOL_LINKER_OPTION])])
m4_ifndef([AC_PROG_EGREP],		[AC_DEFUN([AC_PROG_EGREP])])
m4_ifndef([_LT_AC_PROG_ECHO_BACKSLASH],	[AC_DEFUN([_LT_AC_PROG_ECHO_BACKSLASH])])
m4_ifndef([_LT_AC_SHELL_INIT],		[AC_DEFUN([_LT_AC_SHELL_INIT])])
m4_ifndef([_LT_AC_SYS_LIBPATH_AIX],	[AC_DEFUN([_LT_AC_SYS_LIBPATH_AIX])])
m4_ifndef([_LT_PROG_LTMAIN],		[AC_DEFUN([_LT_PROG_LTMAIN])])
m4_ifndef([_LT_AC_TAGVAR],		[AC_DEFUN([_LT_AC_TAGVAR])])
m4_ifndef([AC_LTDL_ENABLE_INSTALL],	[AC_DEFUN([AC_LTDL_ENABLE_INSTALL])])
m4_ifndef([AC_LTDL_PREOPEN],		[AC_DEFUN([AC_LTDL_PREOPEN])])
m4_ifndef([_LT_AC_SYS_COMPILER],	[AC_DEFUN([_LT_AC_SYS_COMPILER])])
m4_ifndef([_LT_AC_LOCK],		[AC_DEFUN([_LT_AC_LOCK])])
m4_ifndef([AC_LIBTOOL_SYS_OLD_ARCHIVE],	[AC_DEFUN([AC_LIBTOOL_SYS_OLD_ARCHIVE])])
m4_ifndef([_LT_AC_TRY_DLOPEN_SELF],	[AC_DEFUN([_LT_AC_TRY_DLOPEN_SELF])])
m4_ifndef([AC_LIBTOOL_PROG_CC_C_O],	[AC_DEFUN([AC_LIBTOOL_PROG_CC_C_O])])
m4_ifndef([AC_LIBTOOL_SYS_HARD_LINK_LOCKS], [AC_DEFUN([AC_LIBTOOL_SYS_HARD_LINK_LOCKS])])
m4_ifndef([AC_LIBTOOL_OBJDIR],		[AC_DEFUN([AC_LIBTOOL_OBJDIR])])
m4_ifndef([AC_LTDL_OBJDIR],		[AC_DEFUN([AC_LTDL_OBJDIR])])
m4_ifndef([AC_LIBTOOL_PROG_LD_HARDCODE_LIBPATH], [AC_DEFUN([AC_LIBTOOL_PROG_LD_HARDCODE_LIBPATH])])
m4_ifndef([AC_LIBTOOL_SYS_LIB_STRIP],	[AC_DEFUN([AC_LIBTOOL_SYS_LIB_STRIP])])
m4_ifndef([AC_PATH_MAGIC],		[AC_DEFUN([AC_PATH_MAGIC])])
m4_ifndef([AC_PROG_LD_GNU],		[AC_DEFUN([AC_PROG_LD_GNU])])
m4_ifndef([AC_PROG_LD_RELOAD_FLAG],	[AC_DEFUN([AC_PROG_LD_RELOAD_FLAG])])
m4_ifndef([AC_DEPLIBS_CHECK_METHOD],	[AC_DEFUN([AC_DEPLIBS_CHECK_METHOD])])
m4_ifndef([AC_LIBTOOL_PROG_COMPILER_NO_RTTI], [AC_DEFUN([AC_LIBTOOL_PROG_COMPILER_NO_RTTI])])
m4_ifndef([AC_LIBTOOL_SYS_GLOBAL_SYMBOL_PIPE], [AC_DEFUN([AC_LIBTOOL_SYS_GLOBAL_SYMBOL_PIPE])])
m4_ifndef([AC_LIBTOOL_PROG_COMPILER_PIC], [AC_DEFUN([AC_LIBTOOL_PROG_COMPILER_PIC])])
m4_ifndef([AC_LIBTOOL_PROG_LD_SHLIBS],	[AC_DEFUN([AC_LIBTOOL_PROG_LD_SHLIBS])])
m4_ifndef([AC_LIBTOOL_POSTDEP_PREDEP],	[AC_DEFUN([AC_LIBTOOL_POSTDEP_PREDEP])])
m4_ifndef([LT_AC_PROG_EGREP],		[AC_DEFUN([LT_AC_PROG_EGREP])])
m4_ifndef([LT_AC_PROG_SED],		[AC_DEFUN([LT_AC_PROG_SED])])
m4_ifndef([_LT_CC_BASENAME],		[AC_DEFUN([_LT_CC_BASENAME])])
m4_ifndef([_LT_COMPILER_BOILERPLATE],	[AC_DEFUN([_LT_COMPILER_BOILERPLATE])])
m4_ifndef([_LT_LINKER_BOILERPLATE],	[AC_DEFUN([_LT_LINKER_BOILERPLATE])])
m4_ifndef([_AC_PROG_LIBTOOL],		[AC_DEFUN([_AC_PROG_LIBTOOL])])
m4_ifndef([AC_LIBTOOL_SETUP],		[AC_DEFUN([AC_LIBTOOL_SETUP])])
m4_ifndef([_LT_AC_CHECK_DLFCN],		[AC_DEFUN([_LT_AC_CHECK_DLFCN])])
m4_ifndef([AC_LIBTOOL_SYS_DYNAMIC_LINKER],	[AC_DEFUN([AC_LIBTOOL_SYS_DYNAMIC_LINKER])])
m4_ifndef([_LT_AC_TAGCONFIG],		[AC_DEFUN([_LT_AC_TAGCONFIG])])
m4_ifndef([AC_DISABLE_FAST_INSTALL],	[AC_DEFUN([AC_DISABLE_FAST_INSTALL])])
m4_ifndef([_LT_AC_LANG_CXX],		[AC_DEFUN([_LT_AC_LANG_CXX])])
m4_ifndef([_LT_AC_LANG_F77],		[AC_DEFUN([_LT_AC_LANG_F77])])
m4_ifndef([_LT_AC_LANG_GCJ],		[AC_DEFUN([_LT_AC_LANG_GCJ])])
m4_ifndef([AC_LIBTOOL_LANG_C_CONFIG],	[AC_DEFUN([AC_LIBTOOL_LANG_C_CONFIG])])
m4_ifndef([_LT_AC_LANG_C_CONFIG],	[AC_DEFUN([_LT_AC_LANG_C_CONFIG])])
m4_ifndef([AC_LIBTOOL_LANG_CXX_CONFIG],	[AC_DEFUN([AC_LIBTOOL_LANG_CXX_CONFIG])])
m4_ifndef([_LT_AC_LANG_CXX_CONFIG],	[AC_DEFUN([_LT_AC_LANG_CXX_CONFIG])])
m4_ifndef([AC_LIBTOOL_LANG_F77_CONFIG],	[AC_DEFUN([AC_LIBTOOL_LANG_F77_CONFIG])])
m4_ifndef([_LT_AC_LANG_F77_CONFIG],	[AC_DEFUN([_LT_AC_LANG_F77_CONFIG])])
m4_ifndef([AC_LIBTOOL_LANG_GCJ_CONFIG],	[AC_DEFUN([AC_LIBTOOL_LANG_GCJ_CONFIG])])
m4_ifndef([_LT_AC_LANG_GCJ_CONFIG],	[AC_DEFUN([_LT_AC_LANG_GCJ_CONFIG])])
m4_ifndef([AC_LIBTOOL_LANG_RC_CONFIG],	[AC_DEFUN([AC_LIBTOOL_LANG_RC_CONFIG])])
m4_ifndef([_LT_AC_LANG_RC_CONFIG],	[AC_DEFUN([_LT_AC_LANG_RC_CONFIG])])
m4_ifndef([AC_LIBTOOL_CONFIG],		[AC_DEFUN([AC_LIBTOOL_CONFIG])])
m4_ifndef([_LT_AC_FILE_LTDLL_C],	[AC_DEFUN([_LT_AC_FILE_LTDLL_C])])
m4_ifndef([_LT_REQUIRED_DARWIN_CHECKS],	[AC_DEFUN([_LT_REQUIRED_DARWIN_CHECKS])])
m4_ifndef([_LT_AC_PROG_CXXCPP],		[AC_DEFUN([_LT_AC_PROG_CXXCPP])])
m4_ifndef([_LT_PREPARE_SED_QUOTE_VARS],	[AC_DEFUN([_LT_PREPARE_SED_QUOTE_VARS])])
m4_ifndef([_LT_PROG_ECHO_BACKSLASH],	[AC_DEFUN([_LT_PROG_ECHO_BACKSLASH])])
m4_ifndef([_LT_PROG_F77],		[AC_DEFUN([_LT_PROG_F77])])
m4_ifndef([_LT_PROG_FC],		[AC_DEFUN([_LT_PROG_FC])])
m4_ifndef([_LT_PROG_CXX],		[AC_DEFUN([_LT_PROG_CXX])])

# nls.m4 serial 5 (gettext-0.18)
dnl Copyright (C) 1995-2003, 2005-2006, 2008-2014, 2016 Free Software
dnl Foundation, Inc.
dnl This file is free software; the Free Software Foundation
dnl gives unlimited permission to copy and/or distribute it,
dnl with or without modifications, as long as this notice is preserved.
dnl
dnl This file can be used in projects which are not available under
dnl the GNU General Public License or the GNU Library General Public
dnl License but which still want to provide support for the GNU gettext
dnl functionality.
dnl Please note that the actual code of the GNU gettext library is covered
dnl by the GNU Library General Public License, and the rest of the GNU
dnl gettext package is covered by the GNU General Public License.
dnl They are *not* in the public domain.

dnl Authors:
dnl   Ulrich Drepper <drepper@cygnus.com>, 1995-2000.
dnl   Bruno Haible <haible@clisp.cons.org>, 2000-2003.

AC_PREREQ([2.50])

AC_DEFUN([AM_NLS],
[
  AC_MSG_CHECKING([whether NLS is requested])
  dnl Default is enabled NLS
  AC_ARG_ENABLE([nls],
    [  --disable-nls           do not use Native Language Support],
    USE_NLS=$enableval, USE_NLS=yes)
  AC_MSG_RESULT([$USE_NLS])
  AC_SUBST([USE_NLS])
])

# po.m4 serial 24 (gettext-0.19)
dnl Copyright (C) 1995-2014, 2016 Free Software Foundation, Inc.
dnl This file is free software; the Free Software Foundation
dnl gives unlimited permission to copy and/or distribute it,
dnl with or without modifications, as long as this notice is preserved.
dnl
dnl This file can be used in projects which are not available under
dnl the GNU General Public License or the GNU Library General Public
dnl License but which still want to provide support for the GNU gettext
dnl functionality.
dnl Please note that the actual code of the GNU gettext library is covered
dnl by the GNU Library General Public License, and the rest of the GNU
dnl gettext package is covered by the GNU General Public License.
dnl They are *not* in the public domain.

dnl Authors:
dnl   Ulrich Drepper <drepper@cygnus.com>, 1995-2000.
dnl   Bruno Haible <haible@clisp.cons.org>, 2000-2003.

AC_PREREQ([2.60])

dnl Checks for all prerequisites of the po subdirectory.
AC_DEFUN([AM_PO_SUBDIRS],
[
  AC_REQUIRE([AC_PROG_MAKE_SET])dnl
  AC_REQUIRE([AC_PROG_INSTALL])dnl
  AC_REQUIRE([AC_PROG_MKDIR_P])dnl
  AC_REQUIRE([AC_PROG_SED])dnl
  AC_REQUIRE([AM_NLS])dnl

  dnl Release version of the gettext macros. This is used to ensure that
  dnl the gettext macros and po/Makefile.in.in are in sync.
  AC_SUBST([GETTEXT_MACRO_VERSION], [0.19])

  dnl Perform the following tests also if --disable-nls has been given,
  dnl because they are needed for "make dist" to work.

  dnl Search for GNU msgfmt in the PATH.
  dnl The first test excludes Solaris msgfmt and early GNU msgfmt versions.
  dnl The second test excludes FreeBSD msgfmt.
  AM_PATH_PROG_WITH_TEST(MSGFMT, msgfmt,
    [$ac_dir/$ac_word --statistics /dev/null >&]AS_MESSAGE_LOG_FD[ 2>&1 &&
     (if $ac_dir/$ac_word --statistics /dev/null 2>&1 >/dev/null | grep usage >/dev/null; then exit 1; else exit 0; fi)],
    :)
  AC_PATH_PROG([GMSGFMT], [gmsgfmt], [$MSGFMT])

  dnl Test whether it is GNU msgfmt >= 0.15.
changequote(,)dnl
  case `$MSGFMT --version | sed 1q | sed -e 's,^[^0-9]*,,'` in
    '' | 0.[0-9] | 0.[0-9].* | 0.1[0-4] | 0.1[0-4].*) MSGFMT_015=: ;;
    *) MSGFMT_015=$MSGFMT ;;
  esac
changequote([,])dnl
  AC_SUBST([MSGFMT_015])
changequote(,)dnl
  case `$GMSGFMT --version | sed 1q | sed -e 's,^[^0-9]*,,'` in
    '' | 0.[0-9] | 0.[0-9].* | 0.1[0-4] | 0.1[0-4].*) GMSGFMT_015=: ;;
    *) GMSGFMT_015=$GMSGFMT ;;
  esac
changequote([,])dnl
  AC_SUBST([GMSGFMT_015])

  dnl Search for GNU xgettext 0.12 or newer in the PATH.
  dnl The first test excludes Solaris xgettext and early GNU xgettext versions.
  dnl The second test excludes FreeBSD xgettext.
  AM_PATH_PROG_WITH_TEST(XGETTEXT, xgettext,
    [$ac_dir/$ac_word --omit-header --copyright-holder= --msgid-bugs-address= /dev/null >&]AS_MESSAGE_LOG_FD[ 2>&1 &&
     (if $ac_dir/$ac_word --omit-header --copyright-holder= --msgid-bugs-address= /dev/null 2>&1 >/dev/null | grep usage >/dev/null; then exit 1; else exit 0; fi)],
    :)
  dnl Remove leftover from FreeBSD xgettext call.
  rm -f messages.po

  dnl Test whether it is GNU xgettext >= 0.15.
changequote(,)dnl
  case `$XGETTEXT --version | sed 1q | sed -e 's,^[^0-9]*,,'` in
    '' | 0.[0-9] | 0.[0-9].* | 0.1[0-4] | 0.1[0-4].*) XGETTEXT_015=: ;;
    *) XGETTEXT_015=$XGETTEXT ;;
  esac
changequote([,])dnl
  AC_SUBST([XGETTEXT_015])

  dnl Search for GNU msgmerge 0.11 or newer in the PATH.
  AM_PATH_PROG_WITH_TEST(MSGMERGE, msgmerge,
    [$ac_dir/$ac_word --update -q /dev/null /dev/null >&]AS_MESSAGE_LOG_FD[ 2>&1], :)

  dnl Installation directories.
  dnl Autoconf >= 2.60 defines localedir. For older versions of autoconf, we
  dnl have to define it here, so that it can be used in po/Makefile.
  test -n "$localedir" || localedir='${datadir}/locale'
  AC_SUBST([localedir])

  dnl Support for AM_XGETTEXT_OPTION.
  test -n "${XGETTEXT_EXTRA_OPTIONS+set}" || XGETTEXT_EXTRA_OPTIONS=
  AC_SUBST([XGETTEXT_EXTRA_OPTIONS])

  AC_CONFIG_COMMANDS([po-directories], [[
    for ac_file in $CONFIG_FILES; do
      # Support "outfile[:infile[:infile...]]"
      case "$ac_file" in
        *:*) ac_file=`echo "$ac_file"|sed 's%:.*%%'` ;;
      esac
      # PO directories have a Makefile.in generated from Makefile.in.in.
      case "$ac_file" in */Makefile.in)
        # Adjust a relative srcdir.
        ac_dir=`echo "$ac_file"|sed 's%/[^/][^/]*$%%'`
        ac_dir_suffix=/`echo "$ac_dir"|sed 's%^\./%%'`
        ac_dots=`echo "$ac_dir_suffix"|sed 's%/[^/]*%../%g'`
        # In autoconf-2.13 it is called $ac_given_srcdir.
        # In autoconf-2.50 it is called $srcdir.
        test -n "$ac_given_srcdir" || ac_given_srcdir="$srcdir"
        case "$ac_given_srcdir" in
          .)  top_srcdir=`echo $ac_dots|sed 's%/$%%'` ;;
          /*) top_srcdir="$ac_given_srcdir" ;;
          *)  top_srcdir="$ac_dots$ac_given_srcdir" ;;
        esac
        # Treat a directory as a PO directory if and only if it has a
        # POTFILES.in file. This allows packages to have multiple PO
        # directories under different names or in different locations.
        if test -f "$ac_given_srcdir/$ac_dir/POTFILES.in"; then
          rm -f "$ac_dir/POTFILES"
          test -n "$as_me" && echo "$as_me: creating $ac_dir/POTFILES" || echo "creating $ac_dir/POTFILES"
          gt_tab=`printf '\t'`
          cat "$ac_given_srcdir/$ac_dir/POTFILES.in" | sed -e "/^#/d" -e "/^[ ${gt_tab}]*\$/d" -e "s,.*,     $top_srcdir/& \\\\," | sed -e "\$s/\(.*\) \\\\/\1/" > "$ac_dir/POTFILES"
          POMAKEFILEDEPS="POTFILES.in"
          # ALL_LINGUAS, POFILES, UPDATEPOFILES, DUMMYPOFILES, GMOFILES depend
          # on $ac_dir but don't depend on user-specified configuration
          # parameters.
          if test -f "$ac_given_srcdir/$ac_dir/LINGUAS"; then
            # The LINGUAS file contains the set of available languages.
            if test -n "$OBSOLETE_ALL_LINGUAS"; then
              test -n "$as_me" && echo "$as_me: setting ALL_LINGUAS in configure.in is obsolete" || echo "setting ALL_LINGUAS in configure.in is obsolete"
            fi
            ALL_LINGUAS_=`sed -e "/^#/d" -e "s/#.*//" "$ac_given_srcdir/$ac_dir/LINGUAS"`
            # Hide the ALL_LINGUAS assignment from automake < 1.5.
            eval 'ALL_LINGUAS''=$ALL_LINGUAS_'
            POMAKEFILEDEPS="$POMAKEFILEDEPS LINGUAS"
          else
            # The set of available languages was given in configure.in.
            # Hide the ALL_LINGUAS assignment from automake < 1.5.
            eval 'ALL_LINGUAS''=$OBSOLETE_ALL_LINGUAS'
          fi
          # Compute POFILES
          # as      $(foreach lang, $(ALL_LINGUAS), $(srcdir)/$(lang).po)
          # Compute UPDATEPOFILES
          # as      $(foreach lang, $(ALL_LINGUAS), $(lang).po-update)
          # Compute DUMMYPOFILES
          # as      $(foreach lang, $(ALL_LINGUAS), $(lang).nop)
          # Compute GMOFILES
          # as      $(foreach lang, $(ALL_LINGUAS), $(srcdir)/$(lang).gmo)
          case "$ac_given_srcdir" in
            .) srcdirpre= ;;
            *) srcdirpre='$(srcdir)/' ;;
          esac
          POFILES=
          UPDATEPOFILES=
          DUMMYPOFILES=
          GMOFILES=
          for lang in $ALL_LINGUAS; do
            POFILES="$POFILES $srcdirpre$lang.po"
            UPDATEPOFILES="$UPDATEPOFILES $lang.po-update"
            DUMMYPOFILES="$DUMMYPOFILES $lang.nop"
            GMOFILES="$GMOFILES $srcdirpre$lang.gmo"
          done
          # CATALOGS depends on both $ac_dir and the user's LINGUAS
          # environment variable.
          INST_LINGUAS=
          if test -n "$ALL_LINGUAS"; then
            for presentlang in $ALL_LINGUAS; do
              useit=no
              if test "%UNSET%" != "$LINGUAS"; then
                desiredlanguages="$LINGUAS"
              else
                desiredlanguages="$ALL_LINGUAS"
              fi
              for desiredlang in $desiredlanguages; do
                # Use the presentlang catalog if desiredlang is
                #   a. equal to presentlang, or
                #   b. a variant of presentlang (because in this case,
                #      presentlang can be used as a fallback for messages
                #      which are not translated in the desiredlang catalog).
                case "$desiredlang" in
                  "$presentlang"*) useit=yes;;
                esac
              done
              if test $useit = yes; then
                INST_LINGUAS="$INST_LINGUAS $presentlang"
              fi
            done
          fi
          CATALOGS=
          if test -n "$INST_LINGUAS"; then
            for lang in $INST_LINGUAS; do
              CATALOGS="$CATALOGS $lang.gmo"
            done
          fi
          test -n "$as_me" && echo "$as_me: creating $ac_dir/Makefile" || echo "creating $ac_dir/Makefile"
          sed -e "/^POTFILES =/r $ac_dir/POTFILES" -e "/^# Makevars/r $ac_given_srcdir/$ac_dir/Makevars" -e "s|@POFILES@|$POFILES|g" -e "s|@UPDATEPOFILES@|$UPDATEPOFILES|g" -e "s|@DUMMYPOFILES@|$DUMMYPOFILES|g" -e "s|@GMOFILES@|$GMOFILES|g" -e "s|@CATALOGS@|$CATALOGS|g" -e "s|@POMAKEFILEDEPS@|$POMAKEFILEDEPS|g" "$ac_dir/Makefile.in" > "$ac_dir/Makefile"
          for f in "$ac_given_srcdir/$ac_dir"/Rules-*; do
            if test -f "$f"; then
              case "$f" in
                *.orig | *.bak | *~) ;;
                *) cat "$f" >> "$ac_dir/Makefile" ;;
              esac
            fi
          done
        fi
        ;;
      esac
    done]],
   [# Capture the value of obsolete ALL_LINGUAS because we need it to compute
    # POFILES, UPDATEPOFILES, DUMMYPOFILES, GMOFILES, CATALOGS. But hide it
    # from automake < 1.5.
    eval 'OBSOLETE_ALL_LINGUAS''="$ALL_LINGUAS"'
    # Capture the value of LINGUAS because we need it to compute CATALOGS.
    LINGUAS="${LINGUAS-%UNSET%}"
   ])
])

dnl Postprocesses a Makefile in a directory containing PO files.
AC_DEFUN([AM_POSTPROCESS_PO_MAKEFILE],
[
  # When this code is run, in config.status, two variables have already been
  # set:
  # - OBSOLETE_ALL_LINGUAS is the value of LINGUAS set in configure.in,
  # - LINGUAS is the value of the environment variable LINGUAS at configure
  #   time.

changequote(,)dnl
  # Adjust a relative srcdir.
  ac_dir=`echo "$ac_file"|sed 's%/[^/][^/]*$%%'`
  ac_dir_suffix=/`echo "$ac_dir"|sed 's%^\./%%'`
  ac_dots=`echo "$ac_dir_suffix"|sed 's%/[^/]*%../%g'`
  # In autoconf-2.13 it is called $ac_given_srcdir.
  # In autoconf-2.50 it is called $srcdir.
  test -n "$ac_given_srcdir" || ac_given_srcdir="$srcdir"
  case "$ac_given_srcdir" in
    .)  top_srcdir=`echo $ac_dots|sed 's%/$%%'` ;;
    /*) top_srcdir="$ac_given_srcdir" ;;
    *)  top_srcdir="$ac_dots$ac_given_srcdir" ;;
  esac

  # Find a way to echo strings without interpreting backslash.
  if test "X`(echo '\t') 2>/dev/null`" = 'X\t'; then
    gt_echo='echo'
  else
    if test "X`(printf '%s\n' '\t') 2>/dev/null`" = 'X\t'; then
      gt_echo='printf %s\n'
    else
      echo_func () {
        cat <<EOT
$*
EOT
      }
      gt_echo='echo_func'
    fi
  fi

  # A sed script that extracts the value of VARIABLE from a Makefile.
  tab=`printf '\t'`
  sed_x_variable='
# Test if the hold space is empty.
x
s/P/P/
x
ta
# Yes it was empty. Look if we have the expected variable definition.
/^['"${tab}"' ]*VARIABLE['"${tab}"' ]*=/{
  # Seen the first line of the variable definition.
  s/^['"${tab}"' ]*VARIABLE['"${tab}"' ]*=//
  ba
}
bd
:a
# Here we are processing a line from the variable definition.
# Remove comment, more precisely replace it with a space.
s/#.*$/ /
# See if the line ends in a backslash.
tb
:b
s/\\$//
# Print the line, without the trailing backslash.
p
tc
# There was no trailing backslash. The end of the variable definition is
# reached. Clear the hold space.
s/^.*$//
x
bd
:c
# A trailing backslash means that the variable definition continues in the
# next line. Put a nonempty string into the hold space to indicate this.
s/^.*$/P/
x
:d
'
changequote([,])dnl

  # Set POTFILES to the value of the Makefile variable POTFILES.
  sed_x_POTFILES=`$gt_echo "$sed_x_variable" | sed -e '/^ *#/d' -e 's/VARIABLE/POTFILES/g'`
  POTFILES=`sed -n -e "$sed_x_POTFILES" < "$ac_file"`
  # Compute POTFILES_DEPS as
  #   $(foreach file, $(POTFILES), $(top_srcdir)/$(file))
  POTFILES_DEPS=
  for file in $POTFILES; do
    POTFILES_DEPS="$POTFILES_DEPS "'$(top_srcdir)/'"$file"
  done
  POMAKEFILEDEPS=""

  if test -n "$OBSOLETE_ALL_LINGUAS"; then
    test -n "$as_me" && echo "$as_me: setting ALL_LINGUAS in configure.in is obsolete" || echo "setting ALL_LINGUAS in configure.in is obsolete"
  fi
  if test -f "$ac_given_srcdir/$ac_dir/LINGUAS"; then
    # The LINGUAS file contains the set of available languages.
    ALL_LINGUAS_=`sed -e "/^#/d" -e "s/#.*//" "$ac_given_srcdir/$ac_dir/LINGUAS"`
    POMAKEFILEDEPS="$POMAKEFILEDEPS LINGUAS"
  else
    # Set ALL_LINGUAS to the value of the Makefile variable LINGUAS.
    sed_x_LINGUAS=`$gt_echo "$sed_x_variable" | sed -e '/^ *#/d' -e 's/VARIABLE/LINGUAS/g'`
    ALL_LINGUAS_=`sed -n -e "$sed_x_LINGUAS" < "$ac_file"`
  fi
  # Hide the ALL_LINGUAS assignment from automake < 1.5.
  eval 'ALL_LINGUAS''=$ALL_LINGUAS_'
  # Compute POFILES
  # as      $(foreach lang, $(ALL_LINGUAS), $(srcdir)/$(lang).po)
  # Compute UPDATEPOFILES
  # as      $(foreach lang, $(ALL_LINGUAS), $(lang).po-update)
  # Compute DUMMYPOFILES
  # as      $(foreach lang, $(ALL_LINGUAS), $(lang).nop)
  # Compute GMOFILES
  # as      $(foreach lang, $(ALL_LINGUAS), $(srcdir)/$(lang).gmo)
  # Compute PROPERTIESFILES
  # as      $(foreach lang, $(ALL_LINGUAS), $(top_srcdir)/$(DOMAIN)_$(lang).properties)
  # Compute CLASSFILES
  # as      $(foreach lang, $(ALL_LINGUAS), $(top_srcdir)/$(DOMAIN)_$(lang).class)
  # Compute QMFILES
  # as      $(foreach lang, $(ALL_LINGUAS), $(srcdir)/$(lang).qm)
  # Compute MSGFILES
  # as      $(foreach lang, $(ALL_LINGUAS), $(srcdir)/$(frob $(lang)).msg)
  # Compute RESOURCESDLLFILES
  # as      $(foreach lang, $(ALL_LINGUAS), $(srcdir)/$(frob $(lang))/$(DOMAIN).resources.dll)
  case "$ac_given_srcdir" in
    .) srcdirpre= ;;
    *) srcdirpre='$(srcdir)/' ;;
  esac
  POFILES=
  UPDATEPOFILES=
  DUMMYPOFILES=
  GMOFILES=
  PROPERTIESFILES=
  CLASSFILES=
  QMFILES=
  MSGFILES=
  RESOURCESDLLFILES=
  for lang in $ALL_LINGUAS; do
    POFILES="$POFILES $srcdirpre$lang.po"
    UPDATEPOFILES="$UPDATEPOFILES $lang.po-update"
    DUMMYPOFILES="$DUMMYPOFILES $lang.nop"
    GMOFILES="$GMOFILES $srcdirpre$lang.gmo"
    PROPERTIESFILES="$PROPERTIESFILES \$(top_srcdir)/\$(DOMAIN)_$lang.properties"
    CLASSFILES="$CLASSFILES \$(top_srcdir)/\$(DOMAIN)_$lang.class"
    QMFILES="$QMFILES $srcdirpre$lang.qm"
    frobbedlang=`echo $lang | sed -e 's/\..*$//' -e 'y/ABCDEFGHIJKLMNOPQRSTUVWXYZ/abcdefghijklmnopqrstuvwxyz/'`
    MSGFILES="$MSGFILES $srcdirpre$frobbedlang.msg"
    frobbedlang=`echo $lang | sed -e 's/_/-/g' -e 's/^sr-CS/sr-SP/' -e 's/@latin$/-Latn/' -e 's/@cyrillic$/-Cyrl/' -e 's/^sr-SP$/sr-SP-Latn/' -e 's/^uz-UZ$/uz-UZ-Latn/'`
    RESOURCESDLLFILES="$RESOURCESDLLFILES $srcdirpre$frobbedlang/\$(DOMAIN).resources.dll"
  done
  # CATALOGS depends on both $ac_dir and the user's LINGUAS
  # environment variable.
  INST_LINGUAS=
  if test -n "$ALL_LINGUAS"; then
    for presentlang in $ALL_LINGUAS; do
      useit=no
      if test "%UNSET%" != "$LINGUAS"; then
        desiredlanguages="$LINGUAS"
      else
        desiredlanguages="$ALL_LINGUAS"
      fi
      for desiredlang in $desiredlanguages; do
        # Use the presentlang catalog if desiredlang is
        #   a. equal to presentlang, or
        #   b. a variant of presentlang (because in this case,
        #      presentlang can be used as a fallback for messages
        #      which are not translated in the desiredlang catalog).
        case "$desiredlang" in
          "$presentlang"*) useit=yes;;
        esac
      done
      if test $useit = yes; then
        INST_LINGUAS="$INST_LINGUAS $presentlang"
      fi
    done
  fi
  CATALOGS=
  JAVACATALOGS=
  QTCATALOGS=
  TCLCATALOGS=
  CSHARPCATALOGS=
  if test -n "$INST_LINGUAS"; then
    for lang in $INST_LINGUAS; do
      CATALOGS="$CATALOGS $lang.gmo"
      JAVACATALOGS="$JAVACATALOGS \$(DOMAIN)_$lang.properties"
      QTCATALOGS="$QTCATALOGS $lang.qm"
      frobbedlang=`echo $lang | sed -e 's/\..*$//' -e 'y/ABCDEFGHIJKLMNOPQRSTUVWXYZ/abcdefghijklmnopqrstuvwxyz/'`
      TCLCATALOGS="$TCLCATALOGS $frobbedlang.msg"
      frobbedlang=`echo $lang | sed -e 's/_/-/g' -e 's/^sr-CS/sr-SP/' -e 's/@latin$/-Latn/' -e 's/@cyrillic$/-Cyrl/' -e 's/^sr-SP$/sr-SP-Latn/' -e 's/^uz-UZ$/uz-UZ-Latn/'`
      CSHARPCATALOGS="$CSHARPCATALOGS $frobbedlang/\$(DOMAIN).resources.dll"
    done
  fi

  sed -e "s|@POTFILES_DEPS@|$POTFILES_DEPS|g" -e "s|@POFILES@|$POFILES|g" -e "s|@UPDATEPOFILES@|$UPDATEPOFILES|g" -e "s|@DUMMYPOFILES@|$DUMMYPOFILES|g" -e "s|@GMOFILES@|$GMOFILES|g" -e "s|@PROPERTIESFILES@|$PROPERTIESFILES|g" -e "s|@CLASSFILES@|$CLASSFILES|g" -e "s|@QMFILES@|$QMFILES|g" -e "s|@MSGFILES@|$MSGFILES|g" -e "s|@RESOURCESDLLFILES@|$RESOURCESDLLFILES|g" -e "s|@CATALOGS@|$CATALOGS|g" -e "s|@JAVACATALOGS@|$JAVACATALOGS|g" -e "s|@QTCATALOGS@|$QTCATALOGS|g" -e "s|@TCLCATALOGS@|$TCLCATALOGS|g" -e "s|@CSHARPCATALOGS@|$CSHARPCATALOGS|g" -e 's,^#distdir:,distdir:,' < "$ac_file" > "$ac_file.tmp"
  tab=`printf '\t'`
  if grep -l '@TCLCATALOGS@' "$ac_file" > /dev/null; then
    # Add dependencies that cannot be formulated as a simple suffix rule.
    for lang in $ALL_LINGUAS; do
      frobbedlang=`echo $lang | sed -e 's/\..*$//' -e 'y/ABCDEFGHIJKLMNOPQRSTUVWXYZ/abcdefghijklmnopqrstuvwxyz/'`
      cat >> "$ac_file.tmp" <<EOF
$frobbedlang.msg: $lang.po
${tab}@echo "\$(MSGFMT) -c --tcl -d \$(srcdir) -l $lang $srcdirpre$lang.po"; \
${tab}\$(MSGFMT) -c --tcl -d "\$(srcdir)" -l $lang $srcdirpre$lang.po || { rm -f "\$(srcdir)/$frobbedlang.msg"; exit 1; }
EOF
    done
  fi
  if grep -l '@CSHARPCATALOGS@' "$ac_file" > /dev/null; then
    # Add dependencies that cannot be formulated as a simple suffix rule.
    for lang in $ALL_LINGUAS; do
      frobbedlang=`echo $lang | sed -e 's/_/-/g' -e 's/^sr-CS/sr-SP/' -e 's/@latin$/-Latn/' -e 's/@cyrillic$/-Cyrl/' -e 's/^sr-SP$/sr-SP-Latn/' -e 's/^uz-UZ$/uz-UZ-Latn/'`
      cat >> "$ac_file.tmp" <<EOF
$frobbedlang/\$(DOMAIN).resources.dll: $lang.po
${tab}@echo "\$(MSGFMT) -c --csharp -d \$(srcdir) -l $lang $srcdirpre$lang.po -r \$(DOMAIN)"; \
${tab}\$(MSGFMT) -c --csharp -d "\$(srcdir)" -l $lang $srcdirpre$lang.po -r "\$(DOMAIN)" || { rm -f "\$(srcdir)/$frobbedlang.msg"; exit 1; }
EOF
    done
  fi
  if test -n "$POMAKEFILEDEPS"; then
    cat >> "$ac_file.tmp" <<EOF
Makefile: $POMAKEFILEDEPS
EOF
  fi
  mv "$ac_file.tmp" "$ac_file"
])

dnl Initializes the accumulator used by AM_XGETTEXT_OPTION.
AC_DEFUN([AM_XGETTEXT_OPTION_INIT],
[
  XGETTEXT_EXTRA_OPTIONS=
])

dnl Registers an option to be passed to xgettext in the po subdirectory.
AC_DEFUN([AM_XGETTEXT_OPTION],
[
  AC_REQUIRE([AM_XGETTEXT_OPTION_INIT])
  XGETTEXT_EXTRA_OPTIONS="$XGETTEXT_EXTRA_OPTIONS $1"
])

# printf-posix.m4 serial 6 (gettext-0.18.2)
dnl Copyright (C) 2003, 2007, 2009-2016 Free Software Foundation, Inc.
dnl This file is free software; the Free Software Foundation
dnl gives unlimited permission to copy and/or distribute it,
dnl with or without modifications, as long as this notice is preserved.

dnl From Bruno Haible.
dnl Test whether the printf() function supports POSIX/XSI format strings with
dnl positions.

AC_DEFUN([gt_PRINTF_POSIX],
[
  AC_REQUIRE([AC_PROG_CC])
  AC_CACHE_CHECK([whether printf() supports POSIX/XSI format strings],
    gt_cv_func_printf_posix,
    [
      AC_RUN_IFELSE(
        [AC_LANG_SOURCE([[
#include <stdio.h>
#include <string.h>
/* The string "%2$d %1$d", with dollar characters protected from the shell's
   dollar expansion (possibly an autoconf bug).  */
static char format[] = { '%', '2', '$', 'd', ' ', '%', '1', '$', 'd', '\0' };
static char buf[100];
int main ()
{
  sprintf (buf, format, 33, 55);
  return (strcmp (buf, "55 33") != 0);
}]])],
        [gt_cv_func_printf_posix=yes],
        [gt_cv_func_printf_posix=no],
        [
          AC_EGREP_CPP([notposix], [
#if defined __NetBSD__ || defined __BEOS__ || defined _MSC_VER || defined __MINGW32__ || defined __CYGWIN__
  notposix
#endif
            ],
            [gt_cv_func_printf_posix="guessing no"],
            [gt_cv_func_printf_posix="guessing yes"])
        ])
    ])
  case $gt_cv_func_printf_posix in
    *yes)
      AC_DEFINE([HAVE_POSIX_PRINTF], [1],
        [Define if your printf() function supports format strings with positions.])
      ;;
  esac
])

# progtest.m4 serial 7 (gettext-0.18.2)
dnl Copyright (C) 1996-2003, 2005, 2008-2016 Free Software Foundation, Inc.
dnl This file is free software; the Free Software Foundation
dnl gives unlimited permission to copy and/or distribute it,
dnl with or without modifications, as long as this notice is preserved.
dnl
dnl This file can be used in projects which are not available under
dnl the GNU General Public License or the GNU Library General Public
dnl License but which still want to provide support for the GNU gettext
dnl functionality.
dnl Please note that the actual code of the GNU gettext library is covered
dnl by the GNU Library General Public License, and the rest of the GNU
dnl gettext package is covered by the GNU General Public License.
dnl They are *not* in the public domain.

dnl Authors:
dnl   Ulrich Drepper <drepper@cygnus.com>, 1996.

AC_PREREQ([2.50])

# Search path for a program which passes the given test.

dnl AM_PATH_PROG_WITH_TEST(VARIABLE, PROG-TO-CHECK-FOR,
dnl   TEST-PERFORMED-ON-FOUND_PROGRAM [, VALUE-IF-NOT-FOUND [, PATH]])
AC_DEFUN([AM_PATH_PROG_WITH_TEST],
[
# Prepare PATH_SEPARATOR.
# The user is always right.
if test "${PATH_SEPARATOR+set}" != set; then
  # Determine PATH_SEPARATOR by trying to find /bin/sh in a PATH which
  # contains only /bin. Note that ksh looks also at the FPATH variable,
  # so we have to set that as well for the test.
  PATH_SEPARATOR=:
  (PATH='/bin;/bin'; FPATH=$PATH; sh -c :) >/dev/null 2>&1 \
    && { (PATH='/bin:/bin'; FPATH=$PATH; sh -c :) >/dev/null 2>&1 \
           || PATH_SEPARATOR=';'
       }
fi

# Find out how to test for executable files. Don't use a zero-byte file,
# as systems may use methods other than mode bits to determine executability.
cat >conf$$.file <<_ASEOF
#! /bin/sh
exit 0
_ASEOF
chmod +x conf$$.file
if test -x conf$$.file >/dev/null 2>&1; then
  ac_executable_p="test -x"
else
  ac_executable_p="test -f"
fi
rm -f conf$$.file

# Extract the first word of "$2", so it can be a program name with args.
set dummy $2; ac_word=[$]2
AC_MSG_CHECKING([for $ac_word])
AC_CACHE_VAL([ac_cv_path_$1],
[case "[$]$1" in
  [[\\/]]* | ?:[[\\/]]*)
    ac_cv_path_$1="[$]$1" # Let the user override the test with a path.
    ;;
  *)
    ac_save_IFS="$IFS"; IFS=$PATH_SEPARATOR
    for ac_dir in ifelse([$5], , $PATH, [$5]); do
      IFS="$ac_save_IFS"
      test -z "$ac_dir" && ac_dir=.
      for ac_exec_ext in '' $ac_executable_extensions; do
        if $ac_executable_p "$ac_dir/$ac_word$ac_exec_ext"; then
          echo "$as_me: trying $ac_dir/$ac_word..." >&AS_MESSAGE_LOG_FD
          if [$3]; then
            ac_cv_path_$1="$ac_dir/$ac_word$ac_exec_ext"
            break 2
          fi
        fi
      done
    done
    IFS="$ac_save_IFS"
dnl If no 4th arg is given, leave the cache variable unset,
dnl so AC_PATH_PROGS will keep looking.
ifelse([$4], , , [  test -z "[$]ac_cv_path_$1" && ac_cv_path_$1="$4"
])dnl
    ;;
esac])dnl
$1="$ac_cv_path_$1"
if test ifelse([$4], , [-n "[$]$1"], ["[$]$1" != "$4"]); then
  AC_MSG_RESULT([$][$1])
else
  AC_MSG_RESULT([no])
fi
AC_SUBST([$1])dnl
])

# size_max.m4 serial 10
dnl Copyright (C) 2003, 2005-2006, 2008-2016 Free Software Foundation, Inc.
dnl This file is free software; the Free Software Foundation
dnl gives unlimited permission to copy and/or distribute it,
dnl with or without modifications, as long as this notice is preserved.

dnl From Bruno Haible.

AC_DEFUN([gl_SIZE_MAX],
[
  AC_CHECK_HEADERS([stdint.h])
  dnl First test whether the system already has SIZE_MAX.
  AC_CACHE_CHECK([for SIZE_MAX], [gl_cv_size_max], [
    gl_cv_size_max=
    AC_EGREP_CPP([Found it], [
#include <limits.h>
#if HAVE_STDINT_H
#include <stdint.h>
#endif
#ifdef SIZE_MAX
Found it
#endif
], [gl_cv_size_max=yes])
    if test -z "$gl_cv_size_max"; then
      dnl Define it ourselves. Here we assume that the type 'size_t' is not wider
      dnl than the type 'unsigned long'. Try hard to find a definition that can
      dnl be used in a preprocessor #if, i.e. doesn't contain a cast.
      AC_COMPUTE_INT([size_t_bits_minus_1], [sizeof (size_t) * CHAR_BIT - 1],
        [#include <stddef.h>
#include <limits.h>], [size_t_bits_minus_1=])
      AC_COMPUTE_INT([fits_in_uint], [sizeof (size_t) <= sizeof (unsigned int)],
        [#include <stddef.h>], [fits_in_uint=])
      if test -n "$size_t_bits_minus_1" && test -n "$fits_in_uint"; then
        if test $fits_in_uint = 1; then
          dnl Even though SIZE_MAX fits in an unsigned int, it must be of type
          dnl 'unsigned long' if the type 'size_t' is the same as 'unsigned long'.
          AC_COMPILE_IFELSE(
            [AC_LANG_PROGRAM(
               [[#include <stddef.h>
                 extern size_t foo;
                 extern unsigned long foo;
               ]],
               [[]])],
            [fits_in_uint=0])
        fi
        dnl We cannot use 'expr' to simplify this expression, because 'expr'
        dnl works only with 'long' integers in the host environment, while we
        dnl might be cross-compiling from a 32-bit platform to a 64-bit platform.
        if test $fits_in_uint = 1; then
          gl_cv_size_max="(((1U << $size_t_bits_minus_1) - 1) * 2 + 1)"
        else
          gl_cv_size_max="(((1UL << $size_t_bits_minus_1) - 1) * 2 + 1)"
        fi
      else
        dnl Shouldn't happen, but who knows...
        gl_cv_size_max='((size_t)~(size_t)0)'
      fi
    fi
  ])
  if test "$gl_cv_size_max" != yes; then
    AC_DEFINE_UNQUOTED([SIZE_MAX], [$gl_cv_size_max],
      [Define as the maximum value of type 'size_t', if the system doesn't define it.])
  fi
  dnl Don't redefine SIZE_MAX in config.h if config.h is re-included after
  dnl <stdint.h>. Remember that the #undef in AH_VERBATIM gets replaced with
  dnl #define by AC_DEFINE_UNQUOTED.
  AH_VERBATIM([SIZE_MAX],
[/* Define as the maximum value of type 'size_t', if the system doesn't define
   it. */
#ifndef SIZE_MAX
# undef SIZE_MAX
#endif])
])

dnl Autoconf >= 2.61 has AC_COMPUTE_INT built-in.
dnl Remove this when we can assume autoconf >= 2.61.
m4_ifdef([AC_COMPUTE_INT], [], [
  AC_DEFUN([AC_COMPUTE_INT], [_AC_COMPUTE_INT([$2],[$1],[$3],[$4])])
])

# stdint_h.m4 serial 9
dnl Copyright (C) 1997-2004, 2006, 2008-2016 Free Software Foundation, Inc.
dnl This file is free software; the Free Software Foundation
dnl gives unlimited permission to copy and/or distribute it,
dnl with or without modifications, as long as this notice is preserved.

dnl From Paul Eggert.

# Define HAVE_STDINT_H_WITH_UINTMAX if <stdint.h> exists,
# doesn't clash with <sys/types.h>, and declares uintmax_t.

AC_DEFUN([gl_AC_HEADER_STDINT_H],
[
  AC_CACHE_CHECK([for stdint.h], [gl_cv_header_stdint_h],
    [AC_COMPILE_IFELSE(
       [AC_LANG_PROGRAM(
          [[#include <sys/types.h>
            #include <stdint.h>]],
          [[uintmax_t i = (uintmax_t) -1; return !i;]])],
       [gl_cv_header_stdint_h=yes],
       [gl_cv_header_stdint_h=no])])
  if test $gl_cv_header_stdint_h = yes; then
    AC_DEFINE_UNQUOTED([HAVE_STDINT_H_WITH_UINTMAX], [1],
      [Define if <stdint.h> exists, doesn't clash with <sys/types.h>,
       and declares uintmax_t. ])
  fi
])

# threadlib.m4 serial 11 (gettext-0.18.2)
dnl Copyright (C) 2005-2016 Free Software Foundation, Inc.
dnl This file is free software; the Free Software Foundation
dnl gives unlimited permission to copy and/or distribute it,
dnl with or without modifications, as long as this notice is preserved.

dnl From Bruno Haible.

dnl gl_THREADLIB
dnl ------------
dnl Tests for a multithreading library to be used.
dnl If the configure.ac contains a definition of the gl_THREADLIB_DEFAULT_NO
dnl (it must be placed before the invocation of gl_THREADLIB_EARLY!), then the
dnl default is 'no', otherwise it is system dependent. In both cases, the user
dnl can change the choice through the options --enable-threads=choice or
dnl --disable-threads.
dnl Defines at most one of the macros USE_POSIX_THREADS, USE_SOLARIS_THREADS,
dnl USE_PTH_THREADS, USE_WINDOWS_THREADS
dnl Sets the variables LIBTHREAD and LTLIBTHREAD to the linker options for use
dnl in a Makefile (LIBTHREAD for use without libtool, LTLIBTHREAD for use with
dnl libtool).
dnl Sets the variables LIBMULTITHREAD and LTLIBMULTITHREAD similarly, for
dnl programs that really need multithread functionality. The difference
dnl between LIBTHREAD and LIBMULTITHREAD is that on platforms supporting weak
dnl symbols, typically LIBTHREAD is empty whereas LIBMULTITHREAD is not.
dnl Adds to CPPFLAGS the flag -D_REENTRANT or -D_THREAD_SAFE if needed for
dnl multithread-safe programs.

AC_DEFUN([gl_THREADLIB_EARLY],
[
  AC_REQUIRE([gl_THREADLIB_EARLY_BODY])
])

dnl The guts of gl_THREADLIB_EARLY. Needs to be expanded only once.

AC_DEFUN([gl_THREADLIB_EARLY_BODY],
[
  dnl Ordering constraints: This macro modifies CPPFLAGS in a way that
  dnl influences the result of the autoconf tests that test for *_unlocked
  dnl declarations, on AIX 5 at least. Therefore it must come early.
  AC_BEFORE([$0], [gl_FUNC_GLIBC_UNLOCKED_IO])dnl
  AC_BEFORE([$0], [gl_ARGP])dnl

  AC_REQUIRE([AC_CANONICAL_HOST])
  dnl _GNU_SOURCE is needed for pthread_rwlock_t on glibc systems.
  dnl AC_USE_SYSTEM_EXTENSIONS was introduced in autoconf 2.60 and obsoletes
  dnl AC_GNU_SOURCE.
  m4_ifdef([AC_USE_SYSTEM_EXTENSIONS],
    [AC_REQUIRE([AC_USE_SYSTEM_EXTENSIONS])],
    [AC_REQUIRE([AC_GNU_SOURCE])])
  dnl Check for multithreading.
  m4_ifdef([gl_THREADLIB_DEFAULT_NO],
    [m4_divert_text([DEFAULTS], [gl_use_threads_default=no])],
    [m4_divert_text([DEFAULTS], [gl_use_threads_default=])])
  AC_ARG_ENABLE([threads],
AC_HELP_STRING([--enable-threads={posix|solaris|pth|windows}], [specify multithreading API])m4_ifdef([gl_THREADLIB_DEFAULT_NO], [], [
AC_HELP_STRING([--disable-threads], [build without multithread safety])]),
    [gl_use_threads=$enableval],
    [if test -n "$gl_use_threads_default"; then
       gl_use_threads="$gl_use_threads_default"
     else
changequote(,)dnl
       case "$host_os" in
         dnl Disable multithreading by default on OSF/1, because it interferes
         dnl with fork()/exec(): When msgexec is linked with -lpthread, its
         dnl child process gets an endless segmentation fault inside execvp().
         dnl Disable multithreading by default on Cygwin 1.5.x, because it has
         dnl bugs that lead to endless loops or crashes. See
         dnl <http://cygwin.com/ml/cygwin/2009-08/msg00283.html>.
         osf*) gl_use_threads=no ;;
         cygwin*)
               case `uname -r` in
                 1.[0-5].*) gl_use_threads=no ;;
                 *)         gl_use_threads=yes ;;
               esac
               ;;
         *)    gl_use_threads=yes ;;
       esac
changequote([,])dnl
     fi
    ])
  if test "$gl_use_threads" = yes || test "$gl_use_threads" = posix; then
    # For using <pthread.h>:
    case "$host_os" in
      osf*)
        # On OSF/1, the compiler needs the flag -D_REENTRANT so that it
        # groks <pthread.h>. cc also understands the flag -pthread, but
        # we don't use it because 1. gcc-2.95 doesn't understand -pthread,
        # 2. putting a flag into CPPFLAGS that has an effect on the linker
        # causes the AC_LINK_IFELSE test below to succeed unexpectedly,
        # leading to wrong values of LIBTHREAD and LTLIBTHREAD.
        CPPFLAGS="$CPPFLAGS -D_REENTRANT"
        ;;
    esac
    # Some systems optimize for single-threaded programs by default, and
    # need special flags to disable these optimizations. For example, the
    # definition of 'errno' in <errno.h>.
    case "$host_os" in
      aix* | freebsd*) CPPFLAGS="$CPPFLAGS -D_THREAD_SAFE" ;;
      solaris*) CPPFLAGS="$CPPFLAGS -D_REENTRANT" ;;
    esac
  fi
])

dnl The guts of gl_THREADLIB. Needs to be expanded only once.

AC_DEFUN([gl_THREADLIB_BODY],
[
  AC_REQUIRE([gl_THREADLIB_EARLY_BODY])
  gl_threads_api=none
  LIBTHREAD=
  LTLIBTHREAD=
  LIBMULTITHREAD=
  LTLIBMULTITHREAD=
  if test "$gl_use_threads" != no; then
    dnl Check whether the compiler and linker support weak declarations.
    AC_CACHE_CHECK([whether imported symbols can be declared weak],
      [gl_cv_have_weak],
      [gl_cv_have_weak=no
       dnl First, test whether the compiler accepts it syntactically.
       AC_LINK_IFELSE(
         [AC_LANG_PROGRAM(
            [[extern void xyzzy ();
#pragma weak xyzzy]],
            [[xyzzy();]])],
         [gl_cv_have_weak=maybe])
       if test $gl_cv_have_weak = maybe; then
         dnl Second, test whether it actually works. On Cygwin 1.7.2, with
         dnl gcc 4.3, symbols declared weak always evaluate to the address 0.
         AC_RUN_IFELSE(
           [AC_LANG_SOURCE([[
#include <stdio.h>
#pragma weak fputs
int main ()
{
  return (fputs == NULL);
}]])],
           [gl_cv_have_weak=yes],
           [gl_cv_have_weak=no],
           [dnl When cross-compiling, assume that only ELF platforms support
            dnl weak symbols.
            AC_EGREP_CPP([Extensible Linking Format],
              [#ifdef __ELF__
               Extensible Linking Format
               #endif
              ],
              [gl_cv_have_weak="guessing yes"],
              [gl_cv_have_weak="guessing no"])
           ])
       fi
      ])
    if test "$gl_use_threads" = yes || test "$gl_use_threads" = posix; then
      # On OSF/1, the compiler needs the flag -pthread or -D_REENTRANT so that
      # it groks <pthread.h>. It's added above, in gl_THREADLIB_EARLY_BODY.
      AC_CHECK_HEADER([pthread.h],
        [gl_have_pthread_h=yes], [gl_have_pthread_h=no])
      if test "$gl_have_pthread_h" = yes; then
        # Other possible tests:
        #   -lpthreads (FSU threads, PCthreads)
        #   -lgthreads
        gl_have_pthread=
        # Test whether both pthread_mutex_lock and pthread_mutexattr_init exist
        # in libc. IRIX 6.5 has the first one in both libc and libpthread, but
        # the second one only in libpthread, and lock.c needs it.
        #
        # If -pthread works, prefer it to -lpthread, since Ubuntu 14.04
        # needs -pthread for some reason.  See:
        # http://lists.gnu.org/archive/html/bug-gnulib/2014-09/msg00023.html
        save_LIBS=$LIBS
        for gl_pthread in '' '-pthread'; do
          LIBS="$LIBS $gl_pthread"
          AC_LINK_IFELSE(
            [AC_LANG_PROGRAM(
               [[#include <pthread.h>
                 pthread_mutex_t m;
                 pthread_mutexattr_t ma;
               ]],
               [[pthread_mutex_lock (&m);
                 pthread_mutexattr_init (&ma);]])],
            [gl_have_pthread=yes
             LIBTHREAD=$gl_pthread LTLIBTHREAD=$gl_pthread
             LIBMULTITHREAD=$gl_pthread LTLIBMULTITHREAD=$gl_pthread])
          LIBS=$save_LIBS
          test -n "$gl_have_pthread" && break
        done

        # Test for libpthread by looking for pthread_kill. (Not pthread_self,
        # since it is defined as a macro on OSF/1.)
        if test -n "$gl_have_pthread" && test -z "$LIBTHREAD"; then
          # The program links fine without libpthread. But it may actually
          # need to link with libpthread in order to create multiple threads.
          AC_CHECK_LIB([pthread], [pthread_kill],
            [LIBMULTITHREAD=-lpthread LTLIBMULTITHREAD=-lpthread
             # On Solaris and HP-UX, most pthread functions exist also in libc.
             # Therefore pthread_in_use() needs to actually try to create a
             # thread: pthread_create from libc will fail, whereas
             # pthread_create will actually create a thread.
             case "$host_os" in
               solaris* | hpux*)
                 AC_DEFINE([PTHREAD_IN_USE_DETECTION_HARD], [1],
                   [Define if the pthread_in_use() detection is hard.])
             esac
            ])
        elif test -z "$gl_have_pthread"; then
          # Some library is needed. Try libpthread and libc_r.
          AC_CHECK_LIB([pthread], [pthread_kill],
            [gl_have_pthread=yes
             LIBTHREAD=-lpthread LTLIBTHREAD=-lpthread
             LIBMULTITHREAD=-lpthread LTLIBMULTITHREAD=-lpthread])
          if test -z "$gl_have_pthread"; then
            # For FreeBSD 4.
            AC_CHECK_LIB([c_r], [pthread_kill],
              [gl_have_pthread=yes
               LIBTHREAD=-lc_r LTLIBTHREAD=-lc_r
               LIBMULTITHREAD=-lc_r LTLIBMULTITHREAD=-lc_r])
          fi
        fi
        if test -n "$gl_have_pthread"; then
          gl_threads_api=posix
          AC_DEFINE([USE_POSIX_THREADS], [1],
            [Define if the POSIX multithreading library can be used.])
          if test -n "$LIBMULTITHREAD" || test -n "$LTLIBMULTITHREAD"; then
            if case "$gl_cv_have_weak" in *yes) true;; *) false;; esac; then
              AC_DEFINE([USE_POSIX_THREADS_WEAK], [1],
                [Define if references to the POSIX multithreading library should be made weak.])
              LIBTHREAD=
              LTLIBTHREAD=
            fi
          fi
        fi
      fi
    fi
    if test -z "$gl_have_pthread"; then
      if test "$gl_use_threads" = yes || test "$gl_use_threads" = solaris; then
        gl_have_solaristhread=
        gl_save_LIBS="$LIBS"
        LIBS="$LIBS -lthread"
        AC_LINK_IFELSE(
          [AC_LANG_PROGRAM(
             [[
#include <thread.h>
#include <synch.h>
             ]],
             [[thr_self();]])],
          [gl_have_solaristhread=yes])
        LIBS="$gl_save_LIBS"
        if test -n "$gl_have_solaristhread"; then
          gl_threads_api=solaris
          LIBTHREAD=-lthread
          LTLIBTHREAD=-lthread
          LIBMULTITHREAD="$LIBTHREAD"
          LTLIBMULTITHREAD="$LTLIBTHREAD"
          AC_DEFINE([USE_SOLARIS_THREADS], [1],
            [Define if the old Solaris multithreading library can be used.])
          if case "$gl_cv_have_weak" in *yes) true;; *) false;; esac; then
            AC_DEFINE([USE_SOLARIS_THREADS_WEAK], [1],
              [Define if references to the old Solaris multithreading library should be made weak.])
            LIBTHREAD=
            LTLIBTHREAD=
          fi
        fi
      fi
    fi
    if test "$gl_use_threads" = pth; then
      gl_save_CPPFLAGS="$CPPFLAGS"
      AC_LIB_LINKFLAGS([pth])
      gl_have_pth=
      gl_save_LIBS="$LIBS"
      LIBS="$LIBS $LIBPTH"
      AC_LINK_IFELSE(
        [AC_LANG_PROGRAM([[#include <pth.h>]], [[pth_self();]])],
        [gl_have_pth=yes])
      LIBS="$gl_save_LIBS"
      if test -n "$gl_have_pth"; then
        gl_threads_api=pth
        LIBTHREAD="$LIBPTH"
        LTLIBTHREAD="$LTLIBPTH"
        LIBMULTITHREAD="$LIBTHREAD"
        LTLIBMULTITHREAD="$LTLIBTHREAD"
        AC_DEFINE([USE_PTH_THREADS], [1],
          [Define if the GNU Pth multithreading library can be used.])
        if test -n "$LIBMULTITHREAD" || test -n "$LTLIBMULTITHREAD"; then
          if case "$gl_cv_have_weak" in *yes) true;; *) false;; esac; then
            AC_DEFINE([USE_PTH_THREADS_WEAK], [1],
              [Define if references to the GNU Pth multithreading library should be made weak.])
            LIBTHREAD=
            LTLIBTHREAD=
          fi
        fi
      else
        CPPFLAGS="$gl_save_CPPFLAGS"
      fi
    fi
    if test -z "$gl_have_pthread"; then
      case "$gl_use_threads" in
        yes | windows | win32) # The 'win32' is for backward compatibility.
          if { case "$host_os" in
                 mingw*) true;;
                 *) false;;
               esac
             }; then
            gl_threads_api=windows
            AC_DEFINE([USE_WINDOWS_THREADS], [1],
              [Define if the native Windows multithreading API can be used.])
          fi
          ;;
      esac
    fi
  fi
  AC_MSG_CHECKING([for multithread API to use])
  AC_MSG_RESULT([$gl_threads_api])
  AC_SUBST([LIBTHREAD])
  AC_SUBST([LTLIBTHREAD])
  AC_SUBST([LIBMULTITHREAD])
  AC_SUBST([LTLIBMULTITHREAD])
])

AC_DEFUN([gl_THREADLIB],
[
  AC_REQUIRE([gl_THREADLIB_EARLY])
  AC_REQUIRE([gl_THREADLIB_BODY])
])


dnl gl_DISABLE_THREADS
dnl ------------------
dnl Sets the gl_THREADLIB default so that threads are not used by default.
dnl The user can still override it at installation time, by using the
dnl configure option '--enable-threads'.

AC_DEFUN([gl_DISABLE_THREADS], [
  m4_divert_text([INIT_PREPARE], [gl_use_threads_default=no])
])


dnl Survey of platforms:
dnl
dnl Platform           Available  Compiler    Supports   test-lock
dnl                    flavours   option      weak       result
dnl ---------------    ---------  ---------   --------   ---------
dnl Linux 2.4/glibc    posix      -lpthread       Y      OK
dnl
dnl GNU Hurd/glibc     posix
dnl
dnl Ubuntu 14.04       posix      -pthread        Y      OK
dnl
dnl FreeBSD 5.3        posix      -lc_r           Y
dnl                    posix      -lkse ?         Y
dnl                    posix      -lpthread ?     Y
dnl                    posix      -lthr           Y
dnl
dnl FreeBSD 5.2        posix      -lc_r           Y
dnl                    posix      -lkse           Y
dnl                    posix      -lthr           Y
dnl
dnl FreeBSD 4.0,4.10   posix      -lc_r           Y      OK
dnl
dnl NetBSD 1.6         --
dnl
dnl OpenBSD 3.4        posix      -lpthread       Y      OK
dnl
dnl Mac OS X 10.[123]  posix      -lpthread       Y      OK
dnl
dnl Solaris 7,8,9      posix      -lpthread       Y      Sol 7,8: 0.0; Sol 9: OK
dnl                    solaris    -lthread        Y      Sol 7,8: 0.0; Sol 9: OK
dnl
dnl HP-UX 11           posix      -lpthread       N (cc) OK
dnl                                               Y (gcc)
dnl
dnl IRIX 6.5           posix      -lpthread       Y      0.5
dnl
dnl AIX 4.3,5.1        posix      -lpthread       N      AIX 4: 0.5; AIX 5: OK
dnl
dnl OSF/1 4.0,5.1      posix      -pthread (cc)   N      OK
dnl                               -lpthread (gcc) Y
dnl
dnl Cygwin             posix      -lpthread       Y      OK
dnl
dnl Any of the above   pth        -lpth                  0.0
dnl
dnl Mingw              windows                    N      OK
dnl
dnl BeOS 5             --
dnl
dnl The test-lock result shows what happens if in test-lock.c EXPLICIT_YIELD is
dnl turned off:
dnl   OK if all three tests terminate OK,
dnl   0.5 if the first test terminates OK but the second one loops endlessly,
dnl   0.0 if the first test already loops endlessly.

# uintmax_t.m4 serial 12
dnl Copyright (C) 1997-2004, 2007-2016 Free Software Foundation, Inc.
dnl This file is free software; the Free Software Foundation
dnl gives unlimited permission to copy and/or distribute it,
dnl with or without modifications, as long as this notice is preserved.

dnl From Paul Eggert.

AC_PREREQ([2.13])

# Define uintmax_t to 'unsigned long' or 'unsigned long long'
# if it is not already defined in <stdint.h> or <inttypes.h>.

AC_DEFUN([gl_AC_TYPE_UINTMAX_T],
[
  AC_REQUIRE([gl_AC_HEADER_INTTYPES_H])
  AC_REQUIRE([gl_AC_HEADER_STDINT_H])
  if test $gl_cv_header_inttypes_h = no && test $gl_cv_header_stdint_h = no; then
    AC_REQUIRE([AC_TYPE_UNSIGNED_LONG_LONG_INT])
    test $ac_cv_type_unsigned_long_long_int = yes \
      && ac_type='unsigned long long' \
      || ac_type='unsigned long'
    AC_DEFINE_UNQUOTED([uintmax_t], [$ac_type],
      [Define to unsigned long or unsigned long long
       if <stdint.h> and <inttypes.h> don't define.])
  else
    AC_DEFINE([HAVE_UINTMAX_T], [1],
      [Define if you have the 'uintmax_t' type in <stdint.h> or <inttypes.h>.])
  fi
])

# visibility.m4 serial 5 (gettext-0.18.2)
dnl Copyright (C) 2005, 2008, 2010-2016 Free Software Foundation, Inc.
dnl This file is free software; the Free Software Foundation
dnl gives unlimited permission to copy and/or distribute it,
dnl with or without modifications, as long as this notice is preserved.

dnl From Bruno Haible.

dnl Tests whether the compiler supports the command-line option
dnl -fvisibility=hidden and the function and variable attributes
dnl __attribute__((__visibility__("hidden"))) and
dnl __attribute__((__visibility__("default"))).
dnl Does *not* test for __visibility__("protected") - which has tricky
dnl semantics (see the 'vismain' test in glibc) and does not exist e.g. on
dnl Mac OS X.
dnl Does *not* test for __visibility__("internal") - which has processor
dnl dependent semantics.
dnl Does *not* test for #pragma GCC visibility push(hidden) - which is
dnl "really only recommended for legacy code".
dnl Set the variable CFLAG_VISIBILITY.
dnl Defines and sets the variable HAVE_VISIBILITY.

AC_DEFUN([gl_VISIBILITY],
[
  AC_REQUIRE([AC_PROG_CC])
  CFLAG_VISIBILITY=
  HAVE_VISIBILITY=0
  if test -n "$GCC"; then
    dnl First, check whether -Werror can be added to the command line, or
    dnl whether it leads to an error because of some other option that the
    dnl user has put into $CC $CFLAGS $CPPFLAGS.
    AC_MSG_CHECKING([whether the -Werror option is usable])
    AC_CACHE_VAL([gl_cv_cc_vis_werror], [
      gl_save_CFLAGS="$CFLAGS"
      CFLAGS="$CFLAGS -Werror"
      AC_COMPILE_IFELSE(
        [AC_LANG_PROGRAM([[]], [[]])],
        [gl_cv_cc_vis_werror=yes],
        [gl_cv_cc_vis_werror=no])
      CFLAGS="$gl_save_CFLAGS"])
    AC_MSG_RESULT([$gl_cv_cc_vis_werror])
    dnl Now check whether visibility declarations are supported.
    AC_MSG_CHECKING([for simple visibility declarations])
    AC_CACHE_VAL([gl_cv_cc_visibility], [
      gl_save_CFLAGS="$CFLAGS"
      CFLAGS="$CFLAGS -fvisibility=hidden"
      dnl We use the option -Werror and a function dummyfunc, because on some
      dnl platforms (Cygwin 1.7) the use of -fvisibility triggers a warning
      dnl "visibility attribute not supported in this configuration; ignored"
      dnl at the first function definition in every compilation unit, and we
      dnl don't want to use the option in this case.
      if test $gl_cv_cc_vis_werror = yes; then
        CFLAGS="$CFLAGS -Werror"
      fi
      AC_COMPILE_IFELSE(
        [AC_LANG_PROGRAM(
           [[extern __attribute__((__visibility__("hidden"))) int hiddenvar;
             extern __attribute__((__visibility__("default"))) int exportedvar;
             extern __attribute__((__visibility__("hidden"))) int hiddenfunc (void);
             extern __attribute__((__visibility__("default"))) int exportedfunc (void);
             void dummyfunc (void) {}
           ]],
           [[]])],
        [gl_cv_cc_visibility=yes],
        [gl_cv_cc_visibility=no])
      CFLAGS="$gl_save_CFLAGS"])
    AC_MSG_RESULT([$gl_cv_cc_visibility])
    if test $gl_cv_cc_visibility = yes; then
      CFLAG_VISIBILITY="-fvisibility=hidden"
      HAVE_VISIBILITY=1
    fi
  fi
  AC_SUBST([CFLAG_VISIBILITY])
  AC_SUBST([HAVE_VISIBILITY])
  AC_DEFINE_UNQUOTED([HAVE_VISIBILITY], [$HAVE_VISIBILITY],
    [Define to 1 or 0, depending whether the compiler supports simple visibility declarations.])
])

# wchar_t.m4 serial 4 (gettext-0.18.2)
dnl Copyright (C) 2002-2003, 2008-2016 Free Software Foundation, Inc.
dnl This file is free software; the Free Software Foundation
dnl gives unlimited permission to copy and/or distribute it,
dnl with or without modifications, as long as this notice is preserved.

dnl From Bruno Haible.
dnl Test whether <stddef.h> has the 'wchar_t' type.
dnl Prerequisite: AC_PROG_CC

AC_DEFUN([gt_TYPE_WCHAR_T],
[
  AC_CACHE_CHECK([for wchar_t], [gt_cv_c_wchar_t],
    [AC_COMPILE_IFELSE(
       [AC_LANG_PROGRAM(
          [[#include <stddef.h>
            wchar_t foo = (wchar_t)'\0';]],
          [[]])],
       [gt_cv_c_wchar_t=yes],
       [gt_cv_c_wchar_t=no])])
  if test $gt_cv_c_wchar_t = yes; then
    AC_DEFINE([HAVE_WCHAR_T], [1], [Define if you have the 'wchar_t' type.])
  fi
])

# wint_t.m4 serial 5 (gettext-0.18.2)
dnl Copyright (C) 2003, 2007-2016 Free Software Foundation, Inc.
dnl This file is free software; the Free Software Foundation
dnl gives unlimited permission to copy and/or distribute it,
dnl with or without modifications, as long as this notice is preserved.

dnl From Bruno Haible.
dnl Test whether <wchar.h> has the 'wint_t' type.
dnl Prerequisite: AC_PROG_CC

AC_DEFUN([gt_TYPE_WINT_T],
[
  AC_CACHE_CHECK([for wint_t], [gt_cv_c_wint_t],
    [AC_COMPILE_IFELSE(
       [AC_LANG_PROGRAM(
          [[
/* Tru64 with Desktop Toolkit C has a bug: <stdio.h> must be included before
   <wchar.h>.
   BSD/OS 4.0.1 has a bug: <stddef.h>, <stdio.h> and <time.h> must be included
   before <wchar.h>.  */
#include <stddef.h>
#include <stdio.h>
#include <time.h>
#include <wchar.h>
            wint_t foo = (wchar_t)'\0';]],
          [[]])],
       [gt_cv_c_wint_t=yes],
       [gt_cv_c_wint_t=no])])
  if test $gt_cv_c_wint_t = yes; then
    AC_DEFINE([HAVE_WINT_T], [1], [Define if you have the 'wint_t' type.])
  fi
])

# xsize.m4 serial 5
dnl Copyright (C) 2003-2004, 2008-2016 Free Software Foundation, Inc.
dnl This file is free software; the Free Software Foundation
dnl gives unlimited permission to copy and/or distribute it,
dnl with or without modifications, as long as this notice is preserved.

AC_DEFUN([gl_XSIZE],
[
  dnl Prerequisites of lib/xsize.h.
  AC_REQUIRE([gl_SIZE_MAX])
  AC_CHECK_HEADERS([stdint.h])
])

m4_include([bareos/db.m4])
m4_include([bareos/largefiles.m4])
m4_include([bareos/os.m4])
