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
#include <optional>
#include <unordered_map>
#include <map>
#include <utility>
#include <vector>
#include "fvec.h"
#include "util.h"
#include "lib/util.h"

#include "lib/network_order.h"

namespace dedup {
namespace {
using net_u64 = network_order::network<std::uint64_t>;
using net_i64 = network_order::network<std::int64_t>;
using net_u32 = network_order::network<std::uint32_t>;
using net_i32 = network_order::network<std::int32_t>;
using net_u16 = network_order::network<std::uint16_t>;
using net_u8 = std::uint8_t;

struct open_context {
  bool read_only;
  int flags;
  int dird;
};

};  // namespace
struct block {
  net_u32 CheckSum;       /* Block check sum */
  net_u32 BlockSize;      /* Block byte size including the header */
  net_u32 BlockNumber;    /* Block number */
  char ID[4];             /* Identification and block level */
  net_u32 VolSessionId;   /* Session Id for Job */
  net_u32 VolSessionTime; /* Session Time for Job */
  net_u32 Count;          /* number of records inside block */
  net_u64 Begin;          /* start record index of this block */
};

struct record {
  net_i32 FileIndex; /* File index supplied by File daemon */
  net_i32 Stream;    /* Stream number supplied by File daemon */
  net_u32 DataSize;  /* size of following data record in bytes */
  net_u32 Padding;   /* unused */
  net_u32 FileIdx;   /* wich data file has the record */
  net_u32 Size;      /* payload size(*) of record */
  net_u64 Begin;     /* offset into datafile from where to start reading */

  // (*) Size and DataSize can be completely different in cases where the
  //     record is split across.  The first record has DataSize set to the
  //     the combined size, so Size can be much smaller.
  //     Probably redundant when BlockSize is given
};

class volume;

struct save_state {
  std::size_t block_size{0};
  std::size_t record_size{0};
  std::vector<std::size_t> data_sizes;

  save_state() = default;
  save_state(save_state&&) = default;
  save_state& operator=(save_state&&) = default;
  save_state(const save_state&) = delete;
  save_state& operator=(const save_state&) = delete;
};

struct config {
  struct block_file {
    std::string relpath;
    std::uint64_t Start;
    std::uint64_t End;
    std::uint32_t Idx;
  };

  struct record_file {
    std::string relpath;
    std::uint64_t Start;
    std::uint64_t End;
    std::uint32_t Idx;
  };

  struct data_file {
    std::string relpath;
    std::uint64_t Size;
    std::uint64_t BlockSize;
    std::uint32_t Idx;
    bool ReadOnly;
  };

  std::vector<block_file> bfiles;
  std::vector<record_file> rfiles;
  std::vector<data_file> dfiles;

  static std::vector<char> serialize(const config& conf);
  static config deserialize(const char* data, std::size_t size);
  static config make_default(std::uint64_t BlockSize);
};

class data {
 private:
  std::vector<raii_fd> fds;

 public:
  using bsize_map
      = std::map<std::uint64_t, std::uint32_t, std::greater<std::uint64_t>>;

  fvec<record> records;
  fvec<block> blocks;
  std::vector<fvec<char>> datafiles;
  std::unordered_map<std::uint32_t, std::size_t> idx_to_dfile;
  bsize_map bsize_to_idx;

  data(open_context ctx, const config& conf);
};

struct urid  // universial record id
{
  std::uint32_t VolSessionId;   /* Session Id for Job */
  std::uint32_t VolSessionTime; /* Session Time for Job */

  std::int32_t FileIndex; /* File index supplied by File daemon */
  std::int32_t Stream;    /* Stream number supplied by File daemon */

  friend constexpr bool operator==(urid l, urid r)
  {
    return l.VolSessionId == r.VolSessionId
           && l.VolSessionTime == r.VolSessionTime && l.FileIndex == r.FileIndex
           && l.Stream == r.Stream;
  }
};

struct urid_hash {
  std::size_t operator()(urid id) const
  {
    constexpr auto uhash = std::hash<std::uint32_t>{};
    constexpr auto ihash = std::hash<std::int32_t>{};
    std::size_t h0 = hash_combine(0, uhash(id.VolSessionId));
    std::size_t h1 = hash_combine(h0, uhash(id.VolSessionTime));
    std::size_t h2 = hash_combine(h1, ihash(id.FileIndex));
    std::size_t h3 = hash_combine(h2, ihash(id.Stream));

    return h3;
  }
};

class volume {
 public:
  enum open_type
  {
    ReadWrite,
    ReadOnly
  };

  volume(open_type type, const char* path);

  const char* path() const { return sys_path.c_str(); }
  int fileno() const { return dird; }

  static void create_new(int creation_mode,
                         const char* path,
                         std::size_t blocksize);


  // writing interface
  save_state BeginBlock(block_header header);
  void CommitBlock(save_state save);
  void AbortBlock(save_state save);
  void PushRecord(record_header header, const char* data, std::size_t size);

  // reading interface
  std::size_t ReadBlock(std::size_t blocknum, void* data, std::size_t datasize);

  // misc
  void reset();
  void flush();
  void truncate();
  std::size_t blockcount();

 private:
  std::string sys_path;
  int dird;

  std::unordered_map<std::uint32_t, std::string> block_names;
  std::unordered_map<std::uint32_t, std::string> record_names;
  std::unordered_map<std::uint32_t, std::string> data_names;

  std::optional<data> backing;
  void update_config();

  std::optional<block_header> current_block;

  struct record_space {
    std::uint32_t FileIdx; /* in which data file was the space reserved */
    std::uint32_t Size;    /* Size left of reserved space */
    std::uint64_t
        Continue; /* offset into datafile from where to continue writing */
  };

  std::unordered_map<urid, record_space, urid_hash> unfinished;
};
};  // namespace dedup

#endif  // BAREOS_STORED_BACKENDS_DEDUP_VOLUME_H_
