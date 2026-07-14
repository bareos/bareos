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
/**
 * @file
 * Per-connection proxy session.
 *
 * Drives the WebSocket upgrade, handles the auth/command JSON protocol
 * between the browser and the director.
 */
#ifndef BAREOS_WEBUI_PROXY_PROXY_SESSION_H_
#define BAREOS_WEBUI_PROXY_PROXY_SESSION_H_

#include "proxy_config.h"

#include <string>
#include <string_view>

/**
 * Handle one browser WebSocket connection until it closes.
 * @p fd  — accepted TCP socket (ownership transferred; will be closed here).
 * @p peer — human-readable "host:port" string for logging.
 */
void RunProxySession(int fd,
                     const std::string& peer,
                     const ProxyConfig& config);

/**
 * Normalize a raw console command while preserving a trailing tab completion
 * marker.
 */
std::string NormalizeRawConsoleCommand(std::string_view command);

/**
 * Return true when a command is expected to terminate the console session from
 * the main Bareos prompt.
 */
bool IsExpectedConsoleExitCommand(bool at_main_prompt,
                                  std::string_view command);

/**
 * Return true when a raw text chunk is just an out-of-band console
 * notification and should not be shown as part of the command output.
 */
bool ShouldSuppressRawConsoleChunk(std::string_view command,
                                   std::string_view chunk);

/**
 * Remove out-of-band console notifications from raw command output while
 * preserving the actual command result text.
 */
std::string FilterRawConsoleChunk(std::string_view command,
                                  std::string_view chunk);

// Normalize console text so it can be safely embedded in JSON responses.
std::string NormalizeJsonText(std::string_view text);

#endif  // BAREOS_WEBUI_PROXY_PROXY_SESSION_H_
