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
#include "lib/thread_util.h"

class BareosSocket;

struct connection_info {
  std::string name{};
  int protocol_version{};
  bool authenticated{};
  time_t connect_time{};
};

struct connection : public connection_info {
  struct socket_closer {
    void operator()(BareosSocket*);
  };
  using sock_ptr = std::unique_ptr<BareosSocket, socket_closer>;

  connection() = default;
  connection(std::string_view name,
             int protocol_version,
             BareosSocket* socket,
             bool authenticated = true);

  connection(const connection&) = delete;
  connection& operator=(const connection&) = delete;
  connection(connection&&) = default;
  connection& operator=(connection&&) = default;

  sock_ptr socket{};
};

using connection_pool = synchronized<std::vector<connection>>;

std::optional<connection> take_by_name(connection_pool& pool,
                                       std::string_view v,
                                       int timeout);

std::vector<connection_info> get_connection_info(connection_pool& pool,
                                                 int timeout = 0);

void cleanup_connection_pool(connection_pool& pool, int timeout = 0);
#endif  // BAREOS_LIB_CONNECTION_POOL_H_
