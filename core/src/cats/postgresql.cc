/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2003-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2016 Planets Communications B.V.
   Copyright (C) 2013-2025 Bareos GmbH & Co. KG

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

#include "cats.h"
#include "libpq-fe.h"
#include "postgres_ext.h"     /* needed for NAMEDATALEN */
#include "pg_config_manual.h" /* get NAMEDATALEN on version 8.3 or later */
#include "postgresql.h"
#include "lib/edit.h"
#include "lib/berrno.h"
#include "lib/dlist.h"

/* pull in the generated queries definitions */
#include "postgresql_queries.inc"

/* -----------------------------------------------------------------------
 *
 *   PostgreSQL dependent defines and subroutines
 *
 * -----------------------------------------------------------------------
 */

static pthread_mutex_t db_list_mutex = PTHREAD_MUTEX_INITIALIZER;
static dlist<BareosDbPostgresql>* db_list = NULL;

namespace postgres {

struct result_deleter {
  void operator()(PGresult* result) const { PQclear(result); }
};

using result = std::unique_ptr<PGresult, result_deleter>;

struct retries {
  int amount;
};

result do_query(PGconn* db_handle, const char* query, retries r = {10})
{
  for (int i = 0; i < r.amount; i++) {
    if (i > 1) { Bmicrosleep(5, 0); }
    result res{PQexec(db_handle, query)};
    if (res) {
      auto status = PQresultStatus(res.get());
      if (status != PGRES_TUPLES_OK && status != PGRES_COMMAND_OK) { res = {}; }
      return res;
    }
  }
  return {};
}

const char* strerror(PGconn* db_handle) { return PQerrorMessage(db_handle); }

result try_query(PGconn* db_handle, bool try_reconnection, const char* query)
{
  Dmsg1(500, "try_query starts with '%s'\n", query);

  auto res = do_query(db_handle, query);
  if (!res && try_reconnection) {
    PQreset(db_handle);
    if (PQstatus(db_handle) == CONNECTION_OK) {
      if (do_query(db_handle,
                   "SET datestyle TO 'ISO, YMD';"
                   "SET cursor_tuple_fraction=1;"
                   "SET standard_conforming_strings=on;"
                   "SET client_min_messages TO WARNING;",
                   retries{1})) {
        res = do_query(db_handle, query);
      }
    }
  }
  if (res) {
    Dmsg1(500, "try_query suceeded with query %s", query);
    Dmsg0(500, "We have a result\n");
  } else {
    Dmsg1(500, "try_query failed with query %s", query);
    Dmsg1(50, "Result status fatal: %s, %s\n", query, strerror(db_handle));
  }
  return res;
}
};  // namespace postgres

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
#if defined(USE_BATCH_FILE_INSERT)
    have_batch_insert_ = PQisthreadsafe();
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

  if (!SqlQueryWithoutHandler("SELECT getdatabaseencoding()")) {
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
const char* BareosDbPostgresql::OpenDatabase(JobControlRecord* jcr)
{
  int errstat;
  char buf[10], *port;


  struct pthread_lock {
    pthread_mutex_t* mut;

    pthread_lock(pthread_mutex_t& mut_) : mut{&mut_} { lock_mutex(*mut); }

    ~pthread_lock() { unlock_mutex(*mut); }
  };

  pthread_lock _{db_list_mutex};
  if (connected_) { return nullptr; }

  if ((errstat = RwlInit(&lock_)) != 0) {
    BErrNo be;
    Mmsg1(errmsg, T_("Unable to initialize DB lock. ERR=%s\n"),
          be.bstrerror(errstat));
    return errmsg;
  }

  DbLocker db_lock{this};

  if (db_port_) {
    Bsnprintf(buf, sizeof(buf), "%d", db_port_);
    port = buf;
  } else {
    port = NULL;
  }

  std::vector<const char*> keys;
  std::vector<const char*> values;
  if (db_address_) {
    keys.emplace_back("host");
    values.emplace_back(db_address_);
  }
  if (port) {
    keys.emplace_back("port");
    values.emplace_back(port);
  }
  if (db_name_) {
    keys.emplace_back("dbname");
    values.emplace_back(db_name_);
  }
  if (db_user_) {
    keys.emplace_back("user");
    values.emplace_back(db_user_);
  }
  if (db_password_) {
    keys.emplace_back("password");
    values.emplace_back(db_password_);
  }
  keys.emplace_back("sslmode");
  values.emplace_back("disable");

  keys.emplace_back(nullptr);
  values.emplace_back(nullptr);
  ASSERT(keys.size() == values.size());
  // If connection fails, try at 5 sec intervals for 30 seconds.
  for (int retry = 0; retry < 6; retry++) {
    db_handle_ = PQconnectdbParams(keys.data(), values.data(), true);

    // If connecting does not succeed, try again in case it was a timing
    // problem
    if (PQstatus(db_handle_) == CONNECTION_OK) { break; }

    const char* err = PQerrorMessage(db_handle_);
    if (!err) { err = "unknown reason"; }
    Dmsg1(50, "Could not connect to db: Err=%s\n", err);
    Mmsg2(errmsg,
          T_("Unable to connect to PostgreSQL server. Database=%s User=%s\n"
             "Possible causes: SQL server not running; password incorrect; "
             "server requires ssl; max_connections exceeded.\n(%s)\n"),
          db_name_, db_user_, err);

    // free memory if not successful
    PQfinish(db_handle_);
    db_handle_ = nullptr;

    Bmicrosleep(5, 0);
  }

  Dmsg0(50, "pg_real_connect %s\n", db_handle_ ? "ok" : "failed");
  Dmsg3(50, "db_user=%s db_name=%s db_password=%s\n", db_user_, db_name_,
        (db_password_ == NULL) ? "(NULL)" : db_password_);

  if (!db_handle_) { return errmsg; }

  connected_ = true;
  if (!CheckTablesVersion(jcr)) { return errmsg; }

  SqlQueryWithoutHandler("SET datestyle TO 'ISO, YMD'");
  SqlQueryWithoutHandler("SET cursor_tuple_fraction=1");
  SqlQueryWithoutHandler("SET client_min_messages TO WARNING");

  /* Tell PostgreSQL we are using standard conforming strings
   * and avoid warnings such as:
   *  WARNING:  nonstandard use of \\ in a string literal */
  SqlQueryWithoutHandler("SET standard_conforming_strings=on");

  // Check that encoding is SQL_ASCII
  CheckDatabaseEncoding(jcr);

  return nullptr;
}

void BareosDbPostgresql::CloseDatabase(JobControlRecord* jcr)
{
  if (connected_) { EndTransaction(jcr); }
  lock_mutex(db_list_mutex);
  ref_count_--;
  if (ref_count_ == 0) {
    if (connected_) { SqlFreeResult(); }
    db_list->remove(this);
    if (db_handle_) { PQfinish(db_handle_); }
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
  unlock_mutex(db_list_mutex);
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
  DbLocker _{this};
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
  DbLocker _{this};
  size_t new_len;
  unsigned char* obj;

  obj = PQescapeByteaConn(db_handle_, (unsigned const char*)old, len, &new_len);
  if (!obj) {
    Jmsg(jcr, M_FATAL, 0, T_("PQescapeByteaConn returned NULL.\n"));
    return nullptr;
  }

  if (esc_obj) {
    /* from the PQescapeByteaConn documentation:
     * [..] This result string length includes the terminating zero byte of the
     * result. [...] A terminating zero byte is also added. [...]
     * So this is unnecessary: */
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
  DbLocker _{this};

  if (jcr && jcr->cached_attribute) {
    Dmsg0(400, "Flush last cached attribute.\n");
    if (!CreateAttributesRecord(jcr, jcr->ar)) {
      Jmsg1(jcr, M_FATAL, 0, T_("Attribute create error. %s"), strerror());
    }
    jcr->cached_attribute = false;
  }

  if (!allow_transactions_) { return; }


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

  Dmsg1(500, "BigSqlQuery starts with '%s'\n", query);

  /* This code handles only SELECT queries */
  if (!bstrncasecmp(query, "SELECT", 6)) {
    return SqlQueryWithHandler(query, ResultHandler, ctx);
  }

  if (!ResultHandler) { /* no need of big_query without handler */
    return false;
  }

  DbLocker _{this};

  bool in_transaction = transaction_;
  if (!in_transaction) { /* CURSOR needs transaction */
    SqlQueryWithoutHandler("BEGIN");
  }

  Mmsg(buf_, "DECLARE _bar_cursor CURSOR FOR %s", query);

  if (!SqlQueryWithoutHandler(buf_)) {
    Mmsg(errmsg, T_("Query failed: %s: ERR=%s\n"), buf_, sql_strerror());
    Dmsg0(50, "SqlQueryWithoutHandler failed\n");
    goto bail_out;
  }

  do {
    if (!SqlQueryWithoutHandler("FETCH 100 FROM _bar_cursor")) {
      goto bail_out;
    }
    while ((row = SqlFetchRow()) != NULL) {
      Dmsg1(500, "Fetching %d rows\n", num_rows_);
      if (ResultHandler(ctx, num_fields_, row)) break;
    }
    PQclear(result_);
    result_ = NULL;

  } while (num_rows_ > 0);

  SqlQueryWithoutHandler("CLOSE _bar_cursor");

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
  if (!SqlQueryWithoutHandler(query)) {
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
 * This is where QueryDb comes with Postgresql.
 *
 * Returns:  true  on success
 *           false on failure
 */
bool BareosDbPostgresql::SqlQueryWithoutHandler(const char* query,
                                                query_flags flags)
{
  CheckOwnership();
  auto result
      = postgres::try_query(db_handle_, try_reconnect_ && !transaction_, query);
  if (result) {
    if (!flags.test(query_flag::DiscardResult)) {
      PQclear(result_);
      result_ = result.release();
      field_number_ = -1;
      fields_fetched_ = false;
      num_fields_ = (int)PQnfields(result_);
      Dmsg1(500, "We have %d fields\n", num_fields_);
      num_rows_ = PQntuples(result_);
      Dmsg1(500, "We have %d rows\n", num_rows_);
      row_number_ = 0; /* we can start to fetch something */
    }
    return true;
  } else {
    return false;
  }
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
  fields_fetched_ = false;
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
    Dmsg1(500, "we need space for %" PRIuz " bytes\n",
          sizeof(char*) * num_fields_);
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
  CheckOwnership();
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
    Dmsg2(500, "got value '%s' which became %" PRIu64 "\n",
          PQgetvalue(pg_result, 0, 0), id);
  } else {
    Dmsg1(50, "Result status failed: %s\n", getkeyval_query);
    Mmsg1(errmsg, T_("error fetching currval: %s\n"),
          PQerrorMessage(db_handle_));
  }

bail_out:
  PQclear(pg_result);

  return id;
}

static void ComputeFields(int num_fields,
                          int num_rows,
                          SQL_FIELD fields[/* num_fields */],
                          PGresult* result)
{
  // For a given column, find the max length.
  for (int fidx = 0; fidx < num_fields; ++fidx) { fields[fidx].max_length = 0; }

  for (int ridx = 0; ridx < num_rows; ++ridx) {
    for (int fidx = 0; fidx < num_fields; ++fidx) {
      int length = PQgetisnull(result, ridx, fidx)
                       ? 4 /* "NULL" */
                       : cstrlen(PQgetvalue(result, ridx, fidx));

      if (fields[fidx].max_length < length) {
        fields[fidx].max_length = length;
      }
    }
  }

  for (int fidx = 0; fidx < num_fields; ++fidx) {
    Dmsg1(500, "filling field %d\n", fidx);
    fields[fidx].name = PQfname(result, fidx);
    fields[fidx].type = PQftype(result, fidx);
    fields[fidx].flags = 0;
    Dmsg4(500,
          "ComputeFields finds field '%s' has length='%d' type='%d' and "
          "IsNull=%d\n",
          fields[fidx].name, fields[fidx].max_length, fields[fidx].type,
          fields[fidx].flags);
  }
}

SQL_FIELD* BareosDbPostgresql::SqlFetchField(void)
{
  Dmsg0(500, "SqlFetchField starts\n");

  if (field_number_ >= num_fields_) {
    Dmsg1(100, "requesting field number %d, but only %d fields given\n",
          field_number_, num_fields_);
    return nullptr;
  }

  if (!fields_fetched_) {
    if (!fields_ || fields_size_ < num_fields_) {
      fields_fetched_ = false;
      if (fields_) {
        free(fields_);
        fields_ = NULL;
      }
      Dmsg1(500, "allocating space for %d fields\n", num_fields_);
      fields_ = (SQL_FIELD*)malloc(sizeof(SQL_FIELD) * num_fields_);
      fields_size_ = num_fields_;
    }

    ComputeFields(num_fields_, num_rows_, fields_, result_);

    fields_fetched_ = true;
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
    case 20:   /* int8 (8-byte) */
    case 21:   /* int2 (2-byte) */
    case 23:   /* int4 (4-byte) */
    case 700:  /* float4 (single precision) */
    case 701:  /* float8 (double precision) */
    case 1700: /* numeric + decimal */
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
  lock_mutex(db_list_mutex); /* lock DB queue */

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
  unlock_mutex(db_list_mutex);
  return mdb;
}

bool BareosDbPostgresql::SqlBatchStartFileTable(JobControlRecord*)
{
  CheckOwnership();
  const char* query = "COPY batch FROM STDIN";

  Dmsg0(500, "SqlBatchStartFileTable started\n");

  if (!SqlQueryWithoutHandler("CREATE TEMPORARY TABLE batch ("
                              "FileIndex int,"
                              "JobId int,"
                              "Path varchar,"
                              "Name varchar,"
                              "LStat varchar,"
                              "Md5 varchar,"
                              "DeltaSeq smallint,"
                              "Fhinfo NUMERIC(20),"
                              "Fhnode NUMERIC(20))")) {
    Dmsg0(500, "SqlBatchStartFileTable failed\n");
    return false;
  }

  // We are starting a new query.  reset everything.
  num_rows_ = -1;
  row_number_ = -1;
  field_number_ = -1;

  SqlFreeResult();

  postgres::result res;
  for (int i = 0; i < 10; i++) {
    res.reset(PQexec(db_handle_, query));
    if (res) { break; }
    Bmicrosleep(5, 0);
  }
  if (!res) {
    Dmsg1(50, "Query failed: %s\n", query);
    goto bail_out;
  }

  {
    auto status = PQresultStatus(res.get());
    if (status == PGRES_COPY_IN) {
      num_fields_ = (int)PQnfields(res.get());
      num_rows_ = 0;
    } else {
      Dmsg1(50, "Result status failed: %s\n", query);
      goto bail_out;
    }
  }

  Dmsg0(500, "SqlBatchStartFileTable finishing\n");

  result_ = res.release();

  return true;

bail_out:
  Mmsg1(errmsg, T_("error starting batch mode: %s"),
        PQerrorMessage(db_handle_));
  return false;
}

// Set error to something to abort operation
bool BareosDbPostgresql::SqlBatchEndFileTable(JobControlRecord*,
                                              const char* error)
{
  CheckOwnership();
  int res;
  int count = 30;
  PGresult* pg_result;

  Dmsg0(500, "SqlBatchEndFileTable started\n");

  do {
    res = PQputCopyEnd(db_handle_, error);
  } while (res == 0 && --count > 0);

  if (res == 1) { Dmsg0(500, "ok\n"); }

  if (res <= 0) {
    Dmsg0(500, "we failed\n");
    Mmsg1(errmsg, T_("error ending batch mode: %s"),
          PQerrorMessage(db_handle_));
    Dmsg1(500, "failure %s\n", errmsg);
  }

  pg_result = PQgetResult(db_handle_);
  if (PQresultStatus(pg_result) != PGRES_COMMAND_OK) {
    Mmsg1(errmsg, T_("error ending batch mode: %s"),
          PQerrorMessage(db_handle_));
  }

  PQclear(pg_result);

  Dmsg0(500, "SqlBatchEndFileTable finishing\n");

  return true;
}

/**
 * Escape strings so that PostgreSQL is happy on COPY
 *
 *   NOTE! len is the length of the old string. Your new
 *         string must be long enough (max 2*old+1) to hold
 *         the escaped output.
 */
static char* pgsql_copy_escape(char* dest, const char* src, size_t len)
{
  char c = '\0';

  while (len > 0 && *src) {
    switch (*src) {
      case '\b':
        c = 'b';
        break;
      case '\f':
        c = 'f';
        break;
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
      case '\v':
        c = 'v';
        break;
      case '\'':
        c = '\'';
        break;
      default:
        c = '\0';
        break;
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

bool BareosDbPostgresql::SqlBatchInsertFileTable(JobControlRecord*,
                                                 AttributesDbRecord* ar)
{
  int res;
  int count = 30;
  size_t len;
  const char* digest;
  char ed1[50], ed2[50], ed3[50];

  CheckOwnership();
  esc_name = CheckPoolMemorySize(esc_name, fnl * 2 + 1);
  pgsql_copy_escape(esc_name, fname, fnl);

  esc_path = CheckPoolMemorySize(esc_path, pnl * 2 + 1);
  pgsql_copy_escape(esc_path, path, pnl);

  if (ar->Digest == NULL || ar->Digest[0] == 0) {
    digest = "0";
  } else {
    digest = ar->Digest;
  }

  len = Mmsg(cmd, "%u\t%s\t%s\t%s\t%s\t%s\t%u\t%s\t%s\n", ar->FileIndex,
             edit_int64(ar->JobId, ed1), esc_path, esc_name, ar->attr, digest,
             ar->DeltaSeq, edit_uint64(ar->Fhinfo, ed2),
             edit_uint64(ar->Fhnode, ed3));

  do {
    res = PQputCopyData(db_handle_, cmd, len);
  } while (res == 0 && --count > 0);

  if (res == 1) {
    Dmsg0(500, "ok\n");
    changes++;
  }

  if (res <= 0) {
    Dmsg0(500, "we failed\n");
    Mmsg1(errmsg, T_("error copying in batch mode: %s"),
          PQerrorMessage(db_handle_));
    Dmsg1(500, "failure %s\n", errmsg);
  }

  Dmsg0(500, "SqlBatchInsertFileTable finishing\n");

  return true;
}
