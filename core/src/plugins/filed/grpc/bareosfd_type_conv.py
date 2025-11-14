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
import bareosfd_types


assert int(bareosfd_types.bEventJobStart) == bareos_pb2.Event_JobStart
assert int(bareosfd_types.bEventJobEnd) == bareos_pb2.Event_JobEnd
assert int(bareosfd_types.bEventStartBackupJob) == bareos_pb2.Event_StartBackupJob
assert int(bareosfd_types.bEventEndBackupJob) == bareos_pb2.Event_EndBackupJob
assert int(bareosfd_types.bEventStartRestoreJob) == bareos_pb2.Event_StartRestoreJob
assert int(bareosfd_types.bEventEndRestoreJob) == bareos_pb2.Event_EndRestoreJob
assert int(bareosfd_types.bEventStartVerifyJob) == bareos_pb2.Event_StartVerifyJob
assert int(bareosfd_types.bEventEndVerifyJob) == bareos_pb2.Event_EndVerifyJob
assert int(bareosfd_types.bEventBackupCommand) == bareos_pb2.Event_BackupCommand
assert int(bareosfd_types.bEventRestoreCommand) == bareos_pb2.Event_RestoreCommand
assert int(bareosfd_types.bEventEstimateCommand) == bareos_pb2.Event_EstimateCommand
assert int(bareosfd_types.bEventLevel) == bareos_pb2.Event_Level
assert int(bareosfd_types.bEventSince) == bareos_pb2.Event_Since
assert int(bareosfd_types.bEventCancelCommand) == bareos_pb2.Event_CancelCommand
assert int(bareosfd_types.bEventRestoreObject) == bareos_pb2.Event_RestoreObject
assert int(bareosfd_types.bEventEndFileSet) == bareos_pb2.Event_EndFileSet
assert int(bareosfd_types.bEventPluginCommand) == bareos_pb2.Event_PluginCommand
assert int(bareosfd_types.bEventOptionPlugin) == bareos_pb2.Event_OptionPlugin
assert int(bareosfd_types.bEventHandleBackupFile) == bareos_pb2.Event_HandleBackupFile
assert int(bareosfd_types.bEventNewPluginOptions) == bareos_pb2.Event_NewPluginOptions
assert (
    int(bareosfd_types.bEventVssInitializeForBackup)
    == bareos_pb2.Event_VssInitializeForBackup
)
assert (
    int(bareosfd_types.bEventVssInitializeForRestore)
    == bareos_pb2.Event_VssInitializeForRestore
)
assert int(bareosfd_types.bEventVssSetBackupState) == bareos_pb2.Event_VssSetBackupState
assert (
    int(bareosfd_types.bEventVssPrepareForBackup)
    == bareos_pb2.Event_VssPrepareForBackup
)
assert (
    int(bareosfd_types.bEventVssBackupAddComponents)
    == bareos_pb2.Event_VssBackupAddComponents
)
assert (
    int(bareosfd_types.bEventVssPrepareSnapshot) == bareos_pb2.Event_VssPrepareSnapshot
)
assert (
    int(bareosfd_types.bEventVssCreateSnapshots) == bareos_pb2.Event_VssCreateSnapshots
)
assert (
    int(bareosfd_types.bEventVssRestoreLoadComponentMetadata)
    == bareos_pb2.Event_VssRestoreLoadComponentMetadata
)
assert (
    int(bareosfd_types.bEventVssRestoreSetComponentsSelected)
    == bareos_pb2.Event_VssRestoreSetComponentsSelected
)
assert int(bareosfd_types.bEventVssCloseRestore) == bareos_pb2.Event_VssCloseRestore
assert int(bareosfd_types.bEventVssBackupComplete) == bareos_pb2.Event_VssBackupComplete


def event_type_remote(etype: bareosfd_types.bEventType) -> int:
    return int(etype)


assert int(bareosfd_types.M_ABORT) == bareos_pb2.JMSG_ABORT
assert int(bareosfd_types.M_DEBUG) == bareos_pb2.JMSG_DEBUG
assert int(bareosfd_types.M_FATAL) == bareos_pb2.JMSG_FATAL
assert int(bareosfd_types.M_ERROR) == bareos_pb2.JMSG_ERROR
assert int(bareosfd_types.M_WARNING) == bareos_pb2.JMSG_WARNING
assert int(bareosfd_types.M_INFO) == bareos_pb2.JMSG_INFO
assert int(bareosfd_types.M_SAVED) == bareos_pb2.JMSG_SAVED
assert int(bareosfd_types.M_NOTSAVED) == bareos_pb2.JMSG_NOTSAVED
assert int(bareosfd_types.M_SKIPPED) == bareos_pb2.JMSG_SKIPPED
assert int(bareosfd_types.M_MOUNT) == bareos_pb2.JMSG_MOUNT
assert int(bareosfd_types.M_ERROR_TERM) == bareos_pb2.JMSG_ERROR_TERM
assert int(bareosfd_types.M_TERM) == bareos_pb2.JMSG_TERM
assert int(bareosfd_types.M_RESTORED) == bareos_pb2.JMSG_RESTORED
assert int(bareosfd_types.M_SECURITY) == bareos_pb2.JMSG_SECURITY
assert int(bareosfd_types.M_ALERT) == bareos_pb2.JMSG_ALERT
assert int(bareosfd_types.M_VOLMGMT) == bareos_pb2.JMSG_VOLMGMT


def jmsg_type_remote(jmtype: bareosfd_types.bJobMessageType) -> int:
    return int(jmtype)
