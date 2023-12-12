/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2021-2023 Bareos GmbH & Co. KG

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
#  include "benchmark/benchmark.h"
#else
#  include "include/bareos.h"
#  include "benchmark/benchmark.h"
#endif

#include "dird/ua_tree.cc"
#include "dird/ua_output.h"
#include "dird/ua.h"
#include "dird/ua_restore.cc"

using namespace directordaemon;

UaContext ua;
TreeContext tree;

const int max_depth = 30;

enum HIGH_FILE_NUMBERS
{
  hundred_thousand = 100'000,
  million = 1'000'000,
  ten_million = 10'000'000,
  hundred_million = 100'000'000,
  billion = 1'000'000'000
};

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

void PopulateTree(int quantity, TreeContext* tree)
{
  me = new DirectorResource;
  InitContexts(&ua, tree);

  char* filename = GetPoolMemory(PM_FNAME);
  char* path = GetPoolMemory(PM_FNAME);


  std::string file_path = "/";
  std::string file{};

  for (int i = 0; i < max_depth; ++i) {
    file_path.append("dir" + std::to_string(i) + "/");
    for (int j = 0; j < (quantity / max_depth); ++j) {
      file = "file" + std::to_string(j);

      strcpy(path, file_path.c_str());
      strcpy(filename, file.c_str());

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
}

static void BM_populatetree(benchmark::State& state)
{
  for (auto _ : state) { PopulateTree(state.range(0), &tree); }
}

static void BM_markallfiles(benchmark::State& state)
{
  FakeCdCmd(&ua, &tree, "/");
  for (auto _ : state) { FakeMarkCmd(&ua, &tree, "*"); }
}

BENCHMARK(BM_populatetree)
    ->Arg(HIGH_FILE_NUMBERS::hundred_thousand)
    ->Unit(benchmark::kSecond);
BENCHMARK(BM_markallfiles)
    ->Arg(HIGH_FILE_NUMBERS::hundred_thousand)
    ->Unit(benchmark::kSecond);


BENCHMARK(BM_populatetree)
    ->Arg(HIGH_FILE_NUMBERS::million)
    ->Unit(benchmark::kSecond);
BENCHMARK(BM_markallfiles)
    ->Arg(HIGH_FILE_NUMBERS::million)
    ->Unit(benchmark::kSecond);


BENCHMARK(BM_populatetree)
    ->Arg(HIGH_FILE_NUMBERS::ten_million)
    ->Unit(benchmark::kSecond);
BENCHMARK(BM_markallfiles)
    ->Arg(HIGH_FILE_NUMBERS::ten_million)
    ->Unit(benchmark::kSecond);

/*
 * Over ten million files requires quiet a bit a ram, so if you are going to
 * use the higher numbers, make sure you have enough ressources, otherwise the
 * benchmark will crash
 */

BENCHMARK_MAIN();
