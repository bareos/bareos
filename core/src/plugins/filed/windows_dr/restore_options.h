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

#ifndef BAREOS_PLUGINS_FILED_WINDOWS_DR_RESTORE_OPTIONS_H_
#define BAREOS_PLUGINS_FILED_WINDOWS_DR_RESTORE_OPTIONS_H_

#include "parser.h"

#if defined(HAVE_WIN32)
struct restore_options {};
#else

struct restore_directory {
  std::string_view path;
};

using restore_files = std::span<std::string>;

struct restore_options {
  GenericLogger* logger;

  std::variant<restore_directory, restore_files> location;

  restore_options(restore_options&&) = default;
  restore_options(const restore_options&) = default;

  restore_options& operator=(restore_options&&) = default;
  restore_options& operator=(const restore_options&) = default;

  static restore_options into_directory(GenericLogger* logger,
                                        std::string_view dir)
  {
    return restore_options{logger, restore_directory{dir}};
  }

 private:
  restore_options(GenericLogger* logger_,
                  std::variant<restore_directory, restore_files> location_)
      : logger{logger_}, location{location_}
  {
  }
};
#endif

#endif  // BAREOS_PLUGINS_FILED_WINDOWS_DR_RESTORE_OPTIONS_H_
