/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2018-2023 Bareos GmbH & Co. KG

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

template <typename Mutex, template <typename> typename Lock, typename T>
class locked {
 public:
  locked(Mutex& mut, T& data) : lock{mut}, data(data) {}

  locked(Lock<Mutex> lock, T& data) : lock{std::move(lock)}, data(data) {}

  const T& get() const { return data; }
  T& get() { return data; }

  T* operator->() { return &data; }
  T& operator*() { return data; }

  const T* operator->() const { return &data; }
  const T& operator*() const { return data; }

  template <typename Pred> void wait(std::condition_variable& cv, Pred&& p)
  {
    cv.wait(lock, p);
  }

 private:
  Lock<Mutex> lock;
  T& data;
};

template <typename T>
using unique_locked = locked<std::mutex, std::unique_lock, T>;
template <typename T>
using shared_const_locked
    = locked<std::shared_mutex, std::shared_lock, const T>;
template <typename T>
using unique_mut_locked = locked<std::shared_mutex, std::unique_lock, T>;

template <typename T> class synchronized {
 public:
  template <typename... Args>
  synchronized(Args... args) : data{std::forward<Args>(args)...}
  {
  }

  unique_locked<T> lock() { return {mut, data}; }

  std::optional<unique_locked<T>> try_lock()
  {
    std::unique_lock l(mut, std::try_to_lock);
    if (l.owns_lock()) {
      return unique_locked<T>{std::move(l), data};
    } else {
      return std::nullopt;
    }
  }

  unique_locked<const T> lock() const { return {mut, data}; }

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

  unique_mut_locked<T> wlock() { return {mut, data}; }
  std::optional<unique_mut_locked<T>> try_wlock()
  {
    std::unique_lock l(mut, std::try_to_lock);
    if (l.owns_lock()) {
      return {std::move(l), data};
    } else {
      return std::nullopt;
    }
  }

  shared_const_locked<T> rlock() const { return {mut, data}; }
  std::optional<shared_const_locked<T>> try_rlock() const
  {
    std::shared_lock l(mut, std::try_to_lock);
    if (l.owns_lock()) {
      return {std::move(l), data};
    } else {
      return std::nullopt;
    }
  }

 private:
  mutable std::shared_mutex mut{};
  T data;
};

#endif  // BAREOS_LIB_THREAD_UTIL_H_
