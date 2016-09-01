/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2009 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2016 Planets Communications B.V.
   Copyright (C) 2013-2016 Bareos GmbH & Co. KG

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
 * BAREOS Catalog Database interface routines
 *
 * Almost generic set of SQL database interface routines
 * (with a little more work) SQL engine specific routines are in
 * mysql.c, postgresql.c, sqlite.c, ...
 *
 * Kern Sibbald, March 2000
 */

#include "bareos.h"

#if HAVE_SQLITE3 || HAVE_MYSQL || HAVE_POSTGRESQL || HAVE_INGRES || HAVE_DBI

#include "cats.h"

/* Forward referenced subroutines */

dbid_list::dbid_list()
{
   memset(this, 0, sizeof(dbid_list));
   max_ids = 1000;
   DBId = (DBId_t *)malloc(max_ids * sizeof(DBId_t));
   num_ids = num_seen = tot_ids = 0;
   PurgedFiles = NULL;
}

dbid_list::~dbid_list()
{
   free(DBId);
}

/*
 * Called here to retrieve an integer from the database
 */
int db_int_handler(void *ctx, int num_fields, char **row)
{
   uint32_t *val = (uint32_t *)ctx;

   Dmsg1(800, "int_handler starts with row pointing at %x\n", row);

   if (row[0]) {
      Dmsg1(800, "int_handler finds '%s'\n", row[0]);
      *val = str_to_int64(row[0]);
   } else {
      Dmsg0(800, "int_handler finds zero\n");
      *val = 0;
   }
   Dmsg0(800, "int_handler finishes\n");
   return 0;
}

/*
 * Called here to retrieve a 32/64 bit integer from the database.
 *   The returned integer will be extended to 64 bit.
 */
int db_int64_handler(void *ctx, int num_fields, char **row)
{
   db_int64_ctx *lctx = (db_int64_ctx *)ctx;

   if (row[0]) {
      lctx->value = str_to_int64(row[0]);
      lctx->count++;
   }
   return 0;
}

/*
 * Called here to retrieve a btime from the database.
 *   The returned integer will be extended to 64 bit.
 */
int db_strtime_handler(void *ctx, int num_fields, char **row)
{
   db_int64_ctx *lctx = (db_int64_ctx *)ctx;

   if (row[0]) {
      lctx->value = str_to_utime(row[0]);
      lctx->count++;
   }
   return 0;
}

/*
 * Use to build a comma separated list of values from a query. "10,20,30"
 */
int db_list_handler(void *ctx, int num_fields, char **row)
{
   db_list_ctx *lctx = (db_list_ctx *)ctx;
   if (num_fields == 1 && row[0]) {
      lctx->add(row[0]);
   }
   return 0;
}

/*
 * specific context passed from db_check_max_connections to db_max_connections_handler.
 */
struct max_connections_context {
   B_DB *db;
   uint32_t nr_connections;
};

/*
 * Called here to retrieve an integer from the database
 */
static inline int db_max_connections_handler(void *ctx, int num_fields, char **row)
{
   struct max_connections_context *context;
   uint32_t index;

   context = (struct max_connections_context *)ctx;
   switch (context->db->get_type_index()) {
   case SQL_TYPE_MYSQL:
      index = 1;
      break;
   default:
      index = 0;
      break;
   }

   if (row[index]) {
      context->nr_connections = str_to_int64(row[index]);
   } else {
      Dmsg0(800, "int_handler finds zero\n");
      context->nr_connections = 0;
   }
   return 0;
}

/*
 * Check catalog max_connections setting
 */
bool B_DB::check_max_connections(JCR *jcr, uint32_t max_concurrent_jobs)
{
   POOL_MEM query(PM_MESSAGE);
   struct max_connections_context context;

   /*
    * Without Batch insert, no need to verify max_connections
    */
   if (!batch_insert_available())
      return true;

   context.db = this;
   context.nr_connections = 0;

   /*
    * Check max_connections setting
    */
   fill_query(query, 46);
   if (!sql_query_with_handler(query.c_str(), db_max_connections_handler, &context)) {
      Jmsg(jcr, M_ERROR, 0, "Can't verify max_connections settings %s", errmsg);
      return false;
   }

   if (context.nr_connections && max_concurrent_jobs && max_concurrent_jobs > context.nr_connections) {
      Mmsg(errmsg,
           _("Potential performance problem:\n"
             "max_connections=%d set for %s database \"%s\" should be larger than Director's "
             "MaxConcurrentJobs=%d\n"),
           context.nr_connections, get_type(), get_db_name(), max_concurrent_jobs);
      Jmsg(jcr, M_WARNING, 0, "%s", errmsg);
      return false;
   }

   return true;
}

/* NOTE!!! The following routines expect that the
 *  calling subroutine sets and clears the mutex
 */

/*
 * Check that the tables correspond to the version we want
 */
bool B_DB::check_tables_version(JCR *jcr)
{
   uint32_t bareos_db_version = 0;
   const char *query = "SELECT VersionId FROM Version";

   if (!sql_query_with_handler(query, db_int_handler, (void *)&bareos_db_version)) {
      Jmsg(jcr, M_FATAL, 0, "%s", errmsg);
      return false;
   }

   if (bareos_db_version != BDB_VERSION) {
      Mmsg(errmsg, "Version error for database \"%s\". Wanted %d, got %d\n",
          get_db_name(), BDB_VERSION, bareos_db_version);
      Jmsg(jcr, M_FATAL, 0, "%s", errmsg);
      return false;
   }

   return true;
}

/*
 * Utility routine for queries. The database MUST be locked before calling here.
 * Returns: false on failure
 *          true on success
 */
bool B_DB::QueryDB(const char *file, int line, JCR *jcr, const char *select_cmd)
{
   sql_free_result();
   Dmsg1(1000, "query: %s\n", select_cmd);
   if (!sql_query(select_cmd, QF_STORE_RESULT)) {
      m_msg(file, line, errmsg, _("query %s failed:\n%s\n"), select_cmd, sql_strerror());
      j_msg(file, line, jcr, M_FATAL, 0, "%s", errmsg);
      if (verbose) {
         j_msg(file, line, jcr, M_INFO, 0, "%s\n", select_cmd);
      }
      return false;
   }

   return true;
}

/*
 * Utility routine to do inserts
 * Returns: false on failure
 *          true on success
 */
bool B_DB::InsertDB(const char *file, int line, JCR *jcr, const char *select_cmd)
{
   int num_rows;

   if (!sql_query(select_cmd)) {
      m_msg(file, line, errmsg,  _("insert %s failed:\n%s\n"), select_cmd, sql_strerror());
      j_msg(file, line, jcr, M_FATAL, 0, "%s", errmsg);
      if (verbose) {
         j_msg(file, line, jcr, M_INFO, 0, "%s\n", select_cmd);
      }
      return false;
   }
   num_rows = sql_affected_rows();
   if (num_rows != 1) {
      char ed1[30];
      m_msg(file, line, errmsg, _("Insertion problem: affected_rows=%s\n"),
         edit_uint64(num_rows, ed1));
      if (verbose) {
         j_msg(file, line, jcr, M_INFO, 0, "%s\n", select_cmd);
      }
      return false;
   }
   changes++;
   return true;
}

/*
 * Utility routine for updates.
 * Returns: false on failure
 *          true on success
 */
bool B_DB::UpdateDB(const char *file, int line, JCR *jcr, const char *update_cmd, int nr_afr)
{
   int num_rows;

   if (!sql_query(update_cmd)) {
      m_msg(file, line, errmsg, _("update %s failed:\n%s\n"), update_cmd, sql_strerror());
      j_msg(file, line, jcr, M_ERROR, 0, "%s", errmsg);
      if (verbose) {
         j_msg(file, line, jcr, M_INFO, 0, "%s\n", update_cmd);
      }
      return false;
   }

   if (nr_afr > 0) {
      num_rows = sql_affected_rows();
      if (num_rows < nr_afr) {
         char ed1[30];
         m_msg(file, line, errmsg, _("Update failed: affected_rows=%s for %s\n"),
               edit_uint64(num_rows, ed1), update_cmd);
         if (verbose) {
//          j_msg(file, line, jcr, M_INFO, 0, "%s\n", update_cmd);
         }
         return false;
      }
   }

   changes++;
   return true;
}

/*
 * Utility routine for deletes
 *
 * Returns: -1 on error
 *           n number of rows affected
 */
int B_DB::DeleteDB(const char *file, int line, JCR *jcr, const char *delete_cmd)
{

   if (!sql_query(delete_cmd)) {
      m_msg(file, line, errmsg, _("delete %s failed:\n%s\n"), delete_cmd, sql_strerror());
      j_msg(file, line, jcr, M_ERROR, 0, "%s", errmsg);
      if (verbose) {
         j_msg(file, line, jcr, M_INFO, 0, "%s\n", delete_cmd);
      }
      return -1;
   }
   changes++;
   return sql_affected_rows();
}

/*
 * Get record max. Query is already in mdb->cmd
 *  No locking done
 *
 * Returns: -1 on failure
 *          count on success
 */
int B_DB::get_sql_record_max(JCR *jcr)
{
   SQL_ROW row;
   int retval = 0;

   if (QUERY_DB(jcr, cmd)) {
      if ((row = sql_fetch_row()) == NULL) {
         Mmsg1(errmsg, _("error fetching row: %s\n"), sql_strerror());
         retval = -1;
      } else {
         retval = str_to_int64(row[0]);
      }
      sql_free_result();
   } else {
      Mmsg1(errmsg, _("error fetching row: %s\n"), sql_strerror());
      retval = -1;
   }
   return retval;
}

/*
 * Return pre-edited error message
 */
char *B_DB::strerror()
{
   return errmsg;
}

/*
 * Given a full filename, split it into its path
 *  and filename parts. They are returned in pool memory
 *  in the mdb structure.
 */
void B_DB::split_path_and_file(JCR *jcr, const char *filename)
{
   const char *p, *f;

   /* Find path without the filename.
    * I.e. everything after the last / is a "filename".
    * OK, maybe it is a directory name, but we treat it like
    * a filename. If we don't find a / then the whole name
    * must be a path name (e.g. c:).
    */
   for (p=f=filename; *p; p++) {
      if (IsPathSeparator(*p)) {
         f = p;                       /* set pos of last slash */
      }
   }
   if (IsPathSeparator(*f)) {         /* did we find a slash? */
      f++;                            /* yes, point to filename */
   } else {
      f = p;                          /* no, whole thing must be path name */
   }

   /* If filename doesn't exist (i.e. root directory), we
    * simply create a blank name consisting of a single
    * space. This makes handling zero length filenames
    * easier.
    */
   fnl = p - f;
   if (fnl > 0) {
      fname = check_pool_memory_size(fname, fnl + 1);
      memcpy(fname, f, fnl);    /* copy filename */
      fname[fnl] = 0;
   } else {
      fname[0] = 0;
      fnl = 0;
   }

   pnl = f - filename;
   if (pnl > 0) {
      path = check_pool_memory_size(path, pnl + 1);
      memcpy(path, filename, pnl);
      path[pnl] = 0;
   } else {
      Mmsg1(errmsg, _("Path length is zero. File=%s\n"), fname);
      Jmsg(jcr, M_ERROR, 0, "%s", errmsg);
      path[0] = 0;
      pnl = 0;
   }

   Dmsg2(500, "split path=%s file=%s\n", path, fname);
}

/*
 * Set maximum field length to something reasonable
 */
static int max_length(int max_length)
{
   int max_len = max_length;
   /* Sanity check */
   if (max_len < 0) {
      max_len = 2;
   } else if (max_len > 100) {
      max_len = 100;
   }
   return max_len;
}

/*
 * List dashes as part of header for listing SQL results in a table
 */
void B_DB::list_dashes(OUTPUT_FORMATTER *send)
{
   int len;
   int num_fields;
   SQL_FIELD *field;

   sql_field_seek(0);
   send->decoration("+");
   num_fields = sql_num_fields();
   for (int i = 0; i < num_fields; i++) {
      field = sql_fetch_field();
      if (!field) {
         break;
      }
      len = max_length(field->max_length + 2);
      for (int j = 0; j < len; j++) {
         send->decoration("-");
      }
      send->decoration("+");
   }
   send->decoration("\n");
}

/*
 * List result handler used by queries done with db_big_sql_query()
 */
int B_DB::list_result(void *vctx, int nb_col, char **row)
{
   JCR *jcr;
   char ewc[30];
   POOL_MEM key;
   POOL_MEM value;
   int num_fields;
   SQL_FIELD *field;
   e_list_type type;
   OUTPUT_FORMATTER *send;
   int col_len, max_len = 0;
   LIST_CTX *pctx = (LIST_CTX *)vctx;

   /*
    * Get pointers from context.
    */
   type = pctx->type;
   send = pctx->send;
   jcr = pctx->jcr;

   /*
    * See if this row must be filtered.
    */
   if (send->has_filters() && !send->filter_data(row)) {
      return 0;
   }

   send->object_start();

   num_fields = sql_num_fields();
   switch (type) {
   case NF_LIST:
   case RAW_LIST:
      /*
       * No need to calculate things like maximum field lenght for
       * unformated or raw output.
       */
      break;
   case HORZ_LIST:
   case VERT_LIST:
      if (!pctx->once) {
         pctx->once = true;

         Dmsg1(800, "list_result starts looking at %d fields\n", num_fields);
         /*
          * Determine column display widths
          */
         sql_field_seek(0);
         for (int i = 0; i < num_fields; i++) {
            Dmsg1(800, "list_result processing field %d\n", i);
            field = sql_fetch_field();
            if (!field) {
               break;
            }

            /*
             * See if this is a hidden column.
             */
            if (send->is_hidden_column(i)) {
               Dmsg1(800, "list_result field %d is hidden\n", i);
               continue;
            }

            col_len = cstrlen(field->name);
            if (type == VERT_LIST) {
               if (col_len > max_len) {
                  max_len = col_len;
               }
            } else {
               if (sql_field_is_numeric(field->type) && (int)field->max_length > 0) { /* fixup for commas */
                  field->max_length += (field->max_length - 1) / 3;
               }
               if (col_len < (int)field->max_length) {
                  col_len = field->max_length;
               }
               if (col_len < 4 && !sql_field_is_not_null(field->flags)) {
                  col_len = 4;                 /* 4 = length of the word "NULL" */
               }
               field->max_length = col_len;    /* reset column info */
            }
         }

         pctx->num_rows++;

         Dmsg0(800, "list_result finished first loop\n");
         if (type == VERT_LIST) {
            break;
         }

         Dmsg1(800, "list_result starts second loop looking at %d fields\n", num_fields);

         /*
          * Keep the result to display the same line at the end of the table
          */
         list_dashes(send);

         send->decoration("|");
         sql_field_seek(0);
         for (int i = 0; i < num_fields; i++) {
            Dmsg1(800, "list_result looking at field %d\n", i);

            field = sql_fetch_field();
            if (!field) {
               break;
            }

            /*
             * See if this is a hidden column.
             */
            if (send->is_hidden_column(i)) {
               Dmsg1(800, "list_result field %d is hidden\n", i);
               continue;
            }

            max_len = max_length(field->max_length);
            send->decoration(" %-*s |", max_len, field->name);
         }
         send->decoration("\n");
         list_dashes(send);
      }
      break;
   default:
      break;
   }

   switch (type) {
   case NF_LIST:
   case RAW_LIST:
      Dmsg1(800, "list_result starts third loop looking at %d fields\n", num_fields);
      sql_field_seek(0);
      for (int i = 0; i < num_fields; i++) {
         field = sql_fetch_field();
         if (!field) {
            break;
         }

         /*
          * See if this is a hidden column.
          */
         if (send->is_hidden_column(i)) {
            Dmsg1(800, "list_result field %d is hidden\n", i);
            continue;
         }

         if (row[i] == NULL) {
            value.bsprintf("%s", "NULL");
         } else {
            value.bsprintf("%s", row[i]);
         }
         send->object_key_value(field->name, value.c_str(), " %s");
      }
      if (type != RAW_LIST) {
         send->decoration("\n");
      }
      break;
   case HORZ_LIST:
      Dmsg1(800, "list_result starts third loop looking at %d fields\n", num_fields);
      sql_field_seek(0);
      send->decoration("|");
      for (int i = 0; i < num_fields; i++) {
         field = sql_fetch_field();
         if (!field) {
            break;
         }

         /*
          * See if this is a hidden column.
          */
         if (send->is_hidden_column(i)) {
            Dmsg1(800, "list_result field %d is hidden\n", i);
            continue;
         }

         max_len = max_length(field->max_length);
         if (row[i] == NULL) {
            value.bsprintf(" %-*s |", max_len, "NULL");
         } else if (sql_field_is_numeric(field->type) && !jcr->gui && is_an_integer(row[i])) {
            value.bsprintf(" %*s |", max_len, add_commas(row[i], ewc));
         } else {
            value.bsprintf(" %-*s |", max_len, row[i]);
         }

         /*
          * Use value format string to send preformated value.
          */
         send->object_key_value(field->name, row[i], value.c_str());
      }
      send->decoration("\n");
      break;
   case VERT_LIST:
      Dmsg1(800, "list_result starts vertical list at %d fields\n", num_fields);
      sql_field_seek(0);
      for (int i = 0; i < num_fields; i++) {
         field = sql_fetch_field();
         if (!field) {
            break;
         }

         /*
          * See if this is a hidden column.
          */
         if (send->is_hidden_column(i)) {
            Dmsg1(800, "list_result field %d is hidden\n", i);
            continue;
         }

         if (row[i] == NULL) {
            key.bsprintf(" %*s: ", max_len, field->name);
            value.bsprintf("%s\n", "NULL");
         } else if (sql_field_is_numeric(field->type) && !jcr->gui && is_an_integer(row[i])) {
            key.bsprintf(" %*s: ", max_len, field->name);
            value.bsprintf("%s\n", add_commas(row[i], ewc));
         } else {
            key.bsprintf(" %*s: ", max_len, field->name);
            value.bsprintf("%s\n", row[i]);
         }

         /*
          * Use value format string to send preformated value.
          */
         send->object_key_value(field->name, key.c_str(), row[i], value.c_str());
      }
      send->decoration("\n");
      break;
   default:
      break;
   }
   send->object_end();

   return 0;
}

int list_result(void *vctx, int nb_col, char **row)
{
   LIST_CTX *pctx = (LIST_CTX *)vctx;
   B_DB *mdb = pctx->mdb;

   return mdb->list_result(vctx, nb_col, row);
}

/*
 * If full_list is set, we list vertically, otherwise, we list on one line horizontally.
 *
 * Return number of rows
 */
int B_DB::list_result(JCR *jcr, OUTPUT_FORMATTER *send, e_list_type type)
{
   SQL_ROW row;
   char ewc[30];
   POOL_MEM key;
   POOL_MEM value;
   int num_fields;
   SQL_FIELD *field;
   bool filters_enabled;
   int col_len, max_len = 0;

   Dmsg0(800, "list_result starts\n");
   if (sql_num_rows() == 0) {
      send->decoration(_("No results to list.\n"));
      return 0;
   }

   num_fields = sql_num_fields();
   switch (type) {
   case NF_LIST:
   case RAW_LIST:
      /*
       * No need to calculate things like column widths for unformated or raw output.
       */
      break;
   case HORZ_LIST:
   case VERT_LIST:
      Dmsg1(800, "list_result starts looking at %d fields\n", num_fields);
      /*
       * Determine column display widths
       */
      sql_field_seek(0);
      for (int i = 0; i < num_fields; i++) {
         Dmsg1(800, "list_result processing field %d\n", i);

         field = sql_fetch_field();
         if (!field) {
            break;
         }

         /*
          * See if this is a hidden column.
          */
         if (send->is_hidden_column(i)) {
            Dmsg1(800, "list_result field %d is hidden\n", i);
            continue;
         }

         col_len = cstrlen(field->name);
         if (type == VERT_LIST) {
            if (col_len > max_len) {
               max_len = col_len;
            }
         } else {
            if (sql_field_is_numeric(field->type) && (int)field->max_length > 0) { /* fixup for commas */
               field->max_length += (field->max_length - 1) / 3;
            }
            if (col_len < (int)field->max_length) {
               col_len = field->max_length;
            }
            if (col_len < 4 && !sql_field_is_not_null(field->flags)) {
               col_len = 4;                 /* 4 = length of the word "NULL" */
            }
            field->max_length = col_len;    /* reset column info */
         }
      }
      break;
   }

   Dmsg0(800, "list_result finished first loop\n");

   /*
    * See if filters are enabled for this list function.
    * We use this to shortcut for calling the filter_data() method in the
    * OUTPUT_FORMATTER class.
    */
   filters_enabled = send->has_filters();

   switch (type) {
   case NF_LIST:
   case RAW_LIST:
      Dmsg1(800, "list_result starts second loop looking at %d fields\n", num_fields);
      while ((row = sql_fetch_row()) != NULL) {
         /*
          * See if we should allow this under the current filtering.
          */
         if (filters_enabled && !send->filter_data(row)) {
            continue;
         }

         send->object_start();
         sql_field_seek(0);
         for (int i = 0; i < num_fields; i++) {
            field = sql_fetch_field();
            if (!field) {
               break;
            }

            /*
             * See if this is a hidden column.
             */
            if (send->is_hidden_column(i)) {
               Dmsg1(800, "list_result field %d is hidden\n", i);
               continue;
            }

            if (row[i] == NULL) {
               value.bsprintf("%s", "NULL");
            } else {
               value.bsprintf("%s", row[i]);
            }
            send->object_key_value(field->name, value.c_str(), " %s");
         }
         if (type != RAW_LIST) {
            send->decoration("\n");
         }
         send->object_end();
      }
      break;
   case HORZ_LIST:
      Dmsg1(800, "list_result starts second loop looking at %d fields\n", num_fields);
      list_dashes(send);
      send->decoration("|");
      sql_field_seek(0);
      for (int i = 0; i < num_fields; i++) {
         Dmsg1(800, "list_result looking at field %d\n", i);

         field = sql_fetch_field();
         if (!field) {
            break;
         }

         /*
          * See if this is a hidden column.
          */
         if (send->is_hidden_column(i)) {
            Dmsg1(800, "list_result field %d is hidden\n", i);
            continue;
         }

         max_len = max_length(field->max_length);
         send->decoration(" %-*s |", max_len, field->name);
      }
      send->decoration("\n");
      list_dashes(send);

      Dmsg1(800, "list_result starts third loop looking at %d fields\n", num_fields);
      while ((row = sql_fetch_row()) != NULL) {
         /*
          * See if we should allow this under the current filtering.
          */
         if (filters_enabled && !send->filter_data(row)) {
            continue;
         }

         send->object_start();
         sql_field_seek(0);
         send->decoration("|");
         for (int i = 0; i < num_fields; i++) {
            field = sql_fetch_field();
            if (!field) {
               break;
            }

            /*
             * See if this is a hidden column.
             */
            if (send->is_hidden_column(i)) {
               Dmsg1(800, "list_result field %d is hidden\n", i);
               continue;
            }

            max_len = max_length(field->max_length);
            if (row[i] == NULL) {
               value.bsprintf(" %-*s |", max_len, "NULL");
            } else if (sql_field_is_numeric(field->type) && !jcr->gui && is_an_integer(row[i])) {
               value.bsprintf(" %*s |", max_len, add_commas(row[i], ewc));
            } else {
               value.bsprintf(" %-*s |", max_len, row[i]);
            }
            if (i == num_fields-1) {
               value.strcat("\n");
            }

            /*
             * Use value format string to send preformated value
             */
            send->object_key_value(field->name, row[i], value.c_str());
         }
         send->object_end();
      }
      list_dashes(send);
      break;
   case VERT_LIST:
      Dmsg1(800, "list_result starts vertical list at %d fields\n", num_fields);
      while ((row = sql_fetch_row()) != NULL) {
         /*
          * See if we should allow this under the current filtering.
          */
         if (filters_enabled && !send->filter_data(row)) {
            continue;
         }

         send->object_start();
         sql_field_seek(0);
         for (int i = 0; i < num_fields; i++) {
            field = sql_fetch_field();
            if (!field) {
               break;
            }

            /*
             * See if this is a hidden column.
             */
            if (send->is_hidden_column(i)) {
               Dmsg1(800, "list_result field %d is hidden\n", i);
               continue;
            }

            if (row[i] == NULL) {
               key.bsprintf(" %*s: ", max_len, field->name);
               value.bsprintf("%s\n", "NULL");
            } else if (sql_field_is_numeric(field->type) && !jcr->gui && is_an_integer(row[i])) {
               key.bsprintf(" %*s: ", max_len, field->name);
               value.bsprintf("%s\n", add_commas(row[i], ewc));
            } else {
               key.bsprintf(" %*s: ", max_len, field->name);
               value.bsprintf("%s\n", row[i]);
            }

            /*
             * Use value format string to send preformated value
             */
            send->object_key_value(field->name, key.c_str(), row[i], value.c_str());
         }
         send->decoration("\n");
         send->object_end();
      }
      break;
   }

   return sql_num_rows();
}

/*
 * If full_list is set, we list vertically, otherwise, we list on one line horizontally.
 *
 * Return number of rows
 */
int list_result(JCR *jcr, B_DB *mdb, OUTPUT_FORMATTER *send, e_list_type type)
{
   return mdb->list_result(jcr, send, type);
}

/*
 * Open a new connexion to mdb catalog. This function is used by batch and accurate mode.
 */
bool B_DB::open_batch_connection(JCR *jcr)
{
   bool multi_db;

   multi_db = batch_insert_available();
   if (!jcr->db_batch) {
      jcr->db_batch = clone_database_connection(jcr, multi_db, multi_db);
      if (!jcr->db_batch) {
         Mmsg0(errmsg, _("Could not init database batch connection\n"));
         Jmsg(jcr, M_FATAL, 0, "%s", errmsg);
         return false;
      }
   }
   return true;
}

void B_DB::db_debug_print(FILE *fp)
{
   fprintf(fp, "B_DB=%p db_name=%s db_user=%s connected=%s\n",
           this, NPRTB(get_db_name()), NPRTB(get_db_user()), is_connected() ? "true" : "false");
   fprintf(fp, "\tcmd=\"%s\" changes=%i\n", NPRTB(cmd), changes);

   print_lock_info(fp);
}

/*
 * !!! WARNING !!! Use this function only when bareos is stopped.
 * ie, after a fatal signal and before exiting the program
 * Print information about a B_DB object.
 */
void db_debug_print(JCR *jcr, FILE *fp)
{
   B_DB *mdb = jcr->db;

   if (!mdb) {
      return;
   }

   mdb->db_debug_print(fp);
}
#endif /* HAVE_SQLITE3 || HAVE_MYSQL || HAVE_POSTGRESQL || HAVE_INGRES || HAVE_DBI */
