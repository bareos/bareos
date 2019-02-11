/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2007-2012 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2018 Bareos GmbH & Co. KG

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

/**
 * Function pointers to be set here (findlib)
 */
extern int (*plugin_bopen)(BareosWinFilePacket *bfd, const char *fname, int flags, mode_t mode);
extern int (*plugin_bclose)(BareosWinFilePacket *bfd);
extern ssize_t (*plugin_bread)(BareosWinFilePacket *bfd, void *buf, size_t count);
extern ssize_t (*plugin_bwrite)(BareosWinFilePacket *bfd, void *buf, size_t count);
extern boffset_t (*plugin_blseek)(BareosWinFilePacket *bfd, boffset_t offset, int whence);

extern char *exepath;
extern char *version;
extern char *dist_name;

namespace filedaemon {

int SaveFile(JobControlRecord*, FindFilesPacket*, bool);
bool SetCmdPlugin(BareosWinFilePacket*, JobControlRecord*);

/* Exported variables */
ClientResource *me;                        /* my resource */

int SaveFile(JobControlRecord*, FindFilesPacket*, bool)
{
   return 0;
}

bool AccurateMarkFileAsSeen(JobControlRecord*, char*)
{
   return true;
}

bool AccurateMarkAllFilesAsSeen(JobControlRecord*)
{
   return true;
}

bool accurate_unMarkFileAsSeen(JobControlRecord*, char*)
{
   return true;
}

bool accurate_unMarkAllFilesAsSeen(JobControlRecord*)
{
   return true;
}

bool SetCmdPlugin(BareosWinFilePacket*, JobControlRecord*)
{
   return true;
}

TEST(fd, fd_plugins)
{
   char plugin_dir[PATH_MAX];
   JobControlRecord mjcr1, mjcr2;
   JobControlRecord *jcr1 = &mjcr1;
   JobControlRecord *jcr2 = &mjcr2;

   InitMsg(NULL, NULL);

   OSDependentInit();

   getcwd(plugin_dir, sizeof(plugin_dir)-1);
   LoadFdPlugins(plugin_dir, NULL);

   jcr1->JobId = 111;
   NewPlugins(jcr1);

   jcr2->JobId = 222;
   NewPlugins(jcr2);

   EXPECT_EQ(GeneratePluginEvent(jcr1, bEventJobStart, (void *)"Start Job 1"), bRC_OK);
   EXPECT_EQ(GeneratePluginEvent(jcr1, bEventJobEnd), bRC_OK);
   EXPECT_EQ(GeneratePluginEvent(jcr2, bEventJobStart, (void *)"Start Job 2"), bRC_OK);
   FreePlugins(jcr1);
   EXPECT_EQ(GeneratePluginEvent(jcr2, bEventJobEnd), bRC_OK);
   FreePlugins(jcr2);

   UnloadFdPlugins();

   TermMsg();
   CloseMemoryPool();
   LmgrCleanupMain();
   sm_dump(false);
}
} /* namespace filedaemon */
