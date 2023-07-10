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
#include <cstring>
#include <sys/mman.h>

namespace dedup::util {
static_assert(((ssize_t)(off_t)-1) < 0,
              "The code assumes that the error condition is negative when cast "
              "to ssize_t");
class raii_fd {
 public:
  raii_fd() = default;
  raii_fd(const char* path_, int flags_, int mode_)
      : path{path_}
      , flags{flags_}
      , mode{mode_}
      , fd{::open(path.c_str(), flags, mode)}
      , error{fd < 0}
  {
  }

  raii_fd(int dird, const char* path_, int flags_, int mode_)
      : path{path_}
      , flags{flags_}
      , mode{mode_}
      , fd{::openat(dird, path.c_str(), flags, mode)}
      , error{fd < 0}
  {
  }

  raii_fd(raii_fd&& move_from) : raii_fd{} { *this = std::move(move_from); }

  raii_fd& operator=(raii_fd&& move_from)
  {
    std::swap(fd, move_from.fd);
    std::swap(flags, move_from.flags);
    std::swap(mode, move_from.mode);
    std::swap(error, move_from.error);
    std::swap(path, move_from.path);
    return *this;
  }

  bool is_ok() const { return !(fd < 0 || error); }

  int get() const { return fd; }

  bool flush() { return ::fsync(fd) == 0; }

  bool resize(std::size_t new_size) { return ::ftruncate(fd, new_size) == 0; }

  bool write_at(std::size_t offset, const void* data, std::size_t size)
  {
    ssize_t res = ::pwrite(fd, data, size, offset);

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

  bool write(const void* data, std::size_t size)
  {
    ssize_t res = ::write(fd, data, size);
    if (res < 0) {
      error = true;
      return false;
    }

    if (static_cast<std::size_t>(res) != size) { return false; }

    return true;
  }

  bool read_at(std::size_t offset, void* data, std::size_t size) const
  {
    ssize_t res = ::pread(fd, data, size, offset);

    if (res < 0) {
      error = true;
      return false;
    }

    if (static_cast<std::size_t>(res) != size) { return false; }

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

  // bool seek(std::size_t where)
  // {
  //   ssize_t res = ::lseek(fd, where, SEEK_SET);
  //   if (res < 0) { return false; }
  //   return static_cast<std::size_t>(res) == where;
  // }

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

  const char* relative_path() const { return path.c_str(); }

 private:
  std::string path{};
  int flags{};
  int mode{};
  int fd{-1};
  mutable bool error{true};
};

template <typename T> class file_based_array {
  using needs_to_be_trivially_copyable
      = std::enable_if_t<std::is_trivially_copyable_v<T>, bool>;

 public:
  file_based_array() = default;
  file_based_array(raii_fd file, std::size_t used);
  file_based_array(file_based_array&& other) : file_based_array{}
  {
    *this = std::move(other);
  }
  ~file_based_array();
  file_based_array& operator=(file_based_array&& other);

  std::optional<std::size_t> reserve(std::size_t count);
  std::optional<std::size_t> write(const T* arr, std::size_t count);
  std::optional<std::size_t> write_at(std::size_t start,
                                      const T* arr,
                                      std::size_t count);
  inline std::optional<std::size_t> write(const T& val)
  {
    return write(&val, 1);
  }

  inline std::optional<std::size_t> write_at(std::size_t start, const T& val)
  {
    return write_at(start, &val, 1);
  }

  bool read(T* arr, std::size_t count = 1);
  bool read_at(std::size_t start, T* arr, std::size_t count = 1) const;
  bool peek(T* arr, std::size_t count = 1);

  bool move_to(std::size_t start);

  bool flush()
  {
    if (error) { return false; }
    // if we used a cache we would write it out here
    msync(memory, cap * elem_size, MS_SYNC);
    return file.flush();
  }

  inline void clear()
  {
    used = 0;
    iter = 0;
  }

  inline std::size_t size() const { return used; }
  inline std::size_t capacity() const { return cap; }

  inline std::size_t current() const { return iter; }

  inline bool is_ok() const { return !error && file.is_ok(); }

  static constexpr std::size_t elem_size = sizeof(T);

  const raii_fd& backing_file() const { return file; }

 private:
  std::size_t used{0};
  std::size_t cap;
  std::size_t iter{0};
  raii_fd file{};
  T* memory{nullptr};
  bool error{true};

  std::optional<std::size_t> reserve_at(std::size_t at, std::size_t count);
};

template <typename T>
std::optional<std::size_t> file_based_array<T>::reserve(std::size_t count)
{
  std::optional start = reserve_at(iter, count);

  if (start.has_value()) { iter = used; }

  return start;
}

template <typename T>
std::optional<std::size_t> file_based_array<T>::reserve_at(std::size_t at,
                                                           std::size_t count)
{
  if (error) { return std::nullopt; }

  if (at + count < at) {
    return std::nullopt;  // make sure nothing weird is going on
  }

  if (at > used) {
    // since this is an internal function
    // this should never happen.  So if it does set the error flag
    error = true;
    return std::nullopt;
  }

  if (at + count > cap) { return std::nullopt; }

  used = std::max(used, at + count);
  return at;
}

template <typename T>
std::optional<std::size_t> file_based_array<T>::write(const T* arr,
                                                      std::size_t count)
{
  std::optional start = reserve_at(iter, count);

  if (!start) { return std::nullopt; }

  ASSERT(start == iter);
  auto old_iter = iter;
  iter += count;
  // write_at always returns to the current iter
  // if we set iter to the new desired position
  // we prevent the double seek we would need to do otherwise
  auto res = write_at(old_iter, arr, count);

  if (!res) { iter = old_iter; }

  return res;
}

template <typename T>
std::optional<std::size_t> file_based_array<T>::write_at(std::size_t start,
                                                         const T* arr,
                                                         std::size_t count)
{
  if (error) { return std::nullopt; }

  if (start > used) { return std::nullopt; }

  std::memcpy(memory + start, arr, count * elem_size);

  return start;
}

template <typename T> bool file_based_array<T>::read(T* arr, std::size_t count)
{
  if (error) { return false; }

  auto old_iter = iter;
  iter += count;
  // read_at always returns to the current iter
  // if we set iter to the new desired position
  // we prevent the double seek we would need to do otherwise
  bool success = read_at(old_iter, arr, count);

  if (!success) { iter = old_iter; }

  return success;
}

template <typename T>
bool file_based_array<T>::read_at(std::size_t start,
                                  T* arr,
                                  std::size_t count) const
{
  if (error) { return false; }

  if (start + count > used) { return false; }

  // todo: should this be std::copy_n ?
  std::memcpy(arr, memory + start, count * elem_size);

  return true;
}

template <typename T> bool file_based_array<T>::peek(T* arr, std::size_t count)
{
  if (error) { return false; }
  return read_at(iter, arr, count);
}

template <typename T> bool file_based_array<T>::move_to(std::size_t start)
{
  if (error) { return false; }

  if (start > used) { return false; }

  if (iter == start) { return true; }

  iter = start;

  return true;
}

template <typename T>
file_based_array<T>::file_based_array(raii_fd file_, std::size_t used_)
    : used{used_}, cap{0}, file{std::move(file_)}, error{!file.is_ok()}
{
  if (error) { return; }

  // let us compute the capacity

  std::optional size = file.size_then_reset();

  if (!size) {
    error = true;
    return;
  }

  cap = *size / elem_size;

  if (used > cap) {
    error = true;
    return;
  }

  // todo: use the permissions from file here somehow
  void* ptr = mmap(nullptr, cap * elem_size, PROT_READ | PROT_WRITE, MAP_SHARED,
                   file.get(), 0);

  if (ptr == MAP_FAILED) {
    error = true;
    return;
  }

  memory = static_cast<T*>(ptr);
}

template <typename T> file_based_array<T>::~file_based_array()
{
  if (memory) {
    flush();
    munmap(memory, cap * elem_size);
  }
}

template <typename T>
file_based_array<T>& file_based_array<T>::operator=(file_based_array<T>&& other)
{
  std::swap(used, other.used);
  std::swap(cap, other.cap);
  std::swap(iter, other.iter);
  std::swap(file, other.file);
  std::swap(error, other.error);
  std::swap(memory, other.memory);
  return *this;
}

template <typename T> class file_based_vector {
  using needs_to_be_trivially_copyable
      = std::enable_if_t<std::is_trivially_copyable_v<T>, bool>;

 public:
  file_based_vector() = default;
  file_based_vector(raii_fd file,
                    std::size_t used,
                    std::size_t capacity_chunk_size);
  file_based_vector(file_based_vector&& other) : file_based_vector{}
  {
    *this = std::move(other);
  }
  file_based_vector& operator=(file_based_vector&& other);

  std::optional<std::size_t> reserve(std::size_t count);
  std::optional<std::size_t> push_back(const T* arr, std::size_t count);
  inline std::optional<std::size_t> push_back(const T& val)
  {
    return push_back(&val, 1);
  }

  inline bool write_at(std::size_t start, const T& val)
  {
    return write_at(start, &val, 1);
  }

  bool write_at(std::size_t start, const T* arr, std::size_t count);
  bool read_at(std::size_t start, T* arr, std::size_t count) const;

  bool flush()
  {
    if (error) { return false; }
    // if we used a cache we would write it out here
    return file.flush();
  }

  bool shrink_to_fit();

  inline void clear() { used = 0; }

  inline std::size_t size() const { return used; }

  inline bool is_ok() const { return !error && file.is_ok(); }

  const raii_fd& backing_file() const { return file; }

 private:
  std::size_t used{0};
  std::size_t capacity;
  std::size_t capacity_chunk_size{1};
  raii_fd file;
  mutable bool error{true};
  static constexpr std::size_t elem_size = sizeof(T);
};

template <typename T>
std::optional<std::size_t> file_based_vector<T>::reserve(std::size_t count)
{
  if (error) { return std::nullopt; }

  std::size_t new_used = used + count;
  if (new_used < used) {
    return std::nullopt;  // make sure nothing weird is going on
  }

  if (new_used > capacity) {
    std::size_t delta = new_used - capacity;

    // compute first mutilple of capacity_chunk_size that is greater than delta
    std::size_t num_new_items
        = ((delta + capacity_chunk_size - 1) / capacity_chunk_size)
          * capacity_chunk_size;
    ASSERT(num_new_items + capacity >= new_used);

    std::size_t new_cap = capacity + num_new_items;
    if (new_cap < new_used) {
      // something weird is going on :S
      return std::nullopt;
    }

    if (!file.resize(new_cap * elem_size)) {
      error = true;
      return std::nullopt;
    }

    capacity = new_cap;
  }

  return std::exchange(used, new_used);
}

template <typename T>
std::optional<std::size_t> file_based_vector<T>::push_back(const T* arr,
                                                           std::size_t count)
{
  std::optional start = reserve(count);

  if (!start) { return false; }

  // write_at always returns to the current iter
  // if we set iter to the new desired position
  // we prevent the double seek we would need to do otherwise
  if (!write_at(*start, arr, count)) { return std::nullopt; }

  return start;
}

template <typename T>
bool file_based_vector<T>::write_at(std::size_t start,
                                    const T* arr,
                                    std::size_t count)
{
  if (error) { return false; }

  if (start > used) { return false; }

  if (start + count > used) { return false; }

  if (!file.write_at(elem_size * start, arr, count * elem_size)) {
    error = true;
    return false;
  }

  return true;
}

template <typename T>
bool file_based_vector<T>::read_at(std::size_t start,
                                   T* arr,
                                   std::size_t count) const
{
  if (error) { return false; }

  if (start + count > used) { return false; }

  if (!file.read_at(elem_size * start, arr, count * elem_size)) {
    error = true;
    return false;
  }

  return true;
}

template <typename T>
file_based_vector<T>::file_based_vector(raii_fd file_,
                                        std::size_t used_,
                                        std::size_t capacity_chunk_size_)
    : used{used_}
    , capacity{0}
    , capacity_chunk_size(capacity_chunk_size_)
    , file{std::move(file_)}
    , error{!file.is_ok()}
{
  if (error) { return; }

  // let us compute the capacity

  std::optional size = file.size_then_reset();

  if (!size) {
    error = true;
    return;
  }

  capacity = *size / elem_size;

  if (used > capacity) {
    error = true;
    return;
  }
}

template <typename T> bool file_based_vector<T>::shrink_to_fit()
{
  return file.resize(used * elem_size);
}

template <typename T>
file_based_vector<T>& file_based_vector<T>::operator=(
    file_based_vector<T>&& other)
{
  std::swap(used, other.used);
  std::swap(capacity, other.capacity);
  std::swap(capacity_chunk_size, other.capacity_chunk_size);
  std::swap(file, other.file);
  std::swap(error, other.error);
  return *this;
}

}; /* namespace dedup::util */

#endif  // BAREOS_STORED_BACKENDS_DEDUP_UTIL_H_
