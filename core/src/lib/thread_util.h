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
#include <optional>
#include <condition_variable>

template <typename T> class locked {
 public:
  locked(std::mutex& mut, T& data) : lock{mut}, data(data) {}

  locked(std::unique_lock<std::mutex> lock, T& data)
      : lock{std::move(lock)}, data(data)
  {
  }

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
  std::unique_lock<std::mutex> lock;
  T& data;
};

template <typename T> class synchronized {
 public:
  template <typename... Args>
  synchronized(Args... args) : data{std::forward<Args>(args)...}
  {
  }

  locked<T> lock() { return locked{mut, data}; }

  std::optional<locked<T>> try_lock()
  {
    std::unique_lock l(mut, std::try_to_lock);
    if (l.owns_lock()) {
      return locked{std::move(l), data};
    } else {
      return std::nullopt;
    }
  }

  locked<const T> lock() const { return locked{mut, data}; }

 private:
  mutable std::mutex mut{};
  T data;
};

#endif  // BAREOS_LIB_THREAD_UTIL_H_
