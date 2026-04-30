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
 * HTTP + WebSocket server on a single TCP socket.
 *
 * Plain HTTP GET requests are answered with embedded static assets.
 * Requests with "Upgrade: websocket" are handed off to setup_session.
 */
#ifndef BAREOS_BAREOS_SETUP_HTTP_SERVER_H_
#define BAREOS_BAREOS_SETUP_HTTP_SERVER_H_

#include <functional>
#include <string>

/** Called once per accepted WebSocket connection with the raw fd. */
using WsHandler = std::function<void(int fd)>;

/**
 * Listen on the given TCP port and serve requests.
 * - HTTP GET: returns embedded asset (or index.html for unknown paths).
 * - WebSocket upgrade: calls ws_handler with the connected fd.
 * Runs until the process is killed.
 */
void RunHttpServer(int port, WsHandler ws_handler);

#endif  // BAREOS_BAREOS_SETUP_HTTP_SERVER_H_
