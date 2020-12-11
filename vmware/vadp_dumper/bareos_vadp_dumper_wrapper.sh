#!/bin/sh
#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2020-2020 Bareos GmbH & Co. KG
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

cancel=0

if [ ! -d /sys/class/scsi_disk/ ]; then
  cat <<EOT >&2
  Your system does not have /sys/class/scsi_disk/ which is required for
  VADP dumper due to a bug in VMware's VDDK. Please load the sd_mod
  kernel module and make sure /sys/class/scsi_disk/ exists.
  You can do this temporarily by running "modprobe sd_mod" as root.
  Please refer to your OS documentation how to persist this change across reboots.
EOT
  cancel=1
fi

if [ ! -r /etc/mtab ]; then
  cat <<EOT >&2
  Your system does not have /etc/mtab which is required for VADP dumper
  due to a bug in VMware's VDDK. Please create /etc/mtab as a symlink to
  /proc/self/mounts.
  This can be done by running "cd /etc; ln -s ../proc/self/mounts mtab"
  as root.
EOT
  cancel=1
fi

if [ "$cancel" -eq 1 ]; then
  exit 2
fi

exec bareos_vadp_dumper "$@"
