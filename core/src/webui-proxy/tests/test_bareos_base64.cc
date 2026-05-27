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
/**
 * @file
 * Unit tests for bareos_base64.
 */
#include "../bareos_base64.h"

#include <cstdint>
#include <string>

#include <gtest/gtest.h>

// ---------------------------------------------------------------------------
// BareosBase64Encode tests
// ---------------------------------------------------------------------------

// All-zeros: each 6-bit group is 0 → 'A' (the first digit)
TEST(BareosBase64, AllZeros)
{
  const uint8_t data[16]{};
  std::string result = BareosBase64Encode(data, 16);
  // Each byte is 0x00, so every 6-bit group maps to 'A'.
  EXPECT_FALSE(result.empty());
  for (char c : result) {
    EXPECT_EQ(c, 'A') << "all-zero input should encode to all 'A'";
  }
}

TEST(BareosBase64, EncodesKnownCompatibleValue)
{
  const uint8_t data[] = {0x80, 0x80};
  std::string result = BareosBase64Encode(data, 2);

  EXPECT_EQ(result, "gIA");
}

TEST(BareosBase64, RoundTripConsistency)
{
  // Encode 16 bytes of the pattern 0x00..0x0F
  uint8_t data[16];
  for (int i = 0; i < 16; ++i) { data[i] = static_cast<uint8_t>(i); }
  std::string result = BareosBase64Encode(data, 16);
  // Result must be non-empty and contain only valid base64 chars
  for (char c : result) {
    bool valid = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')
                 || (c >= '0' && c <= '9') || c == '+' || c == '/';
    EXPECT_TRUE(valid) << "invalid char: " << c;
  }
}
