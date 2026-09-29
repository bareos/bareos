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
/**
 * @file
 * Per-FileIndex attribute-resend de-duplication, shared by the live
 * Storage daemon job path (append.cc), Copy/Migrate/Virtual Full
 * (mac.cc) and the medium-reading tools (bscan, bls, bextract).
 */

#include "stored/attribute_dedup.h"
#include "include/streams.h"
#include "lib/crypto.h"

namespace storagedaemon {

ProcessedFileData::ProcessedFileData(DeviceRecord* record)
    : volsessionid_(record->VolSessionId)
    , volsessiontime_(record->VolSessionTime)
    , fileindex_(record->FileIndex)
    , stream_(record->Stream)
    , data_len_(record->data_len)
    , data_(record->data, record->data + record->data_len)
{
}

DeviceRecord ProcessedFileData::GetData() const
{
  DeviceRecord devicerecord{};
  devicerecord.VolSessionId = volsessionid_;
  devicerecord.VolSessionTime = volsessiontime_;
  devicerecord.FileIndex = fileindex_;
  devicerecord.Stream = stream_;
  devicerecord.data_len = data_len_;
  devicerecord.data = const_cast<char*>(data_.data());

  return devicerecord;
}

ProcessedFile::ProcessedFile(int32_t fileindex) : fileindex_(fileindex) {}

void ProcessedFile::AddAttribute(DeviceRecord* record)
{
  attributes_.emplace_back(ProcessedFileData(record));
}

bool IsAttribute(DeviceRecord* record)
{
  return record->maskedStream == STREAM_UNIX_ATTRIBUTES
         || record->maskedStream == STREAM_UNIX_ATTRIBUTES_EX
         || record->maskedStream == STREAM_RESTORE_OBJECT
         || CryptoDigestStreamType(record->maskedStream) != CRYPTO_DIGEST_NONE;
}

bool IsUnixAttributeStream(int32_t stream)
{
  int32_t masked_stream = stream & STREAMMASK_TYPE;
  return masked_stream == STREAM_UNIX_ATTRIBUTES
         || masked_stream == STREAM_UNIX_ATTRIBUTES_EX;
}

std::vector<std::size_t> SelectAttributesToSend(
    const std::vector<ProcessedFileData>& attributes)
{
  /* A file can have more than one STREAM_UNIX_ATTRIBUTES/_EX record
   * buffered here if something resent corrected attributes for it
   * (e.g. a backup plugin that only learns the real st_size/st_blocks
   * once it finishes writing, see fd_plugins.cc/bVarFileSizeBlocks).
   * The medium already holds every one of those records -- that part
   * is unaffected -- but only the last (i.e. most up to date) one
   * should ever reach the Director/catalog, so File.LStat isn't
   * duplicated for the same FileIndex. Every other buffered record
   * (digests, restore objects) is unaffected and always selected.
   *
   * The last attribute record is emitted in the slot of the first
   * attribute record (see the header for why the order matters). */
  std::size_t first_unix_attribute_idx = attributes.size();
  std::size_t last_unix_attribute_idx = attributes.size();
  for (std::size_t i = 0; i < attributes.size(); i++) {
    if (IsUnixAttributeStream(attributes[i].GetStream())) {
      if (first_unix_attribute_idx == attributes.size()) {
        first_unix_attribute_idx = i;
      }
      last_unix_attribute_idx = i;
    }
  }

  std::vector<std::size_t> to_send;
  to_send.reserve(attributes.size());
  for (std::size_t i = 0; i < attributes.size(); i++) {
    if (IsUnixAttributeStream(attributes[i].GetStream())) {
      if (i == first_unix_attribute_idx) {
        /* Emit the last (most up to date) attribute record here, in
         * the first attribute's slot, so any following digest record
         * still arrives after the file row is created. */
        to_send.push_back(last_unix_attribute_idx);
      }
      /* Superseded or already emitted attribute records are skipped. */
      continue;
    }
    to_send.push_back(i);
  }
  return to_send;
}

}  // namespace storagedaemon
