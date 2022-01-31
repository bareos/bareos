/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2022-2022 Bareos GmbH & Co. KG

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

#ifndef BAREOS_LIB_MONOTONIC_BUFFER_H_
#define BAREOS_LIB_MONOTONIC_BUFFER_H_
#include <cstddef>

class MonotonicBuffer {
 public:
  enum class Size
  {
    Small,
    Medium,
    Large
  };

 private:
  struct Allocation;
  Allocation* mem_block_{nullptr};
  Size grow_size_;

 public:
  MonotonicBuffer(Size grow_size = Size::Large) : grow_size_(grow_size) {}
  ~MonotonicBuffer();
  void* allocate(size_t size);
};
#endif  // BAREOS_LIB_MONOTONIC_BUFFER_H_
