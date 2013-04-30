/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2010-2011 Free Software Foundation Europe e.V.

   The main author of Bacula is Kern Sibbald, with contributions from
   many others, a complete list can be found in the file AUTHORS.
   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation and included
   in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   General Public License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.

   Bacula® is a registered trademark of Kern Sibbald.
   The licensor of Bacula is the Free Software Foundation Europe
   (FSFE), Fiduciary Program, Sumatrastrasse 25, 8006 Zürich,
   Switzerland, email:ftf@fsfeurope.org.
*/
/*
 * Dummy bacula backend function replaced with the correct one at install time.
 */

#include "bacula.h"
#include "cats.h"

B_DB *db_init_database(JCR *jcr, const char *db_driver, const char *db_name, const char *db_user,
                       const char *db_password, const char *db_address, int db_port, const char *db_socket,
                       bool mult_db_connections, bool disable_batch_insert)
{
   Jmsg(jcr, M_FATAL, 0, _("Please replace this dummy libbaccats library with a proper one.\n"));

   return NULL;
}

