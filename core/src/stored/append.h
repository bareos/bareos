/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2018-2026 Bareos GmbH & Co. KG

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
#ifndef BAREOS_STORED_APPEND_H_
#define BAREOS_STORED_APPEND_H_


#include "include/bareos.h"
#include "lib/bsock.h"
#include "stored/device_control_record.h"
#include "stored/record.h"

namespace storagedaemon {

class ProcessedFileData {
 public:
  explicit ProcessedFileData(DeviceRecord* record);
  DeviceRecord GetData();
  int32_t GetStream() const { return stream_; }

 private:
  uint32_t volsessionid_{0};
  uint32_t volsessiontime_{0};
  int32_t fileindex_{0};
  int32_t stream_{0};
  uint32_t data_len_{0};
  std::string data_{};
};

class ProcessedFile {
 public:
  ProcessedFile() = default;
  explicit ProcessedFile(int32_t fileindex);

  void SendAttributesToDirector(JobControlRecord* jcr);
  void AddAttribute(DeviceRecord* record);
  int32_t GetFileIndex() { return fileindex_; }
  const std::vector<ProcessedFileData>& GetAttributes() const
  {
    return attributes_;
  }

 private:
  int32_t fileindex_{-1};
  std::vector<ProcessedFileData> attributes_;
};

bool DoAppendData(JobControlRecord* jcr, BareosSocket* bs, const char* what);
bool IsAttribute(DeviceRecord* record);
bool SendAttrsToDir(JobControlRecord* jcr, DeviceRecord* rec);

/**
 * True if stream (raw, i.e. as in DeviceRecord::Stream/
 * ProcessedFileData::GetStream(), not yet masked) is a
 * STREAM_UNIX_ATTRIBUTES or STREAM_UNIX_ATTRIBUTES_EX record -- the
 * record type that carries the on-disk stat() information (including
 * st_size/st_blocks) for a file.
 */
bool IsUnixAttributeStream(int32_t stream);

/**
 * Given all the buffered records for one file, return which indices
 * should actually be forwarded to the Director/catalog: every
 * non-STREAM_UNIX_ATTRIBUTES(_EX) record (digests, restore objects),
 * plus only the *last* STREAM_UNIX_ATTRIBUTES(_EX) record (in case
 * more than one was buffered, e.g. because a plugin resent corrected
 * attributes for the same file). Exposed separately from
 * ProcessedFile::SendAttributesToDirector() so this selection logic
 * can be unit-tested without needing a live Director connection.
 */
std::vector<bool> SelectAttributesToSend(
    const std::vector<ProcessedFileData>& attributes);
}  // namespace storagedaemon

#endif  // BAREOS_STORED_APPEND_H_
