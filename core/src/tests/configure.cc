/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2022-2024 Bareos GmbH & Co. KG

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

#include "testing_dir_common.h"

#include "dird/ua.h"
#include "include/jcr.h"
#include "dird/ua_configure.cc"

TEST(ConfigureExport, ReturnsQuotedNameAndPassword)
{
  InitDirGlobals();
  std::string path_to_config = std::string("configs/configure");
  PConfigParser client_config(DirectorPrepareResources(path_to_config));
  if (!client_config) { return; }


  JobControlRecord jcr{};
  directordaemon::UaContext* ua = directordaemon::new_ua_context(&jcr);
  PoolMem resource(PM_MESSAGE);
  ConfigureCreateFdResourceString(ua, resource, "bareos-fd");
  std::string expected_output{
      "Director {\n"
      "  Name = \"bareos director\"\n"
      "  Password = \"[md5]9999\"\n"
      "}\n"};

  EXPECT_EQ(resource.c_str(), expected_output);

  FreeUaContext(ua);
}
