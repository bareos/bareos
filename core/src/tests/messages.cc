/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2020-2020 Bareos GmbH & Co. KG

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

#if defined(HAVE_MINGW)
#include "include/bareos.h"
#include "gtest/gtest.h"
#else
#include "gtest/gtest.h"
#include "include/bareos.h"
#endif

#include "dird/dird_conf.h"
#include "dird/dird_globals.h"
#include "lib/messages_resource.h"
#include "lib/parse_conf.h"
#include "lib/util.h"

using directordaemon::InitDirConfig;
using directordaemon::my_config;

TEST(messages, send_message_to_all_configured_destinations)
{
  std::string config_dir = getenv_std_string("BAREOS_CONFIG_DIR");
  std::string working_dir = getenv_std_string("BAREOS_WORKING_DIR");

  ASSERT_FALSE(working_dir.empty());
  ASSERT_FALSE(config_dir.empty());

  SetWorkingDirectory(working_dir.c_str());
  InitConsoleMsg(working_dir.c_str());

  std::unique_ptr<ConfigurationParser> p{
      InitDirConfig(config_dir.c_str(), M_ERROR_TERM)};

  directordaemon::my_config = p.get();
  ASSERT_TRUE(directordaemon::my_config->ParseConfig());

  MessagesResource* messages =
      dynamic_cast<MessagesResource*>(directordaemon::my_config->GetResWithName(
          directordaemon::R_MSGS, "Standard"));

  ASSERT_NE(messages, nullptr);

  InitMsg(NULL, messages); /* initialize message handler */

  DispatchMessage(nullptr, M_ERROR, 0, "This is an error message");
}
