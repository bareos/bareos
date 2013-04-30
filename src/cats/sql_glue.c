/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2009-2011 Free Software Foundation Europe e.V.

   The main author of Bacula is Kern Sibbald, with contributions from
   many others, a complete list can be found in the file AUTHORS.
   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation and included
   in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   General Public License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.

   Bacula® is a registered trademark of Kern Sibbald.

   The licensor of Bacula is the Free Software Foundation Europe
   (FSFE), Fiduciary Program, Sumatrastrasse 25, 8006 Zürich,
   Switzerland, email:ftf@fsfeurope.org.
*/
/*
 * Bacula Glue code for the catalog refactoring.
 *
 * Written by Marco van Wieringen, November 2009
 */

#include "bacula.h"

#if HAVE_SQLITE3 || HAVE_MYSQL || HAVE_POSTGRESQL || HAVE_INGRES || HAVE_DBI

#include "cats.h"
#include "bdb_priv.h"
#include "sql_glue.h"

/* -----------------------------------------------------------------------
 *
 *   Generic Glue Routines
 *
 * -----------------------------------------------------------------------
 */
bool db_match_database(B_DB *mdb, const char *db_driver, const char *db_name,
                       const char *db_address, int db_port)
{
   return mdb->db_match_database(db_driver, db_name, db_address, db_port);
}

B_DB *db_clone_database_connection(B_DB *mdb, JCR *jcr, bool mult_db_connections)
{
   return mdb->db_clone_database_connection(jcr, mult_db_connections);
}

const char *db_get_type(B_DB *mdb)
{
   return mdb->db_get_type();
}

int db_get_type_index(B_DB *mdb)
{
   return mdb->db_get_type_index();
}

bool db_open_database(JCR *jcr, B_DB *mdb)
{
   return mdb->db_open_database(jcr);
}

void db_close_database(JCR *jcr, B_DB *mdb)
{
   if (mdb) {
      mdb->db_close_database(jcr);
   }
}

void db_thread_cleanup(B_DB *mdb)
{
   mdb->db_thread_cleanup();
}

void db_escape_string(JCR *jcr, B_DB *mdb, char *snew, char *old, int len)
{
   mdb->db_escape_string(jcr, snew, old, len);
}

char *db_escape_object(JCR *jcr, B_DB *mdb, char *old, int len)
{
   return mdb->db_escape_object(jcr, old, len);
}

void db_unescape_object(JCR *jcr, B_DB *mdb, 
                        char *from, int32_t expected_len, 
                        POOLMEM **dest, int32_t *len)
{
   mdb->db_unescape_object(jcr, from, expected_len, dest, len);
}

void db_start_transaction(JCR *jcr, B_DB *mdb)
{
   mdb->db_start_transaction(jcr);
}

void db_end_transaction(JCR *jcr, B_DB *mdb)
{
   mdb->db_end_transaction(jcr);
}

bool db_sql_query(B_DB *mdb, const char *query, int flags)
{
   mdb->errmsg[0] = 0;
   return mdb->db_sql_query(query, flags);
}

bool db_sql_query(B_DB *mdb, const char *query, DB_RESULT_HANDLER *result_handler, void *ctx)
{
   mdb->errmsg[0] = 0;
   return mdb->db_sql_query(query, result_handler, ctx);
}

bool db_big_sql_query(B_DB *mdb, const char *query, DB_RESULT_HANDLER *result_handler, void *ctx)
{
   mdb->errmsg[0] = 0;
   return mdb->db_big_sql_query(query, result_handler, ctx);
}

void sql_free_result(B_DB *mdb)
{
   ((B_DB_PRIV *)mdb)->sql_free_result();
}

SQL_ROW sql_fetch_row(B_DB *mdb)
{
   return ((B_DB_PRIV *)mdb)->sql_fetch_row();
}

bool sql_query(B_DB *mdb, const char *query, int flags)
{
   mdb->errmsg[0] = 0;
   return ((B_DB_PRIV *)mdb)->sql_query(query, flags);
}

const char *sql_strerror(B_DB *mdb)
{
   return ((B_DB_PRIV *)mdb)->sql_strerror();
}

int sql_num_rows(B_DB *mdb)
{
   return ((B_DB_PRIV *)mdb)->sql_num_rows();
}

void sql_data_seek(B_DB *mdb, int row)
{
   ((B_DB_PRIV *)mdb)->sql_data_seek(row);
}

int sql_affected_rows(B_DB *mdb)
{
   return ((B_DB_PRIV *)mdb)->sql_affected_rows();
}

uint64_t sql_insert_autokey_record(B_DB *mdb, const char *query, const char *table_name)
{
   return ((B_DB_PRIV *)mdb)->sql_insert_autokey_record(query, table_name);
}

void sql_field_seek(B_DB *mdb, int field)
{
   ((B_DB_PRIV *)mdb)->sql_field_seek(field);
}

SQL_FIELD *sql_fetch_field(B_DB *mdb)
{
   return ((B_DB_PRIV *)mdb)->sql_fetch_field();
}

int sql_num_fields(B_DB *mdb)
{
   return ((B_DB_PRIV *)mdb)->sql_num_fields();
}

bool sql_field_is_not_null(B_DB *mdb, int field_type)
{
   return ((B_DB_PRIV *)mdb)->sql_field_is_not_null(field_type);
}

bool sql_field_is_numeric(B_DB *mdb, int field_type)
{
   return ((B_DB_PRIV *)mdb)->sql_field_is_numeric(field_type);
}

bool sql_batch_start(JCR *jcr, B_DB *mdb)
{
   return ((B_DB_PRIV *)mdb)->sql_batch_start(jcr);
}

bool sql_batch_end(JCR *jcr, B_DB *mdb, const char *error)
{
   return ((B_DB_PRIV *)mdb)->sql_batch_end(jcr, error);
}

bool sql_batch_insert(JCR *jcr, B_DB *mdb, ATTR_DBR *ar)
{
   return ((B_DB_PRIV *)mdb)->sql_batch_insert(jcr, ar);
}

#endif /* HAVE_SQLITE3 || HAVE_MYSQL || HAVE_POSTGRESQL || HAVE_INGRES || HAVE_DBI */
