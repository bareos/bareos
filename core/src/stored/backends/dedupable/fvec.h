/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2023-2024 Bareos GmbH & Co. KG

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

#ifndef BAREOS_STORED_BACKENDS_DEDUPABLE_FVEC_H_
#define BAREOS_STORED_BACKENDS_DEDUPABLE_FVEC_H_

#include <cstdlib>
#include <stdexcept>

extern "C" {
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
}
#include <system_error>
#include <limits>
#include <cstring>
#include <utility>

namespace dedup {
struct access {
  static constexpr struct {
  } rdonly{};
  static constexpr struct {
  } rdwr{};

  // write only is simply not possible, since its not supported by most
  // cpus/operating systems!
  using rdonly_t = decltype(rdonly);
  using rdwr_t = decltype(rdwr);
};

namespace {
template <std::size_t Size> constexpr std::size_t MinGrowthSize()
{
  // We want to grow at least 2MiB each time.
  std::size_t min_growth_size = 1024ull * 1024ull * 2;

  return (min_growth_size + Size - 1) / Size;
}

template <std::size_t Size> constexpr std::size_t MaxGrowthSize()
{
  // We want to grow at most 10MiB each time.
  std::size_t max_growth_size = 1024ull * 1024ull * 100ull;

  return max_growth_size / Size;
}
};  // namespace

template <typename T> class fvec : access {
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


  fvec() = default;

  fvec(rdonly_t, int fd, size_type initial_size = 0)
      : fvec(fd, initial_size, PROT_READ)
  {
  }

  fvec(rdwr_t, int fd, size_type initial_size = 0)
      : fvec(fd, initial_size, PROT_READ | PROT_WRITE)
  {
  }

  fvec(bool readonly, int fd, size_type initial_size = 0)
      : fvec(fd,
             initial_size,
             readonly ? (PROT_READ) : (PROT_READ | PROT_WRITE))
  {
  }

  fvec(const fvec&) = delete;
  fvec& operator=(const fvec&) = delete;

  fvec(fvec&& other) : fvec{} { *this = std::move(other); }

  fvec& operator=(fvec&& other)
  {
    std::swap(buffer, other.buffer);
    std::swap(bytes_allocated, other.bytes_allocated);
    std::swap(count, other.count);
    std::swap(fd, other.fd);
    std::swap(prot, other.prot);

    return *this;
  }

  reference operator[](size_type idx) { return buffer[idx]; }

  const_reference operator[](size_type idx) const { return buffer[idx]; }

  template <typename... Args> reference emplace_back(Args&&... args)
  {
    auto cap = capacity();
    if (count >= cap) {
      // grow by ~50% each time
      auto add = (cap >> 1) + 1;
      if (add > max_growth_size) { add = max_growth_size; }
      reserve(cap + add);
    }
    new (&buffer[count]) T(std::forward<Args>(args)...);
    count += 1;
    return buffer[count - 1];
  }

  void push_back(const T& val) { static_cast<void>(emplace_back(val)); }

  void push_back(T&& val) { static_cast<void>(emplace_back(std::move(val))); }

  size_type size() const { return count; }

  size_type max_size() const { return std::numeric_limits<size_type>::max(); }

  size_type capacity() const { return bytes_allocated / element_size; }

  pointer data() { return buffer; }

  const_pointer data() const { return buffer; }

  bool empty() const { return count == 0; }

  iterator begin() { return &buffer[0]; }

  iterator end() { return &buffer[count]; }

  const_iterator begin() const { return &buffer[0]; }

  const_iterator end() const { return &buffer[count]; }

  void reserve(size_type min_cap)
  {
    auto min_new_size = page_aligned(min_cap * element_size);

    if (min_new_size <= bytes_allocated) { return; }

    auto diff = min_new_size - bytes_allocated;

    if (diff < min_growth_size) { diff = min_growth_size; }

    auto size = bytes_allocated;
    auto new_size = size + diff;
    grow_file(new_size);

    auto* res = MAP_FAILED;
#ifdef MREMAP_MAYMOVE
    if (buffer != nullptr) {
      res = mremap(std::exchange(buffer, nullptr), size, new_size,
                   MREMAP_MAYMOVE, nullptr);

      if (res == MAP_FAILED) {
        throw error("mremap (size = " + std::to_string(size)
                    + ", new size = " + std::to_string(new_size) + ")");
      }
      if (res == nullptr) { throw error("mremap returned nullptr."); }

      // update buffer
      buffer = reinterpret_cast<T*>(res);
    }
#endif


    // mremap is linux specific.  On other systems we
    // try to extend the mapping if possible ...

    if ((res == MAP_FAILED) && (size % page_size == 0)) {
      res = mmap(buffer + size, new_size - size, prot, MAP_SHARED | MAP_FIXED,
                 fd, size);
      // buffer already has the correct value
    }

    if (res == MAP_FAILED) {
      // ... otherwise we do an unmap + mmap

      if (munmap(std::exchange(buffer, nullptr), size) < 0) {
        throw error("munmap (size = " + std::to_string(size) + ")");
      }

      res = reinterpret_cast<T*>(
          mmap(nullptr, new_size, prot, MAP_SHARED, fd, 0));

      if (res == MAP_FAILED) {
        throw error("mmap (size = " + std::to_string(new_size)
                    + ", prot = " + std::to_string(prot)
                    + ", fd = " + std::to_string(fd) + ")");
      }
      if (res == nullptr) { throw error("mmap returned nullptr."); }

      // update buffer
      buffer = reinterpret_cast<T*>(res);
    }

    bytes_allocated = new_size;
#ifdef MADV_HUGEPAGE
    madvise(buffer, bytes_allocated, MADV_HUGEPAGE);
#endif
  }

  T* alloc_uninit(std::size_t num)
  {
    reserve_extra(num);
    count += num;

    return buffer + (count - num);
  }

  // think of (arr, size) as a span; then the name makes sense
  void append_range(const T* arr, std::size_t size)
  {
    reserve_extra(size);
    std::memcpy(end(), arr, size * element_size);
    count += size;
  }

  // does not initialize the new values if new_size > size
  void resize_uninitialized(std::size_t new_size)
  {
    // does nothing if new_size <= cap
    reserve(new_size);

    count = new_size;
  }

  void reserve_extra(size_type additional) { reserve(additional + count); }

  void clear() { count = 0; }

  void resize_to_fit()
  {
    auto size = bytes_allocated;
    auto new_size = count * element_size;
    if (size == new_size) { return; }

    if (munmap(std::exchange(buffer, nullptr), size) < 0) {
      throw error("munmap (size = " + std::to_string(size) + ")");
    }

    bytes_allocated = 0;

    if (ftruncate(fd, new_size) != 0) {
      throw error("ftruncate (new size = " + std::to_string(new_size) + ")");
    }

    if (new_size == 0) { return; }

    auto* res = reinterpret_cast<T*>(
        mmap(nullptr, new_size, prot, MAP_SHARED, fd, 0));

    if (res == MAP_FAILED) {
      throw error("mmap (size = " + std::to_string(new_size)
                  + ", prot = " + std::to_string(prot)
                  + ", fd = " + std::to_string(fd) + ")");
    }
    if (res == nullptr) { throw error("mmap returned nullptr."); }

    // update buffer
    buffer = reinterpret_cast<T*>(res);
    bytes_allocated = new_size;
  }

  void flush()
  {
    auto size = useful_bytes();
    if (msync(buffer, size, MS_SYNC) < 0) {
      throw error("msync (size = " + std::to_string(size) + ")");
    }
  }

  ~fvec()
  {
    if (buffer) { munmap(buffer, bytes_allocated); }
  }

 private:
  static constexpr size_type page_size = 4096;
  T* buffer{nullptr};
  size_type bytes_allocated{0};
  size_type count{0};
  int fd{-1};
  int prot;

  size_type page_aligned(size_type size)
  {
    auto ceil = (size + page_size - 1) / page_size;
    return ceil * page_size;
  }
  size_type useful_bytes() { return count * element_size; }

  static constexpr std::size_t min_growth_size = MinGrowthSize<element_size>();
  static constexpr std::size_t max_growth_size = MaxGrowthSize<element_size>();

  template <typename... Args> static std::system_error error(Args&&... args)
  {
    return std::system_error(errno, std::generic_category(),
                             std::forward<Args>(args)...);
  }

  void grow_file(std::size_t new_size)
  {
    if (ftruncate(fd, new_size) != 0) {
      throw error("ftruncate/allocate (new size = " + std::to_string(new_size)
                  + ")");
    }
  }

  fvec(int fd, size_type initial_size, int prot)
      : count{initial_size}, fd{fd}, prot{prot}
  {
    // we cannot ensure alignment bigger than page alignment
    static_assert(element_align <= 4096, "Alignment too big.");

    // check for unaccounted for weirdness
    static_assert(element_align > 0, "Weird struct");
    static_assert(element_align <= element_size, "Weird struct");
    static_assert(element_size % element_align == 0, "Weird struct");

    struct stat s;
    if (fstat(fd, &s) != 0) {
      throw error("fstat (fd = " + std::to_string(fd) + ")");
    }

    auto filesize = s.st_size;
    auto capacity = filesize / element_size;

    if (count > capacity) {
      throw std::runtime_error("size > capacity (" + std::to_string(count)
                               + " > " + std::to_string(capacity) + ")");
    }

    if (capacity == 0) {
      // mmap errors out if length is zero, as such we need to always ensure
      // that we can map something
      auto new_cap = 1024;
      filesize = new_cap * element_size;
      grow_file(filesize);
    }

    buffer = reinterpret_cast<T*>(
        mmap(nullptr, filesize, prot, MAP_SHARED, fd, 0));
    if (buffer == MAP_FAILED) {
      throw error("mmap (size = " + std::to_string(filesize)
                  + ", prot = " + std::to_string(prot)
                  + ", fd = " + std::to_string(fd) + ")");
    }
    if (buffer == nullptr) {
      // this should not happen
      throw std::runtime_error("mmap returned nullptr.");
    }
    bytes_allocated = filesize;
#ifdef MADV_HUGEPAGE
    madvise(buffer, bytes_allocated, MADV_HUGEPAGE);
#endif
  }
};

};  // namespace dedup


#endif  // BAREOS_STORED_BACKENDS_DEDUPABLE_FVEC_H_
