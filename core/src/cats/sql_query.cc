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

#include "include/bareos.h"

#if HAVE_SQLITE3 || HAVE_MYSQL || HAVE_POSTGRESQL || HAVE_INGRES || HAVE_DBI

#include "cats.h"

static const int debuglevel = 100;

const char *BareosDb::get_predefined_query_name(BareosDb::SQL_QUERY_ENUM query) {
  return query_names[query];
}

const char *BareosDb::get_predefined_query(BareosDb::SQL_QUERY_ENUM query) {
   if(!queries) {
      Emsg0(M_ERROR, 0, "No SQL queries defined. This should not happen.");
      return NULL;
   }

   return queries[query];
}

void BareosDb::FillQuery(BareosDb::SQL_QUERY_ENUM predefined_query, ...)
{
   va_list arg_ptr;

   va_start(arg_ptr, predefined_query);
   FillQueryVaList(cmd, predefined_query, arg_ptr);
   va_end(arg_ptr);
}

void BareosDb::FillQuery(PoolMem &query, BareosDb::SQL_QUERY_ENUM predefined_query, ...)
{
   va_list arg_ptr;

   va_start(arg_ptr, predefined_query);
   FillQueryVaList(query, predefined_query, arg_ptr);
   va_end(arg_ptr);
}

void BareosDb::FillQuery(POOLMEM *&query, BareosDb::SQL_QUERY_ENUM predefined_query, ...)
{
   va_list arg_ptr;

   va_start(arg_ptr, predefined_query);
   FillQueryVaList(query, predefined_query, arg_ptr);
   va_end(arg_ptr);
}



void BareosDb::FillQueryVaList(POOLMEM *&query, BareosDb::SQL_QUERY_ENUM predefined_query, va_list arg_ptr)
{
   PoolMem query_tmp(PM_MESSAGE);

   FillQueryVaList(query_tmp, predefined_query, arg_ptr);
   PmMemcpy(query, query_tmp, query_tmp.strlen()+1);
}



void BareosDb::FillQueryVaList(PoolMem &query, BareosDb::SQL_QUERY_ENUM predefined_query, va_list arg_ptr)
{
   const char *query_name;
   const char *query_template;

   query_name = get_predefined_query_name(predefined_query);
   query_template = get_predefined_query(predefined_query);

   Dmsg3(debuglevel, "called: %s with query name %s (%d)\n", __PRETTY_FUNCTION__, query_name, predefined_query);

   if (query_template) {
      query.Bvsprintf(query_template, arg_ptr);
   }

   Dmsg2(debuglevel, "called: %s query is now %s\n", __PRETTY_FUNCTION__, query.c_str());
}



bool BareosDb::SqlQuery(BareosDb::SQL_QUERY_ENUM predefined_query, ...)
{
   va_list arg_ptr;
   PoolMem query(PM_MESSAGE);

   va_start(arg_ptr, predefined_query);
   FillQueryVaList(query, predefined_query, arg_ptr);
   va_end(arg_ptr);

   return SqlQuery(query.c_str());
}


bool BareosDb::SqlQuery(const char *query, int flags)
{
   bool retval;

   Dmsg2(debuglevel, "called: %s with query %s\n", __PRETTY_FUNCTION__, query);

   DbLock(this);
   retval = SqlQueryWithoutHandler(query, flags);
   if (!retval) {
      Mmsg(errmsg, _("Query failed: %s: ERR=%s\n"), query, sql_strerror());
   }
   DbUnlock(this);

   return retval;
}

bool BareosDb::SqlQuery(const char *query, DB_RESULT_HANDLER *ResultHandler, void *ctx)
{
   bool retval;

   Dmsg2(debuglevel, "called: %s with query %s\n", __PRETTY_FUNCTION__, query);

   DbLock(this);
   retval = SqlQueryWithHandler(query, ResultHandler, ctx);
   if (!retval) {
      Mmsg(errmsg, _("Query failed: %s: ERR=%s\n"), query, sql_strerror());
   }
   DbUnlock(this);

   return retval;
}
#endif /* HAVE_SQLITE3 || HAVE_MYSQL || HAVE_POSTGRESQL || HAVE_INGRES || HAVE_DBI */
