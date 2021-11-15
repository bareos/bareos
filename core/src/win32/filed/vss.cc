/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2005-2010 Free Software Foundation Europe e.V.
   Copyright (C) 2014-2019 Bareos GmbH & Co. KG

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
#  include "filed/jcr_private.h"
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

  if (jcr && jcr->impl->pVSSClient) {
    return jcr->impl->pVSSClient->GetShadowPath(szFilePath, szShadowPath,
                                                nBuflen);
  }

  return false;
}

static bool VSSPathConvertW(const wchar_t* szFilePath,
                            wchar_t* szShadowPath,
                            int nBuflen)
{
  JobControlRecord* jcr = GetJcrFromThreadSpecificData();

  if (jcr && jcr->impl->pVSSClient) {
    return jcr->impl->pVSSClient->GetShadowPathW(szFilePath, szShadowPath,
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
        jcr->impl->pVSSClient = new VSSClientXP();
        break;
      case 2:
        jcr->impl->pVSSClient = new VSSClient2003();
        break;
    }
    // Vista or Longhorn or later
  } else if (g_MajorVersion >= 6) {
    jcr->impl->pVSSClient = new VSSClientVista();
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

  // Check for valid pathname
  bool bIsValidName;

  bIsValidName = strlen(szFilePath) > 3;
  if (bIsValidName)
    bIsValidName &= isalpha(szFilePath[0]) && szFilePath[1] == ':'
                    && szFilePath[2] == '\\';

  if (bIsValidName) {
    int nDriveIndex = toupper(szFilePath[0]) - 'A';
    if (szShadowCopyName_[nDriveIndex][0] != 0) {
      if (WideCharToMultiByte(CP_UTF8, 0, szShadowCopyName_[nDriveIndex], -1,
                              szShadowPath, nBuflen - 1, NULL, NULL)) {
        nBuflen -= (int)strlen(szShadowPath);
        bstrncat(szShadowPath, szFilePath + 2, nBuflen);
        return true;
      }
    }
  }

  bstrncpy(szShadowPath, szFilePath, nBuflen);
  errno = EINVAL;
  return false;
}

bool VSSClient::GetShadowPathW(const wchar_t* szFilePath,
                               wchar_t* szShadowPath,
                               int nBuflen)
{
  if (!bBackupIsInitialized_) return false;

  // Check for valid pathname
  bool bIsValidName;

  bIsValidName = wcslen(szFilePath) > 3;
  if (bIsValidName)
    bIsValidName &= iswalpha(szFilePath[0]) && szFilePath[1] == ':'
                    && szFilePath[2] == '\\';

  if (bIsValidName) {
    int nDriveIndex = towupper(szFilePath[0]) - 'A';
    if (szShadowCopyName_[nDriveIndex][0] != 0) {
      wcsncpy(szShadowPath, szShadowCopyName_[nDriveIndex], nBuflen);
      nBuflen -= (int)wcslen(szShadowCopyName_[nDriveIndex]);
      wcsncat(szShadowPath, szFilePath + 2, nBuflen);
      return true;
    }
  }

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
