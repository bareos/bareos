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

#ifndef BAREOS_PLUGINS_FILED_WINDOWS_DR_FORMAT_H_
#define BAREOS_PLUGINS_FILED_WINDOWS_DR_FORMAT_H_
#if defined(USE_STD_FMT)
#  include <format>
namespace libbareos {
using std::format;
using std::format_string;
using std::make_format_args;
using std::println;
using std::vformat;
}  // namespace libbareos
#else
#  include <fmt/format.h>
#  include <fmt/xchar.h>
namespace libbareos {
using fmt::format;
using fmt::format_string;
using fmt::make_format_args;
using fmt::println;
using fmt::vformat;
}  // namespace libbareos
#endif

#endif  // BAREOS_PLUGINS_FILED_WINDOWS_DR_FORMAT_H_
