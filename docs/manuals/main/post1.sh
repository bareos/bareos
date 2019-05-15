#!/bin/bash

set -e
set -u

SOURCEDIR="build/pandoc/"
SOURCEFILES=$(cd $SOURCEDIR; find -name "*.rst")
DESTDIR="build/post1/"

if ! [ -d "$SOURCEDIR" ]; then
    printf "Failed to access directory %s\n" "$SOURCEDIR"
    exit 1
fi

#echo "$SOURCEFILES"

mkdir -p $DESTDIR

for FILENAME in ${SOURCEFILES}; do
    
    SOURCEFILE="${SOURCEDIR}/${FILENAME}"
    DESTFILE="${DESTDIR}/${FILENAME%.*}.rst"
    mkdir -p `dirname ${DESTFILE}`
    
    #printf "%s -> %s\n" "$SOURCEFILE" "$DESTFILE"
    printf "%s\n" "$DESTFILE"

    cat "$SOURCEFILE" | ./latex-scan.py --standalone > ${DESTFILE} 2> ${DESTFILE}.latex-scan.log

done
