#!/bin/sh
#
# Bacula interface to mtx autoloader
#
#   Created  OCT/31/03 by Alexander Kuehn, derived from Ludwig Jaffe's script
#
#  Works with the HP C1537A L708 DDS3
#
#set -x
# these are the labels of the tapes in each virtual slot, not the slots!
labels="PSE-0001 PSE-0002 PSE-0003 PSE-0004 PSE-0005 PSE-0006 PSE-0007 PSE-0008 PSE-0009 PSE-0010 PSE-0011 PSE-0012"

# who to send a mail to?
recipient=root@localhost
logfile=/var/log/mtx.log

# Delay in seconds how often to check whether a new tape has been inserted
TAPEDELAY=10	# the default is every 10 seconds
echo `date` ":" $@ >>$logfile

# change this if mt is not in the path (use different quotes!)
mt=`which mt`
grep=`which grep`
#
# how to run the console application?
console="/usr/local/sbin/console -c /usr/local/etc/console.conf"

command="$1"

#TAPEDRIVE0 holds the device/name of your 1st and only drive (Bacula supports only 1 drive currently)
#Read TAPEDRIVE from command line parameters
if [ -z "$2" ] ; then
  TAPEDRIVE0=/dev/nsa0 
else
  TAPEDRIVE0=$2 
fi

#Read slot from command line parameters
if [ -z "$3" ] ; then
  slot=`expr 1`
else
  slot=`expr $3`
fi

if [ -z "$command" ] ; then
  echo ""
  echo "The mtx-changer script for Bacula"
  echo "---------------------------------"
  echo ""
  echo "usage: mtx-changer <command> <devicename of tapedrive> [slot]"
  echo "       mtx-changer"
  echo ""
  echo "Valid commands:"
  echo ""
  echo "unload          Unloads a tape into the slot"
  echo "                from where it was loaded."
  echo "load <slot>     Loads a tape from the slot <slot>"
  echo "list            Lists full storage slots"
  echo "loaded          Gives slot from where the tape was loaded."
  echo "                0 means the tape drive is empty."
  echo "slots           Gives Number of avialable slots." 
  echo "volumes         List avialable slots and the label of the." 
  echo "                tape in it (slot:volume)"
  echo "Example:"
  echo "  mtx-changer load /dev/nst0 1   loads a tape from slot1"
  echo "  mtx-changer %a %o %S   "
  echo ""
  exit 0
fi


case "$command" in 
	unload)
		# At first do mt -f /dev/st0 offline to unload the tape 
		#
		# Check if you want to fool me
		echo "unmount"|$console >/dev/null 2>/dev/null
		echo "mtx-changer: Checking if drive is loaded before we unload. Request unload" >>$logfile
		if $mt -f $TAPEDRIVE0 status >/dev/null 2>/dev/null ; then		# mt says status ok
			echo "mtx-changer: Doing mt -f $TAPEDRIVE0 rewoffl to rewind and unload the tape!" >>$logfile
			$mt -f $TAPEDRIVE0 rewoffl
		else
			echo "mtx-changer:  *** Don't fool me! *** The Drive $TAPEDRIVE0 is empty." >>$logfile
		fi
		exit 0
		;;
	
   load)
		#Let's check if drive is loaded before we load it
		echo "mtx-changer: Checking if drive is loaded before we load. I Request loaded" >>$logfile
		LOADEDVOL=`echo "status Storage"|$console|$grep $TAPEDRIVE0|grep ^Device|grep -v "not open."|grep -v "ERR="|grep -v "no Bacula volume is mounted"|sed -e s/^.*Volume\ //|cut -d\" -f2`
#		if [ -z "$LOADEDVOL" ] ; then 	# this is wrong, becaus Bacula would try to use the tape if we mount it!
#			LOADEDVOL=`echo "mount"|$console|$grep $TAPEDRIVE0|grep Device|grep -v "not open."|grep -v "ERR="|sed -e s/^.*Volume\ //|cut -d\" -f2`
#			if [ -z "$LOADEDVOL" ] ; then 
#				echo "mtx-changer: The Drive $TAPEDRIVE0 is empty." >>$logfile
#			else							# restore state?
#				if [ $LOADEDVOL = $3 ] ; then 		# requested Volume mounted -> exit
#					echo "mtx-changer: *** Don't fool me! *** Tape $LOADEDVOL is already in drive $TAPEDRIVE0!" >>$logfile
#					exit
#				else					# oops, wrong volume
#					echo "unmount"|$console >/dev/null 2>/dev/null
#				fi
#			fi
#		fi
		if [ -z "$LOADEDVOL" ] ; then
			echo "unmount"|$console >/dev/null 2>/dev/null
			LOADEDVOL=0
		else
			#Check if you want to fool me
			if [ $LOADEDVOL = $3 ] ; then 
				echo "mtx-changer: *** Don't fool me! *** Tape $LOADEDVOL is already in drive $TAPEDRIVE0!" >>$logfile
				exit
			fi
			echo "mtx-changer: The Drive $TAPEDRIVE0 is loaded with the tape $LOADEDVOL" >>$logfile
			echo "mtx-changer: Unmounting..." >>$logfile
			echo "unmount"|$console >/dev/null 2>/dev/null
		fi
		echo "mtx-changer: Unloading..." >>$logfile
		echo "mtx-changer: Doing mt -f $TAPEDRIVE0 rewoffl to rewind and unload the tape!" >>$logfile
		mt -f $TAPEDRIVE0 rewoffl 2>/dev/null
		#Now we can load the drive as desired 
		echo "mtx-changer: Doing mtx -f $1 $2 $3" >>$logfile
		# extract label for the mail
		count=`expr 1`
		for label in $labels ; do
			if [ $slot -eq $count ] ; then volume=$label ; fi
			count=`expr $count + 1`
		done

		mail -s "Bacula needs volume $volume." $recipient <<END_OF_DATA
Please insert volume $volume from slot $slot into $TAPEDRIVE0 .
Kind regards,
Bacula.
END_OF_DATA
		sleep 15
		$mt status >/dev/null 2>/dev/null
		while [ $? -ne 0 ] ; do
			sleep $TAPEDELAY
			$mt status >/dev/null 2>/dev/null
		done
		mail -s "Bacula says thank you." $recipient <<END_OF_DATA
Thank you for inserting the new tape! (I requested volume $volume from slot $slot.)
Kind regards,
Bacula.
END_OF_DATA
		echo "Successfully loaded a tape into drive $TAPEDRIVE0 (requested $volume from slot $slot)." >>$logfile
		echo "Loading finished." ; >>$logfile
		echo "$slot"
		exit 0
		;;

	list) 
		echo "mtx-changer: Requested list" >>$logfile
		LOADEDVOL=`echo "status Storage"|$console|$grep $TAPEDRIVE0|grep ^Device|grep -v "not open."|grep -v "ERR="|grep -v "no Bacula volume is mounted"|sed -e s/^.*Volume\ //|cut -d\" -f2`
		if [ -z $LOADEDVOL ] ; then		# try mounting
			LOADEDVOL=`echo "mount"|$console|$grep $TAPEDRIVE0|grep Device|grep -v "not open."|grep -v "ERR="|sed -e s/^.*Volume\ //|cut -d\" -f2`
			if [ -z $LOADEDVOL ] ; then		# no luck
				LOADEDVOL="_no_tape"
			else							# restore state
				echo "unmount"|$console >/dev/null 2>/dev/null
			fi
		fi
		count=`expr 1`
		for label in $labels ; do
			if [ "$label" != "$LOADEDVOL" ] ; then
				printf "$count "
			fi
			count=`expr $count + 1`
		done
		printf "\n"
      ;;

   loaded)
		echo "mtx-changer: Request loaded, dev $TAPEDRIVE0" >>$logfile
		LOADEDVOL=`echo "status Storage"|$console|$grep $TAPEDRIVE0|grep ^Device|grep -v "not open."|grep -v "ERR="|grep -v "no Bacula volume is mounted"|sed -e s/^.*Volume\ //|cut -d\" -f2`
		if [ -z $LOADEDVOL ] ; then
			LOADEDVOL=`echo "mount"|$console|$grep $TAPEDRIVE0|grep Device|grep -v "not open."|grep -v "ERR="|grep -v "no Bacula volume is mounted"|sed -e s/^.*Volume\ //|cut -d\" -f2`
			if [ -z "$LOADEDVOL" ] ; then		# no luck
				echo "$TAPEDRIVE0 not mounted!" >>$logfile
			else							# restore state
				echo "unmount"|$console >/dev/null 2>/dev/null
			fi
		fi
		if [ -z "$LOADEDVOL" ] ; then 
			LOADEDVOL="_no_tape" >>$logfile
			echo "0"
		else
			count=`expr 1`
			for label in $labels ; do
				if [ $LOADEDVOL = $label ] ; then echo $count ; fi
				count=`expr $count + 1`
			done
		fi
		exit 0
		;;

   slots)
		echo "mtx-changer: Request slots" >>$logfile
		count=`expr 0`
		for label in $labels ; do
			count=`expr $count + 1`
		done
		echo $count
      ;;

   volumes)
		echo "mtx-changer: Request volumes" >>$logfile
		count=`expr 1`
		for label in $labels ; do
			printf "$count:$label "
			count=`expr $count + 1`
		done
		printf "\n"
      ;;
esac
