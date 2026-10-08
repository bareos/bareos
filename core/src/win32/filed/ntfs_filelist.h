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
#ifndef BAREOS_WIN32_FILED_NTFS_FILELIST_H_
#define BAREOS_WIN32_FILED_NTFS_FILELIST_H_

#include "findlib/filelist.h"

#include <cstdint>
#include <optional>
#include <iomanip>
#include <istream>
#include <ostream>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

namespace filedaemon {

struct NtfsCursor {
  uint64_t journal{};
  int64_t next{};
};

struct NtfsCheckpoint {
  NtfsCursor cursor;
  std::unordered_set<uint64_t> uncertain;
};

inline bool IsNtfsPathWithinRoot(std::string_view path_key,
                                 std::string_view root_key)
{
  return root_key.empty() || path_key == root_key
         || (path_key.size() > root_key.size() && path_key.starts_with(root_key)
             && path_key[root_key.size()] == '/');
}

constexpr bool UsableNtfsCursor(NtfsCursor saved,
                                NtfsCursor current,
                                int64_t first)
{
  return saved.journal == current.journal && saved.next >= first
         && saved.next <= current.next;
}

inline std::optional<NtfsCheckpoint> ReadNtfsCheckpoint(
    std::istream& input,
    const std::string& scope)
{
  std::string identity;
  NtfsCheckpoint checkpoint;
  uint64_t count{};
  if (!(input >> std::quoted(identity) >> checkpoint.cursor.journal
        >> checkpoint.cursor.next >> count)
      || identity != scope || checkpoint.cursor.next < 0) {
    return std::nullopt;
  }
  for (uint64_t i = 0; i < count; ++i) {
    uint64_t reference{};
    if (!(input >> reference)) { return std::nullopt; }
    checkpoint.uncertain.insert(reference);
  }
  input >> std::ws;
  if (!input.eof()) { return std::nullopt; }
  return checkpoint;
}

inline void WriteNtfsCheckpoint(std::ostream& output,
                                const std::string& scope,
                                const NtfsCheckpoint& checkpoint)
{
  output << std::quoted(scope) << '\n'
         << checkpoint.cursor.journal << ' ' << checkpoint.cursor.next << '\n'
         << checkpoint.uncertain.size();
  for (uint64_t reference : checkpoint.uncertain) {
    output << ' ' << reference;
  }
  output << '\n';
}

struct NtfsRecord {
  uint64_t parent{};
  std::string name;
  bool directory{};
  int64_t usn{};
};

// The graph retains file-reference sequence numbers, not just MFT indices.
class NtfsReferenceMap {
 public:
  std::unordered_map<uint64_t, NtfsRecord> records;
  std::unordered_set<uint64_t> changed;
  std::unordered_set<uint64_t> closed;
  uint64_t root{};

  void Observe(uint64_t id, int64_t usn, bool final_close, int64_t since)
  {
    if (usn >= since) { changed.insert(id); }
    if (usn >= since) {
      if (final_close) {
        closed.insert(id);
      } else {
        closed.erase(id);
      }
    }
  }

  std::unordered_set<uint64_t> IncludeUnclosedFiles(
      int64_t since,
      const std::unordered_set<uint64_t>& previously_uncertain)
  {
    std::unordered_set<uint64_t> uncertain;
    for (const auto& [id, record] : records) {
      if (record.directory || closed.contains(id)
          || (record.usn < since && !previously_uncertain.contains(id))) {
        continue;
      }
      changed.insert(id);
      uncertain.insert(id);
    }
    return uncertain;
  }

  std::optional<std::string> Path(uint64_t id) const
  {
    std::string path;
    std::unordered_set<uint64_t> visited;
    while (id != root) {
      if (!visited.insert(id).second) { return std::nullopt; }
      auto it = records.find(id);
      if (it == records.end()) { return std::nullopt; }
      if (it->second.name.empty()) { return std::nullopt; }
      if (it->second.name == "." || it->second.name == ".."
          || it->second.name.find_first_of("/\\") != std::string::npos) {
        return std::nullopt;
      }
      path = "/" + it->second.name + path;
      id = it->second.parent;
    }
    return path;
  }

  bool Changed(uint64_t id) const
  {
    std::unordered_set<uint64_t> visited;
    while (visited.insert(id).second) {
      if (changed.contains(id)) { return true; }
      if (id == root) { return false; }
      auto it = records.find(id);
      if (it == records.end()) { return true; }
      id = it->second.parent;
    }
    return true;
  }
};

#ifdef HAVE_WIN32
class NtfsFileList : public FileList {
 public:
  std::string fallback_reason;
  bool Prepare(JobControlRecord*, FindFilesPacket*, const char*) override;
  const std::vector<std::string>* Children(const char*) const override;
  bool Unchanged(JobControlRecord*, FindFilesPacket*, const char*) override;
  bool SaveCheckpoints(JobControlRecord*);

 private:
  std::unordered_map<std::string, std::vector<std::string>> children_;
  std::unordered_set<std::string> unchanged_;
  std::unordered_map<std::wstring, NtfsCheckpoint> checkpoints_;
  std::unordered_map<std::wstring, std::string> checkpoint_scopes_;
};
#endif

}  // namespace filedaemon
#endif  // BAREOS_WIN32_FILED_NTFS_FILELIST_H_
