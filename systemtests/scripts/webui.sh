#!/bin/bash

#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2022-2022 Bareos GmbH & Co. KG
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

#
# Start the Bareos WebUI webserver.
#

set -e
set -o pipefail
set -u

#shellcheck source=../environment.in
. ./environment

# BAREOS_WEBUI_CONFDIR is often set incorrectly
# (last webui systemtest configured).
# They are always set to the local test directory.
export BAREOS_WEBUI_CONFDIR=${current_test_directory}/etc/bareos-webui/

BAREOS_DIRECTOR_ADDRESS="127.0.0.1"

if ! [ -d "${BAREOS_WEBUI_CONFDIR}" ]; then
    mkdir -p "${BAREOS_WEBUI_CONFDIR}"
fi
if ! [ -e "${BAREOS_WEBUI_CONFDIR}/directors.ini" ]; then
printf '
[localhost-dir]
enabled = "yes"
diraddress = "{}"
dirport = %s
' "${BAREOS_DIRECTOR_ADDRESS}" "${BAREOS_DIRECTOR_PORT}" > "${BAREOS_WEBUI_CONFDIR}/directors.ini"
fi

printf "#\n# Bareos WebUI running on:\n# %s\n#\n" "${BAREOS_WEBUI_BASE_URL}"
exec ${PHP_EXECUTABLE} -S ${BAREOS_DIRECTOR_ADDRESS}:${BAREOS_WEBUI_PORT} -t ${PROJECT_SOURCE_DIR}/../webui/public
