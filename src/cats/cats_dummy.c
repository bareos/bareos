/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2010-2011 Free Software Foundation Europe e.V.

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
/*
 * Dummy bareos backend function replaced with the correct one at install time.
 */

#include "bareos.h"
#include "cats.h"

#ifndef HAVE_DYNAMIC_CATS_BACKENDS

B_DB *db_init_database(JCR *jcr,
                       const char *db_driver,
                       const char *db_name,
                       const char *db_user,
                       const char *db_password,
                       const char *db_address,
                       int db_port,
                       const char *db_socket,
                       bool mult_db_connections,
                       bool disable_batch_insert,
                       bool need_private)
{
   Jmsg(jcr, M_FATAL, 0, _("Please replace this dummy libbareoscats library with a proper one.\n"));
   Dmsg0(0, _("Please replace this dummy libbareoscats library with a proper one.\n"));
   return NULL;
}

void db_flush_backends(void)
{
}

#endif /* HAVE_DYNAMIC_CATS_BACKENDS */
