/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2024-2025 Bareos GmbH & Co. KG

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

#include "proxy_session.h"
#include <string>
#include <vector>

struct ServerConfig {
  std::string bind_host{"localhost"};
  int port{8765};
  DefaultDirectorConfig director;
};

/**
 * Open a listen socket and block until Stop() is called or a fatal error
 * occurs.  Each accepted connection is handed to RunProxySession() in a
 * detached std::thread.
 */
class ProxyServer {
 public:
  explicit ProxyServer(const ServerConfig& cfg) : cfg_(cfg) {}

  /** Blocking: accept loop.  Returns when Stop() is called. */
  void Run();

  /** Signal the accept loop to stop (safe to call from a signal handler). */
  void Stop();

 private:
  ServerConfig cfg_;
  // All listening sockets (one per address family for the bind host).
  // Populated by Run(); closed by Stop().
  std::vector<int> listen_fds_;
};

#endif  // BAREOS_WEBUI_PROXY_PROXY_SERVER_H_
