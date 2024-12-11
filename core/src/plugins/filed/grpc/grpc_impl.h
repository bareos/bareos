/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2024-2024 Bareos GmbH & Co. KG

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

#ifndef BAREOS_PLUGINS_FILED_GRPC_GRPC_IMPL_H_
#define BAREOS_PLUGINS_FILED_GRPC_GRPC_IMPL_H_

#include <unistd.h>
#include <atomic>
#include <variant>
#include <thread>

#include "include/bareos.h"
#include "filed/fd_plugins.h"

struct Socket {
  Socket() = default;
  Socket(int fd) : os{fd} {}

  Socket(const Socket&) = delete;
  Socket& operator=(const Socket&) = delete;

  Socket(Socket&& other) { *this = std::move(other); }

  Socket& operator=(Socket&& other)
  {
    std::swap(os, other.os);
    return *this;
  }

  explicit operator bool() { return os >= 0; }

  int& get() { return os; }
  const int& get() const { return os; }

  int release()
  {
    int fd = os;
    os = -1;
    return fd;
  }

  ~Socket()
  {
    if (os >= 0) close(os);
  }

 private:
  int os{-1};
};

struct OSFile {
  OSFile() = default;
  OSFile(int fd) : os{fd} {}

  OSFile(const OSFile&) = delete;
  OSFile& operator=(const OSFile&) = delete;

  OSFile(OSFile&& other) { *this = std::move(other); }

  OSFile& operator=(OSFile&& other)
  {
    std::swap(os, other.os);
    return *this;
  }

  explicit operator bool() { return os >= 0; }

  int& get() { return os; }
  const int& get() const { return os; }

  int release()
  {
    int fd = os;
    os = -1;
    return fd;
  }

  ~OSFile()
  {
    if (os >= 0) close(os);
  }

 private:
  int os{-1};
};

struct grpc_connection_members;

class grpc_connection {
 public:
  bRC Setup();

  bRC handlePluginEvent(filedaemon::bEventType type, void* data);

  bRC startBackupFile(filedaemon::save_pkt* pkt);
  bRC endBackupFile();
  bRC startRestoreFile(std::string_view cmd);
  bRC endRestoreFile();

  bRC pluginIO(filedaemon::io_pkt* pkt, int io_socket);

  bRC createFile(filedaemon::restore_pkt* pkt);
  bRC setFileAttributes(filedaemon::restore_pkt* pkt);
  bRC checkFile(const char* fname);

  bRC getAcl(filedaemon::acl_pkt* pkt);
  bRC setAcl(filedaemon::acl_pkt* pkt);
  bRC getXattr(filedaemon::xattr_pkt* pkt);
  bRC setXattr(filedaemon::xattr_pkt* pkt);

  friend struct connection_builder;

  grpc_connection(const grpc_connection&) = delete;
  grpc_connection& operator=(const grpc_connection&) = delete;
  grpc_connection(grpc_connection&& other) { *this = std::move(other); }
  grpc_connection& operator=(grpc_connection&& other)
  {
    std::swap(members, other.members);
    return *this;
  }

  ~grpc_connection();

 private:
  grpc_connection() = default;

  grpc_connection_members* members{nullptr};
  bool do_io_in_core{false};
};

struct process {
  pid_t pid{-1};

  process() = default;
  process(pid_t p) : pid{p} {}

  process(const process&) = delete;
  process& operator=(const process&) = delete;

  process(process&& other) { *this = std::move(other); }
  process& operator=(process&& other)
  {
    std::swap(pid, other.pid);
    return *this;
  }

  pid_t get() { return pid; }

  ~process();
};

struct joining_thread {
  std::unique_ptr<std::atomic<bool>> quit;
  std::thread inner;

  template <typename Fun, typename... Args>
  joining_thread(Fun&& f, Args&&... args)
      : quit{std::make_unique<std::atomic<bool>>()}
      , inner{f, quit.get(), std::forward<Args>(args)...}
  {
  }

  joining_thread(const joining_thread&) = delete;
  joining_thread& operator=(const joining_thread&) = delete;
  joining_thread(joining_thread&&) = default;
  joining_thread& operator=(joining_thread&&) = default;

  ~joining_thread()
  {
    if (quit) {
      *quit = true;
      inner.join();
    }
  }
};

struct grpc_child {
  joining_thread stdio_thread;  // this should get destructed last
  PluginContext* ctx;
  process child_pid;  // we want this to be destructed last!
  grpc_connection con;
  Socket io;
};

std::optional<grpc_child> make_connection(PluginContext* ctx,
                                          std::string_view program_path);


#endif  // BAREOS_PLUGINS_FILED_GRPC_GRPC_IMPL_H_
