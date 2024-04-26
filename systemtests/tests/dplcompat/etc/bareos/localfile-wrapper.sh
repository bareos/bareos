#!/bin/bash
set -Eeuo pipefail

case "$1" in
  testconnection)
    [ -d "$archivedir" ]
    ;;
  list)
    for f in "$archivedir"/*.*; do
      base="$(basename "$f")"
      printf "%s %d\n" "${base##*.}" "$(stat "--format=%s" "$f")"
    done
    ;;
  stat)
    exec stat "--format=%s" "$archivedir/$2.$3"
    ;;
  upload)
    exec cat >"$archivedir/$2.$3"
    ;;
  download)
    exec cat "$archivedir/$2.$3"
    ;;
  remove)
    exec rm "$archivedir/$2.$3"
    ;;
  *)
    exit 2
    ;;
esac
