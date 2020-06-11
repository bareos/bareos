#!/bin/bash
. environment

echo "$0: stopping minio server"

tries=0
while pidof minio > /dev/null; do
  kill -SIGTERM "$(pidof minio)"
  sleep 0.1
  (( tries++ )) && [ $tries == '100' ] \
    && { echo "$0: could not stop minio server"; exit 1; }
done

if ! pidof minio; then
  exit 0
fi

echo "$0: could not stop minio server"
exit 2

