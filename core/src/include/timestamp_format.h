/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2023-2023 Bareos GmbH & Co. KG

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

#ifndef BAREOS_INCLUDE_TIMESTAMP_FORMAT_H_
#define BAREOS_INCLUDE_TIMESTAMP_FORMAT_H_
struct TimestampFormat {
  // %z is missing because it is not correctly implemented in windows :(
  // We implement the %z ourselves and add add the timezone in the format +0000
  // to all date strings

  // default format
  // 2023-08-24T17:49:24+0200
  static constexpr const char* Default{"%Y-%m-%dT%H:%M:%S"};

  // for the scheduler preview
  // Fri 06-Oct-2023 02:05+0200
  static constexpr const char* SchedPreview{"%a %d-%b-%Y %H:%M"};

  // for use in TO_CHAR database queries
  // 2023-10-05T08:57:39+0200
  static constexpr const char* Database{"YYYY-MM-DD\"T\"HH24:MI:SSTZHTZM"};

  // to create filenames, so only what is allowed in filenames also on windows
  // 2023-08-24T17.49.24+0200
  static constexpr const char* Filename{"%Y-%m-%dT%H.%M.%S"};
};
#endif  // BAREOS_INCLUDE_TIMESTAMP_FORMAT_H_
