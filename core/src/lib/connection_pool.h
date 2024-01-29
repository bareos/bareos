/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2016-2024 Bareos GmbH & Co. KG

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
/**
 * @file
 * Connection Pool
 *
 * handle/store multiple connections
 */

#ifndef BAREOS_LIB_CONNECTION_POOL_H_
#define BAREOS_LIB_CONNECTION_POOL_H_

#include <atomic>
#include <memory>
#include <vector>
#include <string>
#include <string_view>
#include <optional>
#include <chrono>
#include "lib/thread_util.h"

class BareosSocket;

struct connection_info {
  std::string name{};
  int protocol_version{};
  time_t connect_time{};
};

struct connection : public connection_info {
  struct socket_closer {
    void operator()(BareosSocket* socket);
  };
  using sock_ptr = std::unique_ptr<BareosSocket, socket_closer>;

  connection() = default;
  connection(std::string_view t_name,
             int t_protocol_version,
             BareosSocket* t_socket)
      : connection_info{std::string{t_name}, t_protocol_version, time(nullptr)}
      , socket{t_socket}
  {
  }

  connection(const connection&) = delete;
  connection& operator=(const connection&) = delete;
  connection(connection&&) = default;
  connection& operator=(connection&&) = default;

  sock_ptr socket{};
};

struct connection_pool {
  synchronized<std::vector<connection>, std::timed_mutex> conns;
  std::condition_variable_any element_added;

  void add_authenticated_connection(connection con);
  void clear();

  std::optional<connection> take_by_name(std::string_view v,
                                         std::chrono::seconds timeout
                                         = std::chrono::seconds{0});

  void cleanup(std::chrono::seconds timeout = std::chrono::seconds{0});
  std::vector<connection_info> info();
};

#endif  // BAREOS_LIB_CONNECTION_POOL_H_
