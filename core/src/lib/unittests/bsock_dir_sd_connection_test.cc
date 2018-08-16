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

static void start_sd_server()
{
   int newsockfd = create_accepted_server_socket(BSOCK_TEST_PORT_NUMBER);
   BareosSocket *bs = create_new_bareos_socket(newsockfd);
   HandleConnectionRequest(bs);
}

TEST(bsock_dir_sd, dir_to_sd_connection_test)
{
//   start_sd_server();
}
