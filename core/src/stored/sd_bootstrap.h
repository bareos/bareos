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

#ifndef BAREOS_STORED_SD_BOOTSTRAP_H_
#define BAREOS_STORED_SD_BOOTSTRAP_H_

#include <optional>
#include <string>

#include "lib/cli.h"

namespace storagedaemon {

struct BootstrapModeOptions {
  bool enabled{false};
  std::string bootstrap_url{};
  std::string bootstrap_token{};
  std::string bootstrap_session{};
};

struct BootstrapModeOptionHandles {
  CLI::Option* discovery{nullptr};
  CLI::Option* bootstrap_url{nullptr};
  CLI::Option* bootstrap_token{nullptr};
  CLI::Option* bootstrap_session{nullptr};
};

BootstrapModeOptionHandles AddBootstrapOptions(CLI::App& app,
                                               BootstrapModeOptions& options);

std::optional<std::string> ValidateBootstrapOptions(
    const BootstrapModeOptions& options);

int RunStorageDaemonBootstrap(const BootstrapModeOptions& options);

}  // namespace storagedaemon

#endif  // BAREOS_STORED_SD_BOOTSTRAP_H_
