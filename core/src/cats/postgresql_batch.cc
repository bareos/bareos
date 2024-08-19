/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2003-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2016 Planets Communications B.V.
   Copyright (C) 2013-2024 Bareos GmbH & Co. KG

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

bool BareosDbPostgresql::SqlBatchStartFileTable(JobControlRecord*)
{
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

  for (int i = 0; i < 10; i++) {
    result_ = PQexec(db_handle_, query);
    if (result_) { break; }
    Bmicrosleep(5, 0);
  }
  if (!result_) {
    Dmsg1(50, "Query failed: %s\n", query);
    goto bail_out;
  }

  status_ = PQresultStatus(result_);
  if (status_ == PGRES_COPY_IN) {
    num_fields_ = (int)PQnfields(result_);
    num_rows_ = 0;
    status_ = 1;
  } else {
    Dmsg1(50, "Result status failed: %s\n", query);
    goto bail_out;
  }

  Dmsg0(500, "SqlBatchStartFileTable finishing\n");

  return true;

bail_out:
  Mmsg1(errmsg, T_("error starting batch mode: %s"),
        PQerrorMessage(db_handle_));
  status_ = 0;
  PQclear(result_);
  result_ = NULL;
  return false;
}

// Set error to something to abort operation
bool BareosDbPostgresql::SqlBatchEndFileTable(JobControlRecord*,
                                              const char* error)
{
  int res;
  int count = 30;
  PGresult* pg_result;

  Dmsg0(500, "SqlBatchEndFileTable started\n");

  do {
    res = PQputCopyEnd(db_handle_, error);
  } while (res == 0 && --count > 0);

  if (res == 1) {
    Dmsg0(500, "ok\n");
    status_ = 1;
  }

  if (res <= 0) {
    Dmsg0(500, "we failed\n");
    status_ = 0;
    Mmsg1(errmsg, T_("error ending batch mode: %s"),
          PQerrorMessage(db_handle_));
    Dmsg1(500, "failure %s\n", errmsg);
  }

  pg_result = PQgetResult(db_handle_);
  if (PQresultStatus(pg_result) != PGRES_COMMAND_OK) {
    Mmsg1(errmsg, T_("error ending batch mode: %s"),
          PQerrorMessage(db_handle_));
    status_ = 0;
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
    status_ = 1;
  }

  if (res <= 0) {
    Dmsg0(500, "we failed\n");
    status_ = 0;
    Mmsg1(errmsg, T_("error copying in batch mode: %s"),
          PQerrorMessage(db_handle_));
    Dmsg1(500, "failure %s\n", errmsg);
  }

  Dmsg0(500, "SqlBatchInsertFileTable finishing\n");

  return true;
}


/* ************************************* *
 * ** Generic SQL Copy used by dbcopy ** *
 * ************************************* */

class CleanupResult {
 public:
  CleanupResult(PGresult** r, int* s) : result(r), status(s) {}
  void release() { do_cleanup = false; }

  ~CleanupResult()
  {
    if (do_cleanup) {
      *status = 0;
      PQclear(*result);
      *result = nullptr;
    }
  }

 private:
  PGresult** result;
  int* status;
  bool do_cleanup{true};
};

bool BareosDbPostgresql::SqlCopyStart(
    const std::string& table_name,
    const std::vector<std::string>& column_names)
{
  CleanupResult result_cleanup(&result_, &status_);

  num_rows_ = -1;
  row_number_ = -1;
  field_number_ = -1;

  SqlFreeResult();

  std::string query{"COPY " + table_name + " ("};

  for (const auto& column_name : column_names) {
    query += column_name;
    query += ", ";
  }
  query.resize(query.size() - 2);
  query +=
      ") FROM STDIN WITH ("
      "  FORMAT text"
      ", DELIMITER '\t'"
      ")";

  result_ = PQexec(db_handle_, query.c_str());
  if (!result_) {
    Mmsg1(errmsg, T_("error copying in batch mode: %s"),
          PQerrorMessage(db_handle_));
    return false;
  }

  status_ = PQresultStatus(result_);
  if (status_ != PGRES_COPY_IN) {
    Mmsg1(errmsg, T_("Result status failed: %s"), PQerrorMessage(db_handle_));
    return false;
  }

  std::size_t n = (int)PQnfields(result_);
  if (n != column_names.size()) {
    Mmsg1(errmsg, T_("wrong number of rows: %" PRIuz), n);
    return false;
  }

  num_rows_ = 0;
  status_ = 1;

  result_cleanup.release();
  return true;
}

bool BareosDbPostgresql::SqlCopyInsert(
    const std::vector<DatabaseField>& data_fields)
{
  CleanupResult result_cleanup(&result_, &status_);

  std::string query;

  std::vector<char> buffer;
  for (const auto& field : data_fields) {
    if (strlen(field.data_pointer) != 0U) {
      buffer.resize(strlen(field.data_pointer) * 2 + 1);
      pgsql_copy_escape(buffer.data(), field.data_pointer, buffer.size());
      query += buffer.data();
    }
    query += "\t";
  }
  query.resize(query.size() - 1);
  query += "\n";

  int res = 0;
  int count = 30;

  do {
    res = PQputCopyData(db_handle_, query.data(), query.size());
  } while (res == 0 && --count > 0);

  if (res == 1) { status_ = 1; }

  if (res <= 0) {
    status_ = 0;
    Mmsg1(errmsg, T_("error copying in batch mode: %s"),
          PQerrorMessage(db_handle_));
  }
  return true;
}

bool BareosDbPostgresql::SqlCopyEnd()
{
  int res;
  int count = 30;

  CleanupResult result_cleanup(&result_, &status_);

  do {
    res = PQputCopyEnd(db_handle_, nullptr);
  } while (res == 0 && --count > 0);

  if (res <= 0) {
    Mmsg1(errmsg, T_("error ending batch mode: %s"),
          PQerrorMessage(db_handle_));
    return false;
  }

  status_ = 1;

  result_ = PQgetResult(db_handle_);
  if (PQresultStatus(result_) != PGRES_COMMAND_OK) {
    Mmsg1(errmsg, T_("error ending batch mode: %s"),
          PQerrorMessage(db_handle_));
    return false;
  }

  result_cleanup.release();
  return true;
}


#endif  // HAVE_POSTGRESQL
