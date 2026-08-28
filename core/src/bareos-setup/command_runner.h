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
 * Command runner: fork/exec a command and stream stdout/stderr lines
 * to a callback as they arrive.
 */
#ifndef BAREOS_BAREOS_SETUP_COMMAND_RUNNER_H_
#define BAREOS_BAREOS_SETUP_COMMAND_RUNNER_H_

#include <functional>
#include <string>
#include <vector>

/** Called for each output line.  stream is "stdout" or "stderr". */
using OutputCallback
    = std::function<void(const std::string& line, const std::string& stream)>;

/**
 * Run a command (argv list), optionally prefixed with "sudo".
 * Calls cb for every output line from stdout and stderr as they arrive.
 * Returns the exit code of the child process.
 * Throws std::runtime_error on fork/exec failure.
 */
int RunCommand(const std::vector<std::string>& argv,
               bool use_sudo,
               OutputCallback cb);

/** Run an argv command while supplying a bounded string on stdin. */
int RunCommandWithInput(const std::vector<std::string>& argv,
                        const std::string& input,
                        bool use_sudo,
                        OutputCallback cb);

/** True if this process is currently running as root (effective UID 0). */
bool IsRoot();

/**
 * Ensure a sudo authentication ticket is cached for the current user,
 * prompting for a password on the controlling terminal if necessary.
 * Returns false if authentication failed (e.g. no controlling terminal,
 * wrong password, or the user is not permitted to use sudo).
 * Always returns true without prompting when already running as root.
 */
bool PrimeSudoTicket();

/**
 * Start a detached background thread that refreshes the sudo ticket
 * every 60 seconds for the remainder of the process lifetime, so that
 * long-running wizard sessions never run into an expired ticket
 * mid-install. No-op when already running as root.
 */
void StartSudoKeepAlive();

#endif  // BAREOS_BAREOS_SETUP_COMMAND_RUNNER_H_
