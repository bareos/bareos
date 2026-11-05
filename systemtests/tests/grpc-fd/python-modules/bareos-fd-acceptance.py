# BAREOS - Backup Archiving REcovery Open Sourced
#
# Copyright (C) 2026-2026 Bareos GmbH & Co. KG
#
# This program is Free Software; you can redistribute it and/or
# modify it under the terms of version three of the GNU Affero General Public
# License as published by the Free Software Foundation and included
# in the file LICENSE.
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

import os

import bareosfd
import BareosFdWrapper
from BareosFdWrapper import *  # noqa
from BareosFdPluginLocalFilesetAclXattr import BareosFdPluginLocalFilesetAclXattr


class AcceptancePlugin(BareosFdPluginLocalFilesetAclXattr):
    def append_file_to_backup(self, filename):
        packet = bareosfd.SavePacket()
        packet.fname = filename
        packet.statp = bareosfd.StatPacket()
        stats = os.lstat(filename)
        packet.statp.st_mode = stats.st_mode
        packet.statp.st_size = stats.st_size
        result = bareosfd.AcceptFile(packet)
        if result == bareosfd.bRC_OK:
            super().append_file_to_backup(filename)
        elif result != bareosfd.bRC_Skip:
            raise RuntimeError(f"AcceptFile failed for {filename}: {result}")


def load_bareos_plugin(plugindef):
    BareosFdWrapper.bareos_fd_plugin_object = AcceptancePlugin(plugindef)
    return bareosfd.bRC_OK
