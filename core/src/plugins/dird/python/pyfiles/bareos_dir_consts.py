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

brDirVariable = dict(
    bDirVarJob=1,
    bDirVarLevel=2,
    bDirVarType=3,
    bDirVarJobId=4,
    bDirVarClient=5,
    bDirVarNumVols=6,
    bDirVarPool=7,
    bDirVarStorage=8,
    bDirVarWriteStorage=9,
    bDirVarReadStorage=10,
    bDirVarCatalog=11,
    bDirVarMediaType=12,
    bDirVarJobName=13,
    bDirVarJobStatus=14,
    bDirVarPriority=15,
    bDirVarVolumeName=16,
    bDirVarCatalogRes=17,
    bDirVarJobErrors=18,
    bDirVarJobFiles=19,
    bDirVarSDJobFiles=20,
    bDirVarSDErrors=21,
    bDirVarFDJobStatus=22,
    bDirVarSDJobStatus=23,
    bDirVarPluginDir=24,
    bDirVarLastRate=25,
    bDirVarJobBytes=26,
    bDirVarReadBytes=27,
)

bwDirVariable = dict(
    bwDirVarJobReport=1, bwDirVarVolumeName=2, bwDirVarPriority=3, bwDirVarJobLevel=4
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

bDirEventType = dict(
    bDirEventJobStart=1,
    bDirEventJobEnd=2,
    bDirEventJobInit=3,
    bDirEventJobRun=4,
    bDirEventVolumePurged=5,
    bDirEventNewVolume=6,
    bDirEventNeedVolume=7,
    bDirEventVolumeFull=8,
    bDirEventRecyle=9,
    bDirEventGetScratch=10,
    bDirEventNewPluginOptions=11,
)
