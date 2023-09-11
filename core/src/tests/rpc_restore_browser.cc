/*
   BAREOS® - Backup Archiving REcovery Open Sourced

Copyright (C) 2023-2023 Bareos GmbH & Co. KG

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
#include "dird/jsonrpcrestore.h"

using namespace directordaemon;

void PopulateTree(std::vector<std::string> files, TreeContext* tree)
{
  PathAndFileName path_and_filename;
  int filecount = 0;

  for (const auto& file : files) {
    filecount++;
    std::string fileindex = std::to_string(filecount);
    path_and_filename = SplitPathAndFilename(file.c_str());

    char* row0 = path_and_filename.path.data();
    char* row1 = path_and_filename.filename.data();
    char* row2 = fileindex.data();
    char row3[] = "2";
    char row4[] = "P0A CF2xg IGk B Po Po A 3Y BAA I BhjA7I BU+HEc BhjA7I A A C";
    char row5[] = "0";
    char row6[] = "0";
    char row7[] = "0";
    char* row[] = {row0, row1, row2, row3, row4, row5, row6, row7};

    InsertTreeHandler(tree, 0, row);
  }
}

class RpcRestoreBrowser : public testing::Test {
 protected:
  void SetUp() override
  {
    me = new DirectorResource;
    me->optimize_for_size = true;
    me->optimize_for_speed = false;
    tree.root = new_tree(1);
    tree.node = (TREE_NODE*)tree.root;

    ua = new_ua_context(&jcr);

    PopulateTree(files, &tree);
  }

  void TearDown() override
  {
    FreeUaContext(ua);
    FreeTree(tree.root);
    delete me;
  }

  JobControlRecord jcr{};
  UaContext* ua;
  TreeContext tree;

  const std::vector<std::string> files
      = {"/some/weirdfiles/lonesubdirectory/whatever",
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
         "/testingwildcards/lonesubdirectory/whatever"};
};

TEST_F(RpcRestoreBrowser, MarkAndUnmark)
{
  Json::Value filepattern;
  filepattern.append("/some/weirdfiles/normalefile");
  filepattern.append("/some/weirdfiles/nottooweird");
  EXPECT_EQ(MarkCmd(ua, tree, filepattern)["marked files"].asInt(), 2);
  EXPECT_EQ(UnmarkCmd(ua, tree, filepattern)["unmarked files"].asInt(), 2);

  filepattern.clear();
  filepattern.append("*");
  EXPECT_EQ(MarkCmd(ua, tree, filepattern)["marked files"].asInt(),
            files.size());
}

TEST_F(RpcRestoreBrowser, SimpleListing)
{
  int offset = 0;
  int limit = files.size() + 1;

  EXPECT_EQ(LsCmd(limit, offset, "", &tree)["file list"].size(), 2);
  EXPECT_EQ(LsCmd(limit, offset, "some/weirdfiles/", &tree)["file list"].size(),
            8);
  EXPECT_EQ(LsCount("some/weirdfiles/", &tree)["file count"].asInt(), 8);

  // cd and ls a directory
  EXPECT_EQ(CdCmd("/some/weirdfiles", &tree)["current directory"].asString(),
            "/some/weirdfiles/");
  EXPECT_EQ(LsCmd(limit, offset, "", &tree)["file list"].size(), 8);
}

TEST_F(RpcRestoreBrowser, ListingWithOffsetAndLimit)
{
  int offset = 2;
  int limit = files.size() + 1;
  Json::Value lsresult = LsCmd(limit, offset, "some/weirdfiles/", &tree);
  EXPECT_EQ(lsresult["file list"].size(), 6);
  EXPECT_EQ(lsresult["file list"][0]["name"], "nottooweird");
  EXPECT_EQ(lsresult["file list"][lsresult["file list"].size() - 1]["name"],
            "subdirectory3/");

  offset = 0;
  limit = 5;
  lsresult = LsCmd(limit, offset, "some/weirdfiles/", &tree);
  EXPECT_EQ(lsresult["file list"].size(), 5);
  EXPECT_EQ(lsresult["file list"][0]["name"], "lonesubdirectory/");
  EXPECT_EQ(lsresult["file list"][lsresult["file list"].size() - 1]["name"],
            "potatomashed");
}
