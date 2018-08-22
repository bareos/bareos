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
#include "bsock_test.h"
#include "create_resource.h"
#include "gtest/gtest.h"
#include "stored/socket_server.h"
#include "stored/stored_globals.h"
#include "stored/stored_conf.h"

#define DIRECTOR_DAEMON
#include "include/bareos.h"
#include "console/console_conf.h"
#include "lib/tls_openssl.h"

#include "include/jcr.h"
#include "dird/sd_cmds.h"
#include "dird/dird_globals.h"
#include "dird/dird_conf.h"
#undef DIRECTOR_DAEMON

#include <thread>

static void start_sd_server()
{
   storagedaemon::me = storagedaemon::CreateAndInitializeNewStorageResource();

   int newsockfd = create_accepted_server_socket(BSOCK_TEST_PORT_NUMBER);
   EXPECT_GT(newsockfd, 0);
   if (newsockfd < 0) {
      return;
   }
   std::unique_ptr<BareosSocket> bs(create_new_bareos_socket(newsockfd));
   storagedaemon::HandleConnectionRequest(bs.get());

   delete storagedaemon::me;
   storagedaemon::me = nullptr;
}

static void connect_dir_to_sd()
{
   JobControlRecord jcr;
   memset(&jcr, 0, sizeof(jcr));

   jcr.res.wstore = directordaemon::CreateAndInitializeNewStorageResource();
   jcr.res.wstore->address = (char*)"127.0.0.1";
   jcr.res.wstore->SDport = BSOCK_TEST_PORT_NUMBER;

   directordaemon::me = directordaemon::CreateAndInitializeNewDirectorResource();

   EXPECT_TRUE(directordaemon::ConnectToStorageDaemon(&jcr, 1, 1, true));

   delete jcr.res.wstore;
   jcr.res.wstore = nullptr;

   delete directordaemon::me;
   directordaemon::me = nullptr;
}

//TEST(bsock_dir_sd, dir_to_sd_connection_test)
//{
//   InitForTest();
//   std::thread storage_thread(start_sd_server);
//   std::thread director_thread(connect_dir_to_sd);
//   storage_thread.join();
//   director_thread.join();
//}

/* ********************************************************/
/* dummy wrapper for unused methods from other namespaces */
/* ********************************************************/
namespace directordaemon {
   bool DoReloadConfig() { return false; }
}
