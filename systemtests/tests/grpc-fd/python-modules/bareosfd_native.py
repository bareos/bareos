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

#### BAREOSFD module

from enum import IntEnum, StrEnum, global_enum

@global_enum
class bVariable(IntEnum):
    bVarJobId = 1
    bVarFDName = 2
    bVarLevel = 3
    bVarType = 4
    bVarClient = 5
    bVarJobName = 6
    bVarJobStatus = 7
    bVarSinceTime = 8
    bVarAccurate = 9
    bVarFileSeen = 10
    bVarVssClient = 11
    bVarWorkingDir = 12
    bVarWhere = 13
    bVarRegexWhere = 14
    bVarExePath = 15
    bVarVersion = 16
    bVarDistName = 17
    bVarPrevJobName = 18
    bVarPrefixLinks = 19
    bVarCheckChanges = 20
    bVarUsedConfig = 21
    bVarPluginPath = 22


@global_enum
class bFileType(IntEnum):
    FT_LNKSAVED = 1
    FT_REGE = 2
    FT_REG = 3
    FT_LNK = 4
    FT_DIREND = 5
    FT_SPEC = 6
    FT_NOACCESS = 7
    FT_NOFOLLOW = 8
    FT_NOSTAT = 9
    FT_NOCHG = 10
    FT_DIRNOCHG = 11
    FT_ISARCH = 12
    FT_NORECURSE = 13
    FT_NOFSCHG = 14
    FT_NOOPEN = 15
    FT_RAW = 16
    FT_FIFO = 17
    FT_DIRBEGIN = 18
    FT_INVALIDFS = 19
    FT_INVALIDDT = 20
    FT_REPARSE = 21
    FT_PLUGIN = 22
    FT_DELETED = 23
    FT_BASE = 24
    FT_RESTORE_FIRST = 25
    FT_JUNCTION = 26
    FT_PLUGIN_CONFIG = 27
    FT_PLUGIN_CONFIG_FILLED = 28


@global_enum
class bCFs(IntEnum):
    CF_SKIP = 1
    CF_ERROR = 2
    CF_EXTRACT = 3
    CF_CREATED = 4
    CF_CORE = 5


@global_enum
class bEventType(IntEnum):
    bEventJobStart = 1
    bEventJobEnd = 2
    bEventStartBackupJob = 3
    bEventEndBackupJob = 4
    bEventStartRestoreJob = 5
    bEventEndRestoreJob = 6
    bEventStartVerifyJob = 7
    bEventEndVerifyJob = 8
    bEventBackupCommand = 9
    bEventRestoreCommand = 10
    bEventEstimateCommand = 11
    bEventLevel = 12
    bEventSince = 13
    bEventCancelCommand = 14
    bEventRestoreObject = 15
    bEventEndFileSet = 16
    bEventPluginCommand = 17
    bEventOptionPlugin = 18
    bEventHandleBackupFile = 19
    bEventNewPluginOptions = 20
    bEventVssInitializeForBackup = 21
    bEventVssInitializeForRestore = 22
    bEventVssSetBackupState = 23
    bEventVssPrepareForBackup = 24
    bEventVssBackupAddComponents = 25
    bEventVssPrepareSnapshot = 26
    bEventVssCreateSnapshots = 27
    bEventVssRestoreLoadComponentMetadata = 28
    bEventVssRestoreSetComponentsSelected = 29
    bEventVssCloseRestore = 30
    bEventVssBackupComplete = 31


@global_enum
class bIOPS(IntEnum):
    IO_OPEN = 1
    IO_READ = 2
    IO_WRITE = 3
    IO_CLOSE = 4
    IO_SEEK = 5


@global_enum
class bIOPstatus(IntEnum):
    iostat_error = -1
    iostat_do_in_plugin = 0
    iostat_do_in_core = 1


@global_enum
class bLevels(StrEnum):
    L_FULL = "F"
    L_INCREMENTAL = "I"
    L_DIFFERENTIAL = "D"
    L_SINCE = "S"
    L_VERIFY_CATALOG = "C"
    L_VERIFY_INIT = "V"
    L_VERIFY_VOLUME_TO_CATALOG = "O"
    L_VERIFY_DISK_TO_CATALOG = "d"
    L_VERIFY_DATA = "A"
    L_BASE = "B"
    L_NONE = " "
    L_VIRTUAL_FULL = "f"


@global_enum
class bJobTypes(StrEnum):
    JT_BACKUP = "B"
    JT_MIGRATED_JOB = "M"
    JT_VERIFY = "V"
    JT_CONSOLE = "U"
    JT_SYSTEM = "I"
    JT_ADMIN = "D"
    JT_ARCHIVE = "A"
    JT_JOB_COPY = "C"
    JT_COPY = "c"
    JT_MIGRATE = "g"
    JT_SCAN = "S"
    JT_CONSOLIDATE = "O"


class RestoreObject:
    """bareos restore object"""

    __slots__ = (
        "object_name",
        "object",
        "plugin_name",
        "object_type",
        "object_len",
        "object_full_len",
        "object_index",
        "object_compression",
        "stream",
        "JobId",
    )

    object_name: str
    object: bytes
    plugin_name: str
    object_type: bFileType
    object_len: int
    object_full_len: int
    object_index: int
    object_compression: int
    stream: int
    JobId: int

    def __init__(self):
        self.object_name = None
        self.object = None
        self.plugin_name = None
        self.object_type = 0
        self.object_len = 0
        self.object_full_len = 0
        self.object_index = 0
        self.object_compression = 0
        self.stream = 0
        self.JobId = 0

    def __repr__(self):
        return (
            "RestoreObject("
            f"object_name = {self.object_name}, "
            f"object = {self.object}, "
            f"plugin_name = {self.plugin_name}, "
            f"object_type = {self.object_type}, "
            f"object_len = {self.object_len}, "
            f"object_full_len = {self.object_full_len}, "
            f"object_index = {self.object_index}, "
            f"object_compression = {self.object_compression}, "
            f"stream = {self.stream}, "
            f"JobId = {self.JobId}"
            ")"
        )


from time import time
import stat


class StatPacket:
    """bareos stat packet"""

    __slots__ = (
        "dev",
        "ino",
        "mode",
        "nlink",
        "uid",
        "gid",
        "rdev",
        "size",
        "atime",
        "mtime",
        "ctime",
        "blksize",
        "blocks",
    )

    def __init__(self):
        now = int(time())
        self.dev = 0
        self.ino = 0
        self.mode = 0o700 | stat.S_IFREG
        self.nlink = 0
        self.uid = 0
        self.gid = 0
        self.rdev = 0
        self.size = -1
        self.atime = now
        self.mtime = now
        self.ctime = now
        self.blksize = 4096
        self.blocks = 1

    def __repr__(self):
        return (
            "StatPacket("
            f"dev = {self.dev}, "
            f"ino = {self.ino}, "
            f"mode = {self.mode}, "
            f"nlink = {self.nlink}, "
            f"uid = {self.uid}, "
            f"gid = {self.gid}, "
            f"rdev = {self.rdev}, "
            f"size = {self.size}, "
            f"atime = {self.atime}, "
            f"mtime = {self.mtime}, "
            f"ctime = {self.ctime}, "
            f"blksize = {self.blksize}, "
            f"blocks = {self.blocks}"
            ")"
        )


class SavePacket:
    """bareos save packet"""

    __slots__ = (
        "fname",
        "link",
        "statp",
        "type",
        "flags",
        "no_read",
        "portable",
        "accurate_found",
        "cmd",
        "save_time",
        "delta_seq",
        "object_name",
        "object",
        "object_len",
        "object_index",
    )

    fname: str
    link: str
    statp: int
    type: int
    flags: int
    no_read: bool
    portable: bool
    accurate_found: bool
    cmd: int
    save_time: int
    delta_seq: int
    object_name: str
    object: bytes
    object_len: int
    object_index: int

    def __init__(self):
        self.fname = None
        self.link = None
        self.statp = 0
        self.type = 0
        self.flags = 0
        self.no_read = False
        self.portable = False
        self.accurate_found = False
        self.cmd = 0
        self.save_time = 0
        self.delta_seq = 0
        self.object_name = None
        self.object = None
        self.object_len = 0
        self.object_index = 0

    def __repr__(self):
        return ("SavePacket("
                f"fname={self.fname}, "
                f"link={self.link}, "
                f"type={self.type}, "
                f"flags = {self.flags}, "
                f"no_read = {self.no_read}, "
                f"portable = {self.portable}, "
                f"accurate_found = {self.accurate_found}, "
                f"cmd = {self.cmd}, "
                f"save_time = {self.save_time}, "
                f"delta_seq = {self.delta_seq}, "
                f"object_name = {self.object_name}, "
                f"object = {self.object}, "
                f"object_len = {self.object_len}, "
                f"object_index = {self.object_index}"
                ")"
                )

class RestorePacket:
    # int32_t stream; /* Attribute stream id */
    # int32_t data_stream;          /* Id of data stream to follow */
    # int32_t type;                 /* File type FT */
    # int32_t file_index;           /* File index */
    # int32_t LinkFI;               /* File index to data if hard link */
    # uint32_t uid;                 /* Userid */
    # PyObject* statp;              /* Decoded stat packet */
    # const char* attrEx;           /* Extended attributes if any */
    # const char* ofname;           /* Output filename */
    # const char* olname;           /* Output link name */
    # const char* where;            /* Where */
    # const char* RegexWhere;       /* Regex where */
    # int replace;                  /* Replace flag */
    # int create_status;            /* Status from createFile() */
    # int filedes;                  /* filedescriptor for read/write in core */
    pass


class IoPacket:
    # uint16_t func; /* Function code */
    # int32_t count;               /* Read/Write count */
    # int32_t flags;               /* Open flags */
    # int32_t mode;                /* Permissions for created files */
    # PyObject* buf;               /* Read/Write buffer */
    # const char* fname;           /* Open filename */
    # int32_t status;              /* Return status */
    # int32_t io_errno;            /* Errno code */
    # int32_t lerror;              /* Win32 error code */
    # int32_t whence;              /* Lseek argument */
    # int64_t offset;              /* Lseek argument */
    # bool win32;                  /* Win32 GetLastError returned */
    # int filedes;                 /* filedescriptor for read/write in core */
    pass


class AclPacket:
    # const char* fname; /* Filename */
    # PyObject* content;               /* ACL content */
    pass


class XattrPacket:
    # const char* fname; /* Filename */
    # PyObject* name;                  /* XATTR name */
    # PyObject* value;                 /* XATTR value */
    pass


#### BAREOSFD MODULE END


# import bareos_bp2
#
#
# def readmsg():
#    pass
