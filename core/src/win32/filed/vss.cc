/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2005-2010 Free Software Foundation Europe e.V.
   Copyright (C) 2014-2022 Bareos GmbH & Co. KG

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
 Copyright transferred from MATRIX-Computer GmbH to
   Kern Sibbald by express permission.
 Author          : Thorsten Engel
 Created On      : Fri May 06 21:44:00 2005
*/
/* @file
 Interface to Volume Shadow Copies (VSS)
*/

#ifdef WIN32_VSS

#  include "include/bareos.h"
#  include "filed/filed.h"
#  include "filed/filed_jcr_impl.h"
#  include "lib/thread_specific_data.h"

#  include "ms_atl.h"
#  include <objbase.h>

using namespace filedaemon;

// { b5946137-7b9f-4925-af80-51abd60b20d5 }
static const GUID VSS_SWPRV_ProviderID
    = {0xb5946137,
       0x7b9f,
       0x4925,
       {0xaf, 0x80, 0x51, 0xab, 0xd6, 0x0b, 0x20, 0xd5}};

static bool VSSPathConvert(const char* szFilePath,
                           char* szShadowPath,
                           int nBuflen)
{
  JobControlRecord* jcr = GetJcrFromThreadSpecificData();

  if (jcr && jcr->fd_impl->pVSSClient) {
    return jcr->fd_impl->pVSSClient->GetShadowPath(szFilePath, szShadowPath,
                                                   nBuflen);
  }

  return false;
}

static bool VSSPathConvertW(const wchar_t* szFilePath,
                            wchar_t* szShadowPath,
                            int nBuflen)
{
  JobControlRecord* jcr = GetJcrFromThreadSpecificData();

  if (jcr && jcr->fd_impl->pVSSClient) {
    return jcr->fd_impl->pVSSClient->GetShadowPathW(szFilePath, szShadowPath,
                                                    nBuflen);
  }

  return false;
}

void VSSInit(JobControlRecord* jcr)
{
  // Decide which vss class to initialize
  if (g_MajorVersion == 5) {
    switch (g_MinorVersion) {
      case 1:
        jcr->fd_impl->pVSSClient = new VSSClientXP();
        break;
      case 2:
        jcr->fd_impl->pVSSClient = new VSSClient2003();
        break;
    }
    // Vista or Longhorn or later
  } else if (g_MajorVersion >= 6) {
    jcr->fd_impl->pVSSClient = new VSSClientVista();
  }

  // Setup the callback functions.
  if (!SetVSSPathConvert(VSSPathConvert, VSSPathConvertW)) {
    Jmsg(jcr, M_FATAL, 0, "Failed to setup VSS Path Conversion callbacks.\n");
  }
}

// Destructor
VSSClient::~VSSClient()
{
  /*
   * Release the IVssBackupComponents interface
   * WARNING: this must be done BEFORE calling CoUninitialize()
   */
  if (pVssObject_) {
    //      pVssObject_->Release();
    pVssObject_ = NULL;
  }

  DestroyWriterInfo();

  // Call CoUninitialize if the CoInitialize was performed successfully
  if (bCoInitializeCalled_) { CoUninitialize(); }
}

bool VSSClient::InitializeForBackup(JobControlRecord* jcr)
{
  jcr_ = jcr;

  GeneratePluginEvent(jcr, bEventVssInitializeForBackup);

  return Initialize(0);
}

bool VSSClient::InitializeForRestore(JobControlRecord* jcr)
{
  metadata_ = NULL;
  jcr_ = jcr;

  GeneratePluginEvent(jcr, bEventVssInitializeForRestore);

  return Initialize(0, true /*=>Restore*/);
}

bool VSSClient::GetShadowPath(const char* szFilePath,
                              char* szShadowPath,
                              int nBuflen)
{
  if (!bBackupIsInitialized_) return false;

  int volsize = 64;
  std::unique_ptr<char[]> volbuf = std::unique_ptr<char[]>{new char[volsize], std::default_delete<char[]>{}};
  int size = 512;
  std::unique_ptr<char[]> buf = std::unique_ptr<char[]>{new char[size], std::default_delete<char[]>{}};
  int needed_size;
  char* test = nullptr;
  while (needed_size = GetFullPathNameA(szFilePath, size, buf.get(), &test),
	 needed_size + 1 >= size) {
    size *= 2;
    buf = std::unique_ptr<char[]>{new char[size], std::default_delete<char[]>{}};
  }
  char* MountPoint = buf.get();
  char* VolumeName = volbuf.get();
  if (!GetVolumePathNameA(szFilePath,
			  MountPoint,
			  size)) {

    DWORD lerror = GetLastError();
    LPTSTR msg;
    FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
		  NULL, lerror, 0, (LPTSTR)&msg, 0, NULL);
    MessageBox(NULL, msg, "GetShadowPath", MB_OK);
    goto bail_out;
  }
  {
    // ensure that it ends with a backslash
    // ending with a backslash is a precondition for
    // GetVolumeNameForVolumeMountPoint
    auto len = strlen(MountPoint);
    if (len == 0) goto bail_out;
    if (MountPoint[len-1] != '\\') {
      MountPoint[len] = '\\';
      MountPoint[len+1] = '\0';
    }
  }
  {
    char* tmp = MountPoint;
    while (*tmp && *tmp == *szFilePath) {
      szFilePath += 1;
      tmp += 1;
    }
  }
  if (!GetVolumeNameForVolumeMountPointA(MountPoint,
					 VolumeName, volsize)) {
    DWORD lerror = GetLastError();
    LPTSTR msg;
    FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
		  NULL, lerror, 0, (LPTSTR)&msg, 0, NULL);
    MessageBox(NULL, msg, "GetShadowPath", MB_OK);
    goto bail_out;
  }

  if (auto found = vol_to_vss.find(std::string{VolumeName});
      found != vol_to_vss.end()) {
    if (WideCharToMultiByte(CP_UTF8, 0, found->second.c_str(), -1,
			    szShadowPath, nBuflen - 1, nullptr, nullptr)) {
      auto len = strlen(szShadowPath);
      szShadowPath[len] = '\\';
      szShadowPath[len+1] = '\0';
      nBuflen -= len + 1;

      bstrncat(szShadowPath, szFilePath, nBuflen);
      return true;
    }
  }

 bail_out:
  bstrncpy(szShadowPath, szFilePath, nBuflen);
  errno = EINVAL;
  return false;
}

bool VSSClient::GetShadowPathW(const wchar_t* szFilePath,
                               wchar_t* szShadowPath,
                               int nBuflen)
{
  if (!bBackupIsInitialized_) return false;

  wcsncpy(szShadowPath, szFilePath, nBuflen);
  errno = EINVAL;
  return false;
}

size_t VSSClient::GetWriterCount() const { return writer_info_.size(); }

const char* VSSClient::GetWriterInfo(size_t nIndex) const
{
  if (nIndex < writer_info_.size()) {
    return writer_info_[nIndex].info_text_.c_str();
  }
  return nullptr;
}

int VSSClient::GetWriterState(size_t nIndex) const
{
  if (nIndex < writer_info_.size()) { return writer_info_[nIndex].state_; }
  return 0;
}

void VSSClient::AppendWriterInfo(int nState, const char* pszInfo)
{
  WriterInfo info;
  info.state_ = nState;
  info.info_text_ = pszInfo;
  writer_info_.push_back(info);
}

// Note, this is called at the end of every job, so release all items
void VSSClient::DestroyWriterInfo() { writer_info_.clear(); }
#endif
