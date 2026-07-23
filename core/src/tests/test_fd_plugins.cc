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
#if defined(HAVE_MINGW)
#  include "include/bareos.h"
#  include "gtest/gtest.h"
#else
#  include "gtest/gtest.h"
#  include "include/bareos.h"
#endif

#include "filed/filed.h"
#include "filed/filed_globals.h"
#include "filed/filed_jcr_impl.h"
#include "filed/accurate.h"
#include "filed/heartbeat.h"
#include "filed/fileset.h"
#include "filed/heartbeat.h"
#include "filed/fd_plugins.h"
#include "findlib/attribs.h"
#include "findlib/find.h"
#include "findlib/find_one.h"
#include "findlib/hardlink.h"

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

int SaveFile(JobControlRecord*, FindFilesPacket*, bool) { return 0; }

bool AccurateMarkFileAsSeen(JobControlRecord*, char*) { return true; }

bool AccurateMarkAllFilesAsSeen(JobControlRecord*) { return true; }

bool accurate_unMarkFileAsSeen(JobControlRecord*, char*) { return true; }

bool accurate_unMarkAllFilesAsSeen(JobControlRecord*) { return true; }

bool SetCmdPlugin(BareosFilePacket*, JobControlRecord*) { return true; }

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

// Callback used in postBackupFile unit tests: sets file size to 100.
static bRC TestPostBackupFileSetSize(PluginContext*, save_pkt* sp)
{
  sp->statp.st_size = 100;
  return bRC_OK;
}

// Build a minimal Plugin with PluginFunctions of a given declared size.
// funcs_size controls what the plugin reports as its struct size, letting
// tests simulate an old plugin that doesn't know about postBackupFile.
struct PluginTestHarness {
  PluginFunctions funcs{};
  Plugin plugin{};
  PluginContext ctx{};
  FiledJcrImpl fd_impl{};
  save_pkt sp{};
  FindFilesPacket ff{};
  JobControlRecord jcr{};

  explicit PluginTestHarness(uint32_t funcs_size = sizeof(PluginFunctions))
  {
    funcs.size = funcs_size;
    funcs.version = FD_PLUGIN_INTERFACE_VERSION;
    plugin.plugin_functions = &funcs;
    ctx.plugin = &plugin;
    sp.statp.st_mode = S_IFREG | 0644;
    ff.statp.st_mode = S_IFREG | 0644;
    fd_impl.plugin_sp = &sp;
    jcr.fd_impl = &fd_impl;
    jcr.plugin_ctx = &ctx;
  }
};

// A warning must be recorded when:
//   - the plugin has no postBackupFile callback (old ABI, struct too small)
//   - the file started at st_size == 0 (regular file, explicitly zero)
//   - bytes were actually read/written (ReadBytes grew)
TEST(fd, postbackupfile_warning_when_no_callback_and_bytes_written)
{
  static char bpipe_cmd[] = "bpipe:file=/test:reader=echo hi:writer=/bin/cat";

  // Struct size stops before postBackupFile — simulates a pre-Phase-3 plugin.
  PluginTestHarness h{(uint32_t)offsetof(PluginFunctions, postBackupFile)};
  h.sp.cmd = bpipe_cmd;
  h.sp.statp.st_size = 0;
  h.ff.statp.st_size = 0;
  h.jcr.ReadBytes = 1024;

  PluginPostBackupFile(&h.jcr, &h.ff, /*read_bytes_before=*/0);

  ASSERT_EQ(h.fd_impl.plugin_postwrite_warnings.count("bpipe"), 1u);
  EXPECT_EQ(h.fd_impl.plugin_postwrite_warnings.at("bpipe"), 1u);
}

// A warning must also be recorded when st_size == -1 (unknown, as set by
// bpipe-fd) and bytes were written — the plugin couldn't know the size ahead
// of streaming and never provided a post-write update.
TEST(fd, postbackupfile_warning_when_no_callback_and_size_negative_one)
{
  static char bpipe_cmd[] = "bpipe:file=/test:reader=echo hi:writer=/bin/cat";

  PluginTestHarness h{(uint32_t)offsetof(PluginFunctions, postBackupFile)};
  h.sp.cmd = bpipe_cmd;
  h.sp.statp.st_size = -1;  // bpipe-fd uses -1 for "unknown size"
  h.ff.statp.st_size = -1;
  h.jcr.ReadBytes = 41;

  PluginPostBackupFile(&h.jcr, &h.ff, /*read_bytes_before=*/0);

  ASSERT_EQ(h.fd_impl.plugin_postwrite_warnings.count("bpipe"), 1u);
  EXPECT_EQ(h.fd_impl.plugin_postwrite_warnings.at("bpipe"), 1u);
}

// No warning when the file already had a non-zero size before the backup —
// only genuinely zero-reported files should be flagged.
TEST(fd, postbackupfile_no_warning_for_nonzero_initial_size)
{
  static char bpipe_cmd[] = "bpipe:file=/test:reader=echo hi:writer=/bin/cat";

  PluginTestHarness h{(uint32_t)offsetof(PluginFunctions, postBackupFile)};
  h.sp.cmd = bpipe_cmd;
  h.sp.statp.st_size = 1024;
  h.ff.statp.st_size = 1024;
  h.jcr.ReadBytes = 2048;

  PluginPostBackupFile(&h.jcr, &h.ff, /*read_bytes_before=*/0);

  EXPECT_TRUE(h.fd_impl.plugin_postwrite_warnings.empty());
}

// No warning when no bytes were transferred, even for a zero-size entry —
// an empty bpipe source is legitimate and must not be flagged.
TEST(fd, postbackupfile_no_warning_when_no_bytes_written)
{
  static char bpipe_cmd[] = "bpipe:file=/test:reader=echo hi:writer=/bin/cat";

  PluginTestHarness h{(uint32_t)offsetof(PluginFunctions, postBackupFile)};
  h.sp.cmd = bpipe_cmd;
  h.sp.statp.st_size = 0;
  h.ff.statp.st_size = 0;
  h.jcr.ReadBytes = 100;

  // read_bytes_before == ReadBytes => no new bytes
  PluginPostBackupFile(&h.jcr, &h.ff, /*read_bytes_before=*/100);

  EXPECT_TRUE(h.fd_impl.plugin_postwrite_warnings.empty());
}

// No warning when the plugin implements postBackupFile and updates st_size —
// the callback is the correct opt-in path.
TEST(fd, postbackupfile_no_warning_when_callback_updates_size)
{
  static char plugin_cmd[] = "myplugin:some:args";

  // Full-size struct so postBackupFile is within range.
  PluginTestHarness h{(uint32_t)sizeof(PluginFunctions)};
  h.funcs.postBackupFile = TestPostBackupFileSetSize;
  h.sp.cmd = plugin_cmd;
  h.sp.statp.st_size = 0;
  h.ff.statp.st_size = 0;
  h.jcr.ReadBytes = 500;

  PluginPostBackupFile(&h.jcr, &h.ff, /*read_bytes_before=*/0);

  EXPECT_TRUE(h.fd_impl.plugin_postwrite_warnings.empty());
  // ff_pkt->statp must reflect the size set by the callback (100).
  EXPECT_EQ(h.ff.statp.st_size, (off_t)100);
}

// ff_pkt->statp must always be overwritten with sp->statp after the call,
// regardless of whether the plugin has a callback.
TEST(fd, postbackupfile_always_updates_ffpkt_stat)
{
  static char bpipe_cmd[] = "bpipe:file=/test:reader=echo hi:writer=/bin/cat";

  PluginTestHarness h{(uint32_t)offsetof(PluginFunctions, postBackupFile)};
  h.sp.cmd = bpipe_cmd;
  h.sp.statp.st_size = 42;
  h.sp.statp.st_mode = S_IFREG | 0600;
  h.ff.statp.st_size = 0;
  h.ff.statp.st_mode = S_IFREG | 0644;
  h.jcr.ReadBytes = 42;

  PluginPostBackupFile(&h.jcr, &h.ff, /*read_bytes_before=*/0);

  EXPECT_EQ(h.ff.statp.st_size, (off_t)42);
  EXPECT_EQ(h.ff.statp.st_mode, h.sp.statp.st_mode);
}

} /* namespace filedaemon */
