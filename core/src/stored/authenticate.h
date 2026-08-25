
/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2018-2026 Bareos GmbH & Co. KG

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
#ifndef BAREOS_STORED_AUTHENTICATE_H_
#define BAREOS_STORED_AUTHENTICATE_H_

#include "lib/bsock.h"
#include "include/jcr.h"
#include "lib/parse_conf.h"
#include "stored/stored_conf.h"

namespace storagedaemon {

struct Auth : ::TlsConfigProvider {
  Auth(std::shared_ptr<LoadedConfiguration> conf) : p{std::move(conf)} {}

  const TlsResource* get(global_resource::Type type,
                         std::string_view name) override;

  enum class inbound_type
  {
    Unknown,
    Director,
    Job,
  };

  struct director_data {
    DirectorResource* res{};
  };

  struct job_data {
    JobControlRecord* jcr{};
    TlsResource job{};

    ~job_data()
    {
      if (jcr) { FreeJcr(jcr); }
    }
  };

  inbound_type GetType() const { return type; }

  std::optional<director_data> director;
  std::optional<job_data> job;

 private:
  std::shared_ptr<LoadedConfiguration> p;
  inbound_type type{inbound_type::Unknown};
};


} /* namespace storagedaemon */

#endif  // BAREOS_STORED_AUTHENTICATE_H_
