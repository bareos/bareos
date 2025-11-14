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

from proto import plugin_pb2
from proto import bareos_pb2
import socket
import time
import traceback
import sys

f = open("/tmp/log.out", "w")


def log(msg):
    print(msg, file=f)
    print(msg, file=sys.stderr)


def readnbyte(sock, n):
    buff = bytearray(n)
    pos = 0
    while pos < n:
        cr = sock.recv_into(memoryview(buff)[pos:])
        if cr == 0:
            raise EOFError
        pos += cr
    return buff


def readmsg(sock, msg):
    size_bytes = readnbyte(sock, 4)
    size = int.from_bytes(size_bytes, "little")
    log(f"size = {size}")

    obj_bytes = readnbyte(sock, size)
    msg.ParseFromString(obj_bytes)
    log(f"request = {msg}")


def writemsg(sock, msg):
    obj_bytes = msg.SerializeToString()
    size = len(obj_bytes)
    log(f"send: size = {size}")
    size_bytes = size.to_bytes(4, "little")
    con.plugin.sendall(size_bytes)
    con.plugin.sendall(obj_bytes)


class BareosConnection:
    def __init__(self):
        self.plugin = socket.socket(fileno=3)
        self.core = socket.socket(fileno=4)
        self.io = socket.socket(fileno=5)

    def read_plugin(self):
        req = plugin_pb2.PluginRequest()
        readmsg(self.plugin, req)

        return req

    def write_plugin(self, resp: plugin_pb2.PluginResponse):
        writemsg(self.plugin, resp)


def handle_plugin_event(
    req: plugin_pb2.handlePluginEventRequest, resp: plugin_pb2.handlePluginEventResponse
):
    del req
    resp.res = plugin_pb2.RC_OK


def handle_request(req: plugin_pb2.PluginRequest):
    log(f"handling {req}")

    resp = plugin_pb2.PluginResponse()

    match req.WhichOneof("request"):
        case "handle_plugin":
            resp.handle_plugin.SetInParent()
            handle_plugin_event(req.handle_plugin, resp.handle_plugin)
        # case 'start_backup': pass
        # case 'end_backup_file': pass
        # case 'start_restore_file': pass
        # case 'end_restore_file': pass
        # case 'file_open': pass
        # case 'file_seek': pass
        # case 'file_write': pass
        # case 'file_close': pass
        # case 'create_file': pass
        # case 'set_file_attributes': pass
        # case 'check_file': pass
        # case 'get_acl': pass
        # case 'set_acl': pass
        # case 'get_xattr': pass
        # case 'set_xattr': pass
        case "setup":
            resp.setup.SetInParent()
        # case 'file_read': pass
        case _:
            raise ValueError

    log(f"responding with {resp}")
    return resp


try:
    con = BareosConnection()
    log(f"plugin = {con.plugin}, core = {con.core}, io = {con.io}")

    while True:
        req = con.read_plugin()
        resp = handle_request(req)
        con.write_plugin(resp)

    # req = plugin_pb2.PluginRequest()

    # size_bytes = readnbyte(con.plugin, 4)
    # size = int.from_bytes(size_bytes, 'little')
    # print(f"size = {size}", file=f)

    # obj_bytes = readnbyte(con.plugin, size)
    # req.ParseFromString(obj_bytes)
    # print(f"request = {req}", file=f)

    # resp = plugin_pb2.PluginResponse()
    # resp.setup.SetInParent()

    # obj_bytes = resp.SerializeToString()
    # size = len(obj_bytes)
    # size_bytes = size.to_bytes(4, 'little')
    # con.plugin.sendall(size_bytes)
    # con.plugin.sendall(obj_bytes)

except:
    error = traceback.format_exc()
    log(f"error = {error}")

#
# def setup():
#    pass
