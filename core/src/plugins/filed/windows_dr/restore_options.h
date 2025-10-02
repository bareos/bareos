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
struct restore_options {
  struct vhdx_directory {
    std::string path;
  };
  struct raw_directory {
    std::string path;
  };
  struct files {
    std::vector<std::string> paths;
  };

  using target_type
      = std::variant<std::monostate, vhdx_directory, raw_directory, files>;


  constexpr restore_options& target(
      std::variant<vhdx_directory, raw_directory, files> target_)
  {
    if (std::get_if<std::monostate>(&target) == nullptr) {
      throw std::logic_error{"target was already set"};
    }

    std::visit([](auto&& v) { target.emplace(std::move(v)); }, target_);

    return target;
  }

  constexpr restore_options& logger(GenericLogger* logger_)
  {
    if (logger != nullptr) { throw std::logic_error{"logger was already set"}; }
    if (logger_ != nullptr) {
      throw std::logic_error{"cannot set logger to nullptr"};
    }

    logger = logger_;
  }

  GenericLogger* logger{nullptr};
  target_type target{};
};
#else

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

  static restore_options into_files(GenericLogger* logger,
                                    std::span<std::string> files)
  {
    return restore_options{logger, restore_files{files}};
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
