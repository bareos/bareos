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

#ifndef BAREOS_LIB_CHANNEL_H_
#define BAREOS_LIB_CHANNEL_H_

#include <condition_variable>
#include <optional>
#include <vector>
#include <utility>
#include <variant>

#include "thread_util.h"
#include "include/baconfig.h"

namespace channel {
// a simple single consumer/ single producer queue
// Its composed of three parts: the input, the output and the
// actual queue data.
// Instead of directly interacting with the queue itself you instead
// interact with either the input or the output.
// This ensures that there is only one producer (who writes to the input)
// and one consumer (who reads from the output).

struct channel_closed {};

template <typename T> class queue {
  struct internal {
    std::vector<T> data;
    bool in_dead;
    bool out_dead;
  };

  synchronized<internal> shared{};
  std::condition_variable in_update{};
  std::condition_variable out_update{};
  std::size_t max_size;

 public:
  explicit queue(std::size_t t_max_size) : max_size(t_max_size) {}
  queue(const queue&) = delete;
  queue& operator=(const queue&) = delete;
  queue(queue&&) = delete;
  queue& operator=(queue&&) = delete;

  using locked_type = decltype(shared.lock());

  class handle {
    locked_type locked;
    std::condition_variable* update;

   public:
    handle(locked_type t_locked, std::condition_variable* t_update)
        : locked{std::move(t_locked)}, update(t_update)
    {
    }
    handle(const handle&) = delete;
    handle& operator=(const handle&) = delete;
    handle(handle&& that) : locked{std::move(that.locked)}, update{that.update}
    {
      that.update = nullptr;
    }
    handle& operator=(handle&& that)
    {
      locked = std::move(that.locked);
      update = std::exchange(that.update, nullptr);
    }

    std::vector<T>& data() { return locked->data; }

    ~handle()
    {
      if (update) { update->notify_one(); }
    }
  };

  /* the *_lock functions return std::nullopt only if the channel is closed
   * otherwise they wait until they get the lock
   * The try_*_lock functions instead return a tristate indicating whether
   * they succeeded, failed to acquire the lock or if the channel was closed
   * from the other side. */

  using result_type = std::variant<handle, channel_closed>;

  result_type output_lock()
  {
    auto locked = shared.lock();
    ASSERT(!locked->out_dead);

    locked.wait(in_update, [](const auto& t_queue) {
      return t_queue.data.size() > 0 || t_queue.in_dead;
    });

    if (locked->data.size() == 0) {
      return channel_closed{};
    } else {
      return result_type(std::in_place_type<handle>, std::move(locked),
                         &out_update);
    }
  }

  std::optional<result_type> try_output_lock()
  {
    auto locked = shared.try_lock();
    if (!locked) { return std::nullopt; }
    ASSERT(!locked.value()->out_dead);

    if (locked.value()->data.size() == 0) {
      if (locked.value()->in_dead) {
        return channel_closed{};
      } else {
        return std::nullopt;
      }
    }

    return result_type(std::in_place_type<handle>, std::move(locked).value(),
                       &out_update);
  }

  result_type input_lock()
  {
    auto locked = shared.lock();
    locked.wait(out_update, [max_size = max_size](const auto& t_queue) {
      return t_queue.data.size() < max_size || t_queue.out_dead;
    });
    ASSERT(!locked->in_dead);

    if (locked->out_dead) {
      return channel_closed{};
    } else {
      return result_type(std::in_place_type<handle>, std::move(locked),
                         &in_update);
    }
  }

  std::optional<result_type> try_input_lock()
  {
    auto locked = shared.try_lock();
    if (!locked) { return std::nullopt; }

    ASSERT(!locked.value()->in_dead);
    if (locked.value()->out_dead) { return channel_closed{}; }
    if (locked.value()->data.size() >= max_size) { return std::nullopt; }

    return result_type(std::in_place_type<handle>, std::move(locked).value(),
                       &in_update);
  }

  void close_in()
  {
    shared.lock()->in_dead = true;
    in_update.notify_one();
  }

  void close_out()
  {
    shared.lock()->out_dead = true;
    out_update.notify_one();
  }
};

template <typename T> class input {
  std::shared_ptr<queue<T>> shared;
  bool did_close{false};
  using handle_type = typename queue<T>::handle;
  using result_type = typename queue<T>::result_type;

 public:
  explicit input(std::shared_ptr<queue<T>> t_shared)
      : shared{std::move(t_shared)}
  {
  }
  // after move only operator= and ~in are allowed to be called
  input(input&&) = default;
  input& operator=(input&&) = default;
  input(const input&) = delete;
  input& operator=(const input&) = delete;

  template <typename... Args> bool emplace(Args... args)
  {
    if (did_close) { return false; }
    auto result = shared->input_lock();
    return do_emplace(result, std::forward<Args>(args)...);
  }

  template <typename... Args> bool try_emplace(Args... args)
  {
    if (did_close) { return false; }

    if (auto result = shared->try_input_lock()) {
      return do_emplace(result.value(), std::forward<Args>(args)...);
    }

    return false;
  }

  void try_update_status()
  {
    if (did_close) { return; }

    bool should_close = false;
    if (auto result = shared->try_input_lock()) {
      if (std::get_if<channel_closed>(&result.value())) { should_close = true; }
    }

    if (should_close) { close(); }
  }

  void close()
  {
    if (!did_close) {
      shared->close_in();
      did_close = true;
    }
  }

  bool closed() const { return did_close; }

  ~input()
  {
    if (shared) { close(); }
  }

 private:
  template <typename... Args>
  inline bool do_emplace(result_type& result, Args... args)
  {
    return std::visit(
        [this, &args...](auto&& val) {
          using val_type = std::decay_t<decltype(val)>;
          if constexpr (std::is_same_v<val_type, channel_closed>) {
            close();
            return false;
          } else if constexpr (std::is_same_v<val_type, handle_type>) {
            val.data().emplace_back(std::forward<Args>(args)...);
            return true;
          } else {
            static_assert("Type not handled");
          }
        },
        result);
  }
};

template <typename T> class output {
  std::shared_ptr<queue<T>> shared;
  std::vector<T> cache{};
  typename decltype(cache)::iterator cache_iter = cache.begin();
  bool did_close{false};
  using handle_type = typename queue<T>::handle;
  using result_type = typename queue<T>::result_type;

  enum class with_lock
  {
    No,
    Yes,
  };

 public:
  explicit output(std::shared_ptr<queue<T>> t_shared)
      : shared{std::move(t_shared)}
  {
  }
  // after move only operator= and ~out are allowed to be called
  output(output&&) = default;
  output& operator=(output&&) = default;
  output(const output&) = delete;
  output& operator=(const output&) = delete;

  std::optional<T> get() { return get_internal(with_lock::Yes); }

  std::optional<T> try_get() { return get_internal(with_lock::No); }

  void close()
  {
    if (!did_close) {
      cache.clear();
      cache_iter = cache.begin();
      shared->close_out();
      did_close = true;
    }
  }

  bool closed() const { return did_close; }

  ~output()
  {
    if (shared) { close(); }
  }

 private:
  std::optional<T> get_internal(with_lock lock)
  {
    if (did_close) { return std::nullopt; }
    update_cache(lock);

    if (cache_iter != cache.end()) {
      std::optional result = std::make_optional<T>(std::move(*cache_iter++));
      return result;
    } else {
      return std::nullopt;
    }
  }

  inline void do_update_cache(result_type& result)
  {
    std::visit(
        [this](auto&& val) {
          using val_type = std::decay_t<decltype(val)>;
          if constexpr (std::is_same_v<val_type, channel_closed>) {
            close();
          } else if constexpr (std::is_same_v<val_type, handle_type>) {
            cache.clear();
            std::swap(cache, val.data());
            cache_iter = cache.begin();
          } else {
            static_assert("Type not handled");
          }
        },
        result);
  }

  void update_cache(with_lock lock)
  {
    if (cache_iter == cache.end()) {
      using handle_t = typename queue<T>::handle;
      std::optional<handle_t> handle;
      if (lock == with_lock::No) {
        if (auto result = shared->try_output_lock()) {
          do_update_cache(result.value());
        }
      } else {
        auto result = shared->output_lock();
        do_update_cache(result);
      }
    }
  }
};

template <typename T> using channel_pair = std::pair<input<T>, output<T>>;

template <typename T>
channel_pair<T> CreateBufferedChannel(std::size_t capacity)
{
  auto shared = std::make_shared<queue<T>>(capacity);
  auto in = input(shared);
  auto out = output(shared);
  return {std::move(in), std::move(out)};
}
}  // namespace channel

#endif  // BAREOS_LIB_CHANNEL_H_
