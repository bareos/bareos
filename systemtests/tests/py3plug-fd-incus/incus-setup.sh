#!/bin/bash

# BAREOS - Backup Archiving REcovery Open Sourced
#
# Copyright (C) 2025-2026 Benjamin Somers, IMT Atlantique
# Copyright (C) 2025-2026 Bareos GmbH & Co. KG
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

#shellcheck source=../../environment.in
. ./environment

# Your testing machine needs to allow vm and container to do network operations
# You might need to add the default incus bridge to trusted interfaces
# sudo firewall-cmd --zone=trusted --change-interface=incusbr0 --permanent
# sudo firewall-cmd --reload

# Build a tiny Alpine image. This can be extremely slow, so we do not remove the image at the end,
# to reproduce test results faster.

if ! incus image info "${INCUS_CT_BASE_NAME}" >/dev/null 2>&1; then
  echo "Setting up ${INCUS_CT_BASE_NAME} image"
  incus --verbose image export images:alpine/edge "${tmp}/${INCUS_CT_BASE_NAME}"
  incus --verbose image import "${tmp}/${INCUS_CT_BASE_NAME}" "${tmp}/${INCUS_CT_BASE_NAME}.root" --alias "${INCUS_CT_BASE_NAME}"
  rm -fv "${tmp}/${INCUS_CT_BASE_NAME}*"
fi


if ! incus image info "${INCUS_VM_BASE_NAME}" >/dev/null 2>&1;then
  echo "Setting up ${INCUS_VM_BASE_NAME} image"
  incus --verbose image export images:alpine/edge "${tmp}/${INCUS_VM_BASE_NAME}" --vm
  guestfish --rw -a "${tmp}/${INCUS_VM_BASE_NAME}.root" <<EOF
run
e2fsck-f /dev/sda2
resize2fs-M /dev/sda2
e2fsck-f /dev/sda2
EOF
  qemu-img create -f qcow2 -o preallocation=metadata "${tmp}/disk.400M.qcow2" 400M
  virt-resize --shrink /dev/sda2 "${tmp}/${INCUS_VM_BASE_NAME}.root" "${tmp}/disk.400M.qcow2"
  incus --verbose image import "${tmp}/${INCUS_VM_BASE_NAME}" "${tmp}/disk.400M.qcow2" --alias "${INCUS_VM_BASE_NAME}"
  rm -fv "${tmp}/${INCUS_VM_BASE_NAME}*" "${tmp}/disk.400M.qcow2"
fi

incus image list
