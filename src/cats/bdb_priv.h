/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2011-2011 Free Software Foundation Europe e.V.
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
#ifndef __BDB_PRIV_H_
#define __BDB_PRIV_H_ 1

#ifndef _BDB_PRIV_INTERFACE_
#error "Illegal inclusion of catalog private interface"
#endif

/*
 * Generic definition of a sql_row.
 */
typedef char ** SQL_ROW;

/*
 * Generic definition of a a sql_field.
 */
typedef struct sql_field {
   char *name;                        /* name of column */
   int max_length;                    /* max length */
   uint32_t type;                     /* type */
   uint32_t flags;                    /* flags */
} SQL_FIELD;

class CATS_IMP_EXP B_DB_PRIV: public B_DB {
protected:
   int m_status;                      /* Status */
   int m_num_rows;                    /* Number of rows returned by last query */
   int m_num_fields;                  /* Number of fields returned by last query */
   int m_rows_size;                   /* Size of malloced rows */
   int m_fields_size;                 /* Size of malloced fields */
   int m_row_number;                  /* Row number from xx_data_seek */
   int m_field_number;                /* Field number from sql_field_seek */
   SQL_ROW m_rows;                    /* Defined rows */
   SQL_FIELD *m_fields;               /* Defined fields */
   bool m_allow_transactions;         /* Transactions allowed ? */
   bool m_transaction;                /* Transaction started ? */

public:
   /* methods */
   B_DB_PRIV() {};
   virtual ~B_DB_PRIV() {};

   int sql_num_rows(void) { return m_num_rows; };
   void sql_field_seek(int field) { m_field_number = field; };
   int sql_num_fields(void) { return m_num_fields; };
   virtual void sql_free_result(void) = 0;
   virtual SQL_ROW sql_fetch_row(void) = 0;
   virtual bool sql_query(const char *query, int flags = 0) = 0;
   virtual const char *sql_strerror(void) = 0;
   virtual void sql_data_seek(int row) = 0;
   virtual int sql_affected_rows(void) = 0;
   virtual uint64_t sql_insert_autokey_record(const char *query, const char *table_name) = 0;
   virtual SQL_FIELD *sql_fetch_field(void) = 0;
   virtual bool sql_field_is_not_null(int field_type) = 0;
   virtual bool sql_field_is_numeric(int field_type) = 0;
   virtual bool sql_batch_start(JCR *jcr) = 0;
   virtual bool sql_batch_end(JCR *jcr, const char *error) = 0;
   virtual bool sql_batch_insert(JCR *jcr, ATTR_DBR *ar) = 0;
};

#endif /* __BDB_PRIV_H_ */
