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

using std::chrono::duration;
using std::chrono::minutes;
using std::chrono::steady_clock;
using std::chrono::time_point;

struct ProgressState {
  time_point<steady_clock> start{};
  std::size_t amount{};
  std::size_t ratio{};
};

class Progress {
 public:
  Progress(BareosDb* db, const std::string& table_data);

  void Advance(std::size_t increment);

  std::size_t Rate() const { return state_new.ratio; }
  duration<minutes> RemainingTime() const;

  bool IntegralChange() const { return changed_; }

 private:
  ProgressState state_new;
  ProgressState state_old;
  std::size_t full_amount_{};
  bool changed_{};

  bool is_valid{};
};

#endif  // BAREOS_SRC_DIRD_DBCOPY_PROGRESS_H_
