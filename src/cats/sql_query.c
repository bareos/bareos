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

static alist *query_dirs = NULL;

void db_set_query_dirs(alist *new_query_dirs)
{
   query_dirs = new_query_dirs;
}

/*
 * Quick and dirty strip routine which translates
 * human readable queries into machine readable.
 * e.g. remove all unneeded spaces and the trailing \n
 */
static inline void strip_query_line(char *cmd)
{
   char *bp, *sp = NULL;

   /*
    * Strip the trailing newline.
    */
   strip_trailing_newline(cmd);

   /*
    * Remove all leading space and
    * replace all multiple spaces by one and
    * replace TAB by SPACE.
    */
   bp = cmd;
   while  (*bp) {
      /*
       * See if this is a space.
       */
      if (B_ISSPACE(*bp)) {
         /*
          * Replace a TAB by a SPACE.
          */
         if (*bp == '\t') {
            *bp = ' ';
         }

         /*
          * Save the beginning of the space area.
          */
         if (sp == NULL) {
            sp = bp;
         }
         bp++;
         continue;
      }

      /*
       * If we find the first non space and there
       * is a space pointer set start processing.
       */
      if (sp) {
         /*
          * See if this is a leading space block.
          */
         if (sp == cmd) {
            bstrinlinecpy(cmd, bp);
            sp = NULL;
         } else if ((sp + 1) != bp) {
            /*
             * See if the start of the space block is 2 or more
             * characters back.
             */
            bstrinlinecpy(sp + 1, bp);
            sp = NULL;
         } else {
            /*
             * Just a single space clear the space pointer.
             */
            sp = NULL;
         }
      }
      bp++;
   }

   /*
    * See if there is a trailing space block.
    */
   if (sp) {
      *sp = '\0';
   }
}

static inline void insert_query_into_table(htable *query_table, uint32_t hash_key, char *query_text)
{
   int len;
   BDB_QUERY *query = NULL;

   len = strlen(query_text) + 1;

   /*
    * We allocate all members of the hash entry in the same memory chunk.
    */
   query = (BDB_QUERY *)query_table->hash_malloc(sizeof(BDB_QUERY) + len);
   query->query_text = (char *)query + sizeof(BDB_QUERY);

   strcpy(query->query_text, query_text);
   query_table->insert(hash_key, query);
}

BDB_QUERY_TABLE *load_query_table(const char *database_type)
{
   int errstat;
   int len = 0;
   int major,
       minor,
       release;
   char *query_dir;
   FILE *fp = NULL;
   uint32_t cnt = 0,
            hash_key = 0;
   char query_file[1024],
        buf[1024],
        version[10];
   bool start_seen = false,
        end_seen = false;
   POOLMEM *query_text;
   BDB_QUERY *query = NULL;
   BDB_QUERY_TABLE *query_table;

   /*
    * We need a list of query dirs to scan.
    */
   if (!query_dirs) {
      Emsg0(M_ERROR_TERM, 0, _("Query Dirs not configured.\n"));
   }

   foreach_alist(query_dir, query_dirs) {
      bsnprintf(query_file, sizeof(query_file), "%s/%s.bdbqf", query_dir, database_type);
      fp = fopen(query_file, "r");
      if (fp != NULL) {
         break;
      }
   }

   if (fp == NULL) {
      bsnprintf(query_file, sizeof(query_file), "%s.bdbqf", database_type);
      Emsg1(M_ABORT, 0, _("Unable to open backend queryfile %s\n"), query_file);
   }

   /*
    * Read the first line of the file (Magic)
    */
   fgets(buf, sizeof(buf), fp);
   strip_query_line(buf);
   if (!bstrcasecmp(buf, "(BareosDatabaseBackendQueryFile)")) {
      Emsg1(M_ABORT, 0, _("Illegal query file %s, doesn't start with (BareosDatabaseBackendQueryFile)\n"), query_file);
   }

   /*
    * Read the second line of the file (version) and make sure its the same
    * as our internal VERSION. We only read queryfiles that are compatible
    * with our internal version so we don't get nasty surprises.
    */
   fgets(buf, sizeof(buf), fp);
   strip_query_line(buf);
   if (!bstrncasecmp(buf, "(version", 8)) {
      Emsg1(M_ABORT, 0, _("Illegal query file %s, doesn't have valid (version indentifier)\n"), query_file);
      return NULL;
   }

   if (sscanf(buf, "(version %d.%d.%d)", &major, &minor, &release) != 3) {
      Emsg1(M_ABORT, 0, _("Illegal query file %s, doesn't have valid (version indentifier)\n"), query_file);
      return NULL;
   }

   bsnprintf(version, sizeof(version), "%d.%d.%d", major, minor, release);
   if (!bstrcmp(version, VERSION)) {
      Emsg3(M_ABORT, 0, _("Illegal query file %s, version %s not compatible with %s\n"), query_file, version, VERSION);
      return NULL;
   }

   /*
    * Create a new hash table to hold the queries.
    */
   query_table = (BDB_QUERY_TABLE *)malloc(sizeof(BDB_QUERY_TABLE));
   if ((errstat = rwl_init(&query_table->lock)) != 0) {
      berrno be;
      Emsg1(M_ABORT, 0, _("Unable to initialize DB querytable lock. ERR=%s\n"), be.bstrerror(errstat));
      free(query_table);
      return NULL;
   }

   query_table->table = (htable *)malloc(sizeof(htable));
   query_table->table->init(query, &query->link, QUERY_INITIAL_HASH_SIZE, QUERY_HTABLE_PAGES);

   query_text = get_pool_memory(PM_MESSAGE);
   while (fgets(buf, sizeof(buf), fp)) {
      strip_query_line(buf);
      /*
       * See if this is a comment line.
       */
      if (*buf == '#') {
         continue;
      }

      /*
       * Skip start tag.
       */
      if (bstrcasecmp(buf, "(start)")) {
         start_seen = true;
         continue;
      }

      /*
       * While we have not seen a start tag skip all data.
       */
      if (!start_seen) {
         continue;
      }

      /*
       * Stop after reading end tag.
       */
      if (bstrcasecmp(buf, "(end)")) {
         /*
          * Flush the current entry.
          */
         if (hash_key) {
            insert_query_into_table(query_table->table, hash_key, query_text);
         }
         pm_strcpy(query_text, "");
         len = 0;
         end_seen = true;
         break;
      }

      /*
       * See if this is hash key.
       */
      if (strlen(buf) >= 2) {
         if (*buf == '(' && B_ISDIGIT(*(buf + 1))) {
            /*
             * Flush the current entry.
             */
            if (hash_key) {
               insert_query_into_table(query_table->table, hash_key, query_text);
            }
            pm_strcpy(query_text, "");
            len = 0;

            /*
             * Store the next hash_key.
             */
            if (sscanf(buf, "(%d)", &hash_key) != 1) {
               Emsg2(M_ABORT, 0, _("Illegal query file %s, unable to parse hash tag %s\n"), query_file, buf);
               goto bail_out;
            }
            cnt++;
            continue;
         }
      }

      /*
       * Append data to the query_text buffer.
       */
      if (len > 0) {
         len += pm_strcat(query_text, " ");
      }
      len += pm_strcat(query_text, buf);
   }

   free_pool_memory(query_text);
   fclose(fp);

   if (!end_seen) {
      Emsg1(M_ABORT, 0, _("Illegal query file %s, No (end) tag found\n"), query_file);
      goto bail_out;
   }

   Dmsg2(500, "load_query_table: loaded %d queries from %s\n", cnt, query_file);

   return query_table;

bail_out:
   unload_query_table(query_table);
   return NULL;
}

void unload_query_table(BDB_QUERY_TABLE *query_table)
{
   query_table->table->destroy();
   free(query_table->table);
   rwl_destroy(&query_table->lock);
   free(query_table);
}

/*
 * Lookup a query in the query_table use shortcut logic when same query is
 * being looked up by using cache result.
 */
char *B_DB::lookup_query(uint32_t hash_key)
{
   int errstat;
   char *query_text = NULL;
   BDB_QUERY *query = NULL;

   db_lock(this);

   /*
    * See if we can shortcut using our previous lookup result.
    */
   if (m_last_hash_key == hash_key && m_last_query_text) {
      query_text = m_last_query_text;
   } else {
      /*
       * Lock the hash table while we do a lookup.
       */
      if ((errstat = rwl_writelock_p(&m_query_table->lock, __FILE__, __LINE__)) != 0) {
         berrno be;
         e_msg(__FILE__, __LINE__, M_FATAL, 0, "rwl_writelock failure. stat=%d: ERR=%s\n",
               errstat, be.bstrerror(errstat));
      }

      query = (BDB_QUERY *)m_query_table->table->lookup(hash_key);
      if (query) {
         m_last_hash_key = hash_key;
         query_text = m_last_query_text = query->query_text;
      }

      if ((errstat = rwl_writeunlock(&m_query_table->lock)) != 0) {
         berrno be;
         e_msg(__FILE__, __LINE__, M_FATAL, 0, "rwl_writeunlock failure. stat=%d: ERR=%s\n",
               errstat, be.bstrerror(errstat));
      }
   }

   db_unlock(this);

   /*
    * This should never happen abort if it does as it means the code
    * references a query that doesn't exist in the query table. Only
    * bad things can happen from continuing.
    */
   if (!query_text) {
      Emsg1(M_ABORT, 0, _("Illegal query referenced %ld not in query_table\n"), hash_key);
   }

   return query_text;
}

/*
 * Lookup a query in the query_table and expand it.
 */
void B_DB::fill_query(uint32_t hash_key, ...)
{
   va_list arg_ptr;
   char *query_text;
   int len, maxlen;

   query_text = lookup_query(hash_key);
   if (query_text) {
      for (;;) {
         maxlen = sizeof_pool_memory(cmd) - 1;
         va_start(arg_ptr, hash_key);
         len = bvsnprintf(cmd, maxlen, query_text, arg_ptr);
         va_end(arg_ptr);
         if (len < 0 || len >= (maxlen - 5)) {
            cmd = realloc_pool_memory(cmd, maxlen + maxlen/2);
            continue;
         }
         break;
      }
   }
}

void B_DB::fill_query(POOLMEM *&query, uint32_t hash_key, ...)
{
   va_list arg_ptr;
   char *query_text;
   int len, maxlen;

   query_text = lookup_query(hash_key);
   if (query_text) {
      for (;;) {
         maxlen = sizeof_pool_memory(query) - 1;
         va_start(arg_ptr, hash_key);
         len = bvsnprintf(query, maxlen, query_text, arg_ptr);
         va_end(arg_ptr);
         if (len < 0 || len >= (maxlen - 5)) {
            query = realloc_pool_memory(query, maxlen + maxlen/2);
            continue;
         }
         break;
      }
   }
}

void B_DB::fill_query(POOL_MEM &query, uint32_t hash_key, ...)
{
   va_list arg_ptr;
   char *query_text;
   int len, maxlen;

   query_text = lookup_query(hash_key);
   if (query_text) {
      for (;;) {
         maxlen = query.max_size() - 1;
         va_start(arg_ptr, hash_key);
         len = bvsnprintf(query.c_str(), maxlen, query_text, arg_ptr);
         va_end(arg_ptr);
         if (len < 0 || len >= (maxlen - 5)) {
            query.realloc_pm(maxlen + maxlen/2);
            continue;
         }
         break;
      }
   }
}

/*
 * Lookup a query in the query_table, expand it and execute it.
 */
bool B_DB::sql_query(uint32_t hash_key, ...)
{
   bool retval;
   va_list arg_ptr;
   char *query_text;
   int len, maxlen;

   db_lock(this);

   query_text = lookup_query(hash_key);
   if (query_text) {
      for (;;) {
         maxlen = sizeof_pool_memory(cmd) - 1;
         va_start(arg_ptr, hash_key);
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
      Mmsg(errmsg, _("Query failed: failed to lookup query with hash_key %d\n"), hash_key);
      retval = false;
   }

   db_unlock(this);
   return retval;
}

bool B_DB::sql_query(const char *query, int flags)
{
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
