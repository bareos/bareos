/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2021-2024 Bareos GmbH & Co. KG

   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation, which is
   listed in the file LICENSE.

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
#else
#  include "gtest/gtest.h"
#  include "include/bareos.h"
#endif

#include "dird/ua_tree.cc"
#include "dird/ua_output.h"
#include "dird/ua.h"
#include "dird/ua_restore.cc"

using namespace directordaemon;

int FakeCdCmd(UaContext* ua, TreeContext* tree, std::string path)
{
  std::string command = "cd " + path;
  PmStrcpy(ua->cmd, command.c_str());
  ParseArgsOnly(ua->cmd, ua->args, &ua->argc, ua->argk, ua->argv, MAX_CMD_ARGS);
  return cdcmd(ua, tree);
}

int FakeMarkCmd(UaContext* ua, TreeContext* tree, std::string path)
{
  std::string command = "mark " + path;
  PmStrcpy(ua->cmd, command.c_str());

  ParseArgsOnly(ua->cmd, ua->args, &ua->argc, ua->argk, ua->argv, MAX_CMD_ARGS);
  return MarkElements(ua, tree, true);
}

int FakeUnmarkCmd(UaContext* ua, TreeContext* tree, std::string path)
{
  std::string command = "mark " + path;
  PmStrcpy(ua->cmd, command.c_str());

  ParseArgsOnly(ua->cmd, ua->args, &ua->argc, ua->argk, ua->argv, MAX_CMD_ARGS);
  return MarkElements(ua, tree, false);
}

std::pair<std::string, std::string> SplitFNP(std::string_view file)
{
  auto pos = file.find_last_of("/");

  if (pos == file.npos) {
    // everything is just ap path
    return std::make_pair(std::string(file), "");
  }

  return std::make_pair(std::string(file.substr(0, pos + 1)),
                        std::string(file.substr(pos + 1)));
}

void PopulateTree(std::vector<std::string> files, TreeContext* tree)
{
  for (auto file : files) {
    auto [fpath, fname] = SplitFNP(file);

    char* row0 = fpath.data();
    char* row1 = fname.data();
    char row2[] = "1";
    char row3[] = "2";
    char row4[] = "P0A CF2xg IGk B Po Po A 3Y BAA I BhjA7I BU+HEc BhjA7I A A C";
    char row5[] = "0";
    char row6[] = "0";
    char row7[] = "0";
    char* row[] = {row0, row1, row2, row3, row4, row5, row6, row7};

    InsertTreeHandler(tree, 0, row);
  }
}

class Globbing : public testing::Test {
 protected:
  void SetUp() override
  {
    tree.root = new_tree(1);
    tree.node = tree.root;
    me = new DirectorResource;
    ua = new_ua_context(&jcr);
  }

  void TearDown() override
  {
    FreeUaContext(ua);
    FreeTree(tree.root);
    delete me;
  }

  JobControlRecord jcr{};
  UaContext* ua{nullptr};
  TreeContext tree;
};

TEST_F(Globbing, globbing_in_markcmd)
{
  const std::vector<std::string> files = {
      "/some/weirdfiles/normalefile",
      "/some/weirdfiles/nottooweird",
      "/some/weirdfiles/potato",
      "/some/weirdfiles/potatomashed",
      "/some/weirdfiles/subdirectory1/file1",
      "/some/weirdfiles/subdirectory1/file2",
      "/some/weirdfiles/subdirectory2/file3",
      "/some/weirdfiles/subdirectory2/file4",
      "/some/weirdfiles/subdirectory3/file5",
      "/some/weirdfiles/subdirectory3/file6",
      "/some/weirdfiles/lonesubdirectory/whatever",
      "/some/wastefiles/normalefile",
      "/some/wastefiles/nottooweird",
      "/some/wastefiles/potato",
      "/some/wastefiles/potatomashed",
      "/some/wastefiles/subdirectory1/file1",
      "/some/wastefiles/subdirectory1/file2",
      "/some/wastefiles/subdirectory2/file3",
      "/some/wastefiles/subdirectory2/file4",
      "/some/wastefiles/subdirectory3/file5",
      "/some/wastefiles/subdirectory3/file6",
      "/some/wastefiles/lonesubdirectory/whatever",
      "/testingwildcards/normalefile",
      "/testingwildcards/nottooweird",
      "/testingwildcards/potato",
      "/testingwildcards/potatomashed",
      "/testingwildcards/subdirectory1/file1",
      "/testingwildcards/subdirectory1/file2",
      "/testingwildcards/subdirectory2/file3",
      "/testingwildcards/subdirectory2/file4",
      "/testingwildcards/subdirectory3/file5",
      "/testingwildcards/subdirectory3/file6",
      "/testingwildcards/lonesubdirectory/whatever",
      // how we handle windows files is a bit weird.
      // since "C:" is a WinDir (because it has no leading slash)
      // it _cannot_ also be a NewDir (i.e. a directory thats not backed up)
      // For this to work correctly we have to add this as an actually
      // backed up file!
      "C:/",
      "C:/windir/file1",
      "C:/windir/file2",
      "C:/windir/file3",
      "C:/windir/file4",
  };

  PopulateTree(files, &tree);

  // testing full paths
  FakeCdCmd(ua, &tree, "/");
  EXPECT_EQ(FakeMarkCmd(ua, &tree, "*"), files.size());
  FakeUnmarkCmd(ua, &tree, "*");

  EXPECT_EQ(
      FakeMarkCmd(ua, &tree, "/testingwildcards/lonesubdirectory/whatever"), 1);

  FakeUnmarkCmd(ua, &tree, "*");
  EXPECT_EQ(
      FakeMarkCmd(ua, &tree, "testingwildcards/lonesubdirectory/whatever"), 1);
  FakeUnmarkCmd(ua, &tree, "*");

  EXPECT_EQ(FakeMarkCmd(ua, &tree, "C:/windir/file1"), 1);
  EXPECT_EQ(FakeUnmarkCmd(ua, &tree, "*"), 1);

  // Using full path while being in a different folder than root
  FakeCdCmd(ua, &tree, "/some/weirdfiles/");
  EXPECT_EQ(
      FakeMarkCmd(ua, &tree, "/testingwildcards/lonesubdirectory/whatever"), 1);
  FakeUnmarkCmd(ua, &tree, "*");
  EXPECT_EQ(FakeMarkCmd(ua, &tree, "/testingwildcards/sub*"), 6);
  FakeUnmarkCmd(ua, &tree, "*");

  EXPECT_EQ(
      FakeMarkCmd(ua, &tree, "testingwildcards/lonesubdirectory/whatever"), 0);
  FakeUnmarkCmd(ua, &tree, "*");

  EXPECT_EQ(FakeMarkCmd(ua, &tree, "C:/windir/file1"), 1);
  EXPECT_EQ(FakeUnmarkCmd(ua, &tree, "C:/windir/file1"), 1);

  // Testing patterns in different folders
  FakeCdCmd(ua, &tree, "/some/weirdfiles/");
  EXPECT_EQ(FakeMarkCmd(ua, &tree, "nope"), 0);
  FakeUnmarkCmd(ua, &tree, "*");
  EXPECT_EQ(FakeMarkCmd(ua, &tree, "potato"), 1);
  FakeUnmarkCmd(ua, &tree, "*");
  EXPECT_EQ(FakeMarkCmd(ua, &tree, "potato*"), 2);
  FakeUnmarkCmd(ua, &tree, "*");
  EXPECT_EQ(FakeMarkCmd(ua, &tree, "lonesubdirectory/*"), 1);
  FakeUnmarkCmd(ua, &tree, "*");
  EXPECT_EQ(FakeMarkCmd(ua, &tree, "subdirectory2/*"), 2);
  FakeUnmarkCmd(ua, &tree, "*");
  EXPECT_EQ(FakeMarkCmd(ua, &tree, "sub*/*"), 6);
  FakeUnmarkCmd(ua, &tree, "*");
  EXPECT_EQ(FakeMarkCmd(ua, &tree, "C:/windir/file1"), 1);
  FakeUnmarkCmd(ua, &tree, "*");

  FakeCdCmd(ua, &tree, "/some/");
  EXPECT_EQ(FakeMarkCmd(ua, &tree, "w*/sub*/*"), 12);
  FakeUnmarkCmd(ua, &tree, "*");
  EXPECT_EQ(FakeMarkCmd(ua, &tree, "wei*/sub*/*"), 6);
  FakeUnmarkCmd(ua, &tree, "*");
  EXPECT_EQ(FakeMarkCmd(ua, &tree, "wei*/subroza/*"), 0);
  FakeUnmarkCmd(ua, &tree, "*");
  EXPECT_EQ(FakeMarkCmd(ua, &tree, "w*efiles/sub*/*"), 6);
  FakeUnmarkCmd(ua, &tree, "*");

  FakeCdCmd(ua, &tree, "/testingwildcards/");
  EXPECT_EQ(FakeMarkCmd(ua, &tree, "p?tato"), 1);
  FakeUnmarkCmd(ua, &tree, "*");
  EXPECT_EQ(FakeMarkCmd(ua, &tree, "subdirectory?/file1"), 1);
  FakeUnmarkCmd(ua, &tree, "*");
  EXPECT_EQ(FakeMarkCmd(ua, &tree, "subdirectory?/file?"), 6);
  FakeUnmarkCmd(ua, &tree, "*");
  EXPECT_EQ(FakeMarkCmd(ua, &tree, "subdirectory?/file[!2,!3]"), 4);
  FakeUnmarkCmd(ua, &tree, "*");
  EXPECT_EQ(FakeMarkCmd(ua, &tree, "su[a,b,c]directory?/file1"), 1);
  FakeUnmarkCmd(ua, &tree, "*");

  /* The following two tests are commented out because they should be working
   * correctly, but as the second test suggests, fnmatch (which is the main
   * wildcard matching function used in bareos) has problems handling the curly
   * braces wildcard.
   */
  //  EXPECT_EQ(FakeMarkCmd(&ua, tree, "{*tory1,*tory2}/file1"), 1);
  //  EXPECT_EQ(fnmatch("{*tory1,*tory2}", "subdirectory1", 0), 0);
}

TEST_F(Globbing, double_star_globbing_in_markcmd)
{
  const std::vector<std::string> files = {
      "/some/weirdfiles/normalefile",
      "/some/weirdfiles/nottooweird",
      "/some/weirdfiles/potato",
      "/some/weirdfiles/potatomashed",
      "/some/weirdfiles/subdirectory1/file1",
      "/some/weirdfiles/subdirectory1/file2",
      "/some/weirdfiles/subdirectory2/file3",
      "/some/weirdfiles/subdirectory2/file4",
      "/some/weirdfiles/subdirectory3/file5",
      "/some/weirdfiles/subdirectory3/file6",
      "/some/weirdfiles/lonesubdirectory/whatever",
      "/some/wastefiles/normalefile",
      "/some/wastefiles/nottooweird",
      "/some/wastefiles/potato",
      "/some/wastefiles/potatomashed",
      "/some/wastefiles/subdirectory1/file1",
      "/some/wastefiles/subdirectory1/file2",
      "/some/wastefiles/subdirectory2/file3",
      "/some/wastefiles/subdirectory2/file4",
      "/some/wastefiles/subdirectory3/file5",
      "/some/wastefiles/subdirectory3/file6",
      "/some/wastefiles/lonesubdirectory/whatever",
      "/testingwildcards/normalefile",
      "/testingwildcards/nottooweird",
      "/testingwildcards/potato",
      "/testingwildcards/potatomashed",
      "/testingwildcards/subdirectory1/file1",
      "/testingwildcards/subdirectory1/file2",
      "/testingwildcards/subdirectory2/file3",
      "/testingwildcards/subdirectory2/file4",
      "/testingwildcards/subdirectory3/file5",
      "/testingwildcards/subdirectory3/file6",
      "/testingwildcards/lonesubdirectory/whatever",
      // how we handle windows files is a bit weird.
      // since "C:" is a WinDir (because it has no leading slash)
      // it _cannot_ also be a NewDir (i.e. a directory thats not backed up)
      // For this to work correctly we have to add this as an actually
      // backed up file!
      "C:/",
      "C:/windir/file1",
      "C:/windir/file2",
      "C:/windir/file3",
      "C:/windir/file4",
  };

  PopulateTree(files, &tree);

  // testing full paths
  FakeCdCmd(ua, &tree, "/");
  EXPECT_EQ(FakeMarkCmd(ua, &tree, "**/file1"), 4);
  FakeUnmarkCmd(ua, &tree, "*");

  // Using while being in a different folder than root
  FakeCdCmd(ua, &tree, "/some/weirdfiles/");
  EXPECT_EQ(FakeMarkCmd(ua, &tree, "**/file1"), 1);
  FakeUnmarkCmd(ua, &tree, "*");
  EXPECT_EQ(FakeMarkCmd(ua, &tree, "**/f*1"), 1);
  FakeUnmarkCmd(ua, &tree, "*");

  // Using ** not at the start
  FakeCdCmd(ua, &tree, "/some/");
  EXPECT_EQ(FakeMarkCmd(ua, &tree, "weirdfiles/**/file1"), 1);
  FakeUnmarkCmd(ua, &tree, "*");
  EXPECT_EQ(FakeMarkCmd(ua, &tree, "weirdfiles/**/f*1"), 1);
  FakeUnmarkCmd(ua, &tree, "*");
  EXPECT_EQ(FakeMarkCmd(ua, &tree, "w*files/**/file1"), 2);
  FakeUnmarkCmd(ua, &tree, "*");
  EXPECT_EQ(FakeMarkCmd(ua, &tree, "w**files/**/file1"), 2);
  FakeUnmarkCmd(ua, &tree, "*");

  // testing empty match
  FakeCdCmd(ua, &tree, "/testingwildcards/");
  EXPECT_EQ(FakeMarkCmd(ua, &tree, "**/p?tato"), 1);
  FakeUnmarkCmd(ua, &tree, "*");
}
