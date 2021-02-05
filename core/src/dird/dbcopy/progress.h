/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2020-2021 Bareos GmbH & Co. KG

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

#ifndef BAREOS_DIRD_DBCOPY_PROGRESS_H_
#define BAREOS_DIRD_DBCOPY_PROGRESS_H_

#include <chrono>
#include <ratio>
#include <string>

class BareosDb;

using std::chrono::milliseconds;
using std::chrono::seconds;
using std::chrono::system_clock;
using std::chrono::time_point;

struct ProgressState {
  time_point<system_clock> eta{};
  time_point<system_clock> start{};
  std::size_t amount{};
  std::size_t ratio{};
  milliseconds duration{};
};

class Progress {
 public:
  Progress(BareosDb* db,
           const std::string& table_name,
           std::size_t limit_amount_of_rows_);

  bool Increment();

  using Ratio = std::ratio<100, 1>;
  static constexpr std::size_t number_of_rows_per_increment_ = 800000;

  std::size_t FullNumberOfRows() const { return full_amount_; }
  std::size_t Rate() const { return state_.ratio; }

  std::string Eta() const;
  std::string FullAmount() const;
  std::string RunningHours() const;
  std::string RemainingHours() const;
  std::string StartTime() const;
  static std::string TimeOfDay();
  static constexpr std::size_t default_timestring_size = 100;

 private:
  ProgressState state_;
  ProgressState state_old_;
  time_point<system_clock> start_time_{};
  seconds remaining_seconds_{};
  std::size_t full_amount_{};
  bool is_valid_{false};
  bool is_initial_run_{true};
};

#endif  // BAREOS_DIRD_DBCOPY_PROGRESS_H_
