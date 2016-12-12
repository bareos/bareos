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
 * Dan Langille, December 2003
 * based upon work done by Kern Sibbald, March 2000
 * Major rewrite by Marco van Wieringen, January 2010 for catalog refactoring.
 */
/**
 * @file
 * BAREOS Catalog Database routines specific to PostgreSQL
 * These are PostgreSQL specific routines
 */

#include "bareos.h"

#ifdef HAVE_POSTGRESQL

#include "cats.h"
#include "libpq-fe.h"
#include "postgres_ext.h"       /* needed for NAMEDATALEN */
#include "pg_config_manual.h"   /* get NAMEDATALEN on version 8.3 or later */
#include "bdb_postgresql.h"

/* -----------------------------------------------------------------------
 *
 *   PostgreSQL dependent defines and subroutines
 *
 * -----------------------------------------------------------------------
 */

/**
 * List of open databases
 */
static dlist *db_list = NULL;

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

static BDB_QUERY_TABLE *pgsql_query_table = NULL;

B_DB_POSTGRESQL::B_DB_POSTGRESQL(JCR *jcr,
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
   /*
    * Initialize the parent class members.
    */
   m_db_interface_type = SQL_INTERFACE_TYPE_POSTGRESQL;
   m_db_type = SQL_TYPE_POSTGRESQL;
   m_db_driver = bstrdup("PostgreSQL");
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
#if defined(HAVE_POSTGRESQL_BATCH_FILE_INSERT) || defined(HAVE_PQISTHREADSAFE)
#ifdef HAVE_PQISTHREADSAFE
      m_have_batch_insert = PQisthreadsafe();
#else
      m_have_batch_insert = true;
#endif /* HAVE_PQISTHREADSAFE */
#else
      m_have_batch_insert = true;
#endif /* HAVE_POSTGRESQL_BATCH_FILE_INSERT || HAVE_PQISTHREADSAFE */
#else
      m_have_batch_insert = false;
#endif /* USE_BATCH_FILE_INSERT */
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
   m_buf =  get_pool_memory(PM_FNAME);
   m_allow_transactions = mult_db_connections;
   m_is_private = need_private;
   m_try_reconnect = try_reconnect;
   m_exit_on_fatal = exit_on_fatal;
   m_query_table = pgsql_query_table;
   m_last_hash_key = 0;
   m_last_query_text = NULL;

   /*
    * Initialize the private members.
    */
   m_db_handle = NULL;
   m_result = NULL;

   /*
    * Put the db in the list.
    */
   if (db_list == NULL) {
      db_list = New(dlist(this, &this->m_link));
   }
   db_list->append(this);
}

B_DB_POSTGRESQL::~B_DB_POSTGRESQL()
{
}

/**
 * Check that the database correspond to the encoding we want
 */
bool B_DB_POSTGRESQL::check_database_encoding(JCR *jcr)
{
   SQL_ROW row;
   bool retval = false;

   if (!sql_query_without_handler("SELECT getdatabaseencoding()", QF_STORE_RESULT)) {
      Jmsg(jcr, M_ERROR, 0, "%s", errmsg);
      return false;
   }

   if ((row = sql_fetch_row()) == NULL) {
      Mmsg1(errmsg, _("error fetching row: %s\n"), errmsg);
      Jmsg(jcr, M_ERROR, 0, "Can't check database encoding %s", errmsg);
   } else {
      retval = bstrcmp(row[0], "SQL_ASCII");

      if (retval) {
         /*
          * If we are in SQL_ASCII, we can force the client_encoding to SQL_ASCII too
          */
         sql_query_without_handler("SET client_encoding TO 'SQL_ASCII'");
      } else {
         /*
          * Something is wrong with database encoding
          */
         Mmsg(errmsg, _("Encoding error for database \"%s\". Wanted SQL_ASCII, got %s\n"), get_db_name(), row[0]);
         Jmsg(jcr, M_WARNING, 0, "%s", errmsg);
         Dmsg1(50, "%s", errmsg);
      }
   }

   return retval;
}

/**
 * Now actually open the database.  This can generate errors, which are returned in the errmsg
 *
 * DO NOT close the database or delete mdb here !!!!
 */
bool B_DB_POSTGRESQL::open_database(JCR *jcr)
{
   bool retval = false;
   int errstat;
   char buf[10], *port;

   P(mutex);
   if (m_connected) {
      retval = true;
      goto bail_out;
   }

   if ((errstat = rwl_init(&m_lock)) != 0) {
      berrno be;
      Mmsg1(errmsg, _("Unable to initialize DB lock. ERR=%s\n"), be.bstrerror(errstat));
      goto bail_out;
   }

   if (m_db_port) {
      bsnprintf(buf, sizeof(buf), "%d", m_db_port);
      port = buf;
   } else {
      port = NULL;
   }

   /*
    * If connection fails, try at 5 sec intervals for 30 seconds.
    */
   for (int retry = 0; retry < 6; retry++) {
      /*
       * Connect to the database
       */
      m_db_handle = PQsetdbLogin(m_db_address,   /* default = localhost */
                                 port,           /* default port */
                                 NULL,           /* pg options */
                                 NULL,           /* tty, ignored */
                                 m_db_name,      /* database name */
                                 m_db_user,      /* login name */
                                 m_db_password); /* password */

      /*
       * If no connect, try once more in case it is a timing problem
       */
      if (PQstatus(m_db_handle) == CONNECTION_OK) {
         break;
      }

      bmicrosleep(5, 0);
   }

   Dmsg0(50, "pg_real_connect done\n");
   Dmsg3(50, "db_user=%s db_name=%s db_password=%s\n", m_db_user, m_db_name,
        (m_db_password == NULL) ? "(NULL)" : m_db_password);

   if (PQstatus(m_db_handle) != CONNECTION_OK) {
      Mmsg2(errmsg, _("Unable to connect to PostgreSQL server. Database=%s User=%s\n"
                       "Possible causes: SQL server not running; password incorrect; max_connections exceeded.\n"),
            m_db_name, m_db_user);
      goto bail_out;
   }

   m_connected = true;
   if (!check_tables_version(jcr)) {
      goto bail_out;
   }

   sql_query_without_handler("SET datestyle TO 'ISO, YMD'");
   sql_query_without_handler("SET cursor_tuple_fraction=1");

   /*
    * Tell PostgreSQL we are using standard conforming strings
    * and avoid warnings such as:
    *  WARNING:  nonstandard use of \\ in a string literal
    */
   sql_query_without_handler("SET standard_conforming_strings=on");

   /*
    * Check that encoding is SQL_ASCII
    */
   check_database_encoding(jcr);

   retval = true;

bail_out:
   V(mutex);
   return retval;
}

void B_DB_POSTGRESQL::close_database(JCR *jcr)
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
         PQfinish(m_db_handle);
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
      free_pool_memory(m_buf);
      if (m_db_driver) {
         free(m_db_driver);
      }
      if (m_db_name) {
         free(m_db_name);
      }
      if (m_db_user) {
         free(m_db_user);
      }
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

bool B_DB_POSTGRESQL::validate_connection(void)
{
   bool retval = false;

   /*
    * Perform a null query to see if the connection is still valid.
    */
   db_lock(this);
   if (!sql_query_without_handler("SELECT 1", true)) {
      /*
       * Try resetting the connection.
       */
      PQreset(m_db_handle);
      if (PQstatus(m_db_handle) != CONNECTION_OK) {
         goto bail_out;
      }

      sql_query_without_handler("SET datestyle TO 'ISO, YMD'");
      sql_query_without_handler("SET cursor_tuple_fraction=1");
      sql_query_without_handler("SET standard_conforming_strings=on");

      /*
       * Retry the null query.
       */
      if (!sql_query_without_handler("SELECT 1", true)) {
         goto bail_out;
      }
   }

   sql_free_result();
   retval = true;

bail_out:
   db_unlock(this);
   return retval;
}

/**
 * Escape strings so that PostgreSQL is happy
 *
 *   NOTE! len is the length of the old string. Your new
 *         string must be long enough (max 2*old+1) to hold
 *         the escaped output.
 */
void B_DB_POSTGRESQL::escape_string(JCR *jcr, char *snew, char *old, int len)
{
   int error;

   PQescapeStringConn(m_db_handle, snew, old, len, &error);
   if (error) {
      Jmsg(jcr, M_FATAL, 0, _("PQescapeStringConn returned non-zero.\n"));
      /* error on encoding, probably invalid multibyte encoding in the source string
        see PQescapeStringConn documentation for details. */
      Dmsg0(500, "PQescapeStringConn failed\n");
   }
}

/**
 * Escape binary so that PostgreSQL is happy
 *
 */
char *B_DB_POSTGRESQL::escape_object(JCR *jcr, char *old, int len)
{
   size_t new_len;
   unsigned char *obj;

   obj = PQescapeByteaConn(m_db_handle, (unsigned const char *)old, len, &new_len);
   if (!obj) {
      Jmsg(jcr, M_FATAL, 0, _("PQescapeByteaConn returned NULL.\n"));
   }

   esc_obj = check_pool_memory_size(esc_obj, new_len+1);
   memcpy(esc_obj, obj, new_len);
   esc_obj[new_len]=0;

   PQfreemem(obj);

   return (char *)esc_obj;
}

/**
 * Unescape binary object so that PostgreSQL is happy
 *
 */
void B_DB_POSTGRESQL::unescape_object(JCR *jcr, char *from, int32_t expected_len,
                                      POOLMEM *&dest, int32_t *dest_len)
{
   size_t new_len;
   unsigned char *obj;

   if (!from) {
      dest[0] = '\0';
      *dest_len = 0;
      return;
   }

   obj = PQunescapeBytea((unsigned const char *)from, &new_len);

   if (!obj) {
      Jmsg(jcr, M_FATAL, 0, _("PQunescapeByteaConn returned NULL.\n"));
   }

   *dest_len = new_len;
   dest = check_pool_memory_size(dest, new_len + 1);
   memcpy(dest, obj, new_len);
   dest[new_len] = '\0';

   PQfreemem(obj);

   Dmsg1(010, "obj size: %d\n", *dest_len);
}

/**
 * Start a transaction. This groups inserts and makes things
 * much more efficient. Usually started when inserting
 * file attributes.
 */
void B_DB_POSTGRESQL::start_transaction(JCR *jcr)
{
   if (!jcr->attr) {
      jcr->attr = get_pool_memory(PM_FNAME);
   }
   if (!jcr->ar) {
      jcr->ar = (ATTR_DBR *)malloc(sizeof(ATTR_DBR));
   }

   /*
    * This is turned off because transactions break
    * if multiple simultaneous jobs are run.
    */
   if (!m_allow_transactions) {
      return;
   }

   db_lock(this);
   /*
    * Allow only 25,000 changes per transaction
    */
   if (m_transaction && changes > 25000) {
      end_transaction(jcr);
   }
   if (!m_transaction) {
      sql_query_without_handler("BEGIN");  /* begin transaction */
      Dmsg0(400, "Start PosgreSQL transaction\n");
      m_transaction = true;
   }
   db_unlock(this);
}

void B_DB_POSTGRESQL::end_transaction(JCR *jcr)
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
      Dmsg1(400, "End PostgreSQL transaction changes=%d\n", changes);
   }
   changes = 0;
   db_unlock(this);
}

/**
 * Submit a general SQL command (cmd), and for each row returned,
 * the result_handler is called with the ctx.
 */
bool B_DB_POSTGRESQL::big_sql_query(const char *query, DB_RESULT_HANDLER *result_handler, void *ctx)
{
   SQL_ROW row;
   bool retval = false;
   bool in_transaction = m_transaction;

   Dmsg1(500, "big_sql_query starts with '%s'\n", query);

   /* This code handles only SELECT queries */
   if (!bstrncasecmp(query, "SELECT", 6)) {
      return sql_query_with_handler(query, result_handler, ctx);
   }

   if (!result_handler) {       /* no need of big_query without handler */
      return false;
   }

   db_lock(this);

   if (!in_transaction) {       /* CURSOR needs transaction */
      sql_query_without_handler("BEGIN");
   }

   Mmsg(m_buf, "DECLARE _bac_cursor CURSOR FOR %s", query);

   if (!sql_query_without_handler(m_buf)) {
      Mmsg(errmsg, _("Query failed: %s: ERR=%s\n"), m_buf, sql_strerror());
      Dmsg0(50, "sql_query_without_handler failed\n");
      goto bail_out;
   }

   do {
      if (!sql_query_without_handler("FETCH 100 FROM _bac_cursor")) {
         goto bail_out;
      }
      while ((row = sql_fetch_row()) != NULL) {
         Dmsg1(500, "Fetching %d rows\n", m_num_rows);
         if (result_handler(ctx, m_num_fields, row))
            break;
      }
      PQclear(m_result);
      m_result = NULL;

   } while (m_num_rows > 0);

   sql_query_without_handler("CLOSE _bac_cursor");

   Dmsg0(500, "big_sql_query finished\n");
   sql_free_result();
   retval = true;

bail_out:
   if (!in_transaction) {
      sql_query_without_handler("COMMIT");  /* end transaction */
   }

   db_unlock(this);
   return retval;
}

/**
 * Submit a general SQL command (cmd), and for each row returned,
 * the result_handler is called with the ctx.
 */
bool B_DB_POSTGRESQL::sql_query_with_handler(const char *query, DB_RESULT_HANDLER *result_handler, void *ctx)
{
   SQL_ROW row;
   bool retval = true;

   Dmsg1(500, "sql_query_with_handler starts with '%s'\n", query);

   db_lock(this);
   if (!sql_query_without_handler(query, QF_STORE_RESULT)) {
      Mmsg(errmsg, _("Query failed: %s: ERR=%s\n"), query, sql_strerror());
      Dmsg0(500, "sql_query_with_handler failed\n");
      retval = false;
      goto bail_out;
   }

   Dmsg0(500, "sql_query_with_handler succeeded. checking handler\n");

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
 * This is where QUERY_DB comes with Postgresql.
 *
 * Returns:  true  on success
 *           false on failure
 */
bool B_DB_POSTGRESQL::sql_query_without_handler(const char *query, int flags)
{
   int i;
   bool retry = true;
   bool retval = false;

   Dmsg1(500, "sql_query_without_handler starts with '%s'\n", query);

   /*
    * We are starting a new query. reset everything.
    */
retry_query:
   m_num_rows = -1;
   m_row_number = -1;
   m_field_number = -1;

   if (m_result) {
      PQclear(m_result);  /* hmm, someone forgot to free?? */
      m_result = NULL;
   }

   for (i = 0; i < 10; i++) {
      m_result = PQexec(m_db_handle, query);
      if (m_result) {
         break;
      }
      bmicrosleep(5, 0);
   }

   m_status = PQresultStatus(m_result);
   switch (m_status) {
   case PGRES_TUPLES_OK:
   case PGRES_COMMAND_OK:
      Dmsg0(500, "we have a result\n");

      /*
       * How many fields in the set?
       */
      m_num_fields = (int)PQnfields(m_result);
      Dmsg1(500, "we have %d fields\n", m_num_fields);

      m_num_rows = PQntuples(m_result);
      Dmsg1(500, "we have %d rows\n", m_num_rows);

      m_row_number = 0;      /* we can start to fetch something */
      m_status = 0;          /* succeed */
      retval = true;
      break;
   case PGRES_FATAL_ERROR:
      Dmsg1(50, "Result status fatal: %s\n", query);
      if (m_exit_on_fatal) {
         /*
          * Any fatal error should result in the daemon exiting.
          */
         Emsg0(M_FATAL, 0, "Fatal database error\n");
      }

      if (m_try_reconnect && !m_transaction) {
         /*
          * Only try reconnecting when no transaction is pending.
          * Reconnecting within a transaction will lead to an aborted
          * transaction anyway so we better follow our old error path.
          */
         if (retry) {
            PQreset(m_db_handle);

            /*
             * See if we got a working connection again.
             */
            if (PQstatus(m_db_handle) == CONNECTION_OK) {
               /*
                * Reset the connection settings.
                */
               PQexec(m_db_handle, "SET datestyle TO 'ISO, YMD'");
               PQexec(m_db_handle, "SET cursor_tuple_fraction=1");
               m_result = PQexec(m_db_handle, "SET standard_conforming_strings=on");

               switch (PQresultStatus(m_result)) {
               case PGRES_COMMAND_OK:
                  retry = false;
                  goto retry_query;
               default:
                  break;
               }
            }
         }
      }
      goto bail_out;
   default:
      Dmsg1(50, "Result status failed: %s\n", query);
      goto bail_out;
   }

   Dmsg0(500, "sql_query_without_handler finishing\n");
   goto ok_out;

bail_out:
   Dmsg0(500, "we failed\n");
   PQclear(m_result);
   m_result = NULL;
   m_status = 1;                   /* failed */

ok_out:
   return retval;
}

void B_DB_POSTGRESQL::sql_free_result(void)
{
   db_lock(this);
   if (m_result) {
      PQclear(m_result);
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

SQL_ROW B_DB_POSTGRESQL::sql_fetch_row(void)
{
   int j;
   SQL_ROW row = NULL; /* by default, return NULL */

   Dmsg0(500, "sql_fetch_row start\n");

   if (m_num_fields == 0) {     /* No field, no row */
      Dmsg0(500, "sql_fetch_row finishes returning NULL, no fields\n");
      return NULL;
   }

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
   if (m_row_number >= 0 && m_row_number < m_num_rows) {
      Dmsg2(500, "sql_fetch_row row number '%d' is acceptable (0..%d)\n", m_row_number, m_num_rows);
      /*
       * Get each value from this row
       */
      for (j = 0; j < m_num_fields; j++) {
         m_rows[j] = PQgetvalue(m_result, m_row_number, j);
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

const char *B_DB_POSTGRESQL::sql_strerror(void)
{
   return PQerrorMessage(m_db_handle);
}

void B_DB_POSTGRESQL::sql_data_seek(int row)
{
   /*
    * Set the row number to be returned on the next call to sql_fetch_row
    */
   m_row_number = row;
}

int B_DB_POSTGRESQL::sql_affected_rows(void)
{
   return (unsigned) str_to_int32(PQcmdTuples(m_result));
}

uint64_t B_DB_POSTGRESQL::sql_insert_autokey_record(const char *query, const char *table_name)
{
   int i;
   uint64_t id = 0;
   char sequence[NAMEDATALEN-1];
   char getkeyval_query[NAMEDATALEN+50];
   PGresult *pg_result;

   /*
    * First execute the insert query and then retrieve the currval.
    */
   if (!sql_query_without_handler(query)) {
      return 0;
   }

   m_num_rows = sql_affected_rows();
   if (m_num_rows != 1) {
      return 0;
   }

   changes++;

   /*
    * Obtain the current value of the sequence that
    * provides the serial value for primary key of the table.
    *
    * currval is local to our session.  It is not affected by
    * other transactions.
    *
    * Determine the name of the sequence.
    * PostgreSQL automatically creates a sequence using
    * <table>_<column>_seq.
    * At the time of writing, all tables used this format for
    * for their primary key: <table>id
    * Except for basefiles which has a primary key on baseid.
    * Therefore, we need to special case that one table.
    *
    * everything else can use the PostgreSQL formula.
    */
   if (bstrcasecmp(table_name, "basefiles")) {
      bstrncpy(sequence, "basefiles_baseid", sizeof(sequence));
   } else {
      bstrncpy(sequence, table_name, sizeof(sequence));
      bstrncat(sequence, "_",        sizeof(sequence));
      bstrncat(sequence, table_name, sizeof(sequence));
      bstrncat(sequence, "id",       sizeof(sequence));
   }

   bstrncat(sequence, "_seq", sizeof(sequence));
   bsnprintf(getkeyval_query, sizeof(getkeyval_query), "SELECT currval('%s')", sequence);

   Dmsg1(500, "sql_insert_autokey_record executing query '%s'\n", getkeyval_query);
   for (i = 0; i < 10; i++) {
      pg_result = PQexec(m_db_handle, getkeyval_query);
      if (pg_result) {
         break;
      }
      bmicrosleep(5, 0);
   }
   if (!pg_result) {
      Dmsg1(50, "Query failed: %s\n", getkeyval_query);
      goto bail_out;
   }

   Dmsg0(500, "exec done");

   if (PQresultStatus(pg_result) == PGRES_TUPLES_OK) {
      Dmsg0(500, "getting value");
      id = str_to_uint64(PQgetvalue(pg_result, 0, 0));
      Dmsg2(500, "got value '%s' which became %d\n", PQgetvalue(pg_result, 0, 0), id);
   } else {
      Dmsg1(50, "Result status failed: %s\n", getkeyval_query);
      Mmsg1(errmsg, _("error fetching currval: %s\n"), PQerrorMessage(m_db_handle));
   }

bail_out:
   PQclear(pg_result);

   return id;
}

SQL_FIELD *B_DB_POSTGRESQL::sql_fetch_field(void)
{
   int i, j;
   int max_length;
   int this_length;

   Dmsg0(500, "sql_fetch_field starts\n");

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
         m_fields[i].name = PQfname(m_result, i);
         m_fields[i].type = PQftype(m_result, i);
         m_fields[i].flags = 0;

         /*
          * For a given column, find the max length.
          */
         max_length = 0;
         for (j = 0; j < m_num_rows; j++) {
            if (PQgetisnull(m_result, j, i)) {
                this_length = 4;        /* "NULL" */
            } else {
                this_length = cstrlen(PQgetvalue(m_result, j, i));
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

bool B_DB_POSTGRESQL::sql_field_is_not_null(int field_type)
{
   switch (field_type) {
   case 1:
      return true;
   default:
      return false;
   }
}

bool B_DB_POSTGRESQL::sql_field_is_numeric(int field_type)
{
   /*
    * TEMP: the following is taken from select OID, typname from pg_type;
    */
   switch (field_type) {
   case 20:
   case 21:
   case 23:
   case 700:
   case 701:
      return true;
   default:
      return false;
   }
}

/**
 * Escape strings so that PostgreSQL is happy on COPY
 *
 *   NOTE! len is the length of the old string. Your new
 *         string must be long enough (max 2*old+1) to hold
 *         the escaped output.
 */
static char *pgsql_copy_escape(char *dest, char *src, size_t len)
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

bool B_DB_POSTGRESQL::sql_batch_start(JCR *jcr)
{
   const char *query = "COPY batch FROM STDIN";

   Dmsg0(500, "sql_batch_start started\n");

   if (!sql_query_without_handler("CREATE TEMPORARY TABLE batch ("
                                  "FileIndex int,"
                                  "JobId int,"
                                  "Path varchar,"
                                  "Name varchar,"
                                  "LStat varchar,"
                                  "Md5 varchar,"
                                  "DeltaSeq smallint)")) {
      Dmsg0(500, "sql_batch_start failed\n");
      return false;
   }

   /*
    * We are starting a new query.  reset everything.
    */
   m_num_rows = -1;
   m_row_number = -1;
   m_field_number = -1;

   sql_free_result();

   for (int i=0; i < 10; i++) {
      m_result = PQexec(m_db_handle, query);
      if (m_result) {
         break;
      }
      bmicrosleep(5, 0);
   }
   if (!m_result) {
      Dmsg1(50, "Query failed: %s\n", query);
      goto bail_out;
   }

   m_status = PQresultStatus(m_result);
   if (m_status == PGRES_COPY_IN) {
      /*
       * How many fields in the set?
       */
      m_num_fields = (int) PQnfields(m_result);
      m_num_rows = 0;
      m_status = 1;
   } else {
      Dmsg1(50, "Result status failed: %s\n", query);
      goto bail_out;
   }

   Dmsg0(500, "sql_batch_start finishing\n");

   return true;

bail_out:
   Mmsg1(errmsg, _("error starting batch mode: %s"), PQerrorMessage(m_db_handle));
   m_status = 0;
   PQclear(m_result);
   m_result = NULL;
   return false;
}

/**
 * Set error to something to abort operation
 */
bool B_DB_POSTGRESQL::sql_batch_end(JCR *jcr, const char *error)
{
   int res;
   int count=30;
   PGresult *pg_result;

   Dmsg0(500, "sql_batch_end started\n");

   do {
      res = PQputCopyEnd(m_db_handle, error);
   } while (res == 0 && --count > 0);

   if (res == 1) {
      Dmsg0(500, "ok\n");
      m_status = 1;
   }

   if (res <= 0) {
      Dmsg0(500, "we failed\n");
      m_status = 0;
      Mmsg1(errmsg, _("error ending batch mode: %s"), PQerrorMessage(m_db_handle));
      Dmsg1(500, "failure %s\n", errmsg);
   }

   /*
    * Check command status and return to normal libpq state
    */
   pg_result = PQgetResult(m_db_handle);
   if (PQresultStatus(pg_result) != PGRES_COMMAND_OK) {
      Mmsg1(errmsg, _("error ending batch mode: %s"), PQerrorMessage(m_db_handle));
      m_status = 0;
   }

   PQclear(pg_result);

   Dmsg0(500, "sql_batch_end finishing\n");

   return true;
}

bool B_DB_POSTGRESQL::sql_batch_insert(JCR *jcr, ATTR_DBR *ar)
{
   int res;
   int count=30;
   size_t len;
   const char *digest;
   char ed1[50];

   esc_name = check_pool_memory_size(esc_name, fnl*2+1);
   pgsql_copy_escape(esc_name, fname, fnl);

   esc_path = check_pool_memory_size(esc_path, pnl*2+1);
   pgsql_copy_escape(esc_path, path, pnl);

   if (ar->Digest == NULL || ar->Digest[0] == 0) {
      digest = "0";
   } else {
      digest = ar->Digest;
   }

   len = Mmsg(cmd, "%u\t%s\t%s\t%s\t%s\t%s\t%u\n",
              ar->FileIndex, edit_int64(ar->JobId, ed1), esc_path,
              esc_name, ar->attr, digest, ar->DeltaSeq);

   do {
      res = PQputCopyData(m_db_handle, cmd, len);
   } while (res == 0 && --count > 0);

   if (res == 1) {
      Dmsg0(500, "ok\n");
      changes++;
      m_status = 1;
   }

   if (res <= 0) {
      Dmsg0(500, "we failed\n");
      m_status = 0;
      Mmsg1(errmsg, _("error copying in batch mode: %s"), PQerrorMessage(m_db_handle));
      Dmsg1(500, "failure %s\n", errmsg);
   }

   Dmsg0(500, "sql_batch_insert finishing\n");

   return true;
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
   B_DB_POSTGRESQL *mdb = NULL;

   if (!db_user) {
      Jmsg(jcr, M_FATAL, 0, _("A user name for PostgreSQL must be supplied.\n"));
      return NULL;
   }
   P(mutex);                          /* lock DB queue */

   if (!pgsql_query_table) {
      pgsql_query_table = load_query_table("postgresql");
      if (!pgsql_query_table) {
         goto bail_out;
      }
   }

   /*
    * Look to see if DB already open
    */
   if (db_list && !mult_db_connections && !need_private) {
      foreach_dlist(mdb, db_list) {
         if (mdb->is_private()) {
            continue;
         }

         if (mdb->match_database(db_driver, db_name, db_address, db_port)) {
            Dmsg1(100, "DB REopen %s\n", db_name);
            mdb->increment_refcount();
            goto bail_out;
         }
      }
   }
   Dmsg0(100, "db_init_database first time\n");
   mdb = New(B_DB_POSTGRESQL(jcr,
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
   if (pgsql_query_table) {
      unload_query_table(pgsql_query_table);
      pgsql_query_table = NULL;
   }
}

#endif /* HAVE_POSTGRESQL */
