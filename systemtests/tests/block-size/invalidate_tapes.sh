#!/bin/bash
#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2021-2021 Bareos GmbH & Co. KG
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

. ./environment
. ./tape-config
. ./redirect_output

echo "=== $0 Running ==="

invalidate_slots_on_autochanger() {
  changer_device=$1
  tape_device=$2

  # check that the changer device is present
  if ! mtx -f "$changer_device" inquiry > /dev/null
  then
    echo "Could not query $changer_device."
    exit 1
  fi

  # check that the tape device is present
  if ! mt -f "$tape_device" status
  then
    echo "Could not query $tape_device."
    exit 1
  fi

  # remove tapes from all drives
  while read -r line; do
    changer_status=$(echo "$line" | \
      sed -e 's/Data Transfer Element \([0-9]\):Full (Storage Element \([0-9]*\).*/\1:\2/')
    if [ -n "$changer_status" ]; then
      dte=$(echo "${changer_status}" | awk -F: '{print $1}')
      se=$(echo "${changer_status}" | awk -F: '{print $2}')
      mtx -f "${changer_device}" unload "${se}" "${dte}"
    fi
    done <<< "$(mtx -f "${changer_device}" status | grep "Full (Storage")"

  mtx -f "${changer_device}" unload - ${USE_TAPE_DEVICE} || :

  # invalidate tape label
  i="${FIRST_SLOT_NUMBER}"
  mtx -f "${changer_device}" status | { \
  while read -r line && [ "${i}" -le "$LAST_SLOT_NUMBER"  ]; do
    if echo "${line}" | grep "$(printf 'Storage Element %d:Full\n' ${i} )"; then
      set -x
      mtx -f "${changer_device}" load "${i}" "${USE_TAPE_DEVICE}" && \
      mt -f "${tape_device}" rewind && \
      mt -f "${tape_device}" weof && \
      mtx -f "${changer_device}" unload "${i}" "${USE_TAPE_DEVICE}"
      set +x
      echo
      (( i=i+1 ))
    fi
  done

  slots_ready=$(( i-1 ))

  if [ ${slots_ready} -eq 0 ]; then
    echo "Could not invalidate any tape"
  else
    echo "Invalidated ${slots_ready} tapes of autochanger ${changer_device}."
  fi
  }
}


for i in {0..9}; do
  changer_device="CHANGER_DEVICE${i}"
  [[ -z "${!changer_device:-}" ]] && break
  tape_device="TAPE_DEVICES${i}[${USE_TAPE_DEVICE}]"
  invalidate_slots_on_autochanger "${!changer_device}" "${!tape_device}"
done

echo "=== $0 Ready ==="
