/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2011-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2016 Planets Communications B.V.
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
/*
 * Written by Marco van Wieringen, January 2011
 */
/**
 * @file
 * Generic catalog class methods.
 */

#include "bareos.h"

#if HAVE_SQLITE3 || HAVE_MYSQL || HAVE_POSTGRESQL || HAVE_INGRES || HAVE_DBI

#include "cats.h"

bool B_DB::match_database(const char *db_driver, const char *db_name,
                          const char *db_address, int db_port)
{
   bool match;

   if (db_driver) {
      match = bstrcasecmp(m_db_driver, db_driver) &&
              bstrcmp(m_db_name, db_name) &&
              bstrcmp(m_db_address, db_address) &&
              m_db_port == db_port;
   } else {
      match = bstrcmp(m_db_name, db_name) &&
              bstrcmp(m_db_address, db_address) &&
              m_db_port == db_port;
   }
   return match;
}

/**
 * Clone a B_DB class by either increasing the reference count
 * (when mult_db_connection == false) or by getting a new
 * connection otherwise. We use a method so we can reference
 * the protected members of the class.
 */
B_DB *B_DB::clone_database_connection(JCR *jcr,
                                      bool mult_db_connections,
                                      bool get_pooled_connection,
                                      bool need_private)
{
   /*
    * See if its a simple clone e.g. with mult_db_connections set to false
    * then we just return the calling class pointer.
    */
   if (!mult_db_connections && !need_private) {
      m_ref_count++;
      return this;
   }

   /*
    * A bit more to do here just open a new session to the database.
    * See if we need to get a pooled or non pooled connection.
    */
   if (get_pooled_connection) {
      return db_sql_get_pooled_connection(jcr, m_db_driver, m_db_name, m_db_user, m_db_password,
                                          m_db_address, m_db_port, m_db_socket, mult_db_connections, m_disabled_batch_insert,
                                          m_try_reconnect, m_exit_on_fatal, need_private);
   } else {
      return db_sql_get_non_pooled_connection(jcr, m_db_driver, m_db_name, m_db_user, m_db_password,
                                              m_db_address, m_db_port, m_db_socket, mult_db_connections, m_disabled_batch_insert,
                                              m_try_reconnect, m_exit_on_fatal, need_private);
   }
}

const char *B_DB::get_type(void)
{
   switch (m_db_interface_type) {
   case SQL_INTERFACE_TYPE_MYSQL:
      return "MySQL";
   case SQL_INTERFACE_TYPE_POSTGRESQL:
      return "PostgreSQL";
   case SQL_INTERFACE_TYPE_SQLITE3:
      return "SQLite3";
   case SQL_INTERFACE_TYPE_INGRES:
      return "Ingres";
   case SQL_INTERFACE_TYPE_DBI:
      switch (m_db_type) {
      case SQL_TYPE_MYSQL:
         return "DBI:MySQL";
      case SQL_TYPE_POSTGRESQL:
         return "DBI:PostgreSQL";
      case SQL_TYPE_SQLITE3:
         return "DBI:SQLite3";
      case SQL_TYPE_INGRES:
         return "DBI:Ingres";
      default:
         return "DBI:Unknown";
      }
   default:
      return "Unknown";
   }
}

/**
 * Lock database, this can be called multiple times by the same
 * thread without blocking, but must be unlocked the number of
 * times it was locked using db_unlock().
 */
void B_DB::_lock_db(const char *file, int line)
{
   int errstat;

   if ((errstat = rwl_writelock_p(&m_lock, file, line)) != 0) {
      berrno be;
      e_msg(file, line, M_FATAL, 0, "rwl_writelock failure. stat=%d: ERR=%s\n",
            errstat, be.bstrerror(errstat));
   }
}

/**
 * Unlock the database. This can be called multiple times by the
 * same thread up to the number of times that thread called
 * db_lock()/
 */
void B_DB::_unlock_db(const char *file, int line)
{
   int errstat;

   if ((errstat = rwl_writeunlock(&m_lock)) != 0) {
      berrno be;
      e_msg(file, line, M_FATAL, 0, "rwl_writeunlock failure. stat=%d: ERR=%s\n",
            errstat, be.bstrerror(errstat));
   }
}

void B_DB::print_lock_info(FILE *fp)
{
   if (m_lock.valid == RWLOCK_VALID) {
      fprintf(fp, "\tRWLOCK=%p w_active=%i w_wait=%i\n", &m_lock, m_lock.w_active, m_lock.w_wait);
   }
}

/**
 * Escape strings so that database engine is happy.
 *
 * NOTE! len is the length of the old string. Your new
 *       string must be long enough (max 2*old+1) to hold
 *       the escaped output.
 */
void B_DB::escape_string(JCR *jcr, char *snew, char *old, int len)
{
   char *n, *o;

   n = snew;
   o = old;
   while (len--) {
      switch (*o) {
      case '\'':
         *n++ = '\'';
         *n++ = '\'';
         o++;
         break;
      case 0:
         *n++ = '\\';
         *n++ = 0;
         o++;
         break;
      default:
         *n++ = *o++;
         break;
      }
   }
   *n = 0;
}

/**
 * Escape binary object.
 * We base64 encode the data so its normal ASCII
 * Memory is stored in B_DB struct, no need to free it.
 */
char *B_DB::escape_object(JCR *jcr, char *old, int len)
{
   int length;
   int max_length;

   max_length = (len * 4) / 3;
   esc_obj = check_pool_memory_size(esc_obj, max_length + 1);
   length = bin_to_base64(esc_obj, max_length, old, len, true);
   esc_obj[length] = '\0';

   return esc_obj;
}

/**
 * Unescape binary object
 * We base64 encode the data so its normal ASCII
 */
void B_DB::unescape_object(JCR *jcr, char *from, int32_t expected_len,
                           POOLMEM *&dest, int32_t *dest_len)
{
   if (!from) {
      dest[0] = '\0';
      *dest_len = 0;
      return;
   }

   dest = check_pool_memory_size(dest, expected_len + 1);
   base64_to_bin(dest, expected_len + 1, from, strlen(from));
   *dest_len = expected_len;
   dest[expected_len] = '\0';
}

#endif /* HAVE_SQLITE3 || HAVE_MYSQL || HAVE_POSTGRESQL || HAVE_INGRES || HAVE_DBI */
