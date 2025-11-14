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

# reexport the types as our own
from bareosfd_types import *
from bareosfd_type_conv import event_type_remote, jmsg_type_remote

from proto import plugin_pb2
from proto import bareos_pb2
import socket
import traceback
import sys
import inspect
from importlib import import_module

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
    log(f"sending {msg} (size = {size}) to {sock}")
    size_bytes = size.to_bytes(4, "little")
    log(f"bytes = {len(size_bytes)}, obj = {len(obj_bytes)}")
    sock.sendall(size_bytes)
    sock.sendall(obj_bytes)


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

    def read_core(self):
        resp = bareos_pb2.CoreResponse()
        readmsg(self.core, resp)
        return resp

    def write_core(self, req: bareos_pb2.CoreRequest):
        writemsg(self.core, req)


con = BareosConnection()
log(f"plugin = {con.plugin}, core = {con.core}, io = {con.io}")

### BAREOS PLUGIN API


def handle_plugin_event(
    req: plugin_pb2.handlePluginEventRequest, resp: plugin_pb2.handlePluginEventResponse
):
    del req
    resp.res = plugin_pb2.RC_OK


def start_backup(
    req: plugin_pb2.startBackupFileRequest, resp: plugin_pb2.startBackupFileResponse
):
    raise ValueError


def end_backup_file(
    req: plugin_pb2.endBackupFileRequest, resp: plugin_pb2.endBackupFileResponse
):
    raise ValueError


def start_restore_file(
    req: plugin_pb2.startRestoreFileRequest, resp: plugin_pb2.startRestoreFileResponse
):
    raise ValueError


def end_restore_file(
    req: plugin_pb2.endRestoreFileRequest, resp: plugin_pb2.endRestoreFileResponse
):
    raise ValueError


def file_open(req: plugin_pb2.fileOpenRequest, resp: plugin_pb2.fileOpenResponse):
    raise ValueError


def file_seek(req: plugin_pb2.fileSeekRequest, resp: plugin_pb2.fileSeekResponse):
    raise ValueError


def file_read(req: plugin_pb2.fileReadRequest, resp: plugin_pb2.fileReadResponse):
    raise ValueError


def file_write(req: plugin_pb2.fileWriteRequest, resp: plugin_pb2.fileWriteResponse):
    raise ValueError


def file_close(req: plugin_pb2.fileCloseRequest, resp: plugin_pb2.fileCloseResponse):
    raise ValueError


def create_file(req: plugin_pb2.createFileRequest, resp: plugin_pb2.createFileResponse):
    raise ValueError


def set_file_attributes(
    req: plugin_pb2.setFileAttributesRequest, resp: plugin_pb2.setFileAttributesResponse
):
    raise ValueError


def check_file(req: plugin_pb2.checkFileRequest, resp: plugin_pb2.checkFileResponse):
    raise ValueError


def set_acl(req: plugin_pb2.setAclRequest, resp: plugin_pb2.setAclResponse):
    raise ValueError


def get_acl(req: plugin_pb2.getAclRequest, resp: plugin_pb2.getAclResponse):
    raise ValueError


def set_xattr(req: plugin_pb2.setXattrRequest, resp: plugin_pb2.setXattrResponse):
    raise ValueError


def get_xattr(req: plugin_pb2.getXattrRequest, resp: plugin_pb2.getXattrResponse):
    raise ValueError


### BAREOS CORE API


def GetValue(var: bVariable):
    raise ValueError


def SetValue(var: bVariable, val):
    raise ValueError


@dataclass(slots=True)
class SourceLocation:
    line: int
    file: str
    function: str


def DebugMessage(level: int, string: str, caller: SourceLocation = None):
    if caller is None:
        frame_info = inspect.getframeinfo(inspect.stack()[1][0])
        caller = SourceLocation(
            frame_info.lineno, frame_info.filename, frame_info.function
        )
    req = bareos_pb2.CoreRequest()
    dbg_req = req.DebugMessage
    dbg_req.level = level
    dbg_req.msg = string.encode()
    dbg_req.line = caller.line
    dbg_req.file = caller.file.encode()
    dbg_req.function = caller.function.encode()

    log(f"writing debug message {req}")
    con.write_core(req)

    return bRC_OK


def JobMessage(type: bJobMessageType, string: str, caller: SourceLocation = None):
    if caller is None:
        frame_info = inspect.getframeinfo(inspect.stack()[1][0])
        caller = SourceLocation(
            frame_info.lineno, frame_info.filename, frame_info.function
        )

    req = bareos_pb2.CoreRequest()
    job_req = req.JobMessage
    job_req.type = jmsg_type_remote(type)
    job_req.msg = string.encode()
    job_req.line = caller.line
    job_req.file = caller.file.encode()
    job_req.function = caller.function.encode()

    log(f"writing job message {req}")
    con.write_core(req)

    pass


def RegisterEvents(events: list[bEventType]):
    req = bareos_pb2.CoreRequest()
    reg_req = req.Register
    for e in events:
        reg_req.event_types.append(event_type_remote(e))
    con.write_core(req)
    resp = con.read_core()
    return bRC_OK


def UnRegisterEvents(events: list[bEventType]):
    req = bareos_pb2.CoreRequest()
    reg_req = req.Unregister
    for e in events:
        reg_req.event_types.append(event_type_remote(e))
    con.write_core(req)
    resp = con.read_core()
    return bRC_OK


def GetInstanceCount():
    raise ValueError


def AddExclude():
    raise ValueError


def AddInclude():
    raise ValueError


def AddOptions():
    raise ValueError


def AddRegex():
    raise ValueError


def AddWild():
    raise ValueError


def NewOptions():
    raise ValueError


def NewInclude():
    raise ValueError


def NewPreInclude():
    raise ValueError


def CheckChanges(save_pkt: SavePacket):
    raise ValueError


def AcceptFile(save_pkt: SavePacket):
    raise ValueError


def SetSeenBitmap(all: bool, fname: str = None):
    raise ValueError


def ClearSeenBitmap(all: bool, fname: str = None):
    raise ValueError


###


def handle_request(req: plugin_pb2.PluginRequest):
    log(f"handling {req}")

    resp = plugin_pb2.PluginResponse()

    match req.WhichOneof("request"):
        case "handle_plugin":
            # resp.handle_plugin.SetInParent()
            DebugMessage(100, f"got handle_plugin")
            handle_plugin_event(req.handle_plugin, resp.handle_plugin)
        case "start_backup":
            DebugMessage(100, f"got start_backup")
            start_backup(req.start_backup, resp.start_backup)
        case "end_backup_file":
            end_backup_file(req.end_backup_file, resp.end_backup_file)
        case "start_restore_file":
            start_restore_file(req.start_restore_file, resp.start_restore_file)
        case "end_restore_file":
            end_restore_file(req.end_restore_file, resp.end_restore_file)
        case "file_open":
            file_open(req.file_open, resp.file_open)
        case "file_seek":
            file_seek(req.file_seek, resp.file_seek)
        case "file_read":
            file_read(req.file_read, resp.file_read)
        case "file_write":
            file_write(req.file_write, resp.file_write)
        case "file_close":
            file_close(req.file_close, resp.file_close)
        case "create_file":
            create_file(req.create_file, resp.create_file)
        case "set_file_attributes":
            set_file_attributes(req.set_file_attributes, resp.set_file_attributes)
        case "check_file":
            check_file(req.check_file, resp.check_file)
        case "get_acl":
            get_acl(req.get_acl, resp.get_acl)
        case "set_acl":
            set_acl(req.set_acl, resp.set_acl)
        case "get_xattr":
            get_xattr(req.get_xattr, resp.get_xattr)
        case "set_xattr":
            set_xattr(req.set_xattr, resp.set_xattr)
        case "setup":
            resp.setup.SetInParent()
            module_name = "bareos-fd-local-fileset-acl-xattr"
            # module = import_module(module_name)
            log(f"paths = {sys.path}")

            module = __import__(module_name, globals(), locals(), [], 0)

            log(f"got module {module}")
            loader = module["load_bareos_plugin"]
            log(f"got loader {loader}")
        case _:
            raise ValueError

    log(f"hello ???")
    log(f"responding with {resp}")
    return resp


try:
    while True:
        req = con.read_plugin()
        resp = handle_request(req)
        con.write_plugin(resp)

except:
    error = traceback.format_exc()
    log(f"error = {error}")
