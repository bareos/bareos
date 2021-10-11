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

#include "include/bareos.h"
#include "cats/cats.h"
#include "dird/dbcopy/progress.h"
#include "include/make_unique.h"

#include <array>
#include <iomanip>
#include <iostream>
#include <locale>
#include <sstream>
#include <thread>

struct Amount {
  bool is_valid{};
  std::size_t amount{};
};

static int AmountHandler(void* ctx, int /*fields*/, char** row)
{
  auto* a = static_cast<Amount*>(ctx);
  std::istringstream iss(row[0]);
  iss >> a->amount;
  a->is_valid = !iss.fail();
  return 1;
}

Progress::Progress(BareosDb* db,
                   const std::string& table_name,
                   std::size_t limit_amount_of_rows_)
{
  std::cout << "--> requesting number of rows to be copied..." << std::endl;

  Amount a;
  std::string query{"SELECT count(*) from " + table_name};

  if (!db->SqlQuery(query.c_str(), AmountHandler, &a)) {
    std::cout << "Could not select number of rows for: " << table_name
              << std::endl;
    return;
  }

  if (a.is_valid) {
    if (limit_amount_of_rows_ != 0 && a.amount > limit_amount_of_rows_) {
      // commandline parameter
      a.amount = limit_amount_of_rows_;
    }
    full_amount_ = a.amount;
    state_.amount = a.amount;
    start_time_ = system_clock::now();
    state_.start = system_clock::now();
    state_old_ = state_;
    is_valid_ = true;
  }
}

bool Progress::Increment()
{
  if (!is_valid_) {
    // if anything failed before do not abort, just omit the progress counter
    return false;
  }

  if (state_.amount != 0) { --state_.amount; }
  if ((state_.amount % number_of_rows_per_increment_) != 0) { return false; }

  state_.start = system_clock::now();

  milliseconds duration = std::chrono::duration_cast<milliseconds>(
      state_.start - state_old_.start);

  if (is_initial_run_) {
    // no average
    state_.duration = duration;
    is_initial_run_ = false;
  } else {
    // averaging
    state_.duration = (state_old_.duration + duration) / 2;
  }

  auto remaining_time = (state_.duration)
                        * (state_.amount / (state_old_.amount - state_.amount));
  remaining_seconds_ = std::chrono::duration_cast<seconds>(remaining_time);

  state_.eta = system_clock::now() + remaining_time;

  state_.ratio
      = Ratio::num - (state_.amount * Ratio::num) / (full_amount_ * Ratio::den);

  bool changed
      = (state_.ratio != state_old_.ratio) || (state_.eta != state_old_.eta);

  state_old_ = state_;

  return changed;
}

struct separate_thousands : std::numpunct<char> {
  char_type do_thousands_sep() const override { return '\''; }
  string_type do_grouping() const override { return "\3"; }
};

std::string Progress::FullAmount() const
{
  auto thousands = std::make_unique<separate_thousands>();
  std::stringstream ss;
  ss.imbue(std::locale(std::cout.getloc(), thousands.release()));
  ss << full_amount_;
  return ss.str();
}

static std::string FormatTime(time_point<system_clock> tp)
{
  std::time_t time = system_clock::to_time_t(tp);
  std::array<char, Progress::default_timestring_size> buffer{};

  std::strftime(buffer.data(), buffer.size(), "%Y-%m-%d %H:%M:%S",
                std::localtime(&time));

  return std::string(buffer.data());
}

std::string Progress::Eta() const { return FormatTime(state_.eta); }

std::string Progress::TimeOfDay() { return FormatTime(system_clock::now()); }

std::string Progress::StartTime() const { return FormatTime(start_time_); }

struct Time {
  explicit Time(seconds duration)
  {
    hours = static_cast<int>(duration.count() / 3600);
    min = static_cast<int>((duration.count() % 3600) / 60);
    sec = static_cast<int>(duration.count() % 60);
  }
  int hours, min, sec;
};

std::string Progress::RemainingHours() const
{
  Time duration(remaining_seconds_);

  std::array<char, default_timestring_size> buffer{};
  sprintf(buffer.data(), "%0dh%02dm%02ds", duration.hours, duration.min,
          duration.sec);
  return std::string(buffer.data());
}

std::string Progress::RunningHours() const
{
  Time duration(
      std::chrono::duration_cast<seconds>(system_clock::now() - start_time_));

  std::array<char, default_timestring_size> buffer{};
  sprintf(buffer.data(), "%0dh%02dm%02ds", duration.hours, duration.min,
          duration.sec);
  return std::string(buffer.data());
}
