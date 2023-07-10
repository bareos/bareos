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
    JobMessage,
    DebugMessage,
    M_INFO,
    M_WARNING,
    bFileType,
    StatPacket,
    SetValue,
    bVarSinceTime,
)

from BareosFdPluginBaseclass import BareosFdPluginBaseclass

import sys
from stat import S_IFREG, S_IRWXU


@BareosPlugin
class TestForceBackupPlugin(BareosFdPluginBaseclass):
    def start_backup_job(self):
        DebugMessage(100, "enter custom start_backup_job()\n")
        ret = SetValue(bVarSinceTime, 0)
        DebugMessage(100, "SetValue() returned {}\n".format(ret))
        return bRC_OK

    def start_backup_file(self, savepkt):
        DebugMessage(100, "enter custom start_backup_file()\n")
        statp = StatPacket()
        statp.st_atime = 372639600
        statp.st_mtime = 372639600
        statp.st_ctime = 372639600
        statp.st_size = 65 * 1024
        statp.st_mode = S_IRWXU | S_IFREG
        savepkt.statp = statp
        savepkt.type = 3  # FT_REG
        savepkt.no_read = False
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
