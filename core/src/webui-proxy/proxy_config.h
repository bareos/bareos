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
#ifndef BAREOS_WEBUI_PROXY_PROXY_CONFIG_H_
#define BAREOS_WEBUI_PROXY_PROXY_CONFIG_H_

#include <map>
#include <optional>
#include <string>

struct DirectorTargetConfig {
  std::string host{"localhost"};
  int port{9101};
  std::string name{"bareos-dir"};
  bool tls_psk_disable{false};
  bool tls_psk_require{true};
};

struct ProxyConfig {
  std::string bind_host{"localhost"};
  int port{8765};
  std::map<std::string, DirectorTargetConfig> allowed_directors;
};

void LoadProxyConfigFile(const std::string& path, ProxyConfig& cfg);
void LoadProxyConfigFromString(const std::string& ini, ProxyConfig& cfg);

DirectorTargetConfig ResolveDirectorTarget(
    const ProxyConfig& cfg,
    const std::optional<std::string>& requested_director,
    const std::optional<std::string>& requested_host,
    const std::optional<int>& requested_port);

#endif  // BAREOS_WEBUI_PROXY_PROXY_CONFIG_H_
