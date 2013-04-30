#!/bin/sh
# local-install.sh
# for Bacula on Slackware platform
# Phil Stracchino 13 Mar 2004
#
# Installs and removes Bacula install section into /etc/rc.d/rc.local
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
   echo Installing Bacula autostart into $TARG:
   COUNT=`grep -c "Bacula section @@@@" $TARG`
   if [ ! "$COUNT" == "0" ] ; then
      echo -e "\tBacula autostart section appears to be already installed.\n\tIf you have changed the configuration, make uninstall-autostart\n\tthen make install-autostart again.\n"
   else
      if [ -w $TARG ] ; then
         if [ -w `dirname $TARG` ] ; then
            cp -p $TARG $TARG.bak
            echo -e "\tBackup copy of $TARG saved in $TARG.bak."
         else
            echo -e "\tWARNING: Unable to create backup copy of $TARG.\n\tAttempting to continue anyway.";
         fi
         cat >> $TARG << EOF
# @@@@ Start Bacula section @@@@
# The line above is needed to automatically remove bacula.

if [ -x /etc/rc.d/rc.bacula-sd ]; then
   /etc/rc.d/rc.bacula-sd start
fi
if [ -x /etc/rc.d/rc.bacula-fd ]; then
   /etc/rc.d/rc.bacula-fd start
fi
if [ -x /etc/rc.d/rc.bacula-dir ]; then
   /etc/rc.d/rc.bacula-dir start
fi

# @@@@ End Bacula section @@@@
EOF
         echo -e "\tBacula autostart section has been installed in $TARG.\n";
      else
         echo -e "\tERROR!  Cannot write to $TARG.\n\tBailing out.\n"
         exit -1
      fi
   fi
elif [ "$1" = "remove" ] ; then
   echo Removing Bacula autostart from $TARG:
   COUNT=`grep -c "Bacula section @@@@" $TARG`
   if [ ! "$COUNT" == "2" ] ; then
      echo -e "\tCould not find Bacula autostart section in $TARG.  Bailing out.\n"
      exit -1
   else
      if [ -w $TARG ] ; then
         if [ -w `dirname $TARG` ] ; then
            cp -p $TARG $TARG.bak
            echo -e "\tBackup copy of $TARG saved in $TARG.bak."
         else
            echo -e "\tWARNING: Unable to create backup copy of $TARG.\n\tAttempting to continue anyway.";
         fi
         FIRST=`grep -n "@@@@ Start Bacula section @@@@" $TARG | cut -d: -f1`
         LAST=`grep -n "@@@@ End Bacula section @@@@" $TARG | cut -d: -f1`
         FIRST=`expr $FIRST - 1`
         LAST=`expr $LAST + 1`
         head -$FIRST $TARG > ./installtmp
         tail +$LAST $TARG >> ./installtmp
         cat ./installtmp > $TARG
         rm ./installtmp
         echo -e "\tBacula autostart section has been removed from $TARG.\n";
      fi
   fi
else
   echo -e "\tUSAGE: $0 install|remove [destdir]"
fi
exit 0
