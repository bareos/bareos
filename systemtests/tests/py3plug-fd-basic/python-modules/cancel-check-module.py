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
    FT_REG,
    GetValue,
    bVarJobId,
    bEventCancelCommand,
    RegisterEvents,
)

from BareosFdPluginBaseclass import BareosFdPluginBaseclass

import sys
from stat import S_IFREG, S_IFDIR, S_IRWXU
from subprocess import Popen, PIPE
import time


@BareosPlugin
class TestPlugin(BareosFdPluginBaseclass):
    def __init__(self, plugindef):
        super().__init__(plugindef)
        JobMessage(M_INFO, "__init__('{}')\n".format(plugindef))

        RegisterEvents([bEventCancelCommand])
        self.files_to_backup = ["result"]

    def parse_plugin_definition(self, plugindef):
        JobMessage(M_INFO, "parse_plugin_definition('{}')\n".format(plugindef))
        self.plugindef = plugindef
        return super().parse_plugin_definition(plugindef)

    def handle_plugin_event(self, event):
        JobMessage(M_INFO, "handle_plugin_event({})\n".format(event))
        if event == bEventCancelCommand:
            JobMessage(M_INFO, f"Canceling jobid={GetValue(bVarJobId)}")
            time.sleep(5)
            return bRC_OK
        else:
            return super().handle_plugin_event(event)

    def start_backup_file(self, savepkt):
        JobMessage(M_INFO, "start_backup_file()\n")
        jobid = GetValue(bVarJobId)

        self.p = Popen("bin/bconsole", stdin=PIPE, text=True)
        self.p.stdin.write(f"cancel jobid={jobid}")
        self.p.stdin.close()

        statp = StatPacket()
        statp.st_size = 0
        statp.st_mode = S_IRWXU | S_IFREG
        savepkt.statp = statp
        savepkt.type = FT_REG
        savepkt.no_read = False
        savepkt.fname = "result"
        return bRC_OK

    def plugin_io_read(self, IOP):
        JobMessage(M_INFO, "plugin_io called plugin_io_read()\n")

        if self.p.poll() == None:
            JobMessage(M_INFO, "waiting...")
            IOP.buf = "1".encode()
            IOP.status = 1
        else:
            JobMessage(M_INFO, f"canceled jobid={jobid}\n")
            self.p = None
            IOP.buf = bytarray()
            IOP.status = 0

        IOP.io_errno = 0
        return bRC_OK
