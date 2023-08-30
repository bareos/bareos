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
   Affero General Public License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.
*/
#ifndef BAREOS_LIB_THREAD_UTIL_H_
#define BAREOS_LIB_THREAD_UTIL_H_

#include <mutex>
#include <shared_mutex>
#include <optional>
#include <condition_variable>

template <typename T, typename Mutex, template <typename> typename Lock>
class locked {
 public:
  locked(Mutex& mut, T* data) : lock{mut}, data(data) {}
  locked(Lock<Mutex> lock, T* data) : lock{std::move(lock)}, data(data) {}

  locked(const locked&) = delete;
  locked& operator=(const locked&) = delete;
  locked(locked&& that) : lock(std::move(that.lock)), data(that.data)
  {
    that.data = nullptr;
  }

  locked& operator=(locked&& that)
  {
    std::swap(lock, that.lock);
    std::swap(data, that.data);
    return *this;
  }

  T& get() { return *data; }
  T& operator*() { return *data; }
  T* operator->() { return data; }

  const T& get() const { return *data; }
  const T& operator*() const { return *data; }
  const T* operator->() const { return data; }

  template <typename Pred> void wait(std::condition_variable& cv, Pred&& p)
  {
    cv.wait(lock, [this, p = std::move(p)] { return p(*data); });
  }

 private:
  Lock<Mutex> lock;
  T* data;
};

template <typename T>
using unique_locked = locked<T, std::mutex, std::unique_lock>;
template <typename T>
using read_locked = locked<const T, std::shared_mutex, std::shared_lock>;
template <typename T>
using write_locked = locked<T, std::shared_mutex, std::unique_lock>;

template <typename T> class synchronized {
 public:
  template <typename... Args>
  synchronized(Args... args) : data{std::forward<Args>(args)...}
  {
  }

  ~synchronized()
  {
    /* obviously nobody should hold the lock while this object is getting
     * destroyed, but we still need to ensure that the thread that
     * is destroying this object has a synchronized view of the contained data
     * so that data's destructor can run with no race conditions. */
    std::unique_lock _{mut};
  }

  [[nodiscard]] unique_locked<T> lock() { return {mut, &data}; }

  [[nodiscard]] std::optional<unique_locked<T>> try_lock()
  {
    std::unique_lock l(mut, std::try_to_lock);
    if (l.owns_lock()) {
      return unique_locked<T>{std::move(l), &data};
    } else {
      return std::nullopt;
    }
  }

  [[nodiscard]] unique_locked<const T> lock() const { return {mut, &data}; }

 private:
  mutable std::mutex mut{};
  T data;
};

template <typename T> class rw_synchronized {
 public:
  template <typename... Args>
  rw_synchronized(Args... args) : data{std::forward<Args>(args)...}
  {
  }

  [[nodiscard]] write_locked<T> wlock() { return {mut, &data}; }
  [[nodiscard]] std::optional<write_locked<T>> try_wlock()
  {
    std::unique_lock l(mut, std::try_to_lock);
    if (l.owns_lock()) {
      return {std::move(l), &data};
    } else {
      return std::nullopt;
    }
  }

  [[nodiscard]] read_locked<T> rlock() const { return {mut, &data}; }
  [[nodiscard]] std::optional<read_locked<T>> try_rlock() const
  {
    std::shared_lock l(mut, std::try_to_lock);
    if (l.owns_lock()) {
      return {std::move(l), &data};
    } else {
      return std::nullopt;
    }
  }

 private:
  mutable std::shared_mutex mut{};
  T data;
};

#endif  // BAREOS_LIB_THREAD_UTIL_H_
