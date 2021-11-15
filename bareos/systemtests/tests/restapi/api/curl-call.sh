#!/bin/bash

#
# BAREOS - Backup Archiving REcovery Open Sourced
#
# Copyright (C) 2021-2021 Bareos GmbH & Co. KG
#
# This program is Free Software; you can redistribute it and/or
# modify it under the terms of version three of the GNU Affero General Public
# License as published by the Free Software Foundation, which is
# listed in the file LICENSE.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
# Affero General Public License for more details.
#
# You should have received a copy of the GNU Affero General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
# 02110-1301, USA.
#

set -e
set -o pipefail
set -u

cd $(dirname "$BASH_SOURCE")
#. ../environment
. ../environment-local

# $1: method (POST, GET)
# $2: endpoint_url
# $3: string to grep for
# $4: extra curl options
# $5: repeat n times with a pause of 1 second between tries to get the result

method="$1"
shift
endpoint="$1"
shift
curl_extra_options="$@"

if [ -z "${REST_API_TOKEN:-}" ]; then
  REST_API_TOKEN=$(./curl-auth.sh)
fi

url="${REST_API_URL}/${endpoint}"

RC=0
# curl doesn't like empty string "" as option, will exit with code 3
curl_cmd="curl -w \nHTTP_CODE=%{http_code}\n -s -X ${method} $url"
OUT=$(${curl_cmd} -H "Content-Type: application/json" -H "accept: application/json" -H "Authorization: Bearer $REST_API_TOKEN" "$@") || RC=$?

printf "$OUT\n"
exit $RC
