/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2007-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2020 Bareos GmbH & Co. KG

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
/* originally Kern Sibbald, October 2007 */

#if defined(HAVE_MINGW)
#  include "include/bareos.h"
#  include "gtest/gtest.h"
#else
#  include "gtest/gtest.h"
#  include "include/bareos.h"
#endif


#include "stored/stored.h"
#include "stored/stored_globals.h"
#include "stored/sd_plugins.h"
#include "lib/crypto_cache.h"
#include "stored/sd_stats.h"
#include "lib/edit.h"
#include "include/jcr.h"

namespace storagedaemon {

TEST(sd, sd_plugins)
{
  char plugin_dir[1000];
  JobControlRecord mjcr1, mjcr2;
  JobControlRecord* jcr1 = &mjcr1;
  JobControlRecord* jcr2 = &mjcr2;

  InitMsg(NULL, NULL);

  OSDependentInit();

  (void)!getcwd(plugin_dir, sizeof(plugin_dir) - 1);

  LoadSdPlugins(plugin_dir, NULL);

  jcr1->JobId = 111;
  NewPlugins(jcr1);

  jcr2->JobId = 222;
  NewPlugins(jcr2);

  EXPECT_EQ(GeneratePluginEvent(jcr1, bSdEventJobStart, (void*)"Start Job 1"),
            bRC_OK);
  EXPECT_EQ(GeneratePluginEvent(jcr1, bSdEventJobEnd), bRC_OK);
  EXPECT_EQ(GeneratePluginEvent(jcr2, bSdEventJobStart, (void*)"Start Job 1"),
            bRC_OK);
  FreePlugins(jcr1);
  EXPECT_EQ(GeneratePluginEvent(jcr2, bSdEventJobEnd), bRC_OK);
  FreePlugins(jcr2);

  UnloadSdPlugins();

  TermMsg();
  CloseMemoryPool();
}

} /* namespace storagedaemon */
