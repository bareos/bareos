/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2016-2016 Planets Communications B.V.
   Copyright (C) 2016-2016 Bareos GmbH & Co. KG

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

/*
 * Lookup a query in the query_table and expand it.
 */
void B_DB::table_fill_query(const char* query_name, ...)
{
    Dmsg2(dbglvl, " called: %s with query name %s\n", __PRETTY_FUNCTION__, query_name);
    va_list arg_ptr;
    const char *query_text;
    int len, maxlen;

    try {
      query_text = m_query_table.at(query_name).c_str();
    } catch (const std::exception& e) {
      Dmsg2(dbglvl, "%s : query_name does not exist in map: %s\n", __PRETTY_FUNCTION__, query_name);
      return;
    }

    if (query_text) {
       for (;;) {
          maxlen = sizeof_pool_memory(cmd) - 1;
          va_start(arg_ptr, query_name);
          len = bvsnprintf(cmd, maxlen, query_text, arg_ptr);
          va_end(arg_ptr);
          if (len < 0 || len >= (maxlen - 5)) {
             cmd = realloc_pool_memory(cmd, maxlen + maxlen/2);
             continue;
          }
          break;
       }
    }
   Dmsg2(dbglvl, " called: %s query is now in cmd %s\n", __PRETTY_FUNCTION__, (const char *)cmd);
}

void B_DB::fill_query(POOLMEM *&query,  const char *query_name, ...)
{
   Dmsg2(dbglvl, " called: %s with query name %s\n", __PRETTY_FUNCTION__, query_name);
   va_list arg_ptr;
   const char *query_text;
   int len, maxlen;

   query_text = m_query_table.at(query_name).c_str();
   if (query_text) {
      for (;;) {
         maxlen = sizeof_pool_memory(query) - 1;
         va_start(arg_ptr, query_text);
         len = bvsnprintf(query, maxlen, query_text, arg_ptr);
         va_end(arg_ptr);
         if (len < 0 || len >= (maxlen - 5)) {
            query = realloc_pool_memory(query, maxlen + maxlen/2);
            continue;
         }
         break;
      }
   }
   Dmsg2(dbglvl, " called: %s query is now %s\n", __PRETTY_FUNCTION__, query);
}

void B_DB::fill_query(POOL_MEM &query, const char *query_name, ...)
{
   Dmsg2(dbglvl, " called: %s with query name %s\n", __PRETTY_FUNCTION__, query_name);
   va_list arg_ptr;
   const char *query_text;
   int len, maxlen;

   query_text = m_query_table.at(query_name).c_str();
   if (query_text) {
      for (;;) {
         maxlen = query.max_size() - 1;
         va_start(arg_ptr, query_text);
         len = bvsnprintf(query.c_str(), maxlen, query_text, arg_ptr);
         va_end(arg_ptr);
         if (len < 0 || len >= (maxlen - 5)) {
            query.realloc_pm(maxlen + maxlen/2);
            continue;
         }
         break;
      }
   }
   Dmsg2(dbglvl, " called: %s query is now %s\n", __PRETTY_FUNCTION__, query.c_str());
}

/*
 * Lookup a query in the query_table, expand it and execute it.
 */
bool B_DB::sql_table_query(const char *query_name, ...)
{
   Dmsg2(dbglvl, " called: %s with query name %s\n", __PRETTY_FUNCTION__, query_name);
   bool retval;
   va_list arg_ptr;
   const char *query_text;
   int len, maxlen;

   db_lock(this);

   query_text = m_query_table.at(query_name).c_str();

   if (query_text) {
      for (;;) {
         maxlen = sizeof_pool_memory(cmd) - 1;
         va_start(arg_ptr, query_name);
         len = bvsnprintf(cmd, maxlen, query_text, arg_ptr);
         va_end(arg_ptr);
         if (len < 0 || len >= (maxlen - 5)) {
            cmd = realloc_pool_memory(cmd, maxlen + maxlen/2);
            continue;
         }
         break;
      }

      retval = sql_query_without_handler(cmd);
      if (!retval) {
         Mmsg(errmsg, _("Query failed: %s: ERR=%s\n"), cmd, sql_strerror());
      }
   } else {
      Mmsg(errmsg, _("Query failed: failed to lookup query with name %s\n"), query_name);
      retval = false;
   }

   db_unlock(this);
   return retval;
}

bool B_DB::sql_query(const char *query, int flags)
{
   Dmsg2(dbglvl, " called: %s with query %s\n", __PRETTY_FUNCTION__, query);
   bool retval;

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
   Dmsg2(dbglvl, " called: %s with query %s\n", __PRETTY_FUNCTION__, query);
   bool retval;

   db_lock(this);
   retval = sql_query_with_handler(query, result_handler, ctx);
   if (!retval) {
      Mmsg(errmsg, _("Query failed: %s: ERR=%s\n"), query, sql_strerror());
   }
   db_unlock(this);

   return retval;
}
#endif /* HAVE_SQLITE3 || HAVE_MYSQL || HAVE_POSTGRESQL || HAVE_INGRES || HAVE_DBI */
