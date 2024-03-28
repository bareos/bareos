/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2005-2010 Free Software Foundation Europe e.V.
   Copyright (C) 2013-2024 Bareos GmbH & Co. KG

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

#  include <unordered_set>

#  include "include/bareos.h"
#  include "include/jcr.h"
#  include "lib/berrno.h"
#  include "findlib/find.h"
#  define FILE_DAEMON 1
#  include "filed/fd_plugins.h"
#  include "fill_proc_address.h"

#  undef setlocale

// STL includes
#  include <vector>
#  include <algorithm>
#  include <string>
#  include <sstream>
#  include <fstream>
#  include <chrono>
using namespace std;

#  include "ms_atl.h"
#  include <objbase.h>

// Kludges to get Vista code to compile.
#  define __in IN
#  define __out OUT
#  define __RPC_unique_pointer
#  define __RPC_string
#  ifndef __RPC__out_ecount_part
#    define __RPC__out_ecount_part(x, y)
#  endif
#  define __RPC__deref_inout_opt
#  define __RPC__out

#  if !defined(ENABLE_NLS)
#    define setlocale(p, d)
#  endif

#  ifdef HAVE_STRSAFE_H
// Used for safe string manipulation
#    include <strsafe.h>
#  endif

#  ifdef HAVE_MINGW
class IXMLDOMDocument;
#  endif

#  define VSSClientGeneric VSSClientVista
#  include "Win2003/vss.h"
#  include "Win2003/vswriter.h"
#  include "Win2003/vsbackup.h"

#  define VSS_ERROR_OBJECT_ALREADY_EXISTS 0x8004230D

#  include "vss.h"


using namespace filedaemon;

static void JmsgVssApiStatus(JobControlRecord* jcr,
                             int msg_status,
                             HRESULT hr,
                             const char* apiName)
{
  const char* errmsg;

  if (hr == S_OK || hr == VSS_S_ASYNC_FINISHED) { return; }

  switch (hr) {
    case E_INVALIDARG:
      errmsg = "One of the parameter values is not valid.";
      break;
    case E_OUTOFMEMORY:
      errmsg = "The caller is out of memory or other system resources.";
      break;
    case E_ACCESSDENIED:
      errmsg
          = "The caller does not have sufficient backup privileges or is not "
            "an "
            "administrator.";
      break;
    case VSS_E_INVALID_XML_DOCUMENT:
      errmsg = "The XML document is not valid.";
      break;
    case VSS_E_OBJECT_NOT_FOUND:
      errmsg = "The specified file does not exist.";
      break;
    case VSS_E_BAD_STATE:
      errmsg
          = "Object is not initialized; called during restore or not called in "
            "correct sequence.";
      break;
    case VSS_E_WRITER_INFRASTRUCTURE:
      errmsg
          = "The writer infrastructure is not operating properly. Check that "
            "the "
            "Event Service and VSS have been started, and check for errors "
            "associated with those services in the error log.";
      break;
    case VSS_S_ASYNC_CANCELLED:
      errmsg
          = "The asynchronous operation was canceled by a previous call to "
            "IVssAsync::Cancel.";
      break;
    case VSS_S_ASYNC_PENDING:
      errmsg = "The asynchronous operation is still running.";
      break;
    case RPC_E_CHANGED_MODE:
      errmsg
          = "Previous call to CoInitializeEx specified the multithread "
            "apartment "
            "(MTA). This call indicates single-threaded apartment has "
            "occurred.";
      break;
    case S_FALSE:
      errmsg = "No writer found for the current component.";
      break;
    default:
      errmsg
          = "Unexpected error. The error code is logged in the error log file.";
      break;
  }
  Jmsg(jcr, msg_status, 0, "VSS API failure calling \"%s\". ERR=%s\n", apiName,
       errmsg);
}

#  ifndef VSS_WS_FAILED_AT_BACKUPSHUTDOWN
#    define VSS_WS_FAILED_AT_BACKUPSHUTDOWN (VSS_WRITER_STATE)15
#  endif

static void JmsgVssWriterStatus(JobControlRecord* jcr,
                                int msg_status,
                                VSS_WRITER_STATE eWriterStatus,
                                char* writer_name)
{
  const char* errmsg;

  // The following are normal states
  if (eWriterStatus == VSS_WS_STABLE
      || eWriterStatus == VSS_WS_WAITING_FOR_BACKUP_COMPLETE) {
    return;
  }

  // Potential errors
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
      errmsg
          = "The writer is waiting for the requester to finish its backup "
            "operation.";
      break;
    case VSS_WS_FAILED_AT_IDENTIFY:
      errmsg
          = "The writer vetoed the shadow copy creation process at the writer "
            "identification state.";
      break;
    case VSS_WS_FAILED_AT_PREPARE_BACKUP:
      errmsg
          = "The writer vetoed the shadow copy creation process during the "
            "backup preparation state.";
      break;
    case VSS_WS_FAILED_AT_PREPARE_SNAPSHOT:
      errmsg
          = "The writer vetoed the shadow copy creation process during the "
            "PrepareForSnapshot state.";
      break;
    case VSS_WS_FAILED_AT_FREEZE:
      errmsg
          = "The writer vetoed the shadow copy creation process during the "
            "freeze state.";
      break;
    case VSS_WS_FAILED_AT_THAW:
      errmsg
          = "The writer vetoed the shadow copy creation process during the "
            "thaw "
            "state.";
      break;
    case VSS_WS_FAILED_AT_POST_SNAPSHOT:
      errmsg
          = "The writer vetoed the shadow copy creation process during the "
            "PostSnapshot state.";
      break;
    case VSS_WS_FAILED_AT_BACKUP_COMPLETE:
      errmsg
          = "The shadow copy has been created and the writer failed during the "
            "BackupComplete state.";
      break;
    case VSS_WS_FAILED_AT_PRE_RESTORE:
      errmsg = "The writer failed during the PreRestore state.";
      break;
    case VSS_WS_FAILED_AT_POST_RESTORE:
      errmsg = "The writer failed during the PostRestore state.";
      break;
    case VSS_WS_FAILED_AT_BACKUPSHUTDOWN:
      errmsg
          = "The writer failed during the shutdown of the backup application.";
  }
  Jmsg(jcr, msg_status, 0, "VSS Writer \"%s\" has invalid state. ERR=%s\n",
       writer_name, errmsg);
}

// Some helper functions

// Get the unique volume name for the given path.
static inline std::wstring GetUniqueVolumeNameForPath(const std::wstring& path)
{
  std::wstring temp{};
  wchar_t volumeName[MAX_PATH];
  wchar_t volumeUniqueName[MAX_PATH];

  if (path.size() <= 0) { return L""; }

  const wchar_t* volumeRoot = path.c_str();
  // Add the backslash termination, if needed.
  if (path.back() != L'\\') {
    temp.assign(path);
    temp.push_back(L'\\');
    volumeRoot = temp.c_str();
  }
  /* Get the volume name alias (might be different from the unique volume name
   * in rare cases). */
  if (!p_GetVolumeNameForVolumeMountPointW
      || !p_GetVolumeNameForVolumeMountPointW(volumeRoot, volumeName,
                                              MAX_PATH)) {
    return L"";
  }

  // Get the unique volume name.
  if (!p_GetVolumeNameForVolumeMountPointW(volumeName, volumeUniqueName,
                                           MAX_PATH)) {
    return L"";
  }

  return volumeUniqueName;
}

static inline bool HandleVolumeMountPoint(
    VSSClientGeneric* pVssClient,
    IVssBackupComponents* pVssObj,
    std::unordered_map<std::string, std::string>& mount_to_vol,
    std::unordered_map<std::wstring, std::wstring>& mount_to_vol_w,
    std::unordered_set<std::wstring>& snapshoted_volumes,
    const std::wstring& parent,
    const wchar_t* mountpoint)
{
  VSS_ID pid;

  bool snapshot_success = false;

  std::wstring full_path = parent + mountpoint;
  std::wstring vol = GetUniqueVolumeNameForPath(full_path);
  std::string pvol = FromUtf16(vol);
  std::string utf8_mp = FromUtf16(mountpoint);
  if (auto found = snapshoted_volumes.find(vol);
      found == snapshoted_volumes.end()) {
    HRESULT hr = pVssObj->AddToSnapshotSet(const_cast<LPWSTR>(vol.c_str()),
                                           GUID_NULL, &pid);
    if (SUCCEEDED(hr)) {
      pVssClient->AddVolumeMountPointSnapshots(pVssObj, vol,
                                               snapshoted_volumes);
      Dmsg1(200, "%s added to snapshotset \n", pvol.c_str());
      snapshot_success = true;
    } else if ((unsigned)hr == VSS_ERROR_OBJECT_ALREADY_EXISTS) {
      Dmsg1(200, "%s already in snapshotset, skipping.\n", pvol.c_str());
    } else {
      Dmsg3(
          200,
          "%s with vmp %s could not be added to snapshotset, COM ERROR: 0x%X\n",
          pvol.c_str(), utf8_mp.c_str(), hr);
    }
  } else {
    snapshot_success = true;
  }

  if (snapshot_success) {
    std::string path = FromUtf16(parent) + utf8_mp;

    mount_to_vol.emplace(std::move(path), std::move(pvol));
    mount_to_vol_w.emplace(std::move(full_path), std::move(vol));
  }

  return snapshot_success;
}

// Helper macro for quick treatment of case statements for error codes
#  define GEN_MERGE(A, B) A##B
#  define GEN_MAKE_W(A) GEN_MERGE(L, A)

#  define CHECK_CASE_FOR_CONSTANT(value) \
    case value:                          \
      return (GEN_MAKE_W(#value));

// Convert a writer status into a string
static inline const wchar_t* GetStringFromWriterStatus(
    VSS_WRITER_STATE eWriterStatus)
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

#  ifdef HAVE_VSS64
/* 64 bit entrypoint name */
#    define VSSVBACK_ENTRY \
      "?CreateVssBackupComponents@@YAJPEAPEAVIVssBackupComponents@@@Z"
#  else
/* 32 bit entrypoint name */
#    define VSSVBACK_ENTRY \
      "?CreateVssBackupComponents@@YGJPAPAVIVssBackupComponents@@@Z"
#  endif

// Constructor
VSSClientGeneric::VSSClientGeneric()
{
  hLib_ = LoadLibraryA("VSSAPI.DLL");
  if (hLib_) {
    BareosFillProcAddress(CreateVssBackupComponents_, hLib_, VSSVBACK_ENTRY);
    BareosFillProcAddress(VssFreeSnapshotProperties_, hLib_,
                          "VssFreeSnapshotProperties");
  }
}

// Destructor
VSSClientGeneric::~VSSClientGeneric()
{
  if (hLib_) { FreeLibrary(hLib_); }
}

// Initialize the COM infrastructure and the internal pointers
bool VSSClientGeneric::Initialize(DWORD dwContext, bool bDuringRestore)
{
  VMPs = 0;
  VMP_snapshots = 0;
  HRESULT hr = S_OK;
  CComPtr<IVssAsync> pAsync1;
  IVssBackupComponents* pVssObj = (IVssBackupComponents*)pVssObject_;

  if (!(CreateVssBackupComponents_ && VssFreeSnapshotProperties_)) {
    Dmsg2(0,
          "VSSClientGeneric::Initialize: CreateVssBackupComponents_=0x%08X, "
          "VssFreeSnapshotProperties_=0x%08X\n",
          CreateVssBackupComponents_, VssFreeSnapshotProperties_);
    Jmsg(jcr_, M_FATAL, 0,
         "Entry point CreateVssBackupComponents or VssFreeSnapshotProperties "
         "missing.\n");
    return false;
  }

  // Initialize COM
  if (!bCoInitializeCalled_) {
    hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (FAILED(hr)) {
      Dmsg1(0, "VSSClientGeneric::Initialize: CoInitializeEx returned 0x%08X\n",
            hr);
      JmsgVssApiStatus(jcr_, M_FATAL, hr, "CoInitializeEx");
      errno = b_errno_win32;
      return false;
    }
    bCoInitializeCalled_ = true;
  }

  // Initialize COM security
  if (!InitializeComSecurity()) {
    JmsgVssApiStatus(jcr_, M_FATAL, hr, "CoInitializeSecurity");
    return false;
  }

  // Release the any old IVssBackupComponents interface
  if (pVssObj) {
    pVssObj->Release();
    pVssObject_ = NULL;
  }

  // Create new internal backup components object
  hr = CreateVssBackupComponents_((IVssBackupComponents**)&pVssObject_);
  if (FAILED(hr)) {
    BErrNo be;
    Dmsg2(0,
          "VSSClientGeneric::Initialize: CreateVssBackupComponents returned "
          "0x%08X. ERR=%s\n",
          hr, be.bstrerror(b_errno_win32));
    JmsgVssApiStatus(jcr_, M_FATAL, hr, "CreateVssBackupComponents");
    errno = b_errno_win32;
    return false;
  }

  // Define shorthand VssObject with time
  pVssObj = (IVssBackupComponents*)pVssObject_;

  if (!bDuringRestore) {
#  if defined(B_VSS_W2K3) || defined(B_VSS_VISTA)
    if (dwContext != VSS_CTX_BACKUP) {
      hr = pVssObj->SetContext(dwContext);
      if (FAILED(hr)) {
        Dmsg1(0,
              "VSSClientGeneric::Initialize: IVssBackupComponents->SetContext "
              "returned 0x%08X\n",
              hr);
        JmsgVssApiStatus(jcr_, M_FATAL, hr, "SetContext");
        errno = b_errno_win32;
        return false;
      }
    }
#  endif

    // 1. InitializeForBackup
    hr = pVssObj->InitializeForBackup();
    if (FAILED(hr)) {
      Dmsg1(0,
            "VSSClientGeneric::Initialize: "
            "IVssBackupComponents->InitializeForBackup returned 0x%08X\n",
            hr);
      JmsgVssApiStatus(jcr_, M_FATAL, hr, "InitializeForBackup");
      errno = b_errno_win32;
      return false;
    }

    /* 2. SetBackupState
     *
     * Generate a bEventVssSetBackupState event and if none of the plugins
     * give back a bRC_Skip it means this will not be performed by any plugin
     * and we should do the generic handling ourself in the core. */
    if (GeneratePluginEvent(jcr_, bEventVssSetBackupState) != bRC_Skip) {
      VSS_BACKUP_TYPE backup_type;

      switch (jcr_->getJobLevel()) {
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
          Dmsg1(0, "VSSClientGeneric::Initialize: unknown backup level %d\n",
                jcr_->getJobLevel());
          backup_type = VSS_BT_FULL;
          break;
      }

      hr = pVssObj->SetBackupState(true,        /* bSelectComponents */
                                   true,        /* bBackupBootableSystemState */
                                   backup_type, /* backupType */
                                   false        /* bPartialFileSupport */
      );
      if (FAILED(hr)) {
        Dmsg1(0,
              "VSSClientGeneric::Initialize: "
              "IVssBackupComponents->SetBackupState returned 0x%08X\n",
              hr);
        JmsgVssApiStatus(jcr_, M_FATAL, hr, "SetBackupState");
        errno = b_errno_win32;
        return false;
      }
    }

    // 3. GatherWriterMetaData
    hr = pVssObj->GatherWriterMetadata(&pAsync1.p);
    if (FAILED(hr)) {
      Dmsg1(0,
            "VSSClientGeneric::Initialize: "
            "IVssBackupComponents->GatherWriterMetadata returned 0x%08X\n",
            hr);
      JmsgVssApiStatus(jcr_, M_FATAL, hr, "GatherWriterMetadata");
      errno = b_errno_win32;
      return false;
    }

    // Wait for the async operation to finish and checks the result
    if (!WaitAndCheckForAsyncOperation(pAsync1.p)) {
      // Error message already printed
      errno = b_errno_win32;
      return false;
    }

#  define VSS_CALL(Obj, Name, ...)                                 \
    do {                                                           \
      HRESULT _vss_call_hr = (Obj)->Name(__VA_ARGS__);             \
      if (FAILED(_vss_call_hr)) {                                  \
        Dmsg1(0,                                                   \
              "VSSClientGeneric::Initialize: "                     \
              "IVssBackupComponents->" #Name " returned 0x%08X\n", \
              _vss_call_hr);                                       \
        JmsgVssApiStatus(jcr_, M_FATAL, _vss_call_hr, #Name);      \
        errno = b_errno_win32;                                     \
        return false;                                              \
      }                                                            \
    } while (0)

    UINT cWriters;
    VSS_CALL(pVssObj, GetWriterMetadataCount, &cWriters);

    VSS_ID idInstance;
    IVssExamineWriterMetadata* pMetadata;

    std::unordered_set<std::string> excluded_files;
    std::unordered_set<std::string> included_files;
    // NOTE(ssura): this is not correct yet
    // see
    // https://learn.microsoft.com/en-us/windows/win32/vss/generating-a-backup-set
    // for more info.  Basically the path that the following GetPath calls
    // return might not be a correct path at all.  We need to iterate
    // over it to get the correct paths, i.e. with Find[First|Next]File.
    // This is currently only used for debugging so it should be ok!

    for (UINT i = 0; i < cWriters; ++i) {
      Dmsg1(500, "VSS Writer: %u\n", i);
      VSS_CALL(pVssObj, GetWriterMetadata, i, &idInstance, &pMetadata);

      UINT cIncludeFiles, cExcludeFiles, cComponents;

      VSS_CALL(pMetadata, GetFileCounts, &cIncludeFiles, &cExcludeFiles,
               &cComponents);

      IVssWMFiledesc* pFileDesc;

      // NOTE(ssura): Include files are not supported on windows yet
      // see
      // https://learn.microsoft.com/en-us/windows/win32/api/vsbackup/nf-vsbackup-ivssexaminewritermetadata-getincludefile

      for (UINT exclude = 0; exclude < cExcludeFiles; ++exclude) {
        VSS_CALL(pMetadata, GetExcludeFile, exclude, &pFileDesc);

        BSTR bstrPath;

        VSS_CALL(pFileDesc, GetPath, &bstrPath);

        char* path = BSTR_2_str(bstrPath);

        if (auto [_, inserted] = excluded_files.emplace(path); inserted) {
          Dmsg1(500, "Excluded path: %s\n", path);
        }

        free(path);

        SysFreeString(bstrPath);

        pFileDesc->Release();
      }

      // NOTE(ssura): components are file groups
      //              exclude trumps component
      //              Components may have dependencies on other components.
      //              Currently this is not supported.

      // NOTE(ssura): a selectable component is _optional_ and can be
      //              included _optionally_.  A non selectable
      //              component is mandatory to include, if its parent is
      //              included!
      // See:
      // https://learn.microsoft.com/en-us/windows/win32/vss/working-with-selectability-for-backup
      //              We do not handle this here for now.

      for (UINT comp = 0; comp < cComponents; ++comp) {
        IVssWMComponent* pComponent;
        VSS_CALL(pMetadata, GetComponent, comp, &pComponent);
        PVSSCOMPONENTINFO pInfo;
        VSS_CALL(pComponent, GetComponentInfo, &pInfo);

        auto cFileCount = pInfo->cFileCount;
        auto cDatabases = pInfo->cDatabases;
        auto cLogFiles = pInfo->cLogFiles;
        auto cDependencies = pInfo->cDependencies;

        char* name = BSTR_2_str(pInfo->bstrComponentName);
        Dmsg1(500, "Start component: %s\n", name);
        free(name);

        BSTR bstrPath;
        for (UINT file = 0; file < cFileCount; ++file) {
          VSS_CALL(pComponent, GetFile, file, &pFileDesc);
          VSS_CALL(pFileDesc, GetPath, &bstrPath);

          char* path = BSTR_2_str(bstrPath);
          if (auto it = included_files.find(path);
              excluded_files.find(path) == excluded_files.end()
              && it == included_files.end()) {
            Dmsg1(1000, "File: %s\n", path);
            included_files.emplace_hint(it, path);
          }
          free(path);

          SysFreeString(bstrPath);
          pFileDesc->Release();
        }
        for (UINT file = 0; file < cDatabases; ++file) {
          VSS_CALL(pComponent, GetDatabaseFile, file, &pFileDesc);
          VSS_CALL(pFileDesc, GetPath, &bstrPath);

          char* path = BSTR_2_str(bstrPath);
          if (auto it = included_files.find(path);
              excluded_files.find(path) == excluded_files.end()
              && it == included_files.end()) {
            Dmsg1(1000, "DB File: %s\n", path);
            included_files.emplace_hint(it, path);
          }
          free(path);

          SysFreeString(bstrPath);
          pFileDesc->Release();
        }

        for (UINT file = 0; file < cLogFiles; ++file) {
          VSS_CALL(pComponent, GetDatabaseLogFile, file, &pFileDesc);
          VSS_CALL(pFileDesc, GetPath, &bstrPath);

          char* path = BSTR_2_str(bstrPath);
          if (auto it = included_files.find(path);
              excluded_files.find(path) == excluded_files.end()
              && it == included_files.end()) {
            Dmsg1(1000, "DB Log File: %s\n", path);
            included_files.emplace_hint(it, path);
          }
          free(path);

          SysFreeString(bstrPath);
          pFileDesc->Release();
        }

        for (UINT dep = 0; dep < cDependencies; ++dep) {
          IVssWMDependency* pDepedency;
          VSS_CALL(pComponent, GetDependency, dep, &pDepedency);
          BSTR bstrComponentName;
          VSS_CALL(pDepedency, GetComponentName, &bstrComponentName);

          char* name2 = BSTR_2_str(bstrComponentName);
          Dmsg1(500, "Depedency: %s\n", name2);
          free(name2);

          SysFreeString(bstrComponentName);
          pDepedency->Release();
        }

        Dmsg1(500, "End component\n");

        VSS_CALL(pComponent, FreeComponentInfo, pInfo);
        pComponent->Release();
      }

      pMetadata->Release();
    }

    Dmsg1(150, "VSS: Found %llu files to exclude and %llu files to include.\n",
          excluded_files.size(), included_files.size());
  }

  // We are during restore now?
  bDuringRestore_ = bDuringRestore;

  // Keep the context
  dwContext_ = dwContext;

  return true;
}

bool VSSClientGeneric::WaitAndCheckForAsyncOperation(IVssAsync* pAsync)
{
  HRESULT hrReturned = S_OK;
  std::chrono::milliseconds timeout = std::chrono::minutes(10);

  HRESULT wait = pAsync->Wait(
      static_cast<DWORD>(timeout.count()));  // wait at most 10 minutes
  if (FAILED(wait)) {
    JmsgVssApiStatus(jcr_, M_FATAL, wait, "async wait");
    return false;
  }

  HRESULT hr = pAsync->QueryStatus(&hrReturned, NULL);

  if (FAILED(hr)) {
    JmsgVssApiStatus(jcr_, M_FATAL, wait, "query async status");
    return false;
  }

  if (hrReturned != VSS_S_ASYNC_FINISHED) {
        Jmsg(jcr_, M_WARNING, 0,
             "WaitAndCheckForAsyncOperation: QueryStatus did not return ASYNC_FINISHED: %lu\n",
	     hrReturned);
    return false;
  }
  return true;
 // Add all drive letters that need to be snapshotted.
}

// Add all drive letters that need to be snapshotted.
void VSSClientGeneric::AddVolumeSnapshots(
    IVssBackupComponents* pVssObj,
    const std::vector<std::wstring>& volumes,
    bool onefs_disabled)
{
  VSS_ID pid;
  std::unordered_set<std::wstring> snapshoted_volumes{};

  /* szDriveLetters contains all drive letters in uppercase
   * If a drive can not being added, it's converted to lowercase in
   * szDriveLetters */
  for (const std::wstring& volume : volumes) {
    std::wstring unique_name = GetUniqueVolumeNameForPath(volume);

    std::string utf_unique = FromUtf16(unique_name);
    std::string utf_vol = FromUtf16(volume);

    bool snapshot_success = false;

    // Store uniquevolumname if it doesnt already exist
    if (auto found = snapshoted_volumes.find(unique_name);
        found == snapshoted_volumes.end()) {
      if (SUCCEEDED(pVssObj->AddToSnapshotSet(
              const_cast<LPWSTR>(unique_name.c_str()), GUID_NULL, &pid))) {
        if (debug_level >= 200) {
          Dmsg2(200, "%s added to snapshotset (Path: %s)\n", utf_unique.c_str(),
                utf_vol.c_str());
        }

        snapshot_success = true;
        snapshoted_volumes.emplace(unique_name.c_str());
      }

      if (onefs_disabled) {
        AddVolumeMountPointSnapshots(pVssObj, unique_name, snapshoted_volumes);
      } else {
        Jmsg(jcr_, M_INFO, 0,
             "VolumeMountpoints are not processed as onefs = yes.\n");
      }
    } else {
      snapshot_success = true;
    }

    if (snapshot_success) {
      mount_to_vol.emplace(std::move(utf_vol), std::move(utf_unique));
      mount_to_vol_w.emplace(volume, std::move(unique_name));
    }
  }
}

/*
 * Add all volume mountpoints that need to be snapshotted.
 * Volumes can be mounted multiple times, but can only be added to the
 * snapshotset once. So we skip adding a volume if it is already in snapshotset.
 * We count the total number of vmps and the number of volumes we added to the
 * snapshotset.
 */
void VSSClientGeneric::AddVolumeMountPointSnapshots(
    IVssBackupComponents* pVssObj,
    const std::wstring& volume,
    std::unordered_set<std::wstring>& snapshoted_volumes)
{
  PoolMem mp(PM_FNAME);
  constexpr auto size = MAX_PATH;
  mp.check_size(sizeof(wchar_t[size]));
  wchar_t* mountpoint = reinterpret_cast<wchar_t*>(mp.addr());

  HANDLE hMount = FindFirstVolumeMountPointW(volume.c_str(), mountpoint, size);
  if (hMount != INVALID_HANDLE_VALUE) {
    // Count number of vmps.
    VMPs += 1;
    if (HandleVolumeMountPoint(this, pVssObj, this->mount_to_vol,
                               this->mount_to_vol_w, snapshoted_volumes, volume,
                               mountpoint)) {
      // Count vmps that were snapshotted
      VMP_snapshots += 1;
    }

    while (FindNextVolumeMountPointW(hMount, mountpoint, size)) {
      // Count number of vmps.
      VMPs += 1;
      if (HandleVolumeMountPoint(this, pVssObj, this->mount_to_vol,
                                 this->mount_to_vol_w, snapshoted_volumes,
                                 volume, mountpoint)) {
        // Count vmps that were snapshotted
        VMP_snapshots += 1;
      }
    }
  }

  FindVolumeMountPointClose(hMount);
}

void VSSClientGeneric::ShowVolumeMountPointStats(JobControlRecord* jcr)
{
  if (VMPs) {
    Jmsg(jcr, M_INFO, 0,
         T_("Volume Mount Points found: %d, added to snapshotset: %d\n"), VMPs,
         VMP_snapshots);
  }
}

bool VSSClientGeneric::CreateSnapshots(const std::vector<std::wstring>& volumes,
                                       bool onefs_disabled)
{
  IVssBackupComponents* pVssObj;
  CComPtr<IVssAsync> pAsync1;
  CComPtr<IVssAsync> pAsync2;
  HRESULT hr;

  /* See
   * http://msdn.microsoft.com/en-us/library/windows/desktop/aa382870%28v=vs.85%29.aspx.
   */
  if (!pVssObject_ || bBackupIsInitialized_) {
    Jmsg(jcr_, M_FATAL, 0,
         "No pointer to VssObject or Backup is not Initialized\n");
    errno = ENOSYS;
    return false;
  }

  uidCurrentSnapshotSet_ = GUID_NULL;

  pVssObj = (IVssBackupComponents*)pVssObject_;

  // startSnapshotSet
  hr = pVssObj->StartSnapshotSet(&uidCurrentSnapshotSet_);
  while ((unsigned)hr == VSS_E_SNAPSHOT_SET_IN_PROGRESS) {
    Bmicrosleep(5, 0);
    Jmsg(jcr_, M_INFO, 0, "VSS_E_SNAPSHOT_SET_IN_PROGRESS, retrying ...\n");
    hr = pVssObj->StartSnapshotSet(&uidCurrentSnapshotSet_);
  }

  if (FAILED(hr)) {
    JmsgVssApiStatus(jcr_, M_FATAL, hr, "StartSnapshotSet");
    errno = ENOSYS;
    return false;
  }

  // AddToSnapshotSet
  if (volumes.size() > 0) {
    AddVolumeSnapshots(pVssObj, volumes, onefs_disabled);
  }

  // PrepareForBackup
  GeneratePluginEvent(jcr_, bEventVssPrepareSnapshot);
  hr = pVssObj->PrepareForBackup(&pAsync1.p);
  if (FAILED(hr)) {
    JmsgVssApiStatus(jcr_, M_FATAL, hr, "PrepareForBackup");
    errno = b_errno_win32;
    return false;
  }

  // Wait for the async operation to finish and checks the result.
  if (!WaitAndCheckForAsyncOperation(pAsync1.p)) {
    errno = b_errno_win32;
    return false;
  }

  // Get latest info about writer status.
  if (!CheckWriterStatus()) {
    errno = b_errno_win32; /* Error already printed */
    return false;
  }

  // DoSnapShotSet
  hr = pVssObj->DoSnapshotSet(&pAsync2.p);
  if (FAILED(hr)) {
    JmsgVssApiStatus(jcr_, M_FATAL, hr, "DoSnapshotSet");
    errno = b_errno_win32;
    return false;
  }

  // Waits for the async operation to finish and checks the result.
  if (!WaitAndCheckForAsyncOperation(pAsync2.p)) {
    errno = b_errno_win32;
    return false;
  }

  // Get latest info about writer status.
  if (!CheckWriterStatus()) {
    errno = b_errno_win32; /* Error already printed */
    return false;
  }

  // Query snapshot info.
  QuerySnapshotSet(uidCurrentSnapshotSet_);

  bBackupIsInitialized_ = true;

  return true;
}

bool VSSClientGeneric::CloseBackup()
{
  bool bRet = false;
  HRESULT hr;
  BSTR xml;
  IVssBackupComponents* pVssObj = (IVssBackupComponents*)pVssObject_;

  if (!pVssObject_) {
    Jmsg(jcr_, M_FATAL, 0, "VssObject is NULL.\n");
    errno = ENOSYS;
    return bRet;
  }
  CComPtr<IVssAsync> pAsync;

  bBackupIsInitialized_ = false;

  hr = pVssObj->BackupComplete(&pAsync.p);
  if (SUCCEEDED(hr)) {
    // Wait for the async operation to finish and checks the result.
    WaitAndCheckForAsyncOperation(pAsync.p);
    bRet = true;
  } else {
    JmsgVssApiStatus(jcr_, M_ERROR, hr, "BackupComplete");
    errno = b_errno_win32;
    pVssObj->AbortBackup();
  }

  // Get latest info about writer status.
  CheckWriterStatus();

  hr = pVssObj->SaveAsXML(&xml);
  if (SUCCEEDED(hr)) {
    metadata_ = xml;
  } else {
    metadata_ = NULL;
  }

  /* FIXME?: The docs
   * http://msdn.microsoft.com/en-us/library/aa384582%28v=VS.85%29.aspx say this
   * isn't required... */
  if (uidCurrentSnapshotSet_ != GUID_NULL) {
    VSS_ID idNonDeletedSnapshotID = GUID_NULL;
    LONG lSnapshots;

    pVssObj->DeleteSnapshots(uidCurrentSnapshotSet_, VSS_OBJECT_SNAPSHOT_SET,
                             false, &lSnapshots, &idNonDeletedSnapshotID);

    uidCurrentSnapshotSet_ = GUID_NULL;
  }

  if (bWriterStatusCurrent_) {
    bWriterStatusCurrent_ = false;
    pVssObj->FreeWriterStatus();
  }

  pVssObj->Release();
  pVssObject_ = NULL;

  // Call CoUninitialize if the CoInitializeEx was performed sucesfully
  if (bCoInitializeCalled_) {
    CoUninitialize();
    bCoInitializeCalled_ = false;
  }

  // from this point on we should only look at the vss snapshot
  // since the "conversion" cache might still contain presnapshot
  // conversions, we need to invalidate them here!

  Win32ResetConversionCache();
  return bRet;
}

wchar_t* VSSClientGeneric::GetMetadata() { return metadata_; }

bool VSSClientGeneric::CloseRestore()
{
  IVssBackupComponents* pVssObj = (IVssBackupComponents*)pVssObject_;
  CComPtr<IVssAsync> pAsync;

  if (!pVssObj) {
    errno = ENOSYS;
    return false;
  }
  return true;
}

// Query all the shadow copies in the given set
void VSSClientGeneric::QuerySnapshotSet(GUID snapshotSetID)
{
  if (!(CreateVssBackupComponents_ && VssFreeSnapshotProperties_)) {
    Jmsg(jcr_, M_FATAL, 0,
         "CreateVssBackupComponents or VssFreeSnapshotProperties API is "
         "NULL.\n");
    errno = ENOSYS;
    return;
  }

  if (snapshotSetID == GUID_NULL || pVssObject_ == NULL) {
    Jmsg(jcr_, M_FATAL, 0, "snapshotSetID == NULL or VssObject is NULL.\n");
    errno = ENOSYS;
    return;
  }

  IVssBackupComponents* pVssObj = (IVssBackupComponents*)pVssObject_;

  // Get list all shadow copies.
  CComPtr<IVssEnumObject> pIEnumSnapshots;
  HRESULT hr = pVssObj->Query(GUID_NULL, VSS_OBJECT_NONE, VSS_OBJECT_SNAPSHOT,
                              (IVssEnumObject**)(&pIEnumSnapshots));

  // If there are no shadow copies, just return
  if (FAILED(hr)) {
    Jmsg(jcr_, M_FATAL, 0, "No Volume Shadow copies made.\n");
    errno = b_errno_win32;
    return;
  }

  // Enumerate all shadow copies.
  VSS_OBJECT_PROP Prop;
  VSS_SNAPSHOT_PROP& Snap = Prop.Obj.Snap;

  while (true) {
    // Get the next element
    ULONG ulFetched;
    hr = (pIEnumSnapshots.p)->Next(1, &Prop, &ulFetched);

    // We reached the end of list
    if (ulFetched == 0) break;

    // Print the shadow copy (if not filtered out)
    if (Snap.m_SnapshotSetId == snapshotSetID) {
      // m_pwszExposedName = mount path! e.g. C:, X:\MountC, ...
      std::string vol = FromUtf16(Snap.m_pwszOriginalVolumeName);
      std::string vss = FromUtf16(Snap.m_pwszSnapshotDeviceObject);
      vol_to_vss.emplace(std::move(vol), std::move(vss));
      vol_to_vss_w.emplace(Snap.m_pwszOriginalVolumeName,
                           Snap.m_pwszSnapshotDeviceObject);
    }
    VssFreeSnapshotProperties_(&Snap);
  }
  errno = 0;
}

// Check the status for all selected writers
bool VSSClientGeneric::CheckWriterStatus()
{
  /* See
   * http://msdn.microsoft.com/en-us/library/windows/desktop/aa382870%28v=vs.85%29.aspx
   */
  IVssBackupComponents* pVssObj = (IVssBackupComponents*)pVssObject_;
  if (!pVssObj) {
    Jmsg(jcr_, M_FATAL, 0, "Cannot get IVssBackupComponents pointer.\n");
    errno = ENOSYS;
    return false;
  }
  DestroyWriterInfo();

  if (bWriterStatusCurrent_) {
    bWriterStatusCurrent_ = false;
    pVssObj->FreeWriterStatus();
  }

  // Gather writer status to detect potential errors
  CComPtr<IVssAsync> pAsync;

  HRESULT hr = pVssObj->GatherWriterStatus(&pAsync.p);
  if (FAILED(hr)) {
    JmsgVssApiStatus(jcr_, M_FATAL, hr, "GatherWriterStatus");
    errno = b_errno_win32;
    return false;
  }

  // Waits for the async operation to finish and checks the result
  if (!WaitAndCheckForAsyncOperation(pAsync.p)) {
    errno = b_errno_win32;
    return false;
  }

  bWriterStatusCurrent_ = true;

  unsigned cWriters = 0;

  hr = pVssObj->GetWriterStatusCount(&cWriters);
  if (FAILED(hr)) {
    JmsgVssApiStatus(jcr_, M_FATAL, hr, "GatherWriterStatusCount");
    errno = b_errno_win32;
    return false;
  }

  int nState;
  POOLMEM* szBuf = GetPoolMemory(PM_FNAME);

  // Enumerate each writer
  for (unsigned iWriter = 0; iWriter < cWriters; iWriter++) {
    VSS_ID idInstance = GUID_NULL;
    VSS_ID idWriter = GUID_NULL;
    VSS_WRITER_STATE eWriterStatus = VSS_WS_UNKNOWN;
    CComBSTR bstrWriterName;
    HRESULT hrWriterFailure = S_OK;

    // Get writer status
    hr = pVssObj->GetWriterStatus(iWriter, &idInstance, &idWriter,
                                  &bstrWriterName, &eWriterStatus,
                                  &hrWriterFailure);
    if (FAILED(hr)) {
      // Api failed
      JmsgVssApiStatus(jcr_, M_WARNING, hr, "GetWriterStatus");
      nState = 0; /* Unknown writer state -- API failed */
    } else {
      switch (eWriterStatus) {
        case VSS_WS_FAILED_AT_IDENTIFY:
        case VSS_WS_FAILED_AT_PREPARE_BACKUP:
        case VSS_WS_FAILED_AT_PREPARE_SNAPSHOT:
        case VSS_WS_FAILED_AT_FREEZE:
        case VSS_WS_FAILED_AT_THAW:
        case VSS_WS_FAILED_AT_POST_SNAPSHOT:
        case VSS_WS_FAILED_AT_BACKUP_COMPLETE:
        case VSS_WS_FAILED_AT_PRE_RESTORE:
        case VSS_WS_FAILED_AT_POST_RESTORE:
#  if defined(B_VSS_W2K3) || defined(B_VSS_VISTA)
        case VSS_WS_FAILED_AT_BACKUPSHUTDOWN:
#  endif
          // Writer status problem
          wchar_2_UTF8(szBuf, bstrWriterName.p);
          JmsgVssWriterStatus(jcr_, M_WARNING, eWriterStatus, szBuf);
          nState = -1; /* bad writer state */
          break;
        default:
          nState = 1; /* Writer state OK */
      }
    }

    // store text info
    char str[1000];
    bstrncpy(str, "\"", sizeof(str));
    wchar_2_UTF8(szBuf, bstrWriterName.p);
    bstrncat(str, szBuf, sizeof(str));
    bstrncat(str, "\", State: 0x", sizeof(str));
    itoa(eWriterStatus, szBuf, SizeofPoolMemory(szBuf));
    bstrncat(str, szBuf, sizeof(str));
    bstrncat(str, " (", sizeof(str));
    wchar_2_UTF8(szBuf, GetStringFromWriterStatus(eWriterStatus));
    bstrncat(str, szBuf, sizeof(str));
    bstrncat(str, ")", sizeof(str));
    AppendWriterInfo(nState, (const char*)str);
  }

  FreePoolMemory(szBuf);
  errno = 0;

  return true;
}

#endif /* WIN32_VSS */
