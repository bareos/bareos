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

namespace storagedaemon {

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

static int OpenDirStructure(const char* path, int flags, int mode,
			      int* fd, int* block_fd,
			      int* record_fd, int* data_fd)
{
  // dir needs to be executable if we want to create files inside
  int create_mode = (flags & O_CREAT) ? 0100 : 0;
  int dir = ::open(path, O_DIRECTORY | O_RDONLY, mode | create_mode);

  if (dir < 0) {
    return dir;
  }

  int block = ::openat(dir, "block", flags, mode);
  if (block < 0) {
    ::close(dir);
    return block;
  }
  int record = ::openat(dir, "record", flags, mode);
  if (record < 0) {
    ::close(block);
    ::close(dir);
    return record;
  }
  int data = ::openat(dir, "data", flags, mode);
  if (data < 0) {
    ::close(record);
    ::close(block);
    ::close(dir);
    return data;
  }

  *fd = dir;
  *block_fd = block;
  *record_fd = record;
  *data_fd = data;

  return dir;

}

static int CreateDirStructure(const char* path, int mode,
			      int* fd, int* block_fd,
			      int* record_fd, int* data_fd)
{
  if (struct stat st; ::stat(path, &st) != -1) {
    return -1;
  }

  if (mkdir(path,  mode | 0100) < 0) {
    return -1;
  }

  return OpenDirStructure(path, O_CREAT | O_RDWR | O_BINARY, mode, fd,
			  block_fd, record_fd, data_fd);
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
  case DeviceMode::CREATE_READ_WRITE: {
    return CreateDirStructure(path, mode, &fd, &block_fd, &record_fd, &data_fd);
  } break;
  case DeviceMode::OPEN_READ_WRITE: {
    return OpenDirStructure(path, O_RDWR | O_BINARY, mode, &fd, &block_fd, &record_fd, &data_fd);
  } break;
  case DeviceMode::OPEN_READ_ONLY: {
    return OpenDirStructure(path, O_RDONLY | O_BINARY, mode, &fd, &block_fd, &record_fd, &data_fd);
  } break;
  case DeviceMode::OPEN_WRITE_ONLY: {
    return OpenDirStructure(path, O_WRONLY | O_BINARY, mode, &fd, &block_fd, &record_fd, &data_fd);
  } break;
  default: {
    Emsg0(M_ABORT, 0, _("Illegal mode given to open dev.\n"));
    return -1;
  }
  }
}

template <typename T>
constexpr T byteswap(T);

template <> constexpr std::uint64_t byteswap(std::uint64_t x) { return __builtin_bswap64(x); }
template <> constexpr std::uint32_t byteswap(std::uint32_t x) { return __builtin_bswap32(x); }
template <> constexpr std::uint16_t byteswap(std::uint16_t x) { return __builtin_bswap16(x); }
template <> constexpr std::uint8_t byteswap(std::uint8_t x) { return x; }

template <typename T>
constexpr T byteswap(T val) {
  using nosign = std::make_unsigned_t<T>;
  return static_cast<T>(byteswap<nosign>(static_cast<nosign>(val)));
}

#if !defined(BYTE_ORDER) || !defined(LITTLE_ENDIAN)
#error "Could not determine endianess."
#elif (BYTE_ORDER == LITTLE_ENDIAN)
template <typename T>
T to_network(T val) { return byteswap(val); }
#else
template <typename T>
T to_network(T val) { return val; }
#endif

struct is_network_order {} is_network_order_v;
struct is_native_order {} is_native_order_v;
template <typename T>
struct network_order {
  T as_network;
  T as_native() const { return to_network(as_network); }
  operator T() const { return as_native(); }

  network_order() = default;
  network_order(is_network_order, T val) : as_network{val} {}
  network_order(is_native_order, T val) : as_network{to_network(val)} {}
  network_order(T val) : network_order{is_native_order_v, val} {}
};

static_assert(std::is_standard_layout_v<network_order<int>>);
static_assert(std::is_pod_v<network_order<int>>);
static_assert(std::has_unique_object_representations_v<network_order<int>>);

template <typename U>
static network_order<U> of_network(U network) {
  return network_order<U>{ is_network_order_v, network };
}

template <typename U>
static network_order<U> of_native(U native) {
  return network_order<U>{ is_native_order_v, native };
}

struct bareos_block_header {
  network_order<uint32_t> CheckSum;                /* Block check sum */
  network_order<uint32_t> BlockSize;               /* Block byte size including the header */
  network_order<uint32_t> BlockNumber;             /* Block number */
  char ID[4];              /* Identification and block level */
  network_order<uint32_t> VolSessionId;            /* Session Id for Job */
  network_order<uint32_t> VolSessionTime;          /* Session Time for Job */
};

struct bareos_record_header {
  network_order<int32_t> FileIndex;   /* File index supplied by File daemon */
  network_order<int32_t> Stream;      /* Stream number supplied by File daemon */
  network_order<uint32_t> DataSize;   /* size of following data record in bytes */
};

struct dedup_block_header {
  bareos_block_header     BareosHeader;
  network_order<uint32_t> RecStart;
  network_order<uint32_t> RecEnd;

  dedup_block_header() = default;

  dedup_block_header(const bareos_block_header& base,
		     std::uint32_t RecStart,
		     std::uint32_t RecEnd)
    : BareosHeader(base)
    , RecStart{of_native(RecStart)}
    , RecEnd{of_native(RecEnd)}
  {}
};

static_assert(std::is_standard_layout_v<dedup_block_header>);
static_assert(std::is_pod_v<dedup_block_header>);
static_assert(std::has_unique_object_representations_v<dedup_block_header>);

struct dedup_record_header {
  bareos_record_header    BareosHeader;
  network_order<uint32_t> DataStart;
  network_order<uint32_t> DataEnd;

  dedup_record_header() = default;

  dedup_record_header(const bareos_record_header& base,
		     std::uint32_t DataStart,
		     std::uint32_t DataEnd)
    : BareosHeader(base)
    , DataStart{of_native(DataStart)}
    , DataEnd{of_native(DataEnd)}
  {}
};

static_assert(std::is_standard_layout_v<dedup_record_header>);
static_assert(std::is_pod_v<dedup_record_header>);
static_assert(std::has_unique_object_representations_v<dedup_record_header>);

static void safe_write(int fd, void* data, std::size_t size)
{
  ASSERT(::write(fd, data, size) == static_cast<ssize_t>(size));
}

static void safe_read(int fd, void* data, std::size_t size)
{
  ASSERT(::read(fd, data, size) == static_cast<ssize_t>(size));
}

void split_and_write(int block_fd, int record_fd, int data_fd, const void* data)
{
  bareos_block_header *block = (bareos_block_header*) data;
  uint32_t bsize = block->BlockSize;

  ASSERT(bsize >= sizeof(*block));

  char* current = (char*) (block + 1);

  char* end = (char*) data + bsize;

  ssize_t recpos = ::lseek(record_fd, 0, SEEK_CUR);
  ASSERT(recpos >= 0);
  uint32_t CurrentRec = recpos / sizeof(dedup_record_header);
  uint32_t RecStart = CurrentRec;
  uint32_t RecEnd = RecStart;

  uint32_t actual_size = 0;

  // TODO: fuse split payloads here somewhere ?!
  while (current != end)  {
    bareos_record_header *record = (bareos_record_header*) current;
    RecEnd += 1;

    ASSERT(current + sizeof(*record) < end);


    // the record payload is [current + sizeof(record)]

    char* payload_start = (char*) (record + 1);
    char* payload_end   = payload_start + record->DataSize;

    if (payload_end > end) {
      // payload is split in multiple blocks
      payload_end = end;
    }

    current = payload_end;

    std::size_t size = payload_end - payload_start;
    ssize_t pos = ::lseek(data_fd, 0, SEEK_CUR);
    ASSERT(pos >= 0);
    dedup_record_header drecord{*record, static_cast<std::uint32_t>(pos),
				static_cast<std::uint32_t>(pos + size)};
    safe_write(data_fd, payload_start, size);
    actual_size += size;
    safe_write(record_fd, &drecord, sizeof(drecord));
    actual_size += sizeof(bareos_record_header);
  }

  dedup_block_header dblock{*block, RecStart, RecEnd};
  safe_write(block_fd, &dblock, sizeof(dblock));
  actual_size += sizeof(bareos_block_header);

  ASSERT(actual_size == bsize);
}

ssize_t dedup_file_device::d_write(int fd, const void* data, size_t size)
{
  (void) fd;
  split_and_write(block_fd, record_fd, data_fd, data);

  return size;
}

ssize_t dedup_file_device::d_read(int fd, void* data, size_t size)
{
  (void) fd;

  dedup_block_header dblock;
  if (ssize_t bytes = ::read(block_fd, &dblock, sizeof(dblock));
      bytes < 0) {
    return bytes;
  } else if (bytes == 0) {
    return 0;
  }

  if (size < dblock.BareosHeader.BlockSize) {
    return -1;
  }

  uint32_t RecStart = dblock.RecStart;
  uint32_t RecEnd   = dblock.RecEnd;
  ASSERT(RecStart <= RecEnd);
  uint32_t NumRec = RecEnd - RecStart;
  if (NumRec == 0) { return 0; }
  ::lseek(record_fd, RecStart * sizeof(dedup_record_header), SEEK_SET);
  dedup_record_header* records = new dedup_record_header[NumRec];

  // if (static_cast<ssize_t>(NumRec * sizeof(dedup_record_header)) != ::read(record_fd, records, NumRec * sizeof(dedup_record_header))) {
  //   delete[] records;
  //   return -1;
  // }
  safe_read(record_fd, records, NumRec * sizeof(dedup_record_header));

  ssize_t lastend = -1;
  char* head = (char*)data;
  char* end  = head + size;
  memcpy(head, &dblock.BareosHeader,
	 sizeof(bareos_block_header));
  head += sizeof(bareos_block_header);
  for (uint32_t i = 0; i < NumRec; ++i) {
    if (lastend != static_cast<ssize_t>(records[i].DataStart)) {
      ::lseek(data_fd, records[i].DataStart, SEEK_SET);
    }

    memcpy(head, &records[i].BareosHeader,
	   sizeof(bareos_record_header));
    head += sizeof(bareos_record_header);
    safe_read(data_fd, head, records[i].DataEnd - records[i].DataStart);
    head += records[i].DataEnd - records[i].DataStart;
    lastend = records[i].DataEnd;
    ASSERT(head <= end);
  }

  return head - (char*)data;
}

int dedup_file_device::d_close(int fd) {
  ::close(block_fd);
  ::close(record_fd);
  ::close(data_fd);
  return ::close(fd);
}

int dedup_file_device::d_ioctl(int, ioctl_req_t, char*) { return -1; }

boffset_t dedup_file_device::d_lseek(DeviceControlRecord*,
				     boffset_t,
				     int)
{
  return -1;
}

bool dedup_file_device::d_truncate(DeviceControlRecord*)
{
  return true;
}

bool dedup_file_device::rewind(DeviceControlRecord* dcr)
{
  if (lseek(block_fd, 0, SEEK_SET) != 0) {
    return false;
  }
  if (lseek(data_fd, 0, SEEK_SET) != 0) {
    return false;
  }
  if (lseek(record_fd, 0, SEEK_SET) != 0) {
    return false;
  }
  block_num = 0;
  file = 0;
  file_addr = 0;
  return UpdatePos(dcr);
}

bool dedup_file_device::UpdatePos(DeviceControlRecord*)
{
  // synchronize (real) device position with this->file, this->block
  auto pos = lseek(block_fd, 0, SEEK_CUR);
  if (pos < 0) return false;

  file_addr = pos;
  block_num = pos / sizeof(dedup_block_header);

  ASSERT(block_num * sizeof(dedup_block_header) == file_addr);

  file = 0;

  return true;
}

bool dedup_file_device::Reposition(DeviceControlRecord*, uint32_t rfile, uint32_t rblock)
{
  Dmsg2(10, "file: %u -> %u; block: %u -> %u\n", file, rfile, block_num, rblock);
  block_num = rblock;
  file = rfile;
  file_addr = 0;
  return true;
}

bool dedup_file_device::eod(DeviceControlRecord* dcr)
{
  if (auto res = ::lseek(block_fd, 0, SEEK_END); res < 0) { return false; }
  if (auto res = ::lseek(record_fd, 0, SEEK_END); res < 0) { return false; }
  if (auto res = ::lseek(data_fd, 0, SEEK_END); res < 0) { return false; }
  return UpdatePos(dcr);
}

REGISTER_SD_BACKEND(dedup, dedup_file_device);

} /* namespace storagedaemon  */
