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
#include "auth_session.h"

#include <algorithm>
#include <array>
#include <limits>
#include <stdexcept>

#include <openssl/rand.h>

ProxySessionStore::ProxySessionStore(std::chrono::seconds ttl,
                                     ProxySessionClock clock)
    : ttl_(ttl), clock_(std::move(clock))
{
}

ProxySession ProxySessionStore::CreateSession(const AuthResult& auth_result,
                                              std::string director_username,
                                              std::string director_password,
                                              std::string preferred_director_id)
{
  const std::time_t now = clock_();

  ProxySession session;
  session.token = GenerateSessionToken();
  session.identity = auth_result.identity;
  session.director_username = std::move(director_username);
  session.director_password = std::move(director_password);
  session.preferred_director_id = std::move(preferred_director_id);
  session.created_at = now;
  session.expires_at = ComputeExpiry(now, auth_result.expires_at);

  std::lock_guard<std::mutex> lock(mutex_);
  RemoveExpiredSessionsLocked(now);
  sessions_[session.token] = session;
  return session;
}

std::optional<ProxySession> ProxySessionStore::GetSession(const std::string& token)
{
  const std::time_t now = clock_();
  std::lock_guard<std::mutex> lock(mutex_);
  RemoveExpiredSessionsLocked(now);

  auto it = sessions_.find(token);
  if (it == sessions_.end()) { return std::nullopt; }
  return it->second;
}

std::optional<ProxySession> ProxySessionStore::RefreshSession(
    const std::string& token,
    const std::optional<std::string>& preferred_director_id)
{
  const std::time_t now = clock_();
  std::lock_guard<std::mutex> lock(mutex_);
  RemoveExpiredSessionsLocked(now);

  auto it = sessions_.find(token);
  if (it == sessions_.end()) { return std::nullopt; }

  if (preferred_director_id && !preferred_director_id->empty()) {
    it->second.preferred_director_id = *preferred_director_id;
  }
  it->second.expires_at = ComputeExpiry(now, std::nullopt);
  return it->second;
}

std::string ProxySessionStore::GenerateSessionToken() const
{
  std::array<unsigned char, 32> bytes{};
  if (RAND_bytes(bytes.data(), bytes.size()) != 1) {
    throw std::runtime_error("Proxy auth: failed to generate session token");
  }

  static constexpr char hex[] = "0123456789abcdef";
  std::string token(bytes.size() * 2, '\0');
  for (size_t i = 0; i < bytes.size(); ++i) {
    token[i * 2] = hex[bytes[i] >> 4];
    token[i * 2 + 1] = hex[bytes[i] & 0x0f];
  }
  return token;
}

std::time_t ProxySessionStore::ComputeExpiry(
    std::time_t now, const std::optional<std::time_t>& upstream_expiry) const
{
  const auto ttl_max = std::chrono::seconds(
      std::max<long long>(ttl_.count(), 1));  // keep sessions positive-lived
  const auto now_seconds = std::chrono::seconds(now);
  const auto session_expiry_seconds = now_seconds + ttl_max;

  if (!upstream_expiry) { return session_expiry_seconds.count(); }

  const auto upstream_seconds = std::chrono::seconds(*upstream_expiry);
  return std::min(session_expiry_seconds, upstream_seconds).count();
}

void ProxySessionStore::RemoveExpiredSessionsLocked(std::time_t now)
{
  for (auto it = sessions_.begin(); it != sessions_.end();) {
    if (it->second.expires_at <= now) {
      it = sessions_.erase(it);
    } else {
      ++it;
    }
  }
}
