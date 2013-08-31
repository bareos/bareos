dnl

AC_DEFUN(BA_CONDITIONAL,
[AC_SUBST($1_TRUE)
AC_SUBST($1_FALSE)
if $2; then
  $1_TRUE=
  $1_FALSE='#'
else
  $1_TRUE='#'
  $1_FALSE=
fi])


AC_DEFUN(BA_CHECK_OPSYS,
[
AC_CYGWIN
if test $HAVE_UNAME=yes -a x`uname -s` = xSunOS
then
        BA_CONDITIONAL(HAVE_SUN_OS, $TRUEPRG)
        AC_DEFINE(HAVE_SUN_OS)
else
        BA_CONDITIONAL(HAVE_SUN_OS, $FALSEPRG)
fi

if test $HAVE_UNAME=yes -a x`uname -s` = xOSF1
then
        BA_CONDITIONAL(HAVE_OSF1_OS, $TRUEPRG)
        AC_DEFINE(HAVE_OSF1_OS)
else
        BA_CONDITIONAL(HAVE_OSF1_OS, $FALSEPRG)
fi

if test $HAVE_UNAME=yes -a x`uname -s` = xAIX
then
        BA_CONDITIONAL(HAVE_AIX_OS, $TRUEPRG)
        AC_DEFINE(HAVE_AIX_OS)
else
        BA_CONDITIONAL(HAVE_AIX_OS, $FALSEPRG)
fi

if test $HAVE_UNAME=yes -a x`uname -s` = xHP-UX
then
        BA_CONDITIONAL(HAVE_HPUX_OS, $TRUEPRG)
        AC_DEFINE(HAVE_HPUX_OS)
else
        BA_CONDITIONAL(HAVE_HPUX_OS, $FALSEPRG)
fi

if test $HAVE_UNAME=yes -a x`uname -s` = xLinux
then
        BA_CONDITIONAL(HAVE_LINUX_OS, $TRUEPRG)
        AC_DEFINE(HAVE_LINUX_OS)
else
        BA_CONDITIONAL(HAVE_LINUX_OS, $FALSEPRG)
fi

if test $HAVE_UNAME=yes -a x`uname -s` = xFreeBSD
then
        BA_CONDITIONAL(HAVE_FREEBSD_OS, $TRUEPRG)
        AC_DEFINE(HAVE_FREEBSD_OS)
else
        BA_CONDITIONAL(HAVE_FREEBSD_OS, $FALSEPRG)
fi

if test $HAVE_UNAME=yes -a x`uname -s` = xNetBSD
then
        BA_CONDITIONAL(HAVE_NETBSD_OS, $TRUEPRG)
        AC_DEFINE(HAVE_NETBSD_OS)
else
        BA_CONDITIONAL(HAVE_NETBSD_OS, $FALSEPRG)
fi

if test $HAVE_UNAME=yes -a x`uname -s` = xOpenBSD
then
        BA_CONDITIONAL(HAVE_OPENBSD_OS, $TRUEPRG)
        AC_DEFINE(HAVE_OPENBSD_OS)
else
        BA_CONDITIONAL(HAVE_OPENBSD_OS, $FALSEPRG)
fi

if test $HAVE_UNAME=yes -a x`uname -s` = xBSD/OS
then
        BA_CONDITIONAL(HAVE_BSDI_OS, $TRUEPRG)
        AC_DEFINE(HAVE_BSDI_OS)
else
        BA_CONDITIONAL(HAVE_BSDI_OS, $FALSEPRG)
fi

if test $HAVE_UNAME=yes -a x`uname -s` = xSGI
then
        BA_CONDITIONAL(HAVE_SGI_OS, $TRUEPRG)
        AC_DEFINE(HAVE_SGI_OS)
else
        BA_CONDITIONAL(HAVE_SGI_OS, $FALSEPRG)
fi

if test $HAVE_UNAME=yes -a x`uname -s` = xIRIX
then
        BA_CONDITIONAL(HAVE_IRIX_OS, $TRUEPRG)
        AC_DEFINE(HAVE_IRIX_OS)
else
        BA_CONDITIONAL(HAVE_IRIX_OS, $FALSEPRG)
fi

if test $HAVE_UNAME=yes -a x`uname -s` = xDarwin
then
    AM_CONDITIONAL(HAVE_DARWIN_OS, $TRUEPRG)
    AC_DEFINE(HAVE_DARWIN_OS)
else
    AM_CONDITIONAL(HAVE_DARWIN_OS, $FALSEPRG)
fi
])

AC_DEFUN(BA_CHECK_OPSYS_DISTNAME,
[AC_MSG_CHECKING(for Operating System Distribution)
if test "x$DISTNAME" != "x"
then
        echo "distname set to $DISTNAME"
elif test $HAVE_UNAME=yes -a x`uname -s` = xOSF1
then
        DISTNAME=alpha
elif test $HAVE_UNAME=yes -a x`uname -s` = xAIX
then
        DISTNAME=aix
elif test $HAVE_UNAME=yes -a x`uname -s` = xHP-UX
then
        DISTNAME=hpux
elif test $HAVE_UNAME=yes -a x`uname -s` = xSunOS
then
        DISTNAME=solaris
elif test $HAVE_UNAME=yes -a x`uname -s` = xFreeBSD
then
        DISTNAME=freebsd
elif test $HAVE_UNAME=yes -a x`uname -s` = xNetBSD
then
        DISTNAME=netbsd
elif test $HAVE_UNAME=yes -a x`uname -s` = xOpenBSD
then
        DISTNAME=openbsd
elif test $HAVE_UNAME=yes -a x`uname -s` = xIRIX
then
        DISTNAME=irix
elif test $HAVE_UNAME=yes -a x`uname -s` = xBSD/OS
then
        DISTNAME=bsdi
elif test -f /etc/SuSE-release
then
        DISTNAME=suse
elif test -d /etc/SuSEconfig
then
        DISTNAME=suse5
elif test -f /etc/mandrake-release
then
        DISTNAME=mandrake
elif test -f /etc/whitebox-release
then
       DISTNAME=redhat
elif test -f /etc/redhat-release
then
        DISTNAME=redhat
elif test -f /etc/gentoo-release
then
        DISTNAME=gentoo
elif test -f /etc/debian_version
then
        DISTNAME=debian
elif test -f /etc/slackware-version
then
        DISTNAME=slackware
elif test $HAVE_UNAME=yes -a x`uname -s` = xDarwin
then
    DISTNAME=darwin
elif test -f /etc/engarde-version
then
        DISTNAME=engarde
elif test "$CYGWIN" = yes
then
        DISTNAME=cygwin
        AC_DEFINE(HAVE_CYGWIN)
else
        DISTNAME=unknown
fi
AC_MSG_RESULT(done)
])

AC_DEFUN(AM_CONDITIONAL,
[AC_SUBST($1_TRUE)
AC_SUBST($1_FALSE)
if $2; then
  $1_TRUE=
  $1_FALSE='#'
else
  $1_TRUE='#'
  $1_FALSE=
fi])
