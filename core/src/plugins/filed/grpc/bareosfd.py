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
from bareosfd_type_conv import (
    event_type_remote,
    brc_type_remote,
    sbf_remote,
    jmsg_type_remote,
    ft_remote,
)
import BareosFdWrapper

from proto import plugin_pb2
from proto import bareos_pb2
from proto import events_pb2
from proto import common_pb2
from proto import backup_pb2
from proto import restore_pb2

from os import SEEK_END, SEEK_CUR, SEEK_SET
import os

from enum import IntEnum

import socket
import traceback
import sys
import inspect
import io
import threading

f = open("/tmp/log.out", "w")


def log(msg):
    print(msg, file=f)
    # print(msg, file=sys.stderr)
    pass


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
    # log(f"size = {size}")

    obj_bytes = readnbyte(sock, size)
    msg.ParseFromString(obj_bytes)

    # log(f"request = {msg}")


def writemsg(sock, msg):
    obj_bytes = msg.SerializeToString()
    size = len(obj_bytes)
    # log(f"sending {msg} (size = {size}) to {sock}")
    size_bytes = size.to_bytes(4, "little")
    # log(f"bytes = {len(size_bytes)}, obj = {len(obj_bytes)}")
    send_bytes = size_bytes + obj_bytes
    sock.sendall(send_bytes)
    # sock.sendall(obj_bytes)


def writemsg_with_fd(sock, msg, fd):
    obj_bytes = msg.SerializeToString()
    size = len(obj_bytes)
    # log(f"sending {msg} (size = {size}) to {sock}")
    size_bytes = size.to_bytes(4, "little")
    # log(f"bytes = {len(size_bytes)}, obj = {len(obj_bytes)}")

    send_bytes = size_bytes + obj_bytes
    send_list = [send_bytes]
    bytes_send = socket.send_fds(sock, send_list, [fd])

    # log(f"sending fd {fd} to core")
    while bytes_send == 0:
        bytes_send = socket.send_fds(sock, send_list, [fd])

    if bytes_send < len(send_bytes):
        sock.sendall(send_bytes[bytes_send:])

    # sock.sendall(obj_bytes)


def bit_is_set(flags: bytearray, index: int) -> bool:
    array_index = index // 8
    byte_index = index & 0b111
    byte_mask = 1 << byte_index

    return (flags[array_index] & byte_mask) != 0


class BareosConnection:
    def __init__(self):
        self.plugin_requests = socket.socket(fileno=3)
        self.core_events = socket.socket(fileno=4)
        self.data_transfer = socket.socket(fileno=5)

    def read_event(self):
        req = plugin_pb2.PluginRequest()
        readmsg(self.core_events)
        return req

    def answer_event(
        self, resp: plugin_pb2.PluginResponse, fd_to_sent: int | None = None
    ):
        if fd_to_sent is None:
            writemsg(self.core_events, resp)
        else:
            writemsg_with_fd(self.core_events, resp, fd_to_sent)

    def submit_request(self,
                       req: bareos_pb2.CoreRequest):
        writemsg(self.plugin_requests, req)

    def get_response(self) -> bareos_pb2.CoreResponse:
        resp = bareos_pb2.CoreResponse()
        readmsg(self.core, resp)
        return resp

    def get_data(self) -> restore_pb2.Restore:
        data = restore_pb2.Restore()
        readmsg(self.data_transfer, data)
        return data

    def send_data(self, data: backup_pb2.Backup):
        writemsg(self.data_transfer, data)


### BAREOS PLUGIN API

level: int = None
since: int = None
plugin_loaded: bool = False
module_path: str = None
quit: bool = False
con: BareosConnection = None
debug_level: int = 2


def bareos_event(event: events_pb2.Event):
    match event.WhichOneof("event"):
        # case "restore_object": pass
        # case "handle_backup_file": pass

        case "job_start":
            return bEventType.bEventJobStart
        case "job_end":
            return bEventType.bEventJobEnd
        case "start_backup_job":
            return bEventType.bEventStartBackupJob
        case "end_backup_job":
            return bEventType.bEventEndBackupJob
        case "start_restore_job":
            return bEventType.bEventStartRestoreJob
        case "end_restore_job":
            return bEventType.bEventEndRestoreJob
        case "start_verify_job":
            return bEventType.bEventStartVerifyJob
        case "end_verify_job":
            return bEventType.bEventEndVerifyJob
        case "cancel_command":
            return bEventType.bEventCancelCommand
        case "end_fileset":
            return bEventType.bEventEndFileSet
        case "option_plugin":
            return bEventType.bEventOptionPlugin
        case "vss_init_backup":
            return bEventType.bEventVssInitializeForBackup
        case "vss_init_restore":
            return bEventType.bEventVssInitializeForRestore
        case "vss_set_backup_state":
            return bEventType.bEventVssSetBackupState
        case "vss_prepare_for_backup":
            return bEventType.bEventVssPrepareForBackup
        case "vss_backup_add_components":
            return bEventType.bEventVssBackupAddComponents
        case "vss_prepare_snapshot":
            return bEventType.bEventVssPrepareSnapshot
        case "vss_create_snapshot":
            return bEventType.bEventVssCreateSnapshots
        case "vss_restore_load_companents_metadata":
            return bEventType.bEventVssRestoreLoadComponentMetadata
        case "vss_restore_set_components_selected":
            return bEventType.bEventVssRestoreSetComponentsSelected
        case "vss_close_restore":
            return bEventType.bEventVssCloseRestore
        case "vss_backup_complete":
            return bEventType.bEventVssBackupComplete

        # these are _not_ handled here in the generic code
        # case "backup_command":
        # case "restore_command":
        # case "estimate_command":
        # case "plugin_command":
        # case "new_plugin_options":
        # case "level":
        # case "since":

        case unexpected:
            # log(f"unexpected event conversion for type '{unexpected}'")
            raise ValueError


def parse_options(options: str) -> str:
    if options == "":
        return None
    if options == "*all*":
        return None

    # log(f"parsing '{options}'")

    # TODO: if the plugin wasnt loaded yet, we need to keep a copy
    #       of options around and append it to the next option
    #       so that the plugin gets it

    plugin_options = []

    rest = options
    # log(f"looking at '{rest}'")
    while rest != "":
        [kv, _, rest] = rest.partition(":")

        # log(f"untangle '{kv}'")

        [key, sep, value] = kv.partition("=")

        if sep == "":
            raise ValueError

        if value == "":
            raise ValueError

        match key:
            # case "module_name":
            #    global module_name
            #    module_name = value
            #    pass
            case "module_path":
                global module_path
                module_path = value
                pass
            case _:
                plugin_options.append(kv)

    inner_options = ":".join(plugin_options)

    # log(f"passing to plugin: '{inner_options}'")

    return inner_options


def handle_plugin_options(options: str) -> bRCs:
    global plugin_loaded

    plugin_options = parse_options(options)

    if plugin_options is None:
        return bRC_Skip

    if not plugin_loaded:
        # global module_name

        # # module = import_module(module_name)
        # log(f"paths = {sys.path}")

        # module = __import__(module_name, globals(), locals(), [], 0)

        # log(f"got module {module}")

        # loader = module.__dict__["load_bareos_plugin"]
        # log(f"got loader {loader}")

        # loader(plugin_options)

        BareosFdWrapper.load_bareos_plugin(plugin_options)
        plugin_loaded = True

    return BareosFdWrapper.parse_plugin_definition(plugin_options)


def handle_plugin_event(
    req: plugin_pb2.handlePluginEventRequest, resp: plugin_pb2.handlePluginEventResponse
):
    result = bRC_Error
    event = req.to_handle

    global plugin_loaded

    match event.WhichOneof("event"):
        case "backup_command":
            result = handle_plugin_options(event.backup_command.data)
        case "restore_command":
            result = handle_plugin_options(event.restore_command.data)
        case "estimate_command":
            result = handle_plugin_options(event.estimate_command.data)
        case "plugin_command":
            result = handle_plugin_options(event.plugin_command.data)
        case "new_plugin_options":
            result = handle_plugin_options(event.new_plugin_options.data)
        case "level":
            level = event.level.level
            result = bRC_OK
        case "since":
            since = event.since.since.seconds
            result = bRC_OK
        case "end_restore_object":
            # the plugin does not get to see this for some reason ...
            result = bRC_OK
        case "restore_object":
            rop = event.rop
            if not plugin_loaded:
                plugin_options = parse_options(rop.used_cmd_string)
                if plugin_options is None:
                    raise ValueError

                global module_name

                # log(f"paths = {sys.path}")

                module = __import__(module_name, globals(), locals(), [], 0)

                # log(f"got module {module}")

                loader = module.__dict__["load_bareos_plugin"]
                # log(f"got loader {loader}")

                result = loader(plugin_options)
                if result != bRC_OK:
                    raise ValueError
                    # break
                plugin_loaded = True

                result = BareosFdWrapper.parse_plugin_definition(plugin_options)
                if result != bRC_OK:
                    raise ValueError
                    # break

            pyrop = RestoreObject()
            pyrop.plugin_name = rop.used_cmd_string
            pyrop.object_len = len(rop.sent.data)
            pyrop.object = rop.sent.data
            pyrop.object_index = rop.sent.index
            pyrop.JobId = rop.jobid
            result = BareosFdWrapper.restore_object_data()

        case "handle_backup_file":
            # this is currently unsupported
            # TODO: better error message
            raise ValueError

        case "job_end":
            if plugin_loaded:
                bevent = bareos_event(event)
                BareosFdWrapper.handle_plugin_event(bevent)
            global quit
            quit = True
            result = bRC_OK

        case "set_debug_level":
            global debug_level
            log(f"setting debug level from {debug_level} to {event.set_debug_level.debug_level}")
            debug_level = event.set_debug_level.debug_level
            result = bRC_OK

        case _:
            if plugin_loaded:
                bevent = bareos_event(event)
                BareosFdWrapper.handle_plugin_event(bevent)

    resp.res = brc_type_remote(result)


io_in_core_fd: int | None = None
last_file_done: bool = False


def start_backup(req: plugin_pb2.StartBackupFileRequest):
    global last_file_done
    global io_in_core_fd

    if io_in_core_fd is not None:
        # log(f"closing leftover fd {io_in_core_fd}")
        iop = IoPacket()

        iop.func = bIOPS.IO_CLOSE
        iop.filedes = io_in_core_fd
        result = BareosFdWrapper.plugin_io(iop)

        if result != bRC_OK:
            raise ValueError

        int_result = BareosFdWrapper.end_backup_file()
        # log(f"got ebf result = {int_result}")
        if int_result != bRC_More:
            # log(f"after closing, we are done!")
            last_file_done = True
        else:
            # log(f"after closing, we continue!")
            pass

    outer_resp = plugin_pb2.PluginResponse()
    resp = outer_resp.start_backup

    if last_file_done:
        # log(f"we are done!")
        resp.result = plugin_pb2.SBF_Stop
        con.write_plugin(outer_resp)
        return

    pkt = SavePacket()
    pkt.cmd = req.cmd  # maybe needs to be a str instead
    pkt.no_read = req.no_read
    pkt.portable = req.portable
    pkt.flags = bytearray(req.flags)
    result = BareosFdWrapper.start_backup_file(pkt)
    resp.result = sbf_remote(result)

    if result != bRC_OK:
        con.write_plugin(outer_resp)
        return

    (category, type) = ft_remote(pkt.type)

    match category:
        case common_pb2.ObjectType:
            resp.object.index = pkt.index
            resp.object.name = pkt.name
            resp.object.data = pkt.object
            resp.type = type

            con.write_plugin(outer_resp)
        case common_pb2.FileType:
            resp.file.no_read = pkt.no_read
            resp.file.portable = pkt.portable
            if bit_is_set(pkt.flags, bFileOption.FO_DELTA):
                resp.file.delta_seq = pkt.delta_seq
            resp.file.offset_backup = bit_is_set(pkt.flags, bFileOption.FO_OFFSETS)
            resp.file.sparse_backup = bit_is_set(pkt.flags, bFileOption.FO_SPARSE)

            resp.file.file = pkt.fname.encode()
            resp.file.ft = type

            resp.file.stats.dev = pkt.statp.st_dev
            resp.file.stats.ino = pkt.statp.st_ino
            resp.file.stats.mode = pkt.statp.st_mode
            resp.file.stats.nlink = pkt.statp.st_nlink
            resp.file.stats.uid = pkt.statp.st_uid
            resp.file.stats.gid = pkt.statp.st_gid
            resp.file.stats.rdev = pkt.statp.st_rdev
            resp.file.stats.size = pkt.statp.st_size

            resp.file.stats.atime.FromSeconds(pkt.statp.st_atime)
            resp.file.stats.mtime.FromSeconds(pkt.statp.st_mtime)
            resp.file.stats.ctime.FromSeconds(pkt.statp.st_ctime)

            resp.file.stats.blksize = pkt.statp.st_blksize
            resp.file.stats.blocks = pkt.statp.st_blocks

            if type == common_pb2.SoftLink:
                resp.file.link = pkt.link.encode()

            if pkt.no_read:
                con.write_plugin(outer_resp)
                return

            fo_req = plugin_pb2.fileOpenRequest()
            fo_req.file = pkt.fname.encode()
            fo_req.flags = os.O_RDONLY
            fo_req.mode = 0
            fo_resp = plugin_pb2.fileOpenResponse()

            io_in_core_fd = file_open2(fo_req)

            # if fo_res != bRC_OK:
            #     err = plugin_pb2.FileRecord()
            #     err.error = "could not open file"
            #     con.write_plugin(err)
            #     return

            # log(f"file_open2 returned {io_in_core_fd}")
            con.write_plugin(outer_resp)
            if io_in_core_fd is not None:
                # resp.io_in_core = True
                # con.write_plugin(outer_resp, io_in_core_fd)
                # return

                read_file = io.FileIO(io_in_core_fd, closefd=False)
                # read_file = os.fdopen(io_in_core_fd, "r", closefd=False)

                array = bytearray(req.max_record_size)
                while True:
                    size = read_file.readinto(array)
                    if size is None or size == 0:
                        fr = plugin_pb2.FileRecord()
                        fr.data = bytes()
                        con.write_plugin(fr)
                        break
                    else:
                        fr = plugin_pb2.FileRecord()
                        fr.data = bytes(array[:size])
                        con.write_plugin(fr)

                io_in_core_fd = None
            else:
                while True:
                    data = file_read(req.max_record_size)
                    if data is None:
                        fr = plugin_pb2.FileRecord()
                        fr.data = bytes()
                        con.write_plugin(fr)
                        break
                    else:
                        fr = plugin_pb2.FileRecord()
                        fr.data = data
                        con.write_plugin(fr)

            iop = IoPacket()

            iop.func = bIOPS.IO_CLOSE
            result = BareosFdWrapper.plugin_io(iop)

            if result != bRC_OK:
                raise ValueError

            int_result = BareosFdWrapper.end_backup_file()
            # log(f"got ebf result = {int_result}")
            if int_result != bRC_More:
                last_file_done = True

        case common_pb2.FileErrorType:
            resp.error.file = pkt.fname
            resp.error.error = type

            con.write_plugin(outer_resp)
        case _:
            raise ValueError


# def end_backup_file(
#     req: plugin_pb2.endBackupFileRequest, resp: plugin_pb2.endBackupFileResponse
# ):
#     result = BareosFdWrapper.end_backup_file()
#     resp.result = ebf_remote(result)


def start_restore_file(
    req: plugin_pb2.startRestoreFileRequest, resp: plugin_pb2.startRestoreFileResponse
):
    raise ValueError


def end_restore_file(
    req: plugin_pb2.endRestoreFileRequest, resp: plugin_pb2.endRestoreFileResponse
):
    raise ValueError


def file_open2(req: plugin_pb2.fileOpenRequest):
    iop = IoPacket()

    iop.func = bIOPS.IO_OPEN
    iop.fname = req.file.decode()
    iop.flags = req.flags
    iop.mode = req.mode

    result = BareosFdWrapper.plugin_io(iop)

    if result != bRC_OK:
        raise ValueError

    # log(f"plugin_io returned {iop.status}")

    if iop.status == iostat_do_in_core:
        # log(f"do io in core")
        return iop.filedes
    else:
        # log(f"do io in plugin")
        return None


def file_open(req: plugin_pb2.fileOpenRequest, resp: plugin_pb2.fileOpenResponse):
    iop = IoPacket()

    iop.func = bIOPS.IO_OPEN
    iop.fname = req.file.decode()
    iop.flags = req.flags
    iop.mode = req.mode

    result = BareosFdWrapper.plugin_io(iop)

    if result != bRC_OK:
        raise ValueError

    if iop.status == iostat_do_in_core:
        # log(f"do io in core")
        resp.io_in_core = True
    else:
        # log(f"do io in plugin")
        resp.io_in_core = False


def file_seek(req: plugin_pb2.fileSeekRequest, resp: plugin_pb2.fileSeekResponse):
    iop = IoPacket()

    iop.func = bIOPS.IO_SEEK
    match req.whence:
        case plugin_pb2.SS_StartOfFile:
            iop.whence = SEEK_SET
        case plugin_pb2.SS_CurrentPos:
            iop.whence = SEEK_CUR
        case plugin_pb2.SS_EndOfFile:
            iop.whence = SEEK_END
        case _:
            raise ValueError

    iop.offset = req.offset

    result = BareosFdWrapper.plugin_io(iop)
    if result != bRC_OK:
        raise ValueError

    resp.SetInParent()


def file_read(num_bytes: int):
    iop = IoPacket()

    iop.func = bIOPS.IO_READ
    iop.count = num_bytes

    result = BareosFdWrapper.plugin_io(iop)
    if result != bRC_OK:
        raise ValueError

    if len(iop.buf) > num_bytes:
        log(f"got too many bytes {iop.status} > {num_bytes}")
        raise ValueError

    # log(f"read {iop.status} {len(iop.buf)}/{num_bytes} bytes")

    if iop.status == 0:
        return None

    # TODO: maybe we can avoid the copy somehow ? ...
    return bytes(iop.buf[: iop.status])

def file_write(req: plugin_pb2.fileWriteRequest, resp: plugin_pb2.fileWriteResponse):
    raise ValueError

def file_close(req: plugin_pb2.fileCloseRequest, resp: plugin_pb2.fileCloseResponse):
    iop = IoPacket()

    iop.func = bIOPS.IO_CLOSE
    result = BareosFdWrapper.plugin_io(iop)

    if result != bRC_OK:
        raise ValueError

    resp.SetInParent()


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
    req = bareos_pb2.CoreRequest()
    match var:
        case bVariable.bVarFDName:
            req.GetString.var = bareos_pb2.BV_FDName
        case bVariable.bVarClient:
            req.GetString.var = bareos_pb2.BV_ClientName
        case bVariable.bVarJobName:
            req.GetString.var = bareos_pb2.BV_JobName
        case bVariable.bVarWorkingDir:
            req.GetString.var = bareos_pb2.BV_WorkingDir
        case bVariable.bVarWhere:
            req.GetString.var = bareos_pb2.BV_Where
        case bVariable.bVarRegexWhere:
            req.GetString.var = bareos_pb2.BV_RegexWhere
        case bVariable.bVarExePath:
            req.GetString.var = bareos_pb2.BV_ExePath
        case bVariable.bVarVersion:
            req.GetString.var = bareos_pb2.BV_BareosVersion
        case bVariable.bVarPrevJobName:
            req.GetString.var = bareos_pb2.BV_PreviousJobName
        case bVariable.bVarUsedConfig:
            req.GetString.var = bareos_pb2.BV_UsedConfig
        case bVariable.bVarPluginPath:
            req.GetString.var = bareos_pb2.BV_PluginPath

        case bVariable.bVarJobId:
            req.GetInt.var = bareos_pb2.BV_JobId
        case bVariable.bVarLevel:
            req.GetInt.var = bareos_pb2.BV_JobLevel
        case bVariable.bVarType:
            req.GetInt.var = bareos_pb2.BV_JobType
        case bVariable.bVarJobStatus:
            req.GetInt.var = bareos_pb2.BV_JobStatus
        case bVariable.bVarSinceTime:
            req.GetInt.var = bareos_pb2.BV_SinceTime
        case bVariable.bVarAccurate:
            req.GetInt.var = bareos_pb2.BV_Accurate
        case bVariable.bVarPrefixLinks:
            req.GetInt.var = bareos_pb2.BV_PrefixLinks
        case bVariable.bVarFileSeen:
            req.GetInt.var = bareos_pb2.BV_FileSeen
        case bVariable.bVarCheckChanges:
            req.GetFlag.var = bareos_pb2.BV_CheckChanges
        case _:
            raise ValueError

    con.write_core(req)
    resp = con.read_core()

    match resp.WhichOneof("response"):
        case "GetString":
            return resp.GetString.value.decode()
        case "GetInt":
            return resp.GetInt.value
        case "GetFlag":
            return resp.GetFlag.value
        case unexpected:
            log(f"received unexpected type {unexpected}")
            raise ValueError


def SetValue(var: bVariable, val):
    raise ValueError


@dataclass(slots=True)
class SourceLocation:
    line: int
    file: str
    function: str


def DebugMessage(level: int, string: str, caller: SourceLocation = None):
    if debug_level < level:
        return

    if caller is None:
        frame = sys._getframe(1)
        caller = SourceLocation(
            frame.f_lineno, frame.f_code.co_filename, frame.f_code.co_name
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

    # print(string)

    return bRC_OK


def JobMessage(type: bJobMessageType, string: str, caller: SourceLocation = None):
    return

    if caller is None:
        frame = sys._getframe(1)
        caller = SourceLocation(
            frame.f_lineno, frame.f_code.co_filename, frame.f_code.co_name
        )

    req = bareos_pb2.CoreRequest()
    job_req = req.JobMessage
    job_req.type = jmsg_type_remote(type)
    job_req.msg = string.encode()
    job_req.line = caller.line
    job_req.file = caller.file.encode()
    job_req.function = caller.function.encode()

    # log(f"writing job message {req}")
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
    # log(f"handling {req}")

    resp = None

    match req.WhichOneof("request"):
        case "handle_plugin":
            # resp.handle_plugin.SetInParent()
            DebugMessage(100, f"got handle_plugin")
            resp = plugin_pb2.PluginResponse()
            handle_plugin_event(req.handle_plugin, resp.handle_plugin)
        case "start_backup":
            DebugMessage(100, f"got start_backup")
            start_backup(req.start_backup)
        # case "end_backup_file":
        #     resp = plugin_pb2.PluginResponse()
        #     end_backup_file(req.end_backup_file, resp.end_backup_file)
        case "start_restore_file":
            resp = plugin_pb2.PluginResponse()
            start_restore_file(req.start_restore_file, resp.start_restore_file)
        case "end_restore_file":
            resp = plugin_pb2.PluginResponse()
            end_restore_file(req.end_restore_file, resp.end_restore_file)
        case "file_open":
            resp = plugin_pb2.PluginResponse()
            file_open(req.file_open, resp.file_open)
        case "file_seek":
            resp = plugin_pb2.PluginResponse()
            file_seek(req.file_seek, resp.file_seek)
        case "file_write":
            resp = plugin_pb2.PluginResponse()
            file_write(req.file_write, resp.file_write)
        case "file_close":
            resp = plugin_pb2.PluginResponse()
            file_close(req.file_close, resp.file_close)
        case "create_file":
            resp = plugin_pb2.PluginResponse()
            create_file(req.create_file, resp.create_file)
        case "set_file_attributes":
            resp = plugin_pb2.PluginResponse()
            set_file_attributes(req.set_file_attributes, resp.set_file_attributes)
        case "check_file":
            resp = plugin_pb2.PluginResponse()
            check_file(req.check_file, resp.check_file)
        case "get_acl":
            resp = plugin_pb2.PluginResponse()
            get_acl(req.get_acl, resp.get_acl)
        case "set_acl":
            resp = plugin_pb2.PluginResponse()
            set_acl(req.set_acl, resp.set_acl)
        case "get_xattr":
            resp = plugin_pb2.PluginResponse()
            get_xattr(req.get_xattr, resp.get_xattr)
        case "set_xattr":
            resp = plugin_pb2.PluginResponse()
            set_xattr(req.set_xattr, resp.set_xattr)
        case _:
            raise ValueError

    # log(f"responding with {resp}")
    if resp is not None:
        con.write_plugin(resp)


log(f"name = '{__name__}'")

def handle_setup(con: BareosConnection):
    event = con.read_event()
    event_type = event.WhichOneof("request")
    match event_type:
        case "setup":
            RegisterEvents(
                [
                    bEventLevel,
                    bEventSince,
                    bEventNewPluginOptions,
                    bEventPluginCommand,
                    bEventJobStart,
                    bEventRestoreCommand,
                    bEventEstimateCommand,
                    bEventBackupCommand,
                    bEventRestoreObject,
                    bEventJobEnd,
                ]
            )
            resp = plugin_bp2.PluginResponse()
            resp.setup.SetInParent()
            con.answer_event(resp)
        case _:
            JobMessage(bJobMessageType.M_FATAL,
                       f"job did not start with setup but with {event_type}")
            exit(187) # random looking number

class Restore:
    def run(self):
        pass

class Verify:
    def run(self):
        pass

class Estimate:
    def run(self):
        pass


@dataclass(slots=True)
class Backup:
    cmd: str
    portable: bool
    enable_acl: bool
    enable_xattr: bool
    no_atime: bool
    flags: bytearray
    buffer_size: int

    def backup_fopen(self, path):
        iop = IoPacket()
        iop.func = bIOPS.IO_OPEN
        iop.fname = path
        iop.flags = os.O_RDONLY
        if hasattr(os, "O_BINARY"):
            iop.flags |= os.O_BINARY

        if no_atime:
            iop.flags |= os.NO_ATIME

        iop.mode = 0

        result = BareosFDWrapper.plugin_io(iop)

        if result != bRC_OK:
            # FIXME
            raise ValueError

        if iop.status = iostat_do_io_in_core:
            return iop.filedes
        else:
            return None


    def backup_fclose(self):
        iop = IoPacket()

        iop.func = bIOPS.IO_CLOSE
        result = BareosFdWrapper.plugin_io(iop)

        if result != bRC_OK:
            # FIXME
            raise ValueError


    def backup_fread(self, num_bytes: int):
        iop = IoPacket()

        iop.func = bIOPS.IO_READ
        iop.count = num_bytes

        result = BareosFdWrapper.plugin_io(iop)

        if result != bRC_OK:
            # FIXME
            raise ValueError

        if iop.status == 0:
            return None

        return bytes(iop.buf[:iop.status])

    def backup_file_content(self, path: str):
        filedes = self.backup_fopen(path)

        if filedes is None:
            resp = backup_pb2.Backup()
            data = resp.file_data
            while True:
                data.data = self.backup_fread(self.buffer_size)
                if data.data is None:
                    data.data = bytes()
                    con.send_data(resp)
                    break
                else:
                    con.send_data(resp)
        else:
            read_file = io.FileIO(filedes, closefd=False)
            resp = backup_pb2.Backup()
            data = resp.file_data
            array = bytearray(self.buffer_size)

            while True:
                size = read_file.readinto(array)
                if size is None or size == 0:
                    data.data = bytes()
                    con.send_data(resp)
                    break
                else:
                    data.data = bytes(array[:size])
                    con.send_data(resp)

        self.backup_fclose()

    def backup_acl(self):
        ap = AclPacket()

        result = BareosFdWrapper.get_acl(ap)
        if result != bRC_OK:
            # FIXME
            raise ValueError

        resp = backup_pb2.Backup()
        acl = resp.acl
        if ap.content is None:
            acl.content = bytes()
        else:
            acl.content = bytes(ap.content)

        con.send_data(resp)

    def backup_xattr(self):
        xp = XattrPacket()
        resp = backup_pb2.Backup()
        xattr = resp.xattr

        while True:
            result = BareosFdWrapper.get_xattr(xp)

            good_result = result in [bRC_OK, bRC_More]

            if not good_result:
                # FIXME
                raise ValueError

            xattr.name = bytes(xp.name)
            xattr.value = bytes(xp.value)
            con.send_data(resp)

            if result != bRC_More:
                break

    def run(self):
        while True:
            pkt = SavePacket()
            pkt.cmd = self.cmd
            pkt.portable = self.portable
            pkt.flags = self.flags

            result = BareosFdWrapper.start_backup_file(pkt)

            if result == bRC_Skip:
                continue;

            if result == bRC_Stop:
                break

            if result != bRC_OK:
                # FIXME
                raise ValueError

            (category, file_type) = ft_remote(pkt.type)

            match category:
                case common_pb2.ObjectType:
                    pass
                case common_pb2.FileType:
                    self.backup_file_content(pkt.fname)

                    if self.enable_acl:
                        self.backup_acl()

                    if self.enable_xattr:
                        self.backup_xattr()

                case common_pb2.FileErrorType:
                    pass


            self.backup_end()

def handle_events():
    while not quit:
        event = con.read_event()
        event_type = event.WhichOneof("request")
        match event_type:
            case "setup":
                JobMessage(bJobMessageType.M_FATAL,
                           "job sent a second setup message")
                exit(189) # random looking number
            case "begin_backup":
                resp = plugin_bp2.PluginResponse()
                resp.begin_backup.SetInParent()
                con.answer_event(resp)
                bb = event.begin_backup
                return Backup(cmd=bb.cmd,
                              portable=bb.portable,
                              enable_acl=bb.enable_acl,
                              enable_xattr=bb.enable_xattr,
                              no_atime=bb.no_atime,
                              flags=bytearray(bb.flags),
                              buffer_size=bb.max_record_size)
            case "begin_restore":
                resp = plugin_bp2.PluginResponse()
                resp.begin_restore.SetInParent()
                con.answer_event(resp)
                return Restore()
            case "begin_estimate":
                resp = plugin_bp2.PluginResponse()
                resp.begin_estimate.SetInParent()
                con.answer_event(resp)
                return Estimate()
            case "begin_verify":
                resp = plugin_bp2.PluginResponse()
                resp.begin_verify.SetInParent()
                con.answer_event(resp)
                return Verify()
            case "handle_plugin_event":
                resp = plugin_bp2.PluginResponse()
                handle_plugin_event(req, resp.handle_plugin_event)
                con.answer_event(resp)

def run():
    log("##############################################")
    log("########## starting bareosfd plugin ##########")
    log("##############################################")
    try:
        from google.protobuf.internal import api_implementation

        impl = api_implementation.Type()
        log(f"proto impl = {impl}")

        global con
        con = BareosConnection()
        log(f"plugin = {con.plugin}, core = {con.core}, io = {con.io}")

        handle_setup(con)

        while True:
            job_handler = handle_events()
            if quit:
                break
            job_handler.run()

        f.close()
    except:
        error = traceback.format_exc()
        log(f"error = {error}")
