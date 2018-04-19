/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2016 Planets Communications B.V.
   Copyright (C) 2013-2017 Bareos GmbH & Co. KG

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

#include "include/bareos.h"

#if HAVE_SQLITE3

#include "cats.h"
#include <sqlite3.h>
#include "bdb_sqlite.h"

/* pull in the generated queries definitions */
#include "sqlite_queries.inc"
#include "lib/edit.h"

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

BareosDbSqlite::BareosDbSqlite(JobControlRecord *jcr,
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
   db_interface_type_ = SQL_INTERFACE_TYPE_SQLITE3;
   db_type_ = SQL_TYPE_SQLITE3;
   db_driver_ = bstrdup("SQLite3");
   db_name_ = bstrdup(db_name);
   if (disable_batch_insert) {
      disabled_batch_insert_ = true;
      have_batch_insert_ = false;
   } else {
      disabled_batch_insert_ = false;
#if defined(USE_BATCH_FILE_INSERT)
#if defined(HAVE_SQLITE3_THREADSAFE)
      have_batch_insert_ = sqlite3_threadsafe();
#else
      have_batch_insert_ = false;
#endif /* HAVE_SQLITE3_THREADSAFE */
#else
      have_batch_insert_ = false;
#endif /* USE_BATCH_FILE_INSERT */
   }
   errmsg = get_pool_memory(PM_EMSG); /* get error message buffer */
   *errmsg = 0;
   cmd = get_pool_memory(PM_EMSG);    /* get command buffer */
   cached_path = get_pool_memory(PM_FNAME);
   cached_path_id = 0;
   ref_count_ = 1;
   fname = get_pool_memory(PM_FNAME);
   path = get_pool_memory(PM_FNAME);
   esc_name = get_pool_memory(PM_FNAME);
   esc_path = get_pool_memory(PM_FNAME);
   esc_obj  = get_pool_memory(PM_FNAME);
   allow_transactions_ = mult_db_connections;
   is_private_ = need_private;
   try_reconnect_ = try_reconnect;
   exit_on_fatal_ = exit_on_fatal;

   /*
    * Initialize the private members.
    */
   db_handle_ = NULL;
   result_ = NULL;
   lowlevel_errmsg_ = NULL;

   /*
    * Put the db in the list.
    */
   if (db_list == NULL) {
      db_list = New(dlist(this, &this->link_));
   }
   db_list->append(this);

   /* make the queries available using the queries variable from the parent class */
   queries = query_definitions;
}

BareosDbSqlite::~BareosDbSqlite()
{
}

/**
 * Now actually open the database.  This can generate errors,
 * which are returned in the errmsg
 *
 * DO NOT close the database or delete mdb here !!!!
 */
bool BareosDbSqlite::open_database(JobControlRecord *jcr)
{
   bool retval = false;
   char *db_path;
   int len;
   struct stat statbuf;
   int status;
   int errstat;
   int retry = 0;

   P(mutex);
   if (connected_) {
      retval = true;
      goto bail_out;
   }

   if ((errstat=rwl_init(&lock_)) != 0) {
      berrno be;
      Mmsg1(errmsg, _("Unable to initialize DB lock. ERR=%s\n"), be.bstrerror(errstat));
      goto bail_out;
   }

   /*
    * Open the database
    */
   len = strlen(working_directory) + strlen(db_name_) + 5;
   db_path = (char *)malloc(len);
   strcpy(db_path, working_directory);
   strcat(db_path, "/");
   strcat(db_path, db_name_);
   strcat(db_path, ".db");
   if (stat(db_path, &statbuf) != 0) {
      Mmsg1(errmsg, _("Database %s does not exist, please create it.\n"), db_path);
      free(db_path);
      goto bail_out;
   }

   for (db_handle_ = NULL; !db_handle_ && retry++ < 10; ) {
      status = sqlite3_open(db_path, &db_handle_);
      if (status != SQLITE_OK) {
         lowlevel_errmsg_ = (char *)sqlite3_errmsg(db_handle_);
         sqlite3_close(db_handle_);
         db_handle_ = NULL;
      } else {
         lowlevel_errmsg_ = NULL;
      }

      Dmsg0(300, "sqlite_open\n");
      if (!db_handle_) {
         bmicrosleep(1, 0);
      }
   }
   if (db_handle_ == NULL) {
      Mmsg2(errmsg, _("Unable to open Database=%s. ERR=%s\n"),
            db_path, lowlevel_errmsg_ ? lowlevel_errmsg_ : _("unknown"));
      free(db_path);
      goto bail_out;
   }
   connected_ = true;
   free(db_path);

   /*
    * Set busy handler to wait when we use mult_db_connections = true
    */
   sqlite3_busy_handler(db_handle_, sqlite_busy_handler, NULL);

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

void BareosDbSqlite::close_database(JobControlRecord *jcr)
{
   if (connected_) {
      end_transaction(jcr);
   }
   P(mutex);
   ref_count_--;
   if (ref_count_ == 0) {
      if (connected_) {
         sql_free_result();
      }
      db_list->remove(this);
      if (connected_ && db_handle_) {
         sqlite3_close(db_handle_);
      }
      if (rwl_is_init(&lock_)) {
         rwl_destroy(&lock_);
      }
      free_pool_memory(errmsg);
      free_pool_memory(cmd);
      free_pool_memory(cached_path);
      free_pool_memory(fname);
      free_pool_memory(path);
      free_pool_memory(esc_name);
      free_pool_memory(esc_path);
      free_pool_memory(esc_obj);
      if (db_driver_) {
         free(db_driver_);
      }
      if (db_name_) {
         free(db_name_);
      }
      delete this;
      if (db_list->size() == 0) {
         delete db_list;
         db_list = NULL;
      }
   }
   V(mutex);
}

bool BareosDbSqlite::validate_connection(void)
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

void BareosDbSqlite::thread_cleanup(void)
{
   sqlite3_thread_cleanup();
}

/**
 * Start a transaction. This groups inserts and makes things
 * much more efficient. Usually started when inserting
 * file attributes.
 */
void BareosDbSqlite::start_transaction(JobControlRecord *jcr)
{
   if (!jcr->attr) {
      jcr->attr = get_pool_memory(PM_FNAME);
   }

   if (!jcr->ar) {
      jcr->ar = (AttributesDbRecord *)malloc(sizeof(AttributesDbRecord));
      jcr->ar->Digest = NULL;
   }

   if (!allow_transactions_) {
      return;
   }

   db_lock(this);
   /*
    * Allow only 10,000 changes per transaction
    */
   if (transaction_ && changes > 10000) {
      end_transaction(jcr);
   }
   if (!transaction_) {
      sql_query_without_handler("BEGIN");  /* begin transaction */
      Dmsg0(400, "Start SQLite transaction\n");
      transaction_ = true;
   }
   db_unlock(this);
}

void BareosDbSqlite::end_transaction(JobControlRecord *jcr)
{
   if (jcr && jcr->cached_attribute) {
      Dmsg0(400, "Flush last cached attribute.\n");
      if (!create_attributes_record(jcr, jcr->ar)) {
         Jmsg1(jcr, M_FATAL, 0, _("Attribute create error. %s"), strerror());
      }
      jcr->cached_attribute = false;
   }

   if (!allow_transactions_) {
      return;
   }

   db_lock(this);
   if (transaction_) {
      sql_query_without_handler("COMMIT"); /* end transaction */
      transaction_ = false;
      Dmsg1(400, "End SQLite transaction changes=%d\n", changes);
   }
   changes = 0;
   db_unlock(this);
}

struct rh_data {
   BareosDbSqlite *mdb;
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
    * The sql_query_with_handler doesn't have access to results_,
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
bool BareosDbSqlite::sql_query_with_handler(const char *query, DB_RESULT_HANDLER *result_handler, void *ctx)
{
   bool retval = false;
   int status;
   struct rh_data rh_data;

   Dmsg1(500, "sql_query_with_handler starts with '%s'\n", query);

   db_lock(this);
   if (lowlevel_errmsg_) {
      sqlite3_free(lowlevel_errmsg_);
      lowlevel_errmsg_ = NULL;
   }
   sql_free_result();

   rh_data.ctx = ctx;
   rh_data.mdb = this;
   rh_data.initialized = false;
   rh_data.result_handler = result_handler;

   status = sqlite3_exec(db_handle_, query, sqlite_result_handler,
                         (void *)&rh_data, &lowlevel_errmsg_);

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
bool BareosDbSqlite::sql_query_without_handler(const char *query, int flags)
{
   int status;
   bool retval = false;

   Dmsg1(500, "sql_query_without_handler starts with '%s'\n", query);

   sql_free_result();
   if (lowlevel_errmsg_) {
      sqlite3_free(lowlevel_errmsg_);
      lowlevel_errmsg_ = NULL;
   }

   status = sqlite3_get_table(db_handle_, (char *)query, &result_,
                              &num_rows_, &num_fields_, &lowlevel_errmsg_);

   row_number_ = 0;               /* no row fetched */
   if (status != 0) {                   /* something went wrong */
      num_rows_ = num_fields_ = 0;
      Dmsg0(500, "sql_query_without_handler finished\n");
   } else {
      Dmsg0(500, "sql_query_without_handler finished\n");
      retval = true;
   }
   return retval;
}

void BareosDbSqlite::sql_free_result(void)
{
   db_lock(this);
   if (fields_) {
      free(fields_);
      fields_ = NULL;
   }
   if (result_) {
      sqlite3_free_table(result_);
      result_ = NULL;
   }
   col_names_ = NULL;
   num_rows_ = num_fields_ = 0;
   db_unlock(this);
}

/**
 * Fetch one row at a time
 */
SQL_ROW BareosDbSqlite::sql_fetch_row(void)
{
   if (!result_ || (row_number_ >= num_rows_)) {
      return NULL;
   }
   row_number_++;
   return &result_[num_fields_ * row_number_];
}

const char *BareosDbSqlite::sql_strerror(void)
{
   return lowlevel_errmsg_ ? lowlevel_errmsg_ : "unknown";
}

void BareosDbSqlite::sql_data_seek(int row)
{
   /*
    * Set the row number to be returned on the next call to sql_fetch_row
    */
   row_number_ = row;
}

int BareosDbSqlite::sql_affected_rows(void)
{
   return sqlite3_changes(db_handle_);
}

uint64_t BareosDbSqlite::sql_insert_autokey_record(const char *query, const char *table_name)
{
   /*
    * First execute the insert query and then retrieve the currval.
    */
   if (!sql_query_without_handler(query)) {
      return 0;
   }

   num_rows_ = sql_affected_rows();
   if (num_rows_ != 1) {
      return 0;
   }

   changes++;

   return sqlite3_last_insert_rowid(db_handle_);
}

SQL_FIELD *BareosDbSqlite::sql_fetch_field(void)
{
   int i, j, len;

   /* We are in the middle of a db_sql_query and we want to get fields info */
   if (col_names_ != NULL) {
      if (num_fields_ > field_number_) {
         sql_field_.name = col_names_[field_number_];
         /* We don't have the maximum field length, so we can use 80 as
          * estimation.
          */
         len = MAX(cstrlen(sql_field_.name), 80/num_fields_);
         sql_field_.max_length = len;

         field_number_++;
         sql_field_.type = 0;  /* not numeric */
         sql_field_.flags = 1; /* not null */
         return &sql_field_;
      } else {                  /* too much fetch_field() */
         return NULL;
      }
   }

   /* We are after a sql_query() that stores the result in results_ */
   if (!fields_ || fields_size_ < num_fields_) {
      if (fields_) {
         free(fields_);
         fields_ = NULL;
      }
      Dmsg1(500, "allocating space for %d fields\n", num_fields_);
      fields_ = (SQL_FIELD *)malloc(sizeof(SQL_FIELD) * num_fields_);
      fields_size_ = num_fields_;

      for (i = 0; i < num_fields_; i++) {
         Dmsg1(500, "filling field %d\n", i);
         fields_[i].name = result_[i];
         fields_[i].max_length = cstrlen(fields_[i].name);
         for (j = 1; j <= num_rows_; j++) {
            if (result_[i + num_fields_ * j]) {
               len = (uint32_t)cstrlen(result_[i + num_fields_ * j]);
            } else {
               len = 0;
            }
            if (len > fields_[i].max_length) {
               fields_[i].max_length = len;
            }
         }
         fields_[i].type = 0;
         fields_[i].flags = 1;        /* not null */

         Dmsg4(500, "sql_fetch_field finds field '%s' has length='%d' type='%d' and IsNull=%d\n",
               fields_[i].name, fields_[i].max_length, fields_[i].type, fields_[i].flags);
      }
   }

   /*
    * Increment field number for the next time around
    */
   return &fields_[field_number_++];
}

bool BareosDbSqlite::sql_field_is_not_null(int field_type)
{
   switch (field_type) {
   case 1:
      return true;
   default:
      return false;
   }
}

bool BareosDbSqlite::sql_field_is_numeric(int field_type)
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
bool BareosDbSqlite::sql_batch_start(JobControlRecord *jcr)
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
                                      "DeltaSeq integer,"
                                      "Fhinfo TEXT,"
                                      "Fhnode TEXT "
                                      ")"
         );
   db_unlock(this);

   return retval;
}

/* set error to something to abort operation */
/**
 * Returns true if OK
 *         false if failed
 */
bool BareosDbSqlite::sql_batch_end(JobControlRecord *jcr, const char *error)
{
   status_ = 0;

   return true;
}

/**
 * Returns true if OK
 *         false if failed
 */
bool BareosDbSqlite::sql_batch_insert(JobControlRecord *jcr, AttributesDbRecord *ar)
{
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

   Mmsg(cmd, "INSERT INTO batch VALUES "
        "(%u,%s,'%s','%s','%s','%s',%u,'%s','%s')",
        ar->FileIndex, edit_int64(ar->JobId,ed1), esc_path,
        esc_name, ar->attr, digest, ar->DeltaSeq,
        edit_uint64(ar->Fhinfo,ed2),
        edit_uint64(ar->Fhnode,ed3));

   return sql_query_without_handler(cmd);
}

/**
 * Initialize database data structure. In principal this should
 * never have errors, or it is really fatal.
 */
#ifdef HAVE_DYNAMIC_CATS_BACKENDS
extern "C" BareosDb CATS_IMP_EXP *backend_instantiate(JobControlRecord *jcr,
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
BareosDb *db_init_database(JobControlRecord *jcr,
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
   BareosDb *mdb = NULL;

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
   mdb = New(BareosDbSqlite(jcr,
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
