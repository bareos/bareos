#!/bin/bash
# migrate existing configuration into new scheme
IFS='
'

for restype in catalog client console counter director fileset job jobdef message pool profile schedule storage; do
    mkdir $restype
    dotcommand=".${restype}"
    if [ $restype = message ]; then
        dotcommand=".msgs"
    fi
    for res in `echo ${dotcommand} | bconsole\
        | sed 's/^Connecting.*//g' \
        | sed "s/^\.${dotcommand}.*//g" \
        | sed 's/^1000 OK.*//g' \
        | sed 's/^Enter.*//g' \
        | sed 's/^You.*//g' \
        `; do
        CONFFILENAME=`echo ${res}|sed 's/ /_/g'`
        echo "show ${restype}=\"${res}\"" | bconsole  \
        | sed 's/^Connecting.*//g' \
        | sed 's/^1000 OK.*//g' \
        | sed 's/^Enter.*//g' \
        | sed 's/^show.*//g' \
        | sed "s/^${restype}.*//g" \
        | sed 's/^You.*//g' \
        | sed '/^$/d' \
        > $restype/${CONFFILENAME}.conf
    done
done
