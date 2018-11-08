/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2018-2018 Bareos GmbH & Co. KG

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

#include "gtest/gtest.h"

#include "console/console_conf.h"
#include "console/console_globals.h"
#include "console/connect_to_director.h"
#include "dird/socket_server.h"
#include "dird/dird_conf.h"
#include "dird/dird_globals.h"

#include "lib/tls_openssl.h"
#include "lib/bnet.h"
#include "lib/bstringlist.h"

#include "include/jcr.h"

#if 0
TEST(bsock, console_director_connection_test)
{
   const char* console_configfile = "/home/franku/01-prj/git/bareos-18.2-release-fixes/regress/bin/";
   std::unique_ptr<ConfigurationParser> c_guard(console::InitConsConfig(console_configfile, M_INFO));

   EXPECT_NE(console::my_config, nullptr);
   if (!console::my_config) {
     return;
   }

   bool ok = console::my_config->ParseConfig();
   EXPECT_TRUE(ok) << "Could not parse console config";

   console::director_resource = reinterpret_cast<console::DirectorResource*>
                                    (console::my_config->GetNextRes(console::R_DIRECTOR, NULL));
   console::console_resource = reinterpret_cast<console::ConsoleResource*>
                                    (console::my_config->GetNextRes(console::R_CONSOLE, NULL));


   const char* director_configfile = "/home/franku/01-prj/git/bareos-18.2-release-fixes/"
                                     "regress/bin/";

   std::unique_ptr<ConfigurationParser> d_guard(directordaemon::InitDirConfig(director_configfile, M_INFO));

   EXPECT_NE(directordaemon::my_config, nullptr);
   if (!directordaemon::my_config) {
     return;
   }

   ok = directordaemon::my_config->ParseConfig();
   EXPECT_TRUE(ok) << "Could not parse director config";

   Dmsg0(200, "Start UA server\n");
   directordaemon::me = (directordaemon::DirectorResource *)directordaemon::
                              my_config->GetNextRes(directordaemon::R_DIRECTOR, nullptr);
   directordaemon::StartSocketServer(directordaemon::me->DIRaddrs);

   JobControlRecord jcr;
   BStringList args;
   uint32_t response_id;
   BareosSocket *UA_sock = console::ConnectToDirector(jcr, 0, args, response_id);
   EXPECT_NE(UA_sock,nullptr);
   EXPECT_EQ(response_id, kMessageIdOk);

   delete UA_sock;
}
#endif

namespace directordaemon {
 bool DoReloadConfig()
 {
   return false;
 }
}
