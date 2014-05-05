#!/bin/sh

if [ 2 -ne $# ] ; then
  echo "Error : $0 needs 2 parameters" 2>&1
  exit 1
fi

COMMAND=$1
TEST_FILE=$2

case $COMMAND in
  prepare)
    echo "Content" > $TEST_FILE
    setfacl -m u:root:rw $TEST_FILE
    for i in `seq -f "%03g" 0 99` ; do
      setfattr -n user.key$i -v value$i $TEST_FILE
    done
    ;;
  clean)
    rm $TEST_FILE
    ;;
  *)
    exit 1
    ;;
esac
