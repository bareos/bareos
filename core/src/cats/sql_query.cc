/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2016-2016 Planets Communications B.V.
   Copyright (C) 2016-2017 Bareos GmbH & Co. KG

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
 * Bareos Catalog Database Query loading routines.
 *
 * Written by Marco van Wieringen, May 2016
 */

#include "bareos.h"

#if HAVE_SQLITE3 || HAVE_MYSQL || HAVE_POSTGRESQL || HAVE_INGRES || HAVE_DBI

#include "cats.h"

static const int dbglvl = 100;

const char *B_DB::get_predefined_query_name(B_DB::SQL_QUERY_ENUM query) {
  return query_names[query];
}

const char *B_DB::get_predefined_query(B_DB::SQL_QUERY_ENUM query) {
   if(!queries) {
      Emsg0(M_ERROR, 0, "No SQL queries defined. This should not happen.");
      return NULL;
   }

   return queries[query];
}

void B_DB::fill_query(B_DB::SQL_QUERY_ENUM predefined_query, ...)
{
   va_list arg_ptr;

   va_start(arg_ptr, predefined_query);
   fill_query_va_list(cmd, predefined_query, arg_ptr);
   va_end(arg_ptr);
}

void B_DB::fill_query(POOL_MEM &query, B_DB::SQL_QUERY_ENUM predefined_query, ...)
{
   va_list arg_ptr;

   va_start(arg_ptr, predefined_query);
   fill_query_va_list(query, predefined_query, arg_ptr);
   va_end(arg_ptr);
}

void B_DB::fill_query(POOLMEM *&query, B_DB::SQL_QUERY_ENUM predefined_query, ...)
{
   va_list arg_ptr;

   va_start(arg_ptr, predefined_query);
   fill_query_va_list(query, predefined_query, arg_ptr);
   va_end(arg_ptr);
}



void B_DB::fill_query_va_list(POOLMEM *&query, B_DB::SQL_QUERY_ENUM predefined_query, va_list arg_ptr)
{
   POOL_MEM query_tmp(PM_MESSAGE);

   fill_query_va_list(query_tmp, predefined_query, arg_ptr);
   pm_memcpy(query, query_tmp, query_tmp.strlen()+1);
}



void B_DB::fill_query_va_list(POOL_MEM &query, B_DB::SQL_QUERY_ENUM predefined_query, va_list arg_ptr)
{
   const char *query_name;
   const char *query_template;

   query_name = get_predefined_query_name(predefined_query);
   query_template = get_predefined_query(predefined_query);

   Dmsg3(dbglvl, "called: %s with query name %s (%d)\n", __PRETTY_FUNCTION__, query_name, predefined_query);

   if (query_template) {
      query.bvsprintf(query_template, arg_ptr);
   }

   Dmsg2(dbglvl, "called: %s query is now %s\n", __PRETTY_FUNCTION__, query.c_str());
}



bool B_DB::sql_query(B_DB::SQL_QUERY_ENUM predefined_query, ...)
{
   va_list arg_ptr;
   POOL_MEM query(PM_MESSAGE);

   va_start(arg_ptr, predefined_query);
   fill_query_va_list(query, predefined_query, arg_ptr);
   va_end(arg_ptr);

   return sql_query(query.c_str());
}


bool B_DB::sql_query(const char *query, int flags)
{
   bool retval;

   Dmsg2(dbglvl, "called: %s with query %s\n", __PRETTY_FUNCTION__, query);

   db_lock(this);
   retval = sql_query_without_handler(query, flags);
   if (!retval) {
      Mmsg(errmsg, _("Query failed: %s: ERR=%s\n"), query, sql_strerror());
   }
   db_unlock(this);

   return retval;
}

bool B_DB::sql_query(const char *query, DB_RESULT_HANDLER *result_handler, void *ctx)
{
   bool retval;

   Dmsg2(dbglvl, "called: %s with query %s\n", __PRETTY_FUNCTION__, query);

   db_lock(this);
   retval = sql_query_with_handler(query, result_handler, ctx);
   if (!retval) {
      Mmsg(errmsg, _("Query failed: %s: ERR=%s\n"), query, sql_strerror());
   }
   db_unlock(this);

   return retval;
}
#endif /* HAVE_SQLITE3 || HAVE_MYSQL || HAVE_POSTGRESQL || HAVE_INGRES || HAVE_DBI */
