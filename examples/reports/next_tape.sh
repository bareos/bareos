#!/bin/bash
#
# A script which kicks out messages if a new tape is required for the next job. 
# It may be used as RunAfterJob script and it works fine for me. 
# Maybe someone considers it useful or has some ideas to improve it.
#
# Contributed by Dirk grosse Osterhues <digo@rbg.informatik.tu-darmstadt.de>
#
# select language: english (en) or german (de)
LANG="en"
# reciepient-address for notification
MAILTO_ADDR="your-email-address"
# bcc-address for notification
BCC_ADDR="email-address"
# directory for temp-files
TEMP_DIR="/tmp/bacula"
# bacula's console.conf
CONSOLE_CONF=/etc/bacula/bconsole.conf
############################################

# test if console.conf exists
if [ ! -f $CONSOLE_CONF ]; then
        echo "You need to reconfigure varible \$CONSOLE_CONF"
        exit 1
fi
# get todays tape
director_output() {
/usr/sbin/bacula-console -c $CONSOLE_CONF <<EOF
status dir
quit
EOF
}
TODAY=`date +%d.%m.%y`
YESTERDAY=`date +%d.%m.%y -d yesterday`
HOST=`hostname -f`

# /root/NEXT-TAPE-$TODAY will be /root/NEXT-TAPE-$YESTERDAY tomorrow ;)
TAPE_TODAY=`director_output|awk '/^Scheduled Jobs/ { getline; getline; getline; print $6;exit }'`

# did it alreadly run for at least one time?
if test -f $TEMP_DIR/NEXT-TAPE-$YESTERDAY ; then
        TAPE_YESTERDAY=`cat $TEMP_DIR/NEXT-TAPE-"$YESTERDAY"`
else
        TAPE_YESTERDAY=$TAPE_TODAY
        echo $TAPE_YESTERDAY>$TEMP_DIR/NEXT-TAPE-$YESTERDAY
fi
echo $TAPE_TODAY>$TEMP_DIR/NEXT-TAPE-$TODAY

# definition of language-dependent variables
case $LANG in
        de)
        MAIL_SUBJECT="[Bacula] Bitte Tape wechslen!"
        MAIL_BODY="Nachricht von Bacula-Backup-System auf $HOST:\
                \n\n Tape entfernen:\t\""$TAPE_YESTERDAY"\"\
                \n Tape einlegen: \t\""$TAPE_TODAY"\""
        ;;
        en)
        MAIL_SUBJECT="[Bacula] Please replace Tape tonight!"
        MAIL_BODY="Message from bacula-backup-service on $HOST:\
                \n\n Remove Tape:\t\""$TAPE_YESTERDAY"\"\
                \n Insert Tape:\t\""$TAPE_TODAY"\""
        ;;
esac

# send notification
if [ $TAPE_TODAY != $TAPE_YESTERDAY ] ; then
        echo -e $MAIL_BODY | mail -a "X-Bacula: Tape-Notifier on $HOST" -s "`echo $MAIL_SUBJECT`" -b $BCC_ADDR $MAILTO_ADDR
fi

# remove older temp-files
find $TEMP_DIR -type f -name NEXT-TAPE-\*| while read I; do
        TAPE_FILE=${I##/tmp/bacula/}
        if [ $TAPE_FILE ]; then
                if [ $TAPE_FILE != NEXT-TAPE-$TODAY ] && [ $TAPE_FILE != NEXT-TAPE-$YESTERDAY ]; then
                        rm  $TEMP_DIR/$TAPE_FILE
                fi
        fi
done
