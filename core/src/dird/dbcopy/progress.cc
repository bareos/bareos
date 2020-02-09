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

#include <iostream>
#include <sstream>

struct Amount {
  bool is_valid{};
  std::size_t amount{};
};

static int AmountHandler(void* ctx, int fields, char** row)
{
  Amount* a = static_cast<Amount*>(ctx);
  std::istringstream iss(row[0]);
  iss >> a->amount;
  a->is_valid = !iss.fail();
  return 1;
}

Progress::Progress(BareosDb* db, const std::string& table_name)
{
  Amount a;
  std::string query{"SELECT count(*) from " + table_name};

  if (!db->SqlQuery(query.c_str(), AmountHandler, &a)) {
    std::cout << "Could not select number of rows for: " << table_name
              << std::endl;
    return;
  }

  if (a.is_valid) {
    state_old.start = steady_clock::now();
    state_old.amount = a.amount;
    is_valid = true;
  }
}

void Progress::Advance(std::size_t increment)
{
  using Ratio = std::ratio<100, 1>;

  state_new.amount = state_old.amount - increment;
  state_new.start = steady_clock::now();

  state_new.ratio =
      (state_new.amount * Ratio::num) / (Ratio::den * state_old.amount);

  auto duration = state_new.start - state_old.start;

  if (state_new.ratio != state_old.ratio) { changed_ = true; }

  state_old = state_new;
}
