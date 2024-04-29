#!/usr/bin/env bash
set -Eeuo pipefail

if [ "$1" = options ]; then
  printf "%s\n" bucket s3cfg s3cmd_prog base_url
  exit 0
fi

# option defaults
: "${s3cfg:=}"
: "${s3cmd_prog:=$(command -v s3cmd)}"
: "${bucket:=backup}"
: "${base_url:=s3://${bucket}}"

s3cmd_common_args=(--no-progress)

if [ -n "${s3cfg:+x}" ]; then
  if [ ! -r "${s3cfg}" ]; then
    echo "provided configuration file '${s3cfg}' is not readable" >&2
    exit 1
  else
    s3cmd_common_args+=(--config "${s3cfg}")
  fi
fi

if [ -z "${s3cmd_prog:+x}" ]; then
  echo "Cannot find s3cmd command" >&2
  exit 1
elif [ ! -x "${s3cmd_prog}" ]; then
  echo "Cannot execute '${s3cmd_prog}'" >&2
  exit 1
fi

run_s3cmd() {
  "$s3cmd_prog" "${s3cmd_common_args[@]}" "$@"
}
exec_s3cmd() {
  exec "$s3cmd_prog" "${s3cmd_common_args[@]}" "$@"
}

if [ $# -eq 2 ]; then
  volume_url="${base_url}/$2/"
elif [ $# -eq 3 ]; then
  part_url="${base_url}/$2/$3"
elif [ $# -gt 3 ]; then
  echo "$0 takes at most 3 arguments" >&2
  exit 1
fi

case "$1" in
  testconnection)
    exec_s3cmd info "${base_url}"
    ;;
  list)
    run_s3cmd ls "${volume_url}" \
    | while read -r date time size url; do
      echo "${url:${#volume_url}}" "$size"
    done
    ;;
  stat)
    # shellcheck disable=SC2030,SC2034
    run_s3cmd ls "$part_url" \
      | (read -r date time size url; echo "$size")
    ;;
  upload)
    exec_s3cmd put - "$part_url"
    ;;
  download)
    exec_s3cmd get "$part_url" -
    ;;
  remove)
    exec_s3cmd del "$part_url"
    ;;
  *)
    exit 2
    ;;
esac
