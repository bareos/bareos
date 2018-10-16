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
#define BAREOS_TEST_LIB
#include "lib/bnet.h"

TEST(BNet, ReadoutCommandIdFromStringTest)
{
  bool ok;
  uint32_t id;

  std::string message1 = "1000";
  message1 += 0x1e;
  message1 += "OK: <director-name> Version: <version>";
  ok = ReadoutCommandIdFromMessage(message1, id);
  EXPECT_EQ(id, kMessageIdOk);
  EXPECT_EQ(ok, true);

  std::string message2 = "1001";
  message2 += 0x1e;
  message2 += "OK: <director-name> Version: <version>";
  ok = ReadoutCommandIdFromMessage(message2, id);
  EXPECT_NE(id, kMessageIdOk);
  EXPECT_EQ(ok, true);
}

TEST(BNet, EvaluateResponseMessage_Wrong_Id)
{
  bool ok;
  uint32_t id;

  std::string message3 = "10A1";
  message3 += 0x1e;
  message3 += "OK: <director-name> Version: <version>";

  std::string human_readable_message;
  ok = EvaluateResponseMessage(message3, id, human_readable_message);

  EXPECT_EQ(id, kMessageIdProtokollError);
  EXPECT_EQ(ok, false);

  const char *m3 {"10A1 OK: <director-name> Version: <version>"};
  EXPECT_STREQ(message3.c_str(), m3);
}

TEST(BNet, EvaluateResponseMessage_Correct_Id)
{
  bool ok;
  uint32_t id;

  std::string message4 = "1001";
  message4 += 0x1e;
  message4 += "OK: <director-name> Version: <version>";

  std::string human_readable_message;
  ok = EvaluateResponseMessage(message4, id, human_readable_message);

  EXPECT_EQ(id, kMessageIdPamRequired);
  EXPECT_EQ(ok, true);

  const char *m3 {"1001 OK: <director-name> Version: <version>"};
  EXPECT_STREQ(message4.c_str(), m3);
}
