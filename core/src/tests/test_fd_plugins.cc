/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2007-2012 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2026 Bareos GmbH & Co. KG

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
/*
 * extracted the TEST_PROGRAM functionality from filed/fd_plugins.cc
 * and adapted for gtest
 *
 * Andreas Rogge, Feb 2019
 */

/**
 * @file
 * Bareos pluginloader
 */
#include "gtest/gtest.h"
#include "include/bareos.h"

#include "filed/filed.h"
#include "filed/filed_globals.h"
#include "filed/accurate.h"
#include "filed/heartbeat.h"
#include "filed/fileset.h"
#include "filed/heartbeat.h"
#include "findlib/attribs.h"
#include "findlib/find.h"
#include "findlib/find_one.h"
#include "findlib/hardlink.h"

#include <limits>
#include <optional>

#ifdef HAVE_MSVC
#  define PATH_MAX MAX_PATH
#endif

// Function pointers to be set here (findlib)
extern int (*plugin_bopen)(BareosFilePacket* bfd,
                           const char* fname,
                           int flags,
                           mode_t mode);
extern int (*plugin_bclose)(BareosFilePacket* bfd);
extern ssize_t (*plugin_bread)(BareosFilePacket* bfd, void* buf, size_t count);
extern ssize_t (*plugin_bwrite)(BareosFilePacket* bfd, void* buf, size_t count);
extern boffset_t (*plugin_blseek)(BareosFilePacket* bfd,
                                  boffset_t offset,
                                  int whence);

extern char* exepath;
extern char* version;

namespace filedaemon {

int SaveFile(JobControlRecord*, FindFilesPacket*, bool);
bool SetCmdPlugin(BareosFilePacket*, JobControlRecord*);
uint64_t PluginBlocksFromSize(uint64_t size);
void FillMissingPluginStatBlocks(struct stat& statp);
bool PluginSizeNeedsFdFallback(const struct stat& statp);
bool PluginFileSizeBlocksAreValid(const PluginFileSizeBlocks& corrected);
std::optional<PluginFileSizeBlocks> FdCountedFileSizeBlocks(
    const struct stat& original_statp,
    bool save_status,
    bool job_canceled,
    bool data_was_read,
    uint64_t read_bytes_before,
    uint64_t read_bytes_after);
bool EncodeAndSendAttributes(JobControlRecord*,
                             FindFilesPacket*,
                             int&,
                             bool reuse_file_index);

int SaveFile(JobControlRecord*, FindFilesPacket*, bool) { return 0; }

bool EncodeAndSendAttributes(JobControlRecord*, FindFilesPacket*, int&, bool)
{
  return true;
}

bool AccurateMarkFileAsSeen(JobControlRecord*, char*) { return true; }

bool AccurateMarkAllFilesAsSeen(JobControlRecord*) { return true; }

bool accurate_unMarkFileAsSeen(JobControlRecord*, char*) { return true; }

bool accurate_unMarkAllFilesAsSeen(JobControlRecord*) { return true; }

bool SetCmdPlugin(BareosFilePacket*, JobControlRecord*) { return true; }

TEST(fd, fill_missing_plugin_stat_blocks)
{
  EXPECT_EQ(PluginBlocksFromSize(0), 0);
  EXPECT_EQ(PluginBlocksFromSize(1), 1);
  EXPECT_EQ(PluginBlocksFromSize(512), 1);
  EXPECT_EQ(PluginBlocksFromSize(513), 2);

  // Plugin reported a real size but left st_blocks at 0: derive it.
  struct stat statp{};
  statp.st_size = 513;
  statp.st_blocks = 0;
  FillMissingPluginStatBlocks(statp);
  EXPECT_EQ(statp.st_blocks, 2);

  // Exact multiple of 512 should not round up an extra block.
  statp = {};
  statp.st_size = 1024;
  statp.st_blocks = 0;
  FillMissingPluginStatBlocks(statp);
  EXPECT_EQ(statp.st_blocks, 2);

  // Plugin already provided real block-allocation data: don't overwrite it.
  statp = {};
  statp.st_size = 100000;
  statp.st_blocks = 7;
  FillMissingPluginStatBlocks(statp);
  EXPECT_EQ(statp.st_blocks, 7);

  // Unknown size (e.g. streaming plugin still filling it in): don't guess.
  statp = {};
  statp.st_size = -1;
  statp.st_blocks = 0;
  FillMissingPluginStatBlocks(statp);
  EXPECT_EQ(statp.st_blocks, 0);

  // Legitimate empty file: nothing to derive.
  statp = {};
  statp.st_size = 0;
  statp.st_blocks = 0;
  FillMissingPluginStatBlocks(statp);
  EXPECT_EQ(statp.st_blocks, 0);

  statp = {};
  statp.st_size = 0;
  statp.st_blocks = -1;
  EXPECT_TRUE(PluginSizeNeedsFdFallback(statp));
}

TEST(fd, plugin_file_size_blocks_validation)
{
  EXPECT_TRUE(PluginFileSizeBlocksAreValid(PluginFileSizeBlocks{1234, 3}));
  EXPECT_TRUE(PluginFileSizeBlocksAreValid(PluginFileSizeBlocks{1234, 0}));
  EXPECT_TRUE(PluginFileSizeBlocksAreValid(PluginFileSizeBlocks{0, 0}));

  EXPECT_FALSE(PluginFileSizeBlocksAreValid(PluginFileSizeBlocks{-1, 0}));
  EXPECT_FALSE(PluginFileSizeBlocksAreValid(PluginFileSizeBlocks{1234, -1}));
}

TEST(fd, fd_counted_plugin_size_blocks_fallback)
{
  struct stat statp{};
  statp.st_size = -1;
  statp.st_blocks = 1;

  std::optional<PluginFileSizeBlocks> corrected
      = FdCountedFileSizeBlocks(statp, true, false, true, 100, 1334);

  ASSERT_TRUE(corrected.has_value());
  EXPECT_EQ(corrected->size, 1234);
  EXPECT_EQ(corrected->blocks, 3);

  statp = {};
  statp.st_size = -1;
  statp.st_blocks = 1;
  corrected = FdCountedFileSizeBlocks(statp, true, false, true, 100, 100);
  ASSERT_TRUE(corrected.has_value());
  EXPECT_EQ(corrected->size, 0);
  EXPECT_EQ(corrected->blocks, 0);
}

TEST(fd, fd_counted_plugin_size_blocks_fallback_is_bounded)
{
  struct stat statp{};
  statp.st_size = 1234;
  statp.st_blocks = 3;

  EXPECT_FALSE(
      FdCountedFileSizeBlocks(statp, true, false, true, 100, 1334).has_value());

  statp = {};
  statp.st_size = -1;
  statp.st_blocks = 1;
  EXPECT_FALSE(FdCountedFileSizeBlocks(statp, false, false, true, 100, 1334)
                   .has_value());
  EXPECT_FALSE(
      FdCountedFileSizeBlocks(statp, true, true, true, 100, 1334).has_value());
  EXPECT_FALSE(FdCountedFileSizeBlocks(statp, true, false, false, 100, 1334)
                   .has_value());
  EXPECT_FALSE(
      FdCountedFileSizeBlocks(statp, true, false, true, 1334, 100).has_value());
  EXPECT_FALSE(
      FdCountedFileSizeBlocks(
          statp, true, false, true, 0,
          static_cast<uint64_t>(std::numeric_limits<int64_t>::max()) + 1)
          .has_value());
}

TEST(fd, fd_plugins)
{
  char plugin_dir[PATH_MAX];
  JobControlRecord mjcr1, mjcr2;
  JobControlRecord* jcr1 = &mjcr1;
  JobControlRecord* jcr2 = &mjcr2;

  OSDependentInit();

  (void)!getcwd(plugin_dir, sizeof(plugin_dir) - 1);

  LoadFdPlugins(plugin_dir, NULL);

  jcr1->JobId = 111;
  NewPlugins(jcr1);

  jcr2->JobId = 222;
  NewPlugins(jcr2);

  EXPECT_EQ(GeneratePluginEvent(jcr1, bEventJobStart, (void*)"Start Job 1"),
            bRC_OK);
  EXPECT_EQ(GeneratePluginEvent(jcr1, bEventJobEnd), bRC_OK);
  EXPECT_EQ(GeneratePluginEvent(jcr2, bEventJobStart, (void*)"Start Job 2"),
            bRC_OK);
  FreePlugins(jcr1);
  EXPECT_EQ(GeneratePluginEvent(jcr2, bEventJobEnd), bRC_OK);
  FreePlugins(jcr2);

  UnloadFdPlugins();
}
} /* namespace filedaemon */
