#!/bin/bash

#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2026 Bareos GmbH & Co. KG
#
#   This program is free software; you can redistribute it and/or modify it
#   under the terms of version three of the GNU Affero General Public License
#   as published by the Free Software Foundation and included in the file
#   LICENSE.
#
#   This program is distributed in the hope that it will be useful, but
#   WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
#   Affero General Public License for more details.
#
#   You should have received a copy of the GNU Affero General Public License
#   along with this program; if not, write to the Free Software Foundation,
#   Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.

set -e
set -o pipefail
set -u

if [ "${TESTTYPE:-}" = "bareos-client-only" ] \
  || [ "${TESTTYPE:-}" = "bareos-universal-client" ]; then
  echo "SKIPPED (client-only does not contain webui)"
  return 0
fi

if [ "${WEBUI:-}" != "yes" ]; then
  echo "SKIPPED (webui test disabled)"
  return 0
fi

# can only check PRODVERSION, not get_bareos_daemon_version,
# as this is executed on jenkins
if ! is_version_ge "${PRODVERSION:-0}" "18.2"; then
  echo "SKIPPED (check only works on 18.2 and newer)"
  return 0
fi

# exclude windows & freebsd
if [ "${DIST:-}" = "windows" ] || [ "${DIST:-}" = "FreeBSD" ]; then
  echo "SKIPPED (webui package not build for this platform)"
  return 0
fi

# exclude centos 6 & rhel 6
if [[ "${DISTRELEASE:-}" = RHEL-6* ]] || [[ "${DISTRELEASE:-}" = CentOS-6* ]]; then
  echo "SKIPPED (webui package not build for this platform)"
  return 0
fi

if [ "${DIST:-}" = "windows" ]; then
  export BAREOS_WEBUI_BASE_URL="http://${VM_IP}/"
else
  export BAREOS_WEBUI_BASE_URL="http://${VM_IP}${webuiListenPort}/bareos-webui/"
fi

export BAREOS_WEBUI_BROWSER="chrome"
export BAREOS_WEBUI_USERNAME="citest"
export BAREOS_WEBUI_PASSWORD="citestpass"
export BAREOS_WEBUI_PROFILE="admin"
export BAREOS_WEBUI_TESTNAME="ALL"

BAREOS_WEBUI_CLIENT_NAME=bareos-fd
export BAREOS_WEBUI_CLIENT_NAME
echo "BAREOS_WEBUI_CLIENT_NAME=${BAREOS_WEBUI_CLIENT_NAME}"

export BAREOS_WEBUI_RESTOREFILE="/usr/sbin/bareos-fd"
if [ "${DIST:-}" = "windows" ]; then
  PROGRAMFILES=$(ssh "${CFG_SSH_OPTS}" "${REMOTEUSER}@${VM_IP}" \
    'echo $PROGRAMFILES | sed "s#C:.#C:/#"')
  export BAREOS_WEBUI_RESTOREFILE="${PROGRAMFILES}/Bareos/bareos-fd.exe"
fi

export BAREOS_WEBUI_LOG_PATH="$(dirname "$0")/.."

cd "$(dirname "$0")/.."

DISPLAY=:99 VM_IP="${VM_IP}" DIST="${DIST}" \
  python3 "$(dirname "$0")/webui-selenium-test.py" -v \
  >"${BAREOS_WEBUI_LOG_PATH}/webui-selenium-test.log" 2>&1

cat "${BAREOS_WEBUI_LOG_PATH}/webui-selenium-test.log"
