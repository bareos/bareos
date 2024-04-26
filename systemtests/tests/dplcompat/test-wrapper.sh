#!/bin/sh
export archivedir=/tmp/archivedir
mkdir -p $archivedir

echo connection test
etc/bareos/localfile-wrapper.sh testconnection
echo $?

echo upload data
echo "Hello, World!" | 
etc/bareos/localfile-wrapper.sh upload test 0000
echo $?

echo 'list objects (output: "0000 14")'
etc/bareos/localfile-wrapper.sh list test
echo $?

echo 'get object size (output: "14")'
etc/bareos/localfile-wrapper.sh stat test 0000
echo $?

echo 'download data (output: "Hello, World!")'
etc/bareos/localfile-wrapper.sh download test 0000
echo $?

echo remove an object
etc/bareos/localfile-wrapper.sh remove test 0000
echo $?
