#!/bin/bash

set -e
set -u

tmp="tmp"
logdir="log"
minio_tmp_data_dir="$tmp"/minio-data-directory
minio_port_number="$1"
minio_alias="$2"-minio

. environment

if ! "${MINIO}" -v > /dev/null 2>&1; then
  echo "$0: could not find minio binary"
  exit 1
fi

if [ -d "$minio_tmp_data_dir" ]; then
  rm -rf "$minio_tmp_data_dir"
fi

mkdir "$minio_tmp_data_dir"

echo "$0: starting minio server"

tries=0
while pidof "$minio_alias" > /dev/null; do
  kill -SIGTERM "$(pidof "$minio_alias")"
  sleep 0.1
  (( tries++ )) && [ $tries == '100' ] \
    && { echo "$0: could not stop minio server"; exit 2; }
done

export MINIO_DOMAIN=localhost,127.0.0.1
exec -a "$minio_alias" "${MINIO}" server --address :${minio_port_number} "$minio_tmp_data_dir" > "$logdir"/minio.log &

if ! pidof ${MINIO} > /dev/null; then
  echo "$0: could not start minio server"
  exit 2
fi

tries=0
while ! s3cmd --config=${S3CFG} ls S3:// > /dev/null 2>&1; do
  sleep 0.1
  (( tries++ )) && [ $tries == '100' ] \
    && { echo "$0: could not start minio server"; exit 3; }
done

exit 0

