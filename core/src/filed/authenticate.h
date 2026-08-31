/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2010 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2026 Bareos GmbH & Co. KG

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

#ifndef BAREOS_FILED_AUTHENTICATE_H_
#define BAREOS_FILED_AUTHENTICATE_H_

#include "filed/filed_conf.h"
#include "lib/bsock.h"
#include "lib/parse_conf.h"
#include "include/jcr.h"

namespace filedaemon {

struct Auth : ::TlsConfigProvider {
  Auth(std::shared_ptr<LoadedConfiguration> conf) : p{std::move(conf)} {}

  const TlsResource* get(global_resource::Type type,
                         std::string_view name) override;

  enum class inbound_type
  {
    Unknown,
    Director,
    Storage,
  };

  struct director_data {
    DirectorResource* res{};
  };

  struct storage_data {
    JobControlRecord* jcr{};
    TlsResource job{};

    ~storage_data()
    {
      if (jcr) { FreeJcr(jcr); }
    }
  };

  inbound_type GetType() const { return type; }

  std::optional<director_data> director;
  std::optional<storage_data> storage;

 private:
  std::shared_ptr<LoadedConfiguration> p;
  inbound_type type{inbound_type::Unknown};
};

} /* namespace filedaemon */

#endif  // BAREOS_FILED_AUTHENTICATE_H_
