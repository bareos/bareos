/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2003-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2016 Planets Communications B.V.
   Copyright (C) 2013-2013 Bareos GmbH & Co. KG

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
 *    João Henrique Freitas, December 2007
 *    based upon work done by Dan Langille, December 2003 and
 *    by Kern Sibbald, March 2000
 * Major rewrite by Marco van Wieringen, January 2010 for catalog refactoring.
 */
/**
 * @file
 * BAREOS Catalog Database routines specific to DBI
 *   These are DBI specific routines
 */
/*
 * This code only compiles against a recent version of libdbi. The current
 * release found on the libdbi website (0.8.3) won't work for this code.
 *
 * You find the libdbi library on http://sourceforge.net/projects/libdbi
 *
 * A fairly recent version of libdbi from CVS works, so either make sure
 * your distribution has a fairly recent version of libdbi installed or
 * clone the CVS repositories from sourceforge and compile that code and
 * install it.
 *
 * You need:
 * cvs co :pserver:anonymous@libdbi.cvs.sourceforge.net:/cvsroot/libdbi
 * cvs co :pserver:anonymous@libdbi-drivers.cvs.sourceforge.net:/cvsroot/libdbi-drivers
 */

#include "include/bareos.h"

#ifdef HAVE_DBI

#include "cats.h"
#include <dbi/dbi.h>
#include <dbi/dbi-dev.h>
#include <bdb_dbi.h>

/* -----------------------------------------------------------------------
 *
 *   DBI dependent defines and subroutines
 *
 * -----------------------------------------------------------------------
 */

/**
 * List of open databases
 */
static dlist *db_list = NULL;

/**
 * Control allocated fields by dbi_getvalue
 */
static dlist *dbi_getvalue_list = NULL;

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

typedef int (*custom_function_insert_t)(void*, const char*, int);
typedef char* (*custom_function_error_t)(void*);
typedef int (*custom_function_end_t)(void*, const char*);

BareosDbDBI::BareosDbDBI(JobControlRecord *jcr,
                   const char *db_driver,
                   const char *db_name,
                   const char *db_user,
                   const char *db_password,
                   const char *db_address,
                   int db_port,
                   const char *db_socket,
                   bool mult_db_connections,
                   bool disable_batch_insert,
                   bool need_private,
                   bool try_reconnect,
                   bool exit_on_fatal)
{
   char *p;
   char new_db_driver[10];
   char db_driverdir[256];
   DbiFieldGet *field;

   p = (char *)(db_driver + 4);
   if (Bstrcasecmp(p, "mysql")) {
      db_type_ = SQL_TYPE_MYSQL;
      bstrncpy(new_db_driver, "mysql", sizeof(new_db_driver));
   } else if (Bstrcasecmp(p, "postgresql")) {
      db_type_ = SQL_TYPE_POSTGRESQL;
      bstrncpy(new_db_driver, "pgsql", sizeof(new_db_driver));
   } else if (Bstrcasecmp(p, "sqlite3")) {
      db_type_ = SQL_TYPE_SQLITE3;
      bstrncpy(new_db_driver, "sqlite3", sizeof(new_db_driver));
   } else {
      Jmsg(jcr, M_ABORT, 0, _("Unknown database type: %s\n"), p);
      return;
   }

   /*
    * Set db_driverdir whereis is the libdbi drivers
    */
   bstrncpy(db_driverdir, DBI_DRIVER_DIR, 255);

   /*
    * Initialize the parent class members.
    */
   db_interface_type_ = SQL_INTERFACE_TYPE_DBI;
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
   if (db_driverdir) {
      db_driverdir_ = bstrdup(db_driverdir);
   }
   db_driver_ = bstrdup(new_db_driver);
   db_port_ = db_port;
   if (disable_batch_insert) {
      disabled_batch_insert_ = true;
      have_batch_insert_ = false;
   } else {
      disabled_batch_insert_ = false;
#if defined(USE_BATCH_FILE_INSERT)
#ifdef HAVE_DBI_BATCH_FILE_INSERT
      have_batch_insert_ = true;
#else
      have_batch_insert_ = false;
#endif /* HAVE_DBI_BATCH_FILE_INSERT */
#else
      have_batch_insert_ = false;
#endif /* USE_BATCH_FILE_INSERT */
   }
   errmsg = GetPoolMemory(PM_EMSG); /* get error message buffer */
   *errmsg = 0;
   cmd = GetPoolMemory(PM_EMSG); /* get command buffer */
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

   /*
    * Initialize the private members.
    */
   db_handle_ = NULL;
   result_ = NULL;
   field_get_ = NULL;

   /*
    * Put the db in the list.
    */
   if (db_list == NULL) {
      db_list = New(dlist(this, &this->link_));
      dbi_getvalue_list = New(dlist(field, &field->link));
   }
   db_list->append(this);
}

BareosDbDBI::~BareosDbDBI()
{
}

/**
 * Now actually open the database.  This can generate errors,
 *   which are returned in the errmsg
 *
 * DO NOT close the database or delete mdb here  !!!!
 */
bool BareosDbDBI::OpenDatabase(JobControlRecord *jcr)
{
   bool retval = false;
   int errstat;
   int dbstat;
   uint8_t len;
   const char *dbi_errmsg;
   char buf[10], *port;
   int numdrivers;
   char *new_db_name = NULL;
   char *new_db_dir = NULL;

   P(mutex);
   if (connected_) {
      retval = true;
      goto bail_out;
   }

   if ((errstat=RwlInit(&lock_)) != 0) {
      BErrNo be;
      Mmsg1(&errmsg, _("Unable to initialize DB lock. ERR=%s\n"),
            be.bstrerror(errstat));
      goto bail_out;
   }

   if (db_port_) {
      Bsnprintf(buf, sizeof(buf), "%d", db_port_);
      port = buf;
   } else {
      port = NULL;
   }

   numdrivers = dbi_initialize_r(db_driverdir_, &(instance_));
   if (numdrivers < 0) {
      Mmsg2(&errmsg, _("Unable to locate the DBD drivers to DBI interface in: \n"
                               "db_driverdir=%s. It is probaly not found any drivers\n"),
                               db_driverdir_,numdrivers);
      goto bail_out;
   }
   db_handle_ = (void **)dbi_conn_new_r(db_driver_, instance_);
   /*
    * Can be many types of databases
    */
   switch (db_type_) {
   case SQL_TYPE_MYSQL:
      dbi_conn_set_option(db_handle_, "host", db_address_);      /* default = localhost */
      dbi_conn_set_option(db_handle_, "port", port);              /* default port */
      dbi_conn_set_option(db_handle_, "username", db_user_);     /* login name */
      dbi_conn_set_option(db_handle_, "password", db_password_); /* password */
      dbi_conn_set_option(db_handle_, "dbname", db_name_);       /* database name */
      break;
   case SQL_TYPE_POSTGRESQL:
      dbi_conn_set_option(db_handle_, "host", db_address_);
      dbi_conn_set_option(db_handle_, "port", port);
      dbi_conn_set_option(db_handle_, "username", db_user_);
      dbi_conn_set_option(db_handle_, "password", db_password_);
      dbi_conn_set_option(db_handle_, "dbname", db_name_);
      break;
   case SQL_TYPE_SQLITE3:
      len = strlen(working_directory) + 5;
      new_db_dir = (char *)malloc(len);
      strcpy(new_db_dir, working_directory);
      strcat(new_db_dir, "/");
      len = strlen(db_name_) + 5;
      new_db_name = (char *)malloc(len);
      strcpy(new_db_name, db_name_);
      strcat(new_db_name, ".db");
      dbi_conn_set_option(db_handle_, "sqlite3_dbdir", new_db_dir);
      dbi_conn_set_option(db_handle_, "dbname", new_db_name);
      Dmsg2(500, "SQLITE: %s %s\n", new_db_dir, new_db_name);
      free(new_db_dir);
      free(new_db_name);
      break;
   }

   /*
    * If connection fails, try at 5 sec intervals for 30 seconds.
    */
   for (int retry=0; retry < 6; retry++) {
      dbstat = dbi_conn_connect(db_handle_);
      if (dbstat == 0) {
         break;
      }

      dbi_conn_error(db_handle_, &dbi_errmsg);
      Dmsg1(50, "dbi error: %s\n", dbi_errmsg);

      Bmicrosleep(5, 0);
   }

   if (dbstat != 0 ) {
      Mmsg3(&errmsg, _("Unable to connect to DBI interface. Type=%s Database=%s User=%s\n"
         "Possible causes: SQL server not running; password incorrect; max_connections exceeded.\n"),
         db_driver_, db_name_, db_user_);
      goto bail_out;
   }

   Dmsg0(50, "dbi_real_connect done\n");
   Dmsg3(50, "db_user=%s db_name=%s db_password=%s\n",
         db_user_, db_name_,
        (db_password_ == NULL) ? "(NULL)" : db_password_);

   connected_ = true;

   if (!CheckTablesVersion(jcr, this)) {
      goto bail_out;
   }

   switch (db_type_) {
   case SQL_TYPE_MYSQL:
      /*
       * Set connection timeout to 8 days specialy for batch mode
       */
      SqlQueryWithoutHandler("SET wait_timeout=691200");
      SqlQueryWithoutHandler("SET interactive_timeout=691200");
      break;
   case SQL_TYPE_POSTGRESQL:
      /*
       * Tell PostgreSQL we are using standard conforming strings
       * and avoid warnings such as:
       * WARNING:  nonstandard use of \\ in a string literal
       */
      SqlQueryWithoutHandler("SET datestyle TO 'ISO, YMD'");
      SqlQueryWithoutHandler("SET standard_conforming_strings=on");
      break;
   }

   retval = true;

bail_out:
   V(mutex);
   return retval;
}

void BareosDbDBI::CloseDatabase(JobControlRecord *jcr)
{
   if (connected_) {
      EndTransaction(jcr);
   }
   P(mutex);
   ref_count_--;
   if (ref_count_ == 0) {
      if (connected_) {
         SqlFreeResult();
      }
      db_list->remove(this);
      if (connected_ && db_handle_) {
         dbi_shutdown_r(instance_);
         db_handle_ = NULL;
         instance_ = NULL;
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
      if (db_driverdir_) {
         free(db_driverdir_);
      }
      delete this;
      if (db_list->size() == 0) {
         delete db_list;
         db_list = NULL;
      }
   }
   V(mutex);
}

bool BareosDbDBI::ValidateConnection(void)
{
   bool retval;

   DbLock(this);
   if (dbi_conn_ping(db_handle_) == 1) {
      retval = true;
      goto bail_out;
   } else {
      retval = false;
      goto bail_out;
   }

bail_out:
   DbUnlock(this);
   return retval;
}

/**
 * Escape strings so that DBI is happy
 *
 *   NOTE! len is the length of the old string. Your new
 *         string must be long enough (max 2*old+1) to hold
 *         the escaped output.
 *
 * dbi_conn_quote_string_copy receives a pointer to pointer.
 * We need copy the value of pointer to snew because libdbi change the
 * pointer
 */
void BareosDbDBI::EscapeString(JobControlRecord *jcr, char *snew, char *old, int len)
{
   char *inew;
   char *pnew;

   if (len == 0) {
      snew[0] = 0;
   } else {
      /*
       * Correct the size of old basead in len and copy new string to inew
       */
      inew = (char *)malloc(sizeof(char) * len + 1);
      bstrncpy(inew,old,len + 1);
      /*
       * Escape the correct size of old
       */
      dbi_conn_escape_string_copy(db_handle_, inew, &pnew);
      free(inew);
      /*
       * Copy the escaped string to snew
       */
      bstrncpy(snew, pnew, 2 * len + 1);
   }

   Dmsg2(500, "dbi_conn_escape_string_copy %p %s\n",snew,snew);
}

/**
 * Escape binary object so that DBI is happy
 * Memory is stored in BareosDb struct, no need to free it
 */
char *BareosDbDBI::EscapeObject(JobControlRecord *jcr, char *old, int len)
{
   size_t new_len;
   char *pnew;

   if (len == 0) {
      esc_obj[0] = 0;
   } else {
      new_len = dbi_conn_escape_string_copy(db_handle_, esc_obj, &pnew);
      esc_obj = CheckPoolMemorySize(esc_obj, new_len+1);
      memcpy(esc_obj, pnew, new_len);
   }

   return esc_obj;
}

/**
 * Unescape binary object so that DBI is happy
 */
void BareosDbDBI::UnescapeObject(JobControlRecord *jcr, char *from, int32_t expected_len,
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

/**
 * Start a transaction. This groups inserts and makes things
 * much more efficient. Usually started when inserting
 * file attributes.
 */
void BareosDbDBI::StartTransaction(JobControlRecord *jcr)
{
   if (!jcr->attr) {
      jcr->attr = GetPoolMemory(PM_FNAME);
   }
   if (!jcr->ar) {
      jcr->ar = (AttributesDbRecord *)malloc(sizeof(AttributesDbRecord));
   }

   switch (db_type_) {
   case SQL_TYPE_SQLITE3:
      if (!allow_transactions_) {
         return;
      }

      DbLock(this);
      /*
       * Allow only 10,000 changes per transaction
       */
      if (transaction_ && changes > 10000) {
         EndTransaction(jcr);
      }
      if (!transaction_) {
         SqlQueryWithoutHandler("BEGIN");  /* begin transaction */
         Dmsg0(400, "Start SQLite transaction\n");
         transaction_ = true;
      }
      DbUnlock(this);
      break;
   case SQL_TYPE_POSTGRESQL:
      /*
       * This is turned off because transactions break
       * if multiple simultaneous jobs are run.
       */
      if (!allow_transactions_) {
         return;
      }

      DbLock(this);
      /*
       * Allow only 25,000 changes per transaction
       */
      if (transaction_ && changes > 25000) {
         EndTransaction(jcr);
      }
      if (!transaction_) {
         SqlQueryWithoutHandler("BEGIN");  /* begin transaction */
         Dmsg0(400, "Start PosgreSQL transaction\n");
         transaction_ = true;
      }
      DbUnlock(this);
      break;
   case SQL_TYPE_INGRES:
      if (!allow_transactions_) {
         return;
      }

      DbLock(this);
      /*
       * Allow only 25,000 changes per transaction
       */
      if (transaction_ && changes > 25000) {
         EndTransaction(jcr);
      }
      if (!transaction_) {
         SqlQueryWithoutHandler("BEGIN");  /* begin transaction */
         Dmsg0(400, "Start Ingres transaction\n");
         transaction_ = true;
      }
      DbUnlock(this);
      break;
   default:
      break;
   }
}

void BareosDbDBI::EndTransaction(JobControlRecord *jcr)
{
   if (jcr && jcr->cached_attribute) {
      Dmsg0(400, "Flush last cached attribute.\n");
      if (!CreateAttributesRecord(jcr, jcr->ar)) {
         Jmsg1(jcr, M_FATAL, 0, _("Attribute create error. %s"), strerror());
      }
      jcr->cached_attribute = false;
   }

   switch (db_type_) {
   case SQL_TYPE_SQLITE3:
      if (!allow_transactions_) {
         return;
      }

      DbLock(this);
      if (transaction_) {
         SqlQueryWithoutHandler("COMMIT"); /* end transaction */
         transaction_ = false;
         Dmsg1(400, "End SQLite transaction changes=%d\n", changes);
      }
      changes = 0;
      DbUnlock(this);
      break;
   case SQL_TYPE_POSTGRESQL:
      if (!allow_transactions_) {
         return;
      }

      DbLock(this);
      if (transaction_) {
         SqlQueryWithoutHandler("COMMIT"); /* end transaction */
         transaction_ = false;
         Dmsg1(400, "End PostgreSQL transaction changes=%d\n", changes);
      }
      changes = 0;
      DbUnlock(this);
      break;
   case SQL_TYPE_INGRES:
      if (!allow_transactions_) {
         return;
      }

      DbLock(this);
      if (transaction_) {
         SqlQueryWithoutHandler("COMMIT"); /* end transaction */
         transaction_ = false;
         Dmsg1(400, "End Ingres transaction changes=%d\n", changes);
      }
      changes = 0;
      DbUnlock(this);
      break;
   default:
      break;
   }
}

/**
 * Submit a general SQL command (cmd), and for each row returned,
 * the ResultHandler is called with the ctx.
 */
bool BareosDbDBI::SqlQueryWithHandler(const char *query, DB_RESULT_HANDLER *ResultHandler, void *ctx)
{
   bool retval = true;
   SQL_ROW row;

   Dmsg1(500, "SqlQueryWithHandler starts with %s\n", query);

   DbLock(this);
   if (!SqlQueryWithoutHandler(query, QF_STORE_RESULT)) {
      Mmsg(errmsg, _("Query failed: %s: ERR=%s\n"), query, sql_strerror());
      Dmsg0(500, "SqlQueryWithHandler failed\n");
      retval = false;
      goto bail_out;
   }

   Dmsg0(500, "SqlQueryWithHandler succeeded. checking handler\n");

   if (ResultHandler != NULL) {
      Dmsg0(500, "SqlQueryWithHandler invoking handler\n");
      while ((row = SqlFetchRow()) != NULL) {
         Dmsg0(500, "SqlQueryWithHandler SqlFetchRow worked\n");
         if (ResultHandler(ctx, num_fields_, row))
            break;
      }
      SqlFreeResult();
   }

   Dmsg0(500, "SqlQueryWithHandler finished\n");

bail_out:
   DbUnlock(this);
   return retval;
}

/**
 * Note, if this routine returns 1 (failure), BAREOS expects
 *  that no result has been stored.
 *
 *  Returns:  true on success
 *            false on failure
 */
bool BareosDbDBI::SqlQueryWithoutHandler(const char *query, int flags)
{
   bool retval = false;
   const char *dbi_errmsg;

   Dmsg1(500, "SqlQueryWithoutHandler starts with %s\n", query);

   /*
    * We are starting a new query.  reset everything.
    */
   num_rows_ = -1;
   row_number_ = -1;
   field_number_ = -1;

   if (result_) {
      dbi_result_free(result_);  /* hmm, someone forgot to free?? */
      result_ = NULL;
   }

   result_ = (void **)dbi_conn_query(db_handle_, query);

   if (!result_) {
      Dmsg2(50, "Query failed: %s %p\n", query, result_);
      goto bail_out;
   }

   status_ = (dbi_error_flag) dbi_conn_error(db_handle_, &dbi_errmsg);
   if (status_ == DBI_ERROR_NONE) {
      Dmsg1(500, "we have a result\n", query);

      /*
       * How many fields in the set?
       * num_fields starting at 1
       */
      num_fields_ = dbi_result_get_numfields(result_);
      Dmsg1(500, "we have %d fields\n", num_fields_);
      /*
       * If no result num_rows is 0
       */
      num_rows_ = dbi_result_get_numrows(result_);
      Dmsg1(500, "we have %d rows\n", num_rows_);

      status_ = (dbi_error_flag) 0;                  /* succeed */
   } else {
      Dmsg1(50, "Result status failed: %s\n", query);
      goto bail_out;
   }

   Dmsg0(500, "SqlQueryWithoutHandler finishing\n");
   retval = true;
   goto ok_out;

bail_out:
   status_ = (dbi_error_flag) dbi_conn_error(db_handle_, &dbi_errmsg);
   //dbi_conn_error(db_handle_, &dbi_errmsg);
   Dmsg4(500, "SqlQueryWithoutHandler we failed dbi error: "
                   "'%s' '%p' '%d' flag '%d''\n", dbi_errmsg, result_, result_, status_);
   dbi_result_free(result_);
   result_ = NULL;
   status_ = (dbi_error_flag) 1;                   /* failed */

ok_out:
   return retval;
}

void BareosDbDBI::SqlFreeResult(void)
{
   DbiFieldGet *f;

   DbLock(this);
   if (result_) {
      dbi_result_free(result_);
      result_ = NULL;
   }
   if (rows_) {
      free(rows_);
      rows_ = NULL;
   }
   /*
    * Now is time to free all value return by dbi_get_value
    * this is necessary because libdbi don't free memory return by yours results
    * and BAREOS has some routine wich call more than once time sql_fetch_row
    *
    * Using a queue to store all pointer allocate is a good way to free all things
    * when necessary
    */
   foreach_dlist(f, dbi_getvalue_list) {
      free(f->value);
      free(f);
   }
   if (fields_) {
      free(fields_);
      fields_ = NULL;
   }
   num_rows_ = num_fields_ = 0;
   DbUnlock(this);
}

/* dbi_getvalue
 * like PQgetvalue;
 * char *PQgetvalue(const PGresult *res,
 *                int row_number,
 *                int column_number);
 *
 * use dbi_result_seek_row to search in result set
 * use example to return only strings
 */
static char *dbi_getvalue(dbi_result *result, int row_number, unsigned int column_number)
{
   char *buf = NULL;
   const char *dbi_errmsg;
   const char *field_name;
   unsigned short dbitype;
   size_t field_length;
   int64_t num;

   /* correct the index for dbi interface
    * dbi index begins 1
    * I prefer do not change others functions
    */
   Dmsg3(600, "dbi_getvalue pre-starting result '%p' row number '%d' column number '%d'\n",
         result, row_number, column_number);

   column_number++;

   if(row_number == 0) {
     row_number++;
   }

   Dmsg3(600, "dbi_getvalue starting result '%p' row number '%d' column number '%d'\n",
                        result, row_number, column_number);

   if(dbi_result_seek_row(result, row_number)) {

      field_name = dbi_result_get_field_name(result, column_number);
      field_length = dbi_result_get_field_length(result, field_name);
      dbitype = dbi_result_get_field_type_idx(result,column_number);

      Dmsg3(500, "dbi_getvalue start: type: '%d' "
            "field_length bytes: '%d' fieldname: '%s'\n",
            dbitype, field_length, field_name);

      if(field_length) {
         //buf = (char *)malloc(sizeof(char *) * field_length + 1);
         buf = (char *)malloc(field_length + 1);
      } else {
         /*
          * if numbers
          */
         buf = (char *)malloc(sizeof(char *) * 50);
      }

      switch (dbitype) {
      case DBI_TYPE_INTEGER:
         num = dbi_result_get_longlong(result, field_name);
         edit_int64(num, buf);
         field_length = strlen(buf);
         break;
      case DBI_TYPE_STRING:
         if(field_length) {
            field_length = Bsnprintf(buf, field_length + 1, "%s",
            dbi_result_get_string(result, field_name));
         } else {
            buf[0] = 0;
         }
         break;
      case DBI_TYPE_BINARY:
         /*
          * dbi_result_get_binary return a NULL pointer if value is empty
          * following, change this to what BAREOS expected
          */
         if(field_length) {
            field_length = Bsnprintf(buf, field_length + 1, "%s",
                  dbi_result_get_binary(result, field_name));
         } else {
            buf[0] = 0;
         }
         break;
      case DBI_TYPE_DATETIME:
         time_t last;
         struct tm tm;

         last = dbi_result_get_datetime(result, field_name);

         if(last == -1) {
                field_length = Bsnprintf(buf, 20, "0000-00-00 00:00:00");
         } else {
            Blocaltime(&last, &tm);
            field_length = Bsnprintf(buf, 20, "%04d-%02d-%02d %02d:%02d:%02d",
                  (tm.tm_year + 1900), (tm.tm_mon + 1), tm.tm_mday,
                  tm.tm_hour, tm.tm_min, tm.tm_sec);
         }
         break;
      }

   } else {
      dbi_conn_error(dbi_result_get_conn(result), &dbi_errmsg);
      Dmsg1(500, "dbi_getvalue error: %s\n", dbi_errmsg);
   }

   Dmsg3(500, "dbi_getvalue finish buffer: '%p' num bytes: '%d' data: '%s'\n",
      buf, field_length, buf);

   /*
    * Don't worry about this buf
    */
   return buf;
}

SQL_ROW BareosDbDBI::SqlFetchRow(void)
{
   int j;
   SQL_ROW row = NULL; /* by default, return NULL */

   Dmsg0(500, "SqlFetchRow start\n");
   if ((!rows_ || rows_size_ < num_fields_) && num_rows_ > 0) {
      if (rows_) {
         Dmsg0(500, "SqlFetchRow freeing space\n");
         Dmsg2(500, "SqlFetchRow row: '%p' num_fields: '%d'\n", rows_, num_fields_);
         if (num_rows_ != 0) {
            for (j = 0; j < num_fields_; j++) {
               Dmsg2(500, "SqlFetchRow row '%p' '%d'\n", rows_[j], j);
                  if (rows_[j]) {
                     free(rows_[j]);
                  }
            }
         }
         free(rows_);
      }
      Dmsg1(500, "we need space for %d bytes\n", sizeof(char *) * num_fields_);
      rows_ = (SQL_ROW)malloc(sizeof(char *) * num_fields_);
      rows_size_ = num_fields_;

      /*
       * Now reset the row_number now that we have the space allocated
       */
      row_number_ = 1;
   }

   /*
    * If still within the result set
    */
   if (row_number_ <= num_rows_ && row_number_ != DBI_ERROR_BADPTR) {
      Dmsg2(500, "SqlFetchRow row number '%d' is acceptable (1..%d)\n", row_number_, num_rows_);
      /*
       * Get each value from this row
       */
      for (j = 0; j < num_fields_; j++) {
         rows_[j] = dbi_getvalue(result_, row_number_, j);
         /*
          * Allocate space to queue row
          */
         field_get_ = (DbiFieldGet *)malloc(sizeof(DbiFieldGet));
         /*
          * Store the pointer in queue
          */
         field_get_->value = rows_[j];
         Dmsg4(500, "SqlFetchRow row[%d] field: '%p' in queue: '%p' has value: '%s'\n",
               j, rows_[j], field_get_->value, rows_[j]);
         /*
          * Insert in queue to future free
          */
         dbi_getvalue_list->append(field_get_);
      }
      /*
       * Increment the row number for the next call
       */
      row_number_++;

      row = rows_;
   } else {
      Dmsg2(500, "SqlFetchRow row number '%d' is NOT acceptable (1..%d)\n", row_number_, num_rows_);
   }

   Dmsg1(500, "SqlFetchRow finishes returning %p\n", row);

   return row;
}

const char *BareosDbDBI::sql_strerror(void)
{
   const char *dbi_errmsg;

   dbi_conn_error(db_handle_, &dbi_errmsg);

   return dbi_errmsg;
}

void BareosDbDBI::SqlDataSeek(int row)
{
   /*
    * Set the row number to be returned on the next call to sql_fetch_row
    */
   row_number_ = row;
}

int BareosDbDBI::SqlAffectedRows(void)
{
#if 0
   return dbi_result_get_numrows_affected(result);
#else
   return 1;
#endif
}

uint64_t BareosDbDBI::SqlInsertAutokeyRecord(const char *query, const char *table_name)
{
   char sequence[30];
   uint64_t id = 0;

   /*
    * First execute the insert query and then retrieve the currval.
    */
   if (!SqlQueryWithoutHandler(query)) {
      return 0;
   }

   num_rows_ = SqlAffectedRows();
   if (num_rows_ != 1) {
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
   if (db_type_ == SQL_TYPE_POSTGRESQL) {
      if (Bstrcasecmp(table_name, "basefiles")) {
         bstrncpy(sequence, "basefiles_baseid", sizeof(sequence));
      } else {
         bstrncpy(sequence, table_name, sizeof(sequence));
         bstrncat(sequence, "_", sizeof(sequence));
         bstrncat(sequence, table_name, sizeof(sequence));
         bstrncat(sequence, "id", sizeof(sequence));
      }

      bstrncat(sequence, "_seq", sizeof(sequence));
      id = dbi_conn_sequence_last(db_handle_, NT_(sequence));
   } else {
      id = dbi_conn_sequence_last(db_handle_, NT_(table_name));
   }

   return id;
}

/* dbi_getisnull
 * like PQgetisnull
 * int PQgetisnull(const PGresult *res,
 *                 int row_number,
 *                 int column_number);
 *
 *  use dbi_result_seek_row to search in result set
 */
static int DbiGetisnull(dbi_result *result, int row_number, int column_number) {
   int i;

   if (row_number == 0) {
      row_number++;
   }

   column_number++;

   if (dbi_result_seek_row(result, row_number)) {
      i = dbi_result_field_is_null_idx(result,column_number);
      return i;
   } else {
      return 0;
   }
}

SQL_FIELD *BareosDbDBI::SqlFetchField(void)
{
   int i, j;
   int dbi_index;
   int MaxLength;
   int this_length;
   char *cbuf = NULL;

   Dmsg0(500, "SqlFetchField starts\n");

   if (!fields_ || fields_size_ < num_fields_) {
      if (fields_) {
         free(fields_);
         fields_ = NULL;
      }
      Dmsg1(500, "allocating space for %d fields\n", num_fields_);
      fields_ = (SQL_FIELD *)malloc(sizeof(SQL_FIELD) * num_fields_);
      fields_size_ = num_fields_;

      for (i = 0; i < num_fields_; i++) {
         /*
          * num_fields is starting at 1, increment i by 1
          */
         dbi_index = i + 1;
         Dmsg1(500, "filling field %d\n", i);
         fields_[i].name = (char *)dbi_result_get_field_name(result_, dbi_index);
         fields_[i].type = dbi_result_get_field_type_idx(result_, dbi_index);
         fields_[i].flags = dbi_result_get_field_attribs_idx(result_, dbi_index);

         /*
          * For a given column, find the max length.
          */
         MaxLength = 0;
         for (j = 0; j < num_rows_; j++) {
            if (DbiGetisnull(result_, j, dbi_index)) {
                this_length = 4;        /* "NULL" */
            } else {
               cbuf = dbi_getvalue(result_, j, dbi_index);
               this_length = cstrlen(cbuf);
               /*
                * cbuf is always free
                */
               free(cbuf);
            }

            if (MaxLength < this_length) {
               MaxLength = this_length;
            }
         }
         fields_[i].MaxLength = MaxLength;

         Dmsg4(500, "SqlFetchField finds field '%s' has length='%d' type='%d' and IsNull=%d\n",
               fields_[i].name, fields_[i].MaxLength, fields_[i].type, fields_[i].flags);
      }
   }

   /*
    * Increment field number for the next time around
    */
   return &fields_[field_number_++];
}

bool BareosDbDBI::SqlFieldIsNotNull(int field_type)
{
   switch (field_type) {
   case (1 << 0):
      return true;
   default:
      return false;
   }
}

bool BareosDbDBI::SqlFieldIsNumeric(int field_type)
{
   switch (field_type) {
   case 1:
   case 2:
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
static char *postgresql_copy_escape(char *dest, char *src, size_t len)
{
   /*
    * We have to escape \t, \n, \r, \
    */
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
 * This can be a bit strang but is the one way to do
 *
 * Returns true if OK
 *         false if failed
 */
bool BareosDbDBI::SqlBatchStart(JobControlRecord *jcr)
{
   bool retval = true;
   const char *query = "COPY batch FROM STDIN";

   Dmsg0(500, "SqlBatchStart started\n");

   DbLock(this);
   switch (db_type_) {
   case SQL_TYPE_MYSQL:
      if (!SqlQueryWithoutHandler("CREATE TEMPORARY TABLE batch ("
                                     "FileIndex integer,"
                                     "JobId integer,"
                                     "Path blob,"
                                     "Name blob,"
                                     "LStat tinyblob,"
                                     "MD5 tinyblob,"
                                     "DeltaSeq smallint)")) {
         Dmsg0(500, "SqlBatchStart failed\n");
         goto bail_out;
      }
      Dmsg0(500, "SqlBatchStart finishing\n");
      goto ok_out;
   case SQL_TYPE_POSTGRESQL:
      if (!SqlQueryWithoutHandler("CREATE TEMPORARY TABLE batch ("
                                     "FileIndex int,"
                                     "JobId int,"
                                     "Path varchar,"
                                     "Name varchar,"
                                     "LStat varchar,"
                                     "MD5 varchar,"
                                     "DeltaSeq int)")) {
         Dmsg0(500, "SqlBatchStart failed\n");
         goto bail_out;
      }

      /*
       * We are starting a new query.  reset everything.
       */
      num_rows_ = -1;
      row_number_ = -1;
      field_number_ = -1;

      SqlFreeResult();

      for (int i=0; i < 10; i++) {
         SqlQueryWithoutHandler(query);
         if (result_) {
            break;
         }
         Bmicrosleep(5, 0);
      }
      if (!result_) {
         Dmsg1(50, "Query failed: %s\n", query);
         goto bail_out;
      }

      status_ = (dbi_error_flag)dbi_conn_error(db_handle_, NULL);
      //status_ = DBI_ERROR_NONE;

      if (status_ == DBI_ERROR_NONE) {
         /*
          * How many fields in the set?
          */
         num_fields_ = dbi_result_get_numfields(result_);
         num_rows_ = dbi_result_get_numrows(result_);
         status_ = (dbi_error_flag) 1;
      } else {
         Dmsg1(50, "Result status failed: %s\n", query);
         goto bail_out;
      }

      Dmsg0(500, "SqlBatchStart finishing\n");
      goto ok_out;
   case SQL_TYPE_SQLITE3:
      if (!SqlQueryWithoutHandler("CREATE TEMPORARY TABLE batch ("
                                     "FileIndex integer,"
                                     "JobId integer,"
                                     "Path blob,"
                                     "Name blob,"
                                     "LStat tinyblob,"
                                     "MD5 tinyblob,"
                                     "DeltaSeq smallint)")) {
         Dmsg0(500, "SqlBatchStart failed\n");
         goto bail_out;
      }
      Dmsg0(500, "SqlBatchStart finishing\n");
      goto ok_out;
   }

bail_out:
   Mmsg1(&errmsg, _("error starting batch mode: %s"), sql_strerror());
   status_ = (dbi_error_flag) 0;
   SqlFreeResult();
   result_ = NULL;
   retval = false;

ok_out:
   DbUnlock(this);
   return retval;
}

/**
 * Set error to something to abort operation
 */
bool BareosDbDBI::SqlBatchEnd(JobControlRecord *jcr, const char *error)
{
   int res = 0;
   int count = 30;
   int (*custom_function)(void*, const char*) = NULL;
   dbi_conn_t *myconn = (dbi_conn_t *)(db_handle_);

   Dmsg0(500, "SqlBatchStart started\n");

   switch (db_type_) {
   case SQL_TYPE_MYSQL:
      status_ = (dbi_error_flag) 0;
      break;
   case SQL_TYPE_POSTGRESQL:
      custom_function = (custom_function_end_t)dbi_driver_specific_function(dbi_conn_get_driver(myconn), "PQputCopyEnd");

      do {
         res = (*custom_function)(myconn->connection, error);
      } while (res == 0 && --count > 0);

      if (res == 1) {
         Dmsg0(500, "ok\n");
         status_ = (dbi_error_flag) 1;
      }

      if (res <= 0) {
         Dmsg0(500, "we failed\n");
         status_ = (dbi_error_flag) 0;
         //Mmsg1(&errmsg, _("error ending batch mode: %s"), PQerrorMessage(myconn));
       }
      break;
   case SQL_TYPE_SQLITE3:
      status_ = (dbi_error_flag) 0;
      break;
   }

   Dmsg0(500, "SqlBatchStart finishing\n");

   return true;
}

/**
 * This function is big and use a big switch.
 * In near future is better split in small functions
 * and refactory.
 */
bool BareosDbDBI::SqlBatchInsert(JobControlRecord *jcr, AttributesDbRecord *ar)
{
   int res;
   int count=30;
   dbi_conn_t *myconn = (dbi_conn_t *)(db_handle_);
   int (*custom_function)(void*, const char*, int) = NULL;
   char* (*custom_function_error)(void*) = NULL;
   size_t len;
   char *digest;
   char ed1[50];

   Dmsg0(500, "SqlBatchStart started \n");

   esc_name = CheckPoolMemorySize(esc_name, fnl*2+1);
   esc_path = CheckPoolMemorySize(esc_path, pnl*2+1);

   if (ar->Digest == NULL || ar->Digest[0] == 0) {
      digest = "";
   } else {
      digest = ar->Digest;
   }

   switch (db_type_) {
   case SQL_TYPE_MYSQL:
      db_escape_string(jcr, esc_name, fname, fnl);
      db_escape_string(jcr, esc_path, path, pnl);
      len = Mmsg(cmd, "INSERT INTO batch VALUES "
                      "(%u,%s,'%s','%s','%s','%s',%u)",
                      ar->FileIndex, edit_int64(ar->JobId,ed1), esc_path,
                      esc_name, ar->attr, digest, ar->DeltaSeq);

      if (!SqlQueryWithoutHandler(cmd))
      {
         Dmsg0(500, "SqlBatchStart failed\n");
         goto bail_out;
      }

      Dmsg0(500, "SqlBatchStart finishing\n");

      return true;
      break;
   case SQL_TYPE_POSTGRESQL:
      postgresql_copy_escape(esc_name, fname, fnl);
      postgresql_copy_escape(esc_path, path, pnl);
      len = Mmsg(cmd, "%u\t%s\t%s\t%s\t%s\t%s\t%u\n",
                     ar->FileIndex, edit_int64(ar->JobId, ed1), esc_path,
                     esc_name, ar->attr, digest, ar->DeltaSeq);

      /*
       * libdbi don't support CopyData and we need call a postgresql
       * specific function to do this work
       */
      Dmsg2(500, "SqlBatchInsert :\n %s \ncmd_size: %d",cmd, len);
      custom_function = (custom_function_insert_t)dbi_driver_specific_function(dbi_conn_get_driver(myconn),"PQputCopyData");
      if (custom_function != NULL) {
         do {
            res = (*custom_function)(myconn->connection, cmd, len);
         } while (res == 0 && --count > 0);

         if (res == 1) {
            Dmsg0(500, "ok\n");
            changes++;
            status_ = (dbi_error_flag) 1;
         }

         if (res <= 0) {
            Dmsg0(500, "SqlBatchInsert failed\n");
            goto bail_out;
         }

         Dmsg0(500, "SqlBatchInsert finishing\n");
         return true;
      } else {
         /*
          * Ensure to detect a PQerror
          */
         custom_function_error = (custom_function_error_t)dbi_driver_specific_function(dbi_conn_get_driver(myconn), "PQerrorMessage");
         Dmsg1(500, "SqlBatchInsert failed\n PQerrorMessage: %s", (*custom_function_error)(myconn->connection));
         goto bail_out;
      }
      break;
   case SQL_TYPE_SQLITE3:
      db_escape_string(jcr, esc_name, fname, fnl);
      db_escape_string(jcr, esc_path, path, pnl);
      len = Mmsg(cmd, "INSERT INTO batch VALUES "
                      "(%u,%s,'%s','%s','%s','%s',%u)",
                      ar->FileIndex, edit_int64(ar->JobId,ed1), esc_path,
                      esc_name, ar->attr, digest, ar->DeltaSeq);

      if (!SqlQueryWithoutHandler(cmd))
      {
         Dmsg0(500, "SqlBatchInsert failed\n");
         goto bail_out;
      }

      Dmsg0(500, "SqlBatchInsert finishing\n");

      return true;
      break;
   }

bail_out:
   Mmsg1(&errmsg, _("error inserting batch mode: %s"), sql_strerror());
   status_ = (dbi_error_flag) 0;
   SqlFreeResult();
   return false;
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
                       bool need_private,
                       bool try_reconnect,
                       bool exit_on_fatal,
                       bool need_private)
#endif
{
   BareosDbDBI *mdb = NULL;

   if (!db_driver) {
      Jmsg(jcr, M_ABORT, 0, _("Driver type not specified in Catalog resource.\n"));
   }

   if (strlen(db_driver) < 5 || db_driver[3] != ':' || !bstrncasecmp(db_driver, "dbi", 3)) {
      Jmsg(jcr, M_ABORT, 0, _("Invalid driver type, must be \"dbi:<type>\"\n"));
   }

   if (!db_user) {
      Jmsg(jcr, M_FATAL, 0, _("A user name for DBI must be supplied.\n"));
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
   mdb = New(BareosDbDBI(jcr,
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
extern "C" void flush_backend(void)
#else
void DbFlushBackends(void)
#endif
{
}

#endif /* HAVE_DBI */
