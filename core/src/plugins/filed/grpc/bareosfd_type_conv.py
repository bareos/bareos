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

from proto import bareos_pb2
from proto import plugin_pb2
from proto import common_pb2
from proto import restore_pb2
import bareosfd_types
import os

### EventType


def event_type_remote(etype: bareosfd_types.bEventType) -> int:
    return int(etype)


assert event_type_remote(bareosfd_types.bEventJobStart) == bareos_pb2.Event_JobStart
assert event_type_remote(bareosfd_types.bEventJobEnd) == bareos_pb2.Event_JobEnd
assert (
    event_type_remote(bareosfd_types.bEventStartBackupJob)
    == bareos_pb2.Event_StartBackupJob
)
assert (
    event_type_remote(bareosfd_types.bEventEndBackupJob)
    == bareos_pb2.Event_EndBackupJob
)
assert (
    event_type_remote(bareosfd_types.bEventStartRestoreJob)
    == bareos_pb2.Event_StartRestoreJob
)
assert (
    event_type_remote(bareosfd_types.bEventEndRestoreJob)
    == bareos_pb2.Event_EndRestoreJob
)
assert (
    event_type_remote(bareosfd_types.bEventStartVerifyJob)
    == bareos_pb2.Event_StartVerifyJob
)
assert (
    event_type_remote(bareosfd_types.bEventEndVerifyJob)
    == bareos_pb2.Event_EndVerifyJob
)
assert (
    event_type_remote(bareosfd_types.bEventBackupCommand)
    == bareos_pb2.Event_BackupCommand
)
assert (
    event_type_remote(bareosfd_types.bEventRestoreCommand)
    == bareos_pb2.Event_RestoreCommand
)
assert (
    event_type_remote(bareosfd_types.bEventEstimateCommand)
    == bareos_pb2.Event_EstimateCommand
)
assert event_type_remote(bareosfd_types.bEventLevel) == bareos_pb2.Event_Level
assert event_type_remote(bareosfd_types.bEventSince) == bareos_pb2.Event_Since
assert (
    event_type_remote(bareosfd_types.bEventCancelCommand)
    == bareos_pb2.Event_CancelCommand
)
assert (
    event_type_remote(bareosfd_types.bEventRestoreObject)
    == bareos_pb2.Event_RestoreObject
)
assert event_type_remote(bareosfd_types.bEventEndFileSet) == bareos_pb2.Event_EndFileSet
assert (
    event_type_remote(bareosfd_types.bEventPluginCommand)
    == bareos_pb2.Event_PluginCommand
)
assert (
    event_type_remote(bareosfd_types.bEventOptionPlugin)
    == bareos_pb2.Event_OptionPlugin
)
assert (
    event_type_remote(bareosfd_types.bEventHandleBackupFile)
    == bareos_pb2.Event_HandleBackupFile
)
assert (
    event_type_remote(bareosfd_types.bEventNewPluginOptions)
    == bareos_pb2.Event_NewPluginOptions
)
assert (
    event_type_remote(bareosfd_types.bEventVssInitializeForBackup)
    == bareos_pb2.Event_VssInitializeForBackup
)
assert (
    event_type_remote(bareosfd_types.bEventVssInitializeForRestore)
    == bareos_pb2.Event_VssInitializeForRestore
)
assert (
    event_type_remote(bareosfd_types.bEventVssSetBackupState)
    == bareos_pb2.Event_VssSetBackupState
)
assert (
    event_type_remote(bareosfd_types.bEventVssPrepareForBackup)
    == bareos_pb2.Event_VssPrepareForBackup
)
assert (
    event_type_remote(bareosfd_types.bEventVssBackupAddComponents)
    == bareos_pb2.Event_VssBackupAddComponents
)
assert (
    event_type_remote(bareosfd_types.bEventVssPrepareSnapshot)
    == bareos_pb2.Event_VssPrepareSnapshot
)
assert (
    event_type_remote(bareosfd_types.bEventVssCreateSnapshots)
    == bareos_pb2.Event_VssCreateSnapshots
)
assert (
    event_type_remote(bareosfd_types.bEventVssRestoreLoadComponentMetadata)
    == bareos_pb2.Event_VssRestoreLoadComponentMetadata
)
assert (
    event_type_remote(bareosfd_types.bEventVssRestoreSetComponentsSelected)
    == bareos_pb2.Event_VssRestoreSetComponentsSelected
)
assert (
    event_type_remote(bareosfd_types.bEventVssCloseRestore)
    == bareos_pb2.Event_VssCloseRestore
)
assert (
    event_type_remote(bareosfd_types.bEventVssBackupComplete)
    == bareos_pb2.Event_VssBackupComplete
)

### JMSG


def jmsg_type_remote(jmtype: bareosfd_types.bJobMessageType) -> int:
    return int(jmtype)


assert jmsg_type_remote(bareosfd_types.M_ABORT) == bareos_pb2.JMSG_ABORT
assert jmsg_type_remote(bareosfd_types.M_DEBUG) == bareos_pb2.JMSG_DEBUG
assert jmsg_type_remote(bareosfd_types.M_FATAL) == bareos_pb2.JMSG_FATAL
assert jmsg_type_remote(bareosfd_types.M_ERROR) == bareos_pb2.JMSG_ERROR
assert jmsg_type_remote(bareosfd_types.M_WARNING) == bareos_pb2.JMSG_WARNING
assert jmsg_type_remote(bareosfd_types.M_INFO) == bareos_pb2.JMSG_INFO
assert jmsg_type_remote(bareosfd_types.M_SAVED) == bareos_pb2.JMSG_SAVED
assert jmsg_type_remote(bareosfd_types.M_NOTSAVED) == bareos_pb2.JMSG_NOTSAVED
assert jmsg_type_remote(bareosfd_types.M_SKIPPED) == bareos_pb2.JMSG_SKIPPED
assert jmsg_type_remote(bareosfd_types.M_MOUNT) == bareos_pb2.JMSG_MOUNT
assert jmsg_type_remote(bareosfd_types.M_ERROR_TERM) == bareos_pb2.JMSG_ERROR_TERM
assert jmsg_type_remote(bareosfd_types.M_TERM) == bareos_pb2.JMSG_TERM
assert jmsg_type_remote(bareosfd_types.M_RESTORED) == bareos_pb2.JMSG_RESTORED
assert jmsg_type_remote(bareosfd_types.M_SECURITY) == bareos_pb2.JMSG_SECURITY
assert jmsg_type_remote(bareosfd_types.M_ALERT) == bareos_pb2.JMSG_ALERT
assert jmsg_type_remote(bareosfd_types.M_VOLMGMT) == bareos_pb2.JMSG_VOLMGMT

### BRC


def brc_type_remote(brc: bareosfd_types.bRCs) -> int:
    return int(brc) + 1


assert brc_type_remote(bareosfd_types.bRC_OK) == plugin_pb2.RC_OK
assert brc_type_remote(bareosfd_types.bRC_Stop) == plugin_pb2.RC_Stop
assert brc_type_remote(bareosfd_types.bRC_Error) == plugin_pb2.RC_Error
assert brc_type_remote(bareosfd_types.bRC_More) == plugin_pb2.RC_More
assert brc_type_remote(bareosfd_types.bRC_Term) == plugin_pb2.RC_Term
assert brc_type_remote(bareosfd_types.bRC_Seen) == plugin_pb2.RC_Seen
assert brc_type_remote(bareosfd_types.bRC_Core) == plugin_pb2.RC_Core
assert brc_type_remote(bareosfd_types.bRC_Skip) == plugin_pb2.RC_Skip
assert brc_type_remote(bareosfd_types.bRC_Cancel) == plugin_pb2.RC_Cancel

### StartRestoreFile

# def cf_remote(cf: bareosfd_types.bCFs) -> int:
#     return int(cf)
#
# assert cf_remote(bareosfd_types.CF_SKIP) == restore_pb2.CREATION_SKIP
# assert cf_remote(bareosfd_types.CF_CORE) == restore_pb2.CREATION_CORE
# assert cf_remote(bareosfd_types.CF_EXTRACT) == restore_pb2.CREATION_PLUGIN
# assert cf_remote(bareosfd_types.CF_CREATED) == restore_pb2.CREATION_NODATA
# assert cf_remote(bareosfd_types.CF_ERROR) == restore_pb2.CREATION_ERROR

### StartBackupFile


def sbf_remote(brc: bareosfd_types.bRCs):
    raise ValueError


#    match brc:
#        case bareosfd_types.bRC_OK:
#            return plugin_pb2.SBF_OK
#        case bareosfd_types.bRC_Stop:
#            return plugin_pb2.SBF_Stop
#        case bareosfd_types.bRC_Skip:
#            return plugin_pb2.SBF_Skip
#        case _:
#            raise ValueError


# assert sbf_remote(bareosfd_types.bRC_OK) == plugin_pb2.SBF_OK
# assert sbf_remote(bareosfd_types.bRC_Stop) == plugin_pb2.SBF_Stop
# assert sbf_remote(bareosfd_types.bRC_Skip) == plugin_pb2.SBF_Skip

### FileType


def ft_remote(
    ft: bareosfd_types.bFileType,
) -> (type, int):
    # sadly protobuf uses simple ints for enums, so we need to
    # distinguish them with a tuple
    match ft:
        ### object types
        case bareosfd_types.FT_RESTORE_FIRST:
            return (common_pb2.ObjectType, common_pb2.Blob)
        case bareosfd_types.FT_PLUGIN_CONFIG:
            return (common_pb2.ObjectType, common_pb2.Config)
        case bareosfd_types.FT_PLUGIN_CONFIG_FILLED:
            return (common_pb2.ObjectType, common_pb2.FilledConfig)

        ### file types
        case bareosfd_types.FT_LNKSAVED:
            return (common_pb2.FileType, common_pb2.HardlinkCopy)
        case bareosfd_types.FT_REGE:
            return (common_pb2.FileType, common_pb2.RegularFile)
        case bareosfd_types.FT_REG:
            return (common_pb2.FileType, common_pb2.RegularFile)
        case bareosfd_types.FT_LNK:
            return (common_pb2.FileType, common_pb2.SoftLink)
        case bareosfd_types.FT_DIREND:
            return (common_pb2.FileType, common_pb2.Directory)
        case bareosfd_types.FT_SPEC:
            return (common_pb2.FileType, common_pb2.SpecialFile)
        case bareosfd_types.FT_RAW:
            return (common_pb2.FileType, common_pb2.BlockDevice)
        case bareosfd_types.FT_REPARSE:
            return (common_pb2.FileType, common_pb2.ReparsePoint)
        case bareosfd_types.FT_JUNCTION:
            return (common_pb2.FileType, common_pb2.Junction)
        case bareosfd_types.FT_FIFO:
            return (common_pb2.FileType, common_pb2.Fifo)
        case bareosfd_types.FT_DELETED:
            return (common_pb2.FileType, common_pb2.Deleted)
        case bareosfd_types.FT_ISARCH:
            return (common_pb2.FileType, common_pb2.RegularFile)

        ### file error types

        case bareosfd_types.FT_NOACCESS:
            return (common_pb2.FileErrorType, common_pb2.CouldNotAccessFile)
        case bareosfd_types.FT_NOFOLLOW:
            return (common_pb2.FileErrorType, common_pb2.CouldNotFollowLink)
        case bareosfd_types.FT_NOSTAT:
            return (common_pb2.FileErrorType, common_pb2.CouldNotStat)
        case bareosfd_types.FT_NORECURSE:
            return (common_pb2.FileErrorType, common_pb2.RecursionDisabled)
        case bareosfd_types.FT_NOFSCHG:
            return (common_pb2.FileErrorType, common_pb2.CouldNotChangeFilesystem)
        case bareosfd_types.FT_NOOPEN:
            return (common_pb2.FileErrorType, common_pb2.CouldNotOpenDirectory)
        case bareosfd_types.FT_INVALIDFS:
            return (common_pb2.FileErrorType, common_pb2.InvalidFileSystem)
        case bareosfd_types.FT_INVALIDDT:
            return (common_pb2.FileErrorType, common_pb2.InvalidDriveType)

        case _:
            raise ValueError


def ft_from_remote(ft: int) -> bareosfd_types.bFileType:
    # sadly protobuf uses simple ints for enums, so we need to
    # distinguish them with a tuple
    match ft:
        ### file types
        case common_pb2.HardlinkCopy:
            return bareosfd_types.FT_LNKSAVED
        case common_pb2.RegularFile:
            return bareosfd_types.FT_REGE
        case common_pb2.RegularFile:
            return bareosfd_types.FT_REG
        case common_pb2.SoftLink:
            return bareosfd_types.FT_LNK
        case common_pb2.Directory:
            return bareosfd_types.FT_DIREND
        case common_pb2.SpecialFile:
            return bareosfd_types.FT_SPEC
        case common_pb2.BlockDevice:
            return bareosfd_types.FT_RAW
        case common_pb2.ReparsePoint:
            return bareosfd_types.FT_REPARSE
        case common_pb2.Junction:
            return bareosfd_types.FT_JUNCTION
        case common_pb2.Fifo:
            return bareosfd_types.FT_FIFO
        case common_pb2.Deleted:
            return bareosfd_types.FT_DELETED
        case common_pb2.RegularFile:
            return bareosfd_types.FT_ISARCH
        case _:
            raise ValueError


### STAT


def stat_init_to_remote(res: common_pb2.OsStat, stat: bareosfd_types.StatPacket):
    res.dev = stat.st_dev
    res.ino = stat.st_ino
    res.mode = stat.st_mode
    res.nlink = stat.st_nlink
    res.uid = stat.st_uid
    res.gid = stat.st_gid
    res.rdev = stat.st_rdev
    res.size = stat.st_size
    res.atime.FromSeconds(stat.st_atime)
    res.mtime.FromSeconds(stat.st_mtime)
    res.ctime.FromSeconds(stat.st_ctime)
    res.blksize = stat.st_blksize
    res.blocks = stat.st_blocks


def stat_from_remote(stat: common_pb2.OsStat) -> bareosfd_types.StatPacket:
    res = bareosfd_types.StatPacket()
    res.st_dev = stat.dev
    res.st_ino = stat.ino
    res.st_mode = stat.mode
    res.st_nlink = stat.nlink
    res.st_uid = stat.uid
    res.st_gid = stat.gid
    res.st_rdev = stat.rdev
    res.st_size = stat.size
    res.st_atime = stat.atime.ToSeconds()
    res.st_mtime = stat.mtime.ToSeconds()
    res.st_ctime = stat.ctime.ToSeconds()
    res.st_blksize = stat.blksize
    res.st_blocks = stat.blocks
    return res


### REPLACE


def replace_from_remote(rep: int) -> bareosfd_types.bReplace:
    match rep:
        case common_pb2.REPLACE_NEVER:
            return bareosfd_types.bReplace.REPLACE_NEVER
        case common_pb2.REPLACE_IF_OLDER:
            return bareosfd_types.bReplace.REPLACE_IFOLDER
        case common_pb2.REPLACE_IF_NEWER:
            return bareosfd_types.bReplace.REPLACE_IFNEWER
        case common_pb2.REPLACE_ALWAYS:
            return bareosfd_types.bReplace.REPLACE_ALWAYS
        case _:
            raise ValueError
