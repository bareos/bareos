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
#include "proxy_config.h"

#include <stdexcept>
#include <string>

#include <jansson.h>

namespace {

std::optional<std::string> OptionalString(json_t* obj, const char* key)
{
  json_t* value = json_object_get(obj, key);
  if (!value) { return std::nullopt; }
  if (!json_is_string(value)) {
    throw std::runtime_error(std::string("Proxy config: '") + key
                             + "' must be a string");
  }
  return std::string(json_string_value(value));
}

std::optional<int> OptionalInteger(json_t* obj, const char* key)
{
  json_t* value = json_object_get(obj, key);
  if (!value) { return std::nullopt; }
  if (!json_is_integer(value)) {
    throw std::runtime_error(std::string("Proxy config: '") + key
                             + "' must be an integer");
  }
  return static_cast<int>(json_integer_value(value));
}

std::optional<bool> OptionalBool(json_t* obj, const char* key)
{
  json_t* value = json_object_get(obj, key);
  if (!value) { return std::nullopt; }
  if (!json_is_boolean(value)) {
    throw std::runtime_error(std::string("Proxy config: '") + key
                             + "' must be a boolean");
  }
  return json_is_true(value);
}

DirectorTargetConfig ParseDirectorConfig(json_t* obj,
                                         const DirectorTargetConfig& defaults,
                                         const std::string& fallback_name)
{
  if (!json_is_object(obj)) {
    throw std::runtime_error("Proxy config: director entries must be objects");
  }

  DirectorTargetConfig result = defaults;
  if (auto host = OptionalString(obj, "host")) { result.host = *host; }
  if (auto port = OptionalInteger(obj, "port")) { result.port = *port; }

  if (auto name = OptionalString(obj, "director_name")) {
    result.name = *name;
  } else if (!fallback_name.empty()) {
    result.name = fallback_name;
  }

  if (auto tls_psk_disable = OptionalBool(obj, "tls_psk_disable")) {
    result.tls_psk_disable = *tls_psk_disable;
  }
  result.tls_psk_require = !result.tls_psk_disable;
  return result;
}

void ApplyParsedProxyConfig(json_t* root, ProxyConfig& cfg)
{
  if (!json_is_object(root)) {
    throw std::runtime_error("Proxy config: root value must be an object");
  }

  if (auto bind_host = OptionalString(root, "ws_host")) {
    cfg.bind_host = *bind_host;
  }
  if (auto port = OptionalInteger(root, "ws_port")) { cfg.port = *port; }
  if (auto default_allowed = OptionalString(root, "default_allowed_director")) {
    cfg.default_allowed_director = *default_allowed;
  }

  if (json_t* director = json_object_get(root, "director")) {
    cfg.director = ParseDirectorConfig(director, cfg.director, cfg.director.name);
  }

  if (json_t* allowed = json_object_get(root, "allowed_directors")) {
    if (!json_is_object(allowed)) {
      throw std::runtime_error(
          "Proxy config: 'allowed_directors' must be an object");
    }

    cfg.allowed_directors.clear();
    const char* key = nullptr;
    json_t* value = nullptr;
    json_object_foreach (allowed, key, value) {
      cfg.allowed_directors.emplace(
          key, ParseDirectorConfig(value, cfg.director, key));
    }
  }

  if (!cfg.default_allowed_director.empty()
      && !cfg.allowed_directors.contains(cfg.default_allowed_director)) {
    throw std::runtime_error("Proxy config: 'default_allowed_director' does not "
                             "match any configured allowed director");
  }
}

}  // namespace

void LoadProxyConfigFile(const std::string& path, ProxyConfig& cfg)
{
  json_error_t error{};
  json_t* root = json_load_file(path.c_str(), 0, &error);
  if (!root) {
    throw std::runtime_error("Proxy config: cannot load '" + path + "': "
                             + error.text);
  }

  try {
    ApplyParsedProxyConfig(root, cfg);
  } catch (...) {
    json_decref(root);
    throw;
  }

  json_decref(root);
}

void LoadProxyConfigFromString(const std::string& json, ProxyConfig& cfg)
{
  json_error_t error{};
  json_t* root = json_loads(json.c_str(), 0, &error);
  if (!root) {
    throw std::runtime_error("Proxy config: cannot parse JSON: "
                             + std::string(error.text));
  }

  try {
    ApplyParsedProxyConfig(root, cfg);
  } catch (...) {
    json_decref(root);
    throw;
  }

  json_decref(root);
}

DirectorTargetConfig ResolveDirectorTarget(
    const ProxyConfig& cfg,
    const std::optional<std::string>& requested_director,
    const std::optional<std::string>& requested_host,
    const std::optional<int>& requested_port)
{
  if (requested_host || requested_port) {
    throw std::runtime_error(
        "Proxy config: host and port overrides are not allowed");
  }

  if (cfg.allowed_directors.empty()) {
    if (requested_director && !requested_director->empty()
        && *requested_director != cfg.director.name) {
      throw std::runtime_error("Proxy config: director '" + *requested_director
                               + "' is not allowed");
    }
    return cfg.director;
  }

  std::string director_id;
  if (requested_director && !requested_director->empty()) {
    director_id = *requested_director;
  } else if (!cfg.default_allowed_director.empty()) {
    director_id = cfg.default_allowed_director;
  } else if (cfg.allowed_directors.size() == 1) {
    director_id = cfg.allowed_directors.begin()->first;
  } else {
    throw std::runtime_error(
        "Proxy config: no director selected from the allowlist");
  }

  auto it = cfg.allowed_directors.find(director_id);
  if (it == cfg.allowed_directors.end()) {
    throw std::runtime_error("Proxy config: director '" + director_id
                             + "' is not in the allowlist");
  }

  return it->second;
}
