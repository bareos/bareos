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

#include "include/bareos.h"
#include "stored/stored.h"
#include "stored/stored_globals.h"
#include "stored/sd_backends.h"
#include "stored/device_control_record.h"
#include "lib/berrno.h"
#include "lib/util.h"
#include "lib/edit.h"
#include "lib/network_order.h"
#include "dedup_device.h"
#include "dedup/device_options.h"
#include "dedup/util.h"
#include "lib/bsys.h"

#include <unistd.h>
#include <utility>
#include <stdexcept>
#include <cstring>
#include <filesystem>

namespace storagedaemon {

namespace {
constexpr std::uint64_t block_number(std::uint32_t rfile, std::uint32_t rblock)
{
  std::uint64_t block_num = static_cast<std::uint64_t>(rfile) << 32 | rblock;
  return block_num;
}

constexpr std::pair<std::uint32_t, std::uint32_t> file_block(
    std::uint64_t blocknumber)
{
  std::uint32_t file = (blocknumber >> 32);
  std::uint32_t block = blocknumber;

  return {file, block};
}

constexpr bool check_open_mode(DeviceMode open_mode)
{
  switch (open_mode) {
    case DeviceMode::CREATE_READ_WRITE:
    case DeviceMode::OPEN_READ_WRITE:
    case DeviceMode::OPEN_READ_ONLY:
      return true;
      // write only is not possible
    case DeviceMode::OPEN_WRITE_ONLY:
    default:
      return false;
  }
}
};  // namespace

// Mount the device. Timeout is ignored.
bool dedup_device::MountBackend(DeviceControlRecord*, int)
{
  bool was_mounted = std::exchange(mounted, true);
  return !was_mounted;
}

// Unmount the device. Timeout is ignored.
bool dedup_device::UnmountBackend(DeviceControlRecord*, int)
{
  bool was_mounted = std::exchange(mounted, false);
  return was_mounted;
}

bool dedup_device::ScanForVolumeImpl(DeviceControlRecord* dcr)
{
  (void)dcr;
  return false;
}

int dedup_device::d_open(const char* path, int, int mode)
{
  if (openvol) {
    Emsg0(M_ERROR, 0, T_("Volume %s is opened already on this device.\n"),
          openvol->path());
    return -1;
  }

  if (!check_open_mode(open_mode)) {
    Emsg0(M_ABORT, 0, T_("Illegal mode given to open dev. (mode = %d)\n"),
          (int)open_mode);
    return -1;
  }

  bool read_only = open_mode == DeviceMode::OPEN_READ_ONLY;


  try {
    auto parsed = dedup::device_option_parser::parse(dev_options ?: "");

    for (auto& warning : parsed.warnings) {
      Emsg0(M_WARNING, 0, "Dedup device option warning: %s\n", warning.c_str());
    }

    if (!read_only) {
      // try creating a new one but ignore any errors
      // NOTE(ssura): this looks weird, but bareos first tries to open
      //              new volumes with OPEN_READ_ONLY twice before trying
      //              to open them with CREATE_READ_WRITE.  As such
      //              not doing this will lead to bareos outputting two error
      //              messages everytime a job tries to write to a new volume!
      //              Also sometimes bareos calls this with CREATE_READ_WRITE
      //              even though it knows that it already exists.
      //              E.g. when relabeling because of a truncate command.
      try {
        dedup::volume::create_new(mode, path, parsed.options.blocksize);
      } catch (const std::exception& ex) {
        Dmsg3(200,
              "Could not create new volume %s while opening as %s. "
              "ERR=%s\n",
              path,
              open_mode == (DeviceMode::CREATE_READ_WRITE) ? "CREATE_READ_WRITE"
                                                           : "OPEN_READ_WRITE",
              ex.what());
      }
    }

    auto& vol
        = (read_only)
              ? openvol.emplace(dedup::volume::open_type::ReadOnly, path)
              : openvol.emplace(dedup::volume::open_type::ReadWrite, path);

    return vol.fileno();
  } catch (const std::exception& ex) {
    Emsg0(M_ERROR, 0, T_("Could not open volume. ERR=%s\n"), ex.what());
    return -1;
  }
}

ssize_t dedup_device::d_write(int fd, const void* data, size_t size)
{
  using block_header = dedup::block_header;
  using record_header = dedup::record_header;
  using chunked_reader = dedup::chunked_reader;

  struct raii_save_state {
    dedup::volume* vol;
    dedup::save_state save;

    raii_save_state(dedup::volume& vol, block_header header)
        : vol{&vol}, save{vol.BeginBlock(header)}
    {
    }

    void commit()
    {
      vol->CommitBlock(std::move(save));
      vol = nullptr;
    }

    ~raii_save_state()
    {
      if (vol) {
        vol->AbortBlock(std::move(save));
        vol = nullptr;
      }
    }
  };

  if (!openvol) {
    Emsg0(M_ERROR, 0, T_("Trying to write dedup volume when none are open.\n"));
    return -1;
  }

  if (openvol->fileno() != fd) {
    Emsg0(M_ERROR, 0,
          T_("Trying to write dedup volume that is not open "
             "(open = %d, trying to write = %d).\n"),
          openvol->fileno(), fd);
    return -1;
  }

  if (current_block() == 0) {
    if (!ResetOpenVolume()) { return -1; }

    SetEot();
  }

  std::size_t datawritten = 0;
  try {
    chunked_reader stream{data, size};

    while (!stream.finished()) {
      block_header block;
      if (!stream.read(&block, sizeof(block))) {
        throw std::runtime_error("Could not read block header from stream.");
      }

      auto blocksize = sizeof(block);

      // when save goes out of scope without being commited, it will get
      // aborted
      auto save = raii_save_state(openvol.value(), block);

      if (auto* block_data = stream.get(block.size()); !block_data) {
        throw std::runtime_error("Could not read block data from stream.");
      } else {
        chunked_reader records{block_data, block.size()};

        while (!records.finished()) {
          record_header record;
          if (!records.read(&record, sizeof(record))) {
            throw std::runtime_error(
                "Could not read record header from stream.");
          }

          blocksize += sizeof(record);

          // record might be split, so only take as much as we can
          auto rsize = std::min(record.size(), records.leftover());
          if (rsize != record.size()) {
            Dmsg2(500,
                  "Found split record. Record size = %llu, but only %llu bytes "
                  "available.\n",
                  record.size(), rsize);
          }
          if (auto* record_data = records.get(rsize); !record_data) {
            throw std::runtime_error("Could not read record data from stream.");
          } else {
            openvol->PushRecord(record, record_data, rsize);
            blocksize += rsize;
          }
        }
      }

      save.commit();
      datawritten += blocksize;
    }

    return datawritten;
  } catch (const std::exception& ex) {
    Emsg0(M_ERROR, 0,
          T_("Encountered error while trying to write to volume %s. ERR=%s\n"),
          openvol->path(), ex.what());
    return datawritten;
  }
}

ssize_t dedup_device::d_read(int fd, void* data, size_t size)
{
  if (!openvol) {
    Emsg0(M_ERROR, 0, T_("Trying to write dedup volume when none are open.\n"));
    return -1;
  }

  if (openvol->fileno() != fd) {
    Emsg0(M_ERROR, 0,
          T_("Trying to write dedup volume that is not open "
             "(open = %d, trying to write = %d).\n"),
          openvol->fileno(), fd);
    return -1;
  }

  try {
    auto ret = openvol->ReadBlock(current_block(), data, size);

    if (current_block() + 1 == openvol->blockcount()) {
      SetEot();
    } else {
      ClearEot();
    }

    return ret;
  } catch (const std::exception& ex) {
    Emsg0(M_ERROR, 0,
          T_("Encountered error while trying to read from volume %s. ERR=%s\n"),
          openvol->path(), ex.what());
    return -1;
  }
}

int dedup_device::d_close(int fd)
{
  if (!openvol) {
    Emsg0(M_ERROR, 0, T_("Trying to close dedup volume when none are open.\n"));
    return -1;
  }

  if (openvol->fileno() != fd) {
    Emsg0(M_ERROR, 0,
          T_("Trying to close dedup volume that is not open "
             "(open = %d, trying to close = %d).\n"),
          openvol->fileno(), fd);
    return -1;
  }

  std::string volname = openvol->path();
  try {
    openvol.reset();
    return 0;
  } catch (const std::exception& ex) {
    Emsg0(M_ERROR, 0, T_("Could not close dedup device %s. ERR=%s\n"),
          volname.c_str(), ex.what());
    return -1;
  }
}

int dedup_device::d_ioctl(int, ioctl_req_t, char*) { return -1; }

boffset_t dedup_device::d_lseek(DeviceControlRecord*, boffset_t, int)
{
  return -1;
}

bool dedup_device::d_truncate(DeviceControlRecord* dcr)
{
  if (!openvol) {
    Emsg0(M_ERROR, 0,
          T_("Trying to truncate dedup volume when none are open.\n"));
    return false;
  }

  std::string path = openvol->path();
  struct stat s;
  if (stat(path.c_str(), &s) < 0) {
    Emsg0(M_ERROR, 0, "Could not stat %s.\n", path.c_str());
    return false;
  }

  openvol.reset();

  try {
    for (auto entry : std::filesystem::directory_iterator{path}) {
      auto filename = entry.path();
      Pmsg2(000, "Deleting %s\n", filename.c_str());
      if (int res = SecureErase(dcr->jcr, filename.c_str()); res < 0) {
        Emsg0(M_ERROR, 0, T_("Secure erase on file %s returned %d.\n"),
              filename.c_str(), res);
        return false;
      }
    }
  } catch (const std::exception& ex) {
    Emsg0(M_ERROR, 0, T_("Could not delete volume %s. ERR=%s\n"), path.c_str(),
          ex.what());
    return false;
  }

  std::error_code ec;
  if (!std::filesystem::remove(path, ec)) {
    Emsg0(M_ERROR, 0, T_("Could not delete %s. ERR=%s\n"), path.c_str(),
          ec.message().c_str());
    return false;
  }

  try {
    auto parsed = dedup::device_option_parser::parse(dev_options ?: "");
    dedup::volume::create_new(s.st_mode, path.c_str(),
                              parsed.options.blocksize);
    auto& vol
        = openvol.emplace(dedup::volume::open_type::ReadWrite, path.c_str());
    Device::fd = vol.fileno();
  } catch (const std::exception& ex) {
    Emsg0(M_ERROR, 0, T_("Could not recreate %s. ERR=%s\n"), path.c_str(),
          ex.what());
    return false;
  }

  return true;
}

std::size_t dedup_device::current_block()
{
  return block_number(file, block_num);
}

bool dedup_device::rewind(DeviceControlRecord* dcr)
{
  return Reposition(dcr, 0, 0);
}

bool dedup_device::UpdatePos(DeviceControlRecord*)
{
  // the volume itself is stateless
  // so we have nothing to do
  return true;
}

bool dedup_device::Reposition(DeviceControlRecord* dcr,
                              uint32_t rfile,
                              uint32_t rblock)
{
  if (!openvol) {
    Emsg0(M_ERROR, 0,
          T_("Trying to reposition dedup volume when none are open.\n"));
    return false;
  }
  Dmsg2(10, "file: %u -> %u; block: %u -> %u\n", file, rfile, block_num,
        rblock);
  block_num = rblock;
  file = rfile;


  if (block_number(file, block_num) == openvol->blockcount()) {
    SetEot();
  } else {
    ClearEot();
  }

  return UpdatePos(dcr);
}

bool dedup_device::eod(DeviceControlRecord* dcr)
{
  if (!openvol) {
    Emsg0(M_ERROR, 0,
          T_("Trying to move to end of dedup volume when none are open.\n"));
    return false;
  }
  auto endblock = openvol->blockcount();
  auto [rfile, rblock] = file_block(endblock);

  return Reposition(dcr, rfile, rblock);
}

bool dedup_device::d_flush(DeviceControlRecord*)
{
  if (!openvol) {
    Emsg0(M_ERROR, 0, T_("Trying to flush dedup volume when none are open.\n"));
    return false;
  }

  try {
    openvol->flush();
    return true;
  } catch (const std::exception& ex) {
    Emsg0(M_ERROR, 0, T_("Could not flush volume %s. ERR=%s\n"),
          openvol->path(), ex.what());
    (void)ex;
    return false;
  }
}

bool dedup_device::ResetOpenVolume()
{
  ASSERT(!!openvol);
  // FIXME(ssura): support secure erase command
  try {
    openvol->reset();
    return true;
  } catch (const std::exception& ex) {
    Emsg0(M_ERROR, 0,
          T_("Encountered error while trying to reset volume %s. ERR=%s\n"),
          openvol->path(), ex.what());
    return false;
  }
}

REGISTER_SD_BACKEND(dedup, dedup_device);

} /* namespace storagedaemon  */
