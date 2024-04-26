#!/bin/bash
#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2024-2024 Bareos GmbH & Co. KG
#
#   This program is Free Software; you can redistribute it and/or
#   modify it under the terms of version three of the GNU Affero General Public
#   License as published by the Free Software Foundation and included
#   in the file LICENSE.
#
#   This program is distributed in the hope that it will be useful, but
#   WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
#   Affero General Public License for more details.
#
#   You should have received a copy of the GNU Affero General Public License
#   along with this program; if not, write to the Free Software
#   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
#   02110-1301, USA.

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
