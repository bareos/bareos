/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2011-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2016-2016 Planets Communications B.V.
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
#ifndef BAREOS_CATS_BDB_PRIV_H_
#define BAREOS_CATS_BDB_PRIV_H_ 1

#ifndef _BDB_PRIV_INTERFACE_
#error "Illegal inclusion of catalog private interface"
#endif

class BareosDbPrivateInterface : public BareosDb {
 protected:
  /*
   * Members
   */
  int status_ = 0;              /**< Status */
  int num_rows_ = 0;            /**< Number of rows returned by last query */
  int num_fields_ = 0;          /**< Number of fields returned by last query */
  int rows_size_ = 0;           /**< Size of malloced rows */
  int fields_size_ = 0;         /**< Size of malloced fields */
  int row_number_ = 0;          /**< Row number from xx_data_seek */
  int field_number_ = 0;        /**< Field number from SqlFieldSeek */
  SQL_ROW rows_ = nullptr;      /**< Defined rows */
  SQL_FIELD* fields_ = nullptr; /**< Defined fields */
  bool allow_transactions_ = false; /**< Transactions allowed ? */
  bool transaction_ = false;        /**< Transaction started ? */

 private:
  /*
   * Methods
   */
  int SqlNumRows(void) override { return num_rows_; }
  void SqlFieldSeek(int field) override { field_number_ = field; }
  int SqlNumFields(void) override { return num_fields_; }
  virtual void SqlFreeResult(void) override = 0;
  virtual SQL_ROW SqlFetchRow(void) override = 0;
  virtual bool SqlQueryWithHandler(const char* query,
                                   DB_RESULT_HANDLER* ResultHandler,
                                   void* ctx) override = 0;
  virtual bool SqlQueryWithoutHandler(const char* query,
                                      int flags = 0) override = 0;
  virtual const char* sql_strerror(void) override = 0;
  virtual void SqlDataSeek(int row) override = 0;
  virtual int SqlAffectedRows(void) override = 0;
  virtual uint64_t SqlInsertAutokeyRecord(const char* query,
                                          const char* table_name) override = 0;
  virtual SQL_FIELD* SqlFetchField(void) override = 0;
  virtual bool SqlFieldIsNotNull(int field_type) override = 0;
  virtual bool SqlFieldIsNumeric(int field_type) override = 0;
  virtual bool SqlBatchStart(JobControlRecord* jcr) override = 0;
  virtual bool SqlBatchEnd(JobControlRecord* jcr,
                           const char* error) override = 0;
  virtual bool SqlBatchInsert(JobControlRecord* jcr,
                              AttributesDbRecord* ar) override = 0;

 public:
  BareosDbPrivateInterface() = default;
  virtual ~BareosDbPrivateInterface() = default;
};
#endif /* BAREOS_CATS_BDB_PRIV_H_ */
