#!/bin/bash

set -e
set -u

SOURCEDIR="build/split/"
SOURCEFILES=$(cd $SOURCEDIR; find -name "*.tex")
DESTDIR="build/pandoc/"

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

    pandoc --verbose --columns=500  -f latex+raw_tex --top-level-division=part -t rst "$SOURCEFILE" -o ${DESTFILE}

done
