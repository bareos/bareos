#!/bin/sh
# script for creating / stopping a ssh-tunnel to a backupclient
# Stephan Holl<sholl@gmx.net>
# Modified by Joshua Kugler <joshua.kugler@uaf.edu>
#
#

# variables
USER=bacula
CLIENT=$2
LOCAL=your.backup.server.host.name
SSH=/usr/bin/ssh

case "$1" in
 start)
    # create ssh-tunnel 
        echo "Starting SSH-tunnel to $CLIENT..."
        $SSH -fnCN2 -o PreferredAuthentications=publickey -i /usr/local/bacula/ssh/id_dsa -l $USER -R 9101:$LOCAL:9101 -R 9103:$LOCAL:9103 $CLIENT > /dev/null 2> /dev/null
        exit $?
        ;;

 stop)
        # remove tunnel 
        echo "Stopping SSH-tunnel to $CLIENT..."
        # find PID killem
        PID=`ps ax | grep "ssh -fnCN2 -o PreferredAuthentications=publickey -i /usr/local/bacula/ssh/id_dsa" | grep "$CLIENT" | awk '{ print $1 }'`
        kill $PID
        exit $?
        ;;
 *)
        #  usage:
        echo "             "
        echo "      Start SSH-tunnel to client-host"
        echo "      to bacula-director and storage-daemon"
        echo "            "
        echo "      USAGE:"
        echo "      ssh-tunnel.sh {start|stop} client.fqdn"
        echo ""
        exit 1
        ;;
esac
