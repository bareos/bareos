#!/bin/sh
# This script is to create a summary of the job notifications from bacula
# and send it to people who care.
#
# For it to work, you need to have all Bacula job report
# loggin to file, edit path for Your needs
# This should be run after all backup jobs have finished.
# Tested with bacula-1.38.0

# Some improvements by: Andrey Yakovlev <freedom@kiev.farlep.net>  (ISP Farlep)
# Contributed by Andrew J. Millar <andrew@alphajuliet.org.uk>
# Patched by Andrey A. Yakovlev <freedom@kiev.farlep.net>

# Use awk to create the report, pass to column to be
# formatted nicely, then on to mail to be sent to
# people who care.

EMAIL_LIST="freedom@kiev.farlep.net"

LOG='/var/db/bacula/log'


#---------------------------------------------------------------------

awk -F\:\  'BEGIN {
                        print "Client Status Type StartTime EndTime Files Bytes"
                }
                
        /orion-dir: New file:/ {
                print $3
        }
        
        /orion-dir: File:/ {
                print $3
        }
                
        /Client/ {
                CLIENT=$2; sub(/"/, "", CLIENT) ; sub(/".*$/, "", CLIENT)
        }
        /Backup Level/ {
                TYPE=$2 ; sub(/,.*$/, "", TYPE)
        }
        /Start time/ {
                STARTTIME=$2; sub(/.*-.*-.* /, "", STARTTIME)
        }
        /End time/ {
                ENDTIME=$2; sub(/.*-.*-.* /, "", ENDTIME)
        }
        /Files Examined/ {
                SDFILES=$2
                SDBYTES=0
        }
        /SD Files Written/ {
                SDFILES=$2
        }
        /SD Bytes Written/ {
                SDBYTES=$2
        }
        /Termination/ {
                TERMINATION=$2 ;
                sub(/Backup/, "", TERMINATION) ;
                gsub(/\*\*\*/, "", TERMINATION) ;
                sub(/Verify OK/, "OK-Verify", TERMINATION) ;
                sub(/y[ ]/, "y-", TERMINATION) ;
                printf "%s %s %s %s %s %s %s \n", CLIENT,TERMINATION,TYPE,STARTTIME,ENDTIME,SDFILES,SDBYTES}' ${LOG} | \
        column -t -x | \
        mail -s "Bacula Summary for `date -v -1d  +'%a, %F'`" ${EMAIL_LIST}
#
# Empty the LOG
cat ${LOG} > ${LOG}.old
cat /dev/null > ${LOG}
#
# That's all folks
