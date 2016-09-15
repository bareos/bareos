#!/bin/bash

#
# Split existing configuration into separate resource files, as used by the
# http://doc.bareos.org/master/html/bareos-manual-main-reference.html#SubdirectoryConfigurationScheme
#
# Author: philipp.storz@bareos.com
#

BCONSOLE=${BCONSOLE:-/usr/bin/bconsole}

bconsole()
{
  local out="$1"
  local cmd="$2"
  local temp=`mktemp`
  printf "%s\n%s\n%s\n" "gui on" "@out $temp" "$cmd" | $BCONSOLE > /dev/null
  # The send command is also written to the output. Remove it.
  # Error messages normally also contain the command, so they are also removed.
  grep -v -e "$cmd" "$temp" > "$out"
}

for restype in catalog client console counter director fileset job jobdefs messages pool profile schedule storage; do
    printf "\n%s:\n" "$restype"
    printf "==========\n"
    mkdir $restype 2>/dev/null
    if [ $restype = director ]; then
        bconsole "$restype/bareos-dir.conf" "show director"
    else
        dotcommand=".${restype}"
        if [ $restype = messages ]; then
            dotcommand=".msgs"
        fi
        TEMP=`mktemp`
        bconsole "$TEMP" "$dotcommand"
        cat $TEMP
        while read res; do
            #CONFFILENAME=`sed 's/ /_/g' <<< ${res}`
            CONFFILENAME="${res}"
            bconsole "$restype/${CONFFILENAME}.conf" "show ${restype}=\"${res}\""
        done < $TEMP
    fi
done

