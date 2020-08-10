/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2018-2018 Bareos GmbH & Co. KG

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
#ifndef BAREOS_LIB_BNET_SEVER_TCP_H_
#define BAREOS_LIB_BNET_SEVER_TCP_H_

#include <atomic>
#include <functional>

class ConfigurationParser;
class ThreadList;

enum class BnetServerState
{
  kUndefined = 0,
  kStarting,
  kError,
  kStarted,
  kEnded
};

class alist;
class dlist;

void BnetThreadServerTcp(
    dlist* addr_list,
    int max_clients,
    alist* sockfds,
    ThreadList& thread_list,
    bool nokeepalive,
    std::function<void*(ConfigurationParser* config, void* bsock)>
        HandleConnectionRequest,
    ConfigurationParser* config,
    std::atomic<BnetServerState>* const server_state = nullptr,
    std::function<void*(void* bsock)> UserAgentShutdownCallback = nullptr,
    std::function<void()> CustomCallback = nullptr);

void BnetStopAndWaitForThreadServerTcp(pthread_t tid);

#endif  // BAREOS_LIB_BNET_SEVER_TCP_H_
