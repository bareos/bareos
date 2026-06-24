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

# Build a tiny Alpine image. This can be extremely slow, so we do not remove the image at the end,
# to reproduce test results faster.

if ! incus image info "$VMIMAGE" >/dev/null 2>&1; then
  incus image export images:alpine/edge "$tmp" --vm
  guestfish --rw -a "$tmp/disk.qcow2" <<EOF
run
e2fsck-f /dev/sda2
resize2fs-M /dev/sda2
e2fsck-f /dev/sda2
EOF
  qemu-img create -f qcow2 -o preallocation=metadata "$tmp/disk.400M.qcow2" 400M
  virt-resize --shrink /dev/sda2 "$tmp/disk.qcow2" "$tmp/disk.400M.qcow2"
  incus image import "$tmp/incus.tar.xz" "$tmp/disk.400M.qcow2" --alias "$VMIMAGE"
  rm "$tmp/incus.tar.xz" "$tmp/disk.qcow2" "$tmp/disk.400M.qcow2"
fi
