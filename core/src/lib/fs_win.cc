/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2024-2024 Bareos GmbH & Co. KG

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

#include "fs.h"
#include <windows.h>
#include <algorithm>


namespace fs {

namespace {
uint64_t combine(LARGE_INTEGER i)
{
  return static_cast<uint64_t>(i.HighPart) << 32
         | static_cast<uint64_t>(i.LowPart);
}

time_t FromWindowsTime(LARGE_INTEGER time)
{
  (void)time;
  return 0;
}
};  // namespace


std::optional<file_attributes> file_attributes::of(native_handle h)
{
  FILE_BASIC_INFO basic_info{};
  FILE_STANDARD_INFO standard_info{};
  FILE_ATTRIBUTE_TAG_INFO attribute_info{};
  // FILE_ID_INFO id_info{};

  BOOL success
      = GetFileInformationByHandleEx(h, FileBasicInfo, &basic_info,
                                     sizeof(basic_info))
        && GetFileInformationByHandleEx(h, FileStandardInfo, &standard_info,
                                        sizeof(standard_info))
        && GetFileInformationByHandleEx(
            h, FileAttributeTagInfo, &attribute_info, sizeof(attribute_info));
  // && GetFileInformationByHandleEx(h, FileIdInfo, &id_info,
  //                                 sizeof(id_info));

  if (!success) { return std::nullopt; }

  uint16_t mode = 0777;

  auto size = combine(standard_info.EndOfFile);

  uint64_t special = 0;

  return file_attributes{
      // .dev = id_info.VolumeSerialNumber,
      // .ino = id_info.FileId,
      .dev = 0,
      .ino = 0,
      .mode = mode,
      .nlink = static_cast<int16_t>(standard_info.NumberOfLinks),
      .uid = 0,
      .gid = 0,
      .rdev = special,
      .size = size,
      .atime = FromWindowsTime(basic_info.LastAccessTime),
      .mtime = FromWindowsTime(basic_info.LastWriteTime),
      .ctime = std::max(FromWindowsTime(basic_info.CreationTime),
                        FromWindowsTime(basic_info.ChangeTime)),
      .blksize = 4096,
      .blocks = (size + 4095) / 4096,
  };
}
}  // namespace fs
