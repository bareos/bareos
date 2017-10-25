#!/bin/sh
# determine distname
# extracted from os.m4

if test "x$DISTNAME" != "x"
then
   echo "distname set to $DISTNAME"
else
   which lsb_release > /dev/null 2>&1
   if test $? = 0
   then
      LSB_DISTRIBUTOR=`lsb_release -i -s`
      case ${LSB_DISTRIBUTOR} in
         *SUSE*)
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
      else
         DISTNAME=unknown
         DISTVER=unknown
      fi
   fi
fi

if test "x$OBS_DISTRIBUTION" != "x"
then
   echo "obsdistname set to $OBS_DISTRIBUTION"
else
   if test -f /.build.log; then
      OBS_PROJECT=`grep 'Building bareos for project' /.build.log | cut -d' ' -f10 | sed "s#'##g"`
      OBS_DISTRIBUTION=`grep 'Building bareos for project' /.build.log | cut -d' ' -f12 | sed "s#'##g"`
      OBS_ARCH=`grep 'Building bareos for project' /.build.log | cut -d' ' -f14 | sed "s#'##g"`
      OBS_SRCMD5=`grep 'Building bareos for project' /.build.log | cut -d' ' -f16 | sed "s#'##g"`
   fi
fi

echo "$DISTNAME;$DISTVER;$OBS_PROJECT;$OBS_DISTRIBUTION;$OBS_ARCH;$OBS_SRCMD5"
