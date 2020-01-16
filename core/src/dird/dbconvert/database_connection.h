/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2012 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2016 Planets Communications B.V.
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

#ifndef BAREOS_SRC_DIRD_DBCONVERT_DATABASE_CONNECTION_H_
#define BAREOS_SRC_DIRD_DBCONVERT_DATABASE_CONNECTION_H_

#include "include/bareos.h"
#include "dird/dird_globals.h"
#include "lib/parse_conf.h"

class BareosDb;
class JobControlRecord;

using directordaemon::my_config;

class DatabaseConnection {
 public:
  BareosDb* db{};

  DatabaseConnection(const std::string& catalog_resource_name,
                     ConfigurationParser* config)
  {
    jcr_.reset(directordaemon::NewDirectorJcr());

    jcr_->impl->res.catalog =
        static_cast<directordaemon::CatalogResource*>(config->GetResWithName(
            directordaemon::R_CATALOG, catalog_resource_name.c_str()));

    if (jcr_->impl->res.catalog == nullptr) {
      std::string err{"Could not find catalog resource "};
      err += catalog_resource_name;
      err += ".";
      throw std::runtime_error(err);
    }

    db = directordaemon::GetDatabaseConnection(jcr_.get());

    if (db == nullptr) {
      std::string err{"Could not open database "};
      err += catalog_resource_name;
      err += ".";
      throw std::runtime_error(err);
    }
  }

 private:
  std::unique_ptr<JobControlRecord> jcr_{};
};

#endif  // BAREOS_SRC_DIRD_DBCONVERT_DATABASE_CONNECTIO_H_
