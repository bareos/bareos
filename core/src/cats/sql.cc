/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2009 Free Software Foundation Europe e.V.
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
// Kern Sibbald, March 2000
/**
 * @file
 * BAREOS Catalog Database interface routines
 *
 * Almost generic set of SQL database interface routines
 * (with a little more work) SQL engine specific routines are in
 * postgresql.c, ...
 */

#include "include/bareos.h"

#include "cats.h"
#include "lib/edit.h"

/* Forward referenced subroutines */

dbid_list::dbid_list()
{
  max_ids = 1000;
  DBId = (DBId_t*)malloc(max_ids * sizeof(DBId_t));
  num_ids = num_seen = tot_ids = 0;
  PurgedFiles = nullptr;
}

dbid_list::~dbid_list() { free(DBId); }

DBId_t dbid_list::get(int i) const
{
  if (i >= size()) {
    Emsg2(
        M_ERROR_TERM, 0,
        T_("Unable to access dbid_list entry %d. Only %d entries available.\n"),
        i, size());
    return (DBId_t)0;
  }
  return DBId[i];
}


int DbIntHandler(void* ctx, int, char** row)
{
  uint32_t* val = (uint32_t*)ctx;

  Dmsg1(800, "int_handler starts with row pointing at %p\n", row);

  if (row[0]) {
    Dmsg1(800, "int_handler finds '%s'\n", row[0]);
    *val = str_to_int64(row[0]);
  } else {
    Dmsg0(800, "int_handler finds zero\n");
    *val = 0;
  }
  Dmsg0(800, "int_handler finishes\n");
  return 0;
}

/**
 * Called here to retrieve a 32/64 bit integer from the database.
 *   The returned integer will be extended to 64 bit.
 */
int db_int64_handler(void* ctx, int, char** row)
{
  db_int64_ctx* lctx = (db_int64_ctx*)ctx;

  if (row[0]) {
    lctx->value = str_to_int64(row[0]);
    lctx->count++;
  }
  return 0;
}

// Use to build a comma separated list of values from a query. "10,20,30"
int DbListHandler(void* ctx, int num_fields, char** row)
{
  db_list_ctx* lctx = (db_list_ctx*)ctx;
  if (num_fields == 1 && row[0]) { lctx->add(row[0]); }
  return 0;
}

// Use to build a vector of Ids from a query.
int DbIdListHandler(void* ctx, int num_fields, char** row)
{
  std::vector<DBId_t>* lctx = (std::vector<DBId_t>*)ctx;
  if (num_fields == 1 && row[0]) { lctx->push_back(std::stoul(row[0])); }
  return 0;
}

/**
 * specific context passed from db_check_max_connections to
 * DbMaxConnectionsHandler.
 */
struct max_connections_context {
  uint32_t nr_connections;
};

static inline int DbMaxConnectionsHandler(void* ctx, int count, char** row)
{
  auto* context = static_cast<struct max_connections_context*>(ctx);

  if (count != 1) {
    Dmsg0(50, "Expected exactly one field, got %d\n", count);
    return -1;
  }

  if (row[0]) {
    context->nr_connections = str_to_int64(row[0]);
  } else {
    Dmsg0(800, "int_handler finds zero\n");
    context->nr_connections = 0;
  }
  return 0;
}

bool BareosDb::CheckMaxConnections(JobControlRecord* jcr,
                                   uint32_t max_concurrent_jobs)
{
  // Without Batch insert, no need to verify max_connections, as all
  // jobs share a connection otherwise
  if (!BatchInsertAvailable()) return true;

  struct max_connections_context context{};
  PoolMem query(PM_MESSAGE);

  // Check max_connections setting
  FillQuery(query, SQL_QUERY::sql_get_max_connections);
  if (!SqlQueryWithHandler(query.c_str(), DbMaxConnectionsHandler, &context)) {
    Jmsg(jcr, M_ERROR, 0, "Can't verify max_connections settings %s", errmsg);
    return false;
  }

  if (context.nr_connections && max_concurrent_jobs
      && max_concurrent_jobs > context.nr_connections) {
    DbLocker _{this};
    Mmsg(errmsg,
         T_("Potential performance problem:\n"
            "max_connections=%d set for %s database \"%s\" should be larger "
            "than Director's "
            "MaxConcurrentJobs=%d\n"),
         context.nr_connections, GetType(), get_db_name(), max_concurrent_jobs);
    Jmsg(jcr, M_WARNING, 0, "%s", errmsg);
    return false;
  }

  return true;
}

bool BareosDb::CheckTablesVersion(JobControlRecord* jcr)
{
  uint32_t bareos_db_version = 0;
  const char* query = "SELECT VersionId FROM Version";

  if (!SqlQueryWithHandler(query, DbIntHandler, (void*)&bareos_db_version)) {
    Jmsg(jcr, M_FATAL, 0, "%s", errmsg);
    return false;
  }

  if (bareos_db_version != BDB_VERSION) {
    DbLocker _{this};
    Mmsg(errmsg, "Version error for database \"%s\". Wanted %d, got %d\n",
         get_db_name(), BDB_VERSION, bareos_db_version);
    Jmsg(jcr, M_FATAL, 0, "%s", errmsg);
    return false;
  }

  return true;
}

/* NOTE!!! The following routines expect that the
 *  calling subroutine sets and clears the mutex */

/**
 * Utility routine for queries. The database MUST be locked before calling here.
 * Returns: false on failure
 *          true on success
 */
bool BareosDb::QueryDb(JobControlRecord* jcr,
                       const char* select_cmd,
                       libbareos::source_location loc)
{
  CheckOwnership();

  SqlFreeResult();
  Dmsg1(1000, "query: %s\n", select_cmd);
  if (!SqlQuery(select_cmd)) {
    msg_(loc.file_name(), loc.line(), errmsg, T_("query %s failed:\n%s\n"),
         select_cmd, sql_strerror());
    j_msg(loc.file_name(), loc.line(), jcr, M_FATAL, 0, "%s", errmsg);
    if (g_verbose) {
      j_msg(loc.file_name(), loc.line(), jcr, M_INFO, 0, "%s\n", select_cmd);
    }
    return false;
  }

  return true;
}

/**
 * Utility routine to do inserts
 * Returns: false on failure
 *          true on success
 */
int BareosDb::InsertDb(JobControlRecord* jcr,
                       const char* select_cmd,
                       libbareos::source_location loc)
{
  CheckOwnership();
  int num_rows;

  if (!SqlQuery(select_cmd)) {
    msg_(loc.file_name(), loc.line(), errmsg, T_("insert %s failed:\n%s\n"),
         select_cmd, sql_strerror());
    j_msg(loc.file_name(), loc.line(), jcr, M_FATAL, 0, "%s", errmsg);
    if (g_verbose) {
      j_msg(loc.file_name(), loc.line(), jcr, M_INFO, 0, "%s\n", select_cmd);
    }
    return -1;
  }
  num_rows = SqlAffectedRows();
  if (num_rows != 1) {
    char ed1[30];
    msg_(loc.file_name(), loc.line(), errmsg,
         T_("Insertion problem: affected_rows=%s\n"),
         edit_uint64(num_rows, ed1));
    if (g_verbose) {
      j_msg(loc.file_name(), loc.line(), jcr, M_INFO, 0, "%s\n", select_cmd);
    }
    return num_rows;
  }
  changes++;
  return num_rows;
}

/**
 * Utility routine for updates.
 * Returns: false on failure
 *          true on success
 */
int BareosDb::UpdateDb(JobControlRecord* jcr,
                       const char* UpdateCmd,
                       libbareos::source_location loc)
{
  CheckOwnership();
  if (!SqlQuery(UpdateCmd)) {
    msg_(loc.file_name(), loc.line(), errmsg, T_("update %s failed:\n%s\n"),
         UpdateCmd, sql_strerror());
    j_msg(loc.file_name(), loc.line(), jcr, M_ERROR, 0, "%s", errmsg);
    if (g_verbose) {
      j_msg(loc.file_name(), loc.line(), jcr, M_INFO, 0, "%s\n", UpdateCmd);
    }
    return -1;
  }

  changes++;
  return SqlAffectedRows();
}

/**
 * Utility routine for deletes
 *
 * Returns: -1 on error
 *           n number of rows affected
 */
int BareosDb::DeleteDb(JobControlRecord* jcr,
                       const char* DeleteCmd,
                       libbareos::source_location loc)
{
  CheckOwnership();
  if (!SqlQuery(DeleteCmd)) {
    msg_(loc.file_name(), loc.line(), errmsg, T_("delete %s failed:\n%s\n"),
         DeleteCmd, sql_strerror());
    j_msg(loc.file_name(), loc.line(), jcr, M_ERROR, 0, "%s", errmsg);
    if (g_verbose) {
      j_msg(loc.file_name(), loc.line(), jcr, M_INFO, 0, "%s\n", DeleteCmd);
    }
    return -1;
  }
  changes++;
  return SqlAffectedRows();
}

/**
 * Get record max. Query is already in mdb->cmd
 *  No locking done
 *
 * Returns: -1 on failure
 *          count on success
 */
int BareosDb::GetSqlRecordMax(JobControlRecord* jcr)
{
  CheckOwnership();

  SQL_ROW row;
  int retval = 0;

  if (QueryDb(jcr, cmd)) {
    if ((row = SqlFetchRow()) == NULL) {
      Mmsg1(errmsg, T_("error fetching row: %s\n"), sql_strerror());
      retval = -1;
    } else {
      retval = str_to_int64(row[0]);
    }
    SqlFreeResult();
  } else {
    Mmsg1(errmsg, T_("error fetching row: %s\n"), sql_strerror());
    retval = -1;
  }
  return retval;
}

char* BareosDb::strerror(libbareos::source_location loc)
{
  // it does not make sense to call this function without holding the lock,
  // as it may not be the real error anymore.
  CheckOwnership(loc);
  return errmsg;
}

/**
 * Given a full filename, split it into its path
 *  and filename parts. They are returned in pool memory
 *  in the mdb structure.
 */
void BareosDb::SplitPathAndFile(JobControlRecord* jcr, const char* filename)
{
  CheckOwnership();
  const char *p, *f;

  /* Find path without the filename.
   * I.e. everything after the last / is a "filename".
   * OK, maybe it is a directory name, but we treat it like
   * a filename. If we don't find a / then the whole name
   * must be a path name (e.g. c:).
   */
  for (p = f = filename; *p; p++) {
    if (IsPathSeparator(*p)) { f = p; /* set pos of last slash */ }
  }
  if (IsPathSeparator(*f)) { /* did we find a slash? */
    f++;                     /* yes, point to filename */
  } else {
    f = p; /* no, whole thing must be path name */
  }

  /* If filename doesn't exist (i.e. root directory), we
   * simply create a blank name consisting of a single
   * space. This makes handling zero length filenames
   * easier.
   */
  fnl = p - f;
  if (fnl > 0) {
    fname = CheckPoolMemorySize(fname, fnl + 1);
    memcpy(fname, f, fnl); /* copy filename */
    fname[fnl] = 0;
  } else {
    fname[0] = 0;
    fnl = 0;
  }

  pnl = f - filename;
  if (pnl > 0) {
    path = CheckPoolMemorySize(path, pnl + 1);
    memcpy(path, filename, pnl);
    path[pnl] = 0;
  } else {
    Mmsg1(errmsg, T_("Path length is zero. File=%s\n"), fname);
    Jmsg(jcr, M_ERROR, 0, "%s", errmsg);
    path[0] = 0;
    pnl = 0;
  }

  Dmsg2(500, "split path=%s file=%s\n", path, fname);
}

static int MaxLength(int MaxLength)
{
  int max_len = MaxLength;
  /* Sanity check */
  if (max_len < 0) {
    max_len = 2;
  } else if (max_len > 100) {
    max_len = 100;
  }
  return max_len;
}

// List dashes as part of header for listing SQL results in a table
void BareosDb::ListDashes(OutputFormatter* send)
{
  int len;
  int num_fields;
  SQL_FIELD* field;

  SqlFieldSeek(0);
  send->Decoration("+");
  num_fields = SqlNumFields();
  for (int i = 0; i < num_fields; i++) {
    field = SqlFetchField();
    if (!field) { break; }
    len = MaxLength(field->max_length + 2);
    for (int j = 0; j < len; j++) { send->Decoration("-"); }
    send->Decoration("+");
  }
  send->Decoration("\n");
}

// List result handler used by queries done with db_big_sql_query()
int BareosDb::ListResult(void* vctx, int, char** row)
{
  CheckOwnership();

  JobControlRecord* jcr;
  char ewc[30];
  PoolMem key;
  PoolMem value;
  int num_fields;
  SQL_FIELD* field;
  e_list_type type;
  OutputFormatter* send;
  int col_len, max_len = 0;
  ListContext* pctx = (ListContext*)vctx;

  type = pctx->type;
  send = pctx->send;
  jcr = pctx->jcr;

  // See if this row must be filtered.
  if (send->HasFilters() && !send->FilterData(row)) { return 0; }

  send->ObjectStart();

  num_fields = SqlNumFields();
  switch (type) {
    case NF_LIST:
    case RAW_LIST:
      /* No need to calculate things like maximum field lenght for
       * unformated or raw output. */
      break;
    case HORZ_LIST:
    case VERT_LIST:
      if (!pctx->once) {
        pctx->once = true;

        Dmsg1(800, "ListResult starts looking at %d fields\n", num_fields);
        // Determine column display widths
        SqlFieldSeek(0);
        for (int i = 0; i < num_fields; i++) {
          Dmsg1(800, "ListResult processing field %d\n", i);
          field = SqlFetchField();
          if (!field) { break; }

          if (send->IsHiddenColumn(i)) {
            Dmsg1(800, "ListResult field %d is hidden\n", i);
            continue;
          }

          col_len = cstrlen(field->name);
          if (type == VERT_LIST) {
            if (col_len > max_len) { max_len = col_len; }
          } else {
            if (SqlFieldIsNumeric(field->type) && (int)field->max_length > 0
                && strcmp(field->name, "jobid") != 0) { /* fixup for commas */
              field->max_length += (field->max_length - 1) / 3;
            }
            if (col_len < (int)field->max_length) {
              col_len = field->max_length;
            }
            if (col_len < 4 && !SqlFieldIsNotNull(field->flags)) {
              col_len = 4; /* 4 = length of the word "NULL" */
            }
            field->max_length = col_len; /* reset column info */
          }
        }

        pctx->num_rows++;

        Dmsg0(800, "ListResult finished first loop\n");
        if (type == VERT_LIST) { break; }

        Dmsg1(800, "ListResult starts second loop looking at %d fields\n",
              num_fields);

        ListDashes(send);

        send->Decoration("|");
        SqlFieldSeek(0);
        for (int i = 0; i < num_fields; i++) {
          Dmsg1(800, "ListResult looking at field %d\n", i);

          field = SqlFetchField();
          if (!field) { break; }

          if (send->IsHiddenColumn(i)) {
            Dmsg1(800, "ListResult field %d is hidden\n", i);
            continue;
          }

          max_len = MaxLength(field->max_length);
          send->Decoration(" %-*s |", max_len, field->name);
        }
        send->Decoration("\n");
        ListDashes(send);
      }
      break;
    default:
      break;
  }

  switch (type) {
    case NF_LIST:
    case RAW_LIST:
      Dmsg1(800, "ListResult starts third loop looking at %d fields\n",
            num_fields);
      SqlFieldSeek(0);
      for (int i = 0; i < num_fields; i++) {
        field = SqlFetchField();
        if (!field) { break; }

        if (send->IsHiddenColumn(i)) {
          Dmsg1(800, "ListResult field %d is hidden\n", i);
          continue;
        }

        if (row[i] == NULL) {
          value.bsprintf("%s", "NULL");
        } else {
          value.bsprintf("%s", row[i]);
        }
        send->ObjectKeyValue(field->name, value.c_str(), " %s");
      }
      if (type != RAW_LIST) { send->Decoration("\n"); }
      break;
    case HORZ_LIST:
      Dmsg1(800, "ListResult starts third loop looking at %d fields\n",
            num_fields);
      SqlFieldSeek(0);
      send->Decoration("|");
      for (int i = 0; i < num_fields; i++) {
        field = SqlFetchField();
        if (!field) { break; }

        if (send->IsHiddenColumn(i)) {
          Dmsg1(800, "ListResult field %d is hidden\n", i);
          continue;
        }

        max_len = MaxLength(field->max_length);
        if (row[i] == NULL) {
          value.bsprintf(" %-*s |", max_len, "NULL");
        } else if (SqlFieldIsNumeric(field->type) && !jcr->gui
                   && IsAnInteger(row[i])
                   && strcmp(field->name, "jobid") != 0) {
          value.bsprintf(" %*s |", max_len, add_commas(row[i], ewc));
        } else {
          value.bsprintf(" %-*s |", max_len, row[i]);
        }

        // Use value format string to send preformated value.
        send->ObjectKeyValue(field->name, row[i], value.c_str());
      }
      send->Decoration("\n");
      break;
    case VERT_LIST:
      Dmsg1(800, "ListResult starts vertical list at %d fields\n", num_fields);
      SqlFieldSeek(0);
      for (int i = 0; i < num_fields; i++) {
        field = SqlFetchField();
        if (!field) { break; }

        if (send->IsHiddenColumn(i)) {
          Dmsg1(800, "ListResult field %d is hidden\n", i);
          continue;
        }

        if (row[i] == NULL) {
          key.bsprintf(" %*s: ", max_len, field->name);
          value.bsprintf("%s\n", "NULL");
        } else if (SqlFieldIsNumeric(field->type) && !jcr->gui
                   && IsAnInteger(row[i])) {
          key.bsprintf(" %*s: ", max_len, field->name);
          if (strcmp(field->name, "jobid") != 0) {
            value.bsprintf("%s\n", add_commas(row[i], ewc));
          } else {
            value.bsprintf("%s\n", row[i]);
          }

          key.bsprintf(" %*s: ", max_len, field->name);
          value.bsprintf("%s\n", row[i]);
        }

        // Use value format string to send preformated value.
        send->ObjectKeyValue(field->name, key.c_str(), row[i], value.c_str());
      }
      send->Decoration("\n");
      break;
    default:
      break;
  }
  send->ObjectEnd();

  return 0;
}

int ListResult(void* vctx, int nb_col, char** row)
{
  ListContext* pctx = (ListContext*)vctx;
  BareosDb* mdb = pctx->mdb;

  return mdb->ListResult(vctx, nb_col, row);
}

/**
 * If full_list is set, we list vertically, otherwise, we list on one line
 * horizontally.
 *
 * Return number of rows
 */
int BareosDb::ListResult(JobControlRecord* jcr,
                         OutputFormatter* send,
                         e_list_type type)
{
  CheckOwnership();

  SQL_ROW row;
  char ewc[30];
  PoolMem key;
  PoolMem value;
  int num_fields;
  SQL_FIELD* field;
  bool filters_enabled;
  int col_len, max_len = 0;

  Dmsg0(800, "ListResult starts\n");
  if (SqlNumRows() == 0) {
    send->Decoration(T_("No results to list.\n"));
    return 0;
  }

  num_fields = SqlNumFields();
  switch (type) {
    case E_LIST_INIT:
    case NF_LIST:
    case RAW_LIST:
      /* No need to calculate things like column widths for unformatted or raw
       * output. */
      break;
    case HORZ_LIST:
    case VERT_LIST:
      Dmsg1(800, "ListResult starts looking at %d fields\n", num_fields);
      // Determine column display widths
      SqlFieldSeek(0);
      for (int i = 0; i < num_fields; i++) {
        Dmsg1(800, "ListResult processing field %d\n", i);

        field = SqlFetchField();
        if (!field) { break; }

        // See if this is a hidden column.
        if (send->IsHiddenColumn(i)) {
          Dmsg1(800, "ListResult field %d is hidden\n", i);
          continue;
        }

        col_len = cstrlen(field->name);
        if (type == VERT_LIST) {
          if (col_len > max_len) { max_len = col_len; }
        } else {
          if (SqlFieldIsNumeric(field->type) && (int)field->max_length > 0
              && strcmp(field->name, "jobid") != 0) { /* fixup for commas */
            field->max_length += (field->max_length - 1) / 3;
          }
          if (col_len < (int)field->max_length) { col_len = field->max_length; }
          if (col_len < 4 && !SqlFieldIsNotNull(field->flags)) {
            col_len = 4; /* 4 = length of the word "NULL" */
          }
          field->max_length = col_len; /* reset column info */
        }
      }
      break;
  }

  Dmsg0(800, "ListResult finished first loop\n");

  /* See if filters are enabled for this list function.
   * We use this to shortcut for calling the FilterData() method in the
   * OutputFormatter class. */
  filters_enabled = send->HasFilters();

  switch (type) {
    case E_LIST_INIT:
    case NF_LIST:
    case RAW_LIST:
      Dmsg1(800, "ListResult starts second loop looking at %d fields\n",
            num_fields);
      while ((row = SqlFetchRow()) != NULL) {
        // See if we should allow this under the current filtering.
        if (filters_enabled && !send->FilterData(row)) { continue; }

        send->ObjectStart();
        SqlFieldSeek(0);
        for (int i = 0; i < num_fields; i++) {
          field = SqlFetchField();
          if (!field) { break; }

          // See if this is a hidden column.
          if (send->IsHiddenColumn(i)) {
            Dmsg1(800, "ListResult field %d is hidden\n", i);
            continue;
          }

          if (row[i] == NULL) {
            value.bsprintf("%s", "NULL");
          } else {
            value.bsprintf("%s", row[i]);
          }
          send->ObjectKeyValue(field->name, value.c_str(), " %s");
        }
        if (type != RAW_LIST) { send->Decoration("\n"); }
        send->ObjectEnd();
      }
      break;
    case HORZ_LIST:
      Dmsg1(800, "ListResult starts second loop looking at %d fields\n",
            num_fields);
      ListDashes(send);
      send->Decoration("|");
      SqlFieldSeek(0);
      for (int i = 0; i < num_fields; i++) {
        Dmsg1(800, "ListResult looking at field %d\n", i);

        field = SqlFetchField();
        if (!field) { break; }

        // See if this is a hidden column.
        if (send->IsHiddenColumn(i)) {
          Dmsg1(800, "ListResult field %d is hidden\n", i);
          continue;
        }

        max_len = MaxLength(field->max_length);
        send->Decoration(" %-*s |", max_len, field->name);
      }
      send->Decoration("\n");
      ListDashes(send);

      Dmsg1(800, "ListResult starts third loop looking at %d fields\n",
            num_fields);
      while ((row = SqlFetchRow()) != NULL) {
        // See if we should allow this under the current filtering.
        if (filters_enabled && !send->FilterData(row)) { continue; }

        send->ObjectStart();
        SqlFieldSeek(0);
        send->Decoration("|");

        for (int i = 0; i < num_fields; i++) {
          field = SqlFetchField();
          if (!field) { break; }

          // See if this is a hidden column.
          if (send->IsHiddenColumn(i)) {
            Dmsg1(800, "ListResult field %d is hidden\n", i);
            continue;
          }

          max_len = MaxLength(field->max_length);
          if (row[i] == NULL) {
            value.bsprintf(" %-*s |", max_len, "NULL");
          } else if (SqlFieldIsNumeric(field->type) && !jcr->gui
                     && IsAnInteger(row[i])) {
            if (strcmp(field->name, "jobid") != 0) {
              value.bsprintf(" %*s |", max_len, add_commas(row[i], ewc));
            } else {
              value.bsprintf(" %*s |", max_len, row[i]);
            }

          } else {
            value.bsprintf(" %-*s |", max_len, row[i]);
          }
          if (i == num_fields - 1) { value.strcat("\n"); }

          // Use value format string to send preformated value
          send->ObjectKeyValue(field->name, row[i], value.c_str());
        }
        send->ObjectEnd();
      }
      ListDashes(send);
      break;
    case VERT_LIST:
      Dmsg1(800, "ListResult starts vertical list at %d fields\n", num_fields);
      while ((row = SqlFetchRow()) != NULL) {
        // See if we should allow this under the current filtering.
        if (filters_enabled && !send->FilterData(row)) { continue; }

        send->ObjectStart();
        SqlFieldSeek(0);
        for (int i = 0; i < num_fields; i++) {
          field = SqlFetchField();
          if (!field) { break; }

          // See if this is a hidden column.
          if (send->IsHiddenColumn(i)) {
            Dmsg1(800, "ListResult field %d is hidden\n", i);
            continue;
          }

          if (row[i] == NULL) {
            key.bsprintf(" %*s: ", max_len, field->name);
            value.bsprintf("%s\n", "NULL");
          } else if (SqlFieldIsNumeric(field->type) && !jcr->gui
                     && IsAnInteger(row[i])) {
            key.bsprintf(" %*s: ", max_len, field->name);
            if (strcmp(field->name, "jobid") != 0) {
              value.bsprintf("%s\n", add_commas(row[i], ewc));
            } else {
              value.bsprintf("%s\n", row[i]);
            }

          } else {
            key.bsprintf(" %*s: ", max_len, field->name);
            value.bsprintf("%s\n", row[i]);
          }

          // Use value format string to send preformated value
          send->ObjectKeyValue(field->name, key.c_str(), row[i], value.c_str());
        }
        send->Decoration("\n");
        send->ObjectEnd();
      }
      break;
  }

  return SqlNumRows();
}

/*
 * If full_list is set, we list vertically, otherwise, we list on one line
 * horizontally.
 *
 * Return number of rows
 */
int ListResult(JobControlRecord* jcr,
               BareosDb* mdb,
               OutputFormatter* send,
               e_list_type type)
{
  return mdb->ListResult(jcr, send, type);
}

/**
 * Open a new connection to mdb catalog. This function is used by batch and
 * accurate mode.
 */
bool BareosDb::OpenBatchConnection(JobControlRecord* jcr)
{
  CheckOwnership();

  bool multi_db;

  multi_db = BatchInsertAvailable();
  if (!jcr->db_batch) {
    jcr->db_batch = CloneDatabaseConnection(jcr, multi_db, multi_db);
    if (!jcr->db_batch) {
      Mmsg0(errmsg, T_("Could not init database batch connection\n"));
      Jmsg(jcr, M_FATAL, 0, "%s", errmsg);
      return false;
    }
  }
  return true;
}

void BareosDb::DbDebugPrint(FILE* fp)
{
  fprintf(fp, "BareosDb=%p db_name=%s db_user=%s connected=%s\n", this,
          NPRTB(get_db_name()), NPRTB(get_db_user()),
          IsConnected() ? "true" : "false");
  fprintf(fp, "\tcmd=\"%s\" changes=%i\n", NPRTB(cmd), changes);

  PrintLockInfo(fp);
}

/**
 * !!! WARNING !!! Use this function only when bareos is stopped.
 * ie, after a fatal signal and before exiting the program
 * Print information about a BareosDb object.
 */
void DbDebugPrint(JobControlRecord* jcr, FILE* fp)
{
  BareosDb* mdb = jcr->db;

  if (!mdb) { return; }

  mdb->DbDebugPrint(fp);
}
