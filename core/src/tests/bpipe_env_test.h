/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2019-2024 Bareos GmbH & Co. KG

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

#ifndef BAREOS_TESTS_BPIPE_ENV_TEST_H_
#define BAREOS_TESTS_BPIPE_ENV_TEST_H_

#include <string>
#include <string_view>

constexpr const char* environment_key = "MyVarA";
#if HAVE_WIN32
constexpr const char* env_tester = "env_tester.exe";
#else
constexpr const char* env_tester = "./env_tester";
#endif

inline std::string format(std::string_view val)
{
  using namespace std::literals;
  return "Got Value '"s + std::string(val) + "'"s;
}

#endif  // BAREOS_TESTS_BPIPE_ENV_TEST_H_
