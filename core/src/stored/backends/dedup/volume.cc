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
config config_from_data(
    const std::unordered_map<std::uint32_t, std::string>& block_names,
    const std::unordered_map<std::uint32_t, std::string>& part_names,
    const std::unordered_map<std::uint32_t, std::string>& data_names,
    const data& backing)
{
  config new_conf;

  auto& bfs = new_conf.bfiles;
  auto& pfs = new_conf.pfiles;
  auto& dfs = new_conf.dfiles;

  bfs.push_back(config::block_file{
      .relpath = block_names.at(0),
      .Start = 0,
      .End = backing.blocks.size(),
      .Idx = 0,
  });

  pfs.push_back(config::part_file{
      .relpath = part_names.at(0),
      .Start = 0,
      .End = backing.parts.size(),
      .Idx = 0,
  });

  for (auto [bsize, idx] : backing.bsize_to_idx) {
    auto dfile = backing.idx_to_dfile.at(idx);
    auto& df = backing.datafiles.at(dfile);

    if (df.size() % bsize != 0) { throw std::runtime_error("bad data file"); }

    dfs.push_back(config::data_file{
        .relpath = data_names.at(idx),
        .Size = df.size(),
        .BlockSize = bsize,
        .Idx = idx,
        .ReadOnly = false,
    });
  }

  return new_conf;
}

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

block to_dedup(block_header header, std::uint64_t Begin, std::uint32_t Count)
{
  return block{
      .CheckSum = header.CheckSum,
      .BlockSize = header.BlockSize,
      .BlockNumber = header.BlockNumber,
      .ID = {header.ID[0], header.ID[1], header.ID[2], header.ID[3]},
      .VolSessionId = header.VolSessionId,
      .VolSessionTime = header.VolSessionTime,
      .Count = Count,
      .Begin = Begin,
  };
}
block_header from_dedup(block b)
{
  return block_header{
      .CheckSum = b.CheckSum,
      .BlockSize = b.BlockSize,
      .BlockNumber = b.BlockNumber,
      .ID = {b.ID[0], b.ID[1], b.ID[2], b.ID[3]},
      .VolSessionId = b.VolSessionId,
      .VolSessionTime = b.VolSessionTime,
  };
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

  raii_fd conf_fd = openat(dird, "config", flags);

  if (!conf_fd) {
    std::string errctx = "Cannot open '";
    errctx += path;
    errctx += "/config'";
    throw std::system_error(errno, std::generic_category(), errctx);
  }
  auto content = LoadFile(conf_fd.fileno());
  auto conf = config::deserialize(content.data(), content.size());

  for (auto& bf : conf.bfiles) { block_names[bf.Idx] = bf.relpath; }
  for (auto& pf : conf.pfiles) { record_names[pf.Idx] = pf.relpath; }
  for (auto& df : conf.dfiles) { data_names[df.Idx] = df.relpath; }

  backing.emplace(
      open_context{
          .read_only = read_only,
          .flags = flags,
          .dird = dird,
      },
      conf);
}

data::data(open_context ctx, const config& conf)
{
  if (conf.bfiles.size() != 1) {
    throw std::runtime_error("bad config (num blockfiles ("
                             + std::to_string(conf.bfiles.size()) + ") != 1)");
  }
  if (conf.pfiles.size() != 1) {
    throw std::runtime_error("bad config (num recordfiles ("
                             + std::to_string(conf.pfiles.size()) + ") != 1)");
  }

  auto& bf = conf.bfiles[0];
  if (bf.Start != 0) { throw std::runtime_error("blockfile start != 0."); }

  auto& pf = conf.pfiles[0];
  if (pf.Start != 0) { throw std::runtime_error("recordfile start != 0."); }

  raii_fd bfd = OpenRelative(ctx, bf.relpath.c_str());
  raii_fd pfd = OpenRelative(ctx, pf.relpath.c_str());
  blocks = decltype(blocks){ctx.read_only, bfd.fileno(), bf.End};
  parts = decltype(parts){ctx.read_only, pfd.fileno(), pf.End};
  fds.emplace_back(std::move(bfd));
  fds.emplace_back(std::move(pfd));

  for (auto& df : conf.dfiles) {
    raii_fd& fd = fds.emplace_back(OpenRelative(ctx, df.relpath.c_str()));
    if (!ctx.read_only && df.ReadOnly) {
      throw std::runtime_error("file '" + df.relpath + "' is readonly,"
			       " but write permissions requested.");
    }
    auto idx = datafiles.size();
    datafiles.emplace_back(ctx.read_only, fd.fileno(), df.Size);

    idx_to_dfile[df.Idx] = idx;
    bsize_to_idx[df.BlockSize] = df.Idx;
  }

  if (bsize_to_idx.find(1) == bsize_to_idx.end()) {
    throw std::runtime_error("bad config (no datafile with BlockSize 1).");
  }
}

void volume::update_config()
{
  raii_fd conf_fd = openat(dird, "config", O_WRONLY);

  if (!conf_fd) {
    std::string errctx = "Could not open dedup config file";
    throw std::system_error(errno, std::generic_category(), errctx);
  }

  config conf = config_from_data(block_names, record_names, data_names,
                                 backing.value());

  auto serialized = config::serialize(conf);

  WriteFile(conf_fd.fileno(), serialized);
}

std::size_t volume::blockcount() { return backing->blocks.size(); }

save_state volume::BeginBlock(block_header header)
{
  if (current_block) {
    throw std::runtime_error(
        "Trying to start new block before finishing last block.");
  }

  save_state s;

  s.block_size = backing->blocks.size();
  s.part_size = backing->parts.size();

  for (auto& vec : backing->datafiles) { s.data_sizes.push_back(vec.size()); }

  current_block.emplace(header);

  return s;
}

void volume::CommitBlock(save_state&& s)
{
  if (!current_block) {
    throw std::runtime_error("Cannot commit block that was not started.");
  }
  auto start = s.part_size;
  auto count = backing->parts.size() - s.part_size;
  backing->blocks.push_back(to_dedup(current_block.value(), start, count));

  update_config();

  // this looks weird but this is why:
  // if the commit fails for some reason, we need to ensure that the caller
  // can still abort the block (so we cannot move s immediately)
  // but if the commit succeeded we do not want the caller to still have access
  // to the save state (so we want to move it).
  // To solve this dilemma, we take an rvalue reference and move out of it
  // once everything is done (this is now).
  save_state discard = std::move(s);
  static_cast<void>(discard);
  current_block.reset();
}

void volume::AbortBlock(save_state s)
{
  backing->blocks.resize_uninitialized(s.block_size);
  backing->parts.resize_uninitialized(s.part_size);
  ASSERT(s.data_sizes.size() == backing->datafiles.size());

  for (std::size_t i = 0; i < s.data_sizes.size(); ++i) {
    backing->datafiles[i].resize_uninitialized(s.data_sizes[i]);
  }

  if (current_block) { current_block.reset(); }
}

auto volume::reserve_parts(record_header header) -> std::vector<reserved_part>
{
  if (header.Stream < 0) {
    // this header might be a continuation header
    urid rec_id = {
        .VolSessionId = current_block->VolSessionId,
        .VolSessionTime = current_block->VolSessionTime,
        .FileIndex = header.FileIndex,
        .Stream = -header.Stream,
    };

    if (auto found = unfinished.find(rec_id); found != unfinished.end()) {
      auto res = std::move(found->second);
      unfinished.erase(found);
      return res;
    }
  }

  std::vector<reserved_part> reserved_parts;
  std::size_t full_size = header.DataSize;

  for (auto [bsize, idx] : backing->bsize_to_idx) {
    auto copy_size = (full_size / bsize) * bsize;

    if (copy_size > 0) {
      auto& vec = backing->datafiles[backing->idx_to_dfile[idx]];
      auto* start = vec.alloc_uninit(copy_size);
      reserved_parts.push_back(reserved_part{
          .FileIdx = idx,
          .Size = SafeCast(copy_size),
          .Continue = static_cast<std::uint64_t>(start - vec.data())});
    }

    full_size -= copy_size;

    if (full_size == 0) break;
  }

  return reserved_parts;
}

void volume::PushRecord(record_header header,
                        const char* data,
                        std::size_t size)
{
  if (!current_block) {
    throw std::runtime_error(
        "Cannot write record to volume when no block was started.");
  }

  // first write the header ...
  {
    auto it = backing->bsize_to_idx.find(1);
    if (it == backing->bsize_to_idx.end()) {
      throw std::runtime_error(
          "Bad dedup volume: no data file with blocksize 1.");
    }

    auto& vec = backing->datafiles[backing->idx_to_dfile[it->second]];

    char* start = vec.alloc_uninit(sizeof(header));
    std::memcpy(start, &header, sizeof(header));
    backing->parts.push_back(part{.FileIdx = it->second,
                                  .Size = SafeCast(sizeof(header)),
                                  .Begin = (start - vec.data())});
  }


  // ... then reserve space for the data ...
  auto reserved_parts = reserve_parts(header);

  // ... and then we finally write the data where it belongs
  while (size > 0) {
    auto& p = reserved_parts.front();

    auto& vec = backing->datafiles[backing->idx_to_dfile[p.FileIdx]];

    auto copy_size = std::min(SafeCast(size), p.Size);

    std::memcpy(vec.data() + p.Continue, data, copy_size);
    backing->parts.push_back(
        part{.FileIdx = p.FileIdx, .Size = copy_size, .Begin = p.Continue});

    data += copy_size;
    size -= copy_size;

    p.Continue += copy_size;
    p.Size -= copy_size;

    if (p.Size == 0) { reserved_parts.erase(reserved_parts.begin()); }
  }

  if (reserved_parts.size()) {
    // something is left over -> add unfinished entry
    urid rec_id = {
        .VolSessionId = current_block->VolSessionId,
        .VolSessionTime = current_block->VolSessionTime,
        .FileIndex = header.FileIndex,
        .Stream = header.Stream,
    };

    unfinished.emplace(rec_id, std::move(reserved_parts));
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

  auto conf = config::make_default(blocksize);

  auto data = config::serialize(conf);

  raii_fd conf_fd = openat(dird.fileno(), "config", flags, creation_mode);
  if (!conf_fd) {
    std::string errctx = "Cannot open '";
    errctx += path;
    errctx += "/config'";
    throw std::system_error(errno, std::generic_category(), errctx);
  }
  WriteFile(conf_fd.fileno(), data);

  for (auto& bfile : conf.bfiles) {
    if (raii_fd block_fd
        = openat(dird.fileno(), bfile.relpath.c_str(), flags, creation_mode);
        !block_fd) {
      std::string errctx = "Cannot open '";
      errctx += path;
      errctx += "/";
      errctx += bfile.relpath;
      errctx += "'";
      throw std::system_error(errno, std::generic_category(), errctx);
    }
  }
  for (auto& pfile : conf.pfiles) {
    if (raii_fd record_fd
        = openat(dird.fileno(), pfile.relpath.c_str(), flags, creation_mode);
        !record_fd) {
      std::string errctx = "Cannot open '";
      errctx += path;
      errctx += "/";
      errctx += pfile.relpath;
      errctx += "'";
      throw std::system_error(errno, std::generic_category(), errctx);
    }
  }
  for (auto& dfile : conf.dfiles) {
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

void volume::reset()
{
  backing->blocks.clear();
  backing->parts.clear();
  for (auto& vec : backing->datafiles) { vec.clear(); }

  update_config();
}

void volume::truncate()
{
  reset();
  backing->blocks.resize_to_fit();
  backing->parts.resize_to_fit();
  for (auto& vec : backing->datafiles) { vec.resize_to_fit(); }
}

void volume::flush()
{
  backing->blocks.flush();
  backing->parts.flush();
  for (auto& vec : backing->datafiles) { vec.flush(); }
}

std::size_t volume::ReadBlock(std::size_t blocknum,
                              void* data,
                              std::size_t size)
{
  if (backing->blocks.size() == blocknum) {
    // trying to read one past the end is ok.  Just return 0 here to signal
    // that the volume has reached its end.
    return 0;
  } else if (backing->blocks.size() < blocknum) {
    throw std::invalid_argument("blocknum is out of bounds ("
                                + std::to_string(blocknum) + " > "
                                + std::to_string(backing->blocks.size()) + ")");
  }

  chunked_writer stream(data, size);

  auto block = backing->blocks[blocknum];
  auto begin = block.Begin.load();
  auto end = begin + block.Count;

  if (backing->parts.size() < end) {
    throw std::runtime_error("Trying to read parts [" + std::to_string(begin)
                             + ", " + std::to_string(end) + ") but only "
                             + std::to_string(backing->parts.size())
                             + " parts exist.");
  }

  block_header header = from_dedup(block);

  if (!stream.write(&header, sizeof(header))) { return 0; }

  for (auto cur = begin; cur != end; ++cur) {
    auto part = backing->parts[cur];
    // auto rheader = from_dedup(record);

    auto didx = part.FileIdx.load();

    auto dfile = backing->idx_to_dfile.find(didx);
    if (dfile == backing->idx_to_dfile.end()) {
      throw std::runtime_error("Trying to read from unknown file index "
                               + std::to_string(didx)
                               + "; known file indices are ...");
    }

    // if (!stream.write(&rheader, sizeof(rheader))) { return 0; }

    auto dbegin = part.Begin.load();
    auto dsize = part.Size.load();

    auto& vec = backing->datafiles[dfile->second];

    if (vec.size() < dbegin + dsize) {
      throw std::runtime_error(
          "Trying to read region [" + std::to_string(dbegin) + ", "
          + std::to_string(dbegin + dsize) + ") from file __ but only"
          + std::to_string(vec.size()) + " bytes are used.");
    }

    if (!stream.write(vec.data() + dbegin, dsize)) { return 0; }
  }

  return size - stream.leftover();
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
struct serializable_part_file {
  net_string RelPath;
  net<decltype(config::part_file::Start)> Start;
  net<decltype(config::part_file::End)> End;
  net<decltype(config::part_file::Idx)> Idx;

  serializable_part_file() = default;
  serializable_part_file(const config::part_file& pf,
                         std::vector<char>& string_area)
      : RelPath(string_area, pf.relpath.data(), pf.relpath.size())
      , Start{pf.Start}
      , End{pf.End}
      , Idx{pf.Idx}
  {
  }

  config::part_file unserialize(std::string_view string_area)
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
  enum version : std::uint64_t
  {
    v0,  // for testing purposes if needed
    v1,
  };
  net_u64 version;
  net<std::uint32_t> string_size{};
  net<std::uint32_t> num_blockfiles{};
  net<std::uint32_t> num_partfiles{};
  net<std::uint32_t> num_datafiles{};
};
};  // namespace


std::vector<char> config::serialize(const config& conf)
{
  std::vector<char> serial;

  config_header hdr;
  hdr.version = config_header::version::v1;

  std::vector<serializable_block_file> bfs;
  for (auto bfile : conf.bfiles) { bfs.emplace_back(bfile, serial); }
  std::vector<serializable_part_file> pfs;
  for (auto pfile : conf.pfiles) { pfs.emplace_back(pfile, serial); }
  std::vector<serializable_data_file> dfs;
  for (auto dfile : conf.dfiles) { dfs.emplace_back(dfile, serial); }

  hdr.string_size = serial.size();

  for (auto& bf : bfs) {
    auto* as_char = reinterpret_cast<const char*>(&bf);
    serial.insert(serial.end(), as_char, as_char + sizeof(bf));
    hdr.num_blockfiles = hdr.num_blockfiles + 1;
  }
  for (auto& pf : pfs) {
    auto* as_char = reinterpret_cast<const char*>(&pf);
    serial.insert(serial.end(), as_char, as_char + sizeof(pf));
    hdr.num_partfiles = hdr.num_partfiles + 1;
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

config config::make_default(std::uint64_t BlockSize)
{
  return config{
    .bfiles = {
      {"blocks", 0, 0, 0},
    },
    .pfiles = {
      {"parts", 0, 0, 0},
    },
    .dfiles = {
      {"aligned.data", 0, BlockSize, 0, false},
      {"unaligned.data", 0, 1, 1, false},
    }
  };
}

namespace {
config deserialize_config_v1(chunked_reader stream, config_header& hdr)
{
  config conf;

  if (hdr.version != config_header::version::v1) {
    throw std::runtime_error(
        "Internal error: trying to deserialize wrong config version.");
  }

  if (hdr.num_blockfiles != 1) {
    throw std::runtime_error("bad config file (num blockfiles != 1)");
  }
  if (hdr.num_partfiles != 1) {
    throw std::runtime_error("bad config file (num partfiles != 1)");
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

    conf.bfiles.push_back(bf.unserialize(string_area));
  }
  {
    serializable_part_file pf;
    if (!stream.read(&pf, sizeof(pf))) {
      throw std::runtime_error("config file to small.");
    }

    conf.pfiles.push_back(pf.unserialize(string_area));
  }

  for (std::size_t i = 0; i < hdr.num_datafiles; ++i) {
    serializable_data_file df;
    if (!stream.read(&df, sizeof(df))) {
      throw std::runtime_error("config file to small.");
    }

    conf.dfiles.push_back(df.unserialize(string_area));
  }

  if (!stream.finished()) { throw std::runtime_error("config file to big."); }

  return conf;
}
};  // namespace

config config::deserialize(const char* data, std::size_t size)
{
  chunked_reader stream(data, size);

  config_header hdr;
  if (!stream.read(&hdr, sizeof(hdr))) {
    throw std::runtime_error("config file to small.");
  }

  switch (hdr.version.load()) {
    case config_header::version::v1: {
      return deserialize_config_v1(std::move(stream), hdr);
    } break;
    default: {
      throw std::runtime_error("bad config version (version = "
                               + std::to_string(hdr.version.load()) + ")");
    }
  }
}
};  // namespace dedup
