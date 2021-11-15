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
#include "dird/dbcopy/database_connection.h"
#include "dird/dbcopy/database_export.h"
#include "dird/dbcopy/database_export_postgresql.h"
#include "dird/dbcopy/database_import.h"
#include "dird/dird_conf.h"
#include "dird/dird_globals.h"
#include "dird/get_database_connection.h"
#include "dird/jcr_private.h"
#include "dird/job.h"
#include "include/make_unique.h"
#include "lib/parse_conf.h"
#include "lib/util.h"

#include <array>
#include <iostream>

#if !defined HAVE_DYNAMIC_CATS_BACKENDS
#  error "NOT DEFINED: HAVE_DYNAMIC_CATS_BACKENDS"
#endif

namespace directordaemon {
bool DoReloadConfig() { return false; }
}  // namespace directordaemon

class DbCopy {
 public:
  explicit DbCopy(int argc, char** argv)
  {
    InitMsg(nullptr, nullptr);
    SetWorkingDir();
    cl.ParseCommandLine(argc, argv);

    ParseConfig();
    std::cout << "Copying tables from \"" << cl.source_db_resource_name
              << "\" to \"" << cl.destination_db_resource_name << "\""
              << std::endl;
    ConnectToDatabases();
  }

  void DoDatabaseCopy()
  {
    std::cout << "gathering information about source catalog \""
              << cl.source_db_resource_name << "\"..." << std::endl;
    std::unique_ptr<DatabaseImport> imp(
        DatabaseImport::Create(*source_db_, cl.maximum_number_of_rows));

    std::cout << "gathering information about destination catalog \""
              << cl.destination_db_resource_name << "\"..." << std::endl;
    std::unique_ptr<DatabaseExport> exp(
        DatabaseExport::Create(*destination_db_,
                               cl.use_sql_insert_statements_instead_of_copy
                                   ? DatabaseExport::InsertMode::kSqlInsert
                                   : DatabaseExport::InsertMode::kSqlCopy,
                               cl.empty_destination_tables));

    std::cout << "copying tables..." << std::endl;
    imp->ExportTo(*exp);
  }

 private:
  void SetWorkingDir()
  {
    if (getcwd(current_working_directory_.data(),
               current_working_directory_.size())
        == nullptr) {
      throw std::runtime_error(
          "Could not determine current working directory.");
    }
    SetWorkingDirectory(current_working_directory_.data());
  }

  void ParseConfig()
  {
    directordaemon::my_config
        = directordaemon::InitDirConfig(cl.configpath_.c_str(), M_ERROR_TERM);

    my_config_.reset(directordaemon::my_config);

    if (!directordaemon::my_config->ParseConfig()) {
      throw std::runtime_error("Error when loading config.");
    }

    directordaemon::me = dynamic_cast<directordaemon::DirectorResource*>(
        my_config->GetNextRes(directordaemon::R_DIRECTOR, nullptr));

    if (directordaemon::me == nullptr) {
      throw std::runtime_error("Could not find director resource.");
    }

    DbSetBackendDirs(directordaemon::me->backend_directories);
  }

  void ConnectToDatabases()
  {
    try {
      source_db_ = std::make_unique<DatabaseConnection>(
          cl.source_db_resource_name, directordaemon::my_config);

      destination_db_ = std::make_unique<DatabaseConnection>(
          cl.destination_db_resource_name, directordaemon::my_config);

      if (source_db_->db_type != DatabaseType::Enum::kMysql) {
        throw std::runtime_error("Error: Source database is not mysql");
      }

      if (destination_db_->db_type != DatabaseType::Enum::kPostgresql) {
        throw std::runtime_error(
            "Error: Destination database is not postgresql");
      }

    } catch (const std::runtime_error& e) {
      throw e;
    }
  }

  class CommandLineParser {
    friend class DbCopy;

   public:
    void ParseCommandLine(int argc, char** argv)
    {
      int c{};
      bool options_error{false};
      int argument_count{};

      while ((c = getopt(argc, argv, "ic:l:?")) != -1 && !options_error) {
        switch (c) {
          case 'c':
            configpath_ = optarg;
            argument_count += 2;
            break;
          case 'i':
            use_sql_insert_statements_instead_of_copy = true;
            argument_count += 1;
            break;
#if 0
          case 'd':
            empty_destination_tables = true;
            ++argument_count;
            break;
#endif
          case 'l':
            try {
              maximum_number_of_rows = std::stoul(optarg);
            } catch (...) {
              throw std::runtime_error("Wrong argument for 'l'");
            }
            argument_count += 2;
            break;
          case '?':
          default:
            options_error = true;
            break;
        }
      }

      ++argument_count;  // program name
      ++argument_count;  // source catalog name
      ++argument_count;  // destination catalog name

      if (options_error || argc != argument_count) {
        usage();
        throw std::runtime_error(std::string());
      }
      source_db_resource_name = argv[argument_count - 2];
      destination_db_resource_name = argv[argument_count - 1];
    }

   private:
    std::string configpath_{"/etc/bareos"};
    std::string source_db_resource_name, destination_db_resource_name;
    bool empty_destination_tables{false};
    bool use_sql_insert_statements_instead_of_copy{false};
    std::size_t maximum_number_of_rows{};
    static constexpr std::size_t year_of_release = 2020;

    static void usage() noexcept
    {
      kBareosVersionStrings.PrintCopyright(stderr, year_of_release);

      fprintf(
          stderr,
          _("Usage: bareos-dbcopy [options] Source Destination\n"
            "        -c <path>   use <path> as configuration file or "
            "directory\n"
            "        -i          use SQL INSERT statements instead of COPY\n"
            "        -?          print this message.\n"
            "\n"));
    }
  };  // class CommandLineParser

 public:
  ~DbCopy() = default;
  DbCopy(const DbCopy& other) = delete;
  DbCopy(const DbCopy&& other) = delete;
  DbCopy& operator=(const DbCopy& rhs) = delete;
  DbCopy& operator=(const DbCopy&& rhs) = delete;

 private:
  CommandLineParser cl;
  std::unique_ptr<DatabaseConnection> source_db_;
  std::unique_ptr<DatabaseConnection> destination_db_;
  std::unique_ptr<ConfigurationParser> my_config_;
  static constexpr std::size_t sizeof_workingdir = 10 * 1024;
  std::array<char, sizeof_workingdir> current_working_directory_{};
};

class Cleanup {
 public:
  Cleanup() = default;
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
    DbCopy dbcopy(argc, argv);
    dbcopy.DoDatabaseCopy();
  } catch (const std::runtime_error& e) {
    std::string errstring{e.what()};
    if (!errstring.empty()) {
      std::cerr << std::endl << std::endl << e.what() << std::endl;
    }
    return 1;
  }
  std::cout << "database copy completed successfully" << std::endl;
  return 0;
}
