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
 * Kern Sibbald, January 2002
 * Major rewrite by Marco van Wieringen, January 2010 for catalog refactoring.
 */
/**
 * @file
 * BAREOS Catalog Database routines specific to SQLite
 */

#include "bareos.h"

#if HAVE_SQLITE3

#include "cats.h"
#include <sqlite3.h>
#include "bdb_sqlite.h"

#include <iostream>
/* -----------------------------------------------------------------------
 *
 *    SQLite dependent defines and subroutines
 *
 * -----------------------------------------------------------------------
 */

/*
 * List of open databases
 */
static dlist *db_list = NULL;

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

/*
 * When using mult_db_connections = true,
 * sqlite can be BUSY. We just need sleep a little in this case.
 */
static int sqlite_busy_handler(void *arg, int calls)
{
   bmicrosleep(0, 500);
   return 1;
}

B_DB_SQLITE::B_DB_SQLITE(JCR *jcr,
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
   m_db_interface_type = SQL_INTERFACE_TYPE_SQLITE3;
   m_db_type = SQL_TYPE_SQLITE3;
   m_db_driver = bstrdup("SQLite3");
   m_db_name = bstrdup(db_name);
   if (disable_batch_insert) {
      m_disabled_batch_insert = true;
      m_have_batch_insert = false;
   } else {
      m_disabled_batch_insert = false;
#if defined(USE_BATCH_FILE_INSERT)
#if defined(HAVE_SQLITE3_THREADSAFE)
      m_have_batch_insert = sqlite3_threadsafe();
#else
      m_have_batch_insert = false;
#endif /* HAVE_SQLITE3_THREADSAFE */
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
   esc_obj  = get_pool_memory(PM_FNAME);
   m_allow_transactions = mult_db_connections;
   m_is_private = need_private;
   m_try_reconnect = try_reconnect;
   m_exit_on_fatal = exit_on_fatal;

   /*
    * Initialize the private members.
    */
   m_db_handle = NULL;
   m_result = NULL;
   m_lowlevel_errmsg = NULL;

   /*
    * Put the db in the list.
    */
   if (db_list == NULL) {
      db_list = New(dlist(this, &this->m_link));
   }
   db_list->append(this);


   /* initialize query table */
   std::map <std::string, std::string>::iterator it;

   const char** qu = queries;

   for (const char **query = query_names; *query; ) {
      m_query_table.insert(std::make_pair(*query++, *qu++));
   }
}

B_DB_SQLITE::~B_DB_SQLITE()
{
}

/**
 * Now actually open the database.  This can generate errors,
 * which are returned in the errmsg
 *
 * DO NOT close the database or delete mdb here !!!!
 */
bool B_DB_SQLITE::open_database(JCR *jcr)
{
   bool retval = false;
   char *db_path;
   int len;
   struct stat statbuf;
   int status;
   int errstat;
   int retry = 0;

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
    * Open the database
    */
   len = strlen(working_directory) + strlen(m_db_name) + 5;
   db_path = (char *)malloc(len);
   strcpy(db_path, working_directory);
   strcat(db_path, "/");
   strcat(db_path, m_db_name);
   strcat(db_path, ".db");
   if (stat(db_path, &statbuf) != 0) {
      Mmsg1(errmsg, _("Database %s does not exist, please create it.\n"), db_path);
      free(db_path);
      goto bail_out;
   }

   for (m_db_handle = NULL; !m_db_handle && retry++ < 10; ) {
      status = sqlite3_open(db_path, &m_db_handle);
      if (status != SQLITE_OK) {
         m_lowlevel_errmsg = (char *)sqlite3_errmsg(m_db_handle);
         sqlite3_close(m_db_handle);
         m_db_handle = NULL;
      } else {
         m_lowlevel_errmsg = NULL;
      }

      Dmsg0(300, "sqlite_open\n");
      if (!m_db_handle) {
         bmicrosleep(1, 0);
      }
   }
   if (m_db_handle == NULL) {
      Mmsg2(errmsg, _("Unable to open Database=%s. ERR=%s\n"),
            db_path, m_lowlevel_errmsg ? m_lowlevel_errmsg : _("unknown"));
      free(db_path);
      goto bail_out;
   }
   m_connected = true;
   free(db_path);

   /*
    * Set busy handler to wait when we use mult_db_connections = true
    */
   sqlite3_busy_handler(m_db_handle, sqlite_busy_handler, NULL);

#if defined(SQLITE3_INIT_QUERY)
   sql_query_without_handler(SQLITE3_INIT_QUERY);
#endif

   if (!check_tables_version(jcr)) {
      goto bail_out;
   }

   retval = true;

bail_out:
   V(mutex);
   return retval;
}

void B_DB_SQLITE::close_database(JCR *jcr)
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
         sqlite3_close(m_db_handle);
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
      delete this;
      if (db_list->size() == 0) {
         delete db_list;
         db_list = NULL;
      }
   }
   V(mutex);
}

bool B_DB_SQLITE::validate_connection(void)
{
   bool retval;

   db_lock(this);
   if (sql_query_without_handler("SELECT 1", true)) {
      sql_free_result();
      retval = true;
      goto bail_out;
   } else {
      retval = false;
      goto bail_out;
   }

bail_out:
   db_unlock(this);
   return retval;
}

void B_DB_SQLITE::thread_cleanup(void)
{
   sqlite3_thread_cleanup();
}

/**
 * Start a transaction. This groups inserts and makes things
 * much more efficient. Usually started when inserting
 * file attributes.
 */
void B_DB_SQLITE::start_transaction(JCR *jcr)
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
   /*
    * Allow only 10,000 changes per transaction
    */
   if (m_transaction && changes > 10000) {
      end_transaction(jcr);
   }
   if (!m_transaction) {
      sql_query_without_handler("BEGIN");  /* begin transaction */
      Dmsg0(400, "Start SQLite transaction\n");
      m_transaction = true;
   }
   db_unlock(this);
}

void B_DB_SQLITE::end_transaction(JCR *jcr)
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
      Dmsg1(400, "End SQLite transaction changes=%d\n", changes);
   }
   changes = 0;
   db_unlock(this);
}

struct rh_data {
   B_DB_SQLITE *mdb;
   DB_RESULT_HANDLER *result_handler;
   void *ctx;
   bool initialized;
};

/**
 * Convert SQLite's callback into BAREOS DB callback
 */
static int sqlite_result_handler(void *arh_data, int num_fields, char **rows, char **col_names)
{
   struct rh_data *rh_data = (struct rh_data *)arh_data;

   /*
    * The sql_query_with_handler doesn't have access to m_results,
    * so if we wan't to get fields information, we need to use col_names
    */
   if (!rh_data->initialized) {
      rh_data->mdb->set_column_names(col_names, num_fields);
      rh_data->initialized = true;
   }
   if (rh_data->result_handler) {
      (*(rh_data->result_handler))(rh_data->ctx, num_fields, rows);
   }

   return 0;
}

/**
 * Submit a general SQL command (cmd), and for each row returned,
 * the result_handler is called with the ctx.
 */
bool B_DB_SQLITE::sql_query_with_handler(const char *query, DB_RESULT_HANDLER *result_handler, void *ctx)
{
   bool retval = false;
   int status;
   struct rh_data rh_data;

   Dmsg1(500, "sql_query_with_handler starts with '%s'\n", query);

   db_lock(this);
   if (m_lowlevel_errmsg) {
      sqlite3_free(m_lowlevel_errmsg);
      m_lowlevel_errmsg = NULL;
   }
   sql_free_result();

   rh_data.ctx = ctx;
   rh_data.mdb = this;
   rh_data.initialized = false;
   rh_data.result_handler = result_handler;

   status = sqlite3_exec(m_db_handle, query, sqlite_result_handler,
                         (void *)&rh_data, &m_lowlevel_errmsg);

   if (status != SQLITE_OK) {
      Mmsg(errmsg, _("Query failed: %s: ERR=%s\n"), query, sql_strerror());
      Dmsg0(500, "sql_query_with_handler finished\n");
      goto bail_out;
   }
   Dmsg0(500, "db_sql_query finished\n");
   sql_free_result();
   retval = true;

bail_out:
   db_unlock(this);
   return retval;
}

/**
 * Submit a sqlite query and retrieve all the data
 */
bool B_DB_SQLITE::sql_query_without_handler(const char *query, int flags)
{
   int status;
   bool retval = false;

   Dmsg1(500, "sql_query_without_handler starts with '%s'\n", query);

   sql_free_result();
   if (m_lowlevel_errmsg) {
      sqlite3_free(m_lowlevel_errmsg);
      m_lowlevel_errmsg = NULL;
   }

   status = sqlite3_get_table(m_db_handle, (char *)query, &m_result,
                              &m_num_rows, &m_num_fields, &m_lowlevel_errmsg);

   m_row_number = 0;               /* no row fetched */
   if (status != 0) {                   /* something went wrong */
      m_num_rows = m_num_fields = 0;
      Dmsg0(500, "sql_query_without_handler finished\n");
   } else {
      Dmsg0(500, "sql_query_without_handler finished\n");
      retval = true;
   }
   return retval;
}

void B_DB_SQLITE::sql_free_result(void)
{
   db_lock(this);
   if (m_fields) {
      free(m_fields);
      m_fields = NULL;
   }
   if (m_result) {
      sqlite3_free_table(m_result);
      m_result = NULL;
   }
   m_col_names = NULL;
   m_num_rows = m_num_fields = 0;
   db_unlock(this);
}

/**
 * Fetch one row at a time
 */
SQL_ROW B_DB_SQLITE::sql_fetch_row(void)
{
   if (!m_result || (m_row_number >= m_num_rows)) {
      return NULL;
   }
   m_row_number++;
   return &m_result[m_num_fields * m_row_number];
}

const char *B_DB_SQLITE::sql_strerror(void)
{
   return m_lowlevel_errmsg ? m_lowlevel_errmsg : "unknown";
}

void B_DB_SQLITE::sql_data_seek(int row)
{
   /*
    * Set the row number to be returned on the next call to sql_fetch_row
    */
   m_row_number = row;
}

int B_DB_SQLITE::sql_affected_rows(void)
{
   return sqlite3_changes(m_db_handle);
}

uint64_t B_DB_SQLITE::sql_insert_autokey_record(const char *query, const char *table_name)
{
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

   return sqlite3_last_insert_rowid(m_db_handle);
}

SQL_FIELD *B_DB_SQLITE::sql_fetch_field(void)
{
   int i, j, len;

   /* We are in the middle of a db_sql_query and we want to get fields info */
   if (m_col_names != NULL) {
      if (m_num_fields > m_field_number) {
         m_sql_field.name = m_col_names[m_field_number];
         /* We don't have the maximum field length, so we can use 80 as
          * estimation.
          */
         len = MAX(cstrlen(m_sql_field.name), 80/m_num_fields);
         m_sql_field.max_length = len;

         m_field_number++;
         m_sql_field.type = 0;  /* not numeric */
         m_sql_field.flags = 1; /* not null */
         return &m_sql_field;
      } else {                  /* too much fetch_field() */
         return NULL;
      }
   }

   /* We are after a sql_query() that stores the result in m_results */
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
         m_fields[i].name = m_result[i];
         m_fields[i].max_length = cstrlen(m_fields[i].name);
         for (j = 1; j <= m_num_rows; j++) {
            if (m_result[i + m_num_fields * j]) {
               len = (uint32_t)cstrlen(m_result[i + m_num_fields * j]);
            } else {
               len = 0;
            }
            if (len > m_fields[i].max_length) {
               m_fields[i].max_length = len;
            }
         }
         m_fields[i].type = 0;
         m_fields[i].flags = 1;        /* not null */

         Dmsg4(500, "sql_fetch_field finds field '%s' has length='%d' type='%d' and IsNull=%d\n",
               m_fields[i].name, m_fields[i].max_length, m_fields[i].type, m_fields[i].flags);
      }
   }

   /*
    * Increment field number for the next time around
    */
   return &m_fields[m_field_number++];
}

bool B_DB_SQLITE::sql_field_is_not_null(int field_type)
{
   switch (field_type) {
   case 1:
      return true;
   default:
      return false;
   }
}

bool B_DB_SQLITE::sql_field_is_numeric(int field_type)
{
   switch (field_type) {
   case 1:
      return true;
   default:
      return false;
   }
}

/**
 * Returns true if OK
 *         false if failed
 */
bool B_DB_SQLITE::sql_batch_start(JCR *jcr)
{
   bool retval;

   db_lock(this);
   retval = sql_query_without_handler("CREATE TEMPORARY TABLE batch ("
                                      "FileIndex integer,"
                                      "JobId integer,"
                                      "Path blob,"
                                      "Name blob,"
                                      "LStat tinyblob,"
                                      "MD5 tinyblob,"
                                      "DeltaSeq integer)");
   db_unlock(this);

   return retval;
}

/* set error to something to abort operation */
/**
 * Returns true if OK
 *         false if failed
 */
bool B_DB_SQLITE::sql_batch_end(JCR *jcr, const char *error)
{
   m_status = 0;

   return true;
}

/**
 * Returns true if OK
 *         false if failed
 */
bool B_DB_SQLITE::sql_batch_insert(JCR *jcr, ATTR_DBR *ar)
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

   Mmsg(cmd, "INSERT INTO batch VALUES "
        "(%u,%s,'%s','%s','%s','%s',%u)",
        ar->FileIndex, edit_int64(ar->JobId,ed1), esc_path,
        esc_name, ar->attr, digest, ar->DeltaSeq);

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
   B_DB *mdb = NULL;

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
            Dmsg1(300, "DB REopen %s\n", db_name);
            mdb->increment_refcount();
            goto bail_out;
         }
      }
   }
   Dmsg0(300, "db_init_database first time\n");
   mdb = New(B_DB_SQLITE(jcr,
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

#endif /* HAVE_SQLITE3 */
