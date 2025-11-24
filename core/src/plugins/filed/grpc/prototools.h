/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2025-2025 Bareos GmbH & Co. KG

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

#ifndef BAREOS_PLUGINS_FILED_GRPC_PROTOTOOLS_H_
#define BAREOS_PLUGINS_FILED_GRPC_PROTOTOOLS_H_

#include <pthread.h>
#include <span>
#include <fcntl.h>
#include <poll.h>
#include <sys/socket.h>
#include <atomic>
#include <optional>
#include <vector>
#include <queue>
#include <cstring>
#include <cassert>

static bool WaitOnReadable(int fd)
{
  struct pollfd pfd = {};
  pfd.fd = fd;
  pfd.events = POLLIN;

  switch (poll(&pfd, 1, 500)) {
    case -1: /* ERROR */
      return false;
    case 0: /* TIMEOUT */ {
      // timeout cannot happen
      return true;
    }
  }

  if ((pfd.revents & POLLIN) != POLLIN) { return false; }

  return true;
}

namespace prototools {
struct ProtoInputStream {
  int fd;
  std::vector<uint8_t> buffer{};
  // this points to the end of the read memory,
  // everything afterwards ([current_offset, size())) is unread
  std::size_t current_offset{0};
  // this is how much data there is in total in the buffer, both read and unread
  std::size_t data_in_buffer{0};

  //                              |       empty       |
  // --------------------------------------------------
  // |                   buffer                       |
  // --------------------------------------------------
  // |     data_in_buffer         |
  // |    read     |    unread    |
  //               ^
  //        current_offset

  using file_descriptor = int;
  std::queue<file_descriptor> received_fds;


  ProtoInputStream(int in_fd) : fd{in_fd}
  {
    // int flags = fcntl(fd, F_GETFL);
    // assert(flags >= 0);
    // flags |= O_NONBLOCK;


    // assert(fcntl(fd, F_SETFL, flags) >= 0);
  }

  ProtoInputStream(const ProtoInputStream&) = delete;
  ProtoInputStream& operator=(const ProtoInputStream&) = delete;
  ProtoInputStream(ProtoInputStream&&) = delete;
  ProtoInputStream& operator=(ProtoInputStream&&) = delete;

  const uint8_t* do_read(uint32_t min_size)
  {
    auto old_data = data_in_buffer - current_offset;

    if (old_data == 0) { current_offset = data_in_buffer = 0; }

    if (old_data > min_size) {
      auto* head = buffer.data() + current_offset;
      current_offset += min_size;
      return head;
    }

    uint32_t read_goal = min_size - old_data;

    if (data_in_buffer + read_goal > buffer.size()) {
      // the data is too big to append in place, so we need to adjust our buffer

      if (min_size > buffer.size()) {
        // the message itself is too big to fit, so we need to resize the buffer
        std::vector<uint8_t> bigger(min_size * 2);

        if (old_data > 0) {
          // its ub to copy from/into a nullptr even with size = 0 ...
          memcpy(bigger.data(), buffer.data() + current_offset, old_data);
        }
        buffer = std::move(bigger);
      } else {
        // the message can fit into our buffer, we just have to make sure
        // to move the unread data to the front
        if (old_data > 0) {
          memmove(buffer.data(), buffer.data() + current_offset, old_data);
        }
      }

      // in any case, we deleted all read data from the front, and moved
      // the unread data to the front
      current_offset = 0;
      data_in_buffer = old_data;
    }

    assert(min_size <= buffer.size());

    uint32_t bytes_read = 0;

    uint8_t* head = buffer.data() + data_in_buffer;
    uint32_t max_size = buffer.size() - data_in_buffer;

    char cbuf[CMSG_SPACE(sizeof(file_descriptor))] = {};

    iovec io = {};
    struct msghdr msg = {};
    msg.msg_iov = &io;
    msg.msg_iovlen = 1;

    msg.msg_control = cbuf;
    msg.msg_controllen = sizeof(cbuf);

    while (bytes_read < read_goal) {
      io.iov_base = head + bytes_read;
      io.iov_len = max_size - bytes_read;

      // this needs to get reset every time, as recvmsg writes into it
      // the actual received len
      msg.msg_controllen = sizeof(cbuf);

      auto new_bytes = recvmsg(fd, &msg, MSG_DONTWAIT);
      if (new_bytes < 0) {
        auto err = errno;
        switch (err) {
#if EAGAIN != EWOULDBLOCK
          case EWOULDBLOCK:
            [[fallthrough]];
#endif
          case EAGAIN: {
            if (!WaitOnReadable(fd)) { return nullptr; }

            continue;
          }
        }
        return nullptr;
      } else if (new_bytes == 0) {
        return nullptr;
      }

      bytes_read += new_bytes;

      if ((msg.msg_flags & MSG_CTRUNC) == MSG_CTRUNC) {
        // something went wrong
        assert(0);
      }

      for (auto* control = CMSG_FIRSTHDR(&msg); control;
           control = CMSG_NXTHDR(&msg, control)) {
        // we received a control message!
        if (control->cmsg_len != CMSG_LEN(sizeof(file_descriptor))) {
          // this shouldnt happen
          assert(0);
        }

        if (control->cmsg_level != SOL_SOCKET
            || control->cmsg_type != SCM_RIGHTS) {
          assert(0);
        }

        auto* data = CMSG_DATA(control);

        auto& received_fd = received_fds.emplace();
        memcpy(&received_fd, data, sizeof(file_descriptor));
      }
    }

    auto* res = buffer.data() + current_offset;
    current_offset += min_size;
    data_in_buffer += bytes_read;

    return res;
  }

  std::optional<int> PopFD()
  {
    if (received_fds.empty()) { return std::nullopt; }

    file_descriptor next = received_fds.front();
    received_fds.pop();
    return next;
  }

  template <typename Message> bool Read(Message& msg)
  {
    uint32_t RequestSize = 0;

    auto* ptr = do_read(sizeof(RequestSize));
    if (!ptr) { return false; }

    memcpy(&RequestSize, ptr, sizeof(RequestSize));

    ptr = do_read(RequestSize);
    if (!ptr) {
      assert(0);
      return false;
    }

    return msg.ParseFromArray(ptr, RequestSize);
  }
};

struct ProtoOutputStream {
  int fd;

  std::vector<uint8_t> buffer;

  ProtoOutputStream(int out_fd) : fd{out_fd} {}

  ProtoOutputStream(const ProtoOutputStream&) = delete;
  ProtoOutputStream& operator=(const ProtoOutputStream&) = delete;
  ProtoOutputStream(ProtoOutputStream&&) = delete;
  ProtoOutputStream& operator=(ProtoOutputStream&&) = delete;

  bool do_write(uint32_t size)
  {
    uint32_t bytes_written = 0;

    while (bytes_written < size) {
      auto new_bytes
          = write(fd, buffer.data() + bytes_written, size - bytes_written);
      if (new_bytes <= 0) { return false; }

      bytes_written += new_bytes;
    }

    return true;
  }

  template <typename Message> bool WriteBuffered(Message& msg)
  {
    uint32_t size = msg.ByteSizeLong();

    uint32_t write_size = size + sizeof(size);

    if (buffer.size() < write_size) { buffer.resize(write_size * 2); }

    uint8_t* current = buffer.data();
    memcpy(current, &size, sizeof(size));
    current += sizeof(size);

    uint8_t* end = msg.SerializeWithCachedSizesToArray(current);

    if (end - current != size) { assert(0); }

    current = end;

    assert(current - buffer.data() == write_size);

    if (!do_write(write_size)) { return false; }

    return true;
  }
  template <typename Message> inline bool Write(Message& msg)
  {
    return WriteBuffered(msg);

    // this api is only used to send single messages
    // so we need to flush after every write as it never makes sense
    // to "pool" multiple requests.
  }
};

struct ProtoBidiStream {
  ProtoInputStream input;
  ProtoOutputStream output;

  std::size_t read_count{}, write_count{};

  std::atomic<std::size_t> concurrent_reads;
  std::atomic<std::size_t> concurrent_writes;

  // for a bidirectional file descriptor; think socket
  ProtoBidiStream(int bidi_fd) : ProtoBidiStream(bidi_fd, bidi_fd) {}

  ProtoBidiStream(int in_fd, int out_fd) : input{in_fd}, output{out_fd} {}

  ProtoBidiStream(const ProtoBidiStream&) = delete;
  ProtoBidiStream& operator=(const ProtoBidiStream&) = delete;
  ProtoBidiStream(ProtoBidiStream&&) = delete;
  ProtoBidiStream& operator=(ProtoBidiStream&&) = delete;

  template <typename Message> bool Read(Message& msg)
  {
    read_count += 1;

    auto before = concurrent_reads++;
    assert(before == 0);

    bool ok = input.Read(msg);

    concurrent_reads -= 1;

    return ok;
  }

  template <typename Message> bool Write(Message& msg)
  {
    write_count += 1;

    auto before = concurrent_writes++;
    assert(before == 0);

    bool ok = output.Write(msg);

    concurrent_writes -= 1;

    return ok;
  }

  template <typename Message> bool WriteBuffered(Message& msg)
  {
    write_count += 1;
    return output.WriteBuffered(msg);
  }

  std::optional<int> PopFD() { return input.PopFD(); }
};

struct Lock {
  pthread_mutex_t* mut{nullptr};

  Lock() {}

  Lock(pthread_mutex_t* m) : mut{m} { pthread_mutex_lock(mut); }

  void lock(pthread_mutex_t* m)
  {
    mut = m;
    pthread_mutex_lock(mut);
  }

  void unlock()
  {
    pthread_mutex_unlock(mut);
    mut = nullptr;
  }

  Lock(const Lock& l) = delete;
  Lock& operator=(const Lock& l) = delete;

  Lock(Lock&& l) { std::swap(l.mut, mut); }
  Lock& operator=(Lock&& l)
  {
    std::swap(l.mut, mut);
    return *this;
  }

  ~Lock()
  {
    if (mut) { pthread_mutex_unlock(mut); }
  }
};

struct InMemoryProtoQueue {
  using size_type = std::uint32_t;


  struct queue_data {
    size_type read_head;
    size_type write_head;
  };

  struct control_block {
    pthread_mutex_t mutex;
    pthread_cond_t queue_changed;

    queue_data queue;
  };

  queue_data cached;


  control_block* ctrl;
  std::span<uint8_t> data;

  std::span<uint8_t> allocate(size_type size)
  {
    if (size > data.size()) {
      // ??
      return {};
    }

    if (data.size() - size <= cached.write_head - cached.read_head) {
    } else {
      Lock l{&ctrl->mutex};
      while (ctrl->queue.write_head + size - ctrl->queue.read_head
             > data.size()) {
        pthread_cond_wait(&ctrl->queue_changed, &ctrl->mutex);
      }

      cached = ctrl->queue;
    }


    return data.subspan(cached.write_head, size);
  }

  inline void update_write_head(std::span<uint8_t> written)
  {
    Lock l{&ctrl->mutex};
    assert(data.data() + ctrl->queue.write_head == written.data());

    ctrl->queue.write_head += written.size();

    cached = ctrl->queue;
    pthread_cond_broadcast(&ctrl->queue_changed);
  }

  template <typename Message> bool Write(Message& msg)
  {
    uint64_t size = msg.ByteSizeLong();

    auto buffer = allocate(size + sizeof(size));

    memcpy(buffer.data(), &size, sizeof(size));
    auto* end
        = msg.SerializeWithCachedSizesToArray(buffer.data() + sizeof(size));

    if (end != &*buffer.end()) {
      // ??
    }

    update_write_head(buffer);

    return true;
  }

  inline std::span<uint8_t> wait_on_data(size_type size)
  {
    if (size > data.size()) {
      // ???
      assert(0);
    }

    if (size > cached.write_head - cached.read_head) {
      Lock l{&ctrl->mutex};
      while (ctrl->queue.write_head - ctrl->queue.read_head > size) {
        pthread_cond_wait(&ctrl->queue_changed, &ctrl->mutex);
      }

      cached = ctrl->queue;
    }

    return data.subspan(cached.read_head, size);
  }

  inline void update_read_head(std::span<uint8_t> written)
  {
    Lock l{&ctrl->mutex};
    assert(data.data() + ctrl->queue.read_head == written.data());

    ctrl->queue.read_head += written.size();

    cached = ctrl->queue;
    pthread_cond_broadcast(&ctrl->queue_changed);
  }

  template <typename Message> bool Read(Message& msg)
  {
    size_type size;

    auto size_data = wait_on_data(sizeof(size));

    std::memcpy(&size, size_data.data(), sizeof(size));

    auto complete_packet = wait_on_data(sizeof(size) + size);
    auto msg_data = complete_packet.subspan(sizeof(size));

    if (!msg.ParseFromArray(msg_data.data(), msg_data.size())) { return false; }

    update_read_head(complete_packet);

    return true;
  }
};
};  // namespace prototools

#endif  // BAREOS_PLUGINS_FILED_GRPC_PROTOTOOLS_H_
