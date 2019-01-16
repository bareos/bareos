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

#include "include/bareos.h"

#ifdef HAVE_MYSQL

#include "cats.h"
#include <mysql.h>
#include <errmsg.h>
#include "bdb_mysql.h"
#include "lib/edit.h"
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

BareosDbMysql::BareosDbMysql(JobControlRecord *jcr,
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
   db_interface_type_ = SQL_INTERFACE_TYPE_MYSQL;
   db_type_ = SQL_TYPE_MYSQL;
   db_driver_ = bstrdup("MySQL");
   db_name_ = bstrdup(db_name);
   db_user_ = bstrdup(db_user);
   if (db_password) {
      db_password_ = bstrdup(db_password);
   }
   if (db_address) {
      db_address_ = bstrdup(db_address);
   }
   if (db_socket) {
      db_socket_ = bstrdup(db_socket);
   }
   db_port_ = db_port;

   if (disable_batch_insert) {
      disabled_batch_insert_ = true;
      have_batch_insert_ = false;
   } else {
      disabled_batch_insert_ = false;
#if defined(USE_BATCH_FILE_INSERT)
# if defined(HAVE_MYSQL_THREAD_SAFE)
      have_batch_insert_ = mysql_thread_safe();
# else
      have_batch_insert_ = false;
# endif /* HAVE_MYSQL_THREAD_SAFE */
#else
      have_batch_insert_ = false;
#endif /* USE_BATCH_FILE_INSERT */
   }
   errmsg = GetPoolMemory(PM_EMSG); /* get error message buffer */
   *errmsg = 0;
   cmd = GetPoolMemory(PM_EMSG);    /* get command buffer */
   cached_path = GetPoolMemory(PM_FNAME);
   cached_path_id = 0;
   ref_count_ = 1;
   fname = GetPoolMemory(PM_FNAME);
   path = GetPoolMemory(PM_FNAME);
   esc_name = GetPoolMemory(PM_FNAME);
   esc_path = GetPoolMemory(PM_FNAME);
   esc_obj = GetPoolMemory(PM_FNAME);
   allow_transactions_ = mult_db_connections;
   is_private_ = need_private;
   try_reconnect_ = try_reconnect;
   exit_on_fatal_ = exit_on_fatal;
   last_hash_key_ = 0;
   last_query_text_ = NULL;

   /*
    * Initialize the private members.
    */
   db_handle_ = NULL;
   result_ = NULL;

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

BareosDbMysql::~BareosDbMysql()
{
}

/**
 * Now actually open the database.  This can generate errors,
 *  which are returned in the errmsg
 *
 * DO NOT close the database or delete mdb here !!!!
 */
bool BareosDbMysql::OpenDatabase(JobControlRecord *jcr)
{
   bool retval = false;
   int errstat;
   my_bool reconnect = 1;

   P(mutex);
   if (connected_) {
      retval = true;
      goto bail_out;
   }

   if ((errstat=RwlInit(&lock_)) != 0) {
      BErrNo be;
      Mmsg1(errmsg, _("Unable to initialize DB lock. ERR=%s\n"), be.bstrerror(errstat));
      goto bail_out;
   }

   /*
    * Connect to the database
    */
#ifdef xHAVE_EMBEDDED_MYSQL
// mysql_server_init(0, NULL, NULL);
#endif
   mysql_init(&instance_);

   Dmsg0(50, "mysql_init done\n");
   /*
    * If connection fails, try at 5 sec intervals for 30 seconds.
    */
   for (int retry=0; retry < 6; retry++) {
      db_handle_ = mysql_real_connect(&instance_,        /* db */
                                       db_address_,       /* default = localhost */
                                       db_user_,          /* login name */
                                       db_password_,      /* password */
                                       db_name_,          /* database name */
                                       db_port_,          /* default port */
                                       db_socket_,        /* default = socket */
                                       CLIENT_FOUND_ROWS); /* flags */

      /*
       * If no connect, try once more in case it is a timing problem
       */
      if (db_handle_ != NULL) {
         break;
      }
      Bmicrosleep(5,0);
   }

   mysql_options(&instance_, MYSQL_OPT_RECONNECT, &reconnect);    /* so connection does not timeout */
   Dmsg0(50, "mysql_real_connect done\n");
   Dmsg3(50, "db_user=%s db_name=%s db_password=%s\n", db_user_, db_name_,
        (db_password_ == NULL) ? "(NULL)" : db_password_);

   if (db_handle_ == NULL) {
      Mmsg2(errmsg, _("Unable to connect to MySQL server.\n"
                       "Database=%s User=%s\n"
                       "MySQL connect failed either server not running or your authorization is incorrect.\n"),
         db_name_, db_user_);
#if MYSQL_VERSION_ID >= 40101
      Dmsg3(50, "Error %u (%s): %s\n",
            mysql_errno(&(instance_)), mysql_sqlstate(&(instance_)),
            mysql_error(&(instance_)));
#else
      Dmsg2(50, "Error %u: %s\n",
            mysql_errno(&(instance_)), mysql_error(&(instance_)));
#endif
      goto bail_out;
   }

   connected_ = true;
   if (!CheckTablesVersion(jcr)) {
      goto bail_out;
   }

   Dmsg3(100, "opendb ref=%d connected=%d db=%p\n", ref_count_, connected_, db_handle_);

   /*
    * Set connection timeout to 8 days specialy for batch mode
    */
   SqlQueryWithoutHandler("SET wait_timeout=691200");
   SqlQueryWithoutHandler("SET interactive_timeout=691200");

   retval = true;

bail_out:
   V(mutex);
   return retval;
}

void BareosDbMysql::CloseDatabase(JobControlRecord *jcr)
{
   if (connected_) {
      EndTransaction(jcr);
   }
   P(mutex);
   ref_count_--;
   Dmsg3(100, "closedb ref=%d connected=%d db=%p\n", ref_count_, connected_, db_handle_);
   if (ref_count_ == 0) {
      if (connected_) {
         SqlFreeResult();
      }
      db_list->remove(this);
      if (connected_) {
         Dmsg1(100, "close db=%p\n", db_handle_);
         mysql_close(&instance_);

#ifdef xHAVE_EMBEDDED_MYSQL
//       mysql_server_end();
#endif
      }
      if (RwlIsInit(&lock_)) {
         RwlDestroy(&lock_);
      }
      FreePoolMemory(errmsg);
      FreePoolMemory(cmd);
      FreePoolMemory(cached_path);
      FreePoolMemory(fname);
      FreePoolMemory(path);
      FreePoolMemory(esc_name);
      FreePoolMemory(esc_path);
      FreePoolMemory(esc_obj);
      if (db_driver_) {
         free(db_driver_);
      }
      if (db_name_) {
         free(db_name_);
      }
      if (db_user_) {
         free(db_user_);
      }
      if (db_password_) {
         free(db_password_);
      }
      if (db_address_) {
         free(db_address_);
      }
      if (db_socket_) {
         free(db_socket_);
      }
      delete this;
      if (db_list->size() == 0) {
         delete db_list;
         db_list = NULL;
      }
   }
   V(mutex);
}

bool BareosDbMysql::ValidateConnection(void)
{
   bool retval;
   unsigned long mysql_threadid;

   /*
    * See if the connection is still valid by using a ping to
    * the server. We also catch a changing threadid which means
    * a reconnect has taken place.
    */
   DbLock(this);
   mysql_threadid = mysql_thread_id(db_handle_);
   if (mysql_ping(db_handle_) == 0) {
      Dmsg2(500, "db_validate_connection connection valid previous threadid %ld new threadid %ld\n",
            mysql_threadid, mysql_thread_id(db_handle_));

      if (mysql_thread_id(db_handle_) != mysql_threadid) {
         mysql_query(db_handle_, "SET wait_timeout=691200");
         mysql_query(db_handle_, "SET interactive_timeout=691200");
      }

      retval = true;
      goto bail_out;
   } else {
      Dmsg0(500, "db_validate_connection connection invalid unable to ping server\n");
      retval = false;
      goto bail_out;
   }

bail_out:
   DbUnlock(this);
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
void BareosDbMysql::ThreadCleanup(void)
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
void BareosDbMysql::EscapeString(JobControlRecord *jcr, char *snew, char *old, int len)
{
   mysql_real_escape_string(db_handle_, snew, old, len);
}

/**
 * Escape binary object so that MySQL is happy
 * Memory is stored in BareosDb struct, no need to free it
 */
char *BareosDbMysql::EscapeObject(JobControlRecord *jcr, char *old, int len)
{
   esc_obj = CheckPoolMemorySize(esc_obj, len*2+1);
   mysql_real_escape_string(db_handle_, esc_obj, old, len);
   return esc_obj;
}

/**
 * Unescape binary object so that MySQL is happy
 */
void BareosDbMysql::UnescapeObject(JobControlRecord *jcr, char *from, int32_t expected_len,
                                 POOLMEM *&dest, int32_t *dest_len)
{
   if (!from) {
      dest[0] = '\0';
      *dest_len = 0;
      return;
   }
   dest = CheckPoolMemorySize(dest, expected_len + 1);
   *dest_len = expected_len;
   memcpy(dest, from, expected_len);
   dest[expected_len] = '\0';
}

void BareosDbMysql::StartTransaction(JobControlRecord *jcr)
{
   if (!jcr->attr) {
      jcr->attr = GetPoolMemory(PM_FNAME);
   }
   if (!jcr->ar) {
      jcr->ar = (AttributesDbRecord *)malloc(sizeof(AttributesDbRecord));
   }
}

void BareosDbMysql::EndTransaction(JobControlRecord *jcr)
{
   if (jcr && jcr->cached_attribute) {
      Dmsg0(400, "Flush last cached attribute.\n");
      if (!CreateAttributesRecord(jcr, jcr->ar)) {
         Jmsg1(jcr, M_FATAL, 0, _("Attribute create error. %s"), strerror());
      }
      jcr->cached_attribute = false;
   }
}

/**
 * Submit a general SQL command (cmd), and for each row returned,
 * the ResultHandler is called with the ctx.
 */
bool BareosDbMysql::SqlQueryWithHandler(const char *query, DB_RESULT_HANDLER *ResultHandler, void *ctx)
{
   int status;
   SQL_ROW row;
   bool send = true;
   bool retry = true;
   bool retval = false;

   Dmsg1(500, "SqlQueryWithHandler starts with %s\n", query);

   DbLock(this);

retry_query:
   status = mysql_query(db_handle_, query);

   switch (status) {
   case 0:
      break;
   case CR_SERVER_GONE_ERROR:
   case CR_SERVER_LOST:
      if (exit_on_fatal_) {
         /*
          * Any fatal error should result in the daemon exiting.
          */
         Emsg0(M_ERROR_TERM, 0, "Fatal database error\n");
      }

      if (try_reconnect_ && !transaction_) {
         /*
          * Only try reconnecting when no transaction is pending.
          * Reconnecting within a transaction will lead to an aborted
          * transaction anyway so we better follow our old error path.
          */
         if (retry) {
            unsigned long mysql_threadid;

            mysql_threadid = mysql_thread_id(db_handle_);
            if (mysql_ping(db_handle_) == 0) {
               /*
                * See if the threadid changed e.g. new connection to the DB.
                */
               if (mysql_thread_id(db_handle_) != mysql_threadid) {
                  mysql_query(db_handle_, "SET wait_timeout=691200");
                  mysql_query(db_handle_, "SET interactive_timeout=691200");
               }

               retry = false;
               goto retry_query;
            }
         }
      }

      /* FALL THROUGH */
   default:
      Mmsg(errmsg, _("Query failed: %s: ERR=%s\n"), query, sql_strerror());
      Dmsg0(500, "SqlQueryWithHandler failed\n");
      goto bail_out;
   }

   Dmsg0(500, "SqlQueryWithHandler succeeded. checking handler\n");

   if (ResultHandler != NULL) {
      if ((result_ = mysql_use_result(db_handle_)) != NULL) {
         num_fields_ = mysql_num_fields(result_);

         /*
          * We *must* fetch all rows
          */
         while ((row = mysql_fetch_row(result_)) != NULL) {
            if (send) {
               /* the result handler returns 1 when it has
                *  seen all the data it wants.  However, we
                *  loop to the end of the data.
                */
               if (ResultHandler(ctx, num_fields_, row)) {
                  send = false;
               }
            }
         }
         SqlFreeResult();
      }
   }

   Dmsg0(500, "SqlQueryWithHandler finished\n");
   retval = true;

bail_out:
   DbUnlock(this);
   return retval;
}

bool BareosDbMysql::SqlQueryWithoutHandler(const char *query, int flags)
{
   int status;
   bool retry = true;
   bool retval = true;

   Dmsg1(500, "SqlQueryWithoutHandler starts with '%s'\n", query);

   /*
    * We are starting a new query. reset everything.
    */
retry_query:
   num_rows_ = -1;
   row_number_ = -1;
   field_number_ = -1;

   if (result_) {
      mysql_free_result(result_);
      result_ = NULL;
   }

   /*
    * query has to be a single SQL statement (not multiple joined by columns (";")).
    * If multiple SQL statements have to be used, they can produce multiple results,
    * each of them needs handling.
    */
   status = mysql_query(db_handle_, query);
   switch (status) {
   case 0:
      Dmsg0(500, "we have a result\n");
      if (flags & QF_STORE_RESULT) {
         result_ = mysql_store_result(db_handle_);
         if (result_ != NULL) {
            num_fields_ = mysql_num_fields(result_);
            Dmsg1(500, "we have %d fields\n", num_fields_);
            num_rows_ = mysql_num_rows(result_);
            Dmsg1(500, "we have %d rows\n", num_rows_);
         } else {
            num_fields_ = 0;
            num_rows_ = mysql_affected_rows(db_handle_);
            Dmsg1(500, "we have %d rows\n", num_rows_);
         }
      } else {
         num_fields_ = 0;
         num_rows_ = mysql_affected_rows(db_handle_);
         Dmsg1(500, "we have %d rows\n", num_rows_);
      }
      break;
   case CR_SERVER_GONE_ERROR:
   case CR_SERVER_LOST:
      if (exit_on_fatal_) {
         /*
          * Any fatal error should result in the daemon exiting.
          */
         Emsg0(M_ERROR_TERM, 0, "Fatal database error\n");
      }

      if (try_reconnect_ && !transaction_) {
         /*
          * Only try reconnecting when no transaction is pending.
          * Reconnecting within a transaction will lead to an aborted
          * transaction anyway so we better follow our old error path.
          */
         if (retry) {
            unsigned long mysql_threadid;

            mysql_threadid = mysql_thread_id(db_handle_);
            if (mysql_ping(db_handle_) == 0) {
               /*
                * See if the threadid changed e.g. new connection to the DB.
                */
               if (mysql_thread_id(db_handle_) != mysql_threadid) {
                  mysql_query(db_handle_, "SET wait_timeout=691200");
                  mysql_query(db_handle_, "SET interactive_timeout=691200");
               }

               retry = false;
               goto retry_query;
            }
         }
      }

      /* FALL THROUGH */
   default:
      Dmsg0(500, "we failed\n");
      status_ = 1;                   /* failed */
      retval = false;
      break;
   }

   return retval;
}

void BareosDbMysql::SqlFreeResult(void)
{
   DbLock(this);
   if (result_) {
      mysql_free_result(result_);
      result_ = NULL;
   }
   if (fields_) {
      free(fields_);
      fields_ = NULL;
   }
   num_rows_ = num_fields_ = 0;
   DbUnlock(this);
}

SQL_ROW BareosDbMysql::SqlFetchRow(void)
{
   if (!result_) {
      return NULL;
   } else {
      return mysql_fetch_row(result_);
   }
}

const char *BareosDbMysql::sql_strerror(void)
{
   return mysql_error(db_handle_);
}

void BareosDbMysql::SqlDataSeek(int row)
{
   return mysql_data_seek(result_, row);
}

int BareosDbMysql::SqlAffectedRows(void)
{
   return mysql_affected_rows(db_handle_);
}

uint64_t BareosDbMysql::SqlInsertAutokeyRecord(const char *query, const char *table_name)
{
   /*
    * First execute the insert query and then retrieve the currval.
    */
   if (mysql_query(db_handle_, query) != 0) {
      return 0;
   }

   num_rows_ = mysql_affected_rows(db_handle_);
   if (num_rows_ != 1) {
      return 0;
   }

   changes++;

   return mysql_insert_id(db_handle_);
}

SQL_FIELD *BareosDbMysql::SqlFetchField(void)
{
   int i;
   MYSQL_FIELD *field;

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
         if ((field = mysql_fetch_field(result_)) != NULL) {
            fields_[i].name = field->name;
            fields_[i].max_length = field->max_length;
            fields_[i].type = field->type;
            fields_[i].flags = field->flags;

            Dmsg4(500, "SqlFetchField finds field '%s' has length='%d' type='%d' and IsNull=%d\n",
                  fields_[i].name, fields_[i].max_length, fields_[i].type, fields_[i].flags);
         }
      }
   }

   /*
    * Increment field number for the next time around
    */
   return &fields_[field_number_++];
}

bool BareosDbMysql::SqlFieldIsNotNull(int field_type)
{
   return IS_NOT_NULL(field_type);
}

bool BareosDbMysql::SqlFieldIsNumeric(int field_type)
{
   return IS_NUM(field_type);
}

/**
 * Returns true if OK
 *         false if failed
 */
bool BareosDbMysql::SqlBatchStart(JobControlRecord *jcr)
{
   bool retval;

   DbLock(this);
   retval = SqlQuery("CREATE TEMPORARY TABLE batch ("
                              "FileIndex integer,"
                              "JobId integer,"
                              "Path blob,"
                              "Name blob,"
                              "LStat tinyblob,"
                              "MD5 tinyblob,"
                              "DeltaSeq integer,"
                              "Fhinfo NUMERIC(20),"
                              "Fhnode NUMERIC(20) )");
   DbUnlock(this);

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
bool BareosDbMysql::SqlBatchEnd(JobControlRecord *jcr, const char *error)
{
   status_ = 0;

   /*
    * Flush any pending inserts.
    */
   if (changes) {
      return SqlQuery(cmd);
   }

   return true;
}

/**
 * Returns true if OK
 *         false if failed
 */
bool BareosDbMysql::SqlBatchInsert(JobControlRecord *jcr, AttributesDbRecord *ar)
{
   const char *digest;
   char ed1[50], ed2[50], ed3[50];

   esc_name = CheckPoolMemorySize(esc_name, fnl*2+1);
   EscapeString(jcr, esc_name, fname, fnl);

   esc_path = CheckPoolMemorySize(esc_path, pnl*2+1);
   EscapeString(jcr, esc_path, path, pnl);

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
           "(%u,%s,'%s','%s','%s','%s',%u,'%s','%s')",
           ar->FileIndex, edit_int64(ar->JobId,ed1), esc_path,
           esc_name, ar->attr, digest, ar->DeltaSeq,
           edit_uint64(ar->Fhinfo,ed2),
           edit_uint64(ar->Fhnode,ed3));
      changes++;
   } else {
      /*
       * We use the esc_obj for temporary storage otherwise
       * we keep on copying data.
       */
      Mmsg(esc_obj, ",(%u,%s,'%s','%s','%s','%s',%u,%u,%u)",
           ar->FileIndex, edit_int64(ar->JobId,ed1), esc_path,
           esc_name, ar->attr, digest, ar->DeltaSeq, ar->Fhinfo, ar->Fhnode);
      PmStrcat(cmd, esc_obj);
      changes++;
   }

   /*
    * See if we need to flush the query buffer filled
    * with multi-row inserts.
    */
   if ((changes % MYSQL_CHANGES_PER_BATCH_INSERT) == 0) {
      if (!SqlQuery(cmd)) {
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
extern "C" BareosDb *backend_instantiate(JobControlRecord *jcr,
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
   BareosDbMysql *mdb = NULL;

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
         if (mdb->IsPrivate()) {
            continue;
         }

         if (mdb->MatchDatabase(db_driver, db_name, db_address, db_port)) {
            Dmsg1(100, "DB REopen %s\n", db_name);
            mdb->IncrementRefcount();
            goto bail_out;
         }
      }
   }
   Dmsg0(100, "db_init_database first time\n");
   mdb = New(BareosDbMysql(jcr,
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
extern "C" void flush_backend(void)
#else
void DbFlushBackends(void)
#endif
{
}

#endif /* HAVE_MYSQL */
