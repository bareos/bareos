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

struct SessionTimeouts {
  std::chrono::steady_clock::duration idle_timeout{std::chrono::minutes(30)};
  std::chrono::steady_clock::duration absolute_lifetime{std::chrono::hours(8)};
};

// Global timeouts with defaults
SessionTimeouts g_session_timeouts;

struct StoredSession {
  ProxyAuthSessionRecord record;
  Clock::time_point created_at;
  Clock::time_point last_used_at;
};

class ProxyAuthSessionStoreImpl {
 public:
  void SetSessionTimeouts(int idle_timeout_minutes,
                          int absolute_lifetime_hours)
  {
    std::lock_guard guard(mutex_);
    g_session_timeouts.idle_timeout
        = std::chrono::minutes(idle_timeout_minutes);
    g_session_timeouts.absolute_lifetime
        = std::chrono::hours(absolute_lifetime_hours);
  }

  void SetSessionTimeoutsForTesting(
      std::chrono::steady_clock::duration idle_timeout,
      std::chrono::steady_clock::duration absolute_lifetime)
  {
    std::lock_guard guard(mutex_);
    g_session_timeouts.idle_timeout = idle_timeout;
    g_session_timeouts.absolute_lifetime = absolute_lifetime;
  }
  std::string CreateSession(std::string_view username,
                            std::string_view password,
                            std::string_view director)
  {
    std::lock_guard guard(mutex_);
    CleanupExpiredLocked(Clock::now());

    ProxyAuthSessionRecord record;
    record.session_id = GenerateSessionId();
    record.current_director = std::string(director);
    record.directors.emplace(
        std::string(director),
        ProxyAuthSessionDirectorRecord{
            .username = std::string(username),
            .password = std::string(password),
        });

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

  bool StoreDirectorCredentials(std::string_view session_id,
                                std::string_view director,
                                std::string_view username,
                                std::string_view password,
                                bool make_current)
  {
    std::lock_guard guard(mutex_);
    const auto now = Clock::now();
    CleanupExpiredLocked(now);

    const auto it = sessions_.find(std::string(session_id));
    if (it == sessions_.end()) { return false; }

    const auto selector = std::string(director);
    it->second.record.directors[selector] = ProxyAuthSessionDirectorRecord{
        .username = std::string(username),
        .password = std::string(password),
    };
    if (make_current || it->second.record.current_director.empty()) {
      it->second.record.current_director = selector;
    }
    it->second.last_used_at = now;
    return true;
  }

  bool SetCurrentDirector(std::string_view session_id, std::string_view director)
  {
    std::lock_guard guard(mutex_);
    const auto now = Clock::now();
    CleanupExpiredLocked(now);

    const auto it = sessions_.find(std::string(session_id));
    if (it == sessions_.end()) { return false; }

    const auto selector = std::string(director);
    if (it->second.record.directors.find(selector)
        == it->second.record.directors.end()) {
      return false;
    }

    it->second.record.current_director = selector;
    it->second.last_used_at = now;
    return true;
  }

  bool RemoveDirector(std::string_view session_id, std::string_view director)
  {
    std::lock_guard guard(mutex_);
    const auto now = Clock::now();
    CleanupExpiredLocked(now);

    const auto it = sessions_.find(std::string(session_id));
    if (it == sessions_.end()) { return false; }

    const auto selector = std::string(director);
    if (it->second.record.directors.erase(selector) == 0) {
      return false;
    }

    if (it->second.record.directors.empty()) {
      sessions_.erase(it);
      return true;
    }

    if (it->second.record.current_director == selector) {
      it->second.record.current_director = it->second.record.directors.begin()->first;
    }
    it->second.last_used_at = now;
    return true;
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
    return now - session.last_used_at > g_session_timeouts.idle_timeout
           || now - session.created_at > g_session_timeouts.absolute_lifetime;
  }

  void CleanupExpiredLocked(Clock::time_point now)
  {
    std::erase_if(sessions_, [this, now](const auto& pair) {
      return IsExpired(pair.second, now);
    });
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
  std::string cookie;
  cookie.reserve(kProxySessionCookieName.size() + session_id.size() + 64);
  cookie.append(kProxySessionCookieName);
  cookie.push_back('=');
  if (!expired) {
    cookie.append(session_id);
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

bool ProxyAuthSessionStore::StoreDirectorCredentials(std::string_view session_id,
                                                     std::string_view director,
                                                     std::string_view username,
                                                     std::string_view password,
                                                     bool make_current)
{
  return GetStore().StoreDirectorCredentials(session_id, director, username,
                                             password, make_current);
}

std::optional<ProxyAuthSessionRecord> ProxyAuthSessionStore::LookupSession(
    std::string_view session_id)
{
  return GetStore().LookupSession(session_id);
}

bool ProxyAuthSessionStore::SetCurrentDirector(std::string_view session_id,
                                               std::string_view director)
{
  return GetStore().SetCurrentDirector(session_id, director);
}

bool ProxyAuthSessionStore::RemoveDirector(std::string_view session_id,
                                           std::string_view director)
{
  return GetStore().RemoveDirector(session_id, director);
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

void ProxyAuthSessionStore::SetSessionTimeouts(int idle_timeout_minutes,
                                               int absolute_lifetime_hours)
{
  return GetStore().SetSessionTimeouts(idle_timeout_minutes,
                                       absolute_lifetime_hours);
}

void ProxyAuthSessionStore::SetSessionTimeoutsForTesting(
    std::chrono::steady_clock::duration idle_timeout,
    std::chrono::steady_clock::duration absolute_lifetime)
{
  return GetStore().SetSessionTimeoutsForTesting(idle_timeout,
                                                 absolute_lifetime);
}
