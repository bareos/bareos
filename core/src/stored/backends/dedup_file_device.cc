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

static constexpr std::uint64_t block_number(std::uint32_t rfile,
                                            std::uint32_t rblock)
{
  std::uint64_t block_num = static_cast<std::uint64_t>(rfile) << 32 | rblock;
  return block_num;
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
  if (open_volume.has_value()) {
    // error: a volume is already open
    return -1;
  }
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

  auto& vol = open_volume.emplace(path, open_mode, mode,
                                  device_resource->dedup_block_size);

  if (vol.is_ok()) {
    return ++fd_ctr;
  } else {
    open_volume.reset();
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
        = vol.append_data(*block, *record, payload_start, payload_size);
    if (!written_loc) { return -1; }

    records.emplace_back(*record, written_loc->begin, payload_size,
                         written_loc->file_index);
    current = payload_end;
  }

  std::optional start = vol.append_records(records.data(), records.size());
  if (!start) {
    // error: could not write records
    return -1;
  }
  if (!vol.append_block(dedup::block_header{*block, *start, records.size()})) {
    return -1;
  }

  return current - begin;
}

ssize_t dedup_file_device::d_write(int fd, const void* data, size_t size)
{
  if (fd != fd_ctr) {
    // error: unknown file descriptor
    return -1;
  }

  if (!open_volume) {
    // error: no volume mounted
    return -1;
  }

  dedup::volume& vol = open_volume.value();
  ASSERT(vol.is_ok());
  SetEot();
  auto current_block = block_number(file, block_num);
  if (current_block == 0 && vol.size() == 1) {
    // we are currently trying to relabel the volume
    // since bareos does this often with empty volumes
    // we have a special case allowing this.
    // We do this by reseting the volume to empty.
    vol.reset();
  }

  if (current_block != vol.size()) {
    // error: not at end of device
    return -1;
  }
  return scatter(vol, data, size);
}

ssize_t gather(dedup::volume& vol,
               std::uint64_t blocknum,
               char* data,
               std::size_t size)
{
  std::optional block = vol.read_block(blocknum);
  dedup::write_buffer buf{data, size};

  if (!block) { return -1; }

  if (block->BareosHeader.BlockSize > size) { return -1; }

  if (!buf.write(block->BareosHeader)) { return -1; }

  std::vector<dedup::record_header> records(block->count);

  vol.read_records(block->start, records.data(), records.size());

  for (auto& record : records) {
    if (!buf.write(record.BareosHeader)) { return -1; }

    if (!vol.read_data(record.file_index, record.start, record.size,
                       buf.reserve(record.size))) {
      return -1;
    }
  }

  return buf.current - data;
}

ssize_t dedup_file_device::d_read(int fd, void* data, size_t size)
{
  if (fd != fd_ctr) {
    // error: unknown file descriptor
    return -1;
  }

  if (!open_volume) {
    // error: no volume mounted
    return -1;
  }

  dedup::volume& vol = open_volume.value();
  ASSERT(vol.is_ok());
  auto block = block_number(file, block_num);
  ssize_t bytes_written = gather(vol, block, static_cast<char*>(data), size);
  if (block + 1 == vol.size()) {
    SetEot();
  } else {
    ClearEot();
  }
  return bytes_written;
}

int dedup_file_device::d_close(int fd)
{
  if (fd != fd_ctr) {
    // error: unknown file descriptor
    return -1;
  }

  if (!open_volume) {
    // error: no volume mounted
    return -1;
  }

  open_volume.reset();
  return 0;
}

int dedup_file_device::d_ioctl(int, ioctl_req_t, char*) { return -1; }

boffset_t dedup_file_device::d_lseek(DeviceControlRecord*, boffset_t, int)
{
  return -1;
}

static bool delete_volume(JobControlRecord* jcr, const std::string& path)
{
  namespace stdfs = std::filesystem;

  try {
    std::vector<std::string> files;
    for (auto& file : stdfs::recursive_directory_iterator(path.c_str())) {
      if (file.is_directory()) {
        // error: unexpected directory in volume
        return false;
      } else {
        files.emplace_back(std::move(file).path().string());
      }
    }

    for (auto& file : files) {
      if (SecureErase(jcr, file.c_str()) != 0) { return false; }
    }

    if (!stdfs::remove(path.c_str())) { return false; }
  } catch (stdfs::filesystem_error& error) {
    Jmsg(jcr, M_FATAL, 0, "Encountered error while deleting volume %s: %s\n",
         path.c_str(), error.what());
    return false;
  }

  return true;
}

bool dedup_file_device::d_truncate(DeviceControlRecord* dcr)
{
  if (!open_volume) {
    // error: no volume mounted
    return -1;
  }

  auto* vol = &open_volume.value();
  ASSERT(vol->is_ok());

  if (!me->secure_erase_cmdline) { return vol->reset(); }

  std::string volume_path = vol->name();
  int perm = vol->get_permissions();

  vol = nullptr;
  // close the volume
  open_volume.reset();

  // delete the volume
  if (!delete_volume(dcr->jcr, volume_path)) { return false; }

  // recreate the volume
  open_mode = DeviceMode::CREATE_READ_WRITE;
  vol = &open_volume.emplace(volume_path.c_str(), open_mode, perm,
                             device_resource->dedup_block_size);

  if (!vol->is_ok()) {
    open_volume.reset();
    fd = -1;
    return false;
  }
  return true;
}

bool dedup_file_device::rewind(DeviceControlRecord* dcr)
{
  if (!open_volume) {
    // error: no volume mounted
    return -1;
  }

  dedup::volume& vol = open_volume.value();
  ASSERT(vol.is_ok());
  block_num = 0;
  file = 0;
  // we do not use file_addr
  // so make sure it stays at 0
  file_addr = 0;
  if (vol.size() == 0) {
    SetEot();
  } else {
    ClearEot();
  }

  return UpdatePos(dcr);
}

bool dedup_file_device::UpdatePos(DeviceControlRecord*)
{
  // the volume itself is stateless
  // so we have nothing to do
  return true;
}

bool dedup_file_device::Reposition(DeviceControlRecord* dcr,
                                   uint32_t rfile,
                                   uint32_t rblock)
{
  Dmsg2(10, "file: %u -> %u; block: %u -> %u\n", file, rfile, block_num,
        rblock);

  if (!open_volume) {
    // error: no volume mounted
    return -1;
  }

  dedup::volume& vol = open_volume.value();

  ASSERT(vol.is_ok());

  block_num = rblock;
  file = rfile;

  if (block_number(file, block_num) == vol.size()) {
    SetEot();
  } else {
    ClearEot();
  }
  return UpdatePos(dcr);
}

bool dedup_file_device::eod(DeviceControlRecord* dcr)
{
  if (!open_volume) {
    // error: no volume mounted
    return -1;
  }

  dedup::volume& vol = open_volume.value();

  ASSERT(vol.is_ok());
  auto block = vol.size();
  block_num = static_cast<std::uint32_t>(block);
  file = static_cast<std::uint32_t>(block >> 32);
  SetEot();
  return UpdatePos(dcr);
}

bool dedup_file_device::d_flush(DeviceControlRecord*)
{
  if (!open_volume) {
    // error: no volume mounted
    return -1;
  }

  dedup::volume& vol = open_volume.value();
  ASSERT(vol.is_ok());

  return vol.flush();
}

REGISTER_SD_BACKEND(dedup, dedup_file_device);

} /* namespace storagedaemon  */
