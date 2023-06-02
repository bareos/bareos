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

int dedup_file_device::d_open(const char* path, int flags, int mode)
{
  // todo parse mode
  // see Device::set_mode

  // create/open the folder structure
  // path
  // +- block
  // +- record
  // +- data

  struct stat st;

  auto i = ::stat(path, &st);
  if (i == -1) {
    if (mkdir(path, mode | 0100) < 0) return -1;
  } else if ((st.st_mode & S_IFMT) != S_IFDIR) {
    return -1;
  }

  fd = ::open(path, O_DIRECTORY | O_RDONLY, mode | 0100);

  if (fd < 0) return fd;

  block_fd = ::openat(fd, "block", flags, mode);
  if (block_fd < 0) return block_fd;
  record_fd = ::openat(fd, "record", flags, mode);
  if (record_fd < 0) return record_fd;
  data_fd = ::openat(fd, "data", flags, mode);
  if (data_fd < 0) return data_fd;

  return fd;
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

template <typename T>
struct network_order {
  T as_network;
  T as_native() const { return byteswap(as_network); }
  operator T() const { return as_native(); }
};

template <typename U>
static network_order<U> of_network(U network) {
  return network_order<U>{ network };
}

template <typename U>
static network_order<U> of_native(U native) {
  return network_order<U>{ byteswap(native) };
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

struct dedup_block_header : public bareos_block_header {
  network_order<uint32_t> RecStart;
  network_order<uint32_t> RecEnd;

  dedup_block_header() = default;

  dedup_block_header(const bareos_block_header& base,
		     std::uint32_t RecStart,
		     std::uint32_t RecEnd)
    : bareos_block_header(base)
    , RecStart{of_native(RecStart)}
    , RecEnd{of_native(RecEnd)}
  {}
};

struct dedup_record_header : bareos_record_header {
  network_order<uint32_t> DataStart;
  network_order<uint32_t> DataEnd;

  dedup_record_header() = default;

  dedup_record_header(const bareos_record_header& base,
		     std::uint32_t DataStart,
		     std::uint32_t DataEnd)
    : bareos_record_header(base)
    , DataStart{of_native(DataStart)}
    , DataEnd{of_native(DataEnd)}
  {}
};

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

  if (size < dblock.BlockSize) {
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
  memcpy(head, static_cast<bareos_block_header*>(&dblock),
	 sizeof(bareos_block_header));
  head += sizeof(bareos_block_header);
  for (uint32_t i = 0; i < NumRec; ++i) {
    if (lastend != static_cast<ssize_t>(records[i].DataStart)) {
      ::lseek(data_fd, records[i].DataStart, SEEK_SET);
    }

    memcpy(head, static_cast<bareos_record_header*>(&records[i]),
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

bool dedup_file_device::rewind(DeviceControlRecord*)
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
  return true;
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

REGISTER_SD_BACKEND(dedup, dedup_file_device);

} /* namespace storagedaemon  */
