dnl Check type of signal routines (posix, 4.2bsd, 4.1bsd or v7)

AC_DEFUN([SIGNAL_CHECK],
 [AC_REQUIRE([AC_TYPE_SIGNAL])
  AC_MSG_CHECKING(for type of signal functions)
  AC_CACHE_VAL(bash_cv_signal_vintage,
  [
    AC_TRY_LINK([#include <signal.h>],[
      sigset_t ss;
      struct sigaction sa;
      sigemptyset(&ss); sigsuspend(&ss);
      sigaction(SIGINT, &sa, (struct sigaction *) 0);
      sigprocmask(SIG_BLOCK, &ss, (sigset_t *) 0);
    ], bash_cv_signal_vintage="posix",
    [
      AC_TRY_LINK([#include <signal.h>], [
          int mask = sigmask(SIGINT);
          sigsetmask(mask); sigblock(mask); sigpause(mask);
      ], bash_cv_signal_vintage="4.2bsd",
      [
        AC_TRY_LINK([
          #include <signal.h>
          RETSIGTYPE foo() { }], [
                  int mask = sigmask(SIGINT);
                  sigset(SIGINT, foo); sigrelse(SIGINT);
                  sighold(SIGINT); sigpause(SIGINT);
          ], bash_cv_signal_vintage="svr3", bash_cv_signal_vintage="v7"
        )]
      )]
    )
  ])
  AC_MSG_RESULT($bash_cv_signal_vintage)
  if test "$bash_cv_signal_vintage" = "posix"; then
    AC_DEFINE(HAVE_POSIX_SIGNALS)
  elif test "$bash_cv_signal_vintage" = "4.2bsd"; then
    AC_DEFINE(HAVE_BSD_SIGNALS)
  elif test "$bash_cv_signal_vintage" = "svr3"; then
    AC_DEFINE(HAVE_USG_SIGHOLD)
  fi
])

AC_DEFUN([BA_CONDITIONAL],
[AC_SUBST($1_TRUE)
AC_SUBST($1_FALSE)
if $2; then
  $1_TRUE=
  $1_FALSE='#'
else
  $1_TRUE='#'
  $1_FALSE=
fi])


AC_DEFUN([BA_CHECK_OPSYS],
[
AC_CYGWIN
if test $HAVE_UNAME=yes -a x`uname -s` = xSunOS
then
        BA_CONDITIONAL(HAVE_SUN_OS, $TRUEPRG)
        AC_DEFINE(HAVE_SUN_OS)
else
        BA_CONDITIONAL(HAVE_SUN_OS, $FALSEPRG)
fi

if test $HAVE_UNAME=yes -a x`uname -s` = xGNU
then
        BA_CONDITIONAL(HAVE_HURD_OS, $TRUEPRG)
        AC_DEFINE(HAVE_HURD_OS)
else
        BA_CONDITIONAL(HAVE_HURD_OS, $FALSEPRG)
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

if test $HAVE_UNAME=yes -a x`uname -s` = xIRIX -o x`uname -s` = xIRIX64
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

AC_DEFUN([BA_CHECK_OPSYS_DISTNAME],
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
elif test $HAVE_UNAME=yes -a x`uname -s` = xGNU
then
        DISTNAME=hurd
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
elif test x$host_vendor = xapple
then
    DISTNAME=osx
elif test $HAVE_UNAME=yes -a x`uname -s` = xDarwin
then
    DISTNAME=darwin
elif test -f /etc/engarde-version
then
        DISTNAME=engarde
elif test -f /etc/arch-release
then
        DISTNAME=archlinux
elif test "$CYGWIN" = yes
then
        DISTNAME=cygwin
        AC_DEFINE(HAVE_CYGWIN)
else
        DISTNAME=unknown
fi
AC_MSG_RESULT(done)
])
