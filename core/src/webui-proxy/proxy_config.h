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
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

class ProxyConfigError : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
};

class ProxyConfigFileError : public ProxyConfigError {
 public:
  using ProxyConfigError::ProxyConfigError;
};

class ProxyConfigParseError : public ProxyConfigError {
 public:
  using ProxyConfigError::ProxyConfigError;
};

class ProxyConfigQuotedValueError : public ProxyConfigParseError {
 public:
  using ProxyConfigParseError::ProxyConfigParseError;
};

class ProxyConfigInvalidCharacterError : public ProxyConfigParseError {
 public:
  using ProxyConfigParseError::ProxyConfigParseError;
};

class ProxyConfigInvalidBooleanError : public ProxyConfigParseError {
 public:
  using ProxyConfigParseError::ProxyConfigParseError;
};

class ProxyConfigInvalidIntegerError : public ProxyConfigParseError {
 public:
  using ProxyConfigParseError::ProxyConfigParseError;
};

class ProxyConfigDuplicateSectionError : public ProxyConfigParseError {
 public:
  using ProxyConfigParseError::ProxyConfigParseError;
};

class ProxyConfigUnknownSectionError : public ProxyConfigParseError {
 public:
  using ProxyConfigParseError::ProxyConfigParseError;
};

class ProxyConfigUnknownKeyError : public ProxyConfigParseError {
 public:
  using ProxyConfigParseError::ProxyConfigParseError;
};

class ProxyConfigMissingDirectorSectionError : public ProxyConfigParseError {
 public:
  using ProxyConfigParseError::ProxyConfigParseError;
};

struct DirectorTargetConfig {
  std::string address{"localhost"};
  int port{9101};
  std::string name;
  bool tls_psk_disable{false};
};

struct ProxyConfig {
  std::string bind_address{"localhost"};
  int port{9104};
  // Keys are client-visible selectors; target.name is the Bareos director name.
  std::map<std::string, DirectorTargetConfig> configured_directors;
};

void LoadProxyConfigFile(const std::string& path, ProxyConfig& cfg);
void LoadProxyConfigFromString(const std::string& ini, ProxyConfig& cfg);

#endif  // BAREOS_WEBUI_PROXY_PROXY_CONFIG_H_
