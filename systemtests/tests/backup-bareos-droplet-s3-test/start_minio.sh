#!/bin/bash

tmp="tmp"
logdir="log"
minio_tmp_data_dir="$tmp"/minio-data-directory

. environment

if ! minio -v > /dev/null 2>&1; then
  echo "$0: could not find minio binary"
  exit 1
fi

if [ -d "$minio_tmp_data_dir" ]; then
  rm -rf "$minio_tmp_data_dir"
fi

mkdir "$minio_tmp_data_dir"

echo "$0: starting minio server"

tries=0
while pidof minio > /dev/null; do
  kill -SIGTERM "$(pidof minio)"
  sleep 0.1
  (( tries++ )) && [ $tries == '100' ] \
    && { echo "$0: could not stop minio server"; exit 2; }
done

minio server "$minio_tmp_data_dir" > "$logdir"/minio.log 2>&1 &

if ! pidof minio > /dev/null; then
  echo "$0: could not start minio server"
  exit 2
fi

tries=0
while ! s3cmd --config=etc/s3cfg-local-minio ls S3:// > /dev/null 2>&1; do
  sleep 0.1
  (( tries++ )) && [ $tries == '20' ] \
    && { echo "$0: could not start minio server"; exit 3; }
done

exit 0

