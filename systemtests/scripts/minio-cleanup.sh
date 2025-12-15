#!/bin/bash
#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2020-2025 Bareos GmbH & Co. KG
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

set -e
set -u

. environment

: "${minio_data_dir:=minio-data-directory}"
: "${minio_alias:=$1-minio}"

echo "$0: stopping minio server"

tries=0
while pidof "$minio_alias" >/dev/null; do
  if ! pkill -f -SIGTERM "$minio_alias"; then
    break
  fi
  sleep 0.1
  ((tries++)) && [ "$tries" == 100 ] \
    && {
      echo "$0: could not stop minio server"
      exit 1
    }
done

if pidof "$minio_alias"; then
  echo "$0: could not stop minio server"
  exit 2
fi

rm -rf "${minio_data_dir}"
