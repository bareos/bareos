/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2021-2022 Bareos GmbH & Co. KG

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

void InitDirGlobals()
{
  OSDependentInit();
#if HAVE_WIN32
  WSA_Init();
#endif
  directordaemon::my_config = nullptr;
  directordaemon::me = nullptr;
}

PConfigParser DirectorPrepareResources(const std::string& path_to_config)
{
  PConfigParser director_config(
      directordaemon::InitDirConfig(path_to_config.c_str(), M_INFO));
  directordaemon::my_config
      = director_config.get(); /* set the director global variable */

  EXPECT_NE(director_config.get(), nullptr);
  if (!director_config) { return nullptr; }

  bool parse_director_config_ok = director_config->ParseConfig();
  EXPECT_TRUE(parse_director_config_ok) << "Could not parse director config";
  if (!parse_director_config_ok) { return nullptr; }

  directordaemon::me
      = (directordaemon::DirectorResource*)director_config->GetNextRes(
          directordaemon::R_DIRECTOR, nullptr);
  directordaemon::my_config->own_resource_ = directordaemon::me;

  return director_config;
}
