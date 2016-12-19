/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2009-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2016-2016 Planets Communications B.V.
   Copyright (C) 2016-2016 Bareos GmbH & Co. KG

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
#ifndef __BDB_DBI_H_
#define __BDB_DBI_H_ 1

struct DBI_FIELD_GET {
   dlink link;
   char *value;
};

class B_DB_DBI: public B_DB_PRIV {
private:
   /*
    * Members.
    */
   dbi_inst m_instance;
   dbi_conn *m_db_handle;
   dbi_result *m_result;
   DBI_FIELD_GET *m_field_get;

private:
   /*
    * Methods.
    */
   bool open_database(JCR *jcr);
   void close_database(JCR *jcr);
   bool validate_connection(void);
   void escape_string(JCR *jcr, char *snew, char *old, int len);
   char *escape_object(JCR *jcr, char *old, int len);
   void unescape_object(JCR *jcr, char *from, int32_t expected_len,
                        POOLMEM *&dest, int32_t *len);
   void start_transaction(JCR *jcr);
   void end_transaction(JCR *jcr);
   bool sql_query_with_handler(const char *query, DB_RESULT_HANDLER *result_handler, void *ctx);
   bool sql_query_without_handler(const char *query, int flags = 0);
   void sql_free_result(void);
   SQL_ROW sql_fetch_row(void);
   const char *sql_strerror(void);
   void sql_data_seek(int row);
   int sql_affected_rows(void);
   uint64_t sql_insert_autokey_record(const char *query, const char *table_name);
   SQL_FIELD *sql_fetch_field(void);
   bool sql_field_is_not_null(int field_type);
   bool sql_field_is_numeric(int field_type);
   bool sql_batch_start(JCR *jcr);
   bool sql_batch_end(JCR *jcr, const char *error);
   bool sql_batch_insert(JCR *jcr, ATTR_DBR *ar);

public:
   /*
    * Methods.
    */
   B_DB_DBI(JCR *jcr,
            const char *db_driver,
            const char *db_name,
            const char *db_user,
            const char *db_password,
            const char *db_address,
            int db_port,
            const char *db_socket,
            bool mult_db_connections,
            bool disable_batch_insert,
            bool try_reconnect,
            bool exit_on_fatal,
            bool need_private);
   ~B_DB_DBI();
};
#endif /* __BDB_DBI_H_ */
