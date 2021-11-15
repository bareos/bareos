#!/bin/bash
#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2020-2021 Bareos GmbH & Co. KG
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
  (( tries++ )) && [ "$tries" == '100' ] \
    && { echo "$0: could not stop minio server"; exit 2; }
done

export MINIO_DOMAIN=localhost,127.0.0.1

certificate_dir="${CMAKE_BINARY_DIR}/systemtests/tls/minio"

if [ "${systemtests_s3_use_https}" == "true" ]; then
  if ! [ -d "${certificate_dir}" ]; then
    echo "$0: could not find certificate dir (${certificate_dir})"
    exit 1
  fi
  certs="--certs-dir ${certificate_dir}"
else
  certs=""
fi

exec -a "$minio_alias" "${MINIO}" server ${certs} --address ":${minio_port_number}" "$minio_tmp_data_dir" > "$logdir"/minio.log &

if ! pidof ${MINIO} > /dev/null; then
  echo "$0: could not start minio server"
  exit 2
fi

tries=0
while ! s3cmd --config=${S3CFG} --no-check-certificate ls S3:// > /dev/null 2>&1; do
  sleep 0.1
  (( tries++ )) && [ "$tries" == '100' ] \
    && { echo "$0: could not start minio server"; exit 3; }
done

exit 0
