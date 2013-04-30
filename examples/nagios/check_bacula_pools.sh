#! /bin/sh
# Author : Ludovic Strappazon. l.strappazon@gmail.com
# Copyright 2004, Free Software Foundation Europe e.V.
# Any comment, advice or enhancement are welcome  :-) 

PATH=/bin:/sbin:/usr/bin:/usr/sbin:/usr/local/bin:/usr/local/sbin
MYSQL="/usr/bin/mysql -u bacula --password=mypassword"
TMP=/tmp
BACULA=/usr/local/bacula

PROGNAME=`basename $0`
PROGPATH=`echo $0 | sed -e 's,[\\/][^\\/][^\\/]*$,,'`
STATUS=""

. $PROGPATH/utils.sh

print_usage() {
        echo "Usage: $PROGNAME -P <pool> -M <media-type> -w <warning threshold> -c <critical threshold> [-S]"
}

print_help() {
        echo ""
        print_usage
        echo ""
        echo "This plugin checks the space available in the pool against the space required for the next scheduled backups"
        echo "Example : $PROGNAME -P default -M LTO -w 20 -c 10 will check the default pool, return OK if (available space) > 1,20*(required space), WARNING if 1,20*(required space) > (available space) > 1,10*(required space), and CRITICAL else."
        echo ""
        echo "With the -S option, it will check the pool named Scratch and return WARNING instead of CRITICAL if the Scratch pool can save the situation."
        echo "Example : $PROGNAME -P default -M LTO -w 20 -c 10 -S will check the default pool, return OK if (available space) > 1,20*(required space), WARNING if 1,20*(required space) > (available space) > 1,10*(required space) or if (available space in default and Scratch) > 1,10*(required space) > (available space in default), and CRITICAL else."
        echo ""
        echo "The evaluation of the space required is done by adding the biggest backups of the same level than the scheduled jobs"
        echo "The available space is evaluated by the number of out of retention tapes and the average VolBytes of these Full tapes"
        echo ""
        echo "The Information Status are : \"Required, Available, Volume Errors\" and \"Will use Scratch pool\" if necessary."
        echo ""
        echo "I think this plugin should be used in passive mode, and ran by a RunAfterJob"
        exit 3
}

NB_ARGS=$#
SCRATCH=0
while getopts :P:M:w:c:hS OPTION
do
  case $OPTION in
    P) POOL="$OPTARG"
       ;;
    M) MEDIA_TYPE="$OPTARG"
       ;;
    S) SCRATCH=1
       ;;
    w) WARNING="$OPTARG"
       ;;
    c) CRITICAL="$OPTARG"
       ;;
    h) print_help
       exit 3
       ;;
    *) print_usage
       exit 3
       ;;
  esac
done
shift $(($OPTIND - 1))

if [ "$NB_ARGS" -ne 8 -a "$NB_ARGS" -ne 9 ]; then
        print_revision $PROGNAME 25/05/2005
        print_usage
        exit 3
fi

LAST_CHECK=`ps -ef | grep check_ba[Cc]ula_pools.sh | awk {'print $5'} | uniq | wc -l`
if [ "$LAST_CHECK" -gt 1 ]; then
        echo "The last check was not complete, you should increase the check_period."
        exit 3
fi

  NB_VOLUMES_OUT_OF_RETENTION=`$MYSQL << EOF
USE bacula
SELECT COUNT(MediaId) from Media, Pool where Media.PoolId=Pool.PoolId and Pool.Name="$POOL" AND LastWritten <> "0000-00-00 00:00:00" AND UNIX_TIMESTAMP()-UNIX_TIMESTAMP(LastWritten)>Media.VolRetention AND Inchanger = "1";
EOF
`
  NB_VOLUMES_OUT_OF_RETENTION=`echo $NB_VOLUMES_OUT_OF_RETENTION | cut -f 2 -d ' '`

NB_VOLUMES_ERROR=`$MYSQL << EOF
USE bacula
SELECT COUNT(MediaId) from Media, Pool where Media.PoolId=Pool.PoolId and Pool.Name="$POOL" AND VolStatus="Error" AND Inchanger = "1";
EOF
`
NB_VOLUMES_ERROR=`echo $NB_VOLUMES_ERROR | cut -f 2 -d ' '`

AVERAGE_CAPA_VOLUME=`$MYSQL << EOF
USE bacula
SELECT SUM(VolBytes)/COUNT(MediaId) FROM Media where VolStatus="Full" AND MediaType="$MEDIA_TYPE";
EOF
`
AVERAGE_CAPA_VOLUME=`echo $AVERAGE_CAPA_VOLUME | cut -f 2 -d ' ' | cut -f 1 -d '.'`

CAPA_VOLUMES_APPEND=`$MYSQL << EOF
USE bacula
SELECT SUM("$AVERAGE_CAPA_VOLUME"-VolBytes) from Media, Pool where Media.PoolId=Pool.PoolId and Pool.Name="$POOL" AND (VolStatus = "Append" OR VolStatus = "Recycle" OR VolStatus = "Purge") AND Inchanger = "1" AND MediaType="$MEDIA_TYPE";
EOF
`
CAPA_VOLUMES_APPEND=`echo $CAPA_VOLUMES_APPEND | cut -f 2 -d ' '`

if [ $SCRATCH -eq 1 ]
then
CAPA_VOLUMES_SCRATCH=`$MYSQL << EOF
USE bacula
SELECT SUM("$AVERAGE_CAPA_VOLUME"-VolBytes) from Media, Pool where Media.PoolId=Pool.PoolId and Pool.Name="Scratch" AND VolStatus = "Append" AND Inchanger = "1" AND MediaType="$MEDIA_TYPE";
EOF
`
CAPA_VOLUMES_SCRATCH=`echo $CAPA_VOLUMES_SCRATCH | cut -f 2 -d ' '`
else 
CAPA_VOLUMES_SCRATCH=0
fi

echo "st
1
q" | $BACULA/etc/bconsole | sed -n /Scheduled/,/Running/p | grep Backup | tr -s [:blank:] | tr '[:blank:]' '@' > ${TMP}/Scheduled.txt

CAPA_REQUIRED=0
for LINE in `cat ${TMP}/Scheduled.txt`
do
  SCHEDULED_JOB=`echo $LINE | awk -F@ '{print $6}'`
  LEVEL=`echo $LINE | awk -F@ '{print $1}' | cut -c 1` 

MAX_VOLUME_JOB_FOR_LEVEL=`$MYSQL << EOF
USE bacula
SELECT MAX(JobBytes) from Job, Pool where Level="$LEVEL" AND Job.Name="$SCHEDULED_JOB" AND Job.PoolId=Pool.PoolId AND Pool.Name="$POOL";
EOF
`
MAX_VOLUME_JOB_FOR_LEVEL=`echo $MAX_VOLUME_JOB_FOR_LEVEL | cut -f 2 -d ' ' `

CAPA_REQUIRED=$[CAPA_REQUIRED+MAX_VOLUME_JOB_FOR_LEVEL]
done

rm ${TMP}/Scheduled.txt

CAPA_WARNING=`echo $[(WARNING+100)*CAPA_REQUIRED]/100 | bc | cut -f 1 -d '.'`
CAPA_CRITICAL=`echo $[(CRITICAL+100)*CAPA_REQUIRED]/100 | bc | cut -f 1 -d '.'`
CAPA_DISP=$[NB_VOLUMES_OUT_OF_RETENTION*AVERAGE_CAPA_VOLUME+CAPA_VOLUMES_APPEND]
CAPA_DISP_INCLUDING_SCRATCH=$[CAPA_DISP+CAPA_VOLUMES_SCRATCH]

MESSAGE="Required : $[CAPA_REQUIRED/1000000000] Go, available : $[CAPA_DISP/1000000000] Go, Volumes Error : $NB_VOLUMES_ERROR"

if [ "$CAPA_DISP" -gt $CAPA_WARNING ]; then
   echo $MESSAGE
   exit 0
elif [ "$CAPA_DISP" -gt $CAPA_CRITICAL ];then
   echo $MESSAGE
   exit 1
elif [ "$CAPA_DISP_INCLUDING_SCRATCH" -gt $CAPA_CRITICAL ];then
   MESSAGE="${MESSAGE}. Will use Scratch Pool !"
   echo $MESSAGE
   exit 1
else
   exit 2
fi
exit 3
