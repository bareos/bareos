/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2025-2025 Bareos GmbH & Co. KG

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

#include <assert.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string_view>
#include <string>
#include <type_traits>
#include <vector>
#include <memory>
#include <chrono>

#include "lib/network_order.h"
#include "stored/crc32/crc32.h"

std::size_t operator""_k(unsigned long long val) { return val * 1024; }

std::size_t operator""_m(unsigned long long val) { return val * 1024_k; }

std::size_t operator""_g(unsigned long long val) { return val * 1024_m; }

static constexpr std::uint32_t BAREOS_TAPE_VERSION = 20;

using block = std::vector<char>;

struct volume {
  std::string name{};
  std::size_t current_block_idx{};
  std::size_t block_size{1_m};
  std::vector<block> blocks{};

  volume(std::string_view name_) : name(name_) {}
};

enum class job_type : std::int32_t
{
  Unset = 0,
  Backup = 'B',      /**< Backup Job */
  MigratedJob = 'M', /**< A previous backup job that was migrated */
  Verify = 'V',      /**< Verify Job */
  Restore = 'R',     /**< Restore Job */
  Console = 'U',     /**< console program */
  System = 'I',      /**< internal system "job" */
  Admin = 'D',       /**< admin job */
  Archive = 'A',     /**< Archive Job */
  JobCopy = 'C',     /**< Copy of a Job */
  Copy = 'c',        /**< Copy Job */
  Migrate = 'g',     /**< Migration Job */
  Scan = 'S',        /**< Scan Job */
  Consolidate = 'O'  /**< Always Incremental Consolidate Job */
};

enum class job_level : int32_t
{
  Unset = 0,
  Full = 'F',         /**< Full backup */
  Incremental = 'I',  /**< Since last backup */
  Differential = 'D', /**< Since last full backup */
  Since = 'S',
  VerifyCatalog = 'C',         /**< Verify from catalog */
  VerifyInit = 'V',            /**< Verify save (init DB) */
  VerifyVolumeToCatalog = 'O', /**< Verify Volume to catalog entries */
  VerifyDiskToCatalog = 'd',   /**< Verify Disk attributes to catalog */
  VerifyData = 'A',            /**< Verify data on volume */
  Base = 'B',                  /**< Base level job */
  None = ' ',                  /**< None, for Restore and Admin */
  VirtualFull = 'f'            /**< Virtual full backup */
};

struct job {
  std::string name{};

  std::size_t vol_session_time{};
  std::size_t vol_session_id{};

  std::size_t current_file_idx{};

  enum class file_state
  {
    ATTRIBUTES,
    DATA,
    // not necessary for now:
    // DIGEST,
  };

  int fd{-1};

  file_state current_state;

  std::size_t current_record_size{128_k};

  job_level level;
  job_type type;

  job(std::string_view name_) : name(name_) {}
};

struct context {
  std::vector<std::unique_ptr<volume>> volumes;
  std::vector<std::unique_ptr<job>> jobs;

  std::size_t next_id;

  std::string default_pool_name;
  std::string default_pool_type;
  std::string default_media_type;

  std::string default_fileset_name;
  std::string default_fileset_md5;

  std::string host_name;
  std::string prog_name;
  std::string prog_version;
  std::string prog_date;
};

void SetError(context*, const char*, ...) {}

job* DeclareJob(context* ctx, std::string_view name)
{
  return ctx->jobs.emplace_back(std::make_unique<job>(name)).get();
}

volume* DeclareVolume(context* ctx, std::string_view name)
{
  return ctx->volumes.emplace_back(std::make_unique<volume>(name)).get();
}

struct record_writer {
  char* begin;

  std::size_t current_size{0};
  std::size_t written_bytes{0};
  std::size_t max_bytes;

  char* size_ptr;

  record_writer(char* data, std::size_t size) noexcept
      : begin{data}, max_bytes{size}
  {
  }

  std::size_t byte_size() const noexcept { return written_bytes; }
  std::size_t record_size() const noexcept { return current_size; }

  void set_size_marker()
  {
    // todo: we have to handle the oom condition somehow
    assert(!size_ptr);
    size_ptr = begin + written_bytes;
  }
};


void WriteBytes(record_writer& writer, std::string_view value)
{
  auto bytes_to_write = value.size() + 1;
  auto bytes_free = writer.max_bytes - writer.written_bytes;

  writer.written_bytes += bytes_to_write;
  writer.current_size += bytes_to_write;

  if (bytes_free < bytes_to_write) { return; }

  auto* ptr = writer.begin + writer.max_bytes - bytes_free;
  std::memcpy(ptr, value.data(), value.size());
  ptr[value.size()] = 0;
}
void WriteBytes(record_writer& writer, const char* data, std::size_t size)
{
  auto bytes_free = writer.max_bytes - writer.written_bytes;

  writer.written_bytes += size;
  writer.current_size += size;

  if (bytes_free < size) { return; }

  auto* ptr = writer.begin + writer.max_bytes - bytes_free;
  std::memcpy(ptr, data, size);
}

void PlaceholderBytes(record_writer& writer, std::size_t size)
{
  writer.written_bytes += size;
  writer.current_size += size;
}

void PhantomBytes(record_writer& writer, std::size_t size)
{
  writer.current_size += size;
}

template <typename T> void WriteNetwork(record_writer& writer, T val)
{
  network_order::network<T> net{val};
  auto bytes = net.stored_bytes();

  WriteBytes(writer, bytes.data(), bytes.size());
}

template <> void WriteNetwork<double>(record_writer& writer, double val)
{
  std::uint64_t as_unsigned;
  static_assert(sizeof(as_unsigned) == sizeof(val));

  std::memcpy(&as_unsigned, &val, sizeof(val));

  WriteNetwork(writer, as_unsigned);
}

void BeginRecord(record_writer& writer,
                 std::int32_t stream,
                 std::int32_t file_idx)
{
  WriteNetwork(writer, file_idx);
  WriteNetwork(writer, stream);

  writer.set_size_marker();
  PlaceholderBytes(writer, sizeof(std::uint32_t));
}

bool EndRecord(record_writer& writer)
{
  assert(writer.size_ptr);

  network_order::network<std::uint32_t> net{
      static_cast<std::uint32_t>(writer.current_size)};

  auto bytes = net.stored_bytes();

  std::memcpy(writer.size_ptr, bytes.data(), bytes.size());

  writer.size_ptr = nullptr;

  writer.current_size = 0;

  return writer.written_bytes <= writer.max_bytes;
}

struct volume_label {
  static constexpr std::int32_t Pre = -1;
  static constexpr std::int32_t Volume = -2;
  static constexpr std::int32_t EndOfMemory = -3;
  static constexpr std::int32_t StartOfSession = -4;
  static constexpr std::int32_t EndOfSession = -5;
  static constexpr std::int32_t EndOfTape = -6;
  static constexpr std::int32_t StartOfObjct = -7;
  static constexpr std::int32_t EndOfObjct = -8;
};

template <typename Enum> std::underlying_type_t<Enum> to_underlying(Enum e)
{
  return static_cast<std::underlying_type_t<Enum>>(e);
}

using net_u32 = network_order::network<std::uint32_t>;

struct block_header {
  net_u32 CheckSum;       /* Block check sum */
  net_u32 BlockSize;      /* Block byte size including the header */
  net_u32 BlockNumber;    /* Block number */
  char ID[4];             /* Identification and block level */
  net_u32 VolSessionId;   /* Session Id for Job */
  net_u32 VolSessionTime; /* Session Time for Job */
};

bool WriteBlockHeader(block& b, job* j)
{
  block_header hdr = {
      .CheckSum = 0,
      .BlockSize = b.size(),
      .BlockNumber = 0,
      .ID = {'B', 'B', '0', '2'},
      .VolSessionId = j->vol_session_id,
      .VolSessionTime = j->vol_session_time,
  };

  if (b.size() < sizeof(hdr)) { return false; }

  std::memcpy(b.data(), &hdr, sizeof(hdr));

  auto checksum = crc32_fast(b.data() + sizeof(hdr.CheckSum),
                             b.size() - sizeof(hdr.CheckSum));

  hdr.CheckSum = checksum;

  std::memcpy(b.data(), &hdr, sizeof(hdr));

  return true;
}

enum class Stream : std::int32_t
{
  UnixAttribs = 1,
  FileData = 2,
};

record_writer BeginBlock(block& b)
{
  record_writer writer{b.data(), b.size()};
  PlaceholderBytes(writer, sizeof(block_header));
  return writer;
}

bool EndBlock(block& b, job* j, record_writer& writer)
{
  if (!WriteBlockHeader(b, j)) { return false; }

  b.resize(writer.written_bytes);
  b.shrink_to_fit();

  return true;
}

block NextBlock(context* ctx, job* job, bool split, std::size_t size)
{
  (void)split;

  block data;
  data.resize(size);

  auto writer = BeginBlock(data);

  // first we need to write the block header

  bool done = false;

  while (!done && writer.byte_size() < size) {
    if (job->current_file_idx == 0) {
      // we need to first generate start of session record

      if (!job->vol_session_id) { job->vol_session_id = ++ctx->next_id; }
      if (!job->vol_session_time) {
        job->vol_session_time
            = std::chrono::steady_clock::now().time_since_epoch().count();
      }

      if (job->type == job_type::Unset) { job->type = job_type::Backup; }

      if (job->level == job_level::Unset) { job->level = job_level::Full; }

      BeginRecord(writer, job->vol_session_id, volume_label::StartOfSession);

      WriteBytes(writer, "Bareos 2.0 immortal\n");
      WriteNetwork(writer, BAREOS_TAPE_VERSION);
      WriteNetwork<std::uint32_t>(writer, job->vol_session_id);

      WriteBytes(writer, ctx->default_pool_name);
      WriteBytes(writer, ctx->default_pool_type);

      WriteNetwork<int64_t>(writer, job->vol_session_time);
      WriteNetwork<double>(writer, 0);

      WriteNetwork(writer, job->vol_session_id);
      WriteNetwork(writer, job->vol_session_time);

      WriteBytes(writer, job->name);
      WriteBytes(writer, ctx->default_fileset_name);

      WriteNetwork(writer, to_underlying(job->type));
      WriteNetwork(writer, to_underlying(job->level));

      WriteBytes(writer, ctx->default_fileset_md5);

      BeginRecord(writer, job->vol_session_id, volume_label::StartOfSession);
      if (!EndRecord(writer)) {
        SetError(ctx, "{} byte block is too small for sos header!", size);
        return {};
      }

      job->current_file_idx += 1;
    } else {
      // we need to write a record header

      switch (job->current_state) {
        case job::file_state::ATTRIBUTES: {
          if (job->fd >= 0) { close(job->fd); }

          job->fd = open("/tmp", O_TMPFILE | O_RDWR, 0664);

          assert(job->fd >= 0);

          struct stat s;
          assert(fstat(job->fd, &s) >= 0);

          BeginRecord(writer, to_underlying(Stream::UnixAttribs),
                      job->current_file_idx);

          // ("%" PRIu32 " %d %s%c%s%c%c%s%c%d%c", jcr->JobFiles,
          //  ff_pkt->type, ff_pkt->fname, 0, attribs.c_str(), 0, 0,
          //  attribsEx, 0, ff_pkt->delta_seq, 0);

          char encoded[1000];
          EncodeStats(encoded, &s, sizeof(s), 0,
                      to_underlying(Stream::FileData));

          WriteBytes(writer, reinterpret_cast<char*>(&s), sizeof(s));

          if (!EndRecord(writer)) {
            done = true;
            break;
          }

          job->current_state = job::file_state::DATA;
        } break;
        case job::file_state::DATA: {
        } break;
      }

      break;
    }
  }

  if (!EndBlock(data, job, writer)) { return {}; }
  return data;
}

void InsertVolumeLabel(context* ctx, volume* vol, job* j)
{
  block b;
  b.resize(400);

  auto writer = BeginBlock(b);

  BeginRecord(writer, 1, volume_label::Volume);

  WriteBytes(writer, "Bareos 2.0 immortal\n");
  WriteNetwork(writer, BAREOS_TAPE_VERSION);
  WriteNetwork(writer, /* btime */ 0);
  WriteNetwork<double>(writer, 0.0);
  WriteNetwork<double>(writer, 0.0);

  WriteBytes(writer, vol->name);
  WriteBytes(writer, "prev volume");
  WriteBytes(writer, ctx->default_pool_name);
  WriteBytes(writer, ctx->default_pool_type);
  WriteBytes(writer, ctx->default_media_type);

  WriteBytes(writer, ctx->host_name);
  WriteBytes(writer, ctx->prog_name);
  WriteBytes(writer, ctx->prog_version);
  WriteBytes(writer, ctx->prog_date);

  if (!EndRecord(writer)) { return; }

  if (!EndBlock(b, j, writer)) { return; }

  vol->blocks.emplace_back(std::move(b));
}

void InsertBlock(context* ctx, volume* vol, job* job)
{
  if (vol->blocks.empty()) { InsertVolumeLabel(ctx, vol, job); }
  vol->blocks.emplace_back(NextBlock(ctx, job, false, vol->block_size));
}

void InsertSplitBlock(context* ctx, volume* vol, job* job)
{
  if (vol->blocks.empty()) { InsertVolumeLabel(ctx, vol, job); }
  vol->blocks.emplace_back(NextBlock(ctx, job, true, vol->block_size));
}

void InsertSmallBlock(context* ctx, volume* vol, job* job, std::size_t size)
{
  if (vol->blocks.empty()) { InsertVolumeLabel(ctx, vol, job); }
  if (vol->block_size <= size) {
    SetError(ctx, "can not add small block of size {}, when block size is {}",
             size, vol->block_size);
  } else {
    vol->blocks.emplace_back(NextBlock(ctx, job, true, size));
  }
}

void FlushJob(context*, volume*, job*) {}

int main()
{
  context ctx[1] = {};

  auto* job1 = DeclareJob(ctx, "Full");
  auto* job2 = DeclareJob(ctx, "Incr");

  auto* vol1 = DeclareVolume(ctx, "Full-0001");
  auto* vol2 = DeclareVolume(ctx, "Full-0002");

  InsertBlock(ctx, vol1, job1);
  InsertSplitBlock(ctx, vol1, job1);
  InsertBlock(ctx, vol1, job1);
  InsertBlock(ctx, vol1, job1);
  InsertSplitBlock(ctx, vol1, job1);
  InsertSplitBlock(ctx, vol1, job2);
  InsertBlock(ctx, vol1, job2);
  InsertBlock(ctx, vol1, job2);
  InsertBlock(ctx, vol1, job2);
  InsertBlock(ctx, vol1, job2);
  InsertSplitBlock(ctx, vol2, job1);
  InsertSplitBlock(ctx, vol2, job1);
  InsertSplitBlock(ctx, vol2, job1);
  InsertSplitBlock(ctx, vol2, job1);
  InsertSplitBlock(ctx, vol2, job1);
  InsertSplitBlock(ctx, vol2, job1);
  InsertSmallBlock(ctx, vol1, job2, 64_k);
  FlushJob(ctx, vol2, job1);
  FlushJob(ctx, vol2, job2);
}
