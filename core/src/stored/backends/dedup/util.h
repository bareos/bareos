/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2023-2024 Bareos GmbH & Co. KG

   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation, which is
   listed in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   Affero General Public License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.
*/

#ifndef BAREOS_STORED_BACKENDS_DEDUP_UTIL_H_
#define BAREOS_STORED_BACKENDS_DEDUP_UTIL_H_

#include <cstdint>
#include "lib/network_order.h"
#include "include/bareos.h"

namespace dedup {

using net_u64 = network_order::network<std::uint64_t>;
using net_i64 = network_order::network<std::int64_t>;
using net_u32 = network_order::network<std::uint32_t>;
using net_i32 = network_order::network<std::int32_t>;
using net_u16 = network_order::network<std::uint16_t>;
using net_u8 = std::uint8_t;

class chunked_reader {
 public:
  chunked_reader(const void* data, std::size_t size)
      : begin{(const char*)data}, end{begin + size}
  {
  }

  const char* get(std::size_t size)
  {
    ASSERT(begin <= end);
    if (static_cast<std::size_t>(end - begin) < size) { return nullptr; }

    begin += size;
    return begin - size;
  }

  bool read(void* mem, std::size_t size)
  {
    ASSERT(begin <= end);
    if (static_cast<std::size_t>(end - begin) < size) { return false; }

    std::memcpy(mem, begin, size);
    begin += size;

    return true;
  }

  bool finished() const { return begin == end; }
  std::size_t leftover() const { return end - begin; }

 private:
  const char* begin;
  const char* end;
};

struct block_header {
  net_u32 CheckSum;       /* Block check sum */
  net_u32 BlockSize;      /* Block byte size including the header */
  net_u32 BlockNumber;    /* Block number */
  char ID[4];             /* Identification and block level */
  net_u32 VolSessionId;   /* Session Id for Job */
  net_u32 VolSessionTime; /* Session Time for Job */

  // actual payload size
  std::size_t size() const { return BlockSize.load() - sizeof(block_header); }
};

struct record_header {
  net_i32 FileIndex; /* File index supplied by File daemon */
  net_i32 Stream;    /* Stream number supplied by File daemon */
  net_u32 DataSize;  /* size of following data record in bytes */

  // actual payload size
  std::size_t size() const { return DataSize.load(); }
};

struct raii_fd {
  raii_fd() = default;
  raii_fd(int fd) : fd{fd} {}
  raii_fd(const raii_fd&) = delete;
  raii_fd& operator=(const raii_fd&) = delete;
  raii_fd(raii_fd&& other) : raii_fd{} { *this = std::move(other); }
  raii_fd& operator=(raii_fd&& other)
  {
    std::swap(fd, other.fd);
    return *this;
  }

  int fileno() { return fd; }

  int release()
  {
    auto old = fd;
    fd = -1;
    return old;
  }

  operator bool() const { return fd >= 0; }

  ~raii_fd()
  {
    if (fd >= 0) { close(fd); }
  }

  int fd{-1};
};

};  // namespace dedup

#endif  // BAREOS_STORED_BACKENDS_DEDUP_UTIL_H_
