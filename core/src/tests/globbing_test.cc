/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2021-2021 Bareos GmbH & Co. KG

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

namespace directordaemon {
bool DoReloadConfig() { return false; }
}  // namespace directordaemon

void InitContexts(UaContext* ua, TreeContext* tree)
{
  ua->cmd = GetPoolMemory(PM_FNAME);
  ua->args = GetPoolMemory(PM_FNAME);
  ua->errmsg = GetPoolMemory(PM_FNAME);
  ua->verbose = true;
  ua->automount = true;
  ua->send = new OutputFormatter(sprintit, ua, filterit, ua);

  tree->root = new_tree(1);
  tree->ua = ua;
  tree->all = false;
  tree->FileEstimate = 100;
  tree->DeltaCount = 1;
  tree->node = (TREE_NODE*)tree->root;
}

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
  return MarkElements(ua, tree);
}

int FakelsCmd(UaContext* ua, TreeContext* tree, std::string path)
{
  std::string command = "ls " + path;
  PmStrcpy(ua->cmd, command.c_str());

  ParseArgsOnly(ua->cmd, ua->args, &ua->argc, ua->argk, ua->argv, MAX_CMD_ARGS);
  return lscmd(ua, tree);
}

void PopulateTree(std::vector<std::string> files, TreeContext* tree)
{
  int pnl, fnl;
  char* filename = GetPoolMemory(PM_FNAME);
  char* path = GetPoolMemory(PM_FNAME);

  for (auto file : files) {
    SplitPathAndFilename(file.c_str(), path, &pnl, filename, &fnl);

    char* row0 = path;
    char* row1 = filename;
    char row2[] = "1";
    char row3[] = "2";
    char row4[] = "ff";
    char row5[] = "0";
    char row6[] = "0";
    char row7[] = "0";
    char* row[] = {row0, row1, row2, row3, row4, row5, row6, row7};

    InsertTreeHandler(tree, 0, row);
  }
}


TEST(globbing, globbing_in_markcmd)
{
  /*The director resource is created in order for certain functions to access
    certain global variables*/
  me = new DirectorResource;
  me->optimize_for_size = true;
  me->optimize_for_speed = false;

  /* creating a ua context is usually done with
   * new_ua_context(JobControlRecord jcr) but database handling make it break in
   * this environment, while not being necessary for the test*/
  UaContext ua;
  TreeContext tree;
  InitContexts(&ua, &tree);

  const std::vector<std::string> files
      = {"/some/weirdfiles/normalefile",
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
         "/testingwildcards/lonesubdirectory/whatever"};

  PopulateTree(files, &tree);

  // testing full paths
  FakeCdCmd(&ua, &tree, "/");
  EXPECT_EQ(FakeMarkCmd(&ua, &tree, "*"), files.size());

  EXPECT_EQ(
      FakeMarkCmd(&ua, &tree, "/testingwildcards/lonesubdirectory/whatever"),
      1);

  EXPECT_EQ(
      FakeMarkCmd(&ua, &tree, "testingwildcards/lonesubdirectory/whatever"), 1);

  // Using full path while being in a different folder than root
  FakeCdCmd(&ua, &tree, "/some/weirdfiles/");
  EXPECT_EQ(
      FakeMarkCmd(&ua, &tree, "/testingwildcards/lonesubdirectory/whatever"),
      1);
  EXPECT_EQ(FakeMarkCmd(&ua, &tree, "/testingwildcards/sub*"), 6);

  EXPECT_EQ(
      FakeMarkCmd(&ua, &tree, "testingwildcards/lonesubdirectory/whatever"), 0);

  // Testing patterns in different folders
  FakeCdCmd(&ua, &tree, "/some/weirdfiles/");
  EXPECT_EQ(FakeMarkCmd(&ua, &tree, "nope"), 0);
  EXPECT_EQ(FakeMarkCmd(&ua, &tree, "potato"), 1);
  EXPECT_EQ(FakeMarkCmd(&ua, &tree, "potato*"), 2);
  EXPECT_EQ(FakeMarkCmd(&ua, &tree, "lonesubdirectory/*"), 1);
  EXPECT_EQ(FakeMarkCmd(&ua, &tree, "subdirectory2/*"), 2);
  EXPECT_EQ(FakeMarkCmd(&ua, &tree, "sub*/*"), 6);

  FakeCdCmd(&ua, &tree, "/some/");
  EXPECT_EQ(FakeMarkCmd(&ua, &tree, "w*/sub*/*"), 12);
  EXPECT_EQ(FakeMarkCmd(&ua, &tree, "wei*/sub*/*"), 6);
  EXPECT_EQ(FakeMarkCmd(&ua, &tree, "wei*/subroza/*"), 0);
  EXPECT_EQ(FakeMarkCmd(&ua, &tree, "w*efiles/sub*/*"), 6);

  FakeCdCmd(&ua, &tree, "/testingwildcards/");
  EXPECT_EQ(FakeMarkCmd(&ua, &tree, "p?tato"), 1);
  EXPECT_EQ(FakeMarkCmd(&ua, &tree, "subdirectory?/file1"), 1);
  EXPECT_EQ(FakeMarkCmd(&ua, &tree, "subdirectory?/file?"), 6);
  EXPECT_EQ(FakeMarkCmd(&ua, &tree, "subdirectory?/file[!2,!3]"), 4);
  EXPECT_EQ(FakeMarkCmd(&ua, &tree, "su[a,b,c]directory?/file1"), 1);

  /* The following two tests are commented out because they should be working
   * correctly, but as the second test suggests, fnmatch (which is the main
   * wildcard matching function used in bareos) has problems handling the curly
   * braces wildcard.
   */
  //  EXPECT_EQ(FakeMarkCmd(&ua, tree, "{*tory1,*tory2}/file1"), 1);
  //  EXPECT_EQ(fnmatch("{*tory1,*tory2}", "subdirectory1", 0), 0);

  delete me;
}
