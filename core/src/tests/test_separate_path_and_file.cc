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
#include "include/bareos.h"
#include "include/filetypes.h"
#include "include/jcr.h"

#include "findlib/find.h"
#include "lib/attr.h"
#include "findlib/create_file.h"

#include <cstring>

#if defined(HAVE_WIN32)
// Regression test for restoring to a UNC path (e.g. \\server\share\...).
// SeparatePathAndFile() used to collapse the leading double separator of a
// UNC path down to a single separator, which made the restored file end up
// under a bogus drive-relative path (e.g. C:\server\share\...) instead of
// the intended network share.
TEST(SeparatePathAndFile, preserves_unc_prefix)
{
  JobControlRecord jcr{};

  char ofile[]
      = "\\\\localhost\\C$\\Users\\Administrator\\restore-target\\file.txt";
  int pnl = SeparatePathAndFile(&jcr, ofile, ofile);

  ASSERT_GT(pnl, 0);
  EXPECT_STREQ(
      ofile,
      "\\\\localhost\\C$\\Users\\Administrator\\restore-target\\file.txt");
  EXPECT_STREQ(ofile + pnl + 1, "file.txt");
  // the path must still start with the double separator of the UNC prefix
  EXPECT_EQ(ofile[0], '\\');
  EXPECT_EQ(ofile[1], '\\');
}

// A run of more than two leading separators (e.g. from over-escaped
// bconsole input) must be collapsed down to exactly two, since 3+ leading
// separators is not a valid UNC prefix and Windows will reject it.
TEST(SeparatePathAndFile, collapses_excess_leading_separators)
{
  JobControlRecord jcr{};

  char ofile[]
      = "\\\\\\localhost\\C$\\Users\\Administrator\\restore-target\\file.txt";
  int pnl = SeparatePathAndFile(&jcr, ofile, ofile);

  ASSERT_GT(pnl, 0);
  EXPECT_STREQ(
      ofile,
      "\\\\localhost\\C$\\Users\\Administrator\\restore-target\\file.txt");
  EXPECT_STREQ(ofile + pnl + 1, "file.txt");
}

// Bareos conventionally also accepts forward slashes as path separators
// (this avoids the need to escape backslashes e.g. in bconsole). Make sure
// the UNC prefix is preserved for that form too: //server/share/...
TEST(SeparatePathAndFile, preserves_forward_slash_unc_prefix)
{
  JobControlRecord jcr{};

  char ofile[] = "//localhost/C$/Users/Administrator/restore-target/file.txt";
  int pnl = SeparatePathAndFile(&jcr, ofile, ofile);

  ASSERT_GT(pnl, 0);
  EXPECT_STREQ(ofile,
               "//localhost/C$/Users/Administrator/restore-target/file.txt");
  EXPECT_STREQ(ofile + pnl + 1, "file.txt");
  EXPECT_EQ(ofile[0], '/');
  EXPECT_EQ(ofile[1], '/');
}

// Mixed separators: a forward-slash UNC prefix followed by backslashes.
TEST(SeparatePathAndFile, preserves_mixed_separator_unc_prefix)
{
  JobControlRecord jcr{};

  char ofile[]
      = "//localhost\\C$\\Users\\Administrator\\restore-target\\file.txt";
  int pnl = SeparatePathAndFile(&jcr, ofile, ofile);

  ASSERT_GT(pnl, 0);
  EXPECT_STREQ(ofile + pnl + 1, "file.txt");
  EXPECT_EQ(ofile[0], '/');
  EXPECT_EQ(ofile[1], '/');
}

TEST(SeparatePathAndFile, regular_path_unaffected)
{
  JobControlRecord jcr{};

  char ofile[] = "C:/Users/Administrator/restore-target/file.txt";
  int pnl = SeparatePathAndFile(&jcr, ofile, ofile);

  ASSERT_GT(pnl, 0);
  EXPECT_STREQ(ofile, "C:/Users/Administrator/restore-target/file.txt");
  EXPECT_STREQ(ofile + pnl + 1, "file.txt");
}
#endif  // HAVE_WIN32
