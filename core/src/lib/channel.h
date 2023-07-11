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

#ifndef BAREOS_LIB_CHANNEL_H_
#define BAREOS_LIB_CHANNEL_H_

#include <condition_variable>
#include <optional>
#include <vector>
#include <utility>

namespace channel {
// a simple single consumer/ single producer queue
// Its composed of three parts: the input, the output and the
// actual queue data.
// Instead of directly interacting with the queue itself you instead
// interact with either the input or the output.
// This ensures that there is only one producer (who writes to the input)
// and one consumer (who reads from the output).
template <typename T> struct in;

template <typename T> struct out;

template <typename T> struct data {
  // this contains the actual queue that the in and
  // out channel use to communicate
  friend struct in<T>;
  friend struct out<T>;

  data(std::size_t capacity);

 private:
  std::unique_lock<std::mutex> wait_for_readable();
  std::unique_lock<std::mutex> wait_for_writable();

  std::size_t size;
  const std::size_t capacity;

  // this should become a span once we switch to c++20
  std::vector<T> storage;

  std::mutex mutex;
  std::condition_variable cv;

  bool in_alive;
  bool out_alive;
};

template <typename T> struct out {
  std::optional<T> get();
  std::optional<T> try_get();
  std::optional<std::vector<T>> get_all();
  void close();
  ~out();

  out(std::shared_ptr<data<T>> shared);
  out(const out&) = delete;
  out& operator=(const out&) = delete;

  out(out&& moved) = default;
  out& operator=(out&& moved) = default;

  bool empty() const { return closed; }

 private:
  T read_unlocked();
  std::shared_ptr<data<T>> shared;
  std::size_t read_pos;
  bool closed;
};

template <typename T> struct in {
  bool put(const T& val);
  bool put(T&& val);
  bool try_put(T& val);
  void close();
  ~in();
  in(std::shared_ptr<data<T>> shared);
  in(const in&) = delete;
  in& operator=(const in&) = delete;

  in(in&& moved) = default;
  in& operator=(in&& moved) = default;
  void wait_till_empty();

 private:
  void write_unlocked(T&& val);
  void write_unlocked(const T& val);
  std::shared_ptr<data<T>> shared;
  std::size_t write_pos;
  bool closed;
};

template <typename T>
std::pair<in<T>, out<T>> CreateBufferedChannel(std::size_t capacity);

static inline std::size_t wrapping_inc(std::size_t num, std::size_t max)
{
  std::size_t result = num + 1;
  if (result == max) { result = 0; }
  return result;
}

template <typename T>
data<T>::data(std::size_t capacity)
    : size(0)
    , capacity(capacity)
    , storage(capacity)
    , mutex()
    , cv()
    , in_alive(false)
    , out_alive(false)
{
}

template <typename T> T out<T>::read_unlocked()
{
  shared->size -= 1;
  auto old = read_pos;
  read_pos = wrapping_inc(read_pos, shared->capacity);
  return std::move(shared->storage[old]);
}

template <typename T> std::unique_lock<std::mutex> data<T>::wait_for_readable()
{
  std::unique_lock lock(mutex);
  cv.wait(lock,
          // only wake up if either there is
          // something in the queue, or
          // the in announced his death
          [this] { return this->size > 0 || !this->in_alive; });

  return lock;
}

template <typename T> std::optional<T> out<T>::get()
{
  if (closed) return std::nullopt;
  std::optional<T> result = std::nullopt;
  {
    auto lock = shared->wait_for_readable();

    if (this->shared->size > 0) {
      result = std::move(read_unlocked());
    } else {
      // if the in is dead and the queue is empty we also close
      shared->out_alive = false;
      closed = true;
    }
  }
  shared->cv.notify_one();
  return result;
}

template <typename T> std::optional<std::vector<T>> out<T>::get_all()
{
  if (closed) return std::nullopt;
  std::optional<std::vector<T>> result{std::nullopt};
  auto lock = shared->wait_for_readable();

  if (shared->size > 0) {
    std::vector<T>& v = result.emplace();
    v.reserve(shared->size);
    while (shared->size > 0) { v.emplace_back(std::move(read_unlocked())); }
  } else {
    shared->out_alive = false;
    closed = true;
  }
  shared->cv.notify_one();

  return result;
}

template <typename T> std::optional<T> out<T>::try_get()
{
  if (closed) return std::nullopt;
  std::optional<T> result = std::nullopt;
  bool had_lock = false;
  if (std::unique_lock lock(shared->mutex, std::try_to_lock);
      lock.owns_lock()) {
    if (this->shared->size > 0) {
      result = std::move(read_unlocked());
    } else if (!shared->in_alive) {
      // if the in is dead and the queue is empty we also close
      shared->out_alive = false;
      closed = true;
    }
    had_lock = true;
  }
  // only notify waiting threads if we actually did something to
  // the shared state!
  if (had_lock) shared->cv.notify_one();
  return result;
}

template <typename T> void out<T>::close()
{
  if (closed) return;

  {
    std::unique_lock lock(shared->mutex);
    shared->out_alive = false;
    closed = true;
  }
  shared->cv.notify_one();
}

template <typename T> out<T>::~out()
{
  if (shared) close();
}

template <typename T>
out<T>::out(std::shared_ptr<data<T>> shared_)
    : shared(shared_), read_pos(0), closed(false)
{
  std::unique_lock lock(shared->mutex);
  shared->out_alive = true;
}

template <typename T> void in<T>::write_unlocked(T&& val)
{
  shared->storage[write_pos] = std::move(val);
  shared->size += 1;
  write_pos = wrapping_inc(write_pos, shared->capacity);
}

template <typename T> void in<T>::write_unlocked(const T& val)
{
  shared->storage[write_pos] = val;
  shared->size += 1;
  write_pos = wrapping_inc(write_pos, shared->capacity);
}

template <typename T> bool in<T>::try_put(T& val)
{
  if (closed) return false;
  bool success = false;
  bool updated = false;

  if (std::unique_lock lock(shared->mutex, std::try_to_lock);
      lock.owns_lock()) {
    if (!shared->out_alive) {
      shared->in_alive = false;
      closed = true;
      updated = true;
    } else if (shared->size < shared->capacity) {
      write_unlocked(std::move(val));
      success = true;
      updated = true;
    }
  }

  if (updated) shared->cv.notify_one();
  return success;
}

template <typename T> std::unique_lock<std::mutex> data<T>::wait_for_writable()
{
  std::unique_lock lock(mutex);
  cv.wait(lock,
          [this] { return this->size < this->capacity || !this->out_alive; });
  return lock;
}

template <typename T> bool in<T>::put(const T& val)
{
  if (closed) return false;
  bool success = false;
  auto lock = shared->wait_for_writable();

  if (shared->out_alive) {
    // since the out is still alive, we know that
    // there is some space free in the storage
    // (otherwise we would still be stuck waiting!)
    write_unlocked(val);
    success = true;
  } else {
    shared->in_alive = false;
    closed = true;
  }

  shared->cv.notify_one();
  return success;
}

template <typename T> bool in<T>::put(T&& val)
{
  if (closed) return false;
  bool success = false;
  auto lock = shared->wait_for_writable();

  if (shared->out_alive) {
    // since the out is still alive, we know that
    // there is some space free in the storage
    write_unlocked(std::move(val));
    success = true;
  } else {
    shared->in_alive = false;
    closed = true;
  }

  shared->cv.notify_one();
  return success;
}

template <typename T> void in<T>::wait_till_empty()
{
  if (closed) return;

  {
    std::unique_lock lock(shared->mutex);
    shared->cv.wait(lock, [this] {
      return this->shared->size == 0 || !this->shared->out_alive;
    });

    if (!shared->out_alive) {
      shared->in_alive = false;
      closed = true;
    }
  }
  shared->cv.notify_one();
}

template <typename T> void in<T>::close()
{
  if (closed) return;

  {
    std::unique_lock lock(shared->mutex);
    shared->in_alive = false;
    closed = true;
  }
  shared->cv.notify_one();
}

template <typename T> in<T>::~in()
{
  if (shared) { close(); }
}

template <typename T>
in<T>::in(std::shared_ptr<data<T>> shared_)
    : shared(shared_), write_pos(0), closed(false)
{
  std::unique_lock lock(shared->mutex);
  shared->in_alive = true;
}

template <typename T>
std::pair<in<T>, out<T>> CreateBufferedChannel(std::size_t capacity)
{
  if (capacity == 0) {
    Dmsg0(100,
          "Tried to create a channel with zero capacity.  This will cause "
          "deadlocks."
          "  Setting capacity to 1.");
    capacity = 1;
  }
  std::shared_ptr<data<T>> shared = std::make_shared<data<T>>(capacity);
  in<T> in(shared);
  out<T> out(shared);
  return {std::move(in), std::move(out)};
}
}  // namespace channel

#endif  // BAREOS_LIB_CHANNEL_H_
