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
#include <chrono>
#include <thread>

void connection::socket_closer::operator()(BareosSocket* t_socket)
{
  if (t_socket) {
    t_socket->close();
    delete t_socket;
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

void remove_inactive(std::vector<connection>& vec)
{
  vec.erase(std::remove_if(vec.begin(), vec.end(),
                           [](auto& conn) {
                             return !check_connection(conn.socket.get(), 0);
                           }),
            vec.end());
}
};  // namespace

std::optional<connection> connection_pool::take_by_name(
    std::string_view v,
    std::chrono::seconds timeout)
{
  std::chrono::time_point endpoint = std::chrono::system_clock::now() + timeout;

  if (std::optional opt = conns.try_lock(endpoint)) {
    auto& locked = opt.value();
    for (;;) {
      auto& vec = locked.get();
      remove_inactive(vec);

      // search from last to first
      // this is necessary for cases where the same client connected multiple
      // times we want to always take the last connection because that one is
      // the most likely to be still alive
      if (auto it = std::find_if(
              vec.rbegin(), vec.rend(),
              [v](auto& connection) { return v == connection.name; });
          it != vec.rend()) {
        auto conn = std::move(*it);
        // std::next(it).base() points to the same element as it,
        // i.e. *it == *next(it).base()
        vec.erase(std::next(it).base());
        return conn;
      }

      if (locked.wait_until(element_added, endpoint)
          == std::cv_status::timeout) {
        break;
      }
    }
  }

  return std::nullopt;
}

std::vector<connection_info> connection_pool::info()
{
  auto locked = conns.lock();
  auto& vec = locked.get();
  remove_inactive(vec);

  // connections are subclasses of connection_info, so we can create a copy
  // of just the connection_info part with implicit casting like so:
  return {vec.begin(), vec.end()};
}

void connection_pool::add_authenticated_connection(connection conn)
{
  auto locked = conns.lock();
  auto& vec = locked.get();
  remove_inactive(vec);
  vec.emplace_back(std::move(conn));
  element_added.notify_all();
}

void connection_pool::clear() { conns.lock()->clear(); }

void connection_pool::cleanup(std::chrono::seconds timeout)
{
  std::chrono::time_point endpoint = std::chrono::system_clock::now() + timeout;

  if (std::optional opt = conns.try_lock(endpoint)) {
    auto& locked = opt.value();
    auto& vec = locked.get();
    remove_inactive(vec);
  }
}
