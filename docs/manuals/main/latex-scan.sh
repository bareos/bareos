#!/bin/bash

set -e
set -u

SOURCEDIR="build/rst-split/"
SOURCEFILES=$(cd $SOURCEDIR; find -name "*.rst")
DESTDIR="build/latex-scan/"


for FILENAME in ${SOURCEFILES}; do
    
    echo $FILENAME
    
    SOURCEFILE="${SOURCEDIR}/${FILENAME}"
    DESTFILE="${DESTDIR}/${FILENAME}"
    mkdir -p `dirname ${DESTFILE}`

    cat "${SOURCEFILE}" | ./latex-scan.py --standalone > ${DESTFILE} 2>${DESTFILE}.latex-scan.log || exit "failed to post convert ${SOURCEFILE}"
    
done
