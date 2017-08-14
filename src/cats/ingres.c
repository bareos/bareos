/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2003-2011 Free Software Foundation Europe e.V.
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
 *    Stefan Reddig, June 2009 with help of Marco van Wieringen April 2010
 *    based upon work done
 *    by Dan Langille, December 2003 and
 *    by Kern Sibbald, March 2000
 * Major rewrite by Marco van Wieringen, January 2010 for catalog refactoring.
 */
/**
 * @file
 * BAREOS Catalog Database routines specific to Ingres
 * These are Ingres specific routines
 */

#include "bareos.h"

#ifdef HAVE_INGRES

#include "cats.h"
#include "myingres.h"
#include "bdb_ingres.h"
#include "lib/breg.h"

/* -----------------------------------------------------------------------
 *
 *   Ingres dependent defines and subroutines
 *
 * -----------------------------------------------------------------------
 */

/**
 * List of open databases.
 */
static dlist *db_list = NULL;

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

struct B_DB_RWRULE {
   int pattern_length;
   char *search_pattern;
   BREGEXP *rewrite_regexp;
   bool trigger;
};

/**
 * Create a new query filter.
 */
static bool db_allocate_query_filter(JCR *jcr, alist *query_filters, int pattern_length,
                                     const char *search_pattern, const char *filter)
{
   B_DB_RWRULE *rewrite_rule;

   rewrite_rule = (B_DB_RWRULE *)malloc(sizeof(B_DB_RWRULE));

   rewrite_rule->pattern_length = pattern_length;
   rewrite_rule->search_pattern = bstrdup(search_pattern);
   rewrite_rule->rewrite_regexp = new_bregexp(filter);
   rewrite_rule->trigger = false;

   if (!rewrite_rule->rewrite_regexp) {
      Jmsg(jcr, M_FATAL, 0, _("Failed to allocate space for query filter.\n"));
      free(rewrite_rule->search_pattern);
      free(rewrite_rule);
      return false;
   } else {
      query_filters->append(rewrite_rule);
      return true;
   }
}

/**
 * Create a stack of all filters that should be applied to a SQL query
 * before submitting it to the database backend.
 */
static inline alist *db_initialize_query_filters(JCR *jcr)
{
   alist *query_filters;

   query_filters = New(alist(10, not_owned_by_alist));

   if (!query_filters) {
      Jmsg(jcr, M_FATAL, 0, _("Failed to allocate space for query filters.\n"));
      return NULL;
   }

   db_allocate_query_filter(jcr, query_filters, 6, "OFFSET",
                            "/LIMIT ([0-9]+) OFFSET ([0-9]+)/OFFSET $2 FETCH NEXT $1 ROWS ONLY/ig");
   db_allocate_query_filter(jcr, query_filters, 5, "LIMIT",
                            "/LIMIT ([0-9]+)/FETCH FIRST $1 ROWS ONLY/ig");
   db_allocate_query_filter(jcr, query_filters, 9, "TEMPORARY",
                            "/CREATE TEMPORARY TABLE (.+)/DECLARE GLOBAL TEMPORARY TABLE $1 ON COMMIT PRESERVE ROWS WITH NORECOVERY/i");

   return query_filters;
}

/**
 * Free all query filters.
 */
static inline void db_destroy_query_filters(alist *query_filters)
{
   B_DB_RWRULE *rewrite_rule;

   foreach_alist(rewrite_rule, query_filters) {
      free_bregexp(rewrite_rule->rewrite_regexp);
      free(rewrite_rule->search_pattern);
      free(rewrite_rule);
   }

   delete query_filters;
}

B_DB_INGRES::B_DB_INGRES(JCR *jcr,
                         const char *db_driver,
                         const char *db_name,
                         const char *db_user,
                         const char *db_password,
                         const char *db_address,
                         int db_port,
                         const char *db_socket,
                         bool mult_db_connections,
                         bool disable_batch_insert,
                         bool try_reconnect,
                         bool exit_on_fatal,
                         bool need_private)
{
   B_DB_INGRES *mdb;
   int next_session_id = 0;

   /*
    * See what the next available session_id is.
    * We first see what the highest session_id is used now.
    */
   if (db_list) {
      foreach_dlist(mdb, db_list) {
         if (mdb->m_session_id > next_session_id) {
            next_session_id = mdb->m_session_id;
         }
      }
   }

   /*
    * Initialize the parent class members.
    */
   m_db_interface_type  = SQL_INTERFACE_TYPE_INGRES;
   m_db_type = SQL_TYPE_INGRES;
   m_db_driver = bstrdup("ingres");
   m_db_name = bstrdup(db_name);
   m_db_user = bstrdup(db_user);
   if (db_password) {
      m_db_password = bstrdup(db_password);
   }
   if (db_address) {
      m_db_address = bstrdup(db_address);
   }
   if (db_socket) {
      m_db_socket = bstrdup(db_socket);
   }
   m_db_port = db_port;
   if (disable_batch_insert) {
      m_disabled_batch_insert = true;
      m_have_batch_insert = false;
   } else {
      m_disabled_batch_insert = false;
#if defined(USE_BATCH_FILE_INSERT)
      m_have_batch_insert = true;
#else
      m_have_batch_insert = false;
#endif
   }

   errmsg = get_pool_memory(PM_EMSG); /* get error message buffer */
   *errmsg = 0;
   cmd = get_pool_memory(PM_EMSG); /* get command buffer */
   cached_path = get_pool_memory(PM_FNAME);
   cached_path_id = 0;
   m_ref_count = 1;
   fname = get_pool_memory(PM_FNAME);
   path = get_pool_memory(PM_FNAME);
   esc_name = get_pool_memory(PM_FNAME);
   esc_path = get_pool_memory(PM_FNAME);
   esc_obj = get_pool_memory(PM_FNAME);
   m_allow_transactions = mult_db_connections;
   m_is_private = need_private;
   m_try_reconnect = try_reconnect;
   m_exit_on_fatal = exit_on_fatal;

   /*
    * Initialize the private members.
    */
   m_db_handle = NULL;
   m_result = NULL;
   m_explicit_commit = true;
   m_session_id = ++next_session_id;
   m_query_filters = db_initialize_query_filters(jcr);

   /*
    * Put the db in the list.
    */
   if (db_list == NULL) {
      db_list = New(dlist(this, &this->m_link));
   }
   db_list->append(this);
}

B_DB_INGRES::~B_DB_INGRES()
{
}

/**
 * Now actually open the database.  This can generate errors,
 *   which are returned in the errmsg
 *
 * DO NOT close the database or delete mdb here !!!!
 */
bool B_DB_INGRES::open_database(JCR *jcr)
{
   bool retval = false;
   int errstat;

   P(mutex);
   if (m_connected) {
      retval = true;
      goto bail_out;
   }

   if ((errstat=rwl_init(&m_lock)) != 0) {
      berrno be;
      Mmsg1(&errmsg, _("Unable to initialize DB lock. ERR=%s\n"),
            be.bstrerror(errstat));
      goto bail_out;
   }

   m_db_handle = INGconnectDB(m_db_name, m_db_user, m_db_password, m_session_id);

   Dmsg0(50, "Ingres real CONNECT done\n");
   Dmsg3(50, "db_user=%s db_name=%s db_password=%s\n", m_db_user, m_db_name,
              m_db_password == NULL ? "(NULL)" : m_db_password);

   if (!m_db_handle) {
      Mmsg2(&errmsg, _("Unable to connect to Ingres server.\n"
            "Database=%s User=%s\n"
            "It is probably not running or your password is incorrect.\n"),
             m_db_name, m_db_user);
      goto bail_out;
   }

   m_connected = true;

   INGsetDefaultLockingMode(m_db_handle);

   if (!check_tables_version(jcr, this)) {
      goto bail_out;
   }

   retval = true;

bail_out:
   V(mutex);
   return retval;
}

void B_DB_INGRES::close_database(JCR *jcr)
{
   if (m_connected) {
      end_transaction(jcr);
   }
   P(mutex);
   m_ref_count--;
   if (m_ref_count == 0) {
      if (m_connected) {
         sql_free_result();
      }
      db_list->remove(this);
      if (m_connected && m_db_handle) {
         INGdisconnectDB(m_db_handle);
      }
      if (m_query_filters) {
         db_destroy_query_filters(m_query_filters);
      }
      if (rwl_is_init(&m_lock)) {
         rwl_destroy(&m_lock);
      }
      free_pool_memory(errmsg);
      free_pool_memory(cmd);
      free_pool_memory(cached_path);
      free_pool_memory(fname);
      free_pool_memory(path);
      free_pool_memory(esc_name);
      free_pool_memory(esc_path);
      free_pool_memory(esc_obj);
      free(m_db_driver);
      free(m_db_name);
      free(m_db_user);
      if (m_db_password) {
         free(m_db_password);
      }
      if (m_db_address) {
         free(m_db_address);
      }
      if (m_db_socket) {
         free(m_db_socket);
      }
      delete this;
      if (db_list->size() == 0) {
         delete db_list;
         db_list = NULL;
      }
   }
   V(mutex);
}

bool B_DB_INGRES::validate_connection(void)
{
   bool retval;

   /*
    * Perform a null query to see if the connection is still valid.
    */
   db_lock(this);
   if (!sql_query_without_handler("SELECT 1", true)) {
      retval = false;
      goto bail_out;
   }

   sql_free_result();
   retval = true;

bail_out:
   db_unlock(this);
   return retval;
}

/**
 * Start a transaction. This groups inserts and makes things
 * much more efficient. Usually started when inserting
 * file attributes.
 */
void B_DB_INGRES::start_transaction(JCR *jcr)
{
   if (!jcr->attr) {
      jcr->attr = get_pool_memory(PM_FNAME);
   }
   if (!jcr->ar) {
      jcr->ar = (ATTR_DBR *)malloc(sizeof(ATTR_DBR));
   }

   if (!m_allow_transactions) {
      return;
   }

   db_lock(this);
   /* Allow only 25,000 changes per transaction */
   if (m_transaction && changes > 25000) {
      end_transaction(jcr);
   }
   if (!m_transaction) {
      sql_query_without_handler("BEGIN");  /* begin transaction */
      Dmsg0(400, "Start Ingres transaction\n");
      m_transaction = true;
   }
   db_unlock(this);
}

void B_DB_INGRES::end_transaction(JCR *jcr)
{
   if (jcr && jcr->cached_attribute) {
      Dmsg0(400, "Flush last cached attribute.\n");
      if (!create_attributes_record(jcr, jcr->ar)) {
         Jmsg1(jcr, M_FATAL, 0, _("Attribute create error. %s"), strerror());
      }
      jcr->cached_attribute = false;
   }

   if (!m_allow_transactions) {
      return;
   }

   db_lock(this);
   if (m_transaction) {
      sql_query_without_handler("COMMIT"); /* end transaction */
      m_transaction = false;
      Dmsg1(400, "End Ingres transaction changes=%d\n", changes);
   }
   changes = 0;
   db_unlock(this);
}

/**
 * Submit a general SQL command (cmd), and for each row returned,
 *  the result_handler is called with the ctx.
 */
bool B_DB_INGRES::sql_query_with_handler(const char *query, DB_RESULT_HANDLER *result_handler, void *ctx)
{
   SQL_ROW row;
   bool retval = true;

   Dmsg1(500, "sql_query_with_handler starts with %s\n", query);

   db_lock(this);
   if (!sql_query_without_handler(query, QF_STORE_RESULT)) {
      Mmsg(errmsg, _("Query failed: %s: ERR=%s\n"), query, sql_strerror());
      Dmsg0(500, "sql_query_with_handler failed\n");
      retval = false;
      goto bail_out;
   }

   if (result_handler != NULL) {
      Dmsg0(500, "sql_query_with_handler invoking handler\n");
      while ((row = sql_fetch_row()) != NULL) {
         Dmsg0(500, "sql_query_with_handler sql_fetch_row worked\n");
         if (result_handler(ctx, m_num_fields, row))
            break;
      }
      sql_free_result();
   }

   Dmsg0(500, "sql_query_with_handler finished\n");

bail_out:
   db_unlock(this);
   return retval;
}

/**
 * Note, if this routine returns false (failure), BAREOS expects
 * that no result has been stored.
 *
 *  Returns:  true  on success
 *            false on failure
 *
 */
bool B_DB_INGRES::sql_query_without_handler(const char *query, int flags)
{
   int cols;
   char *cp, *bp;
   char *dup_query, *new_query;
   bool retval = true;
   bool start_of_transaction = false;
   bool end_of_transaction = false;
   B_DB_RWRULE *rewrite_rule;

   Dmsg1(500, "query starts with '%s'\n", query);
   /*
    * We always make a private copy of the query as we are doing serious
    * rewrites in this engine. When running the private copy through the
    * different query filters we loose the orginal private copy so we
    * first make a extra reference to it so we can free it on exit from the
    * function.
    */
   dup_query = new_query = bstrdup(query);

   /*
    * Iterate over the query string and perform any needed operations.
    * We use a sliding window over the query string where bp points to
    * the previous position in the query and cp to the current position
    * in the query.
    */
   bp = new_query;
   while (bp != NULL) {
      if ((cp = strchr(bp, ' ')) != NULL) {
         *cp++;
      }

      if (bstrncasecmp(bp, "BEGIN", 5)) {
         /*
          * This is the start of a transaction.
          * Inline copy the rest of the query over the BEGIN keyword.
          */
         if (cp) {
            bstrinlinecpy(bp, cp);
         } else {
            *bp = '\0';
         }
         start_of_transaction = true;
      } else if (bstrncasecmp(bp, "COMMIT", 6) && (cp == NULL || !bstrncasecmp(cp, "PRESERVE", 8))) {
         /*
          * This is the end of a transaction. We cannot check for just the COMMIT
          * keyword as a DECLARE of an tempory table also has the word COMMIT in it
          * but its followed by the word PRESERVE.
          * Inline copy the rest of the query over the COMMIT keyword.
          */
         if (cp) {
            bstrinlinecpy(bp, cp);
         } else {
            *bp = '\0';
         }
         end_of_transaction = true;
      }

      /*
       * See what query filter might match.
       */
      foreach_alist(rewrite_rule, m_query_filters) {
         if (bstrncasecmp(bp, rewrite_rule->search_pattern, rewrite_rule->pattern_length)) {
            rewrite_rule->trigger = true;
         }
      }

      /*
       * Slide window.
       */
      bp = cp;
   }

   /*
    * Run the query through all query filters that apply e.g. have the trigger set in the
    * previous loop.
    */
   foreach_alist(rewrite_rule, m_query_filters) {
      if (rewrite_rule->trigger) {
         new_query = rewrite_rule->rewrite_regexp->replace(new_query);
         rewrite_rule->trigger = false;
      }
   }

   if (start_of_transaction) {
      Dmsg0(500,"sql_query_without_handler: Start of transaction\n");
      m_explicit_commit = false;
   }

   /*
    * See if there is any query left after filtering for certain keywords.
    */
   bp = new_query;
   while (bp != NULL && strlen(bp) > 0) {
      /*
       * We are starting a new query.  reset everything.
       */
      m_num_rows = -1;
      m_row_number = -1;
      m_field_number = -1;

      if (m_result) {
         INGclear(m_result);  /* hmm, someone forgot to free?? */
         m_result = NULL;
      }

      /*
       * See if this is a multi-statement query. We split a multi-statement query
       * on the semi-column and feed the individual queries to the Ingres functions.
       * We use a sliding window over the query string where bp points to
       * the previous position in the query and cp to the current position
       * in the query.
       */
      if ((cp = strchr(bp, ';')) != NULL) {
         *cp++ = '\0';
      }

      Dmsg1(500, "sql_query_without_handler after rewrite continues with '%s'\n", bp);

      /*
       * See if we got a store_result hint which could mean we are running a select.
       * If flags has QF_STORE_RESULT not set we are sure its not a query that we
       * need to store anything for.
       */
      if (flags & QF_STORE_RESULT) {
         cols = INGgetCols(m_db_handle, bp, m_explicit_commit);
      } else {
         cols = 0;
      }

      if (cols <= 0) {
         if (cols < 0 ) {
            Dmsg0(500,"sql_query_without_handler: neg.columns: no DML stmt!\n");
            retval = false;
            goto bail_out;
         }
         Dmsg0(500,"sql_query_without_handler (non SELECT) starting...\n");
         /*
          * non SELECT
          */
         m_num_rows = INGexec(m_db_handle, bp, m_explicit_commit);
         if (m_num_rows == -1) {
           Dmsg0(500,"sql_query_without_handler (non SELECT) went wrong\n");
           retval = false;
           goto bail_out;
         } else {
           Dmsg0(500,"sql_query_without_handler (non SELECT) seems ok\n");
         }
      } else {
         /*
          * SELECT
          */
         Dmsg0(500,"sql_query_without_handler (SELECT) starting...\n");
         m_result = INGquery(m_db_handle, bp, m_explicit_commit);
         if (m_result != NULL) {
            Dmsg0(500, "we have a result\n");

            /*
             * How many fields in the set?
             */
            m_num_fields = (int)INGnfields(m_result);
            Dmsg1(500, "we have %d fields\n", m_num_fields);

            m_num_rows = INGntuples(m_result);
            Dmsg1(500, "we have %d rows\n", m_num_rows);
         } else {
            Dmsg0(500, "No resultset...\n");
            retval = false;
            goto bail_out;
         }
      }

      bp = cp;
   }

bail_out:
   if (end_of_transaction) {
      Dmsg0(500,"sql_query_without_handler: End of transaction, commiting work\n");
      m_explicit_commit = true;
      INGcommit(m_db_handle);
   }

   free(dup_query);
   Dmsg0(500, "sql_query_without_handler finishing\n");

   return retval;
}

void B_DB_INGRES::sql_free_result(void)
{
   db_lock(this);
   if (m_result) {
      INGclear(m_result);
      m_result = NULL;
   }
   if (m_rows) {
      free(m_rows);
      m_rows = NULL;
   }
   if (m_fields) {
      free(m_fields);
      m_fields = NULL;
   }
   m_num_rows = m_num_fields = 0;
   db_unlock(this);
}

SQL_ROW B_DB_INGRES::sql_fetch_row(void)
{
   int j;
   SQL_ROW row = NULL; /* by default, return NULL */

   if (!m_result) {
      return row;
   }
   if (m_result->num_rows <= 0) {
      return row;
   }

   Dmsg0(500, "sql_fetch_row start\n");

   if (!m_rows || m_rows_size < m_num_fields) {
      if (m_rows) {
         Dmsg0(500, "sql_fetch_row freeing space\n");
         free(m_rows);
      }
      Dmsg1(500, "we need space for %d bytes\n", sizeof(char *) * m_num_fields);
      m_rows = (SQL_ROW)malloc(sizeof(char *) * m_num_fields);
      m_rows_size = m_num_fields;

      /*
       * Now reset the row_number now that we have the space allocated
       */
      m_row_number = 0;
   }

   /*
    * If still within the result set
    */
   if (m_row_number < m_num_rows) {
      Dmsg2(500, "sql_fetch_row row number '%d' is acceptable (0..%d)\n", m_row_number, m_num_rows);
      /*
       * Get each value from this row
       */
      for (j = 0; j < m_num_fields; j++) {
         m_rows[j] = INGgetvalue(m_result, m_row_number, j);
         Dmsg2(500, "sql_fetch_row field '%d' has value '%s'\n", j, m_rows[j]);
      }
      /*
       * Increment the row number for the next call
       */
      m_row_number++;

      row = m_rows;
   } else {
      Dmsg2(500, "sql_fetch_row row number '%d' is NOT acceptable (0..%d)\n", m_row_number, m_num_rows);
   }

   Dmsg1(500, "sql_fetch_row finishes returning %p\n", row);

   return row;
}

const char *B_DB_INGRES::sql_strerror(void)
{
   return INGerrorMessage(m_db_handle);
}

void B_DB_INGRES::sql_data_seek(int row)
{
   /*
    * Set the row number to be returned on the next call to sql_fetch_row
    */
   m_row_number = row;
}

int B_DB_INGRES::sql_affected_rows(void)
{
   return m_num_rows;
}

/**
 * First execute the insert query and then retrieve the currval.
 * By setting transaction to true we make it an atomic transaction
 * and as such we can get the currval after which we commit if
 * transaction is false. This way things are an atomic operation
 * for Ingres and things work. We save the current transaction status
 * and set transaction in the mdb to true and at the end of this
 * function we restore the actual transaction status.
 */
uint64_t B_DB_INGRES::sql_insert_autokey_record(const char *query, const char *table_name)
{
   char sequence[64];
   char getkeyval_query[256];
   char *currval;
   uint64_t id = 0;
   bool current_explicit_commit;

   /*
    * Save the current transaction status and pretend we are in a transaction.
    */
   current_explicit_commit = m_explicit_commit;
   m_explicit_commit = false;

   /*
    * Execute the INSERT query.
    */
   m_num_rows = INGexec(m_db_handle, query, m_explicit_commit);
   if (m_num_rows == -1) {
      goto bail_out;
   }

   changes++;

   /*
    * Obtain the current value of the sequence that
    * provides the serial value for primary key of the table.
    *
    * currval is local to our session. It is not affected by
    * other transactions.
    *
    * Determine the name of the sequence.
    * As we name all sequences as <table>_seq this is easy.
    */
   bstrncpy(sequence, table_name, sizeof(sequence));
   bstrncat(sequence, "_seq", sizeof(sequence));

   bsnprintf(getkeyval_query, sizeof(getkeyval_query), "SELECT %s.currval FROM %s", sequence, table_name);

   if (m_result) {
      INGclear(m_result);
      m_result = NULL;
   }
   m_result = INGquery(m_db_handle, getkeyval_query, m_explicit_commit);

   if (!m_result) {
      Dmsg1(50, "Query failed: %s\n", getkeyval_query);
      goto bail_out;
   }

   Dmsg0(500, "exec done");

   currval = INGgetvalue(m_result, 0, 0);
   if (currval) {
      id = str_to_uint64(currval);
   }

   INGclear(m_result);
   m_result = NULL;

bail_out:
   /*
    * Restore the actual explicit_commit status.
    */
   m_explicit_commit = current_explicit_commit;

   /*
    * Commit if explicit_commit is not set.
    */
   if (m_explicit_commit) {
      INGcommit(m_db_handle);
   }

   return id;
}

SQL_FIELD *B_DB_INGRES::sql_fetch_field(void)
{
   int i, j;
   int max_length;
   int this_length;

   if (!m_fields || m_fields_size < m_num_fields) {
      if (m_fields) {
         free(m_fields);
         m_fields = NULL;
      }
      Dmsg1(500, "allocating space for %d fields\n", m_num_fields);
      m_fields = (SQL_FIELD *)malloc(sizeof(SQL_FIELD) * m_num_fields);
      m_fields_size = m_num_fields;

      for (i = 0; i < m_num_fields; i++) {
         Dmsg1(500, "filling field %d\n", i);
         m_fields[i].name = INGfname(m_result, i);
         m_fields[i].type = INGftype(m_result, i);
         m_fields[i].flags = 0;

         /*
          * For a given column, find the max length.
          */
         max_length = 0;
         for (j = 0; j < m_num_rows; j++) {
            if (INGgetisnull(m_result, j, i)) {
                this_length = 4;        /* "NULL" */
            } else {
                this_length = cstrlen(INGgetvalue(m_result, j, i));
            }

            if (max_length < this_length) {
               max_length = this_length;
            }
         }
         m_fields[i].max_length = max_length;

         Dmsg4(500, "sql_fetch_field finds field '%s' has length='%d' type='%d' and IsNull=%d\n",
               m_fields[i].name, m_fields[i].max_length, m_fields[i].type, m_fields[i].flags);
      }
   }

   /*
    * Increment field number for the next time around
    */
   return &m_fields[m_field_number++];
}

bool B_DB_INGRES::sql_field_is_not_null(int field_type)
{
   switch (field_type) {
   case 1:
      return true;
   default:
      return false;
   }
}

bool B_DB_INGRES::sql_field_is_numeric(int field_type)
{
   /*
    * See ${II_SYSTEM}/ingres/files/eqsqlda.h for numeric types.
    */
   switch (field_type) {
   case IISQ_DEC_TYPE:
   case IISQ_INT_TYPE:
   case IISQ_FLT_TYPE:
      return true;
   default:
      return false;
   }
}

/**
 * Escape strings so that Ingres is happy on COPY
 *
 *   NOTE! len is the length of the old string. Your new
 *         string must be long enough (max 2*old+1) to hold
 *         the escaped output.
 */
static char *ingres_copy_escape(char *dest, char *src, size_t len)
{
   /* we have to escape \t, \n, \r, \ */
   char c = '\0' ;

   while (len > 0 && *src) {
      switch (*src) {
      case '\n':
         c = 'n';
         break;
      case '\\':
         c = '\\';
         break;
      case '\t':
         c = 't';
         break;
      case '\r':
         c = 'r';
         break;
      default:
         c = '\0' ;
      }

      if (c) {
         *dest = '\\';
         dest++;
         *dest = c;
      } else {
         *dest = *src;
      }

      len--;
      src++;
      dest++;
   }

   *dest = '\0';
   return dest;
}

/**
 * Returns true if OK
 *         false if failed
 */
bool B_DB_INGRES::sql_batch_start(JCR *jcr)
{
   bool ok;

   db_lock(this);
   ok = sql_query_without_handler("DECLARE GLOBAL TEMPORARY TABLE batch ("
                                  "FileIndex INTEGER,"
                                  "JobId INTEGER,"
                                  "Path VARBYTE(32000),"
                                  "Name VARBYTE(32000),"
                                  "LStat VARBYTE(255),"
                                  "MD5 VARBYTE(255),"
                                  "DeltaSeq SMALLINT)"
                                  " ON COMMIT PRESERVE ROWS WITH NORECOVERY");
   db_unlock(this);
   return ok;
}

/**
 * Returns true if OK
 *         false if failed
 */
bool B_DB_INGRES::sql_batch_end(JCR *jcr, const char *error)
{
   m_status = 0;
   return true;
}

/**
 * Returns true if OK
 *         false if failed
 */
bool B_DB_INGRES::sql_batch_insert(JCR *jcr, ATTR_DBR *ar)
{
   size_t len;
   const char *digest;
   char ed1[50], ed2[50], ed3[50];

   esc_name = check_pool_memory_size(esc_name, fnl*2+1);
   escape_string(jcr, esc_name, fname, fnl);

   esc_path = check_pool_memory_size(esc_path, pnl*2+1);
   escape_string(jcr, esc_path, path, pnl);

   if (ar->Digest == NULL || ar->Digest[0] == 0) {
      digest = "0";
   } else {
      digest = ar->Digest;
   }

   len = Mmsg(cmd, "INSERT INTO batch VALUES "
                   "(%u,%s,'%s','%s','%s','%s','%s','%s')",
                   ar->FileIndex, edit_int64(ar->JobId,ed1), esc_path,
                   esc_name, ar->attr, digest,
                   edit_uint64(ar->Fhinfo,ed2),
                   edit_uint64(ar->Fhnode,ed3));


   return sql_query_without_handler(cmd);
}

/**
 * Initialize database data structure. In principal this should
 * never have errors, or it is really fatal.
 */
#ifdef HAVE_DYNAMIC_CATS_BACKENDS
extern "C" B_DB CATS_IMP_EXP *backend_instantiate(JCR *jcr,
                                                  const char *db_driver,
                                                  const char *db_name,
                                                  const char *db_user,
                                                  const char *db_password,
                                                  const char *db_address,
                                                  int db_port,
                                                  const char *db_socket,
                                                  bool mult_db_connections,
                                                  bool disable_batch_insert,
                                                  bool try_reconnect,
                                                  bool exit_on_fatal,
                                                  bool need_private)
#else
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
                       bool try_reconnect,
                       bool exit_on_fatal,
                       bool need_private)
#endif
{
   B_DB_INGRES *mdb = NULL;

   if (!db_user) {
      Jmsg(jcr, M_FATAL, 0, _("A user name for Ingres must be supplied.\n"));
      return NULL;
   }

   P(mutex);                          /* lock DB queue */

   /*
    * Look to see if DB already open
    */
   if (db_list && !mult_db_connections && !need_private) {
      foreach_dlist(mdb, db_list) {
         if (mdb->is_private()) {
            continue;
         }

         if (mdb->db_match_database(db_driver, db_name, db_address, db_port)) {
            Dmsg1(100, "DB REopen %s\n", db_name);
            mdb->increment_refcount();
            goto bail_out;
         }
      }
   }

   Dmsg0(100, "db_init_database first time\n");
   mdb = New(B_DB_INGRES(jcr,
                         db_driver,
                         db_name,
                         db_user,
                         db_password,
                         db_address,
                         db_port,
                         db_socket,
                         mult_db_connections,
                         disable_batch_insert,
                         try_reconnect,
                         exit_on_fatal,
                         need_private));

bail_out:
   V(mutex);
   return mdb;
}

#ifdef HAVE_DYNAMIC_CATS_BACKENDS
extern "C" void CATS_IMP_EXP flush_backend(void)
#else
void db_flush_backends(void)
#endif
{
}

#endif /* HAVE_INGRES */
