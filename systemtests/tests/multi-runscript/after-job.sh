#!/bin/bash

#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2026-2026 Bareos GmbH & Co. KG
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

#shellcheck source=../../environment.in
. ./environment
#shellcheck source=../../scripts/functions
. "${BAREOS_SCRIPTS_DIR}"/functions

if [ -f "${tmp}/script-ran" ]; then
  echo "Script already ran"
  exit 1
fi

echo "creating ${tmp}/script-ran"
cat <<EOF >"${tmp}/script-ran"
cancel all yes
EOF

echo "canceling all jobs"
run_bconsole "${tmp}/script-ran"

# we need to sleep a bit here to make sure that the cancel can actually
# try to cancel the job
echo "sleeping"
sleep 5

echo "done"
exit 0
