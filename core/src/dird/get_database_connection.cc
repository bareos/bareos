/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2012 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2016 Planets Communications B.V.
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

#include "include/bareos.h"
#include "cats/sql_pooling.h"
#include "dird/dird_conf.h"
#include "dird/get_database_connection.h"
#include "dird/jcr_private.h"
#include "include/jcr.h"

namespace directordaemon {

BareosDb* GetDatabaseConnection(JobControlRecord* jcr)
{
  return DbSqlGetPooledConnection(
      jcr, jcr->impl->res.catalog->db_driver, jcr->impl->res.catalog->db_name,
      jcr->impl->res.catalog->db_user,
      jcr->impl->res.catalog->db_password.value,
      jcr->impl->res.catalog->db_address, jcr->impl->res.catalog->db_port,
      jcr->impl->res.catalog->db_socket,
      jcr->impl->res.catalog->mult_db_connections,
      jcr->impl->res.catalog->disable_batch_insert,
      jcr->impl->res.catalog->try_reconnect,
      jcr->impl->res.catalog->exit_on_fatal);
}

}  // namespace directordaemon
