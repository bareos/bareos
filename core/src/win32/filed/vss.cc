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

static std::size_t AppendPathPart(std::string& s, const char* path) {
  for (std::size_t lookahead = 0; path[lookahead]; lookahead += 1) {
    char cur = path[lookahead];
    if (cur == '\\') {
      s.append(path, lookahead+1);
      return lookahead+1;
    }
  }
  return 0;
}

static std::pair<std::string, std::string> FindMountPoint(const char* path,
							  std::unordered_map<std::string, std::string>& mount_to_vol)
{
  std::string current_volume{};
  std::size_t offset{0};
  std::size_t lookahead{0};
  while (lookahead = AppendPathPart(current_volume, path + offset),
	 lookahead != 0) {
    offset += lookahead;
    if (auto found = mount_to_vol.find(current_volume);
	found != mount_to_vol.end()) {
      current_volume.assign(found->second);
      path += offset;
      offset = 0;
    }
  }
  current_volume.resize(current_volume.size() - offset);
  return {std::move(current_volume), path};
}

bool VSSClient::GetShadowPath(const char* szFilePath,
                              char* szShadowPath,
                              int nBuflen)
{
  if (!bBackupIsInitialized_) return false;

  auto [volume, path] = FindMountPoint(szFilePath, this->mount_to_vol);

  if (auto found = vol_to_vss.find(volume);
      found != vol_to_vss.end()) {
    bstrncpy(szShadowPath, found->second.c_str(), nBuflen);
    auto len = strlen(szShadowPath);
    szShadowPath[len] = '\\';
    szShadowPath[len+1] = '\0';
    nBuflen -= len + 1;
    bstrncat(szShadowPath, path.c_str(), nBuflen);
    return true;
  } else {
    Dmsg4(50, "Could not find shadow volume for volume '%s' (path = '%s'; input = '%s').\n"
	  "Falling back to live system!\n",
	  volume.c_str(), path.c_str(), szFilePath);
    goto bail_out;
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
