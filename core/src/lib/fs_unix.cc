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

#include <fcntl.h>
#include <sys/stat.h>
#include "fs.h"


namespace fs {
std::optional<file_attributes> file_attributes::of(native_handle h)
{
  struct stat s;
  if (fstat(h, &s) < 0) { return std::nullopt; }

  return file_attributes{
      .dev = s.st_dev,
      .ino = s.st_ino,
      .mode = static_cast<uint16_t>(s.st_mode),
      .nlink = static_cast<int16_t>(s.st_nlink),
      .uid = s.st_uid,
      .gid = s.st_gid,
      .rdev = s.st_rdev,
      .size = static_cast<uint64_t>(s.st_size),
      .atime = s.st_atime,
      .mtime = s.st_mtime,
      .ctime = s.st_ctime,
      .blksize = static_cast<uint32_t>(s.st_blksize),
      .blocks = static_cast<uint64_t>(s.st_blocks),
  };
}
}  // namespace fs
