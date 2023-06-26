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

#include "dedup/dedup_config.h"

#include <unistd.h>
#include <utility>
#include <optional>
#include <cstring>
#include <filesystem>

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
  namespace fs = std::filesystem;
  const char* mount_point
      = device_resource->mount_point ?: device_resource->archive_device_string;
  for (const auto& entry : fs::directory_iterator{mount_point}) {
    if (!entry.exists()) { continue; }

    if (!entry.is_directory()) { continue; }

    auto path = entry.path().filename();

    if (path == "." || path == "..") { continue; }

    // if (!IsVolumeNameLegal(path.c_str())) {
    //   continue;
    // }

    bstrncpy(dcr->VolumeName, path.c_str(), sizeof(dcr->VolumeName));
    if (!dcr->DirGetVolumeInfo(GET_VOL_INFO_FOR_WRITE)) { continue; }
    /* This was not the volume we expected, but it is OK with
     * the Director, so use it. */
    VolCatInfo = dcr->VolCatInfo;
    return 1;
  }

  // we did not find anything
  dev_errno = EIO;
  return 0;
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
    case DeviceMode::CREATE_READ_WRITE:
      break;
    case DeviceMode::OPEN_READ_WRITE:
      break;
    case DeviceMode::OPEN_READ_ONLY:
      break;
    case DeviceMode::OPEN_WRITE_ONLY:
      break;
    default: {
      Emsg0(M_ABORT, 0, _("Illegal mode given to open dev.\n"));
      return -1;
    }
  }

  dedup::volume vol{path, open_mode, mode, device_resource->dedup_block_size};

  if (vol.is_ok()) {
    int new_fd = fd_ctr;
    auto [iter, inserted] = open_volumes.emplace(new_fd, std::move(vol));

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

ssize_t scatter(dedup::volume& vol, const void* data, size_t size)
{
  if (size > std::numeric_limits<std::uint32_t>::max()) { return -1; }

  auto* block = static_cast<const dedup::bareos_block_header*>(data);
  uint32_t bsize = block->BlockSize;

  if (bsize < sizeof(*block)) {
    // the data size has to at least include the block header!
    // otherwise this will not make any sense
    Emsg0(M_ABORT, 0, _("Trying to write bad block!\n"));
    return -1;
  }

  if (size < bsize) {
    // error: cannot write an uncomplete block
    return -1;
  }

  if (bsize != size) {
    // warning: weird block size
  }

  auto* begin = static_cast<const char*>(data);
  auto* current = begin + sizeof(*block);
  auto* end = begin + bsize;

  std::vector<dedup::record_header> records;
  auto& blockfile = vol.get_active_block_file();

  while (current != end) {
    dedup::bareos_record_header* record = (dedup::bareos_record_header*)current;
    if (current + sizeof(*record) > end) {
      Emsg0(M_ABORT, 0, _("Trying to write bad record!\n"));
      return -1;
    }

    auto* payload_start = reinterpret_cast<const char*>(record + 1);
    auto* payload_end = payload_start + record->DataSize;

    if (payload_end > end) {
      // payload is split in multiple blocks
      payload_end = end;
    }
    std::size_t payload_size = payload_end - payload_start;

    std::optional written_loc
        = vol.write_data(*block, *record, payload_start, payload_size);
    if (!written_loc) { return -1; }

    records.emplace_back(*record, written_loc->begin, payload_size,
                         written_loc->file_index);
    current = payload_end;
  }

  std::optional start = vol.write_records(records.data(), records.size());
  if (!start) {
    // error: could not write records
    return -1;
  }
  blockfile.write(dedup::block_header{*block, *start, records.size()});

  return current - begin;
}

ssize_t dedup_file_device::d_write(int fd, const void* data, size_t size)
{
  if (auto found = open_volumes.find(fd); found != open_volumes.end()) {
    dedup::volume& vol = found->second;
    ASSERT(vol.is_ok());
    vol.changed_volume();
    SetEot();
    return scatter(vol, data, size);
  } else {
    return -1;
  }
}

ssize_t gather(dedup::volume& vol, char* data, std::size_t size)
{
  std::optional block = vol.read_block();
  dedup::write_buffer buf{data, size};

  if (!block) { return -1; }

  if (block->BareosHeader.BlockSize > size) { return -1; }

  if (!buf.write(block->BareosHeader)) { return -1; }

  // std::vector<dedup::record_header> records(block->count);

  for (std::uint64_t record_idx = block->start;
       record_idx < block->start + block->count; ++record_idx) {
    std::optional record = vol.read_record(record_idx);

    if (!record) { return -1; }

    if (!buf.write(record->BareosHeader)) { return -1; }

    if (!vol.read_data(record->file_index, record->start, record->size, buf)) {
      return -1;
    }
  }

  return buf.current - data;
}

ssize_t dedup_file_device::d_read(int fd, void* data, size_t size)
{
  if (auto found = open_volumes.find(fd); found != open_volumes.end()) {
    dedup::volume& vol = found->second;
    ASSERT(vol.is_ok());
    ssize_t bytes_written = gather(vol, static_cast<char*>(data), size);
    if (vol.is_at_end()) {
      SetEot();
    } else {
      ClearEot();
    }
    return bytes_written;
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
    dedup::volume& vol = found->second;
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
    dedup::volume& vol = found->second;
    ASSERT(vol.is_ok());
    if (!vol.goto_begin()) { return false; }
    block_num = 0;
    file = 0;
    file_addr = 0;
    if (vol.is_at_end()) {
      SetEot();
    } else {
      ClearEot();
    }
    return UpdatePos(dcr);
  } else {
    return false;
  }
}

bool dedup_file_device::UpdatePos(DeviceControlRecord*)
{
  if (auto found = open_volumes.find(fd); found != open_volumes.end()) {
    dedup::volume& vol = found->second;
    ASSERT(vol.is_ok());
    std::size_t pos = vol.get_active_block_file().current();

    file_addr = 0;
    file = pos >> 32;
    block_num = pos & ((1ULL << 32) - 1);

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
    dedup::volume& vol = found->second;
    ASSERT(vol.is_ok());

    std::uint64_t block_num = static_cast<std::uint64_t>(rfile) << 32 | rblock;

    if (!vol.goto_block(block_num)) { return false; }

    if (vol.is_at_end()) {
      SetEot();
    } else {
      ClearEot();
    }
    return UpdatePos(dcr);
  } else {
    return false;
  }
}

bool dedup_file_device::eod(DeviceControlRecord* dcr)
{
  if (auto found = open_volumes.find(fd); found != open_volumes.end()) {
    dedup::volume& vol = found->second;
    ASSERT(vol.is_ok());
    if (!vol.goto_end()) { return false; }
    SetEot();
    return UpdatePos(dcr);
  } else {
    return false;
  }
}

REGISTER_SD_BACKEND(dedup, dedup_file_device);

} /* namespace storagedaemon  */
