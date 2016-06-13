/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2005-2010 Free Software Foundation Europe e.V.

   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation and included
   in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   Affero General Public License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.
*/

/*
 * vss.c -- Interface to Volume Shadow Copies (VSS)
 *
 * Copyright transferred from MATRIX-Computer GmbH to
 *   Kern Sibbald by express permission.
 *
 * Author          : Thorsten Engel
 * Created On      : Fri May 06 21:44:00 2005
 */

#ifdef WIN32_VSS

#include "bareos.h"
#include "jcr.h"

#undef setlocale

// STL includes
#include <vector>
#include <algorithm>
#include <string>
#include <sstream>
#include <fstream>
using namespace std;

#include "ms_atl.h"
#include <objbase.h>

/*
 * Kludges to get Vista code to compile.
 */
#define __in  IN
#define __out OUT
#define __RPC_unique_pointer
#define __RPC_string
#ifndef __RPC__out_ecount_part
#define __RPC__out_ecount_part(x, y)
#endif
#define __RPC__deref_inout_opt
#define __RPC__out

#if !defined(ENABLE_NLS)
#define setlocale(p, d)
#endif

#ifdef HAVE_STRSAFE_H
/*
 * Used for safe string manipulation
 */
#include <strsafe.h>
#endif

#ifdef HAVE_MINGW
class IXMLDOMDocument;
#endif

/*
 * Reduce compiler warnings from Windows vss code
 */
#undef uuid
#define uuid(x)

#ifdef B_VSS_XP
#define VSSClientGeneric VSSClientXP
#include "WinXP/vss.h"
#include "WinXP/vswriter.h"
#include "WinXP/vsbackup.h"
#endif

#ifdef B_VSS_W2K3
#define VSSClientGeneric VSSClient2003
#include "Win2003/vss.h"
#include "Win2003/vswriter.h"
#include "Win2003/vsbackup.h"
#endif

#ifdef B_VSS_VISTA
#define VSSClientGeneric VSSClientVista
#include "Win2003/vss.h"
#include "Win2003/vswriter.h"
#include "Win2003/vsbackup.h"
#endif

#define VSS_ERROR_OBJECT_ALREADY_EXISTS 0x8004230D

#include "vss.h"

static void JmsgVssApiStatus(JCR *jcr, int msg_status, HRESULT hr, const char *apiName)
{
   const char *errmsg;

   if (hr == S_OK || hr == VSS_S_ASYNC_FINISHED) {
      return;
   }

   switch (hr) {
   case E_INVALIDARG:
      errmsg = "One of the parameter values is not valid.";
      break;
   case E_OUTOFMEMORY:
      errmsg = "The caller is out of memory or other system resources.";
      break;
   case E_ACCESSDENIED:
      errmsg = "The caller does not have sufficient backup privileges or is not an administrator.";
      break;
   case VSS_E_INVALID_XML_DOCUMENT:
      errmsg = "The XML document is not valid.";
      break;
   case VSS_E_OBJECT_NOT_FOUND:
      errmsg = "The specified file does not exist.";
      break;
   case VSS_E_BAD_STATE:
      errmsg = "Object is not initialized; called during restore or not called in correct sequence.";
      break;
   case VSS_E_WRITER_INFRASTRUCTURE:
      errmsg = "The writer infrastructure is not operating properly. Check that the Event Service and VSS have been started, and check for errors associated with those services in the error log.";
      break;
   case VSS_S_ASYNC_CANCELLED:
      errmsg = "The asynchronous operation was canceled by a previous call to IVssAsync::Cancel.";
      break;
   case VSS_S_ASYNC_PENDING:
      errmsg = "The asynchronous operation is still running.";
      break;
   case RPC_E_CHANGED_MODE:
      errmsg = "Previous call to CoInitializeEx specified the multithread apartment (MTA). This call indicates single-threaded apartment has occurred.";
      break;
   case S_FALSE:
      errmsg = "No writer found for the current component.";
      break;
   default:
      errmsg = "Unexpected error. The error code is logged in the error log file.";
      break;
   }
   Jmsg(jcr, msg_status, 0, "VSS API failure calling \"%s\". ERR=%s\n", apiName, errmsg);
}

#ifndef VSS_WS_FAILED_AT_BACKUPSHUTDOWN
#define VSS_WS_FAILED_AT_BACKUPSHUTDOWN (VSS_WRITER_STATE)15
#endif

static void JmsgVssWriterStatus(JCR *jcr, int msg_status, VSS_WRITER_STATE eWriterStatus, char *writer_name)
{
   const char *errmsg;

   /*
    * The following are normal states
    */
   if (eWriterStatus == VSS_WS_STABLE ||
       eWriterStatus == VSS_WS_WAITING_FOR_BACKUP_COMPLETE) {
      return;
   }

   /*
    * Potential errors
    */
   switch (eWriterStatus) {
   default:
   case VSS_WS_UNKNOWN:
      errmsg = "The writer's state is not known. This is a writer error.";
      break;
   case VSS_WS_WAITING_FOR_FREEZE:
      errmsg = "The writer is waiting for the freeze state.";
      break;
   case VSS_WS_WAITING_FOR_THAW:
      errmsg = "The writer is waiting for the thaw state.";
      break;
   case VSS_WS_WAITING_FOR_POST_SNAPSHOT:
      errmsg = "The writer is waiting for the PostSnapshot state.";
      break;
   case VSS_WS_WAITING_FOR_BACKUP_COMPLETE:
      errmsg = "The writer is waiting for the requester to finish its backup operation.";
      break;
   case VSS_WS_FAILED_AT_IDENTIFY:
      errmsg = "The writer vetoed the shadow copy creation process at the writer identification state.";
      break;
   case VSS_WS_FAILED_AT_PREPARE_BACKUP:
      errmsg = "The writer vetoed the shadow copy creation process during the backup preparation state.";
      break;
   case VSS_WS_FAILED_AT_PREPARE_SNAPSHOT:
      errmsg = "The writer vetoed the shadow copy creation process during the PrepareForSnapshot state.";
      break;
   case VSS_WS_FAILED_AT_FREEZE:
      errmsg = "The writer vetoed the shadow copy creation process during the freeze state.";
      break;
   case VSS_WS_FAILED_AT_THAW:
      errmsg = "The writer vetoed the shadow copy creation process during the thaw state.";
      break;
   case VSS_WS_FAILED_AT_POST_SNAPSHOT:
      errmsg = "The writer vetoed the shadow copy creation process during the PostSnapshot state.";
      break;
   case VSS_WS_FAILED_AT_BACKUP_COMPLETE:
      errmsg = "The shadow copy has been created and the writer failed during the BackupComplete state.";
      break;
   case VSS_WS_FAILED_AT_PRE_RESTORE:
      errmsg = "The writer failed during the PreRestore state.";
      break;
   case VSS_WS_FAILED_AT_POST_RESTORE:
      errmsg = "The writer failed during the PostRestore state.";
      break;
   case VSS_WS_FAILED_AT_BACKUPSHUTDOWN:
      errmsg = "The writer failed during the shutdown of the backup application.";

   }
   Jmsg(jcr, msg_status, 0, "VSS Writer \"%s\" has invalid state. ERR=%s\n", writer_name, errmsg);
}

/*
 * Some helper functions
 */

/*
 * bstrdup a wchar_t string.
 */
static inline wchar_t *wbstrdup(const wchar_t *str)
{
   int len;
   wchar_t *dup;

   len = wcslen(str) + 1;
   dup = (wchar_t *)malloc(len * sizeof(wchar_t));
   memcpy(dup, str, len * sizeof(wchar_t));

   return dup;
}

/*
 * Append a backslash to the current string.
 */
static inline wstring AppendBackslash(wstring str)
{
    if (str.length() == 0) {
        return wstring(L"\\");
    }
    if (str[str.length() - 1] == L'\\') {
        return str;
    }
    return str.append(L"\\");
}

/*
 * Get the unique volume name for the given path.
 */
static inline wstring GetUniqueVolumeNameForPath(wstring path)
{
    wchar_t volumeRootPath[MAX_PATH];
    wchar_t volumeName[MAX_PATH];
    wchar_t volumeUniqueName[MAX_PATH];

    if (path.length() <= 0) {
       return L"";
    }

    /*
     * Add the backslash termination, if needed.
     */
    path = AppendBackslash(path);

    /*
     * Get the root path of the volume.
     */
    if (!p_GetVolumePathNameW || !p_GetVolumePathNameW((LPCWSTR)path.c_str(), volumeRootPath, MAX_PATH)) {
       return L"";
    }

    /*
     * Get the volume name alias (might be different from the unique volume name in rare cases).
     */
    if (!p_GetVolumeNameForVolumeMountPointW || !p_GetVolumeNameForVolumeMountPointW(volumeRootPath, volumeName, MAX_PATH)) {
       return L"";
    }

    /*
     * Get the unique volume name.
     */
    if (!p_GetVolumeNameForVolumeMountPointW(volumeName, volumeUniqueName, MAX_PATH)) {
       return L"";
    }

    return volumeUniqueName;
}

static inline POOLMEM *GetMountedVolumeForMountPointPath(POOLMEM *volumepath, POOLMEM *mountpoint)
{
   POOLMEM *fullPath, *buf, *vol;
   int len;

   /*
    * GetUniqueVolumeNameForPath() should be used here
    */
   len = strlen(volumepath) + 1;
   fullPath = get_pool_memory(PM_FNAME);
   pm_strcpy(fullPath, volumepath);
   pm_strcat(fullPath, mountpoint);

   buf = get_pool_memory(PM_FNAME);
   GetVolumeNameForVolumeMountPoint(fullPath, buf, len);

   Dmsg3(200, "%s%s mounts volume %s\n", volumepath, mountpoint, buf);

   vol = get_pool_memory(PM_FNAME);
   UTF8_2_wchar(vol, buf);

   free_pool_memory(fullPath);
   free_pool_memory(buf);

   return vol;
}

static inline bool HandleVolumeMountPoint(VSSClientGeneric *pVssClient,
                                          IVssBackupComponents *pVssObj,
                                          POOLMEM *&volumepath,
                                          POOLMEM *&mountpoint)
{
   bool retval = false;
   HRESULT hr;
   POOLMEM *vol = NULL;
   POOLMEM *pvol;
   VSS_ID pid;

   vol = GetMountedVolumeForMountPointPath(volumepath, mountpoint);
   hr = pVssObj->AddToSnapshotSet((LPWSTR)vol, GUID_NULL, &pid);

   pvol = get_pool_memory(PM_FNAME);
   wchar_2_UTF8(pvol, (wchar_t *)vol);

   if (SUCCEEDED(hr)) {
      pVssClient->AddVolumeMountPointSnapshots(pVssObj, (wchar_t *)vol);
      Dmsg1(200, "%s added to snapshotset \n", pvol);
      retval = true;
   } else if ((unsigned)hr == VSS_ERROR_OBJECT_ALREADY_EXISTS) {
      Dmsg1(200, "%s already in snapshotset, skipping.\n" ,pvol);
   } else {
      Dmsg3(200, "%s with vmp %s could not be added to snapshotset, COM ERROR: 0x%X\n", vol, mountpoint, hr);
   }

   free_pool_memory(pvol);
   if (vol) {
      free_pool_memory(vol);
   }

   return retval;
}

/*
 * Helper macro for quick treatment of case statements for error codes
 */
#define GEN_MERGE(A, B) A##B
#define GEN_MAKE_W(A) GEN_MERGE(L, A)

#define CHECK_CASE_FOR_CONSTANT(value)                      \
    case value: return (GEN_MAKE_W(#value));

/*
 * Convert a writer status into a string
 */
static inline const wchar_t *GetStringFromWriterStatus(VSS_WRITER_STATE eWriterStatus)
{
    switch (eWriterStatus) {
    CHECK_CASE_FOR_CONSTANT(VSS_WS_STABLE);
    CHECK_CASE_FOR_CONSTANT(VSS_WS_WAITING_FOR_FREEZE);
    CHECK_CASE_FOR_CONSTANT(VSS_WS_WAITING_FOR_THAW);
    CHECK_CASE_FOR_CONSTANT(VSS_WS_WAITING_FOR_POST_SNAPSHOT);
    CHECK_CASE_FOR_CONSTANT(VSS_WS_WAITING_FOR_BACKUP_COMPLETE);
    CHECK_CASE_FOR_CONSTANT(VSS_WS_FAILED_AT_IDENTIFY);
    CHECK_CASE_FOR_CONSTANT(VSS_WS_FAILED_AT_PREPARE_BACKUP);
    CHECK_CASE_FOR_CONSTANT(VSS_WS_FAILED_AT_PREPARE_SNAPSHOT);
    CHECK_CASE_FOR_CONSTANT(VSS_WS_FAILED_AT_FREEZE);
    CHECK_CASE_FOR_CONSTANT(VSS_WS_FAILED_AT_THAW);
    CHECK_CASE_FOR_CONSTANT(VSS_WS_FAILED_AT_POST_SNAPSHOT);
    CHECK_CASE_FOR_CONSTANT(VSS_WS_FAILED_AT_BACKUP_COMPLETE);
    CHECK_CASE_FOR_CONSTANT(VSS_WS_FAILED_AT_PRE_RESTORE);
    CHECK_CASE_FOR_CONSTANT(VSS_WS_FAILED_AT_POST_RESTORE);
    default:
        return L"Error or Undefined";
    }
}

#ifdef HAVE_VSS64
/* 64 bit entrypoint name */
#define VSSVBACK_ENTRY "?CreateVssBackupComponents@@YAJPEAPEAVIVssBackupComponents@@@Z"
#else
/* 32 bit entrypoint name */
#define VSSVBACK_ENTRY "?CreateVssBackupComponents@@YGJPAPAVIVssBackupComponents@@@Z"
#endif

/*
 * Constructor
 */
VSSClientGeneric::VSSClientGeneric()
{
   m_hLib = LoadLibraryA("VSSAPI.DLL");
   if (m_hLib) {
      m_CreateVssBackupComponents = (t_CreateVssBackupComponents) GetProcAddress(m_hLib, VSSVBACK_ENTRY);
      m_VssFreeSnapshotProperties = (t_VssFreeSnapshotProperties) GetProcAddress(m_hLib, "VssFreeSnapshotProperties");
   }
}

/*
 * Destructor
 */
VSSClientGeneric::~VSSClientGeneric()
{
   if (m_hLib) {
      FreeLibrary(m_hLib);
   }
}

/*
 * Initialize the COM infrastructure and the internal pointers
 */
bool VSSClientGeneric::Initialize(DWORD dwContext, bool bDuringRestore)
{
   VMPs = 0;
   VMP_snapshots = 0;
   HRESULT hr = S_OK;
   CComPtr<IVssAsync> pAsync1;
   VSS_BACKUP_TYPE backup_type;
   IVssBackupComponents *pVssObj = (IVssBackupComponents *)m_pVssObject;

   if (!(m_CreateVssBackupComponents && m_VssFreeSnapshotProperties)) {
      Dmsg2(0, "VSSClientGeneric::Initialize: m_CreateVssBackupComponents=0x%08X, m_VssFreeSnapshotProperties=0x%08X\n",
            m_CreateVssBackupComponents, m_VssFreeSnapshotProperties);
      Jmsg(m_jcr, M_FATAL, 0, "Entry point CreateVssBackupComponents or VssFreeSnapshotProperties missing.\n");
      return false;
   }

   /*
    * Initialize COM
    */
   if (!m_bCoInitializeCalled) {
      hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
      if (FAILED(hr)) {
         Dmsg1(0, "VSSClientGeneric::Initialize: CoInitializeEx returned 0x%08X\n", hr);
         JmsgVssApiStatus(m_jcr, M_FATAL, hr, "CoInitializeEx");
         errno = b_errno_win32;
         return false;
      }
      m_bCoInitializeCalled = true;
   }

   /*
    * Initialize COM security
    */
   if (!initialize_com_security()) {
      JmsgVssApiStatus(m_jcr, M_FATAL, hr, "CoInitializeSecurity");
      return false;
   }

   /*
    * Release the any old IVssBackupComponents interface
    */
   if (pVssObj) {
      pVssObj->Release();
      m_pVssObject = NULL;
   }

   /*
    * Create new internal backup components object
    */
   hr = m_CreateVssBackupComponents((IVssBackupComponents**)&m_pVssObject);
   if (FAILED(hr)) {
      berrno be;
      Dmsg2(0, "VSSClientGeneric::Initialize: CreateVssBackupComponents returned 0x%08X. ERR=%s\n",
            hr, be.bstrerror(b_errno_win32));
      JmsgVssApiStatus(m_jcr, M_FATAL, hr, "CreateVssBackupComponents");
      errno = b_errno_win32;
      return false;
   }

   /*
    * Define shorthand VssObject with time
    */
   pVssObj = (IVssBackupComponents *)m_pVssObject;

   if (!bDuringRestore) {
#if defined(B_VSS_W2K3) || defined(B_VSS_VISTA)
      if (dwContext != VSS_CTX_BACKUP) {
         hr = pVssObj->SetContext(dwContext);
         if (FAILED(hr)) {
            Dmsg1(0, "VSSClientGeneric::Initialize: IVssBackupComponents->SetContext returned 0x%08X\n", hr);
            JmsgVssApiStatus(m_jcr, M_FATAL, hr, "SetContext");
            errno = b_errno_win32;
            return false;
         }
      }
#endif

      /*
       * 1. InitializeForBackup
       */
      hr = pVssObj->InitializeForBackup();
      if (FAILED(hr)) {
         Dmsg1(0, "VSSClientGeneric::Initialize: IVssBackupComponents->InitializeForBackup returned 0x%08X\n", hr);
         JmsgVssApiStatus(m_jcr, M_FATAL, hr, "InitializeForBackup");
         errno = b_errno_win32;
         return false;
      }

      /*
       * 2. SetBackupState
       */
      switch (m_jcr->getJobLevel()) {
      case L_FULL:
         backup_type = VSS_BT_FULL;
         break;
      case L_DIFFERENTIAL:
         backup_type = VSS_BT_DIFFERENTIAL;
         break;
      case L_INCREMENTAL:
         backup_type = VSS_BT_INCREMENTAL;
         break;
      default:
         Dmsg1(0, "VSSClientGeneric::Initialize: unknown backup level %d\n", m_jcr->getJobLevel());
         backup_type = VSS_BT_FULL;
         break;
      }

      /*
       * FIXME: need to support partial files - make last parameter true when done
       */
      hr = pVssObj->SetBackupState(true, true, backup_type, false);
      if (FAILED(hr)) {
         Dmsg1(0, "VSSClientGeneric::Initialize: IVssBackupComponents->SetBackupState returned 0x%08X\n", hr);
         JmsgVssApiStatus(m_jcr, M_FATAL, hr, "SetBackupState");
         errno = b_errno_win32;
         return false;
      }

      /*
       * 3. GatherWriterMetaData
       */
      hr = pVssObj->GatherWriterMetadata(&pAsync1.p);
      if (FAILED(hr)) {
         Dmsg1(0, "VSSClientGeneric::Initialize: IVssBackupComponents->GatherWriterMetadata returned 0x%08X\n", hr);
         JmsgVssApiStatus(m_jcr, M_FATAL, hr, "GatherWriterMetadata");
         errno = b_errno_win32;
         return false;
      }

      /*
       * Wait for the async operation to finish and checks the result
       */
      if (!WaitAndCheckForAsyncOperation(pAsync1.p)) {
         /*
          * Error message already printed
          */
         errno = b_errno_win32;
         return false;
      }
   }

   /*
    * We are during restore now?
    */
   m_bDuringRestore = bDuringRestore;

   /*
    * Keep the context
    */
   m_dwContext = dwContext;

   return true;
}

bool VSSClientGeneric::WaitAndCheckForAsyncOperation(IVssAsync* pAsync)
{
   HRESULT hr;
   HRESULT hrReturned = S_OK;
   int timeout = 600; /* 10 minutes.... */
   int queryErrors = 0;

   /*
    * Wait until the async operation finishes
    * unfortunately we can't use a timeout here yet.
    * the interface would allow it on W2k3,
    * but it is not implemented yet....
    */
   do {
      if (hrReturned != S_OK) {
         Sleep(1000);
      }
      hrReturned = S_OK;
      hr = pAsync->QueryStatus(&hrReturned, NULL);

      if (FAILED(hr)) {
         queryErrors++;
      }
   } while ((timeout-- > 0) && (hrReturned == VSS_S_ASYNC_PENDING));

   /*
    * Check the result of the asynchronous operation
    */
   if (hrReturned == VSS_S_ASYNC_FINISHED) {
      return true;
   }

   JmsgVssApiStatus(m_jcr, M_FATAL, hr, "Query Async Status after 10 minute wait");

   return false;
}

/*
 * Add all drive letters that need to be snapshotted.
 */
void VSSClientGeneric::AddDriveSnapshots(IVssBackupComponents *pVssObj, char *szDriveLetters, bool onefs_disabled)
{
   wstring volume;
   wchar_t szDrive[3];
   VSS_ID pid;

   szDrive[1] = ':';
   szDrive[2] = 0;

   /*
    * szDriveLetters contains all drive letters in uppercase
    * If a drive can not being added, it's converted to lowercase in szDriveLetters
    */
   for (size_t i = 0; i < strlen (szDriveLetters); i++) {
      szDrive[0] = szDriveLetters[i];
      volume = GetUniqueVolumeNameForPath(szDrive);

      /*
       * Store uniquevolumname.
       */

      if (SUCCEEDED(pVssObj->AddToSnapshotSet((LPWSTR)volume.c_str(), GUID_NULL, &pid))) {
         if (debug_level >= 200) {

            POOLMEM *szBuf = get_pool_memory(PM_FNAME);

            wchar_2_UTF8(szBuf, volume.c_str());
            Dmsg2(200, "%s added to snapshotset (Drive %s:\\)\n", szBuf, szDrive);
            free_pool_memory(szBuf);
         }
         wcsncpy(m_wszUniqueVolumeName[szDriveLetters[i]-'A'], (LPWSTR)volume.c_str(), MAX_PATH);
      } else {
         szDriveLetters[i] = tolower(szDriveLetters[i]);
      }
      if (onefs_disabled) {
         AddVolumeMountPointSnapshots(pVssObj, (LPWSTR)volume.c_str());
      } else {
         Jmsg(m_jcr, M_INFO, 0, "VolumeMountpoints are not processed as onefs = yes.\n");
      }
   }
}

/*
 * Add all volume mountpoints that need to be snapshotted.
 * Volumes can be mounted multiple times, but can only be added to the snapshotset once.
 * So we skip adding a volume if it is already in snapshotset.
 * We count the total number of vmps and the number of volumes we added to the snapshotset.
 */
void VSSClientGeneric::AddVolumeMountPointSnapshots(IVssBackupComponents *pVssObj, LPWSTR volume)
{
   BOOL b;
   int len;
   HANDLE hMount;
   POOLMEM *mp, *path;

   mp = get_pool_memory(PM_FNAME);
   path = get_pool_memory(PM_FNAME);

   wchar_2_UTF8(path, volume);

   len = wcslen(volume) + 1;

   hMount = FindFirstVolumeMountPoint(path, mp, len);
   if (hMount != INVALID_HANDLE_VALUE) {
      /*
       * Count number of vmps.
       */
      VMPs += 1;
      if (HandleVolumeMountPoint(this, pVssObj, path, mp)) {
         /*
          * Count vmps that were snapshotted
          */
         VMP_snapshots += 1;
      }

      while ((b = FindNextVolumeMountPoint(hMount, mp, len))) {
         /*
          * Count number of vmps.
          */
         VMPs += 1;
         if (HandleVolumeMountPoint(this, pVssObj, path, mp)) {
            /*
             * Count vmps that were snapshotted
             */
            VMP_snapshots += 1;
         }
      }
   }

   FindVolumeMountPointClose(hMount);

   free_pool_memory(path);
   free_pool_memory(mp);
}

void VSSClientGeneric::ShowVolumeMountPointStats(JCR *jcr)
{
   if (VMPs) {
      Jmsg(jcr, M_INFO, 0, _("Volume Mount Points found: %d, added to snapshotset: %d\n"), VMPs, VMP_snapshots);
   }
}

bool VSSClientGeneric::CreateSnapshots(char *szDriveLetters, bool onefs_disabled)
{
   IVssBackupComponents *pVssObj;
   CComPtr<IVssAsync> pAsync1;
   CComPtr<IVssAsync> pAsync2;
   HRESULT hr;

   /*
    * See http://msdn.microsoft.com/en-us/library/windows/desktop/aa382870%28v=vs.85%29.aspx.
    */
   if (!m_pVssObject || m_bBackupIsInitialized) {
      Jmsg(m_jcr, M_FATAL, 0, "No pointer to VssObject or Backup is not Initialized\n");
      errno = ENOSYS;
      return false;
   }

   m_uidCurrentSnapshotSet = GUID_NULL;

   pVssObj = (IVssBackupComponents *)m_pVssObject;

   /*
    * startSnapshotSet
    */
   hr = pVssObj->StartSnapshotSet(&m_uidCurrentSnapshotSet);
   while ((unsigned)hr == VSS_E_SNAPSHOT_SET_IN_PROGRESS) {
      bmicrosleep(5,0);
      Jmsg(m_jcr, M_INFO, 0, "VSS_E_SNAPSHOT_SET_IN_PROGRESS, retrying ...\n");
      hr = pVssObj->StartSnapshotSet(&m_uidCurrentSnapshotSet);
   }

   if (FAILED(hr)) {
      JmsgVssApiStatus(m_jcr, M_FATAL, hr, "StartSnapshotSet");
      errno = ENOSYS;
      return false;
   }

   /*
    * AddToSnapshotSet
    */
   if (szDriveLetters) {
      AddDriveSnapshots(pVssObj, szDriveLetters, onefs_disabled);
   }

   /*
    * PrepareForBackup
    */
   hr = pVssObj->PrepareForBackup(&pAsync1.p);
   if (FAILED(hr)) {
      JmsgVssApiStatus(m_jcr, M_FATAL, hr, "PrepareForBackup");
      errno = b_errno_win32;
      return false;
   }

   /*
    * Wait for the async operation to finish and checks the result.
    */
   if (!WaitAndCheckForAsyncOperation(pAsync1.p)) {
      errno = b_errno_win32;
      return false;
   }

   /*
    * Get latest info about writer status.
    */
   if (!CheckWriterStatus()) {
      errno = b_errno_win32;       /* Error already printed */
      return false;
   }

   /*
    * DoSnapShotSet
    */
   hr = pVssObj->DoSnapshotSet(&pAsync2.p);
   if (FAILED(hr)) {
      JmsgVssApiStatus(m_jcr, M_FATAL, hr, "DoSnapshotSet");
      errno = b_errno_win32;
      return false;
   }

   /*
    * Waits for the async operation to finish and checks the result.
    */
   if (!WaitAndCheckForAsyncOperation(pAsync2.p)) {
      errno = b_errno_win32;
      return false;
   }

   /*
    * Get latest info about writer status.
    */
   if (!CheckWriterStatus()) {
      errno = b_errno_win32;      /* Error already printed */
      return false;
   }

   /*
    * Query snapshot info.
    */
   QuerySnapshotSet(m_uidCurrentSnapshotSet);

   m_bBackupIsInitialized = true;

   return true;
}

bool VSSClientGeneric::CloseBackup()
{
   bool bRet = false;
   HRESULT hr;
   BSTR xml;
   IVssBackupComponents *pVssObj = (IVssBackupComponents *)m_pVssObject;

   if (!m_pVssObject) {
      Jmsg(m_jcr, M_FATAL, 0, "VssOject is NULL.\n");
      errno = ENOSYS;
      return bRet;
   }
   CComPtr<IVssAsync> pAsync;

   m_bBackupIsInitialized = false;

   hr = pVssObj->BackupComplete(&pAsync.p);
   if (SUCCEEDED(hr)) {
      /*
       * Wait for the async operation to finish and checks the result.
       */
      WaitAndCheckForAsyncOperation(pAsync.p);
      bRet = true;
   } else {
      JmsgVssApiStatus(m_jcr, M_ERROR, hr, "BackupComplete");
      errno = b_errno_win32;
      pVssObj->AbortBackup();
   }

   /*
    * Get latest info about writer status.
    */
   CheckWriterStatus();

   hr = pVssObj->SaveAsXML(&xml);
   if (SUCCEEDED(hr)) {
      m_metadata = xml;
   } else {
      m_metadata = NULL;
   }

   /*
    * FIXME?: The docs http://msdn.microsoft.com/en-us/library/aa384582%28v=VS.85%29.aspx say this isn't required...
    */
   if (m_uidCurrentSnapshotSet != GUID_NULL) {
      VSS_ID idNonDeletedSnapshotID = GUID_NULL;
      LONG lSnapshots;

      pVssObj->DeleteSnapshots(m_uidCurrentSnapshotSet,
                               VSS_OBJECT_SNAPSHOT_SET,
                               false,
                               &lSnapshots,
                               &idNonDeletedSnapshotID);

      m_uidCurrentSnapshotSet = GUID_NULL;
   }

   if (m_bWriterStatusCurrent) {
      m_bWriterStatusCurrent = false;
      pVssObj->FreeWriterStatus();
   }

   pVssObj->Release();
   m_pVssObject = NULL;

   /*
    * Call CoUninitialize if the CoInitializeEx was performed sucesfully
    */
   if (m_bCoInitializeCalled) {
      CoUninitialize();
      m_bCoInitializeCalled = false;
   }

   return bRet;
}

wchar_t *VSSClientGeneric::GetMetadata()
{
   return m_metadata;
}

bool VSSClientGeneric::CloseRestore()
{
   IVssBackupComponents *pVssObj = (IVssBackupComponents *)m_pVssObject;
   CComPtr<IVssAsync> pAsync;

   if (!pVssObj) {
      errno = ENOSYS;
      return false;
   }
   return true;
}

/*
 * Query all the shadow copies in the given set
 */
void VSSClientGeneric::QuerySnapshotSet(GUID snapshotSetID)
{
   if (!(m_CreateVssBackupComponents && m_VssFreeSnapshotProperties)) {
      Jmsg(m_jcr, M_FATAL, 0, "CreateVssBackupComponents or VssFreeSnapshotProperties API is NULL.\n");
      errno = ENOSYS;
      return;
   }

   memset(m_szShadowCopyName,0,sizeof (m_szShadowCopyName));

   if (snapshotSetID == GUID_NULL || m_pVssObject == NULL) {
      Jmsg(m_jcr, M_FATAL, 0, "snapshotSetID == NULL or VssObject is NULL.\n");
      errno = ENOSYS;
      return;
   }

   IVssBackupComponents *pVssObj = (IVssBackupComponents *)m_pVssObject;

   /*
    * Get list all shadow copies.
    */
   CComPtr<IVssEnumObject> pIEnumSnapshots;
   HRESULT hr = pVssObj->Query(GUID_NULL,
                               VSS_OBJECT_NONE,
                               VSS_OBJECT_SNAPSHOT,
                               (IVssEnumObject**)(&pIEnumSnapshots));

   /*
    * If there are no shadow copies, just return
    */
   if (FAILED(hr)) {
      Jmsg(m_jcr, M_FATAL, 0, "No Volume Shadow copies made.\n");
      errno = b_errno_win32;
      return;
   }

   /*
    * Enumerate all shadow copies.
    */
   VSS_OBJECT_PROP Prop;
   VSS_SNAPSHOT_PROP& Snap = Prop.Obj.Snap;

   while (true) {
      /*
       * Get the next element
       */
      ULONG ulFetched;
      hr = (pIEnumSnapshots.p)->Next(1, &Prop, &ulFetched);

      /*
       * We reached the end of list
       */
      if (ulFetched == 0)
         break;

      /*
       * Print the shadow copy (if not filtered out)
       */
      if (Snap.m_SnapshotSetId == snapshotSetID)  {
         for (int ch='A'-'A';ch<='Z'-'A';ch++) {
            if (wcscmp(Snap.m_pwszOriginalVolumeName, m_wszUniqueVolumeName[ch]) == 0) {
               wcsncpy(m_szShadowCopyName[ch],Snap.m_pwszSnapshotDeviceObject, MAX_PATH-1);
               break;
            }
         }
      }
      m_VssFreeSnapshotProperties(&Snap);
   }
   errno = 0;
}

/*
 * Check the status for all selected writers
 */
bool VSSClientGeneric::CheckWriterStatus()
{
   /*
    * See http://msdn.microsoft.com/en-us/library/windows/desktop/aa382870%28v=vs.85%29.aspx
    */
   IVssBackupComponents *pVssObj = (IVssBackupComponents *)m_pVssObject;
   if (!pVssObj) {
      Jmsg(m_jcr, M_FATAL, 0, "Cannot get IVssBackupComponents pointer.\n");
      errno = ENOSYS;
      return false;
   }
   DestroyWriterInfo();

   if (m_bWriterStatusCurrent) {
      m_bWriterStatusCurrent = false;
      pVssObj->FreeWriterStatus();
   }

   /*
    * Gather writer status to detect potential errors
    */
   CComPtr<IVssAsync>  pAsync;

   HRESULT hr = pVssObj->GatherWriterStatus(&pAsync.p);
   if (FAILED(hr)) {
      JmsgVssApiStatus(m_jcr, M_FATAL, hr, "GatherWriterStatus");
      errno = b_errno_win32;
      return false;
   }

   /*
    * Waits for the async operation to finish and checks the result
    */
   if (!WaitAndCheckForAsyncOperation(pAsync.p)) {
      errno = b_errno_win32;
      return false;
   }

   m_bWriterStatusCurrent = true;

   unsigned cWriters = 0;

   hr = pVssObj->GetWriterStatusCount(&cWriters);
   if (FAILED(hr)) {
      JmsgVssApiStatus(m_jcr, M_FATAL, hr, "GatherWriterStatusCount");
      errno = b_errno_win32;
      return false;
   }

    int nState;
    POOLMEM *szBuf = get_pool_memory(PM_FNAME);

   /*
    * Enumerate each writer
    */
   for (unsigned iWriter = 0; iWriter < cWriters; iWriter++) {
      VSS_ID idInstance = GUID_NULL;
      VSS_ID idWriter= GUID_NULL;
      VSS_WRITER_STATE eWriterStatus = VSS_WS_UNKNOWN;
      CComBSTR bstrWriterName;
      HRESULT hrWriterFailure = S_OK;

      /*
       * Get writer status
       */
      hr = pVssObj->GetWriterStatus(iWriter,
                                    &idInstance,
                                    &idWriter,
                                    &bstrWriterName,
                                    &eWriterStatus,
                                    &hrWriterFailure);
      if (FAILED(hr)) {
          /*
           * Api failed
           */
          JmsgVssApiStatus(m_jcr, M_WARNING, hr, "GetWriterStatus");
          nState = 0;         /* Unknown writer state -- API failed */
      } else {
          switch(eWriterStatus) {
          case VSS_WS_FAILED_AT_IDENTIFY:
          case VSS_WS_FAILED_AT_PREPARE_BACKUP:
          case VSS_WS_FAILED_AT_PREPARE_SNAPSHOT:
          case VSS_WS_FAILED_AT_FREEZE:
          case VSS_WS_FAILED_AT_THAW:
          case VSS_WS_FAILED_AT_POST_SNAPSHOT:
          case VSS_WS_FAILED_AT_BACKUP_COMPLETE:
          case VSS_WS_FAILED_AT_PRE_RESTORE:
          case VSS_WS_FAILED_AT_POST_RESTORE:
#if defined(B_VSS_W2K3) || defined(B_VSS_VISTA)
          case VSS_WS_FAILED_AT_BACKUPSHUTDOWN:
#endif
             /*
              * Writer status problem
              */
             wchar_2_UTF8(szBuf, bstrWriterName.p);
             JmsgVssWriterStatus(m_jcr, M_WARNING, eWriterStatus, szBuf);
             nState = -1;       /* bad writer state */
             break;
          default:
             nState = 1;        /* Writer state OK */
          }
       }

       /*
        * store text info
        */
       char str[1000];
       bstrncpy(str, "\"", sizeof(str));
       wchar_2_UTF8(szBuf, bstrWriterName.p);
       bstrncat(str, szBuf, sizeof(str));
       bstrncat(str, "\", State: 0x", sizeof(str));
       itoa(eWriterStatus, szBuf, sizeof_pool_memory(szBuf));
       bstrncat(str, szBuf, sizeof(str));
       bstrncat(str, " (", sizeof(str));
       wchar_2_UTF8(szBuf, GetStringFromWriterStatus(eWriterStatus));
       bstrncat(str, szBuf, sizeof(str));
       bstrncat(str, ")", sizeof(str));
       AppendWriterInfo(nState, (const char *)str);
   }

   free_pool_memory(szBuf);
   errno = 0;

   return true;
}
#endif /* WIN32_VSS */
