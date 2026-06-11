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
#ifndef BAREOS_WEBUI_PROXY_PROXY_AUTH_SESSION_H_
#define BAREOS_WEBUI_PROXY_PROXY_AUTH_SESSION_H_

#include <map>
#include <optional>
#include <string>
#include <string_view>

struct ProxyAuthSessionDirectorRecord {
  std::string username;
  std::string password;
};

struct ProxyAuthSessionRecord {
  std::string session_id;
  std::string current_director;
  std::map<std::string, ProxyAuthSessionDirectorRecord> directors;
};

class ProxyAuthSessionStore {
 public:
  static ProxyAuthSessionStore& Instance();

  std::string CreateSession(std::string_view username,
                            std::string_view password,
                            std::string_view director);
  bool StoreDirectorCredentials(std::string_view session_id,
                                std::string_view director,
                                std::string_view username,
                                std::string_view password,
                                bool make_current = true);
  std::optional<ProxyAuthSessionRecord> LookupSession(
      std::string_view session_id);
  bool SetCurrentDirector(std::string_view session_id, std::string_view director);
  bool RemoveDirector(std::string_view session_id, std::string_view director);
  void RemoveSession(std::string_view session_id);

 private:
  ProxyAuthSessionStore() = default;
};

constexpr std::string_view kProxySessionCookieName = "bareos_proxy_session";
constexpr std::string_view kProxySessionPasswordPlaceholder
    = "__proxy_session__";

std::string BuildProxySessionCookie(std::string_view session_id, bool secure);
std::string BuildExpiredProxySessionCookie(bool secure);

#endif  // BAREOS_WEBUI_PROXY_PROXY_AUTH_SESSION_H_
