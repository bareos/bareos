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
#include "proxy_auth_session.h"

#include <openssl/rand.h>

#include <array>
#include <chrono>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>

namespace {

using Clock = std::chrono::steady_clock;
constexpr auto kSessionIdleTimeout = std::chrono::minutes(30);
constexpr auto kSessionAbsoluteLifetime = std::chrono::hours(8);

struct StoredSession {
  ProxyAuthSessionRecord record;
  Clock::time_point created_at;
  Clock::time_point last_used_at;
};

class ProxyAuthSessionStoreImpl {
 public:
  std::string CreateSession(std::string_view username,
                            std::string_view password,
                            std::string_view director)
  {
    std::lock_guard guard(mutex_);
    CleanupExpiredLocked(Clock::now());

    ProxyAuthSessionRecord record;
    record.session_id = GenerateSessionId();
    record.username = std::string(username);
    record.password = std::string(password);
    record.director = std::string(director);

    sessions_[record.session_id] = StoredSession{
        .record = record,
        .created_at = Clock::now(),
        .last_used_at = Clock::now(),
    };
    return record.session_id;
  }

  std::optional<ProxyAuthSessionRecord> LookupSession(std::string_view session_id)
  {
    std::lock_guard guard(mutex_);
    const auto now = Clock::now();
    CleanupExpiredLocked(now);

    const auto it = sessions_.find(std::string(session_id));
    if (it == sessions_.end()) { return std::nullopt; }

    it->second.last_used_at = now;
    return it->second.record;
  }

  void UpdateDirector(std::string_view session_id, std::string_view director)
  {
    std::lock_guard guard(mutex_);
    const auto now = Clock::now();
    CleanupExpiredLocked(now);

    const auto it = sessions_.find(std::string(session_id));
    if (it == sessions_.end()) { return; }

    it->second.record.director = std::string(director);
    it->second.last_used_at = now;
  }

  void RemoveSession(std::string_view session_id)
  {
    std::lock_guard guard(mutex_);
    sessions_.erase(std::string(session_id));
  }

 private:
  static std::string GenerateSessionId()
  {
    std::array<unsigned char, 32> random{};
    if (RAND_bytes(random.data(), static_cast<int>(random.size())) != 1) {
      throw std::runtime_error("Failed to generate proxy session id");
    }

    static constexpr char kHex[] = "0123456789abcdef";
    std::string result;
    result.resize(random.size() * 2);
    for (size_t i = 0; i < random.size(); ++i) {
      result[i * 2] = kHex[(random[i] >> 4) & 0x0Fu];
      result[i * 2 + 1] = kHex[random[i] & 0x0Fu];
    }
    return result;
  }

  static bool IsExpired(const StoredSession& session, Clock::time_point now)
  {
    return now - session.last_used_at > kSessionIdleTimeout
           || now - session.created_at > kSessionAbsoluteLifetime;
  }

  void CleanupExpiredLocked(Clock::time_point now)
  {
    for (auto it = sessions_.begin(); it != sessions_.end();) {
      if (IsExpired(it->second, now)) {
        it = sessions_.erase(it);
      } else {
        ++it;
      }
    }
  }

  std::mutex mutex_;
  std::unordered_map<std::string, StoredSession> sessions_;
};

ProxyAuthSessionStoreImpl& GetStore()
{
  static ProxyAuthSessionStoreImpl store;
  return store;
}

std::string BuildCookie(std::string_view session_id, bool secure, bool expired)
{
  std::string cookie = std::string(kProxySessionCookieName) + "=";
  if (!expired) {
    cookie += std::string(session_id);
  }
  cookie += "; Path=/; HttpOnly; SameSite=Strict";
  if (secure) { cookie += "; Secure"; }
  if (expired) {
    cookie += "; Max-Age=0; Expires=Thu, 01 Jan 1970 00:00:00 GMT";
  }
  return cookie;
}

}  // namespace

ProxyAuthSessionStore& ProxyAuthSessionStore::Instance()
{
  static ProxyAuthSessionStore instance;
  return instance;
}

std::string ProxyAuthSessionStore::CreateSession(std::string_view username,
                                                 std::string_view password,
                                                 std::string_view director)
{
  return GetStore().CreateSession(username, password, director);
}

std::optional<ProxyAuthSessionRecord> ProxyAuthSessionStore::LookupSession(
    std::string_view session_id)
{
  return GetStore().LookupSession(session_id);
}

void ProxyAuthSessionStore::UpdateDirector(std::string_view session_id,
                                           std::string_view director)
{
  GetStore().UpdateDirector(session_id, director);
}

void ProxyAuthSessionStore::RemoveSession(std::string_view session_id)
{
  GetStore().RemoveSession(session_id);
}

std::string BuildProxySessionCookie(std::string_view session_id, bool secure)
{
  return BuildCookie(session_id, secure, false);
}

std::string BuildExpiredProxySessionCookie(bool secure)
{
  return BuildCookie("", secure, true);
}
