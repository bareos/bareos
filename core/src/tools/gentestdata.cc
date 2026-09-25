/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2023-2026 Bareos GmbH & Co. KG

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
#include <cstdio>

#if defined(HAVE_WIN32)
#  include <fcntl.h>
#  include <io.h>
#endif

static void print_random(long);

int main(int argc, char** argv)
{
#if defined(HAVE_WIN32)
  // stdout defaults to text mode on Windows, which silently rewrites
  // every 0x0A byte in our pseudo-random binary output to 0x0D 0x0A,
  // inflating the resulting file's size. Since callers (systemtests)
  // rely on generating files of an exact, known size, stdout must be
  // switched to binary mode before any data is written.
  _setmode(_fileno(stdout), _O_BINARY);
#endif

  CLI::App app{"Generate a stream of pseudo-random testdata"};

  constexpr bool k_is_1000 = false;
  long bytes{1024};
  app.add_option("-s,--size", bytes, "Number of bytes to create")
      ->transform(CLI::AsSizeValue{k_is_1000})
      ->check(CLI::PositiveNumber);

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
