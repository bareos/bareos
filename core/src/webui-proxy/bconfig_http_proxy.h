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
#ifndef BAREOS_WEBUI_PROXY_BCONFIG_HTTP_PROXY_H_
#define BAREOS_WEBUI_PROXY_BCONFIG_HTTP_PROXY_H_

#include <string>
#include <string_view>

struct BconfigHttpProxyConfig {
  std::string upstream_host{"127.0.0.1"};
  int upstream_port{8080};
};

bool IsBconfigProxyRoute(std::string_view target);
void RunBconfigHttpProxySession(int fd,
                                int address_family,
                                const BconfigHttpProxyConfig& cfg);

#endif  // BAREOS_WEBUI_PROXY_BCONFIG_HTTP_PROXY_H_
