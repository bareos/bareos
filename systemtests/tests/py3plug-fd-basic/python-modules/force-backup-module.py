#!/usr/bin/env python
# -*- coding: utf-8 -*-
# BAREOS - Backup Archiving REcovery Open Sourced
#
# Copyright (C) 2023-2023 Bareos GmbH & Co. KG
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
#

# this test module will emit a job message in every single callback there is
# to make sure emitting a message won't break anything

# import all the wrapper functions in our module scope
from BareosFdWrapper import *

from bareosfd import (
    bRC_OK,
    bRC_Error,
    JobMessage,
    DebugMessage,
    M_INFO,
    M_WARNING,
    bFileType,
    StatPacket,
    SetValue,
    GetValue,
    bVarCheckChanges,
    bVarSinceTime,
    FT_REG,
)

from BareosFdPluginBaseclass import BareosFdPluginBaseclass

import sys
from stat import S_IFREG, S_IRWXU


@BareosPlugin
class TestForceBackupPlugin(BareosFdPluginBaseclass):
    def start_backup_job(self):
        DebugMessage(123, "enter custom start_backup_job()\n")

        if self.options["mode"] == "since":
            ret = SetValue(bVarSinceTime, 1)
            DebugMessage(123, "SetValue(bVarSinceTime) returned {}\n".format(ret))
        else:
            ret = SetValue(bVarCheckChanges, False)
            DebugMessage(123, "SetValue(bVarCheckChanges) returned {}\n".format(ret))

        JobMessage(
            M_INFO,
            "bVarSinceTime = {}, bVarCheckChanges = {}\n".format(
                GetValue(bVarSinceTime), GetValue(bVarCheckChanges)
            ),
        )
        return bRC_OK

    def start_backup_file(self, savepkt):
        DebugMessage(123, "enter custom start_backup_file()\n")
        statp = StatPacket()

        # long long time ago: Friday, 23. October 1981 00:00:00 (CET)
        statp.st_atime = 372639600
        statp.st_mtime = 372639600
        statp.st_ctime = 372639600
        statp.st_size = 65 * 1024
        statp.st_mode = S_IRWXU | S_IFREG
        savepkt.statp = statp
        savepkt.type = FT_REG
        savepkt.no_read = False
        if "filename" in self.options:
            savepkt.fname = self.options["filename"]
        else:
            savepkt.fname = "/forced-file"
        return bRC_OK

    def plugin_io_read(self, IOP):
        IOP.buf = bytearray(IOP.count)
        IOP.io_errno = 0
        IOP.status = 0
        return bRC_OK

    def plugin_io_open(self, IOP):
        return bRC_OK

    def plugin_io_close(self, IOP):
        return bRC_OK
