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
class bJobTypes(Enum):
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

    def __init__(self):
        self.object_name = ""
        self.object = bytes()
        self.plugin_name = ""
        self.object_type = 0
        self.object_len = 0
        self.object_full_len = 0
        self.object_index = 0
        self.object_compression = 0
        self.stream = 0
        self.JobId = 0


class StatPacket:
    # uint32_t dev
    # uint64_t ino;
    # uint16_t mode;
    # int16_t nlink;
    # uint32_t uid;
    # uint32_t gid;
    # uint32_t rdev;
    # uint64_t size;
    # time_t atime;
    # time_t mtime;
    # time_t ctime;
    # uint32_t blksize;
    # uint64_t blocks;
    pass


class SavePacket:
    # PyObject* fname;
    # PyObject* link;                /* Link name if any */
    # PyObject* statp;               /* System stat() packet for file */
    # int32_t type;                  /* FT_xx for this file */
    # PyObject* flags;               /* Bareos internal flags */
    # bool no_read;        /* During the save, the file should not be saved */
    # bool portable;       /* set if data format is portable */
    # bool accurate_found; /* Found in accurate list (valid after CheckChanges()) */
    # char* cmd;           /* Command */
    # time_t save_time;    /* Start of incremental time */
    # uint32_t delta_seq;  /* Delta sequence number */
    # PyObject* object_name; /* Object name to create */
    # PyObject* object;      /* Restore object data to save */
    # int32_t object_len;    /* Restore object length */
    # int32_t object_index;  /* Restore object index */
    pass


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
