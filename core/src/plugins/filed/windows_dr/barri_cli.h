/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2025-2025 Bareos GmbH & Co. KG

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

#ifndef BAREOS_PLUGINS_FILED_WINDOWS_DR_BARRI_CLI_H_
#define BAREOS_PLUGINS_FILED_WINDOWS_DR_BARRI_CLI_H_

#include <string>
#include "format.h"
#include "version.h"

#include "CLI/CLI.hpp"
#include "CLI/App.hpp"
#include "CLI/Config.hpp"
#include "CLI/Formatter.hpp"

static inline std::string version_text()
{
  return libbareos::format(R"(barri - bareos recovery imager
Version {} ({})
Copyright (C) 2025-{} Bareos GmbH & Co. KG
Get professional support from https://www.bareos.com)",
                           barri_version, barri_date, barri_year);
}

class SubcommandFormatter : public CLI::Formatter {
 public:
  SubcommandFormatter() : Formatter()
  {
    std::stringstream out;
    out << version_text() << "\n\n";
    long_description = out.str();
  }

  SubcommandFormatter(std::string long_desc) : Formatter()
  {
    std::stringstream out;
    out << version_text() << "\n\n" << long_desc << "\n";
    long_description = out.str();
  }

  std::string make_help(const CLI::App* app,
                        std::string name,
                        CLI::AppFormatMode mode) const override
  {
    std::cout << "( " << (void*)app << ", " << name << ", " << mode << " )"
              << std::endl;
    return Formatter::make_help(app, name, mode);
  }

#if CLI11_VERSION_MAJOR >= 2 && CLI11_VERSION_MINOR >= 5
  std::string make_expanded(const CLI::App* sub,
                            CLI::AppFormatMode AppFormatMode) const override
  {
    CLI::Formatter formatter{};
    return formatter.make_expanded(sub, AppFormatMode);
  }
#else
  std::string make_expanded(const CLI::App* sub) const override
  {
    CLI::Formatter formatter{};
    return formatter.make_expanded(sub);
  }
#endif

  std::string make_description(const CLI::App* app) const override
  {
    auto desc = Formatter::make_description(app);
    std::cout << "pointer:" << (void*)app << std::endl;
    return long_description;
  }

 private:
  std::string long_description;
};

#endif  // BAREOS_PLUGINS_FILED_WINDOWS_DR_BARRI_CLI_H_
