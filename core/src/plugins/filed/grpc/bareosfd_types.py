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

#### BAREOSFD TYPES

from enum import IntEnum, StrEnum, global_enum
from dataclasses import dataclass


@global_enum
class bRCs(IntEnum):
    bRC_OK = 0
    bRC_Stop = 1
    bRC_Error = 2
    bRC_More = 3
    bRC_Term = 4
    bRC_Seen = 5
    bRC_Core = 6
    bRC_Skip = 7
    bRC_Cancel = 8


@global_enum
class bJobMessageType(IntEnum):
    M_ABORT = 1
    M_DEBUG = 2
    M_FATAL = 3
    M_ERROR = 4
    M_WARNING = 5
    M_INFO = 6
    M_SAVED = 7
    M_NOTSAVED = 8
    M_SKIPPED = 9
    M_MOUNT = 10
    M_ERROR_TERM = 11
    M_TERM = 12
    M_RESTORED = 13
    M_SECURITY = 14
    M_ALERT = 15
    M_VOLMGMT = 16


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


@dataclass(slots=True)
class RestoreObject:
    """bareos restore object"""

    object_name: str = None
    object: bytes = None
    plugin_name: str = None
    object_type: bFileType = 0
    object_len: int = 0
    object_full_len: int = 0
    object_index: int = 0
    object_compression: int = 0
    stream: int = 0
    JobId: int = 0


from time import time
import stat


@dataclass(slots=True)
class StatPacket:
    """bareos stat packet"""

    dev: int = 0
    ino: int = 0
    mode: int = 0o700 | stat.S_IFREG
    nlink: int = 0
    uid: int = 0
    gid: int = 0
    rdev: int = 0
    size: int = -1

    # maybe there is a way to initialise these with "now()" somehow ?
    atime: int = 0
    mtime: int = 0
    ctime: int = 0

    blksize: int = 4096
    blocks: int = 1

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


@dataclass(slots=True)
class SavePacket:
    """bareos save packet"""

    fname: str = None
    link: str = None
    statp: int = 0
    type: int = 0
    flags: int = 0
    no_read: bool = False
    portable: bool = False
    accurate_found: bool = False
    cmd: str = None
    save_time: int = 0
    delta_seq: int = 0
    object_name: str = None
    object: bytes = None
    object_len: int = 0
    object_index: int = 0


@dataclass(slots=True)
class RestorePacket:
    """bareos restore packet"""

    stream: int = 0  # Attribute stream id
    data_stream: int = 0  # Id of data stream to follow
    type: int = 0  # File type FT
    file_index: int = 0  # File index
    LinkFI: int = 0  # File index to data if hard link
    uid: int = 0  # Userid
    statp: StatPacket = None  # Decoded stat packet
    attrEx: str = None  # Extended attributes if any
    ofname: str = None  # Output filename
    olname: str = None  # Output link name
    where: str = None  # Where
    RegexWhere: str = None  # Regex where
    replace: int = 0  # Replace flag
    create_status: int = 0  # Status from createFile()
    filedes: int = 0  # filedescriptor for read/write in core


@dataclass(slots=True)
class IoPacket:
    """bareos io packet"""

    func: bIOPS = None  # Function code
    count: int = 0  # Read/Write count
    flags: int = 0  # Open flags
    mode: int = 0  # Permissions for created files
    buf: bytes = None  # Read/Write buffer
    fname: str = None  # Open filename
    status: int = 0  # Return status
    io_errno: int = 0  # Errno code
    lerror: int = 0  # Win32 error code
    whence: int = 0  # Lseek argument
    offset: int = 0  # Lseek argument
    win32: bool = False  # Win32 GetLastError returned
    filedes: int = -1  # filedescriptor for read/write in core


@dataclass(slots=True)
class AclPacket:
    """bareos acl packet"""

    fname: str = None  # Filename
    content: str = None  # ACL content


@dataclass(slots=True)
class XattrPacket:
    """bareos xattr packet"""

    fname: str = None  # Filename
    name: str = None  # XATTR name
    value: str = None  # XATTR value


#### BAREOSFD TYPES END
