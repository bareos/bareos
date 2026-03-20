/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2009-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2016-2016 Planets Communications B.V.
   Copyright (C) 2016-2025 Bareos GmbH & Co. KG

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
#ifndef BAREOS_CATS_POSTGRESQL_H_
#define BAREOS_CATS_POSTGRESQL_H_

#include "include/bareos.h"

#include "cats/column_data.h"
#include "cats.h"
#include "libpq-fe.h"

#include <string>
#include <vector>

struct AttributesDbRecord;
class JobControlRecord;

class BareosDbPostgresql : public BareosDb {
 public:
  BareosDbPostgresql(JobControlRecord* jcr,
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
  ~BareosDbPostgresql();

  dlink<BareosDbPostgresql> link; /**< Queue control */

 private:
  void SqlFieldSeek(int field) override { field_number_ = field; }
  int SqlNumFields(void) override { return num_fields_; }
  const char* OpenDatabase(JobControlRecord* jcr) override;
  void CloseDatabase(JobControlRecord* jcr) override;
  void EscapeString(JobControlRecord* jcr,
                    char* snew,
                    const char* old,
                    int len) override;
  char* EscapeObject(JobControlRecord* jcr, char* old, int len) override;
  void UnescapeObject(JobControlRecord* jcr,
                      char* from,
                      int32_t expected_len,
                      POOLMEM*& dest,
                      int32_t* len) override;
  void StartTransaction(JobControlRecord* jcr) override;
  void EndTransaction(JobControlRecord* jcr) override;
  bool BigSqlQuery(const char* query,
                   DB_RESULT_HANDLER* ResultHandler,
                   void* ctx) override;

  bool SqlQueryWithHandler(const char* query,
                           DB_RESULT_HANDLER* ResultHandler,
                           void* ctx) override;
  bool SqlQueryWithoutHandler(const char* query,
                              query_flags flags = {}) override;
  void SqlFreeResult(void) override;
  SQL_ROW SqlFetchRow(void) override;
  const char* sql_strerror(void) override;
  void SqlDataSeek(int row) override;
  int SqlAffectedRows(void) override;
  uint64_t SqlInsertAutokeyRecord(const char* query,
                                  const char* table_name) override;
  SQL_FIELD* SqlFetchField(void) override;
  bool SqlFieldIsNotNull(int field_type) override;
  bool SqlFieldIsNumeric(int field_type) override;
  bool SqlBatchStartFileTable(JobControlRecord* jcr) override;
  bool SqlBatchEndFileTable(JobControlRecord* jcr, const char* error) override;
  bool SqlBatchInsertFileTable(JobControlRecord* jcr,
                               AttributesDbRecord* ar) override;

  bool CheckDatabaseEncoding(JobControlRecord* jcr);

  bool fields_fetched_
      = false;         /**< Marker, if field descriptions are already fetched */
  int num_fields_ = 0; /**< Number of fields returned by last query */
  int rows_size_ = 0;  /**< Size of malloced rows */
  int fields_size_ = 0;             /**< Size of malloced fields */
  int row_number_ = 0;              /**< Row number from xx_data_seek */
  int field_number_ = 0;            /**< Field number from SqlFieldSeek */
  SQL_ROW rows_ = nullptr;          /**< Defined rows */
  SQL_FIELD* fields_ = nullptr;     /**< Defined fields */
  bool allow_transactions_ = false; /**< Transactions allowed ? */
  bool transaction_ = false;        /**< Transaction started ? */

  PGconn* db_handle_;
  PGresult* result_;
  POOLMEM* buf_; /**< Buffer to manipulate queries */
  static const char*
      query_definitions[]; /**< table of predefined sql queries */
};
#endif  // BAREOS_CATS_POSTGRESQL_H_
