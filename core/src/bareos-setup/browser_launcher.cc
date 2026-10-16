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
   along with this program; if not, write to the Free Software Foundation,
   Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.
*/
#include "browser_launcher.h"

#include <array>
#include <iostream>
#include <stdexcept>

bool TryOpenBrowser(SetupContext& context, const std::string& url)
{
  const std::array<SetupCommand, 3> commands{XdgOpen({url}), Open({url}),
                                             SensibleBrowser({url})};
  for (const auto& command : commands) {
    if (!context.IsToolAvailable(command.tool)) continue;
    try {
      if (context.Run(command, false, [](std::string_view, std::string_view) {})
          == 0) {
        return true;
      }
    } catch (const std::runtime_error& error) {
      std::cerr << "Could not launch browser: " << error.what() << "\n";
    }
  }
  return false;
}
