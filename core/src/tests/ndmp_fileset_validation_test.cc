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

#include "gtest/gtest.h"

#include "dird/ndmp_fileset_validation.h"

#include <string_view>

using directordaemon::NdmpFilesetValidation;
using directordaemon::NdmpFilesetValidationMessage;
using directordaemon::ValidateNdmpFileset;

/* An NDMP backup job saves exactly one filesystem, so exactly one Include
 * block holding exactly one File entry is accepted. */
static_assert(ValidateNdmpFileset(1, 1) == NdmpFilesetValidation::kOk);

// A fileset without an Include block has no filesystem to save.
static_assert(ValidateNdmpFileset(0, 0)
              == NdmpFilesetValidation::kNoIncludeSet);
static_assert(ValidateNdmpFileset(0, 1)
              == NdmpFilesetValidation::kNoIncludeSet);

// Several Include blocks would mean several filesystems in one job.
static_assert(ValidateNdmpFileset(2, 1)
              == NdmpFilesetValidation::kMultipleIncludeSets);

/* The number of Include blocks is reported first, as the File entries are
 * counted in the first block only. */
static_assert(ValidateNdmpFileset(2, 2)
              == NdmpFilesetValidation::kMultipleIncludeSets);

// An empty Include block names no filesystem.
static_assert(ValidateNdmpFileset(1, 0)
              == NdmpFilesetValidation::kNoFileEntry);

// Several File entries would mean several filesystems in one job.
static_assert(ValidateNdmpFileset(1, 2)
              == NdmpFilesetValidation::kMultipleFileEntries);
static_assert(ValidateNdmpFileset(1, 100)
              == NdmpFilesetValidation::kMultipleFileEntries);

/* alist::size() returns a signed value, so guard against a negative count
 * being mistaken for an acceptable fileset. */
static_assert(ValidateNdmpFileset(-1, 1)
              == NdmpFilesetValidation::kNoIncludeSet);
static_assert(ValidateNdmpFileset(1, -1)
              == NdmpFilesetValidation::kNoFileEntry);

// Only an accepted fileset has an empty explanation.
static_assert(NdmpFilesetValidationMessage(NdmpFilesetValidation::kOk)
              == std::string_view{});
static_assert(NdmpFilesetValidationMessage(NdmpFilesetValidation::kNoIncludeSet)
              != std::string_view{});
static_assert(
    NdmpFilesetValidationMessage(NdmpFilesetValidation::kMultipleIncludeSets)
    != std::string_view{});
static_assert(NdmpFilesetValidationMessage(NdmpFilesetValidation::kNoFileEntry)
              != std::string_view{});
static_assert(
    NdmpFilesetValidationMessage(NdmpFilesetValidation::kMultipleFileEntries)
    != std::string_view{});

TEST(ndmp_fileset_validation, accepts_exactly_one_filesystem)
{
  EXPECT_EQ(ValidateNdmpFileset(1, 1), NdmpFilesetValidation::kOk);
}

TEST(ndmp_fileset_validation, rejects_several_filesystems)
{
  EXPECT_EQ(ValidateNdmpFileset(2, 1),
            NdmpFilesetValidation::kMultipleIncludeSets);
  EXPECT_EQ(ValidateNdmpFileset(1, 2),
            NdmpFilesetValidation::kMultipleFileEntries);
}

TEST(ndmp_fileset_validation, rejects_fileset_without_filesystem)
{
  EXPECT_EQ(ValidateNdmpFileset(0, 0), NdmpFilesetValidation::kNoIncludeSet);
  EXPECT_EQ(ValidateNdmpFileset(1, 0), NdmpFilesetValidation::kNoFileEntry);
}

TEST(ndmp_fileset_validation, explains_every_rejection)
{
  for (auto result : {NdmpFilesetValidation::kNoIncludeSet,
                      NdmpFilesetValidation::kMultipleIncludeSets,
                      NdmpFilesetValidation::kNoFileEntry,
                      NdmpFilesetValidation::kMultipleFileEntries}) {
    EXPECT_FALSE(std::string_view{NdmpFilesetValidationMessage(result)}.empty());
  }

  EXPECT_TRUE(
      std::string_view{NdmpFilesetValidationMessage(NdmpFilesetValidation::kOk)}
          .empty());
}
