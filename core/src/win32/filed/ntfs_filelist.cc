/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2026-2026 Bareos GmbH & Co. KG

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
#include "include/bareos.h"
#include "include/jcr.h"
#include "win32/filed/ntfs_filelist.h"
#include "filed/accurate.h"
#include "filed/filed_conf.h"
#include "filed/filed_globals.h"
#include "filed/filed_jcr_impl.h"
#include "findlib/find.h"
#include "win32/include/vss_client.h"

#include <winioctl.h>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace filedaemon {
namespace {

struct Handle {
  HANDLE value{INVALID_HANDLE_VALUE};
  ~Handle()
  {
    if (value != INVALID_HANDLE_VALUE) { CloseHandle(value); }
  }
};

std::runtime_error WindowsError(const char* operation)
{
  DWORD error = GetLastError();
  return std::runtime_error(std::string(operation) + ": Windows error "
                            + std::to_string(error));
}

std::wstring Wide(const std::string& text)
{
  int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(),
                                 static_cast<int>(text.size()), nullptr, 0);
  if (!size) { throw WindowsError("UTF-8 conversion"); }
  std::wstring result(size, L'\0');
  if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(),
                           static_cast<int>(text.size()), result.data(),
                           size)) {
    throw WindowsError("UTF-8 conversion");
  }
  return result;
}

std::string Utf8(const std::wstring& text)
{
  int size = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text.data(),
                                 static_cast<int>(text.size()), nullptr, 0,
                                 nullptr, nullptr);
  if (!size) { throw WindowsError("UTF-16 conversion"); }
  std::string result(size, '\0');
  if (!WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text.data(),
                           static_cast<int>(text.size()), result.data(), size,
                           nullptr, nullptr)) {
    throw WindowsError("UTF-16 conversion");
  }
  return result;
}

std::string Key(const char* path)
{
  std::wstring text = Wide(path);
  for (auto& c : text) {
    if (c == L'\\') { c = L'/'; }
  }
  while (!text.empty() && text.back() == L'/') { text.pop_back(); }
  std::wstring lower(text.size(), L'\0');
  if (!LCMapStringW(LOCALE_INVARIANT, LCMAP_LOWERCASE, text.data(),
                    static_cast<int>(text.size()), lower.data(),
                    static_cast<int>(lower.size()))) {
    throw WindowsError("path normalization");
  }
  return Utf8(lower);
}

std::string Scope(const char* job,
                  const std::wstring& volume,
                  const char* root,
                  const std::string& selection)
{
  return std::string(job) + "|" + Utf8(volume) + "|" + Key(root) + "|"
         + selection;
}

std::wstring StatePath(const std::string& scope)
{
  uint64_t hash = 14695981039346656037ULL;
  for (unsigned char c : scope) {
    hash ^= c;
    hash *= 1099511628211ULL;
  }
  return Wide(me->working_directory) + L"\\ntfs-usn-" + std::to_wstring(hash)
         + L".state";
}

USN_JOURNAL_DATA_V0 Query(HANDLE volume)
{
  USN_JOURNAL_DATA_V0 data{};
  DWORD size{};
  if (!DeviceIoControl(volume, FSCTL_QUERY_USN_JOURNAL, nullptr, 0, &data,
                       sizeof(data), &size, nullptr)) {
    throw WindowsError("FSCTL_QUERY_USN_JOURNAL");
  }
  if (size < sizeof(data)) {
    throw std::runtime_error("truncated journal metadata");
  }
  return data;
}

template <typename Consumer>
void Records(const std::vector<char>& buffer, DWORD bytes, Consumer consume)
{
  size_t offset = sizeof(uint64_t);
  if (bytes < offset || bytes > buffer.size()) {
    throw std::runtime_error("truncated USN response");
  }
  while (offset < bytes) {
    USN_RECORD_V2 record{};
    constexpr size_t header = offsetof(USN_RECORD_V2, FileName);
    if (bytes - offset < header) {
      throw std::runtime_error("truncated USN record");
    }
    std::memcpy(&record, buffer.data() + offset, header);
    if (record.MajorVersion != 2 || record.RecordLength < header
        || record.RecordLength > bytes - offset
        || record.FileNameOffset < header
        || record.FileNameOffset > record.RecordLength
        || record.FileNameLength > record.RecordLength - record.FileNameOffset
        || record.FileNameLength % sizeof(wchar_t) != 0) {
      throw std::runtime_error("unsupported or malformed USN record");
    }
    std::wstring name(record.FileNameLength / sizeof(wchar_t), L'\0');
    std::memcpy(name.data(), buffer.data() + offset + record.FileNameOffset,
                record.FileNameLength);
    consume(record, name);
    offset += record.RecordLength;
  }
}

NtfsReferenceMap Enumerate(JobControlRecord* jcr,
                           HANDLE volume,
                           uint64_t root,
                           int64_t since)
{
  NtfsReferenceMap map;
  map.root = root;
  MFT_ENUM_DATA_V0 request{};
  request.HighUsn = std::numeric_limits<int64_t>::max();
  std::vector<char> buffer(1024 * 1024);
  DWORD bytes{};
  while (DeviceIoControl(volume, FSCTL_ENUM_USN_DATA, &request, sizeof(request),
                         buffer.data(), static_cast<DWORD>(buffer.size()),
                         &bytes, nullptr)) {
    if (jcr->IsJobCanceled()) {
      throw std::runtime_error("job canceled during MFT enumeration");
    }
    Records(buffer, bytes,
            [&](const USN_RECORD_V2& record, const std::wstring& name) {
              map.records.emplace(
                  record.FileReferenceNumber,
                  NtfsRecord{
                      record.ParentFileReferenceNumber, Utf8(name),
                      (record.FileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0,
                      record.Usn});
              // Include changes racing with the bounded journal read.
              if (record.Usn >= since) {
                map.changed.insert(record.FileReferenceNumber);
              }
            });
    uint64_t next{};
    std::memcpy(&next, buffer.data(), sizeof(next));
    if (next <= request.StartFileReferenceNumber) {
      throw std::runtime_error("MFT enumeration made no progress");
    }
    request.StartFileReferenceNumber = next;
  }
  if (GetLastError() != ERROR_HANDLE_EOF) {
    throw WindowsError("FSCTL_ENUM_USN_DATA");
  }
  return map;
}

void ReadChanges(JobControlRecord* jcr,
                 HANDLE volume,
                 NtfsReferenceMap& map,
                 NtfsCursor saved,
                 NtfsCursor current,
                 int64_t first)
{
  READ_USN_JOURNAL_DATA_V0 request{};
  request.StartUsn = first;
  request.ReasonMask = MAXDWORD;
  request.UsnJournalID = current.journal;
  std::vector<char> buffer(1024 * 1024);
  while (request.StartUsn < current.next) {
    if (jcr->IsJobCanceled()) {
      throw std::runtime_error("job canceled during journal enumeration");
    }
    DWORD bytes{};
    if (!DeviceIoControl(volume, FSCTL_READ_USN_JOURNAL, &request,
                         sizeof(request), buffer.data(),
                         static_cast<DWORD>(buffer.size()), &bytes, nullptr)) {
      throw WindowsError("FSCTL_READ_USN_JOURNAL");
    }
    Records(buffer, bytes,
            [&](const USN_RECORD_V2& record, const std::wstring&) {
              map.Observe(record.FileReferenceNumber, record.Usn,
                          (record.Reason & USN_REASON_CLOSE) != 0, saved.next);
            });
    int64_t next{};
    std::memcpy(&next, buffer.data(), sizeof(next));
    if (next <= request.StartUsn) {
      throw std::runtime_error("journal read made no progress");
    }
    request.StartUsn = next;
  }
}

// MFT enumeration returns one name per file, not all hard-link names.
std::vector<std::wstring> Names(const std::wstring& path)
{
  DWORD size = 32768;
  std::vector<wchar_t> buffer(size);
  HANDLE search = FindFirstFileNameW(path.c_str(), 0, &size, buffer.data());
  if (search == INVALID_HANDLE_VALUE) {
    throw WindowsError("FindFirstFileNameW");
  }
  std::vector<std::wstring> result;
  result.emplace_back(buffer.data());
  while (true) {
    size = static_cast<DWORD>(buffer.size());
    if (FindNextFileNameW(search, &size, buffer.data())) {
      result.emplace_back(buffer.data());
      continue;
    }
    DWORD error = GetLastError();
    FindClose(search);
    if (error != ERROR_HANDLE_EOF) {
      SetLastError(error);
      throw WindowsError("FindNextFileNameW");
    }
    return result;
  }
}

std::string PathById(HANDLE volume, uint64_t id)
{
  FILE_ID_DESCRIPTOR descriptor{};
  descriptor.dwSize = sizeof(descriptor);
  descriptor.Type = FileIdType;
  descriptor.FileId.QuadPart = static_cast<LONGLONG>(id);
  Handle file{
      OpenFileById(volume, &descriptor, FILE_READ_ATTRIBUTES,
                   FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                   nullptr, FILE_FLAG_BACKUP_SEMANTICS)};
  if (file.value == INVALID_HANDLE_VALUE) {
    throw std::runtime_error(WindowsError("resolve file reference").what()
                             + std::string(" (FRN ") + std::to_string(id)
                             + ")");
  }
  std::wstring path(32768, L'\0');
  DWORD length = GetFinalPathNameByHandleW(file.value, path.data(),
                                           static_cast<DWORD>(path.size()),
                                           VOLUME_NAME_NONE);
  if (!length || length >= path.size()) {
    throw WindowsError("GetFinalPathNameByHandleW");
  }
  path.resize(length);
  auto result = Utf8(path);
  for (auto& c : result) {
    if (c == '\\') { c = '/'; }
  }
  return result;
}

}  // namespace

bool NtfsFileList::Prepare(JobControlRecord* jcr,
                           FindFilesPacket* ff,
                           const char* root)
{
  children_.clear();
  unchanged_.clear();
  fallback_reason.clear();
  try {
    // Skipping stat cannot evaluate attribute-dependent or per-file options.
    auto* include = ff->fileset->incexe;
    if (include->opts_list.size() != 1 || ff->opt_plugin || ff->StripPath
        || ff->size_match || jcr->rerunning || include->ignoredir.size()
        || BitIsSet(FO_MULTIFS, ff->flags)) {
      throw std::runtime_error("unsupported FileSet options");
    }
    auto* options = static_cast<findFOPTS*>(include->opts_list.get(0));
    if (options->regex.size() || options->regexdir.size()
        || options->regexfile.size() || options->wild.size()
        || options->wilddir.size() || options->wildfile.size()
        || options->wildbase.size()
        || (ff->accurate_opts
            & (accurate_always | accurate_md5 | accurate_sha1
               | accurate_atime))) {
      throw std::runtime_error(
          "per-file matching or checksum/atime comparison");
    }
    std::ostringstream selection_stream;
    selection_stream << std::quoted(
        jcr->fd_impl->director ? jcr->fd_impl->director->resource_name_ : "");
    for (unsigned char flag : ff->flags) {
      selection_stream << ' ' << static_cast<unsigned>(flag);
    }
    selection_stream << ' ' << ff->accurate_opts << ' ' << jcr->accurate;
    for (auto* excluded : ff->fileset->exclude_list) {
      dlistString* name;
      foreach_dlist (name, &excluded->name_list) {
        selection_stream << ' ' << std::quoted(name->c_str());
      }
      if (excluded->opts_list.size()) {
        throw std::runtime_error("Options in Exclude block");
      }
    }
    std::string selection = selection_stream.str();
    std::wstring original = Wide(root);
    for (auto& c : original) {
      if (c == L'/') { c = L'\\'; }
    }
    wchar_t mount[32768]{};
    wchar_t volume_name[64]{};
    wchar_t filesystem[64]{};
    if (!GetVolumePathNameW(original.c_str(), mount, 32768)
        || !GetVolumeNameForVolumeMountPointW(mount, volume_name, 64)
        || !GetVolumeInformationW(mount, nullptr, 0, nullptr, nullptr, nullptr,
                                  filesystem, 64)) {
      throw WindowsError("NTFS volume lookup");
    }
    if (std::wstring(filesystem) != L"NTFS") {
      throw std::runtime_error("volume is not NTFS");
    }
    std::wstring device(volume_name);
    device.pop_back();
    Handle live{CreateFileW(device.c_str(), GENERIC_READ | GENERIC_WRITE,
                            FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                            OPEN_EXISTING, 0, nullptr)};
    if (live.value == INVALID_HANDLE_VALUE) {
      throw WindowsError("open NTFS volume");
    }
    USN_JOURNAL_DATA_V0 data{};
    DWORD bytes{};
    if (!DeviceIoControl(live.value, FSCTL_QUERY_USN_JOURNAL, nullptr, 0, &data,
                         sizeof(data), &bytes, nullptr)) {
      if (GetLastError() != ERROR_JOURNAL_NOT_ACTIVE) {
        throw WindowsError("FSCTL_QUERY_USN_JOURNAL");
      }
      CREATE_USN_JOURNAL_DATA create{};
      create.MaximumSize = 64 * 1024 * 1024;
      create.AllocationDelta = 8 * 1024 * 1024;
      if (!DeviceIoControl(live.value, FSCTL_CREATE_USN_JOURNAL, &create,
                           sizeof(create), nullptr, 0, &bytes, nullptr)) {
        throw WindowsError("FSCTL_CREATE_USN_JOURNAL");
      }
      Jmsg(jcr, M_INFO, 0, T_("Created NTFS USN journal for %s.\n"), root);
    }

    std::wstring enumeration_root(mount);
    HANDLE enumeration_volume = live.value;
    Handle snapshot;
    if (jcr->fd_impl->pVSSClient) {
      std::unique_ptr<wchar_t, decltype(&std::free)> shadow(
          jcr->fd_impl->pVSSClient->GetShadowPathW(mount), &std::free);
      if (!shadow || std::wstring(shadow.get()) == std::wstring(mount)) {
        throw std::runtime_error("volume is not in the VSS snapshot");
      }
      enumeration_root = shadow.get();
      std::wstring snapshot_device = enumeration_root;
      while (snapshot_device.back() == L'\\') { snapshot_device.pop_back(); }
      snapshot.value = CreateFileW(snapshot_device.c_str(), GENERIC_READ,
                                   FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                                   OPEN_EXISTING, 0, nullptr);
      if (snapshot.value == INVALID_HANDLE_VALUE) {
        throw WindowsError("open VSS volume");
      }
      enumeration_volume = snapshot.value;
    }
    data = Query(enumeration_volume);
    NtfsCursor current{data.UsnJournalID, data.NextUsn};
    int64_t since = 0;
    std::optional<NtfsCursor> saved;
    if (ff->incremental) {
      std::string scope
          = Scope(jcr->fd_impl->PrevJob, volume_name, root, selection);
      std::ifstream input(StatePath(scope));
      auto cursor = ReadNtfsCheckpoint(input, scope);
      if (cursor && UsableNtfsCursor(*cursor, current, data.FirstUsn)) {
        saved = *cursor;
        since = cursor->next;
      } else {
        Jmsg(jcr, M_WARNING, 0,
             T_("No valid NTFS checkpoint for %s and previous job %s; "
                "backing up all selected files using MFT discovery.\n"),
             root, jcr->fd_impl->PrevJob);
      }
    }
    Handle directory{CreateFileW(
        enumeration_root.c_str(), FILE_READ_ATTRIBUTES,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
        OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr)};
    BY_HANDLE_FILE_INFORMATION info{};
    if (directory.value == INVALID_HANDLE_VALUE
        || !GetFileInformationByHandle(directory.value, &info)) {
      throw WindowsError("volume root file reference");
    }
    uint64_t root_id
        = (uint64_t{info.nFileIndexHigh} << 32) | info.nFileIndexLow;
    auto map = Enumerate(jcr, enumeration_volume, root_id, since);
    if (saved) {
      ReadChanges(jcr, enumeration_volume, map, *saved, current, data.FirstUsn);
      // Writes to an already-open file can be coalesced without a new USN.
      // Unknown history is not proof that a file was closed.
      map.IncludeUnclosedFiles();
    }

    std::string mount_path = Utf8(mount);
    for (auto& c : mount_path) {
      if (c == '\\') { c = '/'; }
    }
    while (mount_path.back() == '/') { mount_path.pop_back(); }
    children_[Key(mount_path.c_str())];
    std::unordered_map<std::string, std::string> spellings;
    std::unordered_set<std::string> changed_directories;
    for (const auto& [id, record] : map.records) {
      if (jcr->IsJobCanceled()) {
        throw std::runtime_error("job canceled during namespace discovery");
      }
      if (id == root_id) { continue; }
      // Reserved NTFS metadata records are not namespace files.
      if ((id & 0x0000ffffffffffffULL) < 16) { continue; }
      auto relative = map.Path(id);
      if (!relative) { relative = PathById(enumeration_volume, id); }
      // $Extend and System Volume Information hold filesystem/VSS metadata
      // that is neither part of the backup namespace nor openable by name.
      if (relative->starts_with("/$Extend/")
          || relative->starts_with("/System Volume Information")) {
        continue;
      }
      std::string path = mount_path + *relative;
      std::vector<std::wstring> names;
      if (record.directory) {
        names.push_back(Wide(*relative));
      } else {
        try {
          auto native = Wide(relative->substr(1));
          std::replace(native.begin(), native.end(), L'/', L'\\');
          names = Names(enumeration_root + native);
        } catch (const std::runtime_error& error) {
          throw std::runtime_error(std::string(error.what()) + " for "
                                   + *relative);
        }
      }
      for (const auto& name : names) {
        path = mount_path + Utf8(name);
        for (auto& c : path) {
          if (c == '\\') { c = '/'; }
        }
        auto [spelling, inserted] = spellings.emplace(Key(path.c_str()), path);
        if (!inserted && spelling->second != path) {
          throw std::runtime_error("case-sensitive NTFS namespace");
        }
        auto slash = path.find_last_of('/');
        children_[Key(path.substr(0, slash).c_str())].push_back(
            path.substr(slash + 1));
        auto parent = path.substr(0, slash);
        while (parent.size() > mount_path.size()) {
          auto separator = parent.find_last_of('/');
          if (separator == std::string::npos) {
            throw std::runtime_error("invalid volume-relative file name");
          }
          children_[Key(parent.substr(0, separator).c_str())].push_back(
              parent.substr(separator + 1));
          parent.resize(separator);
        }
        if (record.directory) {
          auto key = Key(path.c_str());
          children_[key];
          if (map.Changed(id)) { changed_directories.insert(key); }
        }
        if (saved && !record.directory && !map.Changed(id)) {
          unchanged_.insert(Key(path.c_str()));
        }
        // An alias can have a different parent chain from the MFT's primary
        // name.
        std::erase_if(unchanged_, [&](std::string path) {
          auto slash = path.find_last_of('/');
          while (slash != std::string::npos) {
            path.resize(slash);
            if (changed_directories.contains(path)) { return true; }
            slash = path.find_last_of('/');
          }
          return false;
        });
      }
    }
    auto after = Query(enumeration_volume);
    if (!UsableNtfsCursor(current, {after.UsnJournalID, after.NextUsn},
                          after.FirstUsn)) {
      throw std::runtime_error("journal changed or wrapped during discovery");
    }
    for (auto& [path, names] : children_) {
      std::sort(names.begin(), names.end());
      names.erase(std::unique(names.begin(), names.end()), names.end());
    }
    // The checkpoint precedes live discovery, never its end.
    auto scope = Scope(jcr->Job, volume_name, root, selection);
    checkpoints_[StatePath(scope)] = current;
    checkpoint_scopes_[StatePath(scope)] = scope;
    Jmsg(jcr, M_INFO, 0, T_("Using NTFS %s discovery for %s.\n"),
         saved ? "journal" : "MFT", root);
    return true;
  } catch (const std::runtime_error& error) {
    fallback_reason = error.what();
    children_.clear();
    unchanged_.clear();
    Jmsg(jcr, M_WARNING, 0,
         T_("NTFS discovery for %s unavailable (%s); "
            "using directory traversal.\n"),
         root, error.what());
    return false;
  }
}

const std::vector<std::string>* NtfsFileList::Children(const char* path) const
{
  auto it = children_.find(Key(path));
  return it == children_.end() ? nullptr : &it->second;
}

bool NtfsFileList::Unchanged(JobControlRecord* jcr,
                             FindFilesPacket* ff,
                             const char* path)
{
  if (!unchanged_.contains(Key(path))) { return false; }
  if (jcr->accurate) {
    // A name absent from the catalog must still be backed up.
    if (!jcr->fd_impl->file_list) { return false; }
    auto* payload
        = jcr->fd_impl->file_list->lookup_payload(const_cast<char*>(path));
    if (!payload) { return false; }
    jcr->fd_impl->file_list->MarkFileAsSeen(payload);
  }
  ff->linked = nullptr;
  ff->FileIndex = 0;
  return true;
}

bool NtfsFileList::SaveCheckpoints(JobControlRecord* jcr)
{
  for (const auto& [path, cursor] : checkpoints_) {
    std::ostringstream content;
    WriteNtfsCheckpoint(content, checkpoint_scopes_.at(path), cursor);
    std::string text = content.str();
    std::wstring temporary = path + L".tmp";
    Handle output{CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr,
                              CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr)};
    DWORD written{};
    if (output.value == INVALID_HANDLE_VALUE
        || !WriteFile(output.value, text.data(),
                      static_cast<DWORD>(text.size()), &written, nullptr)
        || written != text.size() || !FlushFileBuffers(output.value)) {
      Jmsg(jcr, M_WARNING, 0, T_("Cannot write NTFS checkpoint: error %lu.\n"),
           GetLastError());
      if (output.value != INVALID_HANDLE_VALUE) {
        CloseHandle(output.value);
        output.value = INVALID_HANDLE_VALUE;
        DeleteFileW(temporary.c_str());
      }
      return false;
    }
    CloseHandle(output.value);
    output.value = INVALID_HANDLE_VALUE;
    if (!MoveFileExW(temporary.c_str(), path.c_str(),
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
      Jmsg(jcr, M_WARNING, 0,
           T_("Cannot publish NTFS checkpoint: error %lu.\n"), GetLastError());
      DeleteFileW(temporary.c_str());
      return false;
    }
  }
  return true;
}

}  // namespace filedaemon
