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
#include "include/make_unique.h"
#include "lib/parse_conf.h"
#include "lib/util.h"

#include <iostream>

#if !defined HAVE_DYNAMIC_CATS_BACKENDS
#error "NOT DEFINED: HAVE_DYNAMIC_CATS_BACKENDS"
#endif

using directordaemon::InitDirConfig;
using directordaemon::my_config;

namespace directordaemon {
bool DoReloadConfig() { return false; }
}  // namespace directordaemon

class DbTool {
 public:
  enum class DbOrigin
  {
    kSource,
    kDestination
  };

  void ConnectToDatabase(DbOrigin origin)
  {
    const char* db_resource_name{};
    DatabasePointers* db_pointer{};

    switch (origin) {
      default:
      case DbOrigin::kSource:
        db_pointer = &source_db_pointers_;
        db_resource_name = cl.source_db_resource_name.c_str();
        break;
      case DbOrigin::kDestination:
        db_pointer = &destination_db_pointers_;
        db_resource_name = cl.destination_db_resource_name.c_str();
        break;
    }

    JobControlRecord* jcr = directordaemon::NewDirectorJcr();

    jcr->impl->res.catalog =
        (directordaemon::CatalogResource*)my_config->GetResWithName(
            directordaemon::R_CATALOG, db_resource_name);

    if (jcr->impl->res.catalog == nullptr) {
      std::string err{"Could not find catalog resource "};
      err += db_resource_name;
      err += ".";
      throw std::runtime_error(err);
    }

    BareosDb* db = directordaemon::GetDatabaseConnection(jcr);

    if (db == nullptr) {
      std::string err{"Could not open database "};
      err += db_resource_name;
      err += ".";
      throw std::runtime_error(err);
    }

    db_pointer->db_ = db;
    db_pointer->jcr_.reset(jcr);

    if (source_db_pointers_.jcr_ != nullptr &&
        destination_db_pointers_.jcr_ != nullptr &&
        source_db_pointers_.jcr_->impl->res.catalog ==
            destination_db_pointers_.jcr_->impl->res.catalog) {
      throw std::runtime_error(
          "Source and destination catalog resource are the same.");
    }
  }

  void ParseConfig()
  {
    my_config = InitDirConfig(cl.configfile.c_str(), M_ERROR_TERM);
    my_config_.reset(my_config);
    if (!my_config->ParseConfig()) {
      throw std::runtime_error("Error when loading config.");
    }
    directordaemon::me =
        (directordaemon::DirectorResource*)my_config->GetNextRes(
            directordaemon::R_DIRECTOR, nullptr);
    if (!directordaemon::me) {
      throw std::runtime_error("Could not find director resource.");
    }
    DbSetBackendDirs(std::move(directordaemon::me->backend_directories));
  }

  void SetWorkingDir()
  {
    if (getcwd(current_working_directory_.data(),
               current_working_directory_.size()) == nullptr) {
      throw std::runtime_error(
          "Could not determine current working directory.");
    }
    SetWorkingDirectory(current_working_directory_.data());
  }

  void DoMigration() {}

  class CommandLineParser {
    friend class DbTool;

   public:
    void ParseCommandLine(int argc, char** argv)
    {
      char c{};
      bool options_error{false};
      bool configfile_set{};

      while ((c = getopt(argc, argv, "c:?")) != -1 && !options_error) {
        switch (c) {
          case 'c':
            configfile = optarg;
            configfile_set = true;
            break;
          case '?':
          default:
            options_error = true;
            break;
        }
      }

      int argcount{configfile_set ? 5 : 3};

      if (options_error || argc != argcount) {
        usage();
        throw std::runtime_error(std::string());
      }
      source_db_resource_name = argv[3];
      destination_db_resource_name = argv[4];
    }

   private:
    std::string configfile{"/etc/bareos"};
    std::string source_db_resource_name, destination_db_resource_name;

    void usage() noexcept
    {
      kBareosVersionStrings.PrintCopyright(stderr, 2000);

      fprintf(stderr, _("Usage: dbtool [options] Source Destination\n"
                        "        -c <path>   use <path> as configuration file "
                        "or directory\n"
                        "        -?          print this message.\n"
                        "\n"));
    }
  };  // class CommandLineParser

  CommandLineParser cl;

 public:
  DbTool() = default;
  DbTool(const DbTool& other) = delete;
  DbTool(const DbTool&& other) = delete;
  DbTool& operator=(const DbTool& rhs) = delete;
  DbTool& operator=(const DbTool&& rhs) = delete;

  ~DbTool()
  {
    DbSqlPoolDestroy();
    DbFlushBackends();
  }

 private:
  std::unique_ptr<ConfigurationParser> my_config_;
  std::array<char, 1024> current_working_directory_;

  class DatabasePointers {
    friend class DbTool;
    BareosDb* db_{};
    std::unique_ptr<JobControlRecord> jcr_{};
  };

  DatabasePointers source_db_pointers_{};
  DatabasePointers destination_db_pointers_{};
};

int main(int argc, char** argv)
{
  DbTool db_tool;
  InitMsg(nullptr, nullptr);

  try {
    db_tool.SetWorkingDir();
    db_tool.cl.ParseCommandLine(argc, argv);
    db_tool.ParseConfig();
    db_tool.ConnectToDatabase(DbTool::DbOrigin::kSource);
    db_tool.ConnectToDatabase(DbTool::DbOrigin::kDestination);
    db_tool.DoMigration();
  } catch (const std::runtime_error& e) {
    std::string errstring{e.what()};
    if (!errstring.empty()) { std::cerr << e.what() << std::endl; }
    return 1;
  }
  return 0;
}
