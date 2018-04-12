#!/bin/sh
# local-install.sh
# for Bareos on Slackware platform
# Phil Stracchino 13 Mar 2004
#
# Installs and removes Bareos install section into /etc/rc.d/rc.local
# provided /etc/rc.d/rc.local is writeable.  Creates a backup copy of
# /etc/rc.d/rc.local in /etc/rc.d/rc.local.bak if /etc/rc.d is writeable.
#
# Usage: local-install.sh install|remove [destdir]
#
# uncomment for debugging:
#set -x

if [ -n "$2" ] ; then
   TARG=$DESTDIR/etc/rc.d/rc.local
else
   TARG=/etc/rc.d/rc.local
fi

if [ ! -f $TARG ] ; then
   echo $TARG does not appear to exist.  Bailing out.
   exit -1
fi

if [ "$1" = "install" ] ; then
   echo Installing Bareos autostart into $TARG:
   COUNT=`grep -c "Bareos section @@@@" $TARG`
   if [ ! "$COUNT" == "0" ] ; then
      echo -e "\tBareos autostart section appears to be already installed.\n\tIf you have changed the configuration, make uninstall-autostart\n\tthen make install-autostart again.\n"
   else
      if [ -w $TARG ] ; then
         if [ -w `dirname $TARG` ] ; then
            cp -p $TARG $TARG.bak
            echo -e "\tBackup copy of $TARG saved in $TARG.bak."
         else
            echo -e "\tWARNING: Unable to create backup copy of $TARG.\n\tAttempting to continue anyway.";
         fi
         cat >> $TARG << EOF
# @@@@ Start Bareos section @@@@
# The line above is needed to automatically remove bareos.

if [ -x /etc/rc.d/rc.bareos-sd ]; then
   /etc/rc.d/rc.bareos-sd start
fi
if [ -x /etc/rc.d/rc.bareos-fd ]; then
   /etc/rc.d/rc.bareos-fd start
fi
if [ -x /etc/rc.d/rc.bareos-dir ]; then
   /etc/rc.d/rc.bareos-dir start
fi

# @@@@ End Bareos section @@@@
EOF
         echo -e "\tBareos autostart section has been installed in $TARG.\n";
      else
         echo -e "\tERROR!  Cannot write to $TARG.\n\tBailing out.\n"
         exit -1
      fi
   fi
elif [ "$1" = "remove" ] ; then
   echo Removing Bareos autostart from $TARG:
   COUNT=`grep -c "Bareos section @@@@" $TARG`
   if [ ! "$COUNT" == "2" ] ; then
      echo -e "\tCould not find Bareos autostart section in $TARG.  Bailing out.\n"
      exit -1
   else
      if [ -w $TARG ] ; then
         if [ -w `dirname $TARG` ] ; then
            cp -p $TARG $TARG.bak
            echo -e "\tBackup copy of $TARG saved in $TARG.bak."
         else
            echo -e "\tWARNING: Unable to create backup copy of $TARG.\n\tAttempting to continue anyway.";
         fi
         FIRST=`grep -n "@@@@ Start Bareos section @@@@" $TARG | cut -d: -f1`
         LAST=`grep -n "@@@@ End Bareos section @@@@" $TARG | cut -d: -f1`
         FIRST=`expr $FIRST - 1`
         LAST=`expr $LAST + 1`
         head -$FIRST $TARG > ./installtmp
         tail +$LAST $TARG >> ./installtmp
         cat ./installtmp > $TARG
         rm ./installtmp
         echo -e "\tBareos autostart section has been removed from $TARG.\n";
      fi
   fi
else
   echo -e "\tUSAGE: $0 install|remove [destdir]"
fi
exit 0
