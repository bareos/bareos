/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2020-2022 Bareos GmbH & Co. KG

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
    list_of_filepointers.insert(std::pair<std::string, FILE*>(filename, fp));
    return fp;
  }

  LogFiles() = default;

  ~LogFiles()
  {
    for (auto fp : list_of_filepointers) {
      fflush(fp.second);
      fclose(fp.second);
    }
    list_of_filepointers.clear();
  }

  static std::map<std::string, FILE*> list_of_filepointers;
};

std::map<std::string, FILE*> LogFiles::list_of_filepointers;

static void SyslogCallback_([[maybe_unused]] int mode, const char* msg)
{
  try {
    FILE* fp = LogFiles::list_of_filepointers.at("syslog");
    // fake-write a syslog entry
    if (fp) { fputs(msg, fp); }
  } catch (const std::out_of_range& e) {
    std::cout << "Could not find <syslog> filepointer";
  }
}

static bool DbLogInsertCallback_([[maybe_unused]] JobControlRecord* jcr,
                                 [[maybe_unused]] utime_t mtime,
                                 const char* msg)
{
  try {
    FILE* fp = LogFiles::list_of_filepointers.at("dblog");
    // fake-write a db log entry
    if (fp) { fputs(msg, fp); }
  } catch (const std::out_of_range& e) {
    std::cout << "Could not find <dblog> filepointer";
  }

  return true;
}

TEST(messages_resource, send_message_to_all_configured_destinations)
{
  std::string config_dir = getenv_std_string("BAREOS_CONFIG_DIR");
  std::string working_dir = getenv_std_string("BAREOS_WORKING_DIR");
  std::string log_dir = getenv_std_string("BAREOS_LOG_DIR");
  std::string backend_dir = getenv_std_string("backenddir");

  ASSERT_FALSE(config_dir.empty());
  ASSERT_FALSE(working_dir.empty());
  ASSERT_FALSE(log_dir.empty());
  ASSERT_FALSE(backend_dir.empty());

  std::string regress_debug = getenv_std_string("REGRESS_DEBUG");

  if (!regress_debug.empty()) {
    if (regress_debug == "1") { debug_level = 200; }
  }

  SetWorkingDirectory(working_dir.c_str());
  InitConsoleMsg(log_dir.c_str());

  // parse config
  std::unique_ptr<ConfigurationParser> p{
      InitDirConfig(config_dir.c_str(), M_ERROR_TERM)};
  directordaemon::my_config = p.get();
  ASSERT_TRUE(directordaemon::my_config->ParseConfig());

  // get message resource
  MessagesResource* messages = dynamic_cast<MessagesResource*>(
      directordaemon::my_config->GetResWithName(directordaemon::R_MSGS,
                                                "Standard"));
  ASSERT_NE(messages, nullptr);

  // initialize message handler
  InitMsg(NULL, messages);

  // this object cleans up all files at exit
  LogFiles cleanup;

  // init syslog mock-up
  ASSERT_NE(LogFiles::Open(log_dir, "syslog"), nullptr);
  RegisterSyslogCallback(SyslogCallback_);

  // init catalog mock-up
  JobControlRecord jcr;
  jcr.db = reinterpret_cast<BareosDb*>(0xdeadbeef);
  ASSERT_NE(LogFiles::Open(log_dir, "dblog"), nullptr);
  SetDbLogInsertCallback(DbLogInsertCallback_);

  DispatchMessage(&jcr, M_ERROR, 0, "\n!!!This is a test error message!!!\n");
  TermMsg();
}
