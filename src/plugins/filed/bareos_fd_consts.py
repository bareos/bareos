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
    M_VOLMGMT=16
)

bVariable = dict(
    bVarJobId=1,
    bVarFDName=2,
    bVarLevel=3,
    bVarType=4,
    bVarClient=5,
    bVarJobName=6,
    bVarJobStatus=7,
    bVarSinceTime=8,
    bVarAccurate=9,
    bVarFileSeen=10,
    bVarVssClient=11,
    bVarWorkingDir=12,
    bVarWhere=13,
    bVarRegexWhere=14,
    bVarExePath=15,
    bVarVersion=16,
    bVarDistName=17,
    bVarPrevJobName=18,
    bVarPrefixLinks=19
)

bFileType = dict(
    FT_LNKSAVED=1,
    FT_REGE=2,
    FT_REG=3,
    FT_LNK=4,
    FT_DIREND=5,
    FT_SPEC=6,
    FT_NOACCESS=7,
    FT_NOFOLLOW=8,
    FT_NOSTAT=9,
    FT_NOCHG=10,
    FT_DIRNOCHG=11,
    FT_ISARCH=12,
    FT_NORECURSE=13,
    FT_NOFSCHG=14,
    FT_NOOPEN=15,
    FT_RAW=16,
    FT_FIFO=17,
    FT_DIRBEGIN=18,
    FT_INVALIDFS=19,
    FT_INVALIDDT=20,
    FT_REPARSE=21,
    FT_PLUGIN=22,
    FT_DELETED=23,
    FT_BASE=24,
    FT_RESTORE_FIRST=25,
    FT_JUNCTION=26,
    FT_PLUGIN_CONFIG=27,
    FT_PLUGIN_CONFIG_FILLED=28
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
    bRC_Cancel=8
)

bCFs = dict(
    CF_SKIP=1,
    CF_ERROR=2,
    CF_EXTRACT=3,
    CF_CREATED=4,
    CF_CORE=5
)

bEventType = dict(
    bEventJobStart=1,
    bEventJobEnd=2,
    bEventStartBackupJob=3,
    bEventEndBackupJob=4,
    bEventStartRestoreJob=5,
    bEventEndRestoreJob=6,
    bEventStartVerifyJob=7,
    bEventEndVerifyJob=8,
    bEventBackupCommand=9,
    bEventRestoreCommand=10,
    bEventEstimateCommand=11,
    bEventLevel=12,
    bEventSince=13,
    bEventCancelCommand=14,
    bEventRestoreObject=15,
    bEventEndFileSet=16,
    bEventPluginCommand=17,
    bEventOptionPlugin=18,
    bEventHandleBackupFile=19,
    bEventNewPluginOptions=20,
    bEventVssInitializeForBackup=21,
    bEventVssInitializeForRestore=22,
    bEventVssBackupAddComponents=23,
    bEventVssPrepareSnapshot=24,
    bEventVssCreateSnapshots=25,
    bEventVssRestoreLoadComponentMetadata=26,
    bEventVssRestoreSetComponentsSelected=27,
    bEventVssCloseRestore=28,
    bEventVssBackupComplete=29
)

bIOPS = dict(
    IO_OPEN=1,
    IO_READ=2,
    IO_WRITE=3,
    IO_CLOSE=4,
    IO_SEEK=5
)
