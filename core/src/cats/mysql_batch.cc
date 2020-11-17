/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2003-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2016 Planets Communications B.V.
   Copyright (C) 2013-2020 Bareos GmbH & Co. KG

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

#ifdef HAVE_MYSQL

#  include "cats.h"
#  include <mysql.h>
#  include <errmsg.h>
#  include "bdb_mysql.h"
#  include "lib/edit.h"

/**
 * Returns true if OK
 *         false if failed
 */
bool BareosDbMysql::SqlBatchStartFileTable(JobControlRecord* jcr)
{
  bool retval;

  DbLock(this);
  retval = SqlQuery(
      "CREATE TEMPORARY TABLE batch ("
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
bool BareosDbMysql::SqlBatchEndFileTable(JobControlRecord* jcr,
                                         const char* error)
{
  status_ = 0;

  /*
   * Flush any pending inserts.
   */
  if (changes) { return SqlQuery(cmd); }

  return true;
}

/**
 * Returns true if OK
 *         false if failed
 */
bool BareosDbMysql::SqlBatchInsertFileTable(JobControlRecord* jcr,
                                            AttributesDbRecord* ar)
{
  const char* digest;
  char ed1[50], ed2[50], ed3[50];

  esc_name = CheckPoolMemorySize(esc_name, fnl * 2 + 1);
  EscapeString(jcr, esc_name, fname, fnl);

  esc_path = CheckPoolMemorySize(esc_path, pnl * 2 + 1);
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
    Mmsg(cmd,
         "INSERT INTO batch VALUES "
         "(%u,%s,'%s','%s','%s','%s',%u,'%s','%s')",
         ar->FileIndex, edit_int64(ar->JobId, ed1), esc_path, esc_name,
         ar->attr, digest, ar->DeltaSeq, edit_uint64(ar->Fhinfo, ed2),
         edit_uint64(ar->Fhnode, ed3));
    changes++;
  } else {
    /*
     * We use the esc_obj for temporary storage otherwise
     * we keep on copying data.
     */
    Mmsg(esc_obj, ",(%u,%s,'%s','%s','%s','%s',%u,%u,%u)", ar->FileIndex,
         edit_int64(ar->JobId, ed1), esc_path, esc_name, ar->attr, digest,
         ar->DeltaSeq, ar->Fhinfo, ar->Fhnode);
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

bool BareosDbMysql::SqlCopyStart(const std::string& table_name,
                                 const std::vector<std::string>& column_names)
{
  return false;
}

bool BareosDbMysql::SqlCopyInsert(const std::vector<DatabaseField>& data_fields)
{
  return false;
}

bool BareosDbMysql::SqlCopyEnd() { return false; }


#endif  // HAVE_MYSQL
