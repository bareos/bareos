/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2023-2024 Bareos GmbH & Co. KG

   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation and included
   in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   General Public License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.
*/

extern "C" {
#include <unistd.h>
#include <fcntl.h>
}

#include "volume.h"

namespace dedup {
namespace {

};

volume::volume(open_type type,
               int creation_mode,
               const char* path,
               std::size_t blocksize)
    : sys_path{path}, blocksize{blocksize}
{
  int dir_mode
      = creation_mode | S_IXUSR;  // directories need execute permissions

  int flags = 0;
  bool read_only = type == open_type::ReadOnly;
  if (read_only) {
    flags |= O_RDONLY;
  } else {
    flags |= O_RDWR | O_CREAT;
  }

  int dir_flags = flags | O_DIRECTORY;
  dird = open(path, dir_flags, dir_mode);

  if (dird < 0) { throw std::invalid_argument("dird < 0"); }

  int aligned_fd = openat(dird, "aligned.data", flags, creation_mode);
  int unaligned_fd = openat(dird, "unaligned.data", flags, creation_mode);
  int records_fd = openat(dird, "records.data", flags, creation_mode);
  int blocks_fd = openat(dird, "blocks.data", flags, creation_mode);

  if (aligned_fd < 0 || unaligned_fd < 0 || records_fd < 0 || blocks_fd < 0) {
    throw std::invalid_argument("fd < 0");
  }

  aligned = decltype(aligned){read_only, aligned_fd};
  unaligned = decltype(unaligned){read_only, unaligned_fd};
  records = decltype(records){read_only, records_fd};
  blocks = decltype(blocks){read_only, blocks_fd};
}


};  // namespace dedup
