/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2023-2023 Bareos GmbH & Co. KG

   This program is Free Software; you can modify it under the terms of
   version three of the GNU Affero General Public License as published by the
   Free Software Foundation, which is listed in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   Affero General Public License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.
*/

#ifndef BAREOS_STORED_BACKENDS_DEDUP_FVEC_H_
#define BAREOS_STORED_BACKENDS_DEDUP_FVEC_H_

#include <cstdlib>
#include <stdexcept>

extern "C" {
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
}

template <typename T> class fvec {
 public:
  using size_type = std::size_t;
  using value_type = T;
  using reference = T&;
  using const_reference = const T&;
  using pointer = T*;
  using const_pointer = const T*;
  using iterator = T*;
  using const_iterator = const T*;

  static constexpr size_type element_size = sizeof(T);
  static constexpr auto element_align = alignof(T);

  fvec(int fd, size_type initial_size = 0) : count{initial_size}, fd{fd}
  {
    static_assert(element_align <= 4096, "Alignment too big.");
    struct stat s;
    if (fstat(fd, &s) != 0) { throw error("fstat"); }

    cap = s.st_size / element_size;

    if (count > cap) {
      throw std::runtime_error("size > capacity (" + std::to_string(count)
                               + " > " + std::to_string(cap) + ")");
    }

    if (cap == 0) {
      auto new_cap = 1024;
      grow_file(new_cap * element_size);
      cap = new_cap;
    }

    buffer
        = reinterpret_cast<T*>(mmap(nullptr, cap * element_size,
                                    PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0));
    if (buffer == MAP_FAILED) { throw error("mmap"); }
    if (buffer == nullptr) {
      // this should not happen
      throw std::runtime_error("mmap returned nullptr.");
    }
  }

  reference operator[](size_type idx) { return buffer[idx]; }

  const_reference operator[](size_type idx) const { return buffer[idx]; }

  template <typename... Args> reference emplace_back(Args&&... args)
  {
    if (count >= cap) {
      // grow by ~50% each time
      reserve(cap + (cap >> 1) + 1);
    }
    new (&buffer[count]) T(std::forward<Args>(args)...);
    count += 1;
    return buffer[count - 1];
  }

  void push_back(const T& val) { static_cast<void>(emplace_back(val)); }

  void push_back(T&& val) { static_cast<void>(emplace_back(std::move(val))); }

  size_type size() const { return count; }

  size_type max_size() const { return std::numeric_limits<size_type>::max(); }

  size_type capacity() const { return cap; }

  pointer data() { return buffer; }

  const_pointer data() const { return buffer; }

  bool empty() const { return count == 0; }

  iterator begin() { return &buffer[0]; }

  iterator end() { return &buffer[count]; }

  const_iterator begin() const { return &buffer[0]; }

  const_iterator end() const { return &buffer[count]; }

  void reserve(size_type new_cap)
  {
    if (new_cap <= cap) { return; }

    grow_file(new_cap * element_size);

    auto res = mremap(buffer, cap * element_size, new_cap * element_size,
                      MREMAP_MAYMOVE, nullptr);

    if (res != nullptr && res != MAP_FAILED) {
      buffer = reinterpret_cast<T*>(res);
      cap = new_cap;
    } else {
      throw error("mremap");
    }
  }

  void reserve_extra(size_type additional) { reserve(additional + count); }

  void clear() { count = 0; }

  void resize_to_fit()
  {
    cap = count;
    if (ftruncate(fd, cap * element_size) != 0) { throw error("ftruncate"); }
  }

  ~fvec() { munmap(buffer, cap * element_size); }

 private:
  T* buffer{nullptr};
  size_type cap{0};
  size_type count{0};
  int fd{-1};

  template <typename... Args> static std::system_error error(Args&&... args)
  {
    return std::system_error(errno, std::generic_category(),
                             std::forward<Args>(args)...);
  }

  void grow_file(std::size_t new_size)
  {
    if (auto err = posix_fallocate(fd, 0, new_size); err != 0) {
      // posix_fallocate does not set errno
      throw std::system_error(err, std::generic_category(), "posix_fallocate");
    }
  }
};

#endif  // BAREOS_STORED_BACKENDS_DEDUP_FVEC_H_
