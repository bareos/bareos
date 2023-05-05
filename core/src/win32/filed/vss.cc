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

static char* VSSPathConvert(const char* szFilePath)
{
  JobControlRecord* jcr = GetJcrFromThreadSpecificData();

  if (jcr && jcr->fd_impl->pVSSClient) {
    return jcr->fd_impl->pVSSClient->GetShadowPath(szFilePath);
  }

  return nullptr;
}

static wchar_t* VSSPathConvertW(const wchar_t* szFilePath)
{
  JobControlRecord* jcr = GetJcrFromThreadSpecificData();

  if (jcr && jcr->fd_impl->pVSSClient) {
    return jcr->fd_impl->pVSSClient->GetShadowPathW(szFilePath);
  }

  return nullptr;
}

void VSSInit(JobControlRecord* jcr)
{
  // Decide which vss class to initialize
  if (g_MajorVersion == 5) {
    Jmsg(jcr, M_FATAL, 0, "Your windows version is not supported anymore.\n");
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

template <typename CharT>
static std::size_t AppendPathPart(std::basic_string<CharT>& s, const CharT* path) {
  for (std::size_t lookahead = 0; path[lookahead]; lookahead += 1) {
    CharT cur = path[lookahead];
    if (cur == CharT{'\\'}) {
      s.append(path, lookahead+1);
      return lookahead+1;
    }
  }
  return 0;
}

// splits path into two sections:
// 1) a 'maximal' mountpoint `current_volume`
// 2) the rest of the path `path`
// `current_volume is maximal in the sense that there are no mount points left
// in `path` that registered in `mount_to_vol`.
// We then return the volume guid of `current_volume` and the rest of the path.
template <typename CharT, typename String = std::basic_string<CharT>>
static std::pair<String, String> FindMountPoint(const CharT* path,
						std::unordered_map<String, String>& mount_to_vol)
{
  String current_volume{};
  std::size_t offset{0};
  std::size_t lookahead{0};
  // idea:
  // we have { C:/ -> VolC, VolC/dir/mountD/ -> VolD } and path is
  // C:/dir/mountD/path.   Then it goes like this:
  // current_volume = C:/ -> VolC; path = dir/mountD/path; lock in path
  // current_volume = VolC/dir/ -> X; path = mountD/path
  // current_volume = VolC/did/mountD/ -> VolD; path = path; lock in path
  // no more / found so reset to the last locked in path and return
  // -> current_volume = VolD; path = path
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
  // TODO: we do not need new strings for that
  // we could just return char pointers!
  return {std::move(current_volume), path};
}

char* VSSClient::GetShadowPath(const char* szFilePath)
{
  using namespace std::literals;
  if (!bBackupIsInitialized_) return nullptr;

  auto [volume, path] = FindMountPoint(szFilePath, this->mount_to_vol);

  if (auto found = vol_to_vss.find(volume);
      found != vol_to_vss.end()) {
    constexpr std::string_view sep = "\\"sv;
    auto len = found->second.size() + sep.size() + path.size() + 1;
    char* shadow_path = (char*)malloc(len * sizeof(*shadow_path));
    shadow_path[len - 1] = '\0';
    auto head = std::copy(std::begin(found->second), std::end(found->second),
			  shadow_path);

    head = std::copy(std::begin(sep), std::end(sep), head);

    head = std::copy(std::begin(path), std::end(path), head);

    ASSERT(head == shadow_path + len - 1);
    return shadow_path;
  } else {
    Dmsg4(50, "Could not find shadow volume for volume '%s' (path = '%s'; input = '%s').\n"
	  "Falling back to live system!\n",
	  volume.c_str(), path.c_str(), szFilePath);
    goto bail_out;
  }

 bail_out:
  errno = EINVAL;
  return nullptr;
}

wchar_t* VSSClient::GetShadowPathW(const wchar_t* szFilePath)
{
  using namespace std::literals;
  if (!bBackupIsInitialized_) return nullptr;

  auto [volume, path] = FindMountPoint(szFilePath, this->mount_to_vol_w);

  if (auto found = vol_to_vss_w.find(volume);
      found != vol_to_vss_w.end()) {
    // we need two extra chars; one for the null terminator and one for
    // the backslash between the parts
    constexpr std::wstring_view sep = L"\\"sv;
    auto len = found->second.size() + sep.size() + path.size() + 1;
    wchar_t* shadow_path = (wchar_t*)malloc(len * sizeof(*shadow_path));
    shadow_path[len - 1] = '\0';
    auto head = std::copy(std::begin(found->second), std::end(found->second),
			  shadow_path);

    head = std::copy(std::begin(sep), std::end(sep), head);

    head = std::copy(std::begin(path), std::end(path), head);

    ASSERT(head == shadow_path + len - 1);
    return shadow_path;
  } else {
    // TODO: how to do dmsg with wstrs ?
    Dmsg4(50, "Could not find shadow volume.\n"
	  "Falling back to live system!\n");
    goto bail_out;
  }

 bail_out:
  errno = EINVAL;
  return nullptr;
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
