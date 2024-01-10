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

#ifndef BAREOS_STORED_BACKENDS_DEDUP_VOLUME_H_
#define BAREOS_STORED_BACKENDS_DEDUP_VOLUME_H_

#include <cstdlib>
#include <string>
#include "fvec.h"

namespace dedup {
struct block {};

struct record {};

struct save_state {};

class volume {
 public:
  enum open_type
  {
    ReadWrite,
    ReadOnly
  };

  volume(open_type type,
         int creation_mode,
         const char* path,
         std::size_t blocksize);

  const char* path() const { return sys_path.c_str(); }
  int fileno() const { return dird; }

  save_state begin() { return {}; }
  void abort(save_state) {}
  void commit(save_state) {}

 private:
  std::string sys_path;
  int dird;
  std::size_t blocksize;

  fvec<char> aligned;
  fvec<char> unaligned;
  fvec<record> records;
  fvec<block> blocks;
};
};  // namespace dedup

#endif  // BAREOS_STORED_BACKENDS_DEDUP_VOLUME_H_
