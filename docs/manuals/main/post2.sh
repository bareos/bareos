#!/bin/bash

set -e
set -u

SOURCEDIR="build/config-directive-description/"
SOURCEFILES=$(cd $SOURCEDIR; find -name "*.tex")
DESTDIR="build/post2/config-directive-description/"

if ! [ -d "$SOURCEDIR" ]; then
    printf "Failed to access directory %s\n" "$SOURCEDIR"
    exit 1
fi

#echo "$SOURCEFILES"

mkdir -p $DESTDIR

for FILENAME in ${SOURCEFILES}; do
    
    SOURCEFILE="${SOURCEDIR}/${FILENAME}"
    DESTFILE="${DESTDIR}/${FILENAME%.*}.rst.inc"
    mkdir -p `dirname ${DESTFILE}`
    
    #printf "%s -> %s\n" "$SOURCEFILE" "$DESTFILE"
    printf "%s\n" "$DESTFILE"

    pandoc --verbose --columns=500  -f latex+raw_tex -t rst "$SOURCEFILE" | ./latex-scan.py --standalone > ${DESTFILE} 2> ${DESTFILE}.latex-scan.log

done
