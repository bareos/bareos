/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2007-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2016 Planets Communications B.V.
   Copyright (C) 2013-2016 Bareos GmbH & Co. KG

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
 * Kern Sibbald, October 2007
 */
/** @file
 * BAREOS pluginloader
 */
#include "gtest/gtest.h"
#include "include/bareos.h"
#include "dird/dird.h"
#include "dird/dird_globals.h"
#include "dird/dir_plugins.h"
#include "lib/edit.h"

namespace directordaemon {

DirectorResource *me = nullptr;

bool DbGetPoolRecord(JobControlRecord*, BareosDb*, PoolDbRecord*);
bool DbGetPoolRecord(JobControlRecord*, BareosDb*, PoolDbRecord*)
{
   return true;
}

TEST(dir, dir_plugins)
{
   me = new DirectorResource;
   ASSERT_NE(me, nullptr);

   char plugin_dir[PATH_MAX];
   JobControlRecord mjcr1, mjcr2;
   JobControlRecord *jcr1 = &mjcr1;
   JobControlRecord *jcr2 = &mjcr2;

   InitMsg(NULL, NULL);

   OSDependentInit();

   getcwd(plugin_dir, sizeof(plugin_dir)-1);
   LoadDirPlugins(plugin_dir, NULL);

   jcr1->JobId = 111;
   NewPlugins(jcr1);

   jcr2->JobId = 222;
   NewPlugins(jcr2);

   EXPECT_EQ(GeneratePluginEvent(jcr1, bDirEventJobStart, (void *)"Start Job 1"), bRC_OK);
   EXPECT_EQ(GeneratePluginEvent(jcr1, bDirEventJobEnd), bRC_OK);
   EXPECT_EQ(GeneratePluginEvent(jcr2, bDirEventJobStart, (void *)"Start Job 1"), bRC_OK);
   FreePlugins(jcr1);
   EXPECT_EQ(GeneratePluginEvent(jcr2, bDirEventJobEnd), bRC_OK);
   FreePlugins(jcr2);

   UnloadDirPlugins();

   TermMsg();
   CloseMemoryPool();
   sm_dump(false);
}

} /* namespace directordaemon */
