#!/bin/bash
set -x

ORIGIN_DIR="../main/"
TARGET_DIR="./source/"

for destfile in `cat destfiles.txt | grep -v ^#`;
do
  file=`echo $destfile | sed 's#.*/##g'`
  chapterdir=`echo $destfile | sed 's#/.*##g'`
  filebase=`basename $destfile .tex`

  mkdir -p ${TARGET_DIR}${chapterdir}
  if [ "${chapterdir}" != "developers" ]; then

    ./pre_conversion_changes.sh  ${ORIGIN_DIR}${file} ${TARGET_DIR}${destfile};

    pandoc --verbose --columns=500  -f latex+raw_tex -t rst ${TARGET_DIR}${destfile} -o ${TARGET_DIR}${chapterdir}/${filebase}.rst || exit "could not convert file $file"

    ./post_conversion_changes.sh ${TARGET_DIR}${chapterdir}/${filebase}.rst
 else
   # developers files are only copied over
   cp ../developers/source/${file} ${TARGET_DIR}${chapterdir}
 fi
done
