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

#ifndef BAREOS_LIB_FS_H_
#define BAREOS_LIB_FS_H_

#include <cstdint>
#include <ctime>
#include <optional>
#include <windows.h>

namespace fs {

#ifdef HAVE_WIN32
using native_handle = HANDLE;
#else
using native_handle = int;
#endif

struct file_attributes {
  std::uint64_t dev;
  std::uint64_t ino;
  std::uint16_t mode;
  std::int16_t nlink;
  std::uint32_t uid;
  std::uint32_t gid;
  std::uint64_t rdev;
  std::uint64_t size;
  std::time_t atime;
  std::time_t mtime;
  std::time_t ctime;
  std::uint32_t blksize;
  std::uint64_t blocks;

  static std::optional<file_attributes> of(native_handle h);
};
};  // namespace fs

#endif  // BAREOS_LIB_FS_H_
