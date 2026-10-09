/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2026 Bareos GmbH & Co. KG

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

#include "dird/check_catalog.h"

#include "gtest/gtest.h"

#include <limits>

namespace directordaemon {
namespace {

TEST(CatalogSequenceWarningThreshold, HandlesIntegerSequenceValues)
{
  EXPECT_FALSE(CatalogSequenceAtWarningThreshold(1'503'238'552, 2'147'483'647));
  EXPECT_TRUE(CatalogSequenceAtWarningThreshold(1'503'238'553, 2'147'483'647));
}

TEST(CatalogSequenceWarningThreshold, HandlesBigintSequenceValues)
{
  constexpr auto maximum
      = static_cast<uint64_t>(std::numeric_limits<int64_t>::max());
  constexpr auto threshold = maximum / 10 * 7 + (maximum % 10 * 7 + 9) / 10;

  EXPECT_FALSE(CatalogSequenceAtWarningThreshold(threshold - 1, maximum));
  EXPECT_TRUE(CatalogSequenceAtWarningThreshold(threshold, maximum));
}

TEST(CatalogSequenceWarningThreshold, RejectsZeroMaximum)
{
  EXPECT_FALSE(CatalogSequenceAtWarningThreshold(0, 0));
}

}  // namespace
}  // namespace directordaemon
