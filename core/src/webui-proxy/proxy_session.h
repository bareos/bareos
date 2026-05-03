/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2024-2026 Bareos GmbH & Co. KG

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
/**
 * @file
 * Per-connection proxy session.
 *
 * Drives the WebSocket upgrade, handles the auth/command JSON protocol
 * between the browser and the director.
 */
#ifndef BAREOS_WEBUI_PROXY_PROXY_SESSION_H_
#define BAREOS_WEBUI_PROXY_PROXY_SESSION_H_

#include "auth_session.h"
#include "director_connection.h"
#include "proxy_config.h"

#include <memory>
#include <optional>
#include <string>

struct ProxyAuthContext {
  DirectorConfig director_config;
  AuthIdentity identity;
  std::optional<std::time_t> expires_at;
  std::string session_token;
  std::string preferred_director_id;
  bool reused_existing_session{false};
};

/**
 * Handle one browser WebSocket connection until it closes.
 * @p fd  — accepted TCP socket (ownership transferred; will be closed here).
 * @p peer — human-readable "host:port" string for logging.
 */
void RunProxySession(int fd,
                     const std::string& peer,
                     const ProxyConfig& config,
                     const std::shared_ptr<ProxySessionStore>& session_store);

/**
 * Normalize a raw console command while preserving a trailing tab completion
 * marker.
 */
std::string NormalizeRawConsoleCommand(std::string command);

/**
 * Resolve one proxy auth request into a normalized identity and Director
 * connection configuration before any network handoff occurs.
 */
ProxyAuthContext ResolveProxyAuthRequest(
    const ProxyConfig& config,
    const std::shared_ptr<ProxySessionStore>& session_store,
    const std::optional<std::string>& requested_session_token,
    const std::optional<std::string>& requested_access_token,
    const std::optional<std::string>& requested_username,
    const std::optional<std::string>& requested_password,
    const std::optional<std::string>& requested_director,
    const std::optional<std::string>& requested_host,
    const std::optional<int>& requested_port,
    bool json_mode);

#endif  // BAREOS_WEBUI_PROXY_PROXY_SESSION_H_
