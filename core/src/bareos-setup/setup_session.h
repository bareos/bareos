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
 * Per-connection setup session: decodes JSON commands from the browser
 * and streams responses back over WebSocket.
 */
#ifndef BAREOS_BAREOS_SETUP_SETUP_SESSION_H_
#define BAREOS_BAREOS_SETUP_SETUP_SESSION_H_

#include <string>

/**
 * Handle one WebSocket connection (fd is already upgraded).
 * Reads JSON messages from the browser, executes actions, and sends
 * JSON responses.  Returns when the connection is closed.
 *
 * When dry_run is true, commands are printed to the output stream
 * instead of being executed; exit_code is always reported as 0.
 */
void RunSetupSession(int fd,
                     bool dry_run = false,
                     bool peer_is_loopback = true);

/**
 * Test-only entry point: execute a single setup step (as identified by
 * RunStep() internally) against an already-connected WebSocket fd,
 * without going through the full session/action-dispatch loop. Exists
 * so unit tests can exercise the real step-orchestration logic (which
 * commands get run, and in what order) instead of only the pure
 * command-builder helpers in setup_steps.h. json_message must be a
 * valid JSON object string (e.g. "{}" if the step needs no fields).
 */
int RunSetupStepForTests(int fd,
                         const std::string& step,
                         const std::string& json_message,
                         bool peer_is_loopback = true,
                         bool dry_run = false);

#endif  // BAREOS_BAREOS_SETUP_SETUP_SESSION_H_
