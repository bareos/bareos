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
 * Shared command builders for the setup wizard steps.
 * Used by both the WebSocket session handler and the TUI wizard.
 */
#ifndef BAREOS_BAREOS_SETUP_SETUP_STEPS_H_
#define BAREOS_BAREOS_SETUP_SETUP_STEPS_H_

#include <string>
#include <vector>

/** Capitalize the first letter (Bareos repo paths use e.g. "Fedora_43"). */
std::string CapFirst(std::string s);

/**
 * Build the command to download and run add_bareos_repositories.sh.
 * For subscription repos, curl uses -u login:password.
 */
std::vector<std::string> BuildAddRepoCmd(const std::string& distro,
                                         const std::string& version,
                                         const std::string& repo_type,
                                         const std::string& login,
                                         const std::string& password);

/**
 * Build the package install command for the detected package manager.
 */
std::vector<std::string> BuildInstallCmd(
    const std::string& pkg_mgr,
    const std::vector<std::string>& packages);

/**
 * Build the database initialization command (three Bareos scripts).
 */
std::vector<std::string> BuildDbCmd();

/**
 * Build the command to create a Bareos WebUI admin console user.
 */
std::vector<std::string> BuildAdminUserCmd(const std::string& username,
                                           const std::string& password);

#endif  // BAREOS_BAREOS_SETUP_SETUP_STEPS_H_
