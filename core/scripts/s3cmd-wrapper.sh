#!/usr/bin/env bash
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


s3cmd_common_args=(--no-progress)

# you can use a custom configuration file like this:
#s3cmd_common_args=(--no-progress --config /etc/bareos/s3cfg)



if ! s3cmd_prog="$(command -v s3cmd)"; then
  echo "Cannot find s3cmd command" >&2
  exit 1
fi

s3cmd_baseurl=s3://backup


s3cmd() {
  "$s3cmd_prog" "${s3cmd_common_args[@]}" "$@"
}

if [ $# -eq 2 ]; then
  volume_url="${s3cmd_baseurl}/$2/"
elif [ $# -eq 3 ]; then
  part_url="${s3cmd_baseurl}/$2/$3"
elif [ $# -gt 3 ]; then
  echo "$0 takes at most 3 arguments" >&2
  exit 1
fi

case "$1" in
  testconnection)
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
    exit 2
    ;;
esac
