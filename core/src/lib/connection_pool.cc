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

// Connection and Connection Pool

#include "include/bareos.h"
#include "connection_pool.h"
#include "lib/util.h"
#include "lib/alist.h"
#include "lib/bsys.h"
#include "lib/bsock.h"

#include <algorithm>

// connection
connection::connection(std::string_view name,
                       int protocol_version,
                       BareosSocket* socket,
                       bool authenticated)
    : connection_info{std::string{name}, protocol_version, authenticated,
                      time(nullptr)}
    , socket{socket}
{
}

void connection::socket_closer::operator()(BareosSocket* socket)
{
  if (socket) {
    socket->close();
    delete socket;
  }
}

namespace {
bool check_connection(BareosSocket* socket, int timeout)
{
  if (!socket) { return false; }
  // idea: send a status command and check for a return value

  // Returns: 1 if data available, 0 if timeout, -1 if error
  int data_available = socket->WaitDataIntr(timeout);

  if (data_available < 0) {
    return false;
  } else if (data_available > 0) {
    if (socket->recv() <= 0) { return false; }
    if (socket->IsError()) { return false; }
  }

  return true;
}

void remove_inactive(std::vector<connection>& vec, int timeout)
{
  vec.erase(std::remove_if(vec.begin(), vec.end(),
                           [timeout](auto& conn) {
                             return !check_connection(conn.socket.get(),
                                                      timeout);
                           }),
            vec.end());
}
};  // namespace

std::optional<connection> take_by_name(connection_pool& pool,
                                       std::string_view v,
                                       int timeout)
{
  auto locked = pool.lock();
  auto& vec = locked.get();
  remove_inactive(vec, timeout);
  if (auto it
      = std::find_if(vec.begin(), vec.end(),
                     [v](auto& connection) { return v == connection.name; });
      it != vec.end()) {
    auto conn = std::move(*it);
    vec.erase(it);
    return conn;
  }
  return std::nullopt;
}

std::vector<connection_info> get_connection_info(connection_pool& pool,
                                                 int timeout)
{
  auto locked = pool.lock();
  auto& vec = locked.get();
  remove_inactive(vec, timeout);

  // connections are subclasses of connection_info, so we can create a copy
  // of just the connection_info part with implicit casting like so:
  return {vec.begin(), vec.end()};
}

void cleanup_connection_pool(connection_pool& pool, int timeout)
{
  auto locked = pool.lock();
  auto& vec = locked.get();
  remove_inactive(vec, timeout);
}
