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
#include "dird/subscription_accounting_table.h"
#include "gtest/gtest.h"

using namespace directordaemon;

TEST(SubscriptionAccountingTable, RuleCodes)
{
  EXPECT_EQ(SubscriptionAccountingRuleCode("st_blocks*512"), "A");
  EXPECT_EQ(SubscriptionAccountingRuleCode("st_size"), "L");
  EXPECT_EQ(SubscriptionAccountingRuleCode("job_bytes"), "J");
  EXPECT_EQ(SubscriptionAccountingRuleCode("future_rule"), "future_rule");
}

TEST(SubscriptionAccountingTable, AlignsHeadersRowsAndTotals)
{
  const auto text = FormatSubscriptionAccountingTable(
      {"FileSet@Client", "Files", "Accounted size", "Logical size", "Rule",
       "Chain jobs", "Status / Reason"},
      {{"Catalog@bareos-fd", "93", "2.438 GB", "2.438 GB", "A", "1", ""},
       {"LongFileSet@long-client", "0", "0 B", "0 B", "L", "23", ""}},
      {"TOTAL", "93", "2.438 GB", "2.438 GB", "", "", ""}, false, true);
  EXPECT_EQ(
      text,
      "FileSet@Client           Files  Accounted size  Logical size  Rule "
      " Chain jobs\n"
      "-----------------------  -----  --------------  ------------  "
      "----  ----------\n"
      "Catalog@bareos-fd           93        2.438 GB      2.438 GB  A  "
      "            1\n"
      "LongFileSet@long-client      0             0 B           0 B  L  "
      "           23\n"
      "-----------------------  -----  --------------  ------------  "
      "----  ----------\n"
      "TOTAL                       93        2.438 GB      2.438 GB\n");
}

TEST(SubscriptionAccountingTable, ExclusionsRemainUnavailable)
{
  const auto text = FormatSubscriptionAccountingTable(
      {"ID", "Files", "Size", "Logical", "Rule", "Jobs", "Status"},
      {{"fs@fd", "-", "-", "-", "-", "-", "Excluded: no usable backup chain"}},
      {"TOTAL", "0", "0 B", "0 B", "", "", ""}, true, true);
  EXPECT_NE(text.find("Excluded: no usable backup chain"), std::string::npos);
  EXPECT_NE(text.find("fs@fd      -     -        -  -        -  Excluded:"),
            std::string::npos);
  EXPECT_NE(text.find("Status"), std::string::npos);
}

TEST(SubscriptionAccountingTable, HidesLogicalSizeWithoutChangingAlignment)
{
  for (bool show_status : {false, true}) {
    const auto text = FormatSubscriptionAccountingTable(
        {"ID", "Files", "Size", "Logical", "Rule", "Jobs", "Status"},
        {{"fs@fd", "3", "2 B", "9 B", "A", "12", "Issue"}},
        {"TOTAL", "3", "2 B", "9 B", "", "", ""}, show_status, false);
    EXPECT_EQ(text.find("Logical"), std::string::npos);
    EXPECT_EQ(text.find("9 B"), std::string::npos);
    EXPECT_NE(text.find("fs@fd      3   2 B  A       12"), std::string::npos);
    EXPECT_NE(text.find("TOTAL      3   2 B"), std::string::npos);
    EXPECT_EQ(text.find("Issue") != std::string::npos, show_status);
  }
}
