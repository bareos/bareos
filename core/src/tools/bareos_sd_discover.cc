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

#include <clocale>
#include <iostream>

#include "include/bareos.h"
#include "include/exit_codes.h"
#include "lib/cli.h"
#include "tools/sd_discovery_cli.h"

namespace {

constexpr const char* kToolDescription
    = "Print storage discovery information as JSON.";

}  // namespace

int main(int argc, char** argv)
{
  setlocale(LC_ALL, "");
  tzset();
  bindtextdomain("bareos", LOCALEDIR);
  textdomain("bareos");

  CLI::App app;
  InitCLIApp(app, kToolDescription);

  std::string format{"json"};
  std::string section{"full"};

  app.add_option("--format", format, "Output format.")
      ->check(CLI::IsMember({"json"}));
  app.add_option("-s,--section", section, "Discovery section to print.")
      ->check(CLI::IsMember({"full", "filesystems", "tape"}));

  ParseBareosApp(app, argc, argv);

  auto parsed_section = storagedaemon::discoverycli::ParseReportSection(section);
  if (!parsed_section) {
    std::cerr << "Unsupported discovery section: " << section << "\n";
    return BEXIT_FAILURE;
  }

  if (format != "json") {
    std::cerr << "Unsupported output format: " << format << "\n";
    return BEXIT_FAILURE;
  }

  const auto report = storagedaemon::ProbeStorageDiscoveryReport();
  std::cout << storagedaemon::discoverycli::RenderDiscoveryReportJson(
                   report, *parsed_section)
            << "\n";
  return BEXIT_SUCCESS;
}
