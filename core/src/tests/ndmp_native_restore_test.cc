/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2026-2026 Bareos GmbH & Co. KG

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

#include "gtest/gtest.h"
#include "include/bareos.h"
#include "lib/tree.h"
#include "dird/ndmp_dma_restore_common.h"

#include <string>
#include <vector>

using directordaemon::NdmpNativeJobsToRestore;
using directordaemon::NdmpNativeRecoveredFiles;

namespace {
constexpr bool Recovered(int count,
                         int ok,
                         bool session_ok,
                         std::uint32_t restored,
                         std::uint32_t failed)
{
  auto files = NdmpNativeRecoveredFiles(count, ok, session_ok);
  return files.restored == restored && files.failed == failed;
}
}  // namespace

// without NDMP_LOG_FILE messages an image counts as one file
static_assert(Recovered(0, 0, true, 1, 0));
static_assert(Recovered(0, 0, false, 0, 0));
// otherwise every name list entry is counted
static_assert(Recovered(7, 7, true, 7, 0));
static_assert(Recovered(7, 5, false, 5, 2));
static_assert(Recovered(7, 0, false, 0, 7));
static_assert(Recovered(3, 3, false, 3, 0));

class NdmpNativeRestore : public testing::Test {
 protected:
  void SetUp() override { root = new_tree(1); }
  void TearDown() override { FreeTree(root); }

  tree_node* AddFile(std::string name, JobId_t JobId, bool extract)
  {
    std::string path{"/data/"};
    tree_node* parent = make_tree_path(path.data(), root);
    tree_node* node = insert_tree_node(path.data(), name.data(),
                                       tree_node_type::File, root, parent);
    node->JobId = JobId;
    node->extract = extract;
    return node;
  }

  TREE_ROOT* root{nullptr};
};

TEST_F(NdmpNativeRestore, no_tree_returns_all_jobs)
{
  EXPECT_EQ(NdmpNativeJobsToRestore("1,2,5", nullptr),
            (std::vector<JobId_t>{1, 2, 5}));
}

TEST_F(NdmpNativeRestore, empty_joblist)
{
  EXPECT_TRUE(NdmpNativeJobsToRestore("", root).empty());
  EXPECT_TRUE(NdmpNativeJobsToRestore(nullptr, root).empty());
}

TEST_F(NdmpNativeRestore, nothing_selected_returns_all_jobs)
{
  AddFile("full", 1, false);
  AddFile("incremental", 2, false);
  EXPECT_EQ(NdmpNativeJobsToRestore("1,2", root), (std::vector<JobId_t>{1, 2}));
}

TEST_F(NdmpNativeRestore, only_incremental_selected)
{
  AddFile("full", 1, false);
  AddFile("incremental", 2, true);
  EXPECT_EQ(NdmpNativeJobsToRestore("1,2", root), (std::vector<JobId_t>{2}));
}

TEST_F(NdmpNativeRestore, only_full_selected)
{
  AddFile("full", 1, true);
  AddFile("incremental", 2, false);
  EXPECT_EQ(NdmpNativeJobsToRestore("1,2", root), (std::vector<JobId_t>{1}));
}

TEST_F(NdmpNativeRestore, all_selected_keeps_job_order)
{
  AddFile("incremental2", 7, true);
  AddFile("full", 3, true);
  AddFile("incremental1", 5, true);
  AddFile("unchanged", 3, true);
  EXPECT_EQ(NdmpNativeJobsToRestore("3,5,7", root),
            (std::vector<JobId_t>{3, 5, 7}));
}

TEST_F(NdmpNativeRestore, job_without_selected_files_is_skipped)
{
  AddFile("full", 3, true);
  AddFile("incremental1", 5, false);
  AddFile("incremental2", 7, true);
  EXPECT_EQ(NdmpNativeJobsToRestore("3,5,7", root),
            (std::vector<JobId_t>{3, 7}));
}
