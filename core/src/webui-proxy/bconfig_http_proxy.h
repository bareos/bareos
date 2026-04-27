/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2026 Bareos GmbH & Co. KG

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
#ifndef BAREOS_WEBUI_PROXY_BCONFIG_HTTP_PROXY_H_
#define BAREOS_WEBUI_PROXY_BCONFIG_HTTP_PROXY_H_

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <atomic>
#include <memory>
#include <string>

struct BconfigHttpProxyConfig {
  std::string bind_host{"localhost"};
  int listen_port{0};
  std::string upstream_host{"127.0.0.1"};
  int upstream_port{8080};
};

class BconfigHttpProxyServer {
 public:
  explicit BconfigHttpProxyServer(const BconfigHttpProxyConfig& cfg);

  void Run();
  void Stop();

 private:
  BconfigHttpProxyConfig cfg_;
  std::atomic<bool> stop_requested_{false};
  std::unique_ptr<boost::asio::io_context> io_context_;
  std::unique_ptr<boost::asio::ip::tcp::acceptor> acceptor_;
};

#endif  // BAREOS_WEBUI_PROXY_BCONFIG_HTTP_PROXY_H_
