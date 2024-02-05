/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2018-2024 Bareos GmbH & Co. KG

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
#ifndef BAREOS_LIB_BNET_SERVER_TCP_H_
#define BAREOS_LIB_BNET_SERVER_TCP_H_

#include <atomic>
#include <functional>
#include <vector>

constexpr int kListenBacklog = 50;

class ConfigurationParser;
class ThreadList;
class IPADDR;

enum class BnetServerState
{
  kUndefined = 0,
  kStarting,
  kError,
  kStarted,
  kEnded
};

template <typename T> class dlist;

void close_socket(int fd);

struct s_sockfd {
  int fd{-1};
  int port{0};

  s_sockfd() = default;

  s_sockfd(const s_sockfd&) = delete;
  s_sockfd& operator=(const s_sockfd&) = delete;
  s_sockfd(s_sockfd&& other) { *this = std::move(other); }
  s_sockfd& operator=(s_sockfd&& other)
  {
    std::swap(other.fd, fd);
    std::swap(other.port, port);
    return *this;
  }

  ~s_sockfd()
  {
    if (fd > 0) {
      close_socket(fd);
      fd = -1;
    }
  }
};

std::vector<s_sockfd> OpenAndBindSockets(dlist<IPADDR>* addr_list);

void BnetThreadServerTcp(
    std::vector<s_sockfd> bound_sockets,
    ThreadList& thread_list,
    std::function<void*(ConfigurationParser* config, void* bsock)>
        HandleConnectionRequest,
    ConfigurationParser* config,
    std::atomic<BnetServerState>* const server_state = nullptr,
    std::function<void*(void* bsock)> UserAgentShutdownCallback = nullptr,
    std::function<void()> CustomCallback = nullptr);

void RemoveDuplicateAddresses(dlist<IPADDR>* addr_list);

int OpenSocketAndBind(IPADDR* ipaddr,
                      dlist<IPADDR>* addr_list,
                      uint16_t port_number);

void BnetStopAndWaitForThreadServerTcp(pthread_t tid);

#endif  // BAREOS_LIB_BNET_SERVER_TCP_H_
