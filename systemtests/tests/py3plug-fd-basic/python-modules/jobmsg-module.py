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

from bareosfd import (
    bRC_OK,
    JobMessage,
    DebugMessage,
    M_INFO,
    M_WARNING,
    bFileType,
    StatPacket,
)

# get direct access to bareos_fd_plugin_object
import BareosFdWrapper

# import all the wrapper functions in our module scope
from BareosFdWrapper import *

from BareosFdPluginBaseclass import BareosFdPluginBaseclass

import sys
from stat import S_IFREG, S_IFDIR, S_IRWXU


class TestPlugin(BareosFdPluginBaseclass):
    def __init__(self, plugindef):
        JobMessage(M_INFO, "__init__('{}')\n".format(plugindef))
        self.plugindef = "not set"
        return super().__init__(plugindef)

    def parse_plugin_definition(self, plugindef):
        JobMessage(M_INFO, "parse_plugin_definition('{}')\n".format(plugindef))
        self.plugindef = plugindef
        return super().parse_plugin_definition(plugindef)

    def handle_plugin_event(self, event):
        JobMessage(M_INFO, "handle_plugin_event({})\n".format(event))
        return super().handle_plugin_event(event)

    def start_backup_file(self, savepkt):
        JobMessage(M_INFO, "start_backup_file()\n")
        statp = StatPacket()
        statp.st_size = 65 * 1024
        statp.st_mode = S_IRWXU | S_IFREG
        savepkt.statp = statp
        savepkt.type = 3  # FT_REG
        savepkt.no_read = False
        savepkt.fname = "/{}".format(self.plugindef)
        return bRC_OK

    def end_backup_file(self):
        JobMessage(M_INFO, "end_backup_file()\n")
        return super().end_backup_file()

    def start_restore_file(self, cmd):
        JobMessage(M_INFO, "start_restore_file()\n")
        return super().start_restore_file(cmd)

    def end_restore_file(self):
        JobMessage(M_INFO, "end_restore_file()\n")
        return super().end_restore_file()

    def restore_object_data(self, ROP):
        JobMessage(M_INFO, "restore_object_data()\n")
        return super().restore_object_data(ROP)

    def plugin_io(self, IOP):
        JobMessage(M_INFO, "plugin_io()\n")
        return super().plugin_io(IOP)

    def plugin_io_open(self, IOP):
        JobMessage(M_INFO, "plugin_io called plugin_io_open()\n")
        return bRC_OK

    def plugin_io_read(self, IOP):
        JobMessage(M_INFO, "plugin_io called plugin_io_read()\n")

        IOP.buf = bytearray(IOP.count)
        IOP.io_errno = 0
        IOP.status = 0
        return bRC_OK

    def plugin_io_close(self, IOP):
        JobMessage(M_INFO, "plugin_io called plugin_io_close()\n")
        return bRC_OK

    def create_file(self, restorepkt):
        JobMessage(M_INFO, "create_file()\n")
        return super().create_file(restorepkt)

    def set_file_attributes(self, restorepkt):
        JobMessage(M_INFO, "set_file_attributes()\n")
        return super().set_file_attributes(restorepkt)

    def check_file(self, fname):
        JobMessage(M_INFO, "check_file()\n")
        return super().check_file(fname)

    def get_acl(self, acl):
        JobMessage(M_INFO, "get_acl()\n")
        acl.content = bytearray(b"acl_content")
        return super().get_acl(acl)

    def set_acl(self, acl):
        JobMessage(M_INFO, "set_acl()\n")
        return bRC_OK

    def get_xattr(self, xattr):
        xattr.name = bytearray(b"xattr_name")
        xattr.value = bytearray(b"xattr_value")
        JobMessage(M_INFO, "get_xattr()\n")

        return super().get_xattr(xattr)

    def set_xattr(self, xattr):
        JobMessage(M_INFO, "set_xattr()\n")
        return bRC_OK

    def handle_backup_file(self, savepkt):
        JobMessage(M_INFO, "handle_backup_file()\n")
        return super().handle_backup_file(savepkt)


def load_bareos_plugin(plugindef):
    DebugMessage(100, "load_bareos_plugin()\n")
    BareosFdWrapper.bareos_fd_plugin_object = TestPlugin(plugindef)
    return bRC_OK
