#!/bin/bash

SRCDIR=`pwd`

for FILE in `find . -name \*.c?`; do
#echo $FILE

FILENAME=`basename $FILE`
DIRECTORY=`dirname $FILE`
GCNO_FILE=`find  . -name $FILENAME.gcno | grep $DIRECTORY`

if [ ! -z "$GCNO_FILE" ]; then
   pushd $DIRECTORY
   gcov $FILENAME -o  $SRCDIR/$GCNO_FILE
   popd
else
   echo "$file: no coverage gcno file found"
fi
done
