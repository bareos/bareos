/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2023-2024 Bareos GmbH & Co. KG

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

#if defined(HAVE_MINGW)
#  include "include/bareos.h"
#  include "gtest/gtest.h"
#  include "gmock/gmock.h"
#else
#  include "gtest/gtest.h"
#  include "gmock/gmock.h"
#  include "include/bareos.h"
#endif

#include "stored/backends/util.h"

#include <iostream>

TEST(parse_device_options, Empty)
{
  auto parsed = util::options::parse_options("");

  auto* error = std::get_if<util::options::error>(&parsed);

  EXPECT_STREQ(error ? error->c_str() : "", "");

  if (error) { return; }

  auto& map = std::get<util::options::options>(parsed);

  EXPECT_EQ(map.size(), 0);
}

TEST(parse_device_options, CorrectParse)
{
  auto parsed = util::options::parse_options("blocksize=3,other option=string");

  auto* error = std::get_if<util::options::error>(&parsed);

  EXPECT_STREQ(error ? error->c_str() : "", "");

  if (error) { return; }

  auto& map = std::get<util::options::options>(parsed);
  EXPECT_EQ(map.size(), 2);

  {
    auto iter = map.find("blocksize");
    EXPECT_NE(iter, map.end());
    if (iter != map.end()) {
      EXPECT_EQ(iter->first, std::string_view{"blocksize"});
      EXPECT_EQ(iter->second, std::string_view{"3"});
    }
  }

  {
    auto iter = map.find("other option");
    EXPECT_NE(iter, map.end());
    if (iter != map.end()) {
      EXPECT_EQ(iter->first, std::string_view{"other option"});
      EXPECT_EQ(iter->second, std::string_view{"string"});
    }
  }
}

TEST(parse_device_options, WeirdSpelling)
{
  auto parsed = util::options::parse_options("  _ BlOcK _ SiZe _  =3");

  auto* error = std::get_if<util::options::error>(&parsed);

  EXPECT_STREQ(error ? error->c_str() : "", "");

  if (error) { return; }

  auto& map = std::get<util::options::options>(parsed);
  EXPECT_EQ(map.size(), 1);

  {
    auto iter = map.find("blocksize");
    EXPECT_NE(iter, map.end());
    if (iter != map.end()) { EXPECT_EQ(iter->second, std::string_view{"3"}); }
  }
}

TEST(parse_device_options, EmptyKey)
{
  auto parsed = util::options::parse_options("=3");

  auto* error = std::get_if<util::options::error>(&parsed);

  EXPECT_NE(error, nullptr);

  if (!error) { return; }

  EXPECT_NE(error->find("[=3]"), error->npos);
  EXPECT_NE(error->find("(key is empty)"), error->npos);
}

TEST(parse_device_options, EmptyVal)
{
  auto parsed = util::options::parse_options("blocksize=");

  auto* error = std::get_if<util::options::error>(&parsed);

  EXPECT_NE(error, nullptr);

  if (!error) { return; }

  EXPECT_NE(error->find("[blocksize=]"), error->npos);
  EXPECT_NE(error->find("(val is empty)"), error->npos);
}

TEST(parse_device_options, DuplicateKey)
{
  std::string first = "blocksize";
  std::string second = "_Block_Size_";
  auto parsed = util::options::parse_options(first + "=3," + second + "=4");

  auto* error = std::get_if<util::options::error>(&parsed);

  EXPECT_NE(error, nullptr);

  if (!error) { return; }

  std::string first_highlighted = "[" + first + "]";
  std::string second_highlighted = "[" + second + "]";

  EXPECT_NE(error->find(first_highlighted), error->npos);
  EXPECT_NE(error->find(second_highlighted, error->find(first_highlighted) + 1),
            error->npos);
  EXPECT_NE(error->find("(duplicate key)"), error->npos);
}