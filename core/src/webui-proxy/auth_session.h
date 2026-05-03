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
#ifndef BAREOS_WEBUI_PROXY_AUTH_SESSION_H_
#define BAREOS_WEBUI_PROXY_AUTH_SESSION_H_

#include <chrono>
#include <ctime>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

struct AuthIdentity {
  std::string provider;
  std::string subject;
  std::string username;
  std::string email;
  std::vector<std::string> groups;
  std::vector<std::string> roles;
  std::map<std::string, std::string> attributes;
};

struct AuthResult {
  AuthIdentity identity;
  std::optional<std::time_t> expires_at;
};

struct ProxySession {
  std::string token;
  AuthIdentity identity;
  std::string director_username;
  std::string director_password;
  std::string preferred_director_id;
  std::time_t created_at{0};
  std::time_t expires_at{0};
};

using ProxySessionClock = std::function<std::time_t()>;

class ProxySessionStore {
 public:
  explicit ProxySessionStore(
      std::chrono::seconds ttl,
      ProxySessionClock clock = []() { return std::time(nullptr); });

  ProxySession CreateSession(const AuthResult& auth_result,
                             std::string director_username,
                             std::string director_password,
                             std::string preferred_director_id);

  std::optional<ProxySession> GetSession(const std::string& token);

  std::optional<ProxySession> RefreshSession(
      const std::string& token,
      const std::optional<std::string>& preferred_director_id = std::nullopt);

  bool RemoveSession(const std::string& token);

 private:
  std::string GenerateSessionToken() const;
  std::time_t ComputeExpiry(
      std::time_t now,
      const std::optional<std::time_t>& upstream_expiry) const;
  void RemoveExpiredSessionsLocked(std::time_t now);

  std::chrono::seconds ttl_;
  ProxySessionClock clock_;
  mutable std::mutex mutex_;
  std::unordered_map<std::string, ProxySession> sessions_;
};

#endif  // BAREOS_WEBUI_PROXY_AUTH_SESSION_H_
