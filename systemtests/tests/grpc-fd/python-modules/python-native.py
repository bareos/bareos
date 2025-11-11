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

import bareos_bp2

#### BAREOSFD module

pDictbVariable = dict()
pDictbFileType = dict()
pDictbCFs = dict()
pDictbEventType = dict()
pDictbIOPS = dict()
pDictbIOPstatus = dict()
pDictbLevels = dict()

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



def readmsg():
