/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2012 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2016 Planets Communications B.V.
   Copyright (C) 2019-2022 Bareos GmbH & Co. KG

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
#include "cats/cats.h"
#include "dird/dird_conf.h"
#include "dird/get_database_connection.h"
#include "dird/director_jcr_impl.h"
#include "include/jcr.h"

namespace directordaemon {

BareosDb* GetDatabaseConnection(JobControlRecord* jcr)
{
  return ConnectDatabase(jcr, jcr->dir_impl->res.catalog->db_driver,
                         jcr->dir_impl->res.catalog->db_name,
                         jcr->dir_impl->res.catalog->db_user,
                         jcr->dir_impl->res.catalog->db_password.value,
                         jcr->dir_impl->res.catalog->db_address,
                         jcr->dir_impl->res.catalog->db_port,
                         jcr->dir_impl->res.catalog->db_socket,
                         jcr->dir_impl->res.catalog->mult_db_connections,
                         jcr->dir_impl->res.catalog->disable_batch_insert,
                         jcr->dir_impl->res.catalog->try_reconnect,
                         jcr->dir_impl->res.catalog->exit_on_fatal);
}

}  // namespace directordaemon
