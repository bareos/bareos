/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2020-2020 Bareos GmbH & Co. KG

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

#ifndef BAREOS_SRC_DIRD_DBCOPY_PROGRESS_H_
#define BAREOS_SRC_DIRD_DBCOPY_PROGRESS_H_

#include <chrono>
#include <ratio>
#include <string>

class BareosDb;

using std::chrono::milliseconds;
using std::chrono::system_clock;
using std::chrono::time_point;

struct ProgressState {
  time_point<system_clock> eta{};
  time_point<system_clock> start{};
  std::size_t amount{};
  std::size_t ratio{};
  milliseconds duration;
};

class Progress {
 public:
  Progress(BareosDb* db,
           const std::string& table_data,
           std::size_t limit_amount_of_rows_);

  bool Increment();

  using Ratio = std::ratio<100, 1>;

  std::size_t Rate() const { return state_.ratio; }
  std::string Eta() const;

 private:
  ProgressState state_;
  ProgressState state_old_;
  std::size_t full_amount_{};
  bool suppress_first_output_{true};
  bool is_valid_{false};
};

#endif  // BAREOS_SRC_DIRD_DBCOPY_PROGRESS_H_
