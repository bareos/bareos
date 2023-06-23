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

#ifndef BAREOS_STORED_BACKENDS_DEDUP_UTIL_H_
#define BAREOS_STORED_BACKENDS_DEDUP_UTIL_H_

#include <fcntl.h>
#include <unistd.h>
#include <memory>
#include <utility>

namespace dedup::util {
static_assert(((ssize_t)(off_t)-1) < 0,
              "The code assumes that the error condition is negative when cast "
              "to ssize_t");
class raii_fd {
 public:
  raii_fd() = default;
  raii_fd(const char* path, int flags, int mode) : flags{flags}, mode{mode}
  {
    fd = ::open(path, flags, mode);
  }

  raii_fd(int dird, const char* path, int flags, int mode)
      : flags{flags}, mode{mode}
  {
    fd = openat(dird, path, flags, mode);
  }

  raii_fd(raii_fd&& move_from) : raii_fd{} { *this = std::move(move_from); }

  raii_fd& operator=(raii_fd&& move_from)
  {
    std::swap(fd, move_from.fd);
    std::swap(flags, move_from.flags);
    std::swap(mode, move_from.mode);
    return *this;
  }

  bool is_ok() const { return !(fd < 0) && error; }

  int get() const { return fd; }

  bool flush() { return ::fsync(fd) == 0; }

  bool resize(std::size_t new_size) { return ::ftruncate(fd, new_size) == 0; }

  bool write(const void* data, std::size_t size)
  {
    ssize_t res = ::write(fd, data, size);
    if (res < 0) {
      error = true;
      return false;
    }

    if (static_cast<std::size_t>(res) != size) {
      error = true;
      return false;
    }

    return true;
  }

  bool read(void* data, std::size_t size)
  {
    ssize_t res = ::read(fd, data, size);
    if (res < 0) {
      error = true;
      return false;
    }

    if (static_cast<std::size_t>(res) != size) {
      error = true;
      return false;
    }

    return true;
  }

  bool seek(std::size_t where)
  {
    ssize_t res = ::lseek(fd, where, SEEK_SET);
    if (res < 0) { return false; }
    return static_cast<std::size_t>(res) == where;
  }

  std::optional<std::size_t> size_then_reset()
  {
    ssize_t size = ::lseek(fd, 0, SEEK_END);
    if (size < 0) {
      error = true;
      return std::nullopt;
    }
    if (::lseek(fd, 0, SEEK_SET) != 0) {
      error = true;
      return std::nullopt;
    }
    return static_cast<std::size_t>(size);
  }

  ~raii_fd()
  {
    if (fd >= 0) { close(fd); }
  }

 private:
  int fd{-1};
  int flags{};
  int mode{};
  bool error{false};
};
}; /* namespace dedup::util */

#endif  // BAREOS_STORED_BACKENDS_DEDUP_UTIL_H_
