/*
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
#include "include/bareos.h"
#include "gtest/gtest.h"
#else
#include "gtest/gtest.h"
#include "include/bareos.h"
#endif

#include "dird/dird_conf.h"
#include "dird/dird_globals.h"
#include "dird/get_database_connection.h"
#include "dird/job.h"
#include "dird/jcr_private.h"
#include "lib/messages_resource.h"
#include "lib/parse_conf.h"
#include "lib/util.h"

#include <memory>
#include <vector>

using directordaemon::InitDirConfig;
using directordaemon::my_config;

namespace directordaemon {
bool DoReloadConfig() { return false; }
}  // namespace directordaemon

class LogFiles {
 public:
  static FILE* Open(const std::string& dir, std::string filename)
  {
    std::string fullpath = dir + "/" + filename;
    FILE* fp = fopen(fullpath.c_str(), "w");
    if (!fp) {
      std::cout << "Could not open syslog file " << fullpath << std::endl;
      return nullptr;
    }
    fps.push_back(fp);
    return fp;
  }

  LogFiles() = default;

  ~LogFiles()
  {
    for (auto fp : fps) {
      fflush(fp);
      fclose(fp);
    }
  }

 private:
  static std::vector<FILE*> fps;
};

std::vector<FILE*> LogFiles::fps;
static FILE* syslog_file{};
static FILE* db_log_file{};

static void SyslogCallback_(int mode, const char* msg)
{
  // fake-write a syslog entry
  if (syslog_file) { fputs(msg, syslog_file); }
}

static bool DbLogInsertCallback_(JobControlRecord* jcr,
                                 utime_t mtime,
                                 const char* msg)
{
  // fake-write a db log entry
  if (db_log_file) { fputs(msg, db_log_file); }
  return true;
}

TEST(messages, send_message_to_all_configured_destinations)
{
  std::string config_dir = getenv_std_string("BAREOS_CONFIG_DIR");
  std::string working_dir = getenv_std_string("BAREOS_WORKING_DIR");
  std::string backend_dir = getenv_std_string("backenddir");
  std::string regress_debug = getenv_std_string("REGRESS_DEBUG");

  ASSERT_FALSE(working_dir.empty());
  ASSERT_FALSE(config_dir.empty());
  ASSERT_FALSE(backend_dir.empty());

  if (!regress_debug.empty()) {
    if (regress_debug == "1") { debug_level = 200; }
  }

  SetWorkingDirectory(working_dir.c_str());
  InitConsoleMsg(working_dir.c_str());
  RegisterSyslogCallback(SyslogCallback_);
  SetDbLogInsertCallback(DbLogInsertCallback_);

  // parse config
  std::unique_ptr<ConfigurationParser> p{
      InitDirConfig(config_dir.c_str(), M_ERROR_TERM)};
  directordaemon::my_config = p.get();
  ASSERT_TRUE(directordaemon::my_config->ParseConfig());

  // get message resource
  MessagesResource* messages =
      dynamic_cast<MessagesResource*>(directordaemon::my_config->GetResWithName(
          directordaemon::R_MSGS, "Standard"));
  ASSERT_NE(messages, nullptr);

  // initialize message handler
  InitMsg(NULL, messages);

  LogFiles cleanup_files;

  syslog_file = LogFiles::Open(working_dir, "syslog.txt");
  db_log_file = LogFiles::Open(working_dir, "dblog.txt");

  ASSERT_NE(syslog_file, nullptr);
  ASSERT_NE(db_log_file, nullptr);

  JobControlRecord jcr;
  jcr.db = reinterpret_cast<BareosDb*>(1);

  DispatchMessage(&jcr, M_ERROR, 0, "This is an error message");
}
