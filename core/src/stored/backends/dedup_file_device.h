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
  raii_fd(const char* path, int flags, int mode) : flags{flags}
						 , mode{mode}
  {
    fd = open(path, flags, mode);
  }

  raii_fd(int dird, const char* path, int flags, int mode) : flags{flags}
							   , mode{mode}
  {
    fd = openat(dird, path, flags, mode);
  }
  raii_fd(int fd = -1, int flags = 0, int mode = 0) : fd{fd}
						    , flags{flags}
						    , mode{mode}
  {
  }

  raii_fd(raii_fd&& move_from) : raii_fd{}
  {
    *this = std::move(move_from);
  }

  raii_fd& operator=(raii_fd&& move_from)
  {
    std::swap(fd, move_from.fd);
    std::swap(flags, move_from.flags);
    std::swap(mode, move_from.mode);
    return *this;
  }

  bool is_ok() const
  {
    return !(fd < 0);
  }

  int get() const
  {
    return fd;
  }

  int get_flags() const
  {
    return flags;
  }

  int get_mode() const
  {
    return mode;
  }

  ~raii_fd()
  {
    if (is_ok()) {
      close(fd);
    }
  }
private:
  int fd{-1};
  int flags;
  int mode;
};

class dedup_volume {
public:
  dedup_volume(const char* path, int flags, int mode)
    : dir{path, O_DIRECTORY | O_RDONLY, mode | ((flags & O_CREAT) ? 0100 : 0)}
    , block{dir.get(), "block", flags, mode}
    , record{dir.get(), "record", flags, mode}
    , data{dir.get(), "data", flags, mode}
  {
  }

  dedup_volume(dedup_volume&&) = default;
  dedup_volume& operator=(dedup_volume&&) = default;

  bool is_ok() const
  {
    return dir.is_ok() && block.is_ok() && record.is_ok() && data.is_ok();
  }

  int get_dir()
  {
    return dir.get();
  }

  int get_block()
  {
    return block.get();
  }

  int get_record()
  {
    return record.get();
  }

  int get_data()
  {
    return data.get();
  }

private:
  raii_fd dir;
  raii_fd block;
  raii_fd record;
  raii_fd data;
};

class dedup_file_device : public Device {
 public:
  dedup_file_device() = default;
  ~dedup_file_device() { close(nullptr); }

  // Interface from Device
  SeekMode GetSeekMode() const override { return SeekMode::FILE_BLOCK; }
  bool CanReadConcurrently() const override { return true; }
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
  bool Reposition(DeviceControlRecord* dcr, uint32_t rfile, uint32_t rblock) override;
  bool eod(DeviceControlRecord* dcr) override;
 private:
  bool mounted{false};

  int fd_ctr;
  std::unordered_map<int, dedup_volume> open_volumes;
};

} /* namespace storagedaemon */

#endif  // BAREOS_STORED_BACKENDS_DEDUP_FILE_DEVICE_H_
