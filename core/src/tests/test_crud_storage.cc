/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2026-2026 Bareos GmbH & Co. KG

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

#include "stored/backends/crud_storage.h"

#include <algorithm>
#include <chrono>
#include <string>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace {
std::string BackendProgram()
{
  std::string program{TEST_PROGRAM};
#if defined(HAVE_WIN32)
  std::replace(program.begin(), program.end(), '/', '\\');
#endif
  return program;
}

#if defined(HAVE_WIN32)
// stat() on Windows resolves file attributes via function pointers that are
// only populated by OSDependentInit(). Without this call every stat() fails,
// regardless of whether the path is valid.
[[maybe_unused]] static bool setup = (OSDependentInit(), true);
#endif
}  // namespace

TEST(crud_storage, list_keeps_backend_alive_while_reading)
{
#if defined(HAVE_WIN32)
  exit(77);
#else
  CrudStorage storage;
  ASSERT_TRUE(storage.set_program(BackendProgram()).has_value());
  storage.set_program_timeout(std::chrono::seconds{4});

  auto result = storage.list("delayed");

  ASSERT_TRUE(result.has_value()) << result.error();
  EXPECT_EQ(result->size(), 6);
  EXPECT_TRUE(result->contains("one"));
  EXPECT_TRUE(result->contains("six"));
#endif
}

TEST(crud_storage, list_rejects_trailing_record_data)
{
  CrudStorage storage;
  ASSERT_TRUE(storage.set_program(BackendProgram()).has_value());

  auto result = storage.list("trailing-data");

  ASSERT_FALSE(result.has_value());
  EXPECT_THAT(result.error(), testing::HasSubstr("could not parse data"));
}
