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


#include "CLI/CLI.hpp"
#include "CLI/App.hpp"
#include "CLI/Config.hpp"
#include "CLI/Formatter.hpp"

static inline std::string version_text()
{
#if !defined(BARRI_VERSION)
#  warning "no barri version defined"
#  define BARRI_VERSION "unknown"
#endif
#if !defined(BARRI_DATE)
#  warning "no barri date defined"
#  define BARRI_DATE "unknown"
#endif
#if !defined(BARRI_YEAR)
#  warning "no barri year defined"
#  define BARRI_YEAR "unknown"
#endif
  return libbareos::format(R"(barri - bareos recovery imager
Version {} ({})
Copyright (C) 2025-{} Bareos GmbH & Co. KG)",
                           BARRI_VERSION, BARRI_DATE, BARRI_YEAR);
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

  std::string make_description(const CLI::App*) const override
  {
    return long_description;
  }

 private:
  std::string long_description;
};

#endif  // BAREOS_PLUGINS_FILED_WINDOWS_DR_BARRI_CLI_H_
