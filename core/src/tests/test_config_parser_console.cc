/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2019-2019 Bareos GmbH & Co. KG

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
#include "include/bareos.h"
#include "lib/parse_conf.h"
#include "lib/smartall.h"
#include "filed/filed_globals.h"
#include "filed/filed_conf.h"

namespace filedaemon {

TEST(ConfigParser, test_filed_config)
{
  InitMsg(NULL, NULL); /* initialize message handler */

  std::string path_to_config_file = std::string(
      PROJECT_SOURCE_DIR "/src/tests/configs/stored_multiplied_device");
  my_config = InitFdConfig(path_to_config_file.c_str(), M_ERROR_TERM);
  my_config->ParseConfig();

  delete my_config;

  TermMsg();         /* Terminate message handler */
  CloseMemoryPool(); /* release free memory in pool */
  debug_level = 200;
  sm_dump(false, false);
}

}  // namespace filedaemon
