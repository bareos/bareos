/**
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
#  include "include/bareos.h"
#  include "gtest/gtest.h"
#else
#  include "gtest/gtest.h"
#  include "include/bareos.h"
#endif

#include "dird/job.h"
#include "dird/ua.h"
#include "dird/ua_cmds.h"
#include "dird/ua_server.h"
#include "include/jcr.h"
#include "include/job_types.h"
#include "include/make_unique.h"
#include "lib/output_formatter.h"

#include "lib/parse_conf.h"
#include "dird/dird_globals.h"
#include "dird/dird_conf.h"


#include <array>

namespace directordaemon {
bool DoReloadConfig() { return false; }
}  // namespace directordaemon

using namespace directordaemon;

static void Test_FreeJcr(JobControlRecord* jcr) { FreeJcr(jcr); }

static bool sprintit(void* ctx, const char* fmt, ...)
{
  va_list arg_ptr;
  PoolMem msg;

  va_start(arg_ptr, fmt);
  msg.Bvsprintf(fmt, arg_ptr);
  va_end(arg_ptr);

  std::cout << msg.c_str() << std::endl;
  return true;
}


TEST(setdevice, scan_command_line)
{
  OSDependentInit();
  std::string path_to_config_file = std::string(
      RELATIVE_PROJECT_SOURCE_DIR "/configs/bareos-configparser-tests");
  my_config = InitDirConfig(path_to_config_file.c_str(), M_ERROR_TERM);

  std::unique_ptr<JobControlRecord, decltype(&Test_FreeJcr)> jcr(
      NewDirectorJcr(), &Test_FreeJcr);

  std::unique_ptr<UaContext, decltype(&FreeUaContext)> ua(
      new_ua_context(jcr.get()), &FreeUaContext);

  delete ua->send;
  ua->send = new OutputFormatter(sprintit, ua.get(), nullptr, ua.get());

  std::string command_line{
      "setdevice storage=File device=Any "
      "autoselect=yes"};

  ParseArgs(command_line.c_str(), ua->args, &ua->argc, ua->argk, ua->argv,
            MAX_CMD_ARGS);

  auto result = SetDeviceCommand::ScanCommandLine(ua.get());
  EXPECT_TRUE(!result.empty());

  std::array<std::string, 5> wrong_command_lines{
      "setdevice device=Any autoselect=yes",
      "setdevice storage=File autoselect=yes",
      "setdevice storage=File device=Any",
      "setdevice torage=File device=Any autoselect=yes",
      "setdevice storage=File device=Any autoselect="};

  for (const auto& c : wrong_command_lines) {
    ParseArgs(c.c_str(), ua->args, &ua->argc, ua->argk, ua->argv, MAX_CMD_ARGS);
    auto result = SetDeviceCommand::ScanCommandLine(ua.get());
    EXPECT_TRUE(result.empty());
  }
}
