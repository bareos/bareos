/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2020-2020 Bareos GmbH & Co. KG

   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation, which is
   listed in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   Affero General Public License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.
*/

#include "include/bareos.h"
#include "cats/cats.h"
#include "cats/cats_backends.h"
#include "cats/sql_pooling.h"
#include "dird/get_database_connection.h"
#include "dird/dird_conf.h"
#include "dird/dird_globals.h"
#include "dird/jcr_private.h"
#include "dird/job.h"
#include "dird/dbconvert/database_connection.h"
#include "dird/dbconvert/database_import.h"
#include "include/make_unique.h"
#include "lib/parse_conf.h"
#include "lib/util.h"

#include <iostream>

#if !defined HAVE_DYNAMIC_CATS_BACKENDS
#error "NOT DEFINED: HAVE_DYNAMIC_CATS_BACKENDS"
#endif

namespace directordaemon {
bool DoReloadConfig() { return false; }
}  // namespace directordaemon

class DbConvert {
 public:
  explicit DbConvert(int argc, char** argv)
  {
    InitMsg(nullptr, nullptr);
    SetWorkingDir();
    cl.ParseCommandLine(argc, argv);
    ParseConfig();
    ConnectToDatabases();
  }

  void DoDatabaseConversion()
  {
    DatabaseImporter imp(*source_db_);

#if 0
    std::unique_ptr<DatabaseTableDescriptions> destination_tables =
        DatabaseTableDescriptions::Create(*destination_db_);
    for (const auto& t : destination_tables->tables) {
      std::cout << t.table_name << std::endl;
    }
#endif
  }

 private:
  void SetWorkingDir()
  {
    if (getcwd(current_working_directory_.data(),
               current_working_directory_.size()) == nullptr) {
      throw std::runtime_error(
          "Could not determine current working directory.");
    }
    SetWorkingDirectory(current_working_directory_.data());
  }

  void ParseConfig()
  {
    directordaemon::my_config =
        directordaemon::InitDirConfig(cl.configpath_.c_str(), M_ERROR_TERM);

    my_config_.reset(directordaemon::my_config);

    if (!directordaemon::my_config->ParseConfig()) {
      throw std::runtime_error("Error when loading config.");
    }

    directordaemon::me = static_cast<directordaemon::DirectorResource*>(
        my_config->GetNextRes(directordaemon::R_DIRECTOR, nullptr));

    if (!directordaemon::me) {
      throw std::runtime_error("Could not find director resource.");
    }

    DbSetBackendDirs(std::move(directordaemon::me->backend_directories));
  }

  void ConnectToDatabases()
  {
    try {
      source_db_ = std::make_unique<DatabaseConnection>(
          cl.source_db_resource_name, directordaemon::my_config);

      destination_db_ = std::make_unique<DatabaseConnection>(
          cl.destination_db_resource_name, directordaemon::my_config);

    } catch (const std::runtime_error& e) {
      throw e;
    }
  }

  class CommandLineParser {
    friend class DbConvert;

   public:
    void ParseCommandLine(int argc, char** argv)
    {
      char c{};
      bool options_error{false};
      bool configpath_set{};

      while ((c = getopt(argc, argv, "c:?")) != -1 && !options_error) {
        switch (c) {
          case 'c':
            configpath_ = optarg;
            configpath_set = true;
            break;
          case '?':
          default:
            options_error = true;
            break;
        }
      }

      int argcount{configpath_set ? 5 : 3};

      if (options_error || argc != argcount) {
        usage();
        throw std::runtime_error(std::string());
      }
      source_db_resource_name = argv[3];
      destination_db_resource_name = argv[4];
    }

   private:
    std::string configpath_{"/etc/bareos"};
    std::string source_db_resource_name, destination_db_resource_name;

    void usage() noexcept
    {
      kBareosVersionStrings.PrintCopyright(stderr, 2000);

      fprintf(stderr, _("Usage: dbconvert [options] Source Destination\n"
                        "        -c <path>   use <path> as configuration file "
                        "or directory\n"
                        "        -?          print this message.\n"
                        "\n"));
    }
  };  // class CommandLineParser

 public:
  ~DbConvert() = default;
  DbConvert(const DbConvert& other) = delete;
  DbConvert(const DbConvert&& other) = delete;
  DbConvert& operator=(const DbConvert& rhs) = delete;
  DbConvert& operator=(const DbConvert&& rhs) = delete;

 private:
  CommandLineParser cl;
  std::unique_ptr<DatabaseConnection> source_db_;
  std::unique_ptr<DatabaseConnection> destination_db_;
  std::unique_ptr<ConfigurationParser> my_config_;
  std::array<char, 1024> current_working_directory_;
};

class Cleanup {
 public:
  ~Cleanup()
  {
    DbSqlPoolDestroy();
    DbFlushBackends();
  }
};

int main(int argc, char** argv)
{
  Cleanup c;

  try {
    DbConvert dbconvert(argc, argv);
    dbconvert.DoDatabaseConversion();
  } catch (const std::runtime_error& e) {
    std::string errstring{e.what()};
    if (!errstring.empty()) { std::cerr << e.what() << std::endl; }
    return 1;
  }
  return 0;
}
