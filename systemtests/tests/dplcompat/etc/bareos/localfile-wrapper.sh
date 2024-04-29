#!/bin/bash
set -Eeuo pipefail

case "$1" in
  options)
    cat << '_EOT_'
storage_path
_EOT_
    ;;
  testconnection)
    [ -d "$storage_path" ]
    ;;
  list)
    for f in "$storage_path"/*.*; do
      base="$(basename "$f")"
      printf "%s %d\n" "${base##*.}" "$(stat "--format=%s" "$f")"
    done
    ;;
  stat)
    exec stat "--format=%s" "$storage_path/$2.$3"
    ;;
  upload)
    exec cat >"$storage_path/$2.$3"
    ;;
  download)
    exec cat "$storage_path/$2.$3"
    ;;
  remove)
    exec rm "$storage_path/$2.$3"
    ;;
  *)
    exit 2
    ;;
esac
