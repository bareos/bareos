/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2019-2022 Bareos GmbH & Co. KG

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

#include "lib/parse_conf.h"
#include "dird/dird_globals.h"
#include "dird/dird_conf.h"
#include "dird/ua_output.h"
#include "dird/show_cmd_available_resources.h"

#include <set>

using namespace directordaemon;

TEST(available_resources_equals_config_resources, check_contents)
{
  std::unique_ptr<ConfigurationParser> my_config(
      InitDirConfig(nullptr, M_ERROR_TERM));

  std::set<uint32_t> set_of_config_resources;

  for (int i = 0; my_config->resource_definitions_[i].name; i++) {
    if (my_config->resource_definitions_[i].rcode != R_DEVICE) {
      /* skip R_DEVICE, as these are special resources, not shown by the show
       * command. */
      set_of_config_resources.insert(my_config->resource_definitions_[i].rcode);
    }
  }

  std::set<uint32_t> set_of_show_cmd_resources;

  for (int i = 0; show_cmd_available_resources[i].res_name; i++) {
    if (show_cmd_available_resources[i].type >= 0) {
      set_of_show_cmd_resources.insert(show_cmd_available_resources[i].type);
    }
  }

  EXPECT_EQ(set_of_config_resources, set_of_show_cmd_resources)
      << "The list of resources for the show command does not match the list "
         "of configured resources in dird_conf.cc.";
}
