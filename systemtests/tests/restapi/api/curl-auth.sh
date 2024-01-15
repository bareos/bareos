#!/bin/bash

#
# BAREOS - Backup Archiving REcovery Open Sourced
#
# Copyright (C) 2021-2023 Bareos GmbH & Co. KG
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

. $(dirname "$BASH_SOURCE")/../environment-local

USERNAME=${1:-admin-notls}
PASSWORD=${2:-secret}

curl --silent -X POST "${REST_API_URL}/token" -H  "accept: application/json" -H  "Content-Type: application/x-www-form-urlencoded" -d "username=${USERNAME}&password=${PASSWORD}" | grep access_token | cut -d '"' -f 4
