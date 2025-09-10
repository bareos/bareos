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

#ifndef BAREOS_PLUGINS_FILED_WINDOWS_DR_RESTORE_H_
#define BAREOS_PLUGINS_FILED_WINDOWS_DR_RESTORE_H_

#include "parser.h"
#include <memory>

namespace barri::restore {
#if defined(HAVE_WIN32)
struct vhdx_directory {
  std::wstring path;
};
struct raw_directory {
  std::wstring path;
};
using disk_ids = std::vector<std::size_t>;

std::unique_ptr<GenericHandler> GetHandler(GenericLogger* logger,
                                           vhdx_directory dir);
std::unique_ptr<GenericHandler> GetHandler(GenericLogger* logger,
                                           raw_directory dir);
std::unique_ptr<GenericHandler> GetHandler(GenericLogger* logger, disk_ids ids);
#else
struct restore_directory {
  std::string_view path;
};

using restore_files = std::span<std::string>;

struct restore_stdout {};

std::unique_ptr<GenericHandler> GetHandler(GenericLogger* logger,
                                           restore_directory dir);
std::unique_ptr<GenericHandler> GetHandler(GenericLogger* logger,
                                           restore_files paths);
std::unique_ptr<GenericHandler> GetHandler(GenericLogger* logger,
                                           restore_stdout stream);
#endif
};  // namespace barri::restore

#endif  // BAREOS_PLUGINS_FILED_WINDOWS_DR_RESTORE_H_
