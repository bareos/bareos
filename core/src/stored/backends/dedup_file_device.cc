/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2023-2023 Bareos GmbH & Co. KG

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

#include "include/bareos.h"
#include "stored/stored.h"
#include "stored/stored_globals.h"
#include "stored/sd_backends.h"
#include "stored/device_control_record.h"
#include "dedup_file_device.h"
#include "lib/berrno.h"
#include "lib/util.h"

#include <unistd.h>
#include <utility>
#include <optional>
#include <cstring>

namespace storagedaemon {

using net_u64 = network_order::network_value<std::uint64_t>;
using net_u32 = network_order::network_value<std::uint32_t>;
using net_u16 = network_order::network_value<std::uint16_t>;
using net_u8 = std::uint8_t;
using net_i32 = network_order::network_value<std::int32_t>;
using net_i64 = network_order::network_value<std::int64_t>;

using iter = std::vector<std::byte>::const_iterator;

std::uint64_t CalculateCheckSum(const std::byte* begin, const std::byte* end)
{
  std::uint64_t checksum = 1103;

  for (const std::byte* current = begin; current != end; ++current) {
    checksum *= 101;
    checksum += static_cast<std::uint64_t>(*current);
  }

  return checksum;
}

std::vector<std::byte> serialize_string(const std::string& s)
{
  // we always write in utf-8
  // we assume that the given string is in utf-8 already
  // in situations where this is not the case, we need to convert
  // the string to utf-8 here
  std::vector<std::byte> bytes{
    reinterpret_cast<const std::byte*>(&*s.begin()),
    reinterpret_cast<const std::byte*>(&*s.end())
  };
  return bytes;
}

std::string deserialize_string(iter begin, iter end)
{
  // the bytes we are given represent a utf-8 string
  // we assume that std::string has an utf-8 encoding here
  // otherwise we would need to convert the data first.
  // We could use std::u8string in C++20 here but there
  // is no builtin way to convert u8string <-> string
  // in C++20 (not in 23 either)
  std::string string{
    reinterpret_cast<const char*>(&*begin),
    reinterpret_cast<const char*>(&*end)
  };
  return string;
}

struct dedup_config_file_header
{
  // since char8_t only exists from C++20, we use std::byte here
  // In the future we should switch to char8_t
  static constexpr std::array<std::byte, 8> correct_identifier = {
    static_cast<std::byte>(u8'D'),
    static_cast<std::byte>(u8'D'),
    static_cast<std::byte>(u8'C'),
    static_cast<std::byte>(u8'O'),
    static_cast<std::byte>(u8'N'),
    static_cast<std::byte>(u8'F'),
    static_cast<std::byte>(u8'I'),
    static_cast<std::byte>(u8'G'),
  };

  std::array<std::byte, 8> identifier; // will say "DDCONFIG" in ascii
  net_u64 file_size;
  net_u32 version;
  net_u32 section_alignment;
  net_u64 file_checksum; // checksum does not include this header
  net_u64 checksum; // checksum for the header; does not include itsef
                    // this _has_ to be the last member!
};

static_assert(sizeof(dedup_config_file_header) == 40);

// sections are 16byte aligned
struct dedup_config_section_header
{
  enum class types : std::uint16_t {
    GeneralInfo = 0,
    BlockFile   = 1,
    RecordFile  = 2,
    DataFile    = 3,
  };
  // total section size = header size + data size + padding
  // padding is between header & data
  net_u16 type;
  net_u16 version;
  net_u32 padding; // the amount of padding between the header and start of data

  net_u32 data_size;
  net_u32 reserved_0{0};

  net_u64 data_checksum;
  net_u64 checksum; // checksum for the header; does not include itsef
                    // this _has_ to be the last member!
};

static_assert(sizeof(dedup_config_section_header) == 32);

struct dedup_config_general_section
{
  enum class byte_order : std::uint8_t {
    little_endian = 1,
    big_endian = 2
  };

  net_u32 block_header_size;
  net_u32 section_header_size;
  net_u8  written_order; // order of values inside file
};

struct dedup_config_block_file_section
{
  net_u32 file_index;
  net_u32 start_block;
  net_u32 end_block;
  net_u32 path_length;
  // the path data follows directly without any padding
  // as if
  // std::byte path_data[]; (in utf8 encoding)
  // was the next member (not legal in C++)
};

struct dedup_config_record_file_section
{
  net_u32 file_index;
  net_u32 start_record;
  net_u32 end_record;
  net_u32 path_length;
  // the path data follows directly without any padding
  // as if
  // std::byte path_data[]; (in utf8 encoding)
  // was the next member (not legal in C++)
};

struct dedup_config_data_file_section
{
  net_i64 block_size;
  net_u32 file_index;
  net_u32 path_length;
  // the path data follows directly without any padding
  // as if
  // std::byte path_data[]; (in utf8 encoding)
  // was the next member (not legal in C++)
};

std::optional<const dedup_config_file_header*> try_read_file_header(iter& current,
								    iter end)
{
  if (static_cast<std::size_t>(end - current) < sizeof(dedup_config_file_header)) {
    return std::nullopt;
  }

  // this is ub; hopefully c++ somedays will get support for memory mapping/etc.
  auto* file_header = reinterpret_cast<const dedup_config_file_header*>(&*current);

  if (file_header->identifier != dedup_config_file_header::correct_identifier) {
    // error: bad file id
    return std::nullopt;
  }

  if (file_header->checksum != CalculateCheckSum(reinterpret_cast<const std::byte*>(file_header),
						 reinterpret_cast<const std::byte*>(&file_header->checksum))) {
    // error: file header checksum does not match
    return std::nullopt;
  }

  current += sizeof(dedup_config_file_header);

  if (file_header->file_checksum != CalculateCheckSum(&*current, &*end)) {
    // error: file checksum does not match
    return std::nullopt;
  }

  return file_header;
}

std::optional<const dedup_config_section_header*> try_read_section_header(iter& current,
									  iter end)
{
  if (static_cast<std::size_t>(end - current) < sizeof(dedup_config_section_header)) {
    return std::nullopt;
  }

  // this is ub; hopefully c++ somedays will get support for memory mapping/etc.
  auto* section_header = reinterpret_cast<const dedup_config_section_header*>(&*current);

  if (section_header->version != 0) {
    // error: bad file id
    return std::nullopt;
  }



  current += sizeof(dedup_config_section_header);

  if (section_header->checksum != CalculateCheckSum(reinterpret_cast<const std::byte*>(section_header),
						    reinterpret_cast<const std::byte*>(&section_header->checksum))) {
    // error: header checksum does not match
    return std::nullopt;
  }

  return section_header;
}

std::size_t alignment_diff(std::size_t current, std::size_t alignment)
{
  auto diff = current % alignment;
  auto offset = (alignment - diff) % alignment;

  return offset;
}

void make_aligned(iter& current, iter begin, std::size_t alignment)
{
  current += alignment_diff(current - begin, alignment);
}

std::vector<std::byte> serialize_with_header(std::uint16_t version,
					     dedup_config_section_header::types type,
					     std::size_t data_align,
					     const std::vector<std::byte>& data)
{
  dedup_config_section_header header;

  header.type = static_cast<std::uint16_t>(type);
  header.version = version;
  header.padding = alignment_diff(sizeof(header), data_align);
  header.data_size = data.size();
  header.data_checksum = CalculateCheckSum(&*data.begin(), &*data.end());
  header.checksum = CalculateCheckSum(reinterpret_cast<std::byte*>(&header),
				      reinterpret_cast<std::byte*>(&header.checksum));

  std::vector<std::byte> bytes{reinterpret_cast<const std::byte*>(&header),
			       reinterpret_cast<const std::byte*>(&header) + sizeof(header)};
  bytes.reserve(sizeof(header) + header.padding + data.size());
  bytes.resize(bytes.size() + header.padding);
  bytes.insert(bytes.end(), data.begin(), data.end());

  return bytes;
}
std::vector<std::byte> serialize_general_info()
{
  dedup_config_general_section general;

  general.block_header_size = 0;
  general.section_header_size = 0;
  general.written_order = static_cast<std::uint8_t>(dedup_config_general_section::byte_order::big_endian);

  std::vector<std::byte> data{reinterpret_cast<std::byte*>(&general),
			 reinterpret_cast<std::byte*>(&general) + sizeof(general)};

  return serialize_with_header(0, dedup_config_section_header::types::GeneralInfo,
			       alignof(general), data);
}

std::vector<std::byte> serialize_block_file(const block_file& blockfile)
{
  dedup_config_block_file_section network;

  network.file_index = blockfile.file_index;
  network.start_block = blockfile.start_block;
  network.end_block = blockfile.end_block;
  network.path_length = blockfile.path.size();

  std::vector<std::byte> data{reinterpret_cast<std::byte*>(&network),
			 reinterpret_cast<std::byte*>(&network) + sizeof(network)};
  std::vector<std::byte> string = serialize_string(blockfile.path);
  data.insert(data.end(), string.begin(), string.end());

  return serialize_with_header(0, dedup_config_section_header::types::BlockFile,
			       alignof(network), data);
}

std::vector<std::byte> serialize_record_file(const record_file& recordfile)
{
  dedup_config_record_file_section network;

  network.file_index = recordfile.file_index;
  network.start_record = recordfile.start_record;
  network.end_record = recordfile.end_record;
  network.path_length = recordfile.path.size();

  std::vector<std::byte> data{reinterpret_cast<std::byte*>(&network),
			 reinterpret_cast<std::byte*>(&network) + sizeof(network)};
  std::vector<std::byte> string = serialize_string(recordfile.path);
  data.insert(data.end(), string.begin(), string.end());

  return serialize_with_header(0, dedup_config_section_header::types::RecordFile,
			       alignof(network), data);
}

std::vector<std::byte> serialize_data_file(const data_file& datafile)
{
  dedup_config_data_file_section network;

  network.block_size = datafile.block_size;
  network.file_index = datafile.file_index;
  network.path_length = datafile.path.size();

  std::vector<std::byte> data{reinterpret_cast<std::byte*>(&network),
			 reinterpret_cast<std::byte*>(&network) + sizeof(network)};
  std::vector<std::byte> string = serialize_string(datafile.path);
  data.insert(data.end(), string.begin(), string.end());

  return serialize_with_header(0, dedup_config_section_header::types::DataFile,
			       alignof(network), data);
}

static std::vector<std::byte> to_bytes(const dedup_volume_config& config)
{
  std::vector<std::byte> bytes;

  dedup_config_file_header file_header;

  bytes.resize(sizeof(file_header)); // reserve space
  bytes.resize(bytes.size() + alignment_diff(bytes.size(), 16));

  std::vector<std::byte> general_info_bytes = serialize_general_info();

  bytes.insert(bytes.end(), general_info_bytes.begin(),
	       general_info_bytes.end());

  for (auto& blockfile : config.blockfiles) {
    bytes.resize(bytes.size() + alignment_diff(bytes.size(), 16));
    std::vector<std::byte> blockfile_bytes = serialize_block_file(blockfile);

    bytes.insert(bytes.end(), blockfile_bytes.begin(),
		 blockfile_bytes.end());
  }

  for (auto& recordfile : config.recordfiles) {
    bytes.resize(bytes.size() + alignment_diff(bytes.size(), 16));
    std::vector<std::byte> recordfile_bytes = serialize_record_file(recordfile);

    bytes.insert(bytes.end(), recordfile_bytes.begin(),
		 recordfile_bytes.end());
  }

  for (auto& datafile : config.datafiles) {
    bytes.resize(bytes.size() + alignment_diff(bytes.size(), 16));
    std::vector<std::byte> datafile_bytes = serialize_data_file(datafile);

    bytes.insert(bytes.end(), datafile_bytes.begin(),
		 datafile_bytes.end());
  }

  file_header.file_size = bytes.size();
  file_header.section_alignment = 16;
  file_header.version = 0;
  file_header.identifier = dedup_config_file_header::correct_identifier;
  file_header.file_checksum = CalculateCheckSum(&bytes.front() + sizeof(file_header), &*bytes.end());
  file_header.checksum = CalculateCheckSum(reinterpret_cast<const std::byte*>(&file_header),
					   reinterpret_cast<const std::byte*>(&file_header.checksum));

  std::copy_n(reinterpret_cast<std::byte*>(&file_header), sizeof(file_header), bytes.begin());

  return bytes;
}


static std::optional<dedup_volume_config> from_bytes(const std::vector<std::byte>& bytes)
{
  auto begin = bytes.begin();
  auto current = begin;
  auto end   = bytes.end();

  if (std::optional file_header_opt = try_read_file_header(current, end)) {
    auto* file_header = file_header_opt.value();
    dedup_volume_config config;
    if (file_header->version != 0) {
      // error: unrecognized version
      return std::nullopt;
    }

    if (file_header->file_size != static_cast<std::size_t>(end - begin)) {
      // error: file size missmatch
      return std::nullopt;
    }

    bool encountered_general_info = false;

    while (current < end) {
      make_aligned(current, begin, file_header->section_alignment);
      std::optional section_header_opt = try_read_section_header(current, end);

      if (!section_header_opt) { return std::nullopt; }

      auto* section_header = section_header_opt.value();

      if (end - current < section_header->padding + section_header->data_size) {
	// error: eof reached
	return std::nullopt;
      }

      current += section_header->padding;

      // todo: compute checksum
      if (section_header->data_checksum != CalculateCheckSum(&*current,
							     &*(current + section_header->data_size))) {
	// error: bad checksum
	return std::nullopt;
      }
      switch (dedup_config_section_header::types{section_header->type.as_native()}) {
      case dedup_config_section_header::types::GeneralInfo: {
	if (encountered_general_info) {
	  // error: two general info
	  return std::nullopt;
	}

	if (section_header->data_size < sizeof(dedup_config_general_section)) {
	  // error: bad section header
	  return std::nullopt;
	}

	encountered_general_info = true;

	auto* general_section = reinterpret_cast<const dedup_config_general_section*>(&*current);
	current += sizeof(*general_section);

	if (general_section->block_header_size != 0) {
	  // error: bad block header size
	  return std::nullopt;
	}

	if (general_section->section_header_size != 0) {
	  // error: bad block header size
	  return std::nullopt;
	}

	if (general_section->written_order != static_cast<std::uint8_t>(dedup_config_general_section::byte_order::big_endian)) {
	  // error: bad byte order
	  return std::nullopt;
	}

      } break;
      case dedup_config_section_header::types::BlockFile: {
	if (section_header->data_size < sizeof(dedup_config_block_file_section)) {
	  // error: bad section header
	  return std::nullopt;
	}

	auto* block = reinterpret_cast<const dedup_config_block_file_section*>(&*current);
	current += sizeof(*block);

	if (end - current < block->path_length) {
	  // error: reached eof
	  return std::nullopt;
	}

	block_file block_file;
	block_file.file_index = block->file_index;
	block_file.start_block = block->start_block;
	block_file.end_block = block->end_block;
	block_file.path = deserialize_string(current, current + block->path_length);

	current += block->path_length;
	config.blockfiles.emplace_back(std::move(block_file));
      } break;
      case dedup_config_section_header::types::RecordFile: {
	if (section_header->data_size < sizeof(dedup_config_record_file_section)) {
	  // error: bad section header
	  return std::nullopt;
	}

	auto* record = reinterpret_cast<const dedup_config_record_file_section*>(&*current);
	current += sizeof(*record);

	if (end - current < record->path_length) {
	  // error: reached eof
	  return std::nullopt;
	}

	record_file record_file;
	record_file.file_index = record->file_index;
	record_file.start_record = record->start_record;
	record_file.end_record = record->end_record;
	record_file.path = deserialize_string(current, current + record->path_length);

	current += record->path_length;
	config.recordfiles.emplace_back(std::move(record_file));
      } break;
      case dedup_config_section_header::types::DataFile: {
	if (section_header->data_size < sizeof(dedup_config_data_file_section)) {
	  // error: bad section header
	  return std::nullopt;
	}

	auto* data = reinterpret_cast<const dedup_config_data_file_section*>(&*current);
	current += sizeof(*data);

	if (end - current < data->path_length) {
	  // error: reached eof
	  return std::nullopt;
	}

	data_file data_file;
	data_file.file_index = data->file_index;
	data_file.block_size = data->block_size;
	data_file.path = deserialize_string(current, current + data->path_length);

	current += data->path_length;
	config.datafiles.emplace_back(std::move(data_file));
      } break;
      default: {
	// error: unrecognized section header
	return std::nullopt;
      }
      }
    }

    if (!encountered_general_info) {
      // error: no general info
      return std::nullopt;
    }

    return config;
  } else {
    // error: file to small
    return std::nullopt;
  }
}

/**
 * Mount the device.
 *
 * If timeout, wait until the mount command returns 0.
 * If !timeout, try to mount the device only once.
 */
bool dedup_file_device::MountBackend(DeviceControlRecord*, int)
{
  bool was_mounted = std::exchange(mounted, true);
  return !was_mounted;
}

/**
 * Unmount the device
 *
 * If timeout, wait until the unmount command returns 0.
 * If !timeout, try to unmount the device only once.
 */
bool dedup_file_device::UnmountBackend(DeviceControlRecord*, int)
{
  bool was_mounted = std::exchange(mounted, false);
  return was_mounted;
}

bool dedup_file_device::ScanForVolumeImpl(DeviceControlRecord* dcr)
{
  return ScanDirectoryForVolume(dcr);
}

int dedup_file_device::d_open(const char* path, int, int mode)
{
  // todo parse mode
  // see Device::set_mode

  // create/open the folder structure
  // path
  // +- block
  // +- record
  // +- data

  switch (open_mode) {
  case DeviceMode::CREATE_READ_WRITE: break;
  case DeviceMode::OPEN_READ_WRITE: break;
  case DeviceMode::OPEN_READ_ONLY: break;
  case DeviceMode::OPEN_WRITE_ONLY: break;
  default: {
    Emsg0(M_ABORT, 0, _("Illegal mode given to open dev.\n"));
    return -1;
  }
  }

  dedup_volume vol{path, open_mode, mode};

  if (vol.is_ok()) {
    int new_fd = fd_ctr;
    auto [iter, inserted]
        = open_volumes.emplace(new_fd, std::move(vol));

    if (!inserted) {
      // volume was already open; that should not be possible
      open_volumes.erase(iter);
      return -1;
    }

    fd_ctr += 1;
    return new_fd;
  } else {
    return -1;
  }
}


struct bareos_block_header {
  net_u32 CheckSum;       /* Block check sum */
  net_u32 BlockSize;      /* Block byte size including the header */
  net_u32 BlockNumber;    /* Block number */
  char ID[4];             /* Identification and block level */
  net_u32 VolSessionId;   /* Session Id for Job */
  net_u32 VolSessionTime; /* Session Time for Job */
};

struct bareos_record_header {
  net_i32 FileIndex; /* File index supplied by File daemon */
  net_i32 Stream;    /* Stream number supplied by File daemon */
  net_u32 DataSize;  /* size of following data record in bytes */
};

struct dedup_block_header {
  bareos_block_header BareosHeader;
  net_u32 RecStart;
  net_u32 RecEnd;

  dedup_block_header() = default;

  dedup_block_header(const bareos_block_header& base,
                     std::uint32_t RecStart,
                     std::uint32_t RecEnd)
      : BareosHeader(base)
      , RecStart{network_order::of_native(RecStart)}
      , RecEnd{network_order::of_native(RecEnd)}
  {
  }
};

static_assert(std::is_standard_layout_v<dedup_block_header>);
static_assert(std::is_pod_v<dedup_block_header>);
static_assert(std::has_unique_object_representations_v<dedup_block_header>);

struct dedup_record_header {
  bareos_record_header BareosHeader;
  net_u32 DataStart;
  net_u32 DataEnd;

  dedup_record_header() = default;

  dedup_record_header(const bareos_record_header& base,
                      std::uint32_t DataStart,
                      std::uint32_t DataEnd)
      : BareosHeader(base)
      , DataStart{network_order::of_native(DataStart)}
      , DataEnd{network_order::of_native(DataEnd)}
  {
  }
};

static_assert(std::is_standard_layout_v<dedup_record_header>);
static_assert(std::is_pod_v<dedup_record_header>);
static_assert(std::has_unique_object_representations_v<dedup_record_header>);

// static void safe_write(int fd, void* data, std::size_t size)
// {
//   ASSERT(::write(fd, data, size) == static_cast<ssize_t>(size));
// }

// static void safe_read(int fd, void* data, std::size_t size)
// {
//   ASSERT(::read(fd, data, size) == static_cast<ssize_t>(size));
// }

ssize_t scatter(dedup_volume& vol, const void* data)
{
  (void) vol;
  (void) data;
  // int block_fd = vol.get_block_file();
  // int record_fd = vol.get_record_file();
  // bareos_block_header* block = (bareos_block_header*)data;
  // uint32_t bsize = block->BlockSize;

  // ASSERT(bsize >= sizeof(*block));

  // char* current = (char*)(block + 1);

  // char* end = (char*)data + bsize;

  // ssize_t recpos = ::lseek(record_fd, 0, SEEK_CUR);
  // ASSERT(recpos >= 0);
  // uint32_t CurrentRec = recpos / sizeof(dedup_record_header);
  // uint32_t RecStart = CurrentRec;
  // uint32_t RecEnd = RecStart;

  // uint32_t actual_size = 0;

  // // TODO: fuse split payloads here somewhere ?!
  // while (current != end) {
  //   bareos_record_header* record = (bareos_record_header*)current;
  //   RecEnd += 1;

  //   ASSERT(current + sizeof(*record) <= end);


  //   // the record payload is [current + sizeof(record)]

  //   char* payload_start = (char*)(record + 1);
  //   char* payload_end = payload_start + record->DataSize;

  //   if (payload_end > end) {
  //     // payload is split in multiple blocks
  //     payload_end = end;
  //   }

  //   current = payload_end;

  //   ssize_t pos = ::lseek(data_fd, 0, SEEK_CUR);
  //   ASSERT(pos >= 0);
  //   if (payload_start < end) {
  //     std::size_t size = payload_end - payload_start;
  //     dedup_record_header drecord{*record, static_cast<std::uint32_t>(pos),
  // 				  static_cast<std::uint32_t>(pos + size)};
  //     safe_write(data_fd, payload_start, size);
  //     actual_size += size;
  //     safe_write(record_fd, &drecord, sizeof(drecord));
  //     actual_size += sizeof(bareos_record_header);
  //   } else {
  //     dedup_record_header drecord{*record, static_cast<std::uint32_t>(pos),
  // 				  static_cast<std::uint32_t>(pos)};
  //     safe_write(record_fd, &drecord, sizeof(drecord));
  //     actual_size += sizeof(bareos_record_header);

  //   }
  // }

  // dedup_block_header dblock{*block, RecStart, RecEnd};
  // safe_write(block_fd, &dblock, sizeof(dblock));
  // actual_size += sizeof(bareos_block_header);

  // ASSERT(actual_size == bsize);
  return -1;
}

ssize_t dedup_file_device::d_write(int fd, const void* data, size_t size)
{
  (void) fd;
  (void) data;
  (void) size;
  if (auto found = open_volumes.find(fd); found != open_volumes.end()) {
    dedup_volume& vol = found->second;
    ASSERT(vol.is_ok());
    vol.changed_volume();
    return scatter(vol, data);
  } else {
    return -1;
  }
}

ssize_t gather(dedup_volume& vol, void* data, std::size_t size)
{
  (void) vol;
  (void) data;
  (void) size;
    // int block_fd = vol.get_block();
    // int record_fd = vol.get_record();
    // int data_fd = vol.get_data();
    // dedup_block_header dblock;
    // if (ssize_t bytes = ::read(block_fd, &dblock, sizeof(dblock)); bytes < 0) {
    //   return bytes;
    // } else if (bytes == 0) {
    //   return 0;
    // }

    // if (size < dblock.BareosHeader.BlockSize) { return -1; }

    // uint32_t RecStart = dblock.RecStart;
    // uint32_t RecEnd = dblock.RecEnd;
    // ASSERT(RecStart <= RecEnd);
    // uint32_t NumRec = RecEnd - RecStart;
    // if (NumRec == 0) { return 0; }
    // ::lseek(record_fd, RecStart * sizeof(dedup_record_header), SEEK_SET);
    // dedup_record_header* records = new dedup_record_header[NumRec];

    // // if (static_cast<ssize_t>(NumRec * sizeof(dedup_record_header)) !=
    // // ::read(record_fd, records, NumRec * sizeof(dedup_record_header))) {
    // //   delete[] records;
    // //   return -1;
    // // }
    // safe_read(record_fd, records, NumRec * sizeof(dedup_record_header));

    // ssize_t lastend = -1;
    // char* head = (char*)data;
    // char* end = head + size;
    // memcpy(head, &dblock.BareosHeader, sizeof(bareos_block_header));
    // head += sizeof(bareos_block_header);
    // for (uint32_t i = 0; i < NumRec; ++i) {
    //   if (lastend != static_cast<ssize_t>(records[i].DataStart)) {
    //     ::lseek(data_fd, records[i].DataStart, SEEK_SET);
    //   }

    //   memcpy(head, &records[i].BareosHeader, sizeof(bareos_record_header));
    //   head += sizeof(bareos_record_header);
    //   safe_read(data_fd, head, records[i].DataEnd - records[i].DataStart);
    //   head += records[i].DataEnd - records[i].DataStart;
    //   lastend = records[i].DataEnd;
    //   ASSERT(head <= end);
    // }

    // return head - (char*)data;
  return -1;
}

ssize_t dedup_file_device::d_read(int fd, void* data, size_t size)
{
  if (auto found = open_volumes.find(fd); found != open_volumes.end()) {
    dedup_volume& vol = found->second;
    ASSERT(vol.is_ok());
    return gather(vol, data, size);
  } else {
    return -1;
  }
}

int dedup_file_device::d_close(int fd)
{
  size_t num_erased = open_volumes.erase(fd);
  if (num_erased == 1) {
    return 0;
  } else {
    return -1;
  }
}

int dedup_file_device::d_ioctl(int, ioctl_req_t, char*) { return -1; }

boffset_t dedup_file_device::d_lseek(DeviceControlRecord*, boffset_t, int)
{
  return -1;
}

bool dedup_file_device::d_truncate(DeviceControlRecord*)
{
  if (auto found = open_volumes.find(fd); found != open_volumes.end()) {
    dedup_volume& vol = found->second;
    ASSERT(vol.is_ok());
    vol.changed_volume();
    return vol.reset();
  } else {
    return false;
  }
}

bool dedup_file_device::rewind(DeviceControlRecord* dcr)
{
  if (auto found = open_volumes.find(fd); found != open_volumes.end()) {
    dedup_volume& vol = found->second;
    ASSERT(vol.is_ok());
    if (lseek(vol.get_block_file(), 0, SEEK_SET) != 0) { return false; }
    if (lseek(vol.get_record_file(), 0, SEEK_SET) != 0) { return false; }
    block_num = 0;
    file = 0;
    file_addr = 0;
    return UpdatePos(dcr);
  } else {
    return false;
  }
}

bool dedup_file_device::UpdatePos(DeviceControlRecord*)
{
  if (auto found = open_volumes.find(fd); found != open_volumes.end()) {
    dedup_volume& vol = found->second;
    ASSERT(vol.is_ok());
    // synchronize (real) device position with this->file, this->block
    auto pos = lseek(vol.get_block_file(), 0, SEEK_CUR);
    if (pos < 0) return false;

    file_addr = pos;
    block_num = pos / sizeof(dedup_block_header);

    ASSERT(block_num * sizeof(dedup_block_header) == file_addr);

    file = 0;

    return true;
  } else {
    return false;
  }
}

bool dedup_file_device::Reposition(DeviceControlRecord* dcr,
                                   uint32_t rfile,
                                   uint32_t rblock)
{
  Dmsg2(10, "file: %u -> %u; block: %u -> %u\n", file, rfile, block_num,
        rblock);
  ASSERT(file == 0);

  if (auto found = open_volumes.find(fd); found != open_volumes.end()) {
    dedup_volume& vol = found->second;
    ASSERT(vol.is_ok());

    if (auto res = ::lseek(vol.get_block_file(), rblock * sizeof(dedup_block_header),
                           SEEK_SET);
        res < 0) {
      return false;
    }

    // todo: if we are not at the end of the device
    //       we should read the block header and position
    //       the record and data files as well
    //       otherwise set the record and data files to their respective end

    return UpdatePos(dcr);
  } else {
    return false;
  }
}

bool dedup_file_device::eod(DeviceControlRecord* dcr)
{
  if (auto found = open_volumes.find(fd); found != open_volumes.end()) {
    dedup_volume& vol = found->second;
    ASSERT(vol.is_ok());
    if (auto res = ::lseek(vol.get_block_file(), 0, SEEK_END); res < 0) {
      return false;
    }
    if (auto res = ::lseek(vol.get_record_file(), 0, SEEK_END); res < 0) {
      return false;
    }
    return UpdatePos(dcr);
  } else {
    return false;
  }
}

REGISTER_SD_BACKEND(dedup, dedup_file_device);

void dedup_volume::write_current_config()
{
  std::vector<std::byte> bytes = to_bytes(config);
  if (ftruncate(configfile.fd.get(), 0) != 0) {
    error = true;
  } else if (::lseek(configfile.fd.get(), 0, SEEK_SET) != 0) {
    error = true;
  } else if (write(configfile.fd.get(), &bytes.front(), bytes.size())
	     != static_cast<ssize_t>(bytes.size())) {
    error = true;
  }
}

bool dedup_volume::load_config()
{
  auto config_end = lseek(configfile.fd.get(), 0, SEEK_END);
  auto config_start = lseek(configfile.fd.get(), 0, SEEK_SET);

  if (config_start != 0 || config_start > config_end) {
    // error: cannot determine config file size
    return false;
  }

  std::vector<std::byte> bytes(config_end - config_start);

  if (read(configfile.fd.get(), &bytes.front(), bytes.size())
      != static_cast<ssize_t>(bytes.size())) {
    // error: cannot read config file
    return false;
  }

  std::optional loaded_config = from_bytes(bytes);
  if (!loaded_config) {
    return false;
  }

  // at the moment we only support configurations that have
  // exactly one block and one record file.
  // This might change in the future
  if (loaded_config->blockfiles.size() != 1) {
    // error: to many/few block files
    return false;
  }

  if (loaded_config->recordfiles.size() != 1) {
    // error: to many/few record files
    return false;
  }

  config = std::move(loaded_config.value());
  return true;
}

} /* namespace storagedaemon  */
