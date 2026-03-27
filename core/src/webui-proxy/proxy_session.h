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

#include <string>

struct DefaultDirectorConfig {
  std::string host;
  int port{9101};
  std::string name;
};

/**
 * Handle one browser WebSocket connection until it closes.
 * @p fd  — accepted TCP socket (ownership transferred; will be closed here).
 * @p peer — human-readable "host:port" string for logging.
 */
void RunProxySession(int fd,
                     const std::string& peer,
                     const DefaultDirectorConfig& defaults);

#endif  // BAREOS_WEBUI_PROXY_PROXY_SESSION_H_
