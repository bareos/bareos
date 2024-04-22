#!/usr/bin/env bash
set -Eeuo pipefail

if ! s3cmd_prog="$(command -v s3cmd)"; then
  echo "Cannot find s3cmd command" >&2
  exit 1
fi

s3cmd_baseurl=s3://backup

s3cmd_common_args=(--no-progress --config /home/arogge/workspace/bareos/build/systemtests/tests/droplet-s3/etc/s3cfg)

s3cmd() {
  "$s3cmd_prog" "${s3cmd_common_args[@]}" "$@"
}

if [ $# -eq 2 ]; then
  volume_url="${s3cmd_baseurl}/$2"
elif [ $# -eq 3 ]; then
  part_url="${s3cmd_baseurl}/$2/$3"
elif [ $# -gt 3 ]; then
  echo "$0 takes at most 3 arguments" >&2
  exit 1
fi

case "$1" in
  testconn)
    s3cmd info "${s3cmd_baseurl}"
    ;;
  list)
    s3cmd ls "${volume_url}" \
    | while read -r date time size url; do
      echo "${url:${#volume_url}}" "$size"
    done
    ;;
  stat)
    # shellcheck disable=SC2030,SC2034
    s3cmd ls "$part_url" \
      | (read -r date time size url; echo "$size")
    ;;
  upload)
    s3cmd put - "$part_url"
    ;;
  download)
    s3cmd get "$part_url" -
    ;;
  remove)
    s3cmd del "$part_url"
    ;;
  *)
    s3cmd "$@"
    ;;
esac

