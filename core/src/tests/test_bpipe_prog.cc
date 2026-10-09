/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2024-2026 Bareos GmbH & Co. KG

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
#include <cstdlib>
#include <chrono>
#include <iostream>
#include <string>
#include <thread>

using namespace std::string_literals;

int main(int argc, char** argv)
{
  if (argc < 2) { exit(1); }
  std::string cmd{argv[1]};
  if (cmd == "true"s) {
    exit(0);
  } else if (cmd == "false"s) {
    exit(1);
  } else if (cmd == "cat"s) {
    char c;
    while (std::cin.get(c)) { std::cout.put(c); }
    exit(0);
  } else if (cmd == "list"s && argc == 3 && argv[2] == "delayed"s) {
    using namespace std::chrono_literals;
    for (auto entry :
         {"one 1\n", "two 2\n", "three 3\n", "four 4\n", "five 5\n"}) {
      std::cout << entry << std::flush;
      std::this_thread::sleep_for(1s);
    }
    std::cout << "six 6\n";
    exit(0);
  } else if (cmd == "list"s && argc == 3 && argv[2] == "trailing-data"s) {
    std::cout << "part 1 trailing\n";
    exit(0);
  } else {
    exit(100);
  }
}
