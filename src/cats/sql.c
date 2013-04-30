/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2000-2009 Free Software Foundation Europe e.V.

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
 * Bacula Catalog Database interface routines
 *
 *     Almost generic set of SQL database interface routines
 *      (with a little more work)
 *     SQL engine specific routines are in mysql.c, postgresql.c,
 *       sqlite.c, ...
 *
 *    Kern Sibbald, March 2000
 *
 *    Version $Id: sql.c 8034 2008-11-11 14:33:46Z ricozz $
 */

#include "bacula.h"

#if HAVE_SQLITE3 || HAVE_MYSQL || HAVE_POSTGRESQL || HAVE_INGRES || HAVE_DBI

#include "cats.h"
#include "bdb_priv.h"
#include "sql_glue.h"

/* Forward referenced subroutines */
void print_dashes(B_DB *mdb);
void print_result(B_DB *mdb);

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
   switch (db_get_type_index(context->db)) {
   case SQL_TYPE_MYSQL:
      index = 1;
   default:
      index = 0;
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
bool db_check_max_connections(JCR *jcr, B_DB *mdb, uint32_t max_concurrent_jobs)
{
   struct max_connections_context context;

   /* Without Batch insert, no need to verify max_connections */
   if (!mdb->batch_insert_available())
      return true;

   context.db = mdb;
   context.nr_connections = 0;

   /* Check max_connections setting */
   if (!db_sql_query(mdb, sql_get_max_connections[db_get_type_index(mdb)],
                     db_max_connections_handler, &context)) {
      Jmsg(jcr, M_ERROR, 0, "Can't verify max_connections settings %s", mdb->errmsg);
      return false;
   }
   if (context.nr_connections && max_concurrent_jobs && max_concurrent_jobs > context.nr_connections) {
      Mmsg(mdb->errmsg,
           _("Potential performance problem:\n"
             "max_connections=%d set for %s database \"%s\" should be larger than Director's "
             "MaxConcurrentJobs=%d\n"),
           context.nr_connections, db_get_type(mdb), mdb->get_db_name(), max_concurrent_jobs);
      Jmsg(jcr, M_WARNING, 0, "%s", mdb->errmsg);
      return false;
   }

   return true;
}

/* NOTE!!! The following routines expect that the
 *  calling subroutine sets and clears the mutex
 */

/* Check that the tables correspond to the version we want */
bool check_tables_version(JCR *jcr, B_DB *mdb)
{
   uint32_t bacula_db_version = 0;
   const char *query = "SELECT VersionId FROM Version";

   bacula_db_version = 0;
   if (!db_sql_query(mdb, query, db_int_handler, (void *)&bacula_db_version)) {
      Jmsg(jcr, M_FATAL, 0, "%s", mdb->errmsg);
      return false;
   }
   if (bacula_db_version != BDB_VERSION) {
      Mmsg(mdb->errmsg, "Version error for database \"%s\". Wanted %d, got %d\n",
          mdb->get_db_name(), BDB_VERSION, bacula_db_version);
      Jmsg(jcr, M_FATAL, 0, "%s", mdb->errmsg);
      return false;
   }
   return true;
}

/*
 * Utility routine for queries. The database MUST be locked before calling here.
 * Returns: 0 on failure
 *          1 on success
 */
int
QueryDB(const char *file, int line, JCR *jcr, B_DB *mdb, char *cmd)
{
   sql_free_result(mdb);
   if (!sql_query(mdb, cmd, QF_STORE_RESULT)) {
      m_msg(file, line, &mdb->errmsg, _("query %s failed:\n%s\n"), cmd, sql_strerror(mdb));
      j_msg(file, line, jcr, M_FATAL, 0, "%s", mdb->errmsg);
      if (verbose) {
         j_msg(file, line, jcr, M_INFO, 0, "%s\n", cmd);
      }
      return 0;
   }

   return 1;
}

/*
 * Utility routine to do inserts
 * Returns: 0 on failure
 *          1 on success
 */
int
InsertDB(const char *file, int line, JCR *jcr, B_DB *mdb, char *cmd)
{
   int num_rows;

   if (!sql_query(mdb, cmd)) {
      m_msg(file, line, &mdb->errmsg,  _("insert %s failed:\n%s\n"), cmd, sql_strerror(mdb));
      j_msg(file, line, jcr, M_FATAL, 0, "%s", mdb->errmsg);
      if (verbose) {
         j_msg(file, line, jcr, M_INFO, 0, "%s\n", cmd);
      }
      return 0;
   }
   num_rows = sql_affected_rows(mdb);
   if (num_rows != 1) {
      char ed1[30];
      m_msg(file, line, &mdb->errmsg, _("Insertion problem: affected_rows=%s\n"),
         edit_uint64(num_rows, ed1));
      if (verbose) {
         j_msg(file, line, jcr, M_INFO, 0, "%s\n", cmd);
      }
      return 0;
   }
   mdb->changes++;
   return 1;
}

/* Utility routine for updates.
 *  Returns: 0 on failure
 *           1 on success
 */
int
UpdateDB(const char *file, int line, JCR *jcr, B_DB *mdb, char *cmd)
{
   int num_rows;

   if (!sql_query(mdb, cmd)) {
      m_msg(file, line, &mdb->errmsg, _("update %s failed:\n%s\n"), cmd, sql_strerror(mdb));
      j_msg(file, line, jcr, M_ERROR, 0, "%s", mdb->errmsg);
      if (verbose) {
         j_msg(file, line, jcr, M_INFO, 0, "%s\n", cmd);
      }
      return 0;
   }
   num_rows = sql_affected_rows(mdb);
   if (num_rows < 1) {
      char ed1[30];
      m_msg(file, line, &mdb->errmsg, _("Update failed: affected_rows=%s for %s\n"),
         edit_uint64(num_rows, ed1), cmd);
      if (verbose) {
//       j_msg(file, line, jcr, M_INFO, 0, "%s\n", cmd);
      }
      return 0;
   }
   mdb->changes++;
   return 1;
}

/*
 * Utility routine for deletes
 *
 * Returns: -1 on error
 *           n number of rows affected
 */
int
DeleteDB(const char *file, int line, JCR *jcr, B_DB *mdb, char *cmd)
{

   if (!sql_query(mdb, cmd)) {
      m_msg(file, line, &mdb->errmsg, _("delete %s failed:\n%s\n"), cmd, sql_strerror(mdb));
      j_msg(file, line, jcr, M_ERROR, 0, "%s", mdb->errmsg);
      if (verbose) {
         j_msg(file, line, jcr, M_INFO, 0, "%s\n", cmd);
      }
      return -1;
   }
   mdb->changes++;
   return sql_affected_rows(mdb);
}


/*
 * Get record max. Query is already in mdb->cmd
 *  No locking done
 *
 * Returns: -1 on failure
 *          count on success
 */
int get_sql_record_max(JCR *jcr, B_DB *mdb)
{
   SQL_ROW row;
   int stat = 0;

   if (QUERY_DB(jcr, mdb, mdb->cmd)) {
      if ((row = sql_fetch_row(mdb)) == NULL) {
         Mmsg1(&mdb->errmsg, _("error fetching row: %s\n"), sql_strerror(mdb));
         stat = -1;
      } else {
         stat = str_to_int64(row[0]);
      }
      sql_free_result(mdb);
   } else {
      Mmsg1(&mdb->errmsg, _("error fetching row: %s\n"), sql_strerror(mdb));
      stat = -1;
   }
   return stat;
}

/*
 * Return pre-edited error message
 */
char *db_strerror(B_DB *mdb)
{
   return mdb->errmsg;
}

/*
 * Given a full filename, split it into its path
 *  and filename parts. They are returned in pool memory
 *  in the mdb structure.
 */
void split_path_and_file(JCR *jcr, B_DB *mdb, const char *fname)
{
   const char *p, *f;

   /* Find path without the filename.
    * I.e. everything after the last / is a "filename".
    * OK, maybe it is a directory name, but we treat it like
    * a filename. If we don't find a / then the whole name
    * must be a path name (e.g. c:).
    */
   for (p=f=fname; *p; p++) {
      if (IsPathSeparator(*p)) {
         f = p;                       /* set pos of last slash */
      }
   }
   if (IsPathSeparator(*f)) {                   /* did we find a slash? */
      f++;                            /* yes, point to filename */
   } else {                           /* no, whole thing must be path name */
      f = p;
   }

   /* If filename doesn't exist (i.e. root directory), we
    * simply create a blank name consisting of a single
    * space. This makes handling zero length filenames
    * easier.
    */
   mdb->fnl = p - f;
   if (mdb->fnl > 0) {
      mdb->fname = check_pool_memory_size(mdb->fname, mdb->fnl+1);
      memcpy(mdb->fname, f, mdb->fnl);    /* copy filename */
      mdb->fname[mdb->fnl] = 0;
   } else {
      mdb->fname[0] = 0;
      mdb->fnl = 0;
   }

   mdb->pnl = f - fname;
   if (mdb->pnl > 0) {
      mdb->path = check_pool_memory_size(mdb->path, mdb->pnl+1);
      memcpy(mdb->path, fname, mdb->pnl);
      mdb->path[mdb->pnl] = 0;
   } else {
      Mmsg1(&mdb->errmsg, _("Path length is zero. File=%s\n"), fname);
      Jmsg(jcr, M_ERROR, 0, "%s", mdb->errmsg);
      mdb->path[0] = 0;
      mdb->pnl = 0;
   }

   Dmsg2(500, "split path=%s file=%s\n", mdb->path, mdb->fname);
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
void list_dashes(B_DB *mdb, DB_LIST_HANDLER *send, void *ctx)
{
   SQL_FIELD  *field;
   int i, j;
   int len;
   int num_fields;

   sql_field_seek(mdb, 0);
   send(ctx, "+");
   num_fields = sql_num_fields(mdb);
   for (i = 0; i < num_fields; i++) {
      field = sql_fetch_field(mdb);
      if (!field) {
         break;
      }
      len = max_length(field->max_length + 2);
      for (j = 0; j < len; j++) {
         send(ctx, "-");
      }
      send(ctx, "+");
   }
   send(ctx, "\n");
}

/* Small handler to print the last line of a list xxx command */
static void last_line_handler(void *vctx, const char *str)
{
   LIST_CTX *ctx = (LIST_CTX *)vctx;
   bstrncat(ctx->line, str, sizeof(ctx->line));
}

int list_result(void *vctx, int nb_col, char **row)
{
   SQL_FIELD *field;
   int i, col_len, max_len = 0;
   int num_fields;
   char buf[2000], ewc[30];

   LIST_CTX *pctx = (LIST_CTX *)vctx;
   DB_LIST_HANDLER *send = pctx->send;
   e_list_type type = pctx->type;
   B_DB *mdb = pctx->mdb;
   void *ctx = pctx->ctx;
   JCR *jcr = pctx->jcr;

   num_fields = sql_num_fields(mdb);
   if (!pctx->once) {
      pctx->once = true;

      Dmsg1(800, "list_result starts looking at %d fields\n", num_fields);
      /* determine column display widths */
      sql_field_seek(mdb, 0);
      for (i = 0; i < num_fields; i++) {
         Dmsg1(800, "list_result processing field %d\n", i);
         field = sql_fetch_field(mdb);
         if (!field) {
            break;
         }
         col_len = cstrlen(field->name);
         if (type == VERT_LIST) {
            if (col_len > max_len) {
               max_len = col_len;
            }
         } else {
            if (sql_field_is_numeric(mdb, field->type) && (int)field->max_length > 0) { /* fixup for commas */
               field->max_length += (field->max_length - 1) / 3;
            }  
            if (col_len < (int)field->max_length) {
               col_len = field->max_length;
            }  
            if (col_len < 4 && !sql_field_is_not_null(mdb, field->flags)) {
               col_len = 4;                 /* 4 = length of the word "NULL" */
            }
            field->max_length = col_len;    /* reset column info */
         }
      }

      pctx->num_rows++;

      Dmsg0(800, "list_result finished first loop\n");
      if (type == VERT_LIST) {
         goto vertical_list;
      }

      Dmsg1(800, "list_result starts second loop looking at %d fields\n", num_fields);

      /* Keep the result to display the same line at the end of the table */
      list_dashes(mdb, last_line_handler, pctx);
      send(ctx, pctx->line);

      send(ctx, "|");
      sql_field_seek(mdb, 0);
      for (i = 0; i < num_fields; i++) {
         Dmsg1(800, "list_result looking at field %d\n", i);
         field = sql_fetch_field(mdb);
         if (!field) {
            break;
         }
         max_len = max_length(field->max_length);
         bsnprintf(buf, sizeof(buf), " %-*s |", max_len, field->name);
         send(ctx, buf);
      }
      send(ctx, "\n");
      list_dashes(mdb, send, ctx);      
   }
   
   Dmsg1(800, "list_result starts third loop looking at %d fields\n", num_fields);

   sql_field_seek(mdb, 0);
   send(ctx, "|");
   for (i = 0; i < num_fields; i++) {
      field = sql_fetch_field(mdb);
      if (!field) {
         break;
      }
      max_len = max_length(field->max_length);
      if (row[i] == NULL) {
         bsnprintf(buf, sizeof(buf), " %-*s |", max_len, "NULL");
      } else if (sql_field_is_numeric(mdb, field->type) && !jcr->gui && is_an_integer(row[i])) {
         bsnprintf(buf, sizeof(buf), " %*s |", max_len,
                   add_commas(row[i], ewc));
      } else {
         bsnprintf(buf, sizeof(buf), " %-*s |", max_len, row[i]);
      }
      send(ctx, buf);
   }
   send(ctx, "\n");
   return 0;

vertical_list:

   Dmsg1(800, "list_result starts vertical list at %d fields\n", num_fields);

   sql_field_seek(mdb, 0);
   for (i = 0; i < num_fields; i++) {
      field = sql_fetch_field(mdb);
      if (!field) {
         break;
      }
      if (row[i] == NULL) {
         bsnprintf(buf, sizeof(buf), " %*s: %s\n", max_len, field->name, "NULL");
      } else if (sql_field_is_numeric(mdb, field->type) && !jcr->gui && is_an_integer(row[i])) {
         bsnprintf(buf, sizeof(buf), " %*s: %s\n", max_len, field->name,
                   add_commas(row[i], ewc));
      } else {
         bsnprintf(buf, sizeof(buf), " %*s: %s\n", max_len, field->name, row[i]);
      }
      send(ctx, buf);
   }
   send(ctx, "\n");
   return 0;
}

/*
 * If full_list is set, we list vertically, otherwise, we
 *  list on one line horizontally.
 * Return number of rows
 */
int list_result(JCR *jcr, B_DB *mdb, DB_LIST_HANDLER *send, void *ctx, e_list_type type)
{
   SQL_FIELD *field;
   SQL_ROW row;
   int i, col_len, max_len = 0;
   int num_fields;
   char buf[2000], ewc[30];

   Dmsg0(800, "list_result starts\n");
   if (sql_num_rows(mdb) == 0) {
      send(ctx, _("No results to list.\n"));
      return sql_num_rows(mdb);
   }

   num_fields = sql_num_fields(mdb);
   Dmsg1(800, "list_result starts looking at %d fields\n", num_fields);
   /* determine column display widths */
   sql_field_seek(mdb, 0);
   for (i = 0; i < num_fields; i++) {
      Dmsg1(800, "list_result processing field %d\n", i);
      field = sql_fetch_field(mdb);
      if (!field) {
         break;
      }
      col_len = cstrlen(field->name);
      if (type == VERT_LIST) {
         if (col_len > max_len) {
            max_len = col_len;
         }
      } else {
         if (sql_field_is_numeric(mdb, field->type) && (int)field->max_length > 0) { /* fixup for commas */
            field->max_length += (field->max_length - 1) / 3;
         }  
         if (col_len < (int)field->max_length) {
            col_len = field->max_length;
         }  
         if (col_len < 4 && !sql_field_is_not_null(mdb, field->flags)) {
            col_len = 4;                 /* 4 = length of the word "NULL" */
         }
         field->max_length = col_len;    /* reset column info */
      }
   }

   Dmsg0(800, "list_result finished first loop\n");
   if (type == VERT_LIST) {
      goto vertical_list;
   }

   Dmsg1(800, "list_result starts second loop looking at %d fields\n", num_fields);
   list_dashes(mdb, send, ctx);
   send(ctx, "|");
   sql_field_seek(mdb, 0);
   for (i = 0; i < num_fields; i++) {
      Dmsg1(800, "list_result looking at field %d\n", i);
      field = sql_fetch_field(mdb);
      if (!field) {
         break;
      }
      max_len = max_length(field->max_length);
      bsnprintf(buf, sizeof(buf), " %-*s |", max_len, field->name);
      send(ctx, buf);
   }
   send(ctx, "\n");
   list_dashes(mdb, send, ctx);

   Dmsg1(800, "list_result starts third loop looking at %d fields\n", num_fields);
   while ((row = sql_fetch_row(mdb)) != NULL) {
      sql_field_seek(mdb, 0);
      send(ctx, "|");
      for (i = 0; i < num_fields; i++) {
         field = sql_fetch_field(mdb);
         if (!field) {
            break;
         }
         max_len = max_length(field->max_length);
         if (row[i] == NULL) {
            bsnprintf(buf, sizeof(buf), " %-*s |", max_len, "NULL");
         } else if (sql_field_is_numeric(mdb, field->type) && !jcr->gui && is_an_integer(row[i])) {
            bsnprintf(buf, sizeof(buf), " %*s |", max_len,
                      add_commas(row[i], ewc));
         } else {
            bsnprintf(buf, sizeof(buf), " %-*s |", max_len, row[i]);
         }
         send(ctx, buf);
      }
      send(ctx, "\n");
   }
   list_dashes(mdb, send, ctx);
   return sql_num_rows(mdb);

vertical_list:

   Dmsg1(800, "list_result starts vertical list at %d fields\n", num_fields);
   while ((row = sql_fetch_row(mdb)) != NULL) {
      sql_field_seek(mdb, 0);
      for (i = 0; i < num_fields; i++) {
         field = sql_fetch_field(mdb);
         if (!field) {
            break;
         }
         if (row[i] == NULL) {
            bsnprintf(buf, sizeof(buf), " %*s: %s\n", max_len, field->name, "NULL");
         } else if (sql_field_is_numeric(mdb, field->type) && !jcr->gui && is_an_integer(row[i])) {
            bsnprintf(buf, sizeof(buf), " %*s: %s\n", max_len, field->name,
                add_commas(row[i], ewc));
         } else {
            bsnprintf(buf, sizeof(buf), " %*s: %s\n", max_len, field->name, row[i]);
         }
         send(ctx, buf);
      }
      send(ctx, "\n");
   }
   return sql_num_rows(mdb);
}

/* 
 * Open a new connexion to mdb catalog. This function is used
 * by batch and accurate mode.
 */
bool db_open_batch_connexion(JCR *jcr, B_DB *mdb)
{
   bool multi_db;

   multi_db = mdb->batch_insert_available();

   if (!jcr->db_batch) {
      jcr->db_batch = db_clone_database_connection(mdb, jcr, multi_db);
      if (!jcr->db_batch) {
         Mmsg0(&mdb->errmsg, _("Could not init database batch connection\n"));
         Jmsg(jcr, M_FATAL, 0, "%s", mdb->errmsg);
         return false;
      }

      if (!db_open_database(jcr, jcr->db_batch)) {
         Mmsg2(&mdb->errmsg,  _("Could not open database \"%s\": ERR=%s\n"),
              jcr->db_batch->get_db_name(), db_strerror(jcr->db_batch));
         Jmsg(jcr, M_FATAL, 0, "%s", mdb->errmsg);
         return false;
      }      
   }
   return true;
}

/*
 * !!! WARNING !!! Use this function only when bacula is stopped.
 * ie, after a fatal signal and before exiting the program
 * Print information about a B_DB object.
 */
void db_debug_print(JCR *jcr, FILE *fp)
{
   B_DB *mdb = jcr->db;

   if (!mdb) {
      return;
   }

   fprintf(fp, "B_DB=%p db_name=%s db_user=%s connected=%s\n",
           mdb, NPRTB(mdb->get_db_name()), NPRTB(mdb->get_db_user()), mdb->is_connected() ? "true" : "false");
   fprintf(fp, "\tcmd=\"%s\" changes=%i\n", NPRTB(mdb->cmd), mdb->changes);
   mdb->print_lock_info(fp);
}

#endif /* HAVE_SQLITE3 || HAVE_MYSQL || HAVE_POSTGRESQL || HAVE_INGRES || HAVE_DBI */
