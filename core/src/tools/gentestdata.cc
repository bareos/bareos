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
#include "lib/cli.h"
#include <random>
#include <cstring>

static void print_random(long);

int main(int argc, char** argv)
{
  CLI::App app{"Generate a stream of pseudo-random testdata"};

  long bytes{1024};
  app.add_option("-s,--size", bytes, "Number of bytes to create");

  CLI11_PARSE(app, argc, argv);

  print_random(bytes);
  return 0;
}


void print_random(long bytes)
{
  std::mt19937_64 generator{};
  using val_type = decltype(generator());

  char buf[sizeof(val_type)];
  for (auto remaining = bytes; remaining > 0; remaining -= sizeof(val_type)) {
    auto value = generator();
    memcpy(buf, &value, sizeof(val_type));
    for (auto i = 0u; i < sizeof(val_type) && remaining - i > 0; i++) {
      std::putchar(buf[i]);
    }
  }
}
