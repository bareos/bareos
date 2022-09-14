/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2010-2012 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2016 Planets Communications B.V.
   Copyright (C) 2013-2022 Bareos GmbH & Co. KG

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
// Written by Marco van Wieringen, March 2010
/**
 * @file
 * BAREOS sql pooling code that manages the database connection pools.
 */

#include "include/bareos.h"

#if HAVE_SQLITE3 || HAVE_MYSQL || HAVE_POSTGRESQL || HAVE_INGRES || HAVE_DBI

#  include "cats.h"
#  include "cats_backends.h"

/**
 * Get a non-pooled connection used when either sql pooling is
 * runtime disabled or at compile time. Or when we run out of
 * pooled connections and need more database connections.
 */
BareosDb* DbSqlGetNonPooledConnection(JobControlRecord* jcr,
                                      const char* db_drivername,
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
                                      bool need_private)
{
  BareosDb* mdb;

#  if defined(HAVE_DYNAMIC_CATS_BACKENDS)
  Dmsg2(100,
        "DbSqlGetNonPooledConnection allocating 1 new non pooled database "
        "connection to database %s, backend type %s\n",
        db_name, db_drivername);
#  else
  Dmsg1(100,
        "DbSqlGetNonPooledConnection allocating 1 new non pooled database "
        "connection to database %s\n",
        db_name);
#  endif
  mdb = db_init_database(jcr, db_drivername, db_name, db_user, db_password,
                         db_address, db_port, db_socket, mult_db_connections,
                         disable_batch_insert, try_reconnect, exit_on_fatal,
                         need_private);
  if (mdb == NULL) { return NULL; }

  if (!mdb->OpenDatabase(jcr)) {
    Jmsg(jcr, M_FATAL, 0, "%s", mdb->strerror());
    mdb->CloseDatabase(jcr);
    return NULL;
  }

  return mdb;
}

/**
 * Initialize the sql connection pool.
 * For non pooling this is a no-op.
 */
bool db_sql_pool_initialize([[maybe_unused]] const char* db_drivername,
                            [[maybe_unused]] const char* db_name,
                            [[maybe_unused]] const char* db_user,
                            [[maybe_unused]] const char* db_password,
                            [[maybe_unused]] const char* db_address,
                            [[maybe_unused]] int db_port,
                            [[maybe_unused]] const char* db_socket,
                            [[maybe_unused]] bool disable_batch_insert,
                            [[maybe_unused]] bool try_reconnect,
                            [[maybe_unused]] bool exit_on_fatal,
                            [[maybe_unused]] int min_connections,
                            [[maybe_unused]] int max_connections,
                            [[maybe_unused]] int increment_connections,
                            [[maybe_unused]] int idle_timeout,
                            [[maybe_unused]] int validate_timeout)
{
  return true;
}

/**
 * Cleanup the sql connection pools.
 * For non pooling this is a no-op.
 */
void DbSqlPoolDestroy(void) {}

/**
 * Flush the sql connection pools.
 * For non pooling this is a no-op.
 */
void DbSqlPoolFlush(void) {}

/**
 * Get a new connection from the pool.
 * For non pooling we just call DbSqlGetNonPooledConnection.
 */
BareosDb* DbSqlGetPooledConnection(JobControlRecord* jcr,
                                   const char* db_drivername,
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
                                   bool need_private)
{
  return DbSqlGetNonPooledConnection(
      jcr, db_drivername, db_name, db_user, db_password, db_address, db_port,
      db_socket, mult_db_connections, disable_batch_insert, try_reconnect,
      exit_on_fatal, need_private);
}

/**
 * Put a connection back onto the pool for reuse.
 * For non pooling we just do a CloseDatabase.
 */
void DbSqlClosePooledConnection(JobControlRecord* jcr,
                                BareosDb* mdb,
                                [[maybe_unused]] bool abort)
{
  mdb->CloseDatabase(jcr);
}

#endif /* HAVE_SQLITE3 || HAVE_MYSQL || HAVE_POSTGRESQL || HAVE_INGRES || \
          HAVE_DBI */
