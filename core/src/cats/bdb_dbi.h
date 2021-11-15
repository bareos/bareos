/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2009-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2016-2016 Planets Communications B.V.
   Copyright (C) 2016-2021 Bareos GmbH & Co. KG

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
#ifndef BAREOS_CATS_BDB_DBI_H_
#define BAREOS_CATS_BDB_DBI_H_

#include "include/bareos.h"
#include "cats/column_data.h"

#include <string>
#include <vector>

struct AttributesDbRecord;
class JobControlRecord;

struct DbiFieldGet {
  dlink link;
  char* value;
};

class BareosDbDBI : public BareosDbPrivateInterface {
 private:
  dbi_inst instance_;
  dbi_conn* db_handle_;
  dbi_result* result_;
  DbiFieldGet* field_get_;

 private:
  bool OpenDatabase(JobControlRecord* jcr);
  void CloseDatabase(JobControlRecord* jcr);
  bool ValidateConnection(void);
  void EscapeString(JobControlRecord* jcr,
                    char* snew,
                    const char* old,
                    int len);
  char* EscapeObject(JobControlRecord* jcr, char* old, int len);
  void UnescapeObject(JobControlRecord* jcr,
                      char* from,
                      int32_t expected_len,
                      POOLMEM*& dest,
                      int32_t* len);
  void StartTransaction(JobControlRecord* jcr);
  void EndTransaction(JobControlRecord* jcr);
  bool SqlQueryWithHandler(const char* query,
                           DB_RESULT_HANDLER* ResultHandler,
                           void* ctx);
  bool SqlQueryWithoutHandler(const char* query, int flags = 0);
  void SqlFreeResult(void);
  SQL_ROW SqlFetchRow(void);
  const char* sql_strerror(void);
  void SqlDataSeek(int row);
  int SqlAffectedRows(void);
  uint64_t SqlInsertAutokeyRecord(const char* query, const char* table_name);
  SQL_FIELD* SqlFetchField(void);
  bool SqlFieldIsNotNull(int field_type);
  bool SqlFieldIsNumeric(int field_type);
  bool SqlBatchStartFileTable(JobControlRecord* jcr);
  bool SqlBatchEndFileTable(JobControlRecord* jcr, const char* error);
  bool SqlBatchInsertFileTable(JobControlRecord* jcr, AttributesDbRecord* ar);
  bool SqlCopyStart(const std::string& table_name,
                    const std::vector<std::string>& column_names);
  bool SqlCopyInsert(const std::vector<DatabaseField>& data_fields);
  bool SqlCopyEnd();

 public:
  BareosDbDBI(JobControlRecord* jcr,
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
              bool need_private);
  ~BareosDbDBI();
};
#endif  // BAREOS_CATS_BDB_DBI_H_
