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
#include "gtest/gtest.h"
#include "stored/socket_server.h"

//static void start_sd_server()
//{
//   int newsockfd = create_accepted_server_socket(BSOCK_TEST_PORT_NUMBER);
//   BareosSocket *bs = create_new_bareos_socket(newsockfd);
//   storagedaemon::HandleConnectionRequest(bs);
//}
//
//#define DIRECTOR_DAEMON
//#include "include/bareos.h"
//#include "console/console_conf.h"
//#include "lib/tls_openssl.h"
//
//#include "include/jcr.h"
//#include "dird/sd_cmds.h"
//#include "dird/dird_globals.h"
//#include "dird/dird_conf.h"
//
//#include <thread>
//
//static void connect_dir_to_sd()
//{
//   JobControlRecord jcr;
//   directordaemon::ConnectToStorageDaemon(&jcr, 10, 1, true);
//}
//#undef DIRECTOR_DAEMON
//
//TEST(bsock_dir_sd, dir_to_sd_connection_test)
//{
//   std::thread server_thread(start_sd_server);
//   connect_dir_to_sd();
//}

/* ********************************************************/
/* dummy wrapper for unused methods from other namespaces */
/* ********************************************************/
namespace directordaemon {
   bool DoReloadConfig() { return false; }
}
