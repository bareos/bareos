/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2019-2019 Bareos GmbH & Co. KG

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

#ifndef BAREOS_LIB_BACKTRACE_H_
#define BAREOS_LIB_BACKTRACE_H_

#include <cstddef>
#include <string>
#include <vector>

struct BacktraceInfo {
  BacktraceInfo(const std::size_t& fn, const std::string& fc)
      : frame_number_(fn), function_call_(fc){};
  std::size_t frame_number_;
  std::string function_call_;
};

std::vector<BacktraceInfo> Backtrace(int skip = 1, int amount = -1);

#endif  // BAREOS_LIB_BACKTRACE_H_
