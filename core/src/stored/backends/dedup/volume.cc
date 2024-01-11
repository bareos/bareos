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
#include <sys/stat.h>
}

#include <system_error>
#include <cerrno>

#include "volume.h"
#include "util.h"

namespace dedup {
namespace {
std::uint32_t SafeCast(std::size_t size)
{
  constexpr std::size_t max = std::numeric_limits<std::uint32_t>::max();
  if (size > max) {
    throw std::invalid_argument(std::to_string(size)
                                + " is bigger than allowed ("
                                + std::to_string(max) + ").");
  }

  return size;
}

std::vector<char> LoadFile(int fd)
{
  std::vector<char> loaded;

  auto current_size = loaded.size();
  for (;;) {
    auto next_size = current_size + 1024 * 1024;

    loaded.resize(next_size);

    auto res = read(fd, loaded.data() + current_size, next_size - current_size);

    if (res < 0) {
      std::string errctx = "while reading";
      throw std::system_error(errno, std::generic_category(), errctx);
    } else if (res == 0) {
      break;
    } else {
      current_size += res;
    }
  }

  loaded.resize(current_size);
  return loaded;
};

void WriteFile(int fd, const std::vector<char>& written)
{
  std::size_t progress = 0;
  while (progress < written.size()) {
    auto res = write(fd, written.data() + progress, written.size() - progress);
    if (res < 0) {
      std::string errctx = "while writing";
      throw std::system_error(errno, std::generic_category(), errctx);
    } else if (res == 0) {
      break;
    } else {
      progress += res;
    }
  }
}

int OpenRelative(open_context ctx, const char* path)
{
  int fd = openat(ctx.dird, path, ctx.flags);

  if (fd < 0) {
    std::string errctx = "while opening '";
    errctx += path;
    errctx += "'";
    throw std::system_error(errno, std::generic_category(), errctx);
  }

  return fd;
}

std::size_t aligned_idx(const std::vector<config::data_file>& datafiles)
{
  ASSERT(datafiles.size() == 2);

  if (datafiles[0].BlockSize != 1) {
    return 0;
  } else {
    return 1;
  }
}
};  // namespace

volume::volume(open_type type, const char* path) : sys_path{path}
{
  bool read_only = type == open_type::ReadOnly;
  int flags = (read_only) ? O_RDONLY : O_RDWR;
  int dir_flags = O_RDONLY | O_DIRECTORY;
  dird = open(path, dir_flags);

  if (dird < 0) {
    std::string errctx = "Cannot open '";
    errctx += path;
    errctx += "'";
    throw std::system_error(errno, std::generic_category(), errctx);
  }

  if (raii_fd conf_fd = openat(dird, "config", flags); conf_fd) {
    conf.emplace(LoadFile(conf_fd.fileno()));
  } else {
    std::string errctx = "Cannot open '";
    errctx += path;
    errctx += "/config'";
    throw std::system_error(errno, std::generic_category(), errctx);
  }

  backing.emplace(
      open_context{
          .read_only = read_only,
          .flags = flags,
          .dird = dird,
      },
      conf.value());
}

data::data(open_context ctx, const config& conf)
{
  auto& bf = conf.blockfile();
  if (bf.Start != 0) { throw std::runtime_error("blockfile start != 0."); }

  auto& rf = conf.recordfile();
  if (rf.Start != 0) { throw std::runtime_error("recordfile start != 0."); }

  auto& dfs = conf.datafiles();
  if (dfs.size() != 2) { throw std::runtime_error("too many datafiles."); }
  const config::data_file* unalf = nullptr;
  const config::data_file* alf = nullptr;

  for (auto& df : dfs) {
    if (df.BlockSize == 1) {
      unalf = &df;
    } else if (df.BlockSize > 1) {
      alf = &df;
    }
  }

  if (!unalf) { throw std::runtime_error("no unaligned file."); }

  if (!alf) { throw std::runtime_error("no aligned file."); }

  raii_fd bfd = OpenRelative(ctx, bf.relpath.c_str());
  raii_fd rfd = OpenRelative(ctx, rf.relpath.c_str());
  raii_fd afd = OpenRelative(ctx, alf->relpath.c_str());
  raii_fd ufd = OpenRelative(ctx, unalf->relpath.c_str());

  if (!ctx.read_only && alf->ReadOnly) {
    throw std::runtime_error("aligned file is readonly, but rw requested.");
  }
  if (!ctx.read_only && unalf->ReadOnly) {
    throw std::runtime_error("unaligned file is readonly, but rw requested.");
  }

  blocks = decltype(blocks){ctx.read_only, bfd.fileno(), bf.End};
  records = decltype(records){ctx.read_only, rfd.fileno(), rf.End};
  aligned = decltype(aligned){ctx.read_only, afd.fileno(), alf->Size};
  unaligned = decltype(unaligned){ctx.read_only, ufd.fileno(), unalf->Size};
}

void volume::update_config()
{
  conf->blockfile().End = backing->blocks.size();
  conf->recordfile().End = backing->records.size();
  for (auto& dfile : conf->datafiles()) {
    if (dfile.BlockSize == 1) {
      dfile.Size = backing->unaligned.size();
    } else {
      dfile.Size = backing->aligned.size();
    }
  }

  auto serialized = conf->serialize();

  raii_fd conf_fd = openat(dird, "config", O_WRONLY);

  if (!conf_fd) {
    std::string errctx = "Could not open dedup config file";
    throw std::system_error(errno, std::generic_category(), errctx);
  }

  WriteFile(conf_fd.fileno(), serialized);
}

save_state volume::BeginBlock()
{
  return {
      .block_size = backing->blocks.size(),
      .record_size = backing->records.size(),
      .aligned_size = backing->aligned.size(),
      .unaligned_size = backing->unaligned.size(),
  };
}

void volume::CommitBlock(save_state s, block_header header)
{
  backing->blocks.push_back(block{
      .CheckSum = header.CheckSum,
      .BlockSize = header.BlockSize,
      .BlockNumber = header.BlockNumber,
      .ID = {header.ID[0], header.ID[1], header.ID[2], header.ID[3]},
      .VolSessionId = header.VolSessionId,
      .VolSessionTime = header.VolSessionTime,
      .Count = backing->records.size() - s.record_size,
      .Begin = s.record_size,
  });

  update_config();
}

void volume::AbortBlock(save_state s)
{
  backing->blocks.resize_uninitialized(s.block_size);
  backing->records.resize_uninitialized(s.record_size);
  backing->aligned.resize_uninitialized(s.aligned_size);
  backing->unaligned.resize_uninitialized(s.unaligned_size);
}

void volume::PushRecord(record_header header,
                        const char* data,
                        std::size_t size)
{
  auto& dfiles = conf->datafiles();
  auto idx = aligned_idx(dfiles);

  if (size % dfiles[idx].BlockSize == 0) {
    // aligned
    auto& dfile = dfiles[idx];
    auto& vec = backing->aligned;

    auto start = vec.size();
    vec.reserve_extra(size);
    vec.append_range(data, size);
    backing->records.push_back(record{
        .FileIndex = header.FileIndex,
        .Stream = header.Stream,
        .DataSize = header.DataSize,
        .Padding = 0,
        .FileIdx = dfile.Idx,
        .Size = size,
        .Begin = start,
    });
  } else {
    // unaligned
    auto& dfile = dfiles[1 - idx];

    auto& vec = backing->unaligned;

    auto start = vec.size();
    vec.reserve_extra(size);
    vec.append_range(data, size);
    backing->records.push_back(record{
        .FileIndex = header.FileIndex,
        .Stream = header.Stream,
        .DataSize = header.DataSize,
        .Padding = 0,
        .FileIdx = dfile.Idx,
        .Size = size,
        .Begin = start,
    });
  }
}

void volume::create_new(int creation_mode,
                        const char* path,
                        std::size_t blocksize)
{
  int dir_mode
      = creation_mode | S_IXUSR;  // directories need execute permissions

  if (mkdir(path, dir_mode) < 0) {
    std::string errctx = "Cannot create directory: '";
    errctx += path;
    errctx += "'";
    throw std::system_error(errno, std::generic_category(), errctx);
  }

  int flags = O_RDWR | O_CREAT;
  int dir_flags = O_RDONLY | O_DIRECTORY;
  raii_fd dird = open(path, dir_flags);

  if (!dird) {
    std::string errctx = "Cannot open '";
    errctx += path;
    errctx += "'";
    throw std::system_error(errno, std::generic_category(), errctx);
  }

  config def{blocksize};

  auto data = def.serialize();

  raii_fd conf_fd = openat(dird.fileno(), "config", flags, creation_mode);
  if (!conf_fd) {
    std::string errctx = "Cannot open '";
    errctx += path;
    errctx += "/config'";
    throw std::system_error(errno, std::generic_category(), errctx);
  }
  WriteFile(conf_fd.fileno(), data);

  if (raii_fd block_fd = openat(dird.fileno(), def.blockfile().relpath.c_str(),
                                flags, creation_mode);
      !block_fd) {
    std::string errctx = "Cannot open '";
    errctx += path;
    errctx += "/";
    errctx += def.blockfile().relpath;
    errctx += "'";
    throw std::system_error(errno, std::generic_category(), errctx);
  }
  if (raii_fd record_fd
      = openat(dird.fileno(), def.recordfile().relpath.c_str(), flags,
               creation_mode);
      !record_fd) {
    std::string errctx = "Cannot open '";
    errctx += path;
    errctx += "/";
    errctx += def.recordfile().relpath;
    errctx += "'";
    throw std::system_error(errno, std::generic_category(), errctx);
  }
  for (auto& dfile : def.datafiles()) {
    if (raii_fd block_fd
        = openat(dird.fileno(), dfile.relpath.c_str(), flags, creation_mode);
        !block_fd) {
      std::string errctx = "Cannot open '";
      errctx += path;
      errctx += "/";
      errctx += dfile.relpath;
      errctx += "'";
      throw std::system_error(errno, std::generic_category(), errctx);
    }
  }
}

namespace {
template <typename T> using net = network_order::network<T>;

struct net_string {
  net_u32 Start;
  net_u32 Size;
  net_string() = default;
  net_string(std::vector<char>& string_area,
             const char* data,
             std::uint32_t size)
      : Start{SafeCast(string_area.size())}, Size{size}
  {
    string_area.insert(string_area.end(), data, data + size);
  }
  std::string unserialize(std::string_view string_area)
  {
    if (string_area.size() < std::size_t{Start} + Size) {
      throw std::runtime_error("string area too small (size="
                               + std::to_string(string_area.size())
                               + ", want= [" + std::to_string(Start) + ", "
                               + std::to_string(Start + Size) + "])");
    }
    return std::string{string_area.substr(Start, Size)};
  }
};

struct serializable_block_file {
  net_string RelPath;
  net<decltype(config::block_file::Start)> Start;
  net<decltype(config::block_file::End)> End;
  net<decltype(config::block_file::Idx)> Idx;

  serializable_block_file() = default;
  serializable_block_file(const config::block_file& bf,
                          std::vector<char>& string_area)
      : RelPath(string_area, bf.relpath.data(), bf.relpath.size())
      , Start{bf.Start}
      , End{bf.End}
      , Idx{bf.Idx}
  {
  }

  config::block_file unserialize(std::string_view string_area)
  {
    return {RelPath.unserialize(string_area), Start, End, Idx};
  }
};
struct serializable_record_file {
  net_string RelPath;
  net<decltype(config::record_file::Start)> Start;
  net<decltype(config::record_file::End)> End;
  net<decltype(config::record_file::Idx)> Idx;

  serializable_record_file() = default;
  serializable_record_file(const config::record_file& rf,
                           std::vector<char>& string_area)
      : RelPath(string_area, rf.relpath.data(), rf.relpath.size())
      , Start{rf.Start}
      , End{rf.End}
      , Idx{rf.Idx}
  {
  }

  config::record_file unserialize(std::string_view string_area)
  {
    return {RelPath.unserialize(string_area), Start, End, Idx};
  }
};
struct serializable_data_file {
  net_string RelPath;
  net<decltype(config::data_file::Size)> Size;
  net<decltype(config::data_file::BlockSize)> BlockSize;
  net<decltype(config::data_file::Idx)> Idx;
  bool ReadOnly;

  serializable_data_file() = default;
  serializable_data_file(const config::data_file& df,
                         std::vector<char>& string_area)
      : RelPath(string_area, df.relpath.data(), df.relpath.size())
      , Size{df.Size}
      , BlockSize{df.BlockSize}
      , Idx{df.Idx}
      , ReadOnly{df.ReadOnly}
  {
  }
  config::data_file unserialize(std::string_view string_area)
  {
    return {
        RelPath.unserialize(string_area), Size, BlockSize, Idx, ReadOnly,
    };
  }
};

struct config_header {
  net<std::uint32_t> string_size{};
  net<std::uint32_t> num_blockfiles{};
  net<std::uint32_t> num_recordfiles{};
  net<std::uint32_t> num_datafiles{};
};
};  // namespace


std::vector<char> config::serialize()
{
  std::vector<char> serial;

  config_header hdr;

  serializable_block_file bf{bfile, serial};
  serializable_record_file rf{rfile, serial};
  std::vector<serializable_data_file> dfs;
  for (auto dfile : dfiles) { dfs.emplace_back(dfile, serial); }

  hdr.string_size = serial.size();

  {
    auto* as_char = reinterpret_cast<const char*>(&bf);
    serial.insert(serial.end(), as_char, as_char + sizeof(bf));
    hdr.num_blockfiles = hdr.num_blockfiles + 1;
  }
  {
    auto* as_char = reinterpret_cast<const char*>(&rf);
    serial.insert(serial.end(), as_char, as_char + sizeof(rf));
    hdr.num_recordfiles = hdr.num_recordfiles + 1;
  }
  for (auto& df : dfs) {
    auto* as_char = reinterpret_cast<const char*>(&df);
    serial.insert(serial.end(), as_char, as_char + sizeof(df));
    hdr.num_datafiles = hdr.num_datafiles + 1;
  }

  {
    auto* as_char = reinterpret_cast<const char*>(&hdr);
    serial.insert(serial.begin(), as_char, as_char + sizeof(hdr));
  }

  return serial;
}

config::config(const std::vector<char>& serial)
{
  chunked_reader stream(serial.data(), serial.size());

  config_header hdr;
  if (!stream.read(&hdr, sizeof(hdr))) {
    throw std::runtime_error("config file to small.");
  }

  if (hdr.num_blockfiles != 1) {
    throw std::runtime_error("bad config file (num blockfiles != 1)");
  }
  if (hdr.num_recordfiles != 1) {
    throw std::runtime_error("bad config file (num recordfiles != 1)");
  }
  if (hdr.num_datafiles != 2) {
    throw std::runtime_error("bad config file (num datafiles != 2)");
  }

  const char* string_begin = stream.get(hdr.string_size);
  if (!string_begin) { throw std::runtime_error("config file to small."); }
  std::string_view string_area(string_begin, hdr.string_size);

  {
    serializable_block_file bf;
    if (!stream.read(&bf, sizeof(bf))) {
      throw std::runtime_error("config file to small.");
    }

    bfile = bf.unserialize(string_area);
  }
  {
    serializable_record_file rf;
    if (!stream.read(&rf, sizeof(rf))) {
      throw std::runtime_error("config file to small.");
    }

    rfile = rf.unserialize(string_area);
  }

  for (std::size_t i = 0; i < hdr.num_datafiles; ++i) {
    serializable_data_file df;
    if (!stream.read(&df, sizeof(df))) {
      throw std::runtime_error("config file to small.");
    }

    dfiles.push_back(df.unserialize(string_area));
  }

  if (!stream.finished()) { throw std::runtime_error("config file to big."); }
}
};  // namespace dedup
