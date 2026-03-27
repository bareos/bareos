/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2024-2025 Bareos GmbH & Co. KG

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
 * Per-connection setup session: decodes JSON commands from the browser
 * and streams responses back over WebSocket.
 */
#ifndef BAREOS_BAREOS_SETUP_SETUP_SESSION_H_
#define BAREOS_BAREOS_SETUP_SETUP_SESSION_H_

/**
 * Handle one WebSocket connection (fd is already upgraded).
 * Reads JSON messages from the browser, executes actions, and sends
 * JSON responses.  Returns when the connection is closed.
 *
 * When dry_run is true, commands are printed to the output stream
 * instead of being executed; exit_code is always reported as 0.
 */
void RunSetupSession(int fd, bool dry_run = false);

#endif  // BAREOS_BAREOS_SETUP_SETUP_SESSION_H_
