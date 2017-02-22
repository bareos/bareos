/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2011 Free Software Foundation Europe e.V.
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
 * Kern Sibbald, March 2000
 * Major rewrite by Marco van Wieringen, January 2010 for catalog refactoring.
 */
/**
 * @file
 * BAREOS Catalog Database routines specific to MySQL
 * These are MySQL specific routines -- hopefully all
 * other files are generic.
 */

#include "bareos.h"

#ifdef HAVE_MYSQL

#include "cats.h"
#include <mysql.h>
#include <errmsg.h>
#include "bdb_mysql.h"

/* pull in the generated queries definitions */
#include "mysql_queries.inc"

/* -----------------------------------------------------------------------
 *
 *   MySQL dependent defines and subroutines
 *
 * -----------------------------------------------------------------------
 */

/**
 * List of open databases
 */
static dlist *db_list = NULL;

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

B_DB_MYSQL::B_DB_MYSQL(JCR *jcr,
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
                       bool need_private
                       )
{
   /*
    * Initialize the parent class members.
    */
   m_db_interface_type = SQL_INTERFACE_TYPE_MYSQL;
   m_db_type = SQL_TYPE_MYSQL;
   m_db_driver = bstrdup("MySQL");
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
# if defined(HAVE_MYSQL_THREAD_SAFE)
      m_have_batch_insert = mysql_thread_safe();
# else
      m_have_batch_insert = false;
# endif /* HAVE_MYSQL_THREAD_SAFE */
#else
      m_have_batch_insert = false;
#endif /* USE_BATCH_FILE_INSERT */
   }
   errmsg = get_pool_memory(PM_EMSG); /* get error message buffer */
   *errmsg = 0;
   cmd = get_pool_memory(PM_EMSG);    /* get command buffer */
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

   /* make the queries available using the queries variable from the parent class */
   queries = query_definitions;
}

B_DB_MYSQL::~B_DB_MYSQL()
{
}

/**
 * Now actually open the database.  This can generate errors,
 *  which are returned in the errmsg
 *
 * DO NOT close the database or delete mdb here !!!!
 */
bool B_DB_MYSQL::open_database(JCR *jcr)
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
      Mmsg1(errmsg, _("Unable to initialize DB lock. ERR=%s\n"), be.bstrerror(errstat));
      goto bail_out;
   }

   /*
    * Connect to the database
    */
#ifdef xHAVE_EMBEDDED_MYSQL
// mysql_server_init(0, NULL, NULL);
#endif
   mysql_init(&m_instance);

   Dmsg0(50, "mysql_init done\n");
   /*
    * If connection fails, try at 5 sec intervals for 30 seconds.
    */
   for (int retry=0; retry < 6; retry++) {
      m_db_handle = mysql_real_connect(&m_instance,        /* db */
                                       m_db_address,       /* default = localhost */
                                       m_db_user,          /* login name */
                                       m_db_password,      /* password */
                                       m_db_name,          /* database name */
                                       m_db_port,          /* default port */
                                       m_db_socket,        /* default = socket */
                                       CLIENT_FOUND_ROWS); /* flags */

      /*
       * If no connect, try once more in case it is a timing problem
       */
      if (m_db_handle != NULL) {
         break;
      }
      bmicrosleep(5,0);
   }

   m_instance.reconnect = 1;             /* so connection does not timeout */
   Dmsg0(50, "mysql_real_connect done\n");
   Dmsg3(50, "db_user=%s db_name=%s db_password=%s\n", m_db_user, m_db_name,
        (m_db_password == NULL) ? "(NULL)" : m_db_password);

   if (m_db_handle == NULL) {
      Mmsg2(errmsg, _("Unable to connect to MySQL server.\n"
                       "Database=%s User=%s\n"
                       "MySQL connect failed either server not running or your authorization is incorrect.\n"),
         m_db_name, m_db_user);
#if MYSQL_VERSION_ID >= 40101
      Dmsg3(50, "Error %u (%s): %s\n",
            mysql_errno(&(m_instance)), mysql_sqlstate(&(m_instance)),
            mysql_error(&(m_instance)));
#else
      Dmsg2(50, "Error %u: %s\n",
            mysql_errno(&(m_instance)), mysql_error(&(m_instance)));
#endif
      goto bail_out;
   }

   m_connected = true;
   if (!check_tables_version(jcr)) {
      goto bail_out;
   }

   Dmsg3(100, "opendb ref=%d connected=%d db=%p\n", m_ref_count, m_connected, m_db_handle);

   /*
    * Set connection timeout to 8 days specialy for batch mode
    */
   sql_query_without_handler("SET wait_timeout=691200");
   sql_query_without_handler("SET interactive_timeout=691200");

   retval = true;

bail_out:
   V(mutex);
   return retval;
}

void B_DB_MYSQL::close_database(JCR *jcr)
{
   if (m_connected) {
      end_transaction(jcr);
   }
   P(mutex);
   m_ref_count--;
   Dmsg3(100, "closedb ref=%d connected=%d db=%p\n", m_ref_count, m_connected, m_db_handle);
   if (m_ref_count == 0) {
      if (m_connected) {
         sql_free_result();
      }
      db_list->remove(this);
      if (m_connected) {
         Dmsg1(100, "close db=%p\n", m_db_handle);
         mysql_close(&m_instance);

#ifdef xHAVE_EMBEDDED_MYSQL
//       mysql_server_end();
#endif
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

bool B_DB_MYSQL::validate_connection(void)
{
   bool retval;
   unsigned long mysql_threadid;

   /*
    * See if the connection is still valid by using a ping to
    * the server. We also catch a changing threadid which means
    * a reconnect has taken place.
    */
   db_lock(this);
   mysql_threadid = mysql_thread_id(m_db_handle);
   if (mysql_ping(m_db_handle) == 0) {
      Dmsg2(500, "db_validate_connection connection valid previous threadid %ld new threadid %ld\n",
            mysql_threadid, mysql_thread_id(m_db_handle));

      if (mysql_thread_id(m_db_handle) != mysql_threadid) {
         mysql_query(m_db_handle, "SET wait_timeout=691200");
         mysql_query(m_db_handle, "SET interactive_timeout=691200");
      }

      retval = true;
      goto bail_out;
   } else {
      Dmsg0(500, "db_validate_connection connection invalid unable to ping server\n");
      retval = false;
      goto bail_out;
   }

bail_out:
   db_unlock(this);
   return retval;
}

/**
 * This call is needed because the message channel thread
 *  opens a database on behalf of a jcr that was created in
 *  a different thread. MySQL then allocates thread specific
 *  data, which is NOT freed when the original jcr thread
 *  closes the database.  Thus the msgchan must call here
 *  to cleanup any thread specific data that it created.
 */
void B_DB_MYSQL::thread_cleanup(void)
{
#ifndef HAVE_WIN32
   mysql_thread_end();
#endif
}

/**
 * Escape strings so that MySQL is happy
 *
 *   NOTE! len is the length of the old string. Your new
 *         string must be long enough (max 2*old+1) to hold
 *         the escaped output.
 */
void B_DB_MYSQL::escape_string(JCR *jcr, char *snew, char *old, int len)
{
   mysql_real_escape_string(m_db_handle, snew, old, len);
}

/**
 * Escape binary object so that MySQL is happy
 * Memory is stored in B_DB struct, no need to free it
 */
char *B_DB_MYSQL::escape_object(JCR *jcr, char *old, int len)
{
   esc_obj = check_pool_memory_size(esc_obj, len*2+1);
   mysql_real_escape_string(m_db_handle, esc_obj, old, len);
   return esc_obj;
}

/**
 * Unescape binary object so that MySQL is happy
 */
void B_DB_MYSQL::unescape_object(JCR *jcr, char *from, int32_t expected_len,
                                 POOLMEM *&dest, int32_t *dest_len)
{
   if (!from) {
      dest[0] = '\0';
      *dest_len = 0;
      return;
   }
   dest = check_pool_memory_size(dest, expected_len + 1);
   *dest_len = expected_len;
   memcpy(dest, from, expected_len);
   dest[expected_len] = '\0';
}

void B_DB_MYSQL::start_transaction(JCR *jcr)
{
   if (!jcr->attr) {
      jcr->attr = get_pool_memory(PM_FNAME);
   }
   if (!jcr->ar) {
      jcr->ar = (ATTR_DBR *)malloc(sizeof(ATTR_DBR));
   }
}

void B_DB_MYSQL::end_transaction(JCR *jcr)
{
   if (jcr && jcr->cached_attribute) {
      Dmsg0(400, "Flush last cached attribute.\n");
      if (!create_attributes_record(jcr, jcr->ar)) {
         Jmsg1(jcr, M_FATAL, 0, _("Attribute create error. %s"), strerror());
      }
      jcr->cached_attribute = false;
   }
}

/**
 * Submit a general SQL command (cmd), and for each row returned,
 * the result_handler is called with the ctx.
 */
bool B_DB_MYSQL::sql_query_with_handler(const char *query, DB_RESULT_HANDLER *result_handler, void *ctx)
{
   int status;
   SQL_ROW row;
   bool send = true;
   bool retry = true;
   bool retval = false;

   Dmsg1(500, "sql_query_with_handler starts with %s\n", query);

   db_lock(this);

retry_query:
   status = mysql_query(m_db_handle, query);

   switch (status) {
   case 0:
      break;
   case CR_SERVER_GONE_ERROR:
   case CR_SERVER_LOST:
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
            unsigned long mysql_threadid;

            mysql_threadid = mysql_thread_id(m_db_handle);
            if (mysql_ping(m_db_handle) == 0) {
               /*
                * See if the threadid changed e.g. new connection to the DB.
                */
               if (mysql_thread_id(m_db_handle) != mysql_threadid) {
                  mysql_query(m_db_handle, "SET wait_timeout=691200");
                  mysql_query(m_db_handle, "SET interactive_timeout=691200");
               }

               retry = false;
               goto retry_query;
            }
         }
      }

      /* FALL THROUGH */
   default:
      Mmsg(errmsg, _("Query failed: %s: ERR=%s\n"), query, sql_strerror());
      Dmsg0(500, "sql_query_with_handler failed\n");
      goto bail_out;
   }

   Dmsg0(500, "sql_query_with_handler succeeded. checking handler\n");

   if (result_handler != NULL) {
      if ((m_result = mysql_use_result(m_db_handle)) != NULL) {
         m_num_fields = mysql_num_fields(m_result);

         /*
          * We *must* fetch all rows
          */
         while ((row = mysql_fetch_row(m_result)) != NULL) {
            if (send) {
               /* the result handler returns 1 when it has
                *  seen all the data it wants.  However, we
                *  loop to the end of the data.
                */
               if (result_handler(ctx, m_num_fields, row)) {
                  send = false;
               }
            }
         }
         sql_free_result();
      }
   }

   Dmsg0(500, "sql_query_with_handler finished\n");
   retval = true;

bail_out:
   db_unlock(this);
   return retval;
}

bool B_DB_MYSQL::sql_query_without_handler(const char *query, int flags)
{
   int status;
   bool retry = true;
   bool retval = true;

   Dmsg1(500, "sql_query_without_handler starts with '%s'\n", query);

   /*
    * We are starting a new query. reset everything.
    */
retry_query:
   m_num_rows = -1;
   m_row_number = -1;
   m_field_number = -1;

   if (m_result) {
      mysql_free_result(m_result);
      m_result = NULL;
   }

   status = mysql_query(m_db_handle, query);
   switch (status) {
   case 0:
      Dmsg0(500, "we have a result\n");
      if (flags & QF_STORE_RESULT) {
         m_result = mysql_store_result(m_db_handle);
         if (m_result != NULL) {
            m_num_fields = mysql_num_fields(m_result);
            Dmsg1(500, "we have %d fields\n", m_num_fields);
            m_num_rows = mysql_num_rows(m_result);
            Dmsg1(500, "we have %d rows\n", m_num_rows);
         } else {
            m_num_fields = 0;
            m_num_rows = mysql_affected_rows(m_db_handle);
            Dmsg1(500, "we have %d rows\n", m_num_rows);
         }
      } else {
         m_num_fields = 0;
         m_num_rows = mysql_affected_rows(m_db_handle);
         Dmsg1(500, "we have %d rows\n", m_num_rows);
      }
      break;
   case CR_SERVER_GONE_ERROR:
   case CR_SERVER_LOST:
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
            unsigned long mysql_threadid;

            mysql_threadid = mysql_thread_id(m_db_handle);
            if (mysql_ping(m_db_handle) == 0) {
               /*
                * See if the threadid changed e.g. new connection to the DB.
                */
               if (mysql_thread_id(m_db_handle) != mysql_threadid) {
                  mysql_query(m_db_handle, "SET wait_timeout=691200");
                  mysql_query(m_db_handle, "SET interactive_timeout=691200");
               }

               retry = false;
               goto retry_query;
            }
         }
      }

      /* FALL THROUGH */
   default:
      Dmsg0(500, "we failed\n");
      m_status = 1;                   /* failed */
      retval = false;
      break;
   }

   return retval;
}

void B_DB_MYSQL::sql_free_result(void)
{
   db_lock(this);
   if (m_result) {
      mysql_free_result(m_result);
      m_result = NULL;
   }
   if (m_fields) {
      free(m_fields);
      m_fields = NULL;
   }
   m_num_rows = m_num_fields = 0;
   db_unlock(this);
}

SQL_ROW B_DB_MYSQL::sql_fetch_row(void)
{
   if (!m_result) {
      return NULL;
   } else {
      return mysql_fetch_row(m_result);
   }
}

const char *B_DB_MYSQL::sql_strerror(void)
{
   return mysql_error(m_db_handle);
}

void B_DB_MYSQL::sql_data_seek(int row)
{
   return mysql_data_seek(m_result, row);
}

int B_DB_MYSQL::sql_affected_rows(void)
{
   return mysql_affected_rows(m_db_handle);
}

uint64_t B_DB_MYSQL::sql_insert_autokey_record(const char *query, const char *table_name)
{
   /*
    * First execute the insert query and then retrieve the currval.
    */
   if (mysql_query(m_db_handle, query) != 0) {
      return 0;
   }

   m_num_rows = mysql_affected_rows(m_db_handle);
   if (m_num_rows != 1) {
      return 0;
   }

   changes++;

   return mysql_insert_id(m_db_handle);
}

SQL_FIELD *B_DB_MYSQL::sql_fetch_field(void)
{
   int i;
   MYSQL_FIELD *field;

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
         if ((field = mysql_fetch_field(m_result)) != NULL) {
            m_fields[i].name = field->name;
            m_fields[i].max_length = field->max_length;
            m_fields[i].type = field->type;
            m_fields[i].flags = field->flags;

            Dmsg4(500, "sql_fetch_field finds field '%s' has length='%d' type='%d' and IsNull=%d\n",
                  m_fields[i].name, m_fields[i].max_length, m_fields[i].type, m_fields[i].flags);
         }
      }
   }

   /*
    * Increment field number for the next time around
    */
   return &m_fields[m_field_number++];
}

bool B_DB_MYSQL::sql_field_is_not_null(int field_type)
{
   return IS_NOT_NULL(field_type);
}

bool B_DB_MYSQL::sql_field_is_numeric(int field_type)
{
   return IS_NUM(field_type);
}

/**
 * Returns true if OK
 *         false if failed
 */
bool B_DB_MYSQL::sql_batch_start(JCR *jcr)
{
   bool retval;

   db_lock(this);
   retval = sql_query("CREATE TEMPORARY TABLE batch ("
                              "FileIndex integer,"
                              "JobId integer,"
                              "Path blob,"
                              "Name blob,"
                              "LStat tinyblob,"
                              "MD5 tinyblob,"
                              "DeltaSeq integer)");
   db_unlock(this);

   /*
    * Keep track of the number of changes in batch mode.
    */
   changes = 0;

   return retval;
}

/* set error to something to abort operation */
/**
 * Returns true if OK
 *         false if failed
 */
bool B_DB_MYSQL::sql_batch_end(JCR *jcr, const char *error)
{
   m_status = 0;

   /*
    * Flush any pending inserts.
    */
   if (changes) {
      return sql_query(cmd);
   }

   return true;
}

/**
 * Returns true if OK
 *         false if failed
 */
bool B_DB_MYSQL::sql_batch_insert(JCR *jcr, ATTR_DBR *ar)
{
   const char *digest;
   char ed1[50];

   esc_name = check_pool_memory_size(esc_name, fnl*2+1);
   escape_string(jcr, esc_name, fname, fnl);

   esc_path = check_pool_memory_size(esc_path, pnl*2+1);
   escape_string(jcr, esc_path, path, pnl);

   if (ar->Digest == NULL || ar->Digest[0] == 0) {
      digest = "0";
   } else {
      digest = ar->Digest;
   }

   /*
    * Try to batch up multiple inserts using multi-row inserts.
    */
   if (changes == 0) {
      Mmsg(cmd, "INSERT INTO batch VALUES "
           "(%u,%s,'%s','%s','%s','%s',%u)",
           ar->FileIndex, edit_int64(ar->JobId,ed1), esc_path,
           esc_name, ar->attr, digest, ar->DeltaSeq);
      changes++;
   } else {
      /*
       * We use the esc_obj for temporary storage otherwise
       * we keep on copying data.
       */
      Mmsg(esc_obj, ",(%u,%s,'%s','%s','%s','%s',%u)",
           ar->FileIndex, edit_int64(ar->JobId,ed1), esc_path,
           esc_name, ar->attr, digest, ar->DeltaSeq);
      pm_strcat(cmd, esc_obj);
      changes++;
   }

   /*
    * See if we need to flush the query buffer filled
    * with multi-row inserts.
    */
   if ((changes % MYSQL_CHANGES_PER_BATCH_INSERT) == 0) {
      if (!sql_query(cmd)) {
         changes = 0;
         return false;
      } else {
         changes = 0;
      }
   }
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
   B_DB_MYSQL *mdb = NULL;

   if (!db_user) {
      Jmsg(jcr, M_FATAL, 0, _("A user name for MySQL must be supplied.\n"));
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

         if (mdb->match_database(db_driver, db_name, db_address, db_port)) {
            Dmsg1(100, "DB REopen %s\n", db_name);
            mdb->increment_refcount();
            goto bail_out;
         }
      }
   }
   Dmsg0(100, "db_init_database first time\n");
   mdb = New(B_DB_MYSQL(jcr,
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
                        need_private
                        ));

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

#endif /* HAVE_MYSQL */
