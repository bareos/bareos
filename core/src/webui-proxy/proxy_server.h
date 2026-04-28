/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2024-2026 Bareos GmbH & Co. KG

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
 * TCP accept loop — spawns one thread per browser connection.
 */
#ifndef BAREOS_WEBUI_PROXY_PROXY_SERVER_H_
#define BAREOS_WEBUI_PROXY_PROXY_SERVER_H_

#include "proxy_config.h"
#include <csignal>
#include <string>
#include <vector>

/**
 * Open a listen socket and block until Stop() is called or a fatal error
 * occurs.  Each accepted connection is handed to RunProxySession() in a
 * detached std::thread.
 */
class ProxyServer {
 public:
  explicit ProxyServer(const ProxyConfig& cfg) : cfg_(cfg) {}

  /** Blocking: accept loop.  Returns when Stop() is called. */
  void Run();

  /** Signal the accept loop to stop (safe to call from a signal handler). */
  void Stop();

 private:
  ProxyConfig cfg_;
  // All listening sockets (one per address family for the bind host).
  // Populated by Run(); closed during Run() shutdown.
  std::vector<int> listen_fds_;
  volatile std::sig_atomic_t stop_requested_{0};

  void CleanupSockets();
};

#endif  // BAREOS_WEBUI_PROXY_PROXY_SERVER_H_
