#!/usr/bin/env python3

# -*- coding: utf-8 -*-
# BAREOS - Backup Archiving REcovery Open Sourced
#
# Copyright (C) 2025-2025 Bareos GmbH & Co. KG
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

# hack!

import bareosfd_native

import proto
import socket
import time


class BareosConnection:
    def __init__(self):
        self.plugin = socket.socket(fileno=3)
        self.core = socket.socket(fileno=4)
        self.io = socket.socket(fileno=5)

    def readmsg(fd: int, msg):
        size = read_size(fd)
        s = read_string(size)
        msg.SerializeFromString(s)
        return msg

    def writemsg(fd: int):
        pass


with open("/tmp/log.out", "w") as f:
    try:
        con = BareosConnection()
        print(f"plugin = {con.plugin}, core = {con.core}, io = {con.io}", file=f)

    except Exception as e:
        print(e, file=f)

#
# def setup():
#    pass
