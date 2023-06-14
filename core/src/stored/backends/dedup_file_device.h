/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2023-2023 Bareos GmbH & Co. KG

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

#ifndef BAREOS_STORED_BACKENDS_DEDUP_FILE_DEVICE_H_
#define BAREOS_STORED_BACKENDS_DEDUP_FILE_DEVICE_H_

#include "stored/dev.h"
#include "lib/network_order.h"

#include <unordered_map>
#include <fcntl.h>

namespace storagedaemon {

class raii_fd {
 public:
  raii_fd(const char* path, int flags, int mode) : flags{flags}, mode{mode}
  {
    fd = open(path, flags, mode);
  }

  raii_fd(int dird, const char* path, int flags, int mode)
      : flags{flags}, mode{mode}
  {
    fd = openat(dird, path, flags, mode);
  }
  raii_fd(int fd = -1, int flags = 0, int mode = 0)
      : fd{fd}, flags{flags}, mode{mode}
  {
  }

  raii_fd(raii_fd&& move_from) : raii_fd{} { *this = std::move(move_from); }

  raii_fd& operator=(raii_fd&& move_from)
  {
    std::swap(fd, move_from.fd);
    std::swap(flags, move_from.flags);
    std::swap(mode, move_from.mode);
    return *this;
  }

  bool is_ok() const { return !(fd < 0); }

  int get() const { return fd; }

  int get_flags() const { return flags; }

  int get_mode() const { return mode; }

  ~raii_fd()
  {
    if (is_ok()) { close(fd); }
  }

 private:
  int fd{-1};
  int flags;
  int mode;
};

struct dedup_volume_file
{
  std::string path{};
  raii_fd fd{};

  dedup_volume_file() = default;
  dedup_volume_file(std::string_view path) : path{path}
  {
  }

  bool open_inside(raii_fd& dir, int mode, DeviceMode dev_mode) {
    int flags;
    switch (dev_mode) {
    case DeviceMode::CREATE_READ_WRITE: {
      flags = O_CREAT | O_RDWR | O_BINARY;
    } break;
    case DeviceMode::OPEN_READ_WRITE: {
      flags = O_RDWR | O_BINARY;
    } break;
    case DeviceMode::OPEN_READ_ONLY: {
      flags = O_RDONLY | O_BINARY;
    } break;
    case DeviceMode::OPEN_WRITE_ONLY: {
      flags = O_WRONLY | O_BINARY;
    } break;
    default: {
      return false;
    }
    }
    fd = raii_fd(dir.get(), path.c_str(), flags, mode);
    return is_ok();
  }

  bool truncate() {
    if (ftruncate(fd.get(), 0) != 0) {
      return false;
    }
    return true;
  }

  bool is_ok() const {
    return fd.is_ok();
  }
};

struct block_file : public dedup_volume_file
{
  block_file() = default;
  block_file(std::string_view path,
	     std::uint32_t file_index,
	     std::uint32_t s,
	     std::uint32_t e) : dedup_volume_file(path)
			      , file_index{file_index}
			      , start_block{s}
			      , end_block{e}
  {}
  std::uint32_t file_index;
  std::uint32_t start_block;
  std::uint32_t end_block;
};

struct record_file : public dedup_volume_file
{
  record_file() = default;
  record_file(std::string_view path,
	      std::uint32_t file_index,
	      std::uint32_t s,
	      std::uint32_t e) : dedup_volume_file(path)
			       , file_index{file_index}
			       , start_record{s}
			       , end_record{e}
  {}
  std::uint32_t file_index;
  std::uint32_t start_record;
  std::uint32_t end_record;
};

struct data_file : public dedup_volume_file
{
  data_file() = default;
  data_file(std::string_view path,
	    std::uint32_t file_index,
	    std::int64_t block_size) : dedup_volume_file{path}
				     , file_index{file_index}
				     , block_size{block_size}
  {}
  std::uint32_t file_index;

  static constexpr std::int64_t read_only_size = -1;
  static constexpr std::int64_t any_size = 0;
  std::int64_t block_size;
};

struct dedup_volume_config {
  std::vector<block_file> blockfiles;
  std::vector<record_file> recordfiles;
  std::vector<data_file> datafiles;

  void create_default()
  {
    blockfiles.clear();
    recordfiles.clear();
    datafiles.clear();
    blockfiles.emplace_back("block", 0, 0, 0);
    recordfiles.emplace_back("record", 0, 0, 0);
    datafiles.emplace_back("data", 0, data_file::any_size);
  }
};

class dedup_volume {
 public:
  dedup_volume(const char* path, DeviceMode dev_mode, int mode)
    : path(path)
    , configfile{"config"}
  {
    int dir_mode = 0100;
    if (dev_mode == DeviceMode::CREATE_READ_WRITE) {
      if (struct stat st; ::stat(path, &st) != -1) { error = true; return; }

      if (mkdir(path, mode | 0100) < 0) { error = true; return; }

      // to create files inside dir, we need executive permissions
      dir_mode |= 0100;
    }

    dir = raii_fd(path, O_CREAT | O_RDWR | O_BINARY | O_DIRECTORY, dir_mode);

    if (!dir.is_ok()) {
      error = true;
      return;
    }

    if (dev_mode == DeviceMode::OPEN_WRITE_ONLY) {
      // we always need to read the config file
      configfile.open_inside(dir, mode, DeviceMode::OPEN_READ_WRITE);
    } else {
      configfile.open_inside(dir, mode, dev_mode);
    }

    if (!configfile.is_ok()) {
      error = true;
      return;
    }

    if (dev_mode == DeviceMode::CREATE_READ_WRITE) {
      config.create_default();
    } else {
      if (!load_config()) {
	error = true;
	return;
      }
    }

    for (auto& blockfile : config.blockfiles) {
      if (!blockfile.open_inside(dir, mode, dev_mode)) {
	error = true;
	return;
      }
    }

    for (auto& recordfile : config.recordfiles) {
      if (!recordfile.open_inside(dir, mode, dev_mode)) {
	error = true;
	return;
      }
    }

    for (auto& datafile : config.datafiles) {
      if (!datafile.open_inside(dir, mode, dev_mode)) {
	error = true;
	return;
      }
    }
  }

  int get_record_file()
  {
    return -1;
  }

  int get_block_file()
  {
    return -1;
  }

  bool reset()
  {
    // TODO: look at unix_file_device for "secure_erase_cmdline"
    for (auto& blockfile : config.blockfiles) {
      if (!blockfile.truncate()) {
	return false;
      }
    }
    for (auto& recordfile : config.recordfiles) {
      if (!recordfile.truncate()) {
	return false;
      }
    }
    for (auto& datafile : config.datafiles) {
      if (!datafile.truncate()) {
	return false;
      }
    }
    return true;
  }

  dedup_volume(dedup_volume&&) = default;
  dedup_volume& operator=(dedup_volume&&) = default;

  ~dedup_volume() {
    if (volume_changed) {
      write_current_config();
    }

    if (error) {
      Emsg1(
          M_FATAL, 0,
          _("Error while writing dedup config.  Volume '%s' may be damaged."),
	  path.c_str());
    }
  }

  bool is_ok() const
  {
    return !error && dir.is_ok() && configfile.is_ok();
  }

  void write_current_config();
  bool load_config();
  void changed_volume() { volume_changed = true; };

 private:
  std::string path;
  raii_fd dir;
  dedup_volume_file configfile;

  dedup_volume_config config;

  bool error{false};
  bool volume_changed{false};
  int mode;
};

class dedup_file_device : public Device {
 public:
  dedup_file_device() = default;
  ~dedup_file_device() { close(nullptr); }

  // Interface from Device
  SeekMode GetSeekMode() const override { return SeekMode::FILE_BLOCK; }
  bool CanReadConcurrently() const override { return false; }
  bool MountBackend(DeviceControlRecord* dcr, int timeout) override;
  bool UnmountBackend(DeviceControlRecord* dcr, int timeout) override;
  bool ScanForVolumeImpl(DeviceControlRecord* dcr) override;
  int d_close(int) override;
  int d_open(const char* pathname, int flags, int mode) override;
  int d_ioctl(int fd, ioctl_req_t request, char* mt = NULL) override;
  boffset_t d_lseek(DeviceControlRecord* dcr,
                    boffset_t offset,
                    int whence) override;
  ssize_t d_read(int fd, void* buffer, size_t count) override;
  ssize_t d_write(int fd, const void* buffer, size_t count) override;
  bool d_truncate(DeviceControlRecord* dcr) override;
  bool rewind(DeviceControlRecord* dcr) override;
  bool UpdatePos(DeviceControlRecord* dcr) override;
  bool Reposition(DeviceControlRecord* dcr,
                  uint32_t rfile,
                  uint32_t rblock) override;
  bool eod(DeviceControlRecord* dcr) override;

 private:
  bool mounted{false};

  int fd_ctr;
  std::unordered_map<int, dedup_volume> open_volumes;
};

} /* namespace storagedaemon */

#endif  // BAREOS_STORED_BACKENDS_DEDUP_FILE_DEVICE_H_
