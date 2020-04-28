#!/bin/sh
# determine platform
# extracted from os.m4

if test "x$PLATFORM" != "x"
then
   echo "platform set to $PLATFORM"
else
   which lsb_release > /dev/null 2>&1
   if test $? = 0
   then
      LSB_DISTRIBUTOR=`lsb_release -i -s`
      case ${LSB_DISTRIBUTOR} in
         *SUSE*)
            PLATFORM=suse
            ;;
         CentOS)
            PLATFORM=redhat
            ;;
         Fedora)
            PLATFORM=redhat
            ;;
         RedHatEnterprise*)
            PLATFORM=redhat
            ;;
         Oracle*)
            PLATFORM=redhat
            ;;
         MandrivaLinux)
            PLATFORM=mandrake
            ;;
         Arch|archlinux|ManjaroLinux)
            PLATFORM=archlinux
            ;;
         LinuxMint)
            PLATFORM=debian
            ;;
         Debian)
            PLATFORM=debian
            ;;
         Ubuntu)
            PLATFORM=ubuntu
            ;;
         Univention)
            PLATFORM=univention
            ;;
         *)
            PLATFORM=""
            ;;
      esac

      #
      # If we got a valid PLATFORM get the DISTVER from lsb_release too.
      #
      if test "x$PLATFORM" != "x"
      then
         DISTVER=`lsb_release -d -s | sed -e 's/"//g'`
      fi
   fi

   #
   # If lsb_release gave us the wanted info we skip this fallback block.
   #
   if test "x$PLATFORM" = "x" -o "x$DISTVER" = "x"
   then
      if test $HAVE_UNAME=yes -a x`uname -s` = xOSF1
      then
         PLATFORM=alpha
         DISTVER=`uname -r`
      elif test $HAVE_UNAME=yes -a x`uname -s` = xAIX
      then
         PLATFORM=aix
         DISTVER=`uname -r`
      elif test $HAVE_UNAME=yes -a x`uname -s` = xHP-UX
      then
         PLATFORM=hpux
         DISTVER=`uname -r`
      elif test $HAVE_UNAME=yes -a x`uname -s` = xSunOS
      then
         PLATFORM=solaris
         DISTVER=`uname -r`
      elif test $HAVE_UNAME=yes -a x`uname -s` = xGNU
      then
         PLATFORM=hurd
         DISTVER=`uname -r`
      elif test $HAVE_UNAME=yes -a x`uname -s` = xFreeBSD
      then
         PLATFORM=freebsd
         DISTVER=`uname -r`
      elif test $HAVE_UNAME=yes -a x`uname -s` = xNetBSD
      then
         PLATFORM=netbsd
         DISTVER=`uname -r`
      elif test $HAVE_UNAME=yes -a x`uname -s` = xOpenBSD
      then
         PLATFORM=openbsd
         DISTVER=`uname -r`
      elif test $HAVE_UNAME=yes -a x`uname -s` = xIRIX
      then
         PLATFORM=irix
         DISTVER=`uname -r`
      elif test $HAVE_UNAME=yes -a x`uname -s` = xBSD/OS
      then
         PLATFORM=bsdi
         DISTVER=`uname -a | awk '{print $3}'`
      elif test -f /etc/SuSE-release
      then
         PLATFORM=suse
         DISTVER=`cat /etc/SuSE-release | \
                  grep VERSION | \
                  cut -f3 -d' '`
      elif test -d /etc/SuSEconfig
      then
         PLATFORM=suse
         DISTVER=5.x
      elif test -f /etc/mandrake-release
      then
         PLATFORM=mandrake
         DISTVER=`cat /etc/mandrake-release | \
                  grep release | \
                  cut -f5 -d' '`
      elif test -f /etc/fedora-release
      then
         PLATFORM=redhat
         DISTVER="`cat /etc/fedora-release | cut -d' ' -f1,3`"
      elif test -f /etc/whitebox-release
      then
         PLATFORM=redhat
         DISTVER=`cat /etc/whitebox-release | grep release`
      elif test -f /etc/redhat-release
      then
         PLATFORM=redhat
         DISTVER=`cat /etc/redhat-release | grep release`
      elif test -f /etc/gentoo-release
      then
         PLATFORM=gentoo
         DISTVER=`awk '/version / { print $5 }' < /etc/gentoo-release`
      elif test -f /etc/debian_version
      then
         if `test -f /etc/apt/sources.list && grep -q ubuntu /etc/apt/sources.list`; then
            PLATFORM=ubuntu
         else
            PLATFORM=debian
         fi
         DISTVER=`cat /etc/debian_version`
      elif test -f /etc/slackware-version
      then
         PLATFORM=slackware
         DISTVER=`cat /etc/slackware-version`
      elif test x$host_vendor = xapple
      then
         PLATFORM=osx
         DISTVER=`uname -r`
      elif test $HAVE_UNAME=yes -a x`uname -s` = xDarwin
      then
         PLATFORM=darwin
         DISTVER=`uname -r`
      elif test -f /etc/engarde-version
      then
         PLATFORM=engarde
         DISTVER=`uname -r`
      elif test -f /etc/arch-release
      then
         PLATFORM=archlinux
      elif test "$CYGWIN" = yes
      then
         PLATFORM=cygwin
      else
         PLATFORM=unknown
         DISTVER=unknown
      fi
   fi
fi

echo "$PLATFORM;$DISTVER"
