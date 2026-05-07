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

#include "tools/sd_discovery_cli.h"

namespace storagedaemon::discoverycli {

std::optional<ReportSection> ParseReportSection(std::string_view value)
{
  if (value == "full") { return ReportSection::kFull; }
  if (value == "filesystems") { return ReportSection::kFilesystems; }
  if (value == "tape") { return ReportSection::kTape; }
  return std::nullopt;
}

StorageDiscoveryReport FilterDiscoveryReport(StorageDiscoveryReport report,
                                             ReportSection section)
{
  switch (section) {
    case ReportSection::kFull:
      return report;
    case ReportSection::kFilesystems:
      report.tape_devices.clear();
      report.changers.clear();
      return report;
    case ReportSection::kTape:
      report.filesystems.clear();
      return report;
  }

  return report;
}

std::string RenderDiscoveryReportJson(const StorageDiscoveryReport& report,
                                      ReportSection section)
{
  return StorageDiscoveryReportToJson(
      FilterDiscoveryReport(StorageDiscoveryReport{report}, section));
}

}  // namespace storagedaemon::discoverycli
