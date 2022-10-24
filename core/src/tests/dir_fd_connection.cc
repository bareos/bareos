/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2022-2022 Bareos GmbH & Co. KG

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
#include "dird/fd_cmds.cc"
#include "dird/job.cc"
#include "dird/jcr_util.h"

TEST(DirectorToClientConnection, DoesNotConnectWhenDisabled)
{
  InitDirGlobals();
  std::string path_to_config
      = std::string(RELATIVE_PROJECT_SOURCE_DIR
                    "/configs/dir_fd_connection/dir_to_fd_connection_no/");

  PConfigParser director_config(DirectorPrepareResources(path_to_config));


  JobControlRecord* jcr
      = directordaemon::NewDirectorJcr(directordaemon::DirdFreeJcr);

  jcr->impl->res.client = static_cast<directordaemon::ClientResource*>(
      directordaemon::my_config->GetResWithName(directordaemon::R_CLIENT,
                                                "fd-no-connection"));
  directordaemon::UaContext* ua = nullptr;

  EXPECT_FALSE(ConnectToFileDaemon(jcr, 0, 0, false, ua));
}
