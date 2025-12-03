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
    # cf_remote,
    jmsg_type_remote,
    ft_remote,
    ft_from_remote,
    stat_init_to_remote,
    stat_from_remote,
    replace_from_remote,
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
import queue

from ctypes import set_errno, get_errno

f = open("/tmp/log.out", "w")


def log(msg):
    print(msg, file=f)
    f.flush()
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
    send_bytes = size_bytes + obj_bytes
    sock.sendall(send_bytes)


def writemsg_with_fd(sock, msg, fd):
    obj_bytes = msg.SerializeToString()
    size = len(obj_bytes)
    log(f"sending {msg} (size = {size}) to {sock}")
    size_bytes = size.to_bytes(4, "little")
    log(f"bytes = {len(size_bytes)}, obj = {len(obj_bytes)}")

    send_bytes = size_bytes + obj_bytes
    send_list = [send_bytes]
    bytes_send = socket.send_fds(sock, send_list, [fd])

    log(f"sending fd {fd} to core")
    while bytes_send == 0:
        bytes_send = socket.send_fds(sock, send_list, [fd])

    if bytes_send < len(send_bytes):
        sock.sendall(send_bytes[bytes_send:])


def bit_is_set(flags: bytearray, index: int) -> bool:
    array_index = index // 8
    byte_index = index & 0b111
    byte_mask = 1 << byte_index

    return (flags[array_index] & byte_mask) != 0


class BareosConnection:
    def __init__(self):
        self.core_events = socket.socket(fileno=3)
        self.plugin_requests = socket.socket(fileno=4)
        self.data_transfer = socket.socket(fileno=5)

    def read_event(self) -> plugin_pb2.PluginRequest:
        req = plugin_pb2.PluginRequest()
        readmsg(self.core_events, req)
        return req

    def answer_event(
        self, resp: plugin_pb2.PluginResponse, fd_to_sent: int | None = None
    ):
        if fd_to_sent is None:
            writemsg(self.core_events, resp)
        else:
            writemsg_with_fd(self.core_events, resp, fd_to_sent)

    def submit_request(self, req: bareos_pb2.CoreRequest):
        writemsg(self.plugin_requests, req)

    def get_response(self) -> bareos_pb2.CoreResponse:
        resp = bareos_pb2.CoreResponse()
        readmsg(self.plugin_requests, resp)
        return resp

    def get_data(self) -> restore_pb2.RestoreRequest:
        data = restore_pb2.RestoreRequest()
        readmsg(self.data_transfer, data)
        return data

    def send_file_resp(self, data: restore_pb2.RestoreResponse):
        writemsg(self.data_transfer, data)

    def send_data(self, data: backup_pb2.Backup):
        writemsg(self.data_transfer, data)


### BAREOS PLUGIN API

level: int = None
since: int = None
plugin_loaded: bool = False
module_path: str = None
con: BareosConnection = None
debug_level: int = 1000


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
            log(f"unexpected event conversion for type '{unexpected}'")
            raise ValueError


def parse_options(options: str) -> str:
    if options == "":
        return None
    if options == "*all*":
        return None

    log(f"parsing '{options}'")

    # TODO: if the plugin wasnt loaded yet, we need to keep a copy
    #       of options around and append it to the next option
    #       so that the plugin gets it

    plugin_options = []

    rest = options
    log(f"looking at '{rest}'")
    while rest != "":
        [kv, _, rest] = rest.partition(":")

        log(f"untangle '{kv}'")

        [key, sep, value] = kv.partition("=")

        if sep == "":
            raise ValueError

        if value == "":
            raise ValueError

        match key:
            case "module_name":
                # we need to remove it from the plugin options
                # as this is what the python-fd plugin does
                pass
            case "module_path":
                global module_path
                module_path = value
            case _:
                plugin_options.append(kv)

    inner_options = ":".join(plugin_options)

    log(f"passing to plugin: '{inner_options}'")

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
    req: plugin_pb2.HandlePluginEventRequest, resp: plugin_pb2.HandlePluginEventResponse
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

                log(f"paths = {sys.path}")

                module = __import__(module_name, globals(), locals(), [], 0)

                log(f"got module {module}")

                loader = module.__dict__["load_bareos_plugin"]
                log(f"got loader {loader}")

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
            result = bRC_OK

        case "set_debug_level":
            global debug_level
            log(
                f"setting debug level from {debug_level} to {event.set_debug_level.debug_level}"
            )
            debug_level = event.set_debug_level.debug_level
            result = bRC_OK

        case _:
            if plugin_loaded:
                bevent = bareos_event(event)
                result = BareosFdWrapper.handle_plugin_event(bevent)
            else:
                log(f"got event {event} too early")
                result = bRC_OK

    resp.res = brc_type_remote(result)


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

    con.submit_request(req)
    resp = con.get_response()

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
    con.submit_request(req)

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

    log(f"writing job message {req}")
    con.submit_request(req)

    pass


def RegisterEvents(events: list[bEventType]):
    req = bareos_pb2.CoreRequest()
    reg_req = req.Register
    for e in events:
        reg_req.event_types.append(event_type_remote(e))
    con.submit_request(req)
    resp = con.get_response()
    return bRC_OK


def UnRegisterEvents(events: list[bEventType]):
    req = bareos_pb2.CoreRequest()
    reg_req = req.Unregister
    for e in events:
        reg_req.event_types.append(event_type_remote(e))
    con.submit_request(req)
    resp = con.get_response()
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

    log(f"responding with {resp}")
    if resp is not None:
        con.write_plugin(resp)


log(f"name = '{__name__}'")


def handle_setup(con: BareosConnection):
    event = con.read_event()
    event_type = event.WhichOneof("request")
    match event_type:
        case "setup":
            debug_level = event.setup.initial_debug_level
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
            resp = plugin_pb2.PluginResponse()
            resp.setup.SetInParent()
            con.answer_event(resp)
        case _:
            JobMessage(
                bJobMessageType.M_FATAL,
                f"job did not start with setup but with {event_type}",
            )
            exit(187)  # random looking number


@dataclass(slots=True)
class Restore:
    cmd: str
    last_msg: restore_pb2.RestoreRequest | None = None
    last_type: str | None = None

    current_file: restore_pb2.BeginFileRequest | None = None
    current_offset: int = 0
    data_done: bool = False
    extract: bool = False

    def pull(self):
        self.last_msg = con.get_data()
        self.last_type = self.last_msg.WhichOneof("request")
        log(f"got msg of type {self.last_type}")

    def have(self, msg_type):
        if self.last_type == None:
            self.pull()
        return self.last_type == msg_type

    def push(self, msg):
        assert self.last_msg is None
        self.last_msg = msg
        self.last_type = msg.WhichOneof("request")

    def peek(self):
        if self.last_msg == None:
            self.pull()

        return self.last_msg

    def pop(self):
        if self.last_msg == None:
            self.pull()

        msg = self.last_msg
        self.last_msg = None
        self.last_type = None
        return msg

    def start_restore_file(self):
        log("start_restore_file")
        if not self.have("begin_file"):
            raise ValueError

        assert self.current_file is None

        self.current_file = self.pop().begin_file

        set_errno(0)
        res = BareosFdWrapper.start_restore_file(self.cmd)

        answer = restore_pb2.RestoreResponse()
        bf = answer.begin_file

        if res == bRC_Error:
            log(f"could not start file")
            bf.system_error = get_errno()
            con.send_file_resp(answer)
            self.current_file = None
            return

        pkt = RestorePacket()
        # pkt.delta_seq = self.current_file.delta_seq
        pkt.replace = replace_from_remote(self.current_file.replace)
        pkt.where = self.current_file.where.decode()
        pkt.statp = stat_from_remote(self.current_file.stats)
        pkt.type = ft_from_remote(self.current_file.file_type)
        pkt.olname = self.current_file.soft_link_to.decode()
        pkt.ofname = self.current_file.output_name.decode()

        res = BareosFdWrapper.create_file(pkt)

        if res == bRC_Error:
            log(f"could not create file")
            bf.system_error = get_errno()
            con.send_file_resp(answer)
            self.current_file = None
            return

        do_core = False

        match pkt.create_status:
            case bCFs.CF_ERROR:
                bf.system_error = get_errno()
                log(f"could not create file (2)")
                self.current_file = None
                self.extract = False
            case bCFs.CF_SKIP:
                log(f"skipping file")
                bf.success = restore_pb2.CREATION_SKIP
                self.current_file = None
                self.extract = False
            case bCFs.CF_CORE:
                log(f"creating file in core")
                bf.success = restore_pb2.CREATION_CORE
                do_core = True
                self.extract = True
            case bCFs.CF_EXTRACT:
                log(f"doing file in plugin")
                bf.success = restore_pb2.CREATION_PLUGIN
                self.extract = True
            case bCFs.CF_CREATED:
                log(f"file with no data")
                bf.success = restore_pb2.CREATION_NODATA
                self.extract = False
            case 0:
                # this looks weird right ?
                # sadly bareos does not correctly check this value
                # so a lot of plugins simply set it to 0
                log(f"found bad plugin ...")
                bf.success = restore_pb2.CREATION_PLUGIN
                self.extract = True
            case unexpected:
                log(f"unexpected return value '{unexpected}'")
                raise ValueError

        con.send_file_resp(answer)

        if do_core:
            if not have("core_creation_done"):
                JobMessage(
                    bJobMessageType.M_FATAL,
                    "plugin requested the core to do the file creation, but core did not do so",
                )
            else:
                creation_done = pop()
                cd = creation_done.creation_done
                if cd.success:
                    DebugMessage(200, "core created file, we can continue with restore")
                else:
                    DebugMessage(200, "core failed createing file; skipping")
                    self.current_file = None

    def end_restore_file(self):
        log("end_restore_file")
        res = BareosFdWrapper.end_restore_file()
        if res == bRC_Error:
            path = self.current_file.output_name
            JobMessage(bJobMessageType.M_ERROR, f"could not end restore of file {path}")

        self.current_file = None
        self.current_offset = 0
        self.data_done = False
        self.extract = False

    def file_open(self):
        if self.current_file is None:
            log(f"cannot open file as its none")
            return

        log("fopen")
        path = self.current_file.output_name
        iop = IoPacket()
        iop.func = bIOPS.IO_OPEN
        iop.fname = path.decode()
        iop.flags = os.O_WRONLY | os.O_CREAT | os.O_TRUNC
        if hasattr(os, "O_BINARY"):
            iop.flags |= os.O_BINARY

        iop.mode = stat.S_IRUSR | stat.S_IWUSR

        # handle io_in_core as well here
        result = BareosFdWrapper.plugin_io(iop)

        if result != bRC_OK:
            JobMessage(bJobMessageType.M_ERROR, f"could not open {path}")
            self.current_file = None

    def file_write(self):
        log("fwrite")
        if not self.have("file_data"):
            self.data_done = True
            return

        msg = self.pop()

        if self.current_file is None:
            self.data_done = True
            return

        bf = msg.file_data

        if bf.offset != self.current_offset:
            iop = IoPacket()
            iop.func = bIOPS.IO_SEEK
            iop.whence = SEEK_SET
            iop.offset = bf.offset

            if BareosFdWrapper.plugin_io(iop) == bRC_Error:
                path = self.current_file.output_name
                JobMessage(
                    bJobMessageType.M_ERROR, f"could not seek to {bf.offset} in {path}"
                )
                self.file_close()
                self.current_file = None
                return

            self.current_offset = bf.offset

        if self.current_file is None:
            return

        iop = IoPacket()
        iop.func = bIOPS.IO_WRITE
        iop.count = len(bf.data)
        iop.buf = bf.data

        if BareosFdWrapper.plugin_io(iop) == bRC_Error:
            path = self.current_file.output_name
            JobMessage(
                bJobMessageType.M_ERROR,
                f"could not write '{len(msg.data)}' bytes at offset '{current_offset}' in file '{path}'",
            )
            self.file_close()
            self.current_file = None
            return

        if iop.status != len(bf.data):
            JobMessage(
                bJobMessageType.M_ERROR,
                f"could not write '{len(msg.data)}' bytes at offset '{current_offset}' in file '{path}': only wrote {iop.status} / {iop.count} bytes",
            )
            self.file_close()
            self.current_file = None
            return

    def file_close(self):
        log("fclose")
        if self.current_file is None:
            return

        iop = IoPacket()
        iop.func = bIOPS.IO_CLOSE
        result = BareosFdWrapper.plugin_io(iop)

        if result == bRC_Error:
            path = self.current_file.output_name
            JobMessage(bJobMessageType.M_ERROR, f"could not close file '{path}'")
            self.current_file = None
            return

    def set_file_attributes(self):
        log("set_file_attributes")
        if self.current_file is None:
            return

        log(f"stats = {self.current_file.stats}")

        rp = RestorePacket()
        rp.replace = replace_from_remote(self.current_file.replace)
        rp.where = self.current_file.where.decode()
        rp.statp = stat_from_remote(self.current_file.stats)
        rp.type = ft_from_remote(self.current_file.file_type)
        rp.olname = self.current_file.soft_link_to.decode()
        rp.ofname = self.current_file.output_name.decode()

        res = BareosFdWrapper.set_file_attributes(rp)
        if res == bRC_Error:
            path = self.current_file.output_name
            JobMessage(
                bJobMessageType.M_ERROR, f"could not set attributes of file '{path}'"
            )
            self.current_file = None
            return

        if rp.create_status == bCFs.CF_CORE:
            path = self.current_file.output_name
            log(f"::::::::::: cannot set in core")
            JobMessage(
                bJobMessageType.M_ERROR,
                f"the grpc layer does not support set_file_attributes() in core, and neither does the core. Attributes of '{path}' are not restored",
            )
            self.current_file = None
            return

    def set_acl(self):
        log("acl")
        if not self.have("acl"):
            return

        acl = self.pop()

        if self.current_file is None:
            return

        pkt = AclPacket()
        pkt.content = bytearray(acl.acl_data)
        BareosFdWrapper.set_acl(pkt)

    def set_xattr(self):
        log("xattr")
        if not self.have("xattr"):
            return

        xattr = self.pop()

        if self.current_file is None:
            return

        pkt = XattrPacket()
        pkt.name = bytearray(xattr.name)
        pkt.value = bytearray(acl.content)
        BareosFdWrapper.set_xattr(pkt)

    def run(self):
        while True:
            if self.have("done"):
                break

            self.start_restore_file()

            if self.extract:
                self.file_open()
                while not self.data_done:
                    self.file_write()
                self.file_close()

            self.set_file_attributes()

            self.set_acl()
            self.set_xattr()

            self.end_restore_file()


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
    buffer_size: int

    def backup_fopen(self, path):
        iop = IoPacket()
        iop.func = bIOPS.IO_OPEN
        iop.fname = path
        iop.flags = os.O_RDONLY
        if hasattr(os, "O_BINARY"):
            iop.flags |= os.O_BINARY

        if self.no_atime:
            iop.flags |= os.O_NOATIME

        iop.mode = 0

        result = BareosFdWrapper.plugin_io(iop)

        if result != bRC_OK:
            error = backup.Backup()
            error.file_error.on_action = common_pb2.FILE_ACTION_OPEN
            error.file_error.message = b"????"
            con.write_data(error)

            # FIXME
            raise ValueError

        if iop.status == iostat_do_in_core:
            return iop.filedes
        else:
            return None

    def backup_fclose(self):
        iop = IoPacket()

        iop.func = bIOPS.IO_CLOSE
        result = BareosFdWrapper.plugin_io(iop)

        if result != bRC_OK:
            error = backup.Backup()
            error.file_error.on_action = common_pb2.FILE_ACTION_CLOSE
            error.file_error.message = b"????"
            con.write_data(error)
            # FIXME
            raise ValueError

    def backup_fread(self, num_bytes: int):
        iop = IoPacket()

        iop.func = bIOPS.IO_READ
        iop.count = num_bytes

        result = BareosFdWrapper.plugin_io(iop)

        if result != bRC_OK:
            error = backup.Backup()
            error.file_error.on_action = common_pb2.FILE_ACTION_READ
            error.file_error.message = b"????"
            con.write_data(error)
            # FIXME
            raise ValueError

        if iop.status == 0:
            log(f"received 0 bytes")
            return None

        log(f"received {iop.status} bytes")
        return bytes(iop.buf[: iop.status])

    def backup_file_content(self, path: str):
        log(f"backing up {path}")
        filedes = self.backup_fopen(path)

        if filedes is None:
            resp = backup_pb2.Backup()
            data = resp.file_data
            while True:
                data_to_send = self.backup_fread(self.buffer_size)
                if data_to_send is None:
                    data.data = bytes()
                    con.send_data(resp)
                    break
                else:
                    data.data = data_to_send
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
            acl.acl_data = bytes()
        else:
            acl.acl_data = bytes(ap.content)

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
            xattr.content = bytes(xp.value)
            con.send_data(resp)

            if result != bRC_More:
                break

    def run(self):
        while True:
            pkt = SavePacket()
            pkt.cmd = self.cmd
            pkt.portable = self.portable

            # nobody seems to use it, so we do not set it up correctly
            # if necessary, we can provide it via the core
            pkt.flags = bytearray(len(bFileOption))

            result = BareosFdWrapper.start_backup_file(pkt)

            if result == bRC_Skip:
                continue

            if result == bRC_Stop:
                break

            if result != bRC_OK:
                # FIXME
                raise ValueError

            (category, file_type) = ft_remote(pkt.type)

            match category:
                case common_pb2.ObjectType:
                    resp = backup_pb2.Backup()
                    bo = resp.begin_object

                    if pkt.object_len == 0:
                        JobMessage(
                            bJobMessageType.M_FATAL, "can not create object of size 0"
                        )
                        raise ValueError

                    obj = bo.object
                    obj.name = pkt.object_name.encode()
                    obj.data = bytes(pkt.object[: pkt.object_len])
                    obj.index = pkt.object_index
                    obj.type = file_type

                    con.send_data(resp)

                case common_pb2.FileType:

                    resp = backup_pb2.Backup()
                    obj = resp.begin_object

                    obj.file.no_read = pkt.no_read
                    log(f"no_read({pkt.fname}) -> {pkt.no_read}")
                    obj.file.portable = pkt.portable
                    if bit_is_set(pkt.flags, bFileOption.FO_DELTA):
                        obj.file.delta_seq = pkt.delta_seq
                    obj.file.offset_backup = bit_is_set(
                        pkt.flags, bFileOption.FO_OFFSETS
                    )
                    obj.file.sparse_backup = bit_is_set(
                        pkt.flags, bFileOption.FO_SPARSE
                    )

                    obj.file.file = pkt.fname.encode()
                    obj.file.ft = file_type

                    stat_init_to_remote(obj.file.stats, pkt.statp)

                    if type == common_pb2.SoftLink:
                        obj.file.link = pkt.link.encode()

                    con.send_data(resp)

                    self.backup_file_content(pkt.fname)

                    if self.enable_acl:
                        self.backup_acl()

                    if self.enable_xattr:
                        self.backup_xattr()

                case common_pb2.FileErrorType:
                    raise ValueError

            result = BareosFdWrapper.end_backup_file()
            if result != bRC_More:
                break

        done = backup_pb2.Backup()
        done.done.SetInParent()
        con.send_data(done)


@dataclass(slots=True)
class CoreEventHandler:
    con: BareosConnection
    unhandled_events: queue.Queue = queue.Queue()
    quit: bool = False

    def run(self):
        try:
            while not self.quit:
                event = self.con.read_event()
                if not self.handle_event(event):
                    self.unhandled_events.put(event)
        except:
            log(f"CoreEventHandler: emergency done")
            self.unhandled_events.shutdown()
            log(f"CoreEventHandler: emergency done (shutdown)")

        log(f"CoreEventHandler: done")
        self.unhandled_events.shutdown()
        log(f"CoreEventHandler: done (shutdown)")

    def next_event(self) -> plugin_pb2.PluginRequest:
        return self.unhandled_events.get()

    def event_handled(self):
        self.unhandled_events.task_done()

    def handle_event(self, event: plugin_pb2.PluginRequest) -> bool:
        event_type = event.WhichOneof("request")
        match event_type:
            case "setup":
                JobMessage(bJobMessageType.M_FATAL, "job sent a second setup message")
                self.quit = True
                return True
            case "handle_plugin_event":
                pevent = event.handle_plugin_event.to_handle
                pevent_type = pevent.WhichOneof("event")
                match pevent_type:
                    case "set_debug_level":
                        global debug_level
                        debug_level = pevent.set_debug_level.debug_level
                        return True
                    case "job_end":
                        self.quit = True
                        return False
                    # we should probably still receive stuff like JobEnd
                    # and so on
                    # case "cancel_command":
                    #     self.quit = True
                    #     return False
                    case _:
                        return False

        return False


def handle_core_events(handler: CoreEventHandler):
    handler.run()


def handle_event(event: plugin_pb2.PluginRequest):
    event_type = event.WhichOneof("request")
    match event_type:
        case "begin_backup":
            resp = plugin_pb2.PluginResponse()
            resp.begin_backup.SetInParent()
            con.answer_event(resp)
            bb = event.begin_backup
            Backup(
                cmd=bb.cmd,
                portable=bb.portable,
                enable_acl=bb.backup_acl,
                enable_xattr=bb.backup_xattr,
                no_atime=bb.no_atime,
                buffer_size=bb.max_record_size,
            ).run()
        case "begin_restore":
            resp = plugin_pb2.PluginResponse()
            resp.begin_restore.SetInParent()
            con.answer_event(resp)
            br = event.begin_restore
            Restore(cmd=br.cmd).run()
        case "begin_estimate":
            resp = plugin_pb2.PluginResponse()
            resp.begin_estimate.SetInParent()
            con.answer_event(resp)
            Estimate().run()
        case "begin_verify":
            resp = plugin_pb2.PluginResponse()
            resp.begin_verify.SetInParent()
            con.answer_event(resp)
            Verify().run()
        case "handle_plugin_event":
            resp = plugin_pb2.PluginResponse()
            handle_plugin_event(event.handle_plugin_event, resp.handle_plugin_event)
            con.answer_event(resp)
        case _:
            raise ValueError


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
        log(
            f"plugin_requests = {con.plugin_requests}, core_events = {con.core_events}, data_transfer = {con.data_transfer}"
        )

        handle_setup(con)

        handler = CoreEventHandler(con)
        handler_thread = threading.Thread(target=handle_core_events, args=(handler,))
        handler_thread.start()

        while True:
            try:
                event = handler.next_event()
                handle_event(event)
                handler.event_handled()
            except queue.ShutDown:
                break

        handler_thread.join()
        f.close()
    except:
        error = traceback.format_exc()
        log(f"error = {error}")
