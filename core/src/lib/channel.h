#ifndef BAREOS_LIB_CHANNEL_H_
#define BAREOS_LIB_CHANNEL_H_

#include <condition_variable>
#include <optional>
#include <vector>

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
  void close();
  ~out();

  out(std::shared_ptr<data<T>> shared);
  out(const out&) = delete;
  out& operator=(out&) = delete;

  out(out&& moved) = default;
  out& operator=(out&& moved) = default;

	bool empty() const {
		return closed;
	}

 private:
  std::shared_ptr<data<T>> shared;
  std::size_t read_pos;
  std::size_t old_size;
  std::size_t capacity;
  bool closed;
};

template <typename T> struct in {
  bool put(const T& val);
  bool put(T&& val);
  void close();
  ~in();
  in(std::shared_ptr<data<T>> shared);
  in(const in&) = delete;
  in& operator=(in&) = delete;

  in(in&& moved) = default;
  in& operator=(in&& moved) = default;

 private:
  std::shared_ptr<data<T>> shared;
  std::size_t write_pos;
  std::size_t old_size;
  std::size_t capacity;
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

template <typename T> std::optional<T> out<T>::get()
{
  std::optional<T> result = std::nullopt;
  if (!closed) {
	  if (old_size > 0)
	  {
		  // take the fast path
		  result = std::move(shared->storage[read_pos]);
	  }
    std::unique_lock lock(shared->mutex);
    shared->cv.wait(
        lock,
        // only wake up if either there is
        // something in the queue, or
        // the in announced his death
        [this] { return this->shared->size > 0 || !this->shared->in_alive; });

    bool in_alive = this->shared->in_alive;
    if (this->shared->size > 0) {
	    // if we did not take the fast path, take it now
	    if (!result) { result = std::move(shared->storage[read_pos]); }
      shared->size -= 1;
      old_size = shared->size; // update the cache
      read_pos = wrapping_inc(read_pos, capacity);
    } else {
      // if the in is dead and the queue is empty we also close
      shared->out_alive = false;
      old_size = 0;
      closed = true;
    }
    shared->cv.notify_one();
    Dmsg4(1000, "size remaining: %d,"
	  " in: %s, out: %s, sucess: %d .\n",
	  old_size, in_alive ? "alive" : "dead",
	  closed ? "dead" : "alive", result.has_value());
  }
  else
  {
	  Dmsg0(1000, "channel is closed.\n");
  }
  return result;
}

template <typename T> std::optional<T> out<T>::try_get()
{
  std::optional<T> result = std::nullopt;
  if (!closed) {
	  bool something_changed = false;
	  if (old_size > 0)
	  {
		  // take the fast path
		  something_changed = true;
		  result = std::move(shared->storage[read_pos]);
	  }
	  bool had_lock = true;
	  bool in_alive = true; // only used if had_lock
	  // TODO: should we use try_lock here instead ?
	  if (std::unique_lock lock(shared->mutex); lock.owns_lock())
	  {
		  in_alive = shared->in_alive;
		  if (this->shared->size > 0) {
			  // if we did not take the fast path, take it now
			  if (!result) {
				  something_changed = true;
				  result = std::move(shared->storage[read_pos]);
			  }
			  shared->size -= 1;
			  old_size = shared->size; // update the cache
			  read_pos = wrapping_inc(read_pos, capacity);
		  } else if (!shared->in_alive) {
			  // if the in is dead and the queue is empty we also close
			  something_changed = true;
			  shared->out_alive = false;
			  old_size = 0;
			  closed = true;
		  }
	  }
	  else
	  {
		  had_lock = false;
	  }
	  if (had_lock)
	  {
		  Dmsg4(1000, "size remaining: %d, in: %s, out: %s, sucess: %d.\n",
			old_size, in_alive ? "alive" : "dead",
			closed ? "dead" : "alive",
			result.has_value());
	  }
	  else
	  {
		  Dmsg0(1000, "Failed to acquire lock.\n");
	  }
    // only notify waiting threads if we actually did something to
    // the shared state!
    if (something_changed) shared->cv.notify_one();
  }
  else
  {
	  Dmsg0(1000, "channel is closed.\n");
  }
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
	: shared(shared_), read_pos(0), old_size(0), capacity(shared_->capacity), closed(false)
{
  std::unique_lock lock(shared->mutex);
  shared->out_alive = true;
}

template <typename T> bool in<T>::put(const T& val)
{
  if (closed) return false;
  bool success = false;
  bool took_fast_path = false;
  if (old_size < capacity)  // size <= old_size is always true!
  {
    // fast path: copy first, then take the lock
    shared->storage[write_pos] = val;
    took_fast_path = true;
  }
  // slow path: take the lock, then copy
  std::unique_lock lock(shared->mutex);
  shared->cv.wait(lock, [this] {
	  return this->shared->size < this->capacity || !this->shared->out_alive;
  });

  if (shared->out_alive) {
	  // since the out is still alive, we know that
	  // there is some space free in the storage
	  if (!took_fast_path)
	  {
		  shared->storage[write_pos] = val;
	  }
	  shared->size += 1;
	  old_size = shared->size; // update the cache!
	  success = true;
  } else {
	  shared->in_alive = false;
	  old_size = this->capacity; // update the cache!
	  closed = true;
  }

  shared->cv.notify_one();
  if (success) { write_pos = wrapping_inc(write_pos, capacity); }
  return success;
}

template <typename T> bool in<T>::put(T&& val)
{
  if (closed) return false;
  bool success = false;
  bool took_fast_path = false;
  if (old_size < capacity)  // size <= old_size is always true!
  {
    // fast path: copy first, then take the lock
    shared->storage[write_pos] = std::move(val);
    took_fast_path = true;
  }
  // slow path: take the lock, then copy
  std::unique_lock lock(shared->mutex);
  shared->cv.wait(lock, [this] {
	  return this->shared->size < this->capacity || !this->shared->out_alive;
  });

  if (shared->out_alive) {
	  // since the out is still alive, we know that
	  // there is some space free in the storage
	  if (!took_fast_path)
	  {
		  shared->storage[write_pos] = std::move(val);
	  }
	  shared->size += 1;
	  old_size = shared->size;
	  success = true;
  } else {
	  shared->in_alive = false;
	  closed = true;
  }

  shared->cv.notify_one();
  if (success) { write_pos = wrapping_inc(write_pos, capacity); }
  return success;
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
    : shared(shared_)
    , write_pos(0)
    , old_size(0)
    , capacity(shared_->capacity)
    , closed(false)
{
  std::unique_lock lock(shared->mutex);
  shared->in_alive = true;
}

template <typename T>
std::pair<in<T>, out<T>> CreateBufferedChannel(std::size_t capacity)
{
  std::shared_ptr<data<T>> shared = std::make_shared<data<T>>(capacity);
  in<T> in(shared);
  out<T> out(shared);
  return {std::move(in), std::move(out)};
}
}  // namespace channel

#endif  // BAREOS_LIB_CHANNEL_H_
