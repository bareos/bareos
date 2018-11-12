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
#include <signal.h>

static void signal_handler(int arg)
{
  return;
}

TEST(bsock, console_director_connection_test)
{
   struct sigaction sig{0};
   sig.sa_handler = signal_handler;
   sigaction(SIGUSR2,&sig, nullptr);
   sigaction(SIGPIPE,&sig, nullptr);

   const char* console_configfile = CMAKE_SOURCE_DIR "/src/tests/configs/";
   std::unique_ptr<ConfigurationParser> console_config_parser(console::InitConsConfig(console_configfile, M_INFO));
   console::my_config = console_config_parser.get(); /* set the console global variable */

   EXPECT_NE(console_config_parser.get(), nullptr);
   if (!console_config_parser) {
     return;
   }

   bool ok = console_config_parser->ParseConfig();
   EXPECT_TRUE(ok) << "Could not parse console config";

   console::director_resource = reinterpret_cast<console::DirectorResource*>
                                    (console_config_parser->GetNextRes(console::R_DIRECTOR, NULL));
   console::console_resource = reinterpret_cast<console::ConsoleResource*>
                                    (console_config_parser->GetNextRes(console::R_CONSOLE, NULL));

   const char* director_configfile = CMAKE_SOURCE_DIR "/src/tests/configs/";

   std::unique_ptr<ConfigurationParser> director_config_parser(directordaemon::InitDirConfig(director_configfile, M_INFO));
   directordaemon::my_config = director_config_parser.get(); /* set the director global variable */

   EXPECT_NE(director_config_parser.get(), nullptr);
   if (!director_config_parser) { return; }

   ok = director_config_parser->ParseConfig();
   EXPECT_TRUE(ok) << "Could not parse director config";
   if (!ok) { return; }

   Dmsg0(200, "Start UA server\n");
   directordaemon::me = (directordaemon::DirectorResource *)
                         director_config_parser->GetNextRes(directordaemon::R_DIRECTOR, nullptr);
   directordaemon::StartSocketServer(directordaemon::me->DIRaddrs);

   JobControlRecord jcr;
   memset(&jcr, 0, sizeof(jcr));
   BStringList args;
   uint32_t response_id;

   std::unique_ptr<BareosSocket, std::function<void(BareosSocket*)>>
                UA_sock(console::ConnectToDirector(jcr, 0, args, response_id), [](BareosSocket *p) { delete p; });

   EXPECT_EQ(response_id, kMessageIdOk);

   EXPECT_NE(UA_sock.get(), nullptr);
   if (!UA_sock) { return; }

   EXPECT_NE(UA_sock->tls_conn.get(), nullptr);
   std::string cipher;
   UA_sock->GetCipherMessageString(cipher);
   std::cout << cipher;

   UA_sock->signal(BNET_TERMINATE);
   UA_sock->close();
   directordaemon::StopSocketServer();
   StopWatchdog();

   UA_sock.reset();

   director_config_parser.reset();
   console_config_parser.reset();
}

namespace directordaemon {
 bool DoReloadConfig()
 {
   return false;
 }
}
