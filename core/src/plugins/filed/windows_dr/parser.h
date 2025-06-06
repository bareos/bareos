/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2025-2025 Bareos GmbH & Co. KG

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

#ifndef BAREOS_PLUGINS_FILED_WINDOWS_DR_PARSER_H_
#define BAREOS_PLUGINS_FILED_WINDOWS_DR_PARSER_H_

#include "file_format.h"

struct disk_info {
  std::uint64_t disk_size;
  std::uint32_t extent_count;
};

class GenericHandler {
 public:
  virtual void BeginRestore(std::size_t num_disks) = 0;
  virtual void EndRestore() = 0;
  virtual void BeginDisk(disk_info info) = 0;
  virtual void EndDisk() = 0;

  virtual void BeginMbrTable(const partition_info_mbr& mbr) = 0;
  virtual void BeginGptTable(const partition_info_gpt& gpt) = 0;
  virtual void BeginRawTable(const partition_info_raw& raw) = 0;
  virtual void MbrEntry(const part_table_entry& entry,
                        const part_table_entry_mbr_data& data)
      = 0;
  virtual void GptEntry(const part_table_entry& entry,
                        const part_table_entry_gpt_data& data)
      = 0;
  virtual void EndPartTable() = 0;

  virtual void BeginExtent(extent_header header) = 0;
  virtual void ExtentData(std::span<const char> data) = 0;
  virtual void EndExtent() = 0;

  virtual ~GenericHandler() {}
};


// this function is basically a simple sax-like parser for the dump format
void parse_file_format(std::istream& stream, GenericHandler* strategy);

#endif  // BAREOS_PLUGINS_FILED_WINDOWS_DR_PARSER_H_
