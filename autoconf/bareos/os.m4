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
    AC_DEFINE(HAVE_POSIX_SIGNALS, 1, [Define to 1 if you have POSIX signals])
  elif test "$bash_cv_signal_vintage" = "4.2bsd"; then
    AC_DEFINE(HAVE_BSD_SIGNALS, 1, [Define to 1 if you have 4.2BSD signals])
  elif test "$bash_cv_signal_vintage" = "svr3"; then
    AC_DEFINE(HAVE_USG_SIGHOLD, 1, [Define to 1 if you have SVR3 signals])
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
   AC_DEFINE(HAVE_SUN_OS, 1, [Define to 1 if you are running Solaris])
else
   BA_CONDITIONAL(HAVE_SUN_OS, $FALSEPRG)
fi

if test $HAVE_UNAME=yes -a x`uname -s` = xGNU
then
   BA_CONDITIONAL(HAVE_HURD_OS, $TRUEPRG)
   AC_DEFINE(HAVE_HURD_OS, 1, [Define to 1 if you are running GNU Hurd])
else
   BA_CONDITIONAL(HAVE_HURD_OS, $FALSEPRG)
fi

if test $HAVE_UNAME=yes -a x`uname -s` = xOSF1
then
   BA_CONDITIONAL(HAVE_OSF1_OS, $TRUEPRG)
   AC_DEFINE(HAVE_OSF1_OS, 1, [Define to 1 if you are running Tru64])
else
   BA_CONDITIONAL(HAVE_OSF1_OS, $FALSEPRG)
fi

if test $HAVE_UNAME=yes -a x`uname -s` = xAIX
then
   BA_CONDITIONAL(HAVE_AIX_OS, $TRUEPRG)
   AC_DEFINE(HAVE_AIX_OS, 1, [Define to 1 if you are running AIX])
else
   BA_CONDITIONAL(HAVE_AIX_OS, $FALSEPRG)
fi

if test $HAVE_UNAME=yes -a x`uname -s` = xHP-UX
then
   BA_CONDITIONAL(HAVE_HPUX_OS, $TRUEPRG)
   AC_DEFINE(HAVE_HPUX_OS, 1, [Define to 1 if you are running HPUX])
else
   BA_CONDITIONAL(HAVE_HPUX_OS, $FALSEPRG)
fi

if test $HAVE_UNAME=yes -a x`uname -s` = xLinux
then
   BA_CONDITIONAL(HAVE_LINUX_OS, $TRUEPRG)
   AC_DEFINE(HAVE_LINUX_OS, 1, [Define to 1 if you are running Linux])
else
   BA_CONDITIONAL(HAVE_LINUX_OS, $FALSEPRG)
fi

if test $HAVE_UNAME=yes -a x`uname -s` = xFreeBSD
then
   BA_CONDITIONAL(HAVE_FREEBSD_OS, $TRUEPRG)
   AC_DEFINE(HAVE_FREEBSD_OS, 1, [Define to 1 if you are running FreeBSD])
else
   BA_CONDITIONAL(HAVE_FREEBSD_OS, $FALSEPRG)
fi

if test $HAVE_UNAME=yes -a x`uname -s` = xNetBSD
then
   BA_CONDITIONAL(HAVE_NETBSD_OS, $TRUEPRG)
   AC_DEFINE(HAVE_NETBSD_OS, 1, [Define to 1 if you are running NetBSD])
else
   BA_CONDITIONAL(HAVE_NETBSD_OS, $FALSEPRG)
fi

if test $HAVE_UNAME=yes -a x`uname -s` = xOpenBSD
then
   BA_CONDITIONAL(HAVE_OPENBSD_OS, $TRUEPRG)
   AC_DEFINE(HAVE_OPENBSD_OS, 1, [Define to 1 if you are running OpenBSD])
else
   BA_CONDITIONAL(HAVE_OPENBSD_OS, $FALSEPRG)
fi

if test $HAVE_UNAME=yes -a x`uname -s` = xBSD/OS
then
   BA_CONDITIONAL(HAVE_BSDI_OS, $TRUEPRG)
   AC_DEFINE(HAVE_BSDI_OS, 1, [Define to 1 if you are running BSDI])
else
   BA_CONDITIONAL(HAVE_BSDI_OS, $FALSEPRG)
fi

if test $HAVE_UNAME=yes -a x`uname -s` = xSGI
then
   BA_CONDITIONAL(HAVE_SGI_OS, $TRUEPRG)
   AC_DEFINE(HAVE_SGI_OS, 1, [Define to 1 if you are running SGI OS])
else
   BA_CONDITIONAL(HAVE_SGI_OS, $FALSEPRG)
fi

if test $HAVE_UNAME=yes -a x`uname -s` = xIRIX -o x`uname -s` = xIRIX64
then
   BA_CONDITIONAL(HAVE_IRIX_OS, $TRUEPRG)
   AC_DEFINE(HAVE_IRIX_OS, 1, [Define to 1 if you are running IRIX])
else
   BA_CONDITIONAL(HAVE_IRIX_OS, $FALSEPRG)
fi

if test $HAVE_UNAME=yes -a x`uname -s` = xDarwin
then
    AM_CONDITIONAL(HAVE_DARWIN_OS, $TRUEPRG)
    AC_DEFINE(HAVE_DARWIN_OS, 1, [Define to 1 if you are running OSX])
else
    AM_CONDITIONAL(HAVE_DARWIN_OS, $FALSEPRG)
fi
])

AC_DEFUN([BA_CHECK_OPSYS_DISTNAME], [
AC_MSG_CHECKING(for Operating System Distribution)
if test "x$DISTNAME" != "x"
then
   echo "distname set to $DISTNAME"
else
   which lsb_release > /dev/null 2>&1
   if test $? = 0
   then
      LSB_DISTRIBUTOR=`lsb_release -i -s`
      case ${LSB_DISTRIBUTOR} in
         "SUSE LINUX")
            DISTNAME=suse
            ;;
         "openSUSE project")
            DISTNAME=suse
            ;;
         CentOS)
            DISTNAME=redhat
            ;;
         Fedora)
            DISTNAME=redhat
            ;;
         RedHatEnterprise*)
            DISTNAME=redhat
            ;;
         Oracle*)
            DISTNAME=redhat
            ;;
         MandrivaLinux)
            DISTNAME=mandrake
            ;;
         Arch|archlinux)
            DISTNAME=archlinux
            ;;
         LinuxMint)
            DISTNAME=debian
            ;;
         Debian)
            DISTNAME=debian
            ;;
         Ubuntu)
            DISTNAME=ubuntu
            ;;
         Univention)
            DISTNAME=univention
            ;;
         *)
            DISTNAME=""
            ;;
      esac

      #
      # If we got a valid DISTNAME get the DISTVER from lsb_release too.
      #
      if test "x$DISTNAME" != "x"
      then
         DISTVER=`lsb_release -d -s | sed -e 's/"//g'`
      fi
   fi

   #
   # If lsb_release gave us the wanted info we skip this fallback block.
   #
   if test "x$DISTNAME" = "x" -o "x$DISTVER" = "x"
   then
      if test $HAVE_UNAME=yes -a x`uname -s` = xOSF1
      then
         DISTNAME=alpha
         DISTVER=`uname -r`
      elif test $HAVE_UNAME=yes -a x`uname -s` = xAIX
      then
         DISTNAME=aix
         DISTVER=`uname -r`
      elif test $HAVE_UNAME=yes -a x`uname -s` = xHP-UX
      then
         DISTNAME=hpux
         DISTVER=`uname -r`
      elif test $HAVE_UNAME=yes -a x`uname -s` = xSunOS
      then
         DISTNAME=solaris
         DISTVER=`uname -r`
      elif test $HAVE_UNAME=yes -a x`uname -s` = xGNU
      then
         DISTNAME=hurd
         DISTVER=`uname -r`
      elif test $HAVE_UNAME=yes -a x`uname -s` = xFreeBSD
      then
         DISTNAME=freebsd
         DISTVER=`uname -r`
      elif test $HAVE_UNAME=yes -a x`uname -s` = xNetBSD
      then
         DISTNAME=netbsd
         DISTVER=`uname -r`
      elif test $HAVE_UNAME=yes -a x`uname -s` = xOpenBSD
      then
         DISTNAME=openbsd
         DISTVER=`uname -r`
      elif test $HAVE_UNAME=yes -a x`uname -s` = xIRIX
      then
         DISTNAME=irix
         DISTVER=`uname -r`
      elif test $HAVE_UNAME=yes -a x`uname -s` = xBSD/OS
      then
         DISTNAME=bsdi
         DISTVER=`uname -a | awk '{print $3}'`
      elif test -f /etc/SuSE-release
      then
         DISTNAME=suse
         DISTVER=`cat /etc/SuSE-release | \
                  grep VERSION | \
                  cut -f3 -d' '`
      elif test -d /etc/SuSEconfig
      then
         DISTNAME=suse
         DISTVER=5.x
      elif test -f /etc/mandrake-release
      then
         DISTNAME=mandrake
         DISTVER=`cat /etc/mandrake-release | \
                  grep release | \
                  cut -f5 -d' '`
      elif test -f /etc/fedora-release
      then
         DISTNAME=redhat
         DISTVER="`cat /etc/fedora-release | cut -d' ' -f1,3`"
      elif test -f /etc/whitebox-release
      then
         DISTNAME=redhat
         DISTVER=`cat /etc/whitebox-release | grep release`
      elif test -f /etc/redhat-release
      then
         DISTNAME=redhat
         DISTVER=`cat /etc/redhat-release | grep release`
      elif test -f /etc/gentoo-release
      then
         DISTNAME=gentoo
         DISTVER=`awk '/version / { print $5 }' < /etc/gentoo-release`
      elif test -f /etc/debian_version
      then
         if `test -f /etc/apt/sources.list && grep -q ubuntu /etc/apt/sources.list`; then
            DISTNAME=ubuntu
         else
            DISTNAME=debian
         fi
         DISTVER=`cat /etc/debian_version`
      elif test -f /etc/slackware-version
      then
         DISTNAME=slackware
         DISTVER=`cat /etc/slackware-version`
      elif test x$host_vendor = xapple
      then
         DISTNAME=osx
         DISTVER=`uname -r`
      elif test $HAVE_UNAME=yes -a x`uname -s` = xDarwin
      then
         DISTNAME=darwin
         DISTVER=`uname -r`
      elif test -f /etc/engarde-version
      then
         DISTNAME=engarde
         DISTVER=`uname -r`
      elif test -f /etc/arch-release
      then
         DISTNAME=archlinux
      elif test "$CYGWIN" = yes
      then
         DISTNAME=cygwin
         AC_DEFINE(HAVE_CYGWIN, 1, [Define to 1 if you use CYGWIN])
      else
         DISTNAME=unknown
         DISTVER=unknown
      fi
   fi
fi
AC_MSG_RESULT(done)
])

AC_DEFUN([BA_CHECK_OBS_DISTNAME], [
AC_MSG_CHECKING(for OBS OS Distribution)
if test "x$OBS_DISTRIBUTION" != "x"
then
   echo "obsdistname set to $OBS_DISTRIBUTION"
else
   if test -f /.build.log; then
      OBS_PROJECT=`grep 'Building bareos for project' /.build.log | cut -d' ' -f10 | sed "s#'##g"`
      OBS_DISTRIBUTION=`grep 'Building bareos for project' /.build.log | cut -d' ' -f12 | sed "s#'##g"`
      OBS_ARCH=`grep 'Building bareos for project' /.build.log | cut -d' ' -f14 | sed "s#'##g"`
      OBS_SRCMD5=`grep 'Building bareos for project' /.build.log | cut -d' ' -f16 | sed "s#'##g"`
      AC_DEFINE(IS_BUILD_ON_OBS, 1, [Define to 1 if things are build using the Open Build Service (OBS)])
   fi
fi
AC_MSG_RESULT(done)
])
