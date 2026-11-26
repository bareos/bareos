/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2018-2026 Bareos GmbH & Co. KG

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

#include "lib/output_formatter.h"
#include <cstdarg>
#include <string>

TEST(output_formatter, constructor_destructor) {}

static bool CaptureOutput(void* context, const char* format, ...)
{
  va_list args;
  va_start(args, format);
  PoolMem output;
  output.Bvsprintf(format, args);
  va_end(args);
  *static_cast<std::string*>(context) += output.c_str();
  return true;
}

TEST(output_formatter, preformattedTextKeepsPercentSigns)
{
  for (int mode : {API_MODE_OFF, API_MODE_ON}) {
    std::string output;
    OutputFormatter formatter(CaptureOutput, &output, nullptr, nullptr, mode);
    formatter.ObjectStart();
    formatter.ObjectKeyValuePreformatted("comment", "100% %s",
                                        " comment: 100% %s\n");
    formatter.ObjectEnd();
    EXPECT_EQ(output, " comment: 100% %s\n");
  }
}

#if HAVE_JANSSON
TEST(output_formatter, preformattedJsonKeepsRawValue)
{
  std::string output;
  OutputFormatter formatter(CaptureOutput, &output, nullptr, nullptr, API_MODE_JSON);
  formatter.ArrayStart("jobs");
  formatter.ObjectStart();
  formatter.ObjectKeyValuePreformatted("comment", "100% %s", " padded text ");
  formatter.ObjectEnd();
  formatter.ArrayEnd("jobs");
  formatter.FinalizeResult(true);
  EXPECT_NE(output.find("\"comment\": \"100% %s\""), std::string::npos);
  EXPECT_EQ(output.find("padded text"), std::string::npos);
}
#endif
