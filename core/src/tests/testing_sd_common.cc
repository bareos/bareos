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

#include "testing_sd_common.h"

void InitSdGlobals()
{
  OSDependentInit();
#if HAVE_WIN32
  WSA_Init();
#endif
  storagedaemon::my_config = nullptr;
  storagedaemon::me = nullptr;
}

PConfigParser StoragePrepareResources(const std::string& path_to_config)
{
  PConfigParser storage_config(
      storagedaemon::InitSdConfig(path_to_config.c_str(), M_INFO));
  storagedaemon::my_config = storage_config.get();

  EXPECT_NE(storage_config.get(), nullptr);
  if (!storage_config) { return nullptr; }

  bool parse_storage_config_ok = storage_config->ParseConfig();
  EXPECT_TRUE(parse_storage_config_ok) << "Could not parse storage config";
  if (!parse_storage_config_ok) { return nullptr; }

  Dmsg0(200, "Start UA server\n");
  storagedaemon::me
      = (storagedaemon::StorageResource*)storage_config->GetNextRes(
          storagedaemon::R_STORAGE, nullptr);
  storagedaemon::my_config->own_resource_ = storagedaemon::me;

  return storage_config;
}
