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
#include <vector>
#include "fvec.h"
#include "util.h"

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
  std::size_t aligned_size{0};
  std::size_t unaligned_size{0};
};

class config {
 public:
  config(const std::vector<char>& serialized);
  config(std::uint64_t BlockSize)
      : bfile{"blocks", 0, 0, 0}
      , rfile{"records", 0, 0, 0}
      , dfiles{
            {"aligned.data", 0, BlockSize, 0, false},
            {"unaligned.data", 0, 1, 1, false},
        }
  {
  }

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

  block_file& blockfile() { return bfile; }
  record_file& recordfile() { return rfile; }
  std::vector<data_file>& datafiles() { return dfiles; }

  const block_file& blockfile() const { return bfile; }
  const record_file& recordfile() const { return rfile; }
  const std::vector<data_file>& datafiles() const { return dfiles; }

  std::vector<char> serialize();

 private:
  block_file bfile;
  record_file rfile;
  std::vector<data_file> dfiles;
};

class data {
 private:
  raii_fd afd, ufd, rfd, bfd;

 public:
  fvec<char> aligned;
  fvec<char> unaligned;
  fvec<record> records;
  fvec<block> blocks;

  data(open_context ctx, const config& conf);
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
  save_state BeginBlock();
  void CommitBlock(save_state save, block_header header);
  void AbortBlock(save_state save);
  void PushRecord(record_header header, const char* data, std::size_t size);

  // reading interface
  std::size_t ReadBlock(std::size_t blocknum, void* data, std::size_t datasize);

  // misc
  void reset();
  void flush();

 private:
  std::string sys_path;
  int dird;

  std::optional<config> conf;
  std::optional<data> backing;
  void update_config();
};
};  // namespace dedup

#endif  // BAREOS_STORED_BACKENDS_DEDUP_VOLUME_H_
