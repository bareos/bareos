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
#include <fmt/format.h>
#include <span>

#include "CLI/CLI.hpp"
#include "include/job_status.h"
#include "lib/network_order.h"
#include "lib/attribs.h"
#include "stored/crc32/crc32.h"
#include "include/filetypes.h"

using rand_type = std::uint64_t;

rand_type next_random(rand_type& state)
{
  // xorshift 64
  rand_type val = state;
  val ^= val << 13;
  val ^= val >> 7;
  val ^= val << 17;
  state = val;
  return val;
}

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

struct file_descriptor {
  int fd{-1};

  file_descriptor() = default;
  explicit file_descriptor(int fd_) : fd{fd_} {}

  file_descriptor(const file_descriptor& other) = delete;
  file_descriptor& operator=(const file_descriptor& other) = delete;

  file_descriptor(file_descriptor&& other) { std::swap(fd, other.fd); }

  file_descriptor& operator=(file_descriptor&& other)
  {
    std::swap(fd, other.fd);
    return *this;
  }

  operator int() const { return fd; }
  operator bool() const { return fd >= 0; }

  ~file_descriptor()
  {
    if (fd >= 0) { close(fd); }
  }
};

struct file {
  std::string name;
  file_descriptor fd;
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

  file f;

  file_state current_state;

  std::size_t current_record_size{128_k};

  std::vector<char> scratch;
  // there are no leftovers, if leftover_offset >= scratch.size()
  std::size_t leftover_offset;

  std::span<char> leftovers()
  {
    if (leftover_offset >= scratch.size()) {
      return {};
    } else {
      return {scratch.data() + leftover_offset,
              scratch.size() - leftover_offset};
    }
  }

  job_level level;
  job_type type;

  job(std::string_view name_) : name(name_) {}


  struct {
    std::uint32_t errors;
    std::uint32_t status;

    // std::uint32_t jobfiles;
    std::uint64_t jobbytes;

    std::uint32_t start_file;
    std::uint32_t start_block;
    std::uint32_t end_file;
    std::uint32_t end_block;
  } statistics = {};

  std::vector<file> files;
};

struct context {
  std::vector<std::unique_ptr<volume>> volumes;
  std::vector<std::unique_ptr<job>> jobs;

  std::size_t next_id;

  std::string default_pool_name;
  std::string default_pool_type;
  std::string default_media_type;

  std::string default_job_name;
  std::string default_client_name;
  std::string default_fileset_name;
  std::string default_fileset_md5;

  std::string host_name;
  std::string prog_name;
  std::string prog_version;
  std::string prog_date;

  std::vector<std::string> errors;

  std::string archive_dir;
  std::string data_dir;


  rand_type rand_state;
};

template <typename... Args>
void SetError(context* ctx, fmt::format_string<Args...> fmt, Args&&... args)
{
  ctx->errors.emplace_back(fmt::format(fmt, std::forward<Args>(args)...));
}

job* DeclareJob(context* ctx, std::string_view name)
{
  return ctx->jobs.emplace_back(std::make_unique<job>(name)).get();
}

volume* DeclareVolume(context* ctx, std::string_view name)
{
  return ctx->volumes.emplace_back(std::make_unique<volume>(name)).get();
}

struct record_writer {
  char* begin{nullptr};

  std::size_t current_record_size{0};
  std::size_t written_bytes{0};
  std::size_t max_bytes{0};

  char* size_ptr{nullptr};

  record_writer(char* data, std::size_t size) noexcept
      : begin{data}, max_bytes{size}
  {
  }

  std::size_t byte_size() const noexcept { return written_bytes; }
  std::size_t record_size() const noexcept { return current_record_size; }

  void set_size_marker()
  {
    // todo: we have to handle the oom condition somehow
    assert(!size_ptr);
    size_ptr = begin + written_bytes;

    // allocate space for size
    written_bytes += 4;
    current_record_size += 4;
  }
};


void WriteBytes(record_writer& writer, std::string_view value)
{
  auto bytes_to_write = value.size() + 1;
  auto bytes_free = writer.max_bytes - writer.written_bytes;

  writer.written_bytes += bytes_to_write;
  writer.current_record_size += bytes_to_write;

  if (bytes_free < bytes_to_write) { return; }

  auto* ptr = writer.begin + writer.max_bytes - bytes_free;
  std::memcpy(ptr, value.data(), value.size());
  ptr[value.size()] = 0;
}
void WriteBytes(record_writer& writer, const char* data, std::size_t size)
{
  auto bytes_free = writer.max_bytes - writer.written_bytes;

  writer.written_bytes += size;
  writer.current_record_size += size;

  if (bytes_free < size) { return; }

  auto* ptr = writer.begin + writer.max_bytes - bytes_free;
  std::memcpy(ptr, data, size);
}

void PlaceholderBytes(record_writer& writer, std::size_t size)
{
  writer.written_bytes += size;
  writer.current_record_size += size;
}

void PhantomBytes(record_writer& writer, std::size_t size)
{
  writer.current_record_size += size;
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

  writer.current_record_size = 0;
}

void BeginContinuationRecord(record_writer& writer,
                             std::int32_t stream,
                             std::int32_t file_idx)
{
  assert(stream >= 0);
  BeginRecord(writer, -stream, file_idx);
}

bool EndRecord(record_writer& writer)
{
  assert(writer.size_ptr);

  network_order::network<std::uint32_t> net{
      static_cast<std::uint32_t>(writer.current_record_size)};

  auto bytes = net.stored_bytes();

  std::memcpy(writer.size_ptr, bytes.data(), bytes.size());

  writer.size_ptr = nullptr;

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
  b.resize(writer.written_bytes);
  b.shrink_to_fit();

  if (!WriteBlockHeader(b, j)) { return false; }
  return true;
}

void full_write(int fd, std::span<char> vec)
{
  std::size_t written = 0;
  while (written < vec.size()) {
    auto bytes_written = write(fd, vec.data() + written, vec.size() - written);

    assert(bytes_written > 0);

    written += bytes_written;
  }
}

bool InsertSessionStartLabel(record_writer& writer, context* ctx, job* job)
{
  if (!job->vol_session_id) { job->vol_session_id = ++ctx->next_id; }
  if (!job->vol_session_time) {
    job->vol_session_time
        = std::chrono::duration_cast<std::chrono::seconds>(
              std::chrono::steady_clock::now().time_since_epoch())
              .count();
  }

  if (job->type == job_type::Unset) { job->type = job_type::Backup; }

  if (job->level == job_level::Unset) { job->level = job_level::Full; }

  BeginRecord(writer, job->vol_session_id, volume_label::StartOfSession);

  WriteBytes(writer, "Bareos 2.0 immortal\n");
  WriteNetwork(writer, BAREOS_TAPE_VERSION);
  WriteNetwork<std::uint32_t>(writer, job->vol_session_id);

  WriteNetwork<int64_t>(
      writer, std::chrono::steady_clock::now().time_since_epoch().count());
  WriteNetwork<double>(writer, 0);

  WriteBytes(writer, ctx->default_pool_name);
  WriteBytes(writer, ctx->default_pool_type);

  WriteBytes(writer, ctx->default_job_name);
  WriteBytes(writer, ctx->default_client_name);

  WriteBytes(writer, job->name);
  WriteBytes(writer, ctx->default_fileset_name);
  WriteNetwork(writer, to_underlying(job->type));
  WriteNetwork(writer, to_underlying(job->level));

  WriteBytes(writer, ctx->default_fileset_md5);

  if (!EndRecord(writer)) {
    SetError(ctx, "{} byte block is too small for sos header!",
             writer.max_bytes);
    return false;
  }

  return true;
}

bool InsertSessionEndLabel(record_writer& writer, context* ctx, job* job)
{
  if (!job->vol_session_id) { job->vol_session_id = ++ctx->next_id; }
  if (!job->vol_session_time) {
    job->vol_session_time
        = std::chrono::duration_cast<std::chrono::seconds>(
              std::chrono::steady_clock::now().time_since_epoch())
              .count();
  }

  if (job->type == job_type::Unset) { job->type = job_type::Backup; }

  if (job->level == job_level::Unset) { job->level = job_level::Full; }

  BeginRecord(writer, job->vol_session_id, volume_label::EndOfSession);

  WriteBytes(writer, "Bareos 2.0 immortal\n");
  WriteNetwork(writer, BAREOS_TAPE_VERSION);
  WriteNetwork<std::uint32_t>(writer, job->vol_session_id);

  WriteNetwork<int64_t>(
      writer, std::chrono::steady_clock::now().time_since_epoch().count());
  WriteNetwork<double>(writer, 0);

  WriteBytes(writer, ctx->default_pool_name);
  WriteBytes(writer, ctx->default_pool_type);

  WriteBytes(writer, ctx->default_job_name);
  WriteBytes(writer, ctx->default_client_name);

  WriteBytes(writer, job->name);
  WriteBytes(writer, ctx->default_fileset_name);
  WriteNetwork(writer, to_underlying(job->type));
  WriteNetwork(writer, to_underlying(job->level));

  WriteBytes(writer, ctx->default_fileset_md5);


  WriteNetwork(writer, std::uint32_t(job->current_file_idx));
  WriteNetwork(writer, job->statistics.jobbytes);
  WriteNetwork(writer, job->statistics.start_block);
  WriteNetwork(writer, job->statistics.end_block);
  WriteNetwork(writer, job->statistics.start_file);
  WriteNetwork(writer, job->statistics.end_file);
  WriteNetwork(writer, job->statistics.errors);
  WriteNetwork(writer, job->statistics.status);

  if (!EndRecord(writer)) {
    SetError(ctx, "{} byte block is too small for sos header!",
             writer.max_bytes);
    return false;
  }

  return true;
}

void random_fill(context* ctx, std::vector<char>& vec)
{
  if constexpr (1) {
    std::size_t i = 0;
    for (; i + sizeof(rand_type) < vec.size(); i += sizeof(rand_type)) {
      rand_type rand_value = next_random(ctx->rand_state);
      memcpy(&vec[i], &rand_value, sizeof(rand_value));
    }

    if (i != vec.size()) {
      rand_type rand_value = next_random(ctx->rand_state);
      memcpy(&vec[i], &rand_value, vec.size() - i);
    }
  } else {
    memset(vec.data(), 0xde, vec.size());
  }
}

block NextBlock(context* ctx, job* job, bool split, std::size_t size)
{
  block data;
  data.resize(size);

  auto writer = BeginBlock(data);

  // first we need to write the block header

  bool done = false;

  while (!done && writer.byte_size() < size) {
    if (job->current_file_idx == 0) {
      // we need to first generate start of session record

      if (!InsertSessionStartLabel(writer, ctx, job)) { return {}; }

      job->current_file_idx += 1;
    } else {
      // we need to write a record header

      switch (job->current_state) {
        case job::file_state::ATTRIBUTES: {
          assert(job->leftovers().empty());

          if (job->f.fd) { job->files.emplace_back(std::move(job->f)); }

          job->f.name = fmt::format("/file-{}", job->current_file_idx);

          auto tmp_dir = ctx->data_dir.empty() ? "/tmp" : ctx->data_dir.c_str();

          job->f.fd = file_descriptor{open(tmp_dir, O_TMPFILE | O_RDWR, 0600)};

          assert(job->f.fd);

          struct stat s;
          assert(fstat(job->f.fd, &s) >= 0);

          BeginRecord(writer, to_underlying(Stream::UnixAttribs),
                      job->current_file_idx);

          char encoded[1000];
          EncodeStat(encoded, &s, sizeof(s), 0,
                     to_underlying(Stream::FileData));

          std::string_view attributes{encoded, strlen(encoded)};

          using namespace std::literals;

          std::string attribute_content
              = fmt::format("{} {} {}\0{}\0\0{}\0{}\0"sv, job->current_file_idx,
                            static_cast<int32_t>(FT_REG), job->f.name.c_str(),
                            attributes, "", 0);

          WriteBytes(writer, attribute_content);

          if (!EndRecord(writer)) {
            SetError(ctx, "could not insert file attributes");
            done = true;
            break;
          }

          job->current_state = job::file_state::DATA;
        } break;
        case job::file_state::DATA: {
          assert(job->f.fd);
          std::span<char> record_data;

          if (auto leftovers = job->leftovers(); !leftovers.empty()) {
            BeginContinuationRecord(writer, to_underlying(Stream::FileData),
                                    job->current_file_idx);

            record_data = leftovers;
          } else {
            BeginRecord(writer, to_underlying(Stream::FileData),
                        job->current_file_idx);

            auto record_data_size = job->current_record_size;

            if (!split) {
              // if we are not a split record, we cannot try to write more data
              // than there is space in the block
              auto current_max_record_data_size = size - writer.byte_size();

              if (record_data_size > current_max_record_data_size) {
                record_data_size = current_max_record_data_size;
              }
            }

            auto& scratch = job->scratch;
            scratch.resize(record_data_size);

            // fill with random data
            random_fill(ctx, scratch);
            full_write(job->f.fd, scratch);

            record_data = std::span{scratch};

            job->leftover_offset = 0;
          }

          auto current_max_record_data_size = size - writer.byte_size();

          if (record_data.size() > current_max_record_data_size) {
            {
              // todo: this isnt totally correct, this should only be done
              // if we just created this record, i.e. we arent in a continuation
              // record

              PhantomBytes(writer,
                           record_data.size() - current_max_record_data_size);
            }


            record_data = record_data.subspan(0, current_max_record_data_size);
          }


          job->statistics.jobbytes += record_data.size();
          job->leftover_offset += record_data.size();

          WriteBytes(writer, record_data.data(), record_data.size());

          if (!EndRecord(writer)) {
            SetError(ctx, "could not insert file data");
            done = true;
            break;
          }
        } break;
      }
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

  using btime_t = int64_t;

  WriteBytes(writer, "Bareos 2.0 immortal\n");
  WriteNetwork(writer, BAREOS_TAPE_VERSION);
  WriteNetwork(writer, btime_t{0});
  WriteNetwork(writer, btime_t{0});
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
  if (job->current_file_idx == 0) {
    std::uint64_t start_addr = 0;
    for (auto& b : vol->blocks) { start_addr += b.size(); }

    job->statistics.start_block = static_cast<std::uint32_t>(start_addr);
    job->statistics.start_file = static_cast<std::uint32_t>(start_addr >> 32);
  }
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

void FlushJob(context* ctx, volume* vol, job* job)
{
  if (vol->blocks.empty()) { InsertVolumeLabel(ctx, vol, job); }

  block data;
  data.resize(vol->block_size);

  if (job->f.fd) { job->files.emplace_back(std::move(job->f)); }

  bool done = false;

  while (!done) {
    auto writer = BeginBlock(data);

    /* TODO: flush rest of split block */

    if (auto leftovers = job->leftovers(); !leftovers.empty()) {
      BeginContinuationRecord(writer, to_underlying(Stream::FileData),
                              job->current_file_idx);


      auto current_max_record_data_size = writer.max_bytes - writer.byte_size();

      if (leftovers.size() >= current_max_record_data_size) {
        leftovers = leftovers.subspan(0, current_max_record_data_size);
      }

      job->leftover_offset += leftovers.size();

      WriteBytes(writer, leftovers.data(), leftovers.size());

      if (!EndRecord(writer)) {
        SetError(ctx, "could not flush rest of split block");
        return;
      }
    }

    std::uint64_t end_addr = 0;
    for (auto& b : vol->blocks) { end_addr += b.size(); }
    job->statistics.end_block = static_cast<std::uint32_t>(end_addr);
    job->statistics.end_file = static_cast<std::uint32_t>(end_addr >> 32);

    job->statistics.status = JS_Terminated;

    // TODO: if this doesnt work because of size constraints, we should
    // create a new block and try again
    if (!InsertSessionEndLabel(writer, ctx, job)) {
      SetError(ctx, "could not insert session end label for job {}",
               job->name.c_str());
      return;
    } else {
      done = true;
    }

    if (!EndBlock(data, job, writer)) {
      SetError(ctx, "could not flush job {}", job->name.c_str());
      return;
    }

    vol->blocks.emplace_back(std::move(data));
  }
}

void full_write(int fd, std::size_t count, const char* bytes)
{
  std::size_t offset = 0;
  while (offset < count) {
    auto bytes_written = write(fd, bytes + offset, count - offset);
    assert(bytes_written > 0);

    offset += bytes_written;
  }
}

void print_error(std::string_view error) { fmt::print("[ERROR] {}\n", error); }

int main(int argc, char* argv[])
{
  CLI::App app;

  std::string archive_dir;
  std::string data_dir;
  app.add_option("--archive_dir", archive_dir,
                 "directory where the volumes will get stored")
      ->required()
      ->check(CLI::ExistingDirectory);

  app.add_option("--data_dir", data_dir,
                 "directory where the job files will get stored")
      ->check(CLI::ExistingDirectory);

  CLI11_PARSE(app, argc, argv);

  context ctx[1] = {};

  srand(time(nullptr));
  ctx->rand_state = rand();

  fmt::print("initial random seed: {}\n", ctx->rand_state);

  ctx->archive_dir = archive_dir;
  ctx->data_dir = data_dir;

  ctx->default_pool_name = "Full";
  ctx->default_pool_type = "Backup";
  ctx->default_media_type = "File";

  ctx->default_fileset_name = "MyFileSet";
  ctx->default_fileset_md5 = "1234";

  ctx->default_client_name = "localuser";
  ctx->default_job_name = "backuper";

  ctx->host_name = "fake.example.com";
  ctx->prog_name = "generate-volume";
  ctx->prog_version = "0.0.1";
  ctx->prog_date = "1. Jan 1970";

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

  if (!ctx->errors.empty()) {
    for (auto& error : ctx->errors) { print_error(error); }
    return 1;
  }

  for (auto& volume : ctx->volumes) {
    std::string path = archive_dir + "/" + volume->name;
    int fd = open(path.c_str(), O_WRONLY | O_CREAT, 0664);

    assert(fd >= 0);

    for (auto& b : volume->blocks) { full_write(fd, b.size(), b.data()); }

    close(fd);
  }

  if (!data_dir.empty()) {
    // realise the files so that we can later compare them to the
    // restore result
    for (auto& job : ctx->jobs) {
      // std::string job_path = ctx->job_path(job.get());

      // printf("making %s\n", job_path.c_str());

      // if (mkdir(job_path.c_str(), 0644) < 0) {
      //   if (errno != EEXIST) {
      //     perror(fmt::format("could not create dir {}", job_path).c_str());
      //     continue;
      //   }
      // }

      for (auto& file : job->files) {
        // note: file.name starts with /
        std::string path
            = fmt::format("{}{}-{}", ctx->data_dir, file.name, job->name);
        if (linkat(file.fd, "", AT_FDCWD, path.c_str(), AT_EMPTY_PATH) < 0) {
          perror(fmt::format("could not realise file {}", path).c_str());
        }
      }
    }
  }
}
