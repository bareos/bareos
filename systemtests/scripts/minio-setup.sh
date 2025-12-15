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

logdir="log"
: "${minio_data_dir:=minio-data-directory}"
: "${minio_port_number:=$1}"
: "${minio_alias:=$2-minio}"

. environment

if ! "${MINIO}" -v >/dev/null 2>&1; then
  echo "$0: could not find minio binary"
  exit 1
fi

if [ -d "$minio_data_dir" ]; then
  rm -rf "$minio_data_dir"
fi

mkdir -p "$minio_data_dir"

echo "$0: starting minio server"

tries=0
while pidof "$minio_alias" >/dev/null; do
  kill -SIGTERM "$(pidof "$minio_alias")"
  sleep 0.1
  ((tries++)) && [ "$tries" == '100' ] \
    && {
      echo "$0: could not stop minio server"
      exit 2
    }
done

if lsof="$(command -v lsof)"; then
  if pid=$("${lsof}" -ti "tcp:$minio_port_number"); then
    print_debug "Killing process listening on $minio_port_number with PID $pid"
    kill -9 "$pid"
  fi
fi

export MINIO_DOMAIN=localhost,127.0.0.1

certificate_dir="${CMAKE_BINARY_DIR}/systemtests/tls/minio"

if [ "${systemtests_s3_use_https}" == "true" ]; then
  if ! [ -d "${certificate_dir}" ]; then
    echo "$0: could not find certificate dir (${certificate_dir})"
    exit 1
  fi
  cert_args=(--certs-dir "${certificate_dir}")
else
  cert_args=()
fi

exec -a "$minio_alias" \
  "${MINIO}" server \
  "${cert_args[@]}" \
  --address ":${minio_port_number}" \
  "${minio_data_dir}" \
  >"$logdir"/minio.log 2>"$logdir"/minio_err.log \
  &
