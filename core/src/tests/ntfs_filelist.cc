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
#include "include/bareos.h"
#include "include/filetypes.h"
#include "include/jcr.h"
#include "findlib/find.h"
#include "findlib/find_one.h"
#include "win32/filed/ntfs_filelist.h"
#include "gtest/gtest.h"

#include <filesystem>
#include <fstream>
#include <limits>
#include <sstream>

#ifdef HAVE_WIN32
#  include "filed/filed_conf.h"
#  include "filed/filed_globals.h"
#  include "filed/filed_jcr_impl.h"
#  include "filed/fileset.h"
#  include "filed/dir_cmd.h"
#  include "filed/accurate.h"
#  include "findlib/enable_priv.h"
#  include "win32/include/vss_client.h"
#  include <vss.h>
#  include <vswriter.h>
#  include <vsbackup.h>
#endif

namespace {
using namespace filedaemon;

static_assert(UsableNtfsCursor({7, 10}, {7, 20}, 10));
static_assert(!UsableNtfsCursor({7, 9}, {7, 20}, 10));
static_assert(!UsableNtfsCursor({6, 10}, {7, 20}, 10));
static_assert(!UsableNtfsCursor({7, 21}, {7, 20}, 10));
static_assert(FOPTS_BYTES >= (FO_NTFS_CHANGE_JOURNAL / 8) + 1);

TEST(NtfsFileList, ResolvesReferencesAndMovedDirectoryDescendants)
{
  NtfsReferenceMap map;
  map.root = 5;
  map.records.emplace(20, NtfsRecord{5, "renamed", true});
  map.records.emplace(21, NtfsRecord{20, "child", false});
  EXPECT_EQ(map.Path(21), "/renamed/child");
  EXPECT_FALSE(map.Changed(21));
  map.changed.insert(20);
  EXPECT_TRUE(map.Changed(21));
  map.records.emplace(22, NtfsRecord{5, "unrelated", false});
  EXPECT_FALSE(map.Changed(22));
}

TEST(NtfsFileList, RejectsMissingParentsCyclesAndInvalidNames)
{
  NtfsReferenceMap map;
  map.root = 5;
  map.records.emplace(20, NtfsRecord{21, "a", true});
  EXPECT_FALSE(map.Path(20));
  EXPECT_TRUE(map.Changed(20));
  map.records.emplace(21, NtfsRecord{20, "b", true});
  EXPECT_FALSE(map.Path(20));
  EXPECT_TRUE(map.Changed(20));
  for (const std::string name : {"", ".", "..", "a/b", "a\\b"}) {
    map.records[22] = {5, name, false};
    EXPECT_FALSE(map.Path(22));
  }
}

TEST(NtfsFileList, DoesNotReuseMftSequenceNumbers)
{
  NtfsReferenceMap map;
  map.root = 5;
  map.records.emplace(0x2000000000014ULL, NtfsRecord{5, "new", true});
  map.records.emplace(21, NtfsRecord{0x1000000000014ULL, "child", false});
  EXPECT_FALSE(map.Path(21));
}

TEST(NtfsFileList, CoalescedOpenWritesAndUnknownHistoryAreNotSkipped)
{
  NtfsReferenceMap map;
  map.root = 5;
  map.records.emplace(20, NtfsRecord{5, "open", false});
  map.records.emplace(21, NtfsRecord{5, "closed", false});
  map.records.emplace(22, NtfsRecord{5, "unknown-history", false});
  map.records.emplace(23, NtfsRecord{5, "newly-reopened", false});
  map.Observe(20, 1, false, 100);
  map.Observe(21, 2, true, 100);
  map.Observe(23, 3, true, 100);
  map.Observe(23, 4, false, 100);
  map.IncludeUnclosedFiles();
  EXPECT_TRUE(map.Changed(20));
  EXPECT_FALSE(map.Changed(21));
  EXPECT_TRUE(map.Changed(22));
  EXPECT_TRUE(map.Changed(23));
  map.Observe(21, 100, true, 100);
  EXPECT_TRUE(map.Changed(21));
}

TEST(NtfsFileList, CheckpointRoundTripAndJobIdentity)
{
  std::stringstream text;
  WriteNtfsCheckpoint(text, "job|volume|root with spaces", {17, 12345});
  auto cursor = ReadNtfsCheckpoint(text, "job|volume|root with spaces");
  ASSERT_TRUE(cursor);
  EXPECT_EQ(cursor->journal, 17);
  EXPECT_EQ(cursor->next, 12345);
  std::stringstream wrong;
  WriteNtfsCheckpoint(wrong, "other-job|volume|root", {17, 12345});
  EXPECT_FALSE(ReadNtfsCheckpoint(wrong, "job|volume|root"));
  for (const std::string malformed :
       {"", "\"scope\" 1", "\"scope\" 1 -2", "\"scope\" 1 2 garbage",
        "\"scope\" invalid 2"}) {
    std::stringstream input(malformed);
    EXPECT_FALSE(ReadNtfsCheckpoint(input, "scope"));
  }
}

class UnchangedFileList : public FileList {
 public:
  bool Prepare(JobControlRecord*, FindFilesPacket*, const char*) override
  { return true; }
  const std::vector<std::string>* Children(const char*) const override
  { return nullptr; }
  bool Unchanged(JobControlRecord*, FindFilesPacket*, const char*) override
  { return true; }
};

TEST(NtfsFileList, UnchangedFilesDoNotStatOrCallTimestampComparison)
{
  UnchangedFileList list;
  FindFilesPacket ff;
  ff.file_list = &list;
  ff.CheckFct = [](JobControlRecord*, FindFilesPacket*) {
    ADD_FAILURE() << "Unchanged file reached comparison";
    return false;
  };
  char nonexistent[] = "nonexistent-ntfs-test-file";
  EXPECT_EQ(FindOneFile(
                nullptr, &ff,
                [](JobControlRecord*, FindFilesPacket*, bool) {
                  ADD_FAILURE() << "Unchanged file reached stat callback";
                  return 0;
                },
                nonexistent, 0, false),
            1);
}

TEST(NtfsFileList, JournalChangesOverridePreservedTimestamps)
{
  FindFilesPacket ff;
  ff.incremental = true;
  ff.save_time = 100;
  ff.statp.st_mtime = 1;
  ff.statp.st_ctime = 1;
  EXPECT_FALSE(CheckChanges(nullptr, &ff));
  ff.journal_changed = true;
  EXPECT_TRUE(CheckChanges(nullptr, &ff));
  ff.CheckFct = [](JobControlRecord*, FindFilesPacket*) { return false; };
  EXPECT_TRUE(CheckChanges(nullptr, &ff));
}

#ifdef HAVE_WIN32
class TestAccurateList : public BareosAccurateFilelistHtable {
 public:
  using BareosAccurateFilelistHtable::BareosAccurateFilelistHtable;
  bool Seen(std::string path)
  {
    auto* payload = lookup_payload(path.data());
    return payload && seen_bitmap_[payload->filenr];
  }
};

class TestVssClient : public VSSClientVista {
 public:
  TestVssClient(const std::wstring& mount, const std::wstring& shadow)
  {
    bBackupIsInitialized_ = true;
    mount_to_vol_w.emplace(mount, L"test-volume");
    vol_to_vss_w.emplace(L"test-volume", shadow);
  }
};

TEST(NtfsFileList, SnapshotNamespaceOrExplicitFallback)
{
  const char* volume = std::getenv("BAREOS_TEST_NTFS_VOLUME");
  const char* shadow = std::getenv("BAREOS_TEST_NTFS_SNAPSHOT");
  if (!volume || !shadow) {
    GTEST_SKIP() << "Requires a dedicated snapshot with snapshot-filelist/"
                    "before-snapshot, and a live-only after-snapshot file";
  }
  auto root = std::filesystem::path(volume) / "snapshot-filelist";
  wchar_t mount[32768]{};
  ASSERT_TRUE(GetVolumePathNameW(root.c_str(), mount, 32768));
  TestVssClient vss(mount, std::filesystem::path(shadow).wstring());
  ClientResource client;
  std::string working = std::filesystem::temp_directory_path().generic_string();
  client.working_directory = working.data();
  auto* old_me = me;
  me = &client;
  JobControlRecord jcr;
  FiledJcrImpl impl;
  jcr.fd_impl = &impl;
  impl.pVSSClient = &vss;
  bstrncpy(jcr.Job, "ntfs-snapshot", sizeof(jcr.Job));
  findFILESET fileset;
  findIncludeExcludeItem include{};
  include.opts_list.init(1, not_owned_by_alist);
  findFOPTS options;
  include.opts_list.append(&options);
  fileset.incexe = &include;
  FindFilesPacket ff;
  ff.fileset = &fileset;
  NtfsFileList list;
  auto name = root.generic_string();
  if (list.Prepare(&jcr, &ff, name.c_str())) {
    const auto* names = list.Children(name.c_str());
    EXPECT_NE(names, nullptr);
    if (names) {
      EXPECT_NE(std::find(names->begin(), names->end(), "before-snapshot"),
                names->end());
      EXPECT_EQ(std::find(names->begin(), names->end(), "after-snapshot"),
                names->end());
    }
  } else {
    EXPECT_FALSE(list.fallback_reason.empty());
    EXPECT_EQ(list.Children(name.c_str()), nullptr);
    EXPECT_FALSE(list.Unchanged(&jcr, &ff, name.c_str()));
    std::cout << "Snapshot fallback: " << list.fallback_reason << '\n';
  }
  impl.pVSSClient = nullptr;
  jcr.fd_impl = nullptr;
  me = old_me;
  client.working_directory = nullptr;
}

TEST(NtfsFileList, FileDaemonDecodesJournalOption)
{
  JobControlRecord jcr;
  FiledJcrImpl impl;
  jcr.fd_impl = &impl;
  impl.ff = init_find_files();
  ASSERT_TRUE(InitFileset(&jcr));
  AddFileset(&jcr, "I");
  AddFileset(&jcr, "O j");
  ASSERT_NE(impl.ff->fileset->incexe, nullptr);
  auto* options = impl.ff->fileset->incexe->current_opts;
  ASSERT_NE(options, nullptr);
  EXPECT_TRUE(BitIsSet(FO_NTFS_CHANGE_JOURNAL, options->flags));
  CleanupFileset(&jcr);
  TermFindFiles(impl.ff);
  impl.ff = nullptr;
  jcr.fd_impl = nullptr;
}

TEST(NtfsFileList, RealJournalFullIncrementalAndHardlinks)
{
  const char* volume = std::getenv("BAREOS_TEST_NTFS_VOLUME");
  if (!volume) { GTEST_SKIP() << "Requires a dedicated NTFS test volume"; }
  std::filesystem::path root = std::filesystem::path(volume) / "filelist";
  ASSERT_TRUE(std::filesystem::create_directories(root));
  auto cleanup = [&] { std::filesystem::remove_all(root); };
  std::ofstream(root / "changed") << "original";
  std::ofstream(root / "unchanged") << "original";
  std::ofstream(root / "deleted") << "delete me";
  std::ofstream open(root / "open");
  open << "first write";
  open.flush();
  std::filesystem::create_hard_link(root / "changed", root / "alias");
  std::filesystem::create_directories(root / "subdir");
  std::ofstream(root / "subdir" / "child") << "child";
  std::ofstream(root / "stable-link") << "stable";
  std::filesystem::create_directories(root / "aliasdir");
  std::filesystem::create_hard_link(root / "stable-link",
                                    root / "aliasdir" / "stable-alias");
  std::string root_name = root.generic_string();

  ClientResource client;
  std::string working = root.generic_string();
  client.working_directory = working.data();
  auto* old_me = me;
  me = &client;
  JobControlRecord jcr;
  FiledJcrImpl impl;
  jcr.fd_impl = &impl;
  bstrncpy(jcr.Job, "ntfs-full", sizeof(jcr.Job));
  OSDependentInit();
  ASSERT_NE(EnableBackupPrivileges(&jcr, 1) & (1 << 1), 0);
  findFILESET fileset;
  findIncludeExcludeItem include{};
  include.opts_list.init(1, not_owned_by_alist);
  findFOPTS options;
  include.opts_list.append(&options);
  fileset.incexe = &include;
  FindFilesPacket ff;
  ff.fileset = &fileset;
  NtfsFileList full;
  bool prepared = full.Prepare(&jcr, &ff, root_name.c_str());
  EXPECT_TRUE(prepared) << full.fallback_reason;
  if (prepared) {
    const auto* names = full.Children(root_name.c_str());
    EXPECT_NE(names, nullptr);
    if (names) {
      EXPECT_NE(std::find(names->begin(), names->end(), "alias"), names->end());
      EXPECT_NE(std::find(names->begin(), names->end(), "changed"),
                names->end());
      EXPECT_NE(std::find(names->begin(), names->end(), "subdir"),
                names->end());
    }
    EXPECT_TRUE(full.SaveCheckpoints(&jcr));
    auto old_time = std::filesystem::last_write_time(root / "changed");
    std::ofstream(root / "changed") << "modified but same timestamp";
    std::filesystem::last_write_time(root / "changed", old_time);
    std::filesystem::remove(root / "deleted");
    std::filesystem::rename(root / "subdir", root / "moved");
    std::filesystem::rename(root / "aliasdir", root / "moved-aliasdir");
    open << "second coalesced write";
    open.flush();
    ff.incremental = true;
    bstrncpy(impl.PrevJob, "ntfs-full", sizeof(impl.PrevJob));
    bstrncpy(jcr.Job, "ntfs-incremental", sizeof(jcr.Job));
    NtfsFileList incremental;
    EXPECT_TRUE(incremental.Prepare(&jcr, &ff, root_name.c_str()))
        << incremental.fallback_reason;
    EXPECT_FALSE(incremental.Unchanged(
        &jcr, &ff, (root / "changed").generic_string().c_str()));
    EXPECT_FALSE(incremental.Unchanged(
        &jcr, &ff, (root / "alias").generic_string().c_str()));
    EXPECT_TRUE(incremental.Unchanged(
        &jcr, &ff, (root / "unchanged").generic_string().c_str()));
    EXPECT_FALSE(incremental.Unchanged(
        &jcr, &ff, (root / "moved" / "child").generic_string().c_str()));
    EXPECT_FALSE(incremental.Unchanged(
        &jcr, &ff, (root / "open").generic_string().c_str()));
    EXPECT_FALSE(incremental.Unchanged(
        &jcr, &ff,
        (root / "moved-aliasdir" / "stable-alias").generic_string().c_str()));
    const auto* after_delete = incremental.Children(root_name.c_str());
    ASSERT_NE(after_delete, nullptr);
    EXPECT_EQ(std::find(after_delete->begin(), after_delete->end(), "deleted"),
              after_delete->end());

    auto* traversal = init_find_files();
    traversal->file_list = &incremental;
    traversal->fileset = &fileset;
    traversal->incremental = true;
    traversal->save_time = std::numeric_limits<time_t>::max();
    static std::vector<std::string> visited;
    visited.clear();
    EXPECT_EQ(FindOneFile(
                  &jcr, traversal,
                  [](JobControlRecord*, FindFilesPacket* packet, bool) {
                    EXPECT_NE(packet->type, FT_NOCHG);
                    EXPECT_NE(packet->type, FT_NOSTAT);
                    visited.emplace_back(packet->fname);
                    packet->FileIndex = 1;
                    return 1;
                  },
                  root_name.data(), static_cast<dev_t>(-1), true),
              1);
    for (const auto& relative :
         {"changed", "alias", "moved/child", "moved-aliasdir/stable-alias"}) {
      EXPECT_NE(std::find(visited.begin(), visited.end(),
                          (root / relative).generic_string()),
                visited.end());
    }
    EXPECT_EQ(std::find(visited.begin(), visited.end(),
                        (root / "unchanged").generic_string()),
              visited.end());
    TermFindFiles(traversal);

    TestAccurateList accurate(&jcr, 2);
    std::string unchanged_name = (root / "unchanged").generic_string();
    std::string deleted_name = (root / "deleted").generic_string();
    char stat[] = "stat";
    EXPECT_TRUE(accurate.AddFile(unchanged_name.data(),
                                 static_cast<int>(unchanged_name.size()), stat,
                                 4, nullptr, 0, 0));
    EXPECT_TRUE(accurate.AddFile(deleted_name.data(),
                                 static_cast<int>(deleted_name.size()), stat, 4,
                                 nullptr, 0, 0));
    impl.file_list = &accurate;
    jcr.accurate = true;
    EXPECT_TRUE(incremental.Unchanged(&jcr, &ff, unchanged_name.c_str()));
    EXPECT_TRUE(accurate.Seen(unchanged_name));
    EXPECT_FALSE(accurate.Seen(deleted_name));
    impl.file_list = nullptr;
    jcr.accurate = false;
    EXPECT_TRUE(incremental.SaveCheckpoints(&jcr));
    bstrncpy(impl.PrevJob, "ntfs-incremental", sizeof(impl.PrevJob));
    bstrncpy(jcr.Job, "ntfs-second-incremental", sizeof(jcr.Job));
    NtfsFileList second;
    EXPECT_TRUE(second.Prepare(&jcr, &ff, root_name.c_str()))
        << second.fallback_reason;
    EXPECT_TRUE(second.Unchanged(&jcr, &ff,
                                 (root / "changed").generic_string().c_str()));
    bstrncpy(impl.PrevJob, "ntfs-full", sizeof(impl.PrevJob));
    NtfsFileList differential;
    EXPECT_TRUE(differential.Prepare(&jcr, &ff, root_name.c_str()))
        << differential.fallback_reason;
    EXPECT_FALSE(differential.Unchanged(
        &jcr, &ff, (root / "changed").generic_string().c_str()));
    SetBit(FO_NO_RECURSION, ff.flags);
    NtfsFileList changed_selection;
    EXPECT_TRUE(changed_selection.Prepare(&jcr, &ff, root_name.c_str()))
        << changed_selection.fallback_reason;
    EXPECT_FALSE(
        changed_selection.Unchanged(&jcr, &ff, unchanged_name.c_str()));
    ClearBit(FO_NO_RECURSION, ff.flags);

    std::vector<std::filesystem::path> temporary_files;
    for (const auto& entry : std::filesystem::directory_iterator(root)) {
      if (entry.path().extension() != ".state") { continue; }
      std::filesystem::path temporary = entry.path().wstring() + L".tmp";
      std::ofstream(temporary) << "do not overwrite";
      temporary_files.push_back(temporary);
    }
    EXPECT_FALSE(incremental.SaveCheckpoints(&jcr));
    for (const auto& temporary : temporary_files) {
      std::string content;
      std::ifstream(temporary) >> content;
      EXPECT_EQ(content, "do");
      std::filesystem::remove(temporary);
    }

    bstrncpy(impl.PrevJob, "missing", sizeof(impl.PrevJob));
    NtfsFileList missing;
    EXPECT_TRUE(missing.Prepare(&jcr, &ff, root_name.c_str()));
    EXPECT_FALSE(missing.Unchanged(&jcr, &ff, unchanged_name.c_str()));
    ff.StripPath = 1;
    EXPECT_FALSE(missing.Prepare(&jcr, &ff, root_name.c_str()));
    EXPECT_EQ(missing.Children(root_name.c_str()), nullptr);
    EXPECT_FALSE(missing.fallback_reason.empty());
  }
  open.close();
  jcr.fd_impl = nullptr;
  me = old_me;
  client.working_directory = nullptr;
  cleanup();
}
#endif

}  // namespace
