/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2011-2011 Free Software Foundation Europe e.V.
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
/*
 * Written by Marco van Wieringen, January 2011
 */
/**
 * @file
 * Generic catalog class methods.
 */

#include "include/bareos.h"

#if HAVE_SQLITE3 || HAVE_MYSQL || HAVE_POSTGRESQL || HAVE_INGRES || HAVE_DBI

#include "cats.h"
#include "sql_pooling.h"

#include "bdb_query_names.inc"
#include "lib/berrno.h"

bool BareosDb::MatchDatabase(const char* db_driver,
                             const char* db_name,
                             const char* db_address,
                             int db_port)
{
  bool match;

  if (db_driver) {
    match = Bstrcasecmp(db_driver_, db_driver) && bstrcmp(db_name_, db_name) &&
            bstrcmp(db_address_, db_address) && db_port_ == db_port;
  } else {
    match = bstrcmp(db_name_, db_name) && bstrcmp(db_address_, db_address) &&
            db_port_ == db_port;
  }
  return match;
}

/**
 * Clone a BareosDb class by either increasing the reference count
 * (when mult_db_connection == false) or by getting a new
 * connection otherwise. We use a method so we can reference
 * the protected members of the class.
 */
BareosDb* BareosDb::CloneDatabaseConnection(JobControlRecord* jcr,
                                            bool mult_db_connections,
                                            bool get_pooled_connection,
                                            bool need_private)
{
  /*
   * See if its a simple clone e.g. with mult_db_connections set to false
   * then we just return the calling class pointer.
   */
  if (!mult_db_connections && !need_private) {
    ref_count_++;
    return this;
  }

  /*
   * A bit more to do here just open a new session to the database.
   * See if we need to get a pooled or non pooled connection.
   */
  if (get_pooled_connection) {
    return DbSqlGetPooledConnection(
        jcr, db_driver_, db_name_, db_user_, db_password_, db_address_,
        db_port_, db_socket_, mult_db_connections, disabled_batch_insert_,
        try_reconnect_, exit_on_fatal_, need_private);
  } else {
    return DbSqlGetNonPooledConnection(
        jcr, db_driver_, db_name_, db_user_, db_password_, db_address_,
        db_port_, db_socket_, mult_db_connections, disabled_batch_insert_,
        try_reconnect_, exit_on_fatal_, need_private);
  }
}

const char* BareosDb::GetType(void)
{
  switch (db_interface_type_) {
    case SQL_INTERFACE_TYPE_MYSQL:
      return "MySQL";
    case SQL_INTERFACE_TYPE_POSTGRESQL:
      return "PostgreSQL";
    case SQL_INTERFACE_TYPE_SQLITE3:
      return "SQLite3";
    case SQL_INTERFACE_TYPE_INGRES:
      return "Ingres";
    case SQL_INTERFACE_TYPE_DBI:
      switch (db_type_) {
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
 * times it was locked using DbUnlock().
 */
void BareosDb::LockDb(const char* file, int line)
{
  int errstat;

  if ((errstat = RwlWritelock_p(&lock_, file, line)) != 0) {
    BErrNo be;
    e_msg(file, line, M_FATAL, 0, "RwlWritelock failure. stat=%d: ERR=%s\n",
          errstat, be.bstrerror(errstat));
  }
}

/**
 * Unlock the database. This can be called multiple times by the
 * same thread up to the number of times that thread called
 * DbLock()/
 */
void BareosDb::UnlockDb(const char* file, int line)
{
  int errstat;

  if ((errstat = RwlWriteunlock(&lock_)) != 0) {
    BErrNo be;
    e_msg(file, line, M_FATAL, 0, "RwlWriteunlock failure. stat=%d: ERR=%s\n",
          errstat, be.bstrerror(errstat));
  }
}

void BareosDb::PrintLockInfo(FILE* fp)
{
  if (lock_.valid == RWLOCK_VALID) {
    fprintf(fp, "\tRWLOCK=%p w_active=%i w_wait=%i\n", &lock_, lock_.w_active,
            lock_.w_wait);
  }
}

/**
 * Escape strings so that database engine is happy.
 *
 * NOTE! len is the length of the old string. Your new
 *       string must be long enough (max 2*old+1) to hold
 *       the escaped output.
 */
void BareosDb::EscapeString(JobControlRecord* jcr,
                            char* snew,
                            const char* old,
                            int len)
{
  char* n;
  const char* o;

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
 * Memory is stored in BareosDb struct, no need to free it.
 */
char* BareosDb::EscapeObject(JobControlRecord* jcr, char* old, int len)
{
  const int MaxLength = Base64LengthUnpadded(len) + 1;
  esc_obj = CheckPoolMemorySize(esc_obj, MaxLength + 1);
  const int length = BinToBase64(esc_obj, MaxLength, old, len, true);
  esc_obj[length] = '\0';

  return esc_obj;
}

/**
 * Unescape binary object
 * We base64 encode the data so its normal ASCII
 */
void BareosDb::UnescapeObject(JobControlRecord* jcr,
                              char* from,
                              int32_t expected_len,
                              POOLMEM*& dest,
                              int32_t* dest_len)
{
  if (!from) {
    dest[0] = '\0';
    *dest_len = 0;
    return;
  }

  dest = CheckPoolMemorySize(dest, expected_len + 1);
  /*
   * Note: Base64ToBin() does not check the expected length correctly,
   * so we must add 2 to make sure it works.
   */
  Base64ToBin(dest, expected_len + 2, from, strlen(from));
  *dest_len = expected_len;
  dest[expected_len] = '\0';
}

#endif /* HAVE_SQLITE3 || HAVE_MYSQL || HAVE_POSTGRESQL || HAVE_INGRES || \
          HAVE_DBI */
