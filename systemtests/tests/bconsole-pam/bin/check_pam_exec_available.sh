#!/bin/bash
#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2019-2022 Bareos GmbH & Co. KG
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
# Verify that our test pam configuration works.
# It uses
# * pam_wrapper  to redirect PAM to our test environemnt, using a specific service file
# * pamtester    to test PAM without the need to run Bareos
# * pam_exec.so  is defined in the bareos PAM service file.
#                It is configured to accept all logins where USERNAME = PASSWORD.
#

set -e
set -u


if [ -z "${PAM_WRAPPER_LIBRARIES:-}" ]; then
    # verify PAM_WRAPPER_LIBRARIES has been set my cmake
    echo "PAM_WRAPPER_LIBRARIES is not set"
    exit 1
fi

export PAM_WRAPPER=1
export PAM_WRAPPER_SERVICE_DIR=etc/pam.d/bareos_discover_pam_exec

if ! [ -e "${PAM_WRAPPER_SERVICE_DIR}" ]; then
    echo "PAM service file ${PAM_WRAPPER_SERVICE_DIR} not found"
    exit 1
fi

# DEBUG
#export PAM_WRAPPER_DEBUGLEVEL=4

# PAM_WRAPPER creates extra environments in /tmp/pam.*/
USERNAME="user"
PASSWORD="user"
echo "$PASSWORD" | LD_PRELOAD=${PAM_WRAPPER_LIBRARIES} pamtester bareos_discover_pam_exec "$USERNAME" authenticate > /dev/null 2>&1

exit $?
