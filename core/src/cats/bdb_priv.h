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

class BareosDbPrivateInterface: public BareosDb {
protected:
   /*
    * Members
    */
   int status_;                      /**< Status */
   int num_rows_;                    /**< Number of rows returned by last query */
   int num_fields_;                  /**< Number of fields returned by last query */
   int rows_size_;                   /**< Size of malloced rows */
   int fields_size_;                 /**< Size of malloced fields */
   int row_number_;                  /**< Row number from xx_data_seek */
   int field_number_;                /**< Field number from SqlFieldSeek */
   SQL_ROW rows_;                    /**< Defined rows */
   SQL_FIELD *fields_;               /**< Defined fields */
   bool allow_transactions_;         /**< Transactions allowed ? */
   bool transaction_;                /**< Transaction started ? */

private:
   /*
    * Methods
    */
   int SqlNumRows(void) { return num_rows_; }
   void SqlFieldSeek(int field) { field_number_ = field; }
   int SqlNumFields(void) { return num_fields_; }
   virtual void SqlFreeResult(void) = 0;
   virtual SQL_ROW SqlFetchRow(void) = 0;
   virtual bool SqlQueryWithHandler(const char *query, DB_RESULT_HANDLER *ResultHandler, void *ctx) = 0;
   virtual bool SqlQueryWithoutHandler(const char *query, int flags = 0) = 0;
   virtual const char *sql_strerror(void) = 0;
   virtual void SqlDataSeek(int row) = 0;
   virtual int SqlAffectedRows(void) = 0;
   virtual uint64_t SqlInsertAutokeyRecord(const char *query, const char *table_name) = 0;
   virtual SQL_FIELD *SqlFetchField(void) = 0;
   virtual bool SqlFieldIsNotNull(int field_type) = 0;
   virtual bool SqlFieldIsNumeric(int field_type) = 0;
   virtual bool SqlBatchStart(JobControlRecord *jcr) = 0;
   virtual bool SqlBatchEnd(JobControlRecord *jcr, const char *error) = 0;
   virtual bool SqlBatchInsert(JobControlRecord *jcr, AttributesDbRecord *ar) = 0;

public:
   /*
    * Methods
    */
   BareosDbPrivateInterface() {
      queries = NULL;
   }
   virtual ~BareosDbPrivateInterface() {}
};
#endif /* BAREOS_CATS_BDB_PRIV_H_ */
