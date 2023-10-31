/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2003-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2016 Planets Communications B.V.
   Copyright (C) 2013-2023 Bareos GmbH & Co. KG

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

#include "include/bareos.h"

#ifdef HAVE_POSTGRESQL

#  include "cats.h"
#  include "libpq-fe.h"
#  include "postgres_ext.h"     /* needed for NAMEDATALEN */
#  include "pg_config_manual.h" /* get NAMEDATALEN on version 8.3 or later */
#  include "postgresql.h"
#  include "lib/edit.h"
#  include "lib/berrno.h"
#  include "lib/dlist.h"

/* pull in the generated queries definitions */
#  include "postgresql_queries.inc"

/* -----------------------------------------------------------------------
 *
 *   PostgreSQL dependent defines and subroutines
 *
 * -----------------------------------------------------------------------
 */

static dlist<BareosDbPostgresql>* db_list = NULL;

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

BareosDbPostgresql::BareosDbPostgresql(JobControlRecord*,
                                       const char*,
                                       const char* db_name,
                                       const char* db_user,
                                       const char* db_password,
                                       const char* db_address,
                                       int db_port,
                                       const char* db_socket,
                                       bool mult_db_connections,
                                       bool disable_batch_insert,
                                       bool try_reconnect,
                                       bool exit_on_fatal,
                                       bool need_private)
{
  // Initialize the parent class members.
  db_interface_type_ = SQL_INTERFACE_TYPE_POSTGRESQL;
  db_type_ = SQL_TYPE_POSTGRESQL;
  db_driver_ = strdup("PostgreSQL");
  db_name_ = strdup(db_name);
  db_user_ = strdup(db_user);
  if (db_password) { db_password_ = strdup(db_password); }
  if (db_address) { db_address_ = strdup(db_address); }
  if (db_socket) { db_socket_ = strdup(db_socket); }
  db_port_ = db_port;
  if (disable_batch_insert) {
    disabled_batch_insert_ = true;
    have_batch_insert_ = false;
  } else {
    disabled_batch_insert_ = false;
#  if defined(USE_BATCH_FILE_INSERT)
#    if defined(HAVE_POSTGRESQL_BATCH_FILE_INSERT) \
        || defined(HAVE_PQISTHREADSAFE)
#      ifdef HAVE_PQISTHREADSAFE
    have_batch_insert_ = PQisthreadsafe();
#      else
    have_batch_insert_ = true;
#      endif /* HAVE_PQISTHREADSAFE */
#    else
    have_batch_insert_ = true;
#    endif /* HAVE_POSTGRESQL_BATCH_FILE_INSERT || HAVE_PQISTHREADSAFE */
#  else
    have_batch_insert_ = false;
#  endif /* USE_BATCH_FILE_INSERT */
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
  buf_ = GetPoolMemory(PM_FNAME);
  allow_transactions_ = mult_db_connections;
  is_private_ = need_private;
  try_reconnect_ = try_reconnect;
  exit_on_fatal_ = exit_on_fatal;
  last_hash_key_ = 0;
  last_query_text_ = NULL;

  // Initialize the private members.
  db_handle_ = NULL;
  result_ = NULL;

  // Put the db in the list.
  if (db_list == NULL) { db_list = new dlist<BareosDbPostgresql>(); }
  db_list->append(this);

  /* make the queries available using the queries variable from the parent class
   */
  queries = query_definitions;
}

BareosDbPostgresql::~BareosDbPostgresql() {}

// Check that the database correspond to the encoding we want
bool BareosDbPostgresql::CheckDatabaseEncoding(JobControlRecord* jcr)
{
  SQL_ROW row;
  bool retval = false;

  if (!SqlQueryWithoutHandler("SELECT getdatabaseencoding()",
                              QF_STORE_RESULT)) {
    Jmsg(jcr, M_ERROR, 0, "%s", errmsg);
    return false;
  }

  if ((row = SqlFetchRow()) == NULL) {
    Mmsg1(errmsg, T_("error fetching row: %s\n"), errmsg);
    Jmsg(jcr, M_ERROR, 0, "Can't check database encoding %s", errmsg);
  } else {
    retval = bstrcmp(row[0], "SQL_ASCII");

    if (retval) {
      /* If we are in SQL_ASCII, we can force the client_encoding to SQL_ASCII
       * too */
      SqlQueryWithoutHandler("SET client_encoding TO 'SQL_ASCII'");
    } else {
      // Something is wrong with database encoding
      Mmsg(errmsg,
           T_("Encoding error for database \"%s\". Wanted SQL_ASCII, got %s\n"),
           get_db_name(), row[0]);
      Jmsg(jcr, M_WARNING, 0, "%s", errmsg);
      Dmsg1(50, "%s", errmsg);
    }
  }

  return retval;
}

/**
 * Now actually open the database.  This can generate errors, which are returned
 * in the errmsg
 *
 * DO NOT close the database or delete mdb here !!!!
 */
bool BareosDbPostgresql::OpenDatabase(JobControlRecord* jcr)
{
  bool retval = false;
  int errstat;
  char buf[10], *port;

  lock_mutex(mutex);
  if (connected_) {
    retval = true;
    goto bail_out;
  }

  if ((errstat = RwlInit(&lock_)) != 0) {
    BErrNo be;
    Mmsg1(errmsg, T_("Unable to initialize DB lock. ERR=%s\n"),
          be.bstrerror(errstat));
    goto bail_out;
  }

  if (db_port_) {
    Bsnprintf(buf, sizeof(buf), "%d", db_port_);
    port = buf;
  } else {
    port = NULL;
  }

  // If connection fails, try at 5 sec intervals for 30 seconds.
  for (int retry = 0; retry < 6; retry++) {
    db_handle_ = PQsetdbLogin(db_address_,   /* default = localhost */
                              port,          /* default port */
                              NULL,          /* pg options */
                              NULL,          /* tty, ignored */
                              db_name_,      /* database name */
                              db_user_,      /* login name */
                              db_password_); /* password */

    // If no connect, try once more in case it is a timing problem
    if (PQstatus(db_handle_) == CONNECTION_OK) { break; }

    Bmicrosleep(5, 0);
  }

  Dmsg0(50, "pg_real_connect %s\n",
        PQstatus(db_handle_) == CONNECTION_OK ? "ok" : "failed");
  Dmsg3(50, "db_user=%s db_name=%s db_password=%s\n", db_user_, db_name_,
        (db_password_ == NULL) ? "(NULL)" : db_password_);

  if (PQstatus(db_handle_) != CONNECTION_OK) {
    Mmsg2(errmsg,
          T_("Unable to connect to PostgreSQL server. Database=%s User=%s\n"
             "Possible causes: SQL server not running; password incorrect; "
             "max_connections exceeded.\n(%s)\n"),
          db_name_, db_user_, PQerrorMessage(db_handle_));
    goto bail_out;
  }

  connected_ = true;
  if (!CheckTablesVersion(jcr)) { goto bail_out; }

  SqlQueryWithoutHandler("SET datestyle TO 'ISO, YMD'");
  SqlQueryWithoutHandler("SET cursor_tuple_fraction=1");

  /* Tell PostgreSQL we are using standard conforming strings
   * and avoid warnings such as:
   *  WARNING:  nonstandard use of \\ in a string literal */
  SqlQueryWithoutHandler("SET standard_conforming_strings=on");

  // Check that encoding is SQL_ASCII
  CheckDatabaseEncoding(jcr);

  retval = true;

bail_out:
  unlock_mutex(mutex);
  return retval;
}

void BareosDbPostgresql::CloseDatabase(JobControlRecord* jcr)
{
  if (connected_) { EndTransaction(jcr); }
  lock_mutex(mutex);
  ref_count_--;
  if (ref_count_ == 0) {
    if (connected_) { SqlFreeResult(); }
    db_list->remove(this);
    if (connected_ && db_handle_) { PQfinish(db_handle_); }
    if (RwlIsInit(&lock_)) { RwlDestroy(&lock_); }
    FreePoolMemory(errmsg);
    FreePoolMemory(cmd);
    FreePoolMemory(cached_path);
    FreePoolMemory(fname);
    FreePoolMemory(path);
    FreePoolMemory(esc_name);
    FreePoolMemory(esc_path);
    FreePoolMemory(esc_obj);
    FreePoolMemory(buf_);
    if (db_driver_) { free(db_driver_); }
    if (db_name_) { free(db_name_); }
    if (db_user_) { free(db_user_); }
    if (db_password_) { free(db_password_); }
    if (db_address_) { free(db_address_); }
    if (db_socket_) { free(db_socket_); }
    delete this;
    if (db_list->size() == 0) {
      delete db_list;
      db_list = NULL;
    }
  }
  unlock_mutex(mutex);
}

bool BareosDbPostgresql::ValidateConnection(void)
{
  // Perform a null query to see if the connection is still valid.
  DbLocker _{this};
  if (!SqlQueryWithoutHandler("SELECT 1", true)) {
    // Try resetting the connection.
    PQreset(db_handle_);
    if (PQstatus(db_handle_) != CONNECTION_OK) { return false; }

    SqlQueryWithoutHandler("SET datestyle TO 'ISO, YMD'");
    SqlQueryWithoutHandler("SET cursor_tuple_fraction=1");
    SqlQueryWithoutHandler("SET standard_conforming_strings=on");

    // Retry the null query.
    if (!SqlQueryWithoutHandler("SELECT 1", true)) { return false; }
  }

  SqlFreeResult();

  return true;
}

/**
 * Escape strings so that PostgreSQL is happy
 *
 *   NOTE! len is the length of the old string. Your new
 *         string must be long enough (max 2*old+1) to hold
 *         the escaped output.
 */
void BareosDbPostgresql::EscapeString(JobControlRecord* jcr,
                                      char* snew,
                                      const char* old,
                                      int len)
{
  int error;

  PQescapeStringConn(db_handle_, snew, old, len, &error);
  if (error) {
    Jmsg(jcr, M_FATAL, 0, T_("PQescapeStringConn returned non-zero.\n"));
    /* error on encoding, probably invalid multibyte encoding in the source
      string see PQescapeStringConn documentation for details. */
    Dmsg0(500, "PQescapeStringConn failed\n");
  }
}

/**
 * Escape binary so that PostgreSQL is happy
 *
 */
char* BareosDbPostgresql::EscapeObject(JobControlRecord* jcr,
                                       char* old,
                                       int len)
{
  size_t new_len;
  unsigned char* obj;

  obj = PQescapeByteaConn(db_handle_, (unsigned const char*)old, len, &new_len);
  if (!obj) {
    Jmsg(jcr, M_FATAL, 0, T_("PQescapeByteaConn returned NULL.\n"));
    return nullptr;
  }

  if (esc_obj) {
    esc_obj = CheckPoolMemorySize(esc_obj, new_len + 1);
    if (esc_obj) {
      memcpy(esc_obj, obj, new_len);
      esc_obj[new_len] = 0;
    }
  }

  if (!esc_obj) { Jmsg(jcr, M_FATAL, 0, T_("esc_obj is NULL.\n")); }

  PQfreemem(obj);

  return (char*)esc_obj;
}

unsigned char* BareosDbPostgresql::EscapeObject(const unsigned char* old,
                                                std::size_t old_len,
                                                std::size_t& new_len)
{
  return PQescapeByteaConn(db_handle_, old, old_len, std::addressof(new_len));
}

void BareosDbPostgresql::FreeEscapedObjectMemory(unsigned char* obj)
{
  PQfreemem(obj);
}


/**
 * Unescape binary object so that PostgreSQL is happy
 *
 */
void BareosDbPostgresql::UnescapeObject(JobControlRecord* jcr,
                                        char* from,
                                        int32_t,
                                        POOLMEM*& dest,
                                        int32_t* dest_len)
{
  size_t new_len;
  unsigned char* obj;

  if (!dest || !dest_len) { return; }

  if (!from) {
    dest[0] = '\0';
    *dest_len = 0;
    return;
  }

  obj = PQunescapeBytea((unsigned const char*)from, &new_len);

  if (!obj) {
    Jmsg(jcr, M_FATAL, 0, T_("PQunescapeByteaConn returned NULL.\n"));
    return;
  }

  *dest_len = new_len;
  dest = CheckPoolMemorySize(dest, new_len + 1);
  if (dest) {
    memcpy(dest, obj, new_len);
    dest[new_len] = '\0';
  }

  PQfreemem(obj);

  Dmsg1(010, "obj size: %d\n", *dest_len);
}

/**
 * Start a transaction. This groups inserts and makes things
 * much more efficient. Usually started when inserting
 * file attributes.
 */
void BareosDbPostgresql::StartTransaction(JobControlRecord* jcr)
{
  if (!jcr->attr) { jcr->attr = GetPoolMemory(PM_FNAME); }
  if (!jcr->ar) {
    jcr->ar = (AttributesDbRecord*)malloc(sizeof(AttributesDbRecord));
  }

  /* This is turned off because transactions break
   * if multiple simultaneous jobs are run. */
  if (!allow_transactions_) { return; }

  DbLocker _{this};
  // Allow only 25,000 changes per transaction
  if (transaction_ && changes > 25000) { EndTransaction(jcr); }
  if (!transaction_) {
    SqlQueryWithoutHandler("BEGIN"); /* begin transaction */
    Dmsg0(400, "Start PosgreSQL transaction\n");
    transaction_ = true;
  }
}

void BareosDbPostgresql::EndTransaction(JobControlRecord* jcr)
{
  if (jcr && jcr->cached_attribute) {
    Dmsg0(400, "Flush last cached attribute.\n");
    if (!CreateAttributesRecord(jcr, jcr->ar)) {
      Jmsg1(jcr, M_FATAL, 0, T_("Attribute create error. %s"), strerror());
    }
    jcr->cached_attribute = false;
  }

  if (!allow_transactions_) { return; }

  DbLocker _{this};
  if (transaction_) {
    SqlQueryWithoutHandler("COMMIT"); /* end transaction */
    transaction_ = false;
    Dmsg1(400, "End PostgreSQL transaction changes=%d\n", changes);
  }
  changes = 0;
}

/**
 * Submit a general SQL command (cmd), and for each row returned,
 * the ResultHandler is called with the ctx.
 */
bool BareosDbPostgresql::BigSqlQuery(const char* query,
                                     DB_RESULT_HANDLER* ResultHandler,
                                     void* ctx)
{
  SQL_ROW row;
  bool retval = false;
  bool in_transaction = transaction_;

  Dmsg1(500, "BigSqlQuery starts with '%s'\n", query);

  /* This code handles only SELECT queries */
  if (!bstrncasecmp(query, "SELECT", 6)) {
    return SqlQueryWithHandler(query, ResultHandler, ctx);
  }

  if (!ResultHandler) { /* no need of big_query without handler */
    return false;
  }

  DbLocker _{this};

  if (!in_transaction) { /* CURSOR needs transaction */
    SqlQueryWithoutHandler("BEGIN");
  }

  Mmsg(buf_, "DECLARE _bac_cursor CURSOR FOR %s", query);

  if (!SqlQueryWithoutHandler(buf_)) {
    Mmsg(errmsg, T_("Query failed: %s: ERR=%s\n"), buf_, sql_strerror());
    Dmsg0(50, "SqlQueryWithoutHandler failed\n");
    goto bail_out;
  }

  do {
    if (!SqlQueryWithoutHandler("FETCH 100 FROM _bac_cursor")) {
      goto bail_out;
    }
    while ((row = SqlFetchRow()) != NULL) {
      Dmsg1(500, "Fetching %d rows\n", num_rows_);
      if (ResultHandler(ctx, num_fields_, row)) break;
    }
    PQclear(result_);
    result_ = NULL;

  } while (num_rows_ > 0);

  SqlQueryWithoutHandler("CLOSE _bac_cursor");

  Dmsg0(500, "BigSqlQuery finished\n");
  SqlFreeResult();
  retval = true;

bail_out:
  if (!in_transaction) {
    SqlQueryWithoutHandler("COMMIT"); /* end transaction */
  }

  return retval;
}

/**
 * Submit a general SQL command (cmd), and for each row returned,
 * the ResultHandler is called with the ctx.
 */
bool BareosDbPostgresql::SqlQueryWithHandler(const char* query,
                                             DB_RESULT_HANDLER* ResultHandler,
                                             void* ctx)
{
  SQL_ROW row;

  Dmsg1(500, "SqlQueryWithHandler starts with '%s'\n", query);

  DbLocker _{this};
  if (!SqlQueryWithoutHandler(query, QF_STORE_RESULT)) {
    Mmsg(errmsg, T_("Query failed: %s: ERR=%s\n"), query, sql_strerror());
    Dmsg0(500, "SqlQueryWithHandler failed\n");
    return false;
  }

  Dmsg0(500, "SqlQueryWithHandler succeeded. checking handler\n");

  if (ResultHandler != NULL) {
    Dmsg0(500, "SqlQueryWithHandler invoking handler\n");
    while ((row = SqlFetchRow()) != NULL) {
      Dmsg0(500, "SqlQueryWithHandler SqlFetchRow worked\n");
      if (ResultHandler(ctx, num_fields_, row)) break;
    }
    SqlFreeResult();
  }

  Dmsg0(500, "SqlQueryWithHandler finished\n");

  return true;
}

/**
 * Note, if this routine returns false (failure), BAREOS expects
 * that no result has been stored.
 * This is where QUERY_DB comes with Postgresql.
 *
 * Returns:  true  on success
 *           false on failure
 */
bool BareosDbPostgresql::SqlQueryWithoutHandler(const char* query, int)
{
  int i;
  bool retry = true;
  bool retval = false;

  Dmsg1(500, "SqlQueryWithoutHandler starts with '%s'\n", query);

  // We are starting a new query. reset everything.
retry_query:
  num_rows_ = -1;
  row_number_ = -1;
  field_number_ = -1;

  if (result_) {
    PQclear(result_); /* hmm, someone forgot to free?? */
    result_ = NULL;
  }

  for (i = 0; i < 10; i++) {
    result_ = PQexec(db_handle_, query);
    if (result_) { break; }
    Bmicrosleep(5, 0);
  }

  status_ = PQresultStatus(result_);
  switch (status_) {
    case PGRES_TUPLES_OK:
    case PGRES_COMMAND_OK:
      Dmsg0(500, "we have a result\n");

      num_fields_ = (int)PQnfields(result_);
      Dmsg1(500, "we have %d fields\n", num_fields_);

      num_rows_ = PQntuples(result_);
      Dmsg1(500, "we have %d rows\n", num_rows_);

      row_number_ = 0; /* we can start to fetch something */
      status_ = 0;     /* succeed */
      retval = true;
      break;
    case PGRES_FATAL_ERROR:
      Dmsg1(50, "Result status fatal: %s, %s\n", query, sql_strerror());
      if (exit_on_fatal_) {
        Emsg1(M_ERROR_TERM, 0, "Fatal database error: %s\n", sql_strerror());
      }

      if (try_reconnect_ && !transaction_) {
        /* Only try reconnecting when no transaction is pending.
         * Reconnecting within a transaction will lead to an aborted
         * transaction anyway so we better follow our old error path. */
        if (retry) {
          PQreset(db_handle_);

          if (PQstatus(db_handle_) == CONNECTION_OK) {
            // Reset the connection settings.
            PQexec(db_handle_, "SET datestyle TO 'ISO, YMD'");
            PQexec(db_handle_, "SET cursor_tuple_fraction=1");
            result_ = PQexec(db_handle_, "SET standard_conforming_strings=on");

            switch (PQresultStatus(result_)) {
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

  Dmsg0(500, "SqlQueryWithoutHandler finishing\n");
  goto ok_out;

bail_out:
  Dmsg0(500, "we failed\n");
  PQclear(result_);
  result_ = NULL;
  status_ = 1; /* failed */

ok_out:
  return retval;
}

void BareosDbPostgresql::SqlFreeResult(void)
{
  DbLocker _{this};
  if (result_) {
    PQclear(result_);
    result_ = NULL;
  }
  if (rows_) {
    free(rows_);
    rows_ = NULL;
  }
  if (fields_) {
    free(fields_);
    fields_ = NULL;
  }
  num_rows_ = num_fields_ = 0;
}

SQL_ROW BareosDbPostgresql::SqlFetchRow(void)
{
  int j;
  SQL_ROW row = NULL; /* by default, return NULL */

  Dmsg0(500, "SqlFetchRow start\n");

  if (num_fields_ == 0) { /* No field, no row */
    Dmsg0(500, "SqlFetchRow finishes returning NULL, no fields\n");
    return NULL;
  }

  if (!rows_ || rows_size_ < num_fields_) {
    if (rows_) {
      Dmsg0(500, "SqlFetchRow freeing space\n");
      free(rows_);
    }
    Dmsg1(500, "we need space for %d bytes\n", sizeof(char*) * num_fields_);
    rows_ = (SQL_ROW)malloc(sizeof(char*) * num_fields_);
    rows_size_ = num_fields_;

    // Now reset the row_number now that we have the space allocated
    row_number_ = 0;
  }

  // If still within the result set
  if (row_number_ >= 0 && row_number_ < num_rows_) {
    Dmsg2(500, "SqlFetchRow row number '%d' is acceptable (0..%d)\n",
          row_number_, num_rows_);
    for (j = 0; j < num_fields_; j++) {
      rows_[j] = PQgetvalue(result_, row_number_, j);
      Dmsg2(500, "SqlFetchRow field '%d' has value '%s'\n", j, rows_[j]);
    }
    // Increment the row number for the next call
    row_number_++;
    row = rows_;
  } else {
    Dmsg2(500, "SqlFetchRow row number '%d' is NOT acceptable (0..%d)\n",
          row_number_, num_rows_);
  }

  Dmsg1(500, "SqlFetchRow finishes returning %p\n", row);

  return row;
}

const char* BareosDbPostgresql::sql_strerror(void)
{
  return PQerrorMessage(db_handle_);
}

void BareosDbPostgresql::SqlDataSeek(int row)
{
  // Set the row number to be returned on the next call to sql_fetch_row
  row_number_ = row;
}

int BareosDbPostgresql::SqlAffectedRows(void)
{
  return (unsigned)str_to_int32(PQcmdTuples(result_));
}

uint64_t BareosDbPostgresql::SqlInsertAutokeyRecord(const char* query,
                                                    const char* table_name)
{
  int i;
  uint64_t id = 0;
  char sequence[NAMEDATALEN - 1];
  char getkeyval_query[NAMEDATALEN + 50];
  PGresult* pg_result;

  // First execute the insert query and then retrieve the currval.
  if (!SqlQueryWithoutHandler(query)) { return 0; }

  num_rows_ = SqlAffectedRows();
  if (num_rows_ != 1) { return 0; }

  changes++;

  /* Obtain the current value of the sequence that
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
   * everything else can use the PostgreSQL formula. */
  if (Bstrcasecmp(table_name, "basefiles")) {
    bstrncpy(sequence, "basefiles_baseid", sizeof(sequence));
  } else {
    bstrncpy(sequence, table_name, sizeof(sequence));
    bstrncat(sequence, "_", sizeof(sequence));
    bstrncat(sequence, table_name, sizeof(sequence));
    bstrncat(sequence, "id", sizeof(sequence));
  }

  bstrncat(sequence, "_seq", sizeof(sequence));
  Bsnprintf(getkeyval_query, sizeof(getkeyval_query), "SELECT currval('%s')",
            sequence);

  Dmsg1(500, "SqlInsertAutokeyRecord executing query '%s'\n", getkeyval_query);
  for (i = 0; i < 10; i++) {
    pg_result = PQexec(db_handle_, getkeyval_query);
    if (pg_result) { break; }
    Bmicrosleep(5, 0);
  }
  if (!pg_result) {
    Dmsg1(50, "Query failed: %s\n", getkeyval_query);
    goto bail_out;
  }

  Dmsg0(500, "exec done\n");

  if (PQresultStatus(pg_result) == PGRES_TUPLES_OK) {
    Dmsg0(500, "getting value\n");
    id = str_to_uint64(PQgetvalue(pg_result, 0, 0));
    Dmsg2(500, "got value '%s' which became %d\n", PQgetvalue(pg_result, 0, 0),
          id);
  } else {
    Dmsg1(50, "Result status failed: %s\n", getkeyval_query);
    Mmsg1(errmsg, T_("error fetching currval: %s\n"),
          PQerrorMessage(db_handle_));
  }

bail_out:
  PQclear(pg_result);

  return id;
}

SQL_FIELD* BareosDbPostgresql::SqlFetchField(void)
{
  int i, j;
  int max_length;
  int this_length;

  Dmsg0(500, "SqlFetchField starts\n");

  if (!fields_ || fields_size_ < num_fields_) {
    if (fields_) {
      free(fields_);
      fields_ = NULL;
    }
    Dmsg1(500, "allocating space for %d fields\n", num_fields_);
    fields_ = (SQL_FIELD*)malloc(sizeof(SQL_FIELD) * num_fields_);
    fields_size_ = num_fields_;

    for (i = 0; i < num_fields_; i++) {
      Dmsg1(500, "filling field %d\n", i);
      fields_[i].name = PQfname(result_, i);
      fields_[i].type = PQftype(result_, i);
      fields_[i].flags = 0;

      // For a given column, find the max length.
      max_length = 0;
      for (j = 0; j < num_rows_; j++) {
        if (PQgetisnull(result_, j, i)) {
          this_length = 4; /* "NULL" */
        } else {
          this_length = cstrlen(PQgetvalue(result_, j, i));
        }

        if (max_length < this_length) { max_length = this_length; }
      }
      fields_[i].max_length = max_length;

      Dmsg4(500,
            "SqlFetchField finds field '%s' has length='%d' type='%d' and "
            "IsNull=%d\n",
            fields_[i].name, fields_[i].max_length, fields_[i].type,
            fields_[i].flags);
    }
  }

  // Increment field number for the next time around
  return &fields_[field_number_++];
}

bool BareosDbPostgresql::SqlFieldIsNotNull(int field_type)
{
  switch (field_type) {
    case 1:
      return true;
    default:
      return false;
  }
}

bool BareosDbPostgresql::SqlFieldIsNumeric(int field_type)
{
  // TEMP: the following is taken from select OID, typname from pg_type;
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
 * Initialize database data structure. In principal this should
 * never have errors, or it is really fatal.
 */

BareosDb* db_init_database(JobControlRecord* jcr,
                           const char* db_driver,
                           const char* db_name,
                           const char* db_user,
                           const char* db_password,
                           const char* db_address,
                           int db_port,
                           const char* db_socket,
                           bool mult_db_connections,
                           bool disable_batch_insert,
                           bool try_reconnect,
                           bool exit_on_fatal,
                           bool need_private)

{
  BareosDbPostgresql* mdb = NULL;

  if (!db_user) {
    Jmsg(jcr, M_FATAL, 0, T_("A user name for PostgreSQL must be supplied.\n"));
    return NULL;
  }
  lock_mutex(mutex); /* lock DB queue */

  // Look to see if DB already open
  if (db_list && !mult_db_connections && !need_private) {
    foreach_dlist (mdb, db_list) {
      if (mdb->IsPrivate()) { continue; }

      if (mdb->MatchDatabase(db_driver, db_name, db_address, db_port)) {
        Dmsg1(100, "DB REopen %s\n", db_name);
        mdb->IncrementRefcount();
        goto bail_out;
      }
    }
  }
  Dmsg0(100, "db_init_database first time\n");
  mdb = new BareosDbPostgresql(jcr, db_driver, db_name, db_user, db_password,
                               db_address, db_port, db_socket,
                               mult_db_connections, disable_batch_insert,
                               try_reconnect, exit_on_fatal, need_private);

bail_out:
  unlock_mutex(mutex);
  return mdb;
}

#endif /* HAVE_POSTGRESQL */
