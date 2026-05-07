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

#ifndef BAREOS_TOOLS_SD_DISCOVERY_CLI_H_
#define BAREOS_TOOLS_SD_DISCOVERY_CLI_H_

#include <optional>
#include <string>
#include <string_view>

#include "stored/sd_discovery_probe.h"

namespace storagedaemon::discoverycli {

enum class ReportSection { kFull, kFilesystems, kTape };

std::optional<ReportSection> ParseReportSection(std::string_view value);
StorageDiscoveryReport FilterDiscoveryReport(StorageDiscoveryReport report,
                                             ReportSection section);
std::string RenderDiscoveryReportJson(const StorageDiscoveryReport& report,
                                      ReportSection section);

}  // namespace storagedaemon::discoverycli

#endif  // BAREOS_TOOLS_SD_DISCOVERY_CLI_H_
