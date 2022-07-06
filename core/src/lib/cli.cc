/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2022-2022 Bareos GmbH & Co. KG

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

#include "lib/bnet_network_dump_private.h"
#include "cli.h"
#include "lib/bnet_network_dump.h"
#include "lib/version.h"
#include "lib/message.h"

void InitCLIApp(CLI::App& app, std::string description, int fsfyear)
{
  if (fsfyear) {
    std::vector<char> copyright(1024);
    kBareosVersionStrings.FormatCopyrightWithFsfAndPlanets(
        copyright.data(), copyright.size(), fsfyear);

    description += "\n" + std::string(copyright.data());
  }

  app.description(description);
  app.set_help_flag("-h,--help,-?", "Print this help message and exit.");
  app.set_version_flag("--version", kBareosVersionStrings.Full);
  app.get_formatter()->column_width(40);
#ifdef HAVE_WIN32
  app.allow_windows_style_options();
#endif
}

void AddDebugOptions(CLI::App& app)
{
  app.add_option("-d,--debug-level", debug_level, "Set debug level to <level>.")
      ->check(CLI::NonNegativeNumber)
      ->type_name("<level>");

  app.add_flag("--dt,--debug-timestamps", dbg_timestamp,
               "Print timestamps in debug output.");
}

void AddVerboseOption(CLI::App& app)
{
  app.add_flag("-v,--verbose", verbose, "Verbose user messages.");
}

void AddNetworkDebuggingOption(CLI::App& app)
{
  app.add_flag("--zp,--plantuml-mode", BnetDumpPrivate::plantuml_mode_,
               "Activate plant UML.")
      ->group("");  // add it to empty group to hide the option from help

  app.add_option("--zs,--set-dump-stack-level-amount",
                 BnetDumpPrivate::stack_level_amount_,
                 "Set stack level amount.")
      ->group("");  // add it to empty group to hide the option from help

  app.add_option("--zf,--set-dump-filename", BnetDumpPrivate::filename_,
                 "Set file name.")
      ->group("");  // add it to empty group to hide the option from help
}
