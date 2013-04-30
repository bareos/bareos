/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2008-2009 Free Software Foundation Europe e.V.

   The main author of Bacula is Kern Sibbald, with contributions from
   many others, a complete list can be found in the file AUTHORS.
   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation, which is 
   listed in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   General Public License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.

   Bacula® is a registered trademark of Kern Sibbald.
   The licensor of Bacula is the Free Software Foundation Europe
   (FSFE), Fiduciary Program, Sumatrastrasse 25, 8006 Zürich,
   Switzerland, email:ftf@fsfeurope.org.
*/
/* 
 *  Written by James Harper, October 2008
 */

extern "C" {

#define BACKUP_NODE_TYPE_MACHINE                0x001
#define BACKUP_NODE_TYPE_ANNOTATION             0x010
#define BACKUP_NODE_TYPE_DISPLAY                0x100

#define ESE_BACKUP_INSTANCE_END_ERROR   0x0
#define ESE_BACKUP_INSTANCE_END_SUCCESS 0x1

#define BACKUP_TYPE_FULL                        0x1
#define BACKUP_TYPE_LOGS_ONLY                   0x2
#define BACKUP_TYPE_FULL_WITH_ALL_LOGS          0x3

#define RESTORE_CLOSE_ABORT                     0x1
#define RESTORE_CLOSE_NORMAL                    0x0

#define ESE_RESTORE_COMPLETE_NOWAIT             0x00010000
#define ESE_RESTORE_COMPLETE_ATTACH_DBS         0x00000001
#define ESE_RESTORE_KEEP_LOG_FILES              0x00020000

//#include <windef.h>

struct ESE_ICON_DESCRIPTION {
   uint32_t ulSize;
   char *pvData;
};

struct BACKUP_NODE_TREE {
   WCHAR *wszName;
   uint32_t fFlags;
   ESE_ICON_DESCRIPTION iconDescription;
   struct BACKUP_NODE_TREE *pNextNode;
   struct BACKUP_NODE_TREE *pChildNode;
};

struct DATABASE_BACKUP_INFO {
   WCHAR *wszDatabaseDisplayName;
   uint32_t cwDatabaseStreams;
   WCHAR *wszDatabaseStreams;
   GUID rguidDatabase;
   uint32_t *rgIconIndexDatabase;
   uint32_t fDatabaseFlags;
};

struct INSTANCE_BACKUP_INFO {
   uint64_t hInstanceId;
   //RPC_STRING wszInstanceName;
   WCHAR *wszInstanceName;
   uint32_t ulIconIndexInstance;
   uint32_t cDatabase;
   DATABASE_BACKUP_INFO *rgDatabase;
   uint32_t cIconDescription;
   ESE_ICON_DESCRIPTION *rgIconDescription;
};

enum RECOVER_STATUS {
   recoverInvalid          = 0,
   recoverNotStarted       = 1,
   recoverStarted          = 2,
   recoverEnded            = 3,
   recoverStatusMax
};

struct RESTORE_ENVIRONMENT      {
   WCHAR *                 m_wszRestoreLogPath;
   WCHAR *                 m_wszSrcInstanceName;
   uint32_t                m_cDatabases;
   WCHAR                   **m_wszDatabaseDisplayName;
   GUID *                  m_rguidDatabase;
   WCHAR *                 m_wszRestoreInstanceSystemPath;
   WCHAR *                 m_wszRestoreInstanceLogPath;
   WCHAR *                 m_wszTargetInstanceName;
   WCHAR **                m_wszDatabaseStreamsS;
   WCHAR **                m_wszDatabaseStreamsD;
   uint32_t                m_ulGenLow;
   uint32_t                m_ulGenHigh;
   WCHAR *                 m_wszLogBaseName;
   time_t                  m_timeLastRestore;
   RECOVER_STATUS  m_statusLastRecover;
   HRESULT                 m_hrLastRecover;
   time_t                  m_timeLastRecover;
   WCHAR *                 m_wszAnnotation;
};

typedef HANDLE HCCX;

typedef HRESULT (WINAPI *HrESEBackupRestoreGetNodes_t)
(
   WCHAR* wszComputerName,
   BACKUP_NODE_TREE* pBackupNodeTree
);

typedef HRESULT (WINAPI *HrESEBackupPrepare_t)
(
   WCHAR* wszBackupServer,
   WCHAR* wszBackupAnnotation,
   uint32_t *pcInstanceInfo,
   INSTANCE_BACKUP_INFO **paInstanceInfo,
   HCCX *phccxBackupContext
);

typedef HRESULT (WINAPI *HrESEBackupEnd_t)
(
   HCCX hccsBackupContext
);

typedef HRESULT (WINAPI *HrESEBackupSetup_t)
(
   HCCX hccsBackupContext,
   uint64_t hInstanceID,
   uint32_t btBackupType
);

typedef HRESULT (WINAPI *HrESEBackupGetLogAndPatchFiles_t)
(
   HCCX hccsBackupContext,
   WCHAR** pwszFiles
);

typedef HRESULT (WINAPI *HrESEBackupInstanceEnd_t)
(
   HCCX hccsBackupContext,
   uint32_t fFlags
);

typedef HRESULT (WINAPI *HrESEBackupOpenFile_t)
(
   HCCX hccsBackupContext,
   WCHAR* wszFileName,
   uint32_t cbReadHintSize,
   uint32_t cSections,
   void** rghFile,
   uint64_t* rgliSectionSize
);

typedef HRESULT (WINAPI *HrESEBackupReadFile_t)
(
   HCCX hccsBackupContext,
   void* hFile,
   void* pvBuffer,
   uint32_t cbBuffer,
   uint32_t* pcbRead
);

typedef HRESULT (WINAPI *HrESEBackupCloseFile_t)
(
   HCCX hccsBackupContext,
   void* hFile
);

typedef HRESULT (WINAPI *HrESEBackupTruncateLogs_t)
(
   HCCX hccsBackupContext
);

typedef HRESULT (WINAPI *HrESERestoreOpen_t)
(
   WCHAR* wszBackupServer,
   WCHAR* wszBackupAnnotation,
   WCHAR* wszSrcInstanceName,
   WCHAR* wszRestoreLogPath,
   HCCX* phccxRestoreContext
);

typedef HRESULT (WINAPI *HrESERestoreReopen_t)
(
   WCHAR* wszBackupServer,
   WCHAR* wszBackupAnnotation,
   WCHAR* wszRestoreLogPath,
   HCCX* phccxRestoreContext
);

typedef HRESULT (WINAPI *HrESERestoreClose_t)
(
   HCCX phccxRestoreContext,
   uint32_t fRestoreAbort
);

typedef HRESULT (WINAPI *HrESERestoreComplete_t)
(
   HCCX phccxRestoreContext,
   WCHAR* wszCheckpointFilePath,
   WCHAR* wszLogFilePath,
   WCHAR* wszTargetInstanceName,
   uint32_t fFlags
);

typedef HRESULT (WINAPI *HrESERestoreSaveEnvironment_t)
(
   HCCX phccxRestoreContext
);

typedef HRESULT (WINAPI *HrESERestoreGetEnvironment_t)
(
   HCCX phccxRestoreContext,
   RESTORE_ENVIRONMENT **ppRestoreEnvironment
);

typedef HRESULT (WINAPI *HrESERestoreAddDatabase_t)
(
   HCCX phccxRestoreContext,
   WCHAR* wszDatabaseDisplayName,
   GUID guidDatabase,
   WCHAR* wszDatabaseStreamsS,
   WCHAR** wszDatabaseStreamsD
);

typedef HRESULT (WINAPI *HrESERestoreOpenFile_t)
(
   HCCX phccxRestoreContext,
   WCHAR* wszFileName,
   uint32_t cSections,
   void* rghFile
);

bRC
loadExchangeApi();

const char *
ESEErrorMessage(HRESULT result);

#define hrLogfileHasBadSignature    (HRESULT)0xC8000262L
#define hrLogfileNotContiguous      (HRESULT)0xC8000263L
#define hrCBDatabaseInUse           (HRESULT)0xC7FE1F41L
#define hrRestoreAtFileLevel        (HRESULT)0xC7FF0FA5L
#define hrMissingFullBackup         (HRESULT)0xC8000230L
#define hrBackupInProgress          (HRESULT)0xC80001F9L
#define hrCBDatabaseNotFound        (HRESULT)0xC7FE1F42L
#define hrErrorFromESECall          (HRESULT)0xC7FF1004L

extern HrESEBackupRestoreGetNodes_t HrESEBackupRestoreGetNodes;
extern HrESEBackupPrepare_t HrESEBackupPrepare;
extern HrESEBackupGetLogAndPatchFiles_t HrESEBackupGetLogAndPatchFiles;
extern HrESEBackupTruncateLogs_t HrESEBackupTruncateLogs;
extern HrESEBackupEnd_t HrESEBackupEnd;
extern HrESEBackupSetup_t HrESEBackupSetup;
extern HrESEBackupInstanceEnd_t HrESEBackupInstanceEnd;
extern HrESEBackupOpenFile_t HrESEBackupOpenFile;
extern HrESEBackupReadFile_t HrESEBackupReadFile;
extern HrESEBackupCloseFile_t HrESEBackupCloseFile;

extern HrESERestoreOpen_t HrESERestoreOpen;
extern HrESERestoreReopen_t HrESERestoreReopen;
extern HrESERestoreComplete_t HrESERestoreComplete;
extern HrESERestoreClose_t HrESERestoreClose;
extern HrESERestoreGetEnvironment_t HrESERestoreGetEnvironment;
extern HrESERestoreSaveEnvironment_t HrESERestoreSaveEnvironment;
extern HrESERestoreAddDatabase_t HrESERestoreAddDatabase;
extern HrESERestoreOpenFile_t HrESERestoreOpenFile;

#if !defined(MINGW64)  && (_WIN32_WINNT < 0x0500)
typedef enum _COMPUTER_NAME_FORMAT {
   ComputerNameNetBIOS,
   ComputerNameDnsHostname,
   ComputerNameDnsDomain,
   ComputerNameDnsFullyQualified,
   ComputerNamePhysicalNetBIOS,
   ComputerNamePhysicalDnsHostname,
   ComputerNamePhysicalDnsDomain,
   ComputerNamePhysicalDnsFullyQualified,
   ComputerNameMax
} COMPUTER_NAME_FORMAT;

BOOL WINAPI GetComputerNameExW(
   COMPUTER_NAME_FORMAT NameType,
   LPWSTR lpBuffer,
   LPDWORD lpnSize
);
#endif

}
