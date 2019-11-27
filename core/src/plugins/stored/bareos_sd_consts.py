# BAREOS - Backup Archiving REcovery Open Sourced
#
# Copyright (C) 2013-2014 Bareos GmbH & Co. KG
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
#
# Author: Marco van Wieringen
#
bJobMessageType = dict(
    M_ABORT=1,
    M_DEBUG=2,
    M_FATAL=3,
    M_ERROR=4,
    M_WARNING=5,
    M_INFO=6,
    M_SAVED=7,
    M_NOTSAVED=8,
    M_SKIPPED=9,
    M_MOUNT=10,
    M_ERROR_TERM=11,
    M_TERM=12,
    M_RESTORED=13,
    M_SECURITY=14,
    M_ALERT=15,
    M_VOLMGMT=16,
)

bsdrVariable = dict(
    bsdVarJob=1,
    bsdVarLevel=2,
    bsdVarType=3,
    bsdVarJobId=4,
    bsdVarClient=5,
    bsdVarPool=6,
    bsdVarPoolType=7,
    bsdVarStorage=8,
    bsdVarMediaType=9,
    bsdVarJobName=10,
    bsdVarJobStatus=11,
    bsdVarVolumeName=12,
    bsdVarJobErrors=13,
    bsdVarJobFiles=14,
    bsdVarJobBytes=15,
    bsdVarCompatible=16,
    bsdVarPluginDir=17,
)

bsdwVariable = dict(
    bsdwVarJobReport=1, bsdwVarVolumeName=2, bsdwVarPriority=3, bsdwVarJobLevel=4
)

bRCs = dict(
    bRC_OK=0,
    bRC_Stop=1,
    bRC_Error=2,
    bRC_More=3,
    bRC_Term=4,
    bRC_Seen=5,
    bRC_Core=6,
    bRC_Skip=7,
    bRC_Cancel=8,
)

bsdEventType = dict(
    bsdEventJobStart=1,
    bsdEventJobEnd=2,
    bsdEventDeviceInit=3,
    bsdEventDeviceMount=4,
    bsdEventVolumeLoad=5,
    bsdEventDeviceReserve=6,
    bsdEventDeviceOpen=7,
    bsdEventLabelRead=8,
    bsdEventLabelVerified=9,
    bsdEventLabelWrite=10,
    bsdEventDeviceClose=11,
    bsdEventVolumeUnload=12,
    bsdEventDeviceUnmount=13,
    bsdEventReadError=14,
    bsdEventWriteError=15,
    bsdEventDriveStatus=16,
    bsdEventVolumeStatus=17,
    bsdEventSetupRecordTranslation=18,
    bsdEventReadRecordTranslation=19,
    bsdEventWriteRecordTranslation=20,
    bsdEventDeviceRelease=21,
    bsdEventNewPluginOptions=22,
    bsdEventChangerLock=23,
    bsdEventChangerUnlock=24,
)
