/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2024-2024 Bareos GmbH & Co. KG

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
#include <iostream>
#include <string>

using namespace std::string_literals;

int main(int argc, char** argv)
{
  if (argc != 2) { exit(1); }
  std::string cmd{argv[1]};
  if (cmd == "true"s) {
    exit(0);
  } else if (cmd == "false"s) {
    exit(1);
  } else if (cmd == "cat"s) {
    char c;
    while (std::cin.get(c)) { std::cout.put(c); }
    exit(0);
  } else {
    exit(100);
  }
}
