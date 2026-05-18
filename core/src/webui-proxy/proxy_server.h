/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2026-2026 Bareos GmbH & Co. KG

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

/**
 * Global shutdown flag for the proxy process. Signal handlers set this directly
 * so they never need to dereference a ProxyServer object during shutdown.
 */
extern volatile std::sig_atomic_t g_proxy_shutdown_requested;

/**
 * Open a listen socket and block until shutdown is requested or a fatal error
 * occurs. Each accepted connection is handed to RunProxySession() in a
 * detached std::thread.
 */
class ProxyServer {
 public:
  explicit ProxyServer(const ProxyConfig& cfg) : cfg_(cfg) {}

  /** Blocking: accept loop. Returns when shutdown is requested. */
  void Run();

 private:
  ProxyConfig cfg_;
};

#endif  // BAREOS_WEBUI_PROXY_PROXY_SERVER_H_
