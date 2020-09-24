#!/bin/bash
set -e
set -u

. environment

minio_alias=$1-minio

echo "$0: stopping minio server"

tries=0
while pidof "$minio_alias" > /dev/null; do
  if ! pkill -f -SIGTERM "$minio_alias" ; then
    break
  fi
  sleep 0.1
  (( tries++ )) && [ $tries == '100' ] \
    && { echo "$0: could not stop minio server"; exit 1; }
done

if ! pidof "$minio_alias"; then
  exit 0
fi

echo "$0: could not stop minio server"
exit 2

