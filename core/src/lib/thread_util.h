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
  locked(Mutex& t_mut, T* t_data) : lock{t_mut}, data(t_data) {}
  locked(Lock<Mutex> t_lock, T* t_data) : lock{std::move(t_lock)}, data(t_data)
  {
  }

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

  template <typename CondVar, typename P> void wait(CondVar& cv, P&& pred)
  {
    cv.wait(lock, [this, pred = std::move(pred)] { return pred(*data); });
  }

  template <typename CondVar, typename TimePoint, typename P>
  bool wait_until(CondVar& cv, TimePoint tp, P&& pred)
  {
    return cv.wait_until(
        lock, tp, [this, pred = std::move(pred)] { return pred(*data); });
  }

  template <typename CondVar, typename TimePoint>
  std::cv_status wait_until(CondVar& cv, TimePoint tp)
  {
    return cv.wait_until(lock, tp);
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


template <typename T, typename Mutex = std::mutex> class synchronized {
 public:
  using unique_locked = locked<T, Mutex, std::unique_lock>;
  using const_unique_locked = locked<const T, Mutex, std::unique_lock>;

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

  [[nodiscard]] unique_locked lock() { return {mut, &data}; }

  template <typename... Args>
  [[nodiscard]] std::optional<unique_locked> try_lock(Args... args)
  {
    static_assert(sizeof...(Args) > 0);
    std::unique_lock l(mut, std::forward<Args>(args)...);
    if (l.owns_lock()) {
      return unique_locked{std::move(l), &data};
    } else {
      return std::nullopt;
    }
  }

  [[nodiscard]] std::optional<unique_locked> try_lock()
  {
    return try_lock(std::try_to_lock);
  }

  [[nodiscard]] const_unique_locked lock() const { return {mut, &data}; }

 private:
  mutable Mutex mut{};
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
