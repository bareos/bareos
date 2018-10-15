/**
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2018-2018 Bareos GmbH & Co. KG

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

#include "gtest/gtest.h"
#include "include/bareos.h"
#include "lib/bnet.h"

TEST(BNet, ReadoutCommandIdFromStringTest)
{
  bool ok;
  uint32_t id;

  const std::string message1 {"1000 OK: <director-name> Version: <version>"};
  ok = ReadoutCommandIdFromString(message1, id);
  EXPECT_EQ(id, kMessageIdOk);
  EXPECT_EQ(ok, true);

  const std::string message2 {"1001 OK: <director-name> Version: <version>"};
  ok = ReadoutCommandIdFromString(message2, id);
  EXPECT_NE(id, kMessageIdOk);
  EXPECT_EQ(ok, true);

  const char *m3 {"10A1 OK: <director-name> Version: <version>"};
  const std::string message3 (m3);
  ok = ReadoutCommandIdFromString(message3, id);
  EXPECT_EQ(id, kMessageIdProtokollError);
  EXPECT_EQ(ok, false);
  EXPECT_STREQ(message3.c_str(), m3);
}
