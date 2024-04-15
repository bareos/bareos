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

set -e
set -u
set -o pipefail

YEAR=$(date +"%Y")
PROJECT_SOURCE_DIR="$(dirname "$0")/.."
LICENSE_TEMPLATE="${PROJECT_SOURCE_DIR}/LICENSE.template"
LICENSE_FILE="${PROJECT_SOURCE_DIR}/LICENSE.txt"

echo "Updating file $LICENSE_FILE from template."

# replace empty lines by "    .":
#sed -i -z -r -e 's/\n[[:space:]]*\n    /\n    .\n    /g' LICENSE.template

# replace ${YEAR} by current year
# and handle @include statements

true > "${LICENSE_FILE}"
sed "s/\${YEAR}/${YEAR}/g" < "${LICENSE_TEMPLATE}" | while IFS= read -r line; do
    if grep -q '^@include("' <<< "${line}"; then
        include_file=$(sed -n 's/@include("\(.*\)")/\1/p' <<< "${line}")
        sed -r -e 's/^[[:space:]]*$/./' -e 's/^(.*)$/    \1/' "${PROJECT_SOURCE_DIR}/${include_file}" >> "${LICENSE_FILE}"
    else
        echo "${line}" >> "${LICENSE_FILE}"
    fi
done
