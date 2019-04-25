/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2010-2012 Free Software Foundation Europe e.V.
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
 * Written by Marco van Wieringen, March 2010
 */
/**
 * @file
 * BAREOS sql pooling code that manages the database connection pools.
 */

#include "include/bareos.h"

#if HAVE_SQLITE3 || HAVE_MYSQL || HAVE_POSTGRESQL || HAVE_INGRES || HAVE_DBI

#include "cats.h"
#include "cats_backends.h"

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

#if defined(HAVE_DYNAMIC_CATS_BACKENDS)
  Dmsg2(100,
        "DbSqlGetNonPooledConnection allocating 1 new non pooled database "
        "connection to database %s, backend type %s\n",
        db_name, db_drivername);
#else
  Dmsg1(100,
        "DbSqlGetNonPooledConnection allocating 1 new non pooled database "
        "connection to database %s\n",
        db_name);
#endif
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

#ifdef HAVE_SQL_POOLING

static dlist* db_pooling_descriptors = NULL;

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

static void DestroyPoolDescriptor(SqlPoolDescriptor* spd, bool flush_only)
{
  SqlPoolEntry *spe, *spe_next;

  spe = (SqlPoolEntry*)spd->pool_entries->first();
  while (spe) {
    spe_next = (SqlPoolEntry*)spd->pool_entries->get_next(spe);
    if (!flush_only || spe->reference_count == 0) {
      Dmsg3(100,
            "DbSqlPoolDestroy destroy db pool connection %d to %s, backend "
            "type %s\n",
            spe->id, spe->db_handle->get_db_name(), spe->db_handle->GetType());
      spe->db_handle->CloseDatabase(NULL);
      if (flush_only) {
        spd->pool_entries->remove(spe);
        free(spe);
      }
      spd->nr_connections--;
    }
    spe = spe_next;
  }

  /*
   * See if there is anything left on this pool and we are flushing the pool.
   */
  if (flush_only && spd->nr_connections == 0) {
    db_pooling_descriptors->remove(spd);
    delete spd->pool_entries;
    free(spd);
  }
}

/**
 * Initialize the sql connection pool.
 */
bool db_sql_pool_initialize(const char* db_drivername,
                            const char* db_name,
                            const char* db_user,
                            const char* db_password,
                            const char* db_address,
                            int db_port,
                            const char* db_socket,
                            bool disable_batch_insert,
                            bool try_reconnect,
                            bool exit_on_fatal,
                            int min_connections,
                            int max_connections,
                            int increment_connections,
                            int idle_timeout,
                            int validate_timeout)
{
  int cnt;
  BareosDb* mdb;
  time_t now;
  SqlPoolDescriptor* spd = NULL;
  SqlPoolEntry* spe = NULL;
  bool retval = false;

  /*
   * See if pooling is runtime disabled.
   */
  if (max_connections == 0) {
    Dmsg0(100,
          "db_sql_pool_initialize pooling disabled as max_connections == 0\n");
    return true;
  }

  /*
   * First make sure the values make any sense.
   */
  if (min_connections <= 0 || max_connections <= 0 ||
      increment_connections <= 0 || min_connections > max_connections) {
    Jmsg(NULL, M_FATAL, 0,
         _("Illegal values for sql pool initialization, min_connections = %d, "
           "max_connections = %d, increment_connections = %d"),
         min_connections, max_connections, increment_connections);
    return false;
  }

  P(mutex);
  time(&now);

  if (db_pooling_descriptors == NULL) {
    db_pooling_descriptors = new dlist(spd, &spd->link);
  }

  /*
   * Create a new pool descriptor.
   */
  spd = (SqlPoolDescriptor*)malloc(sizeof(SqlPoolDescriptor));
  memset(spd, 0, sizeof(SqlPoolDescriptor));
  spd->pool_entries = new dlist(spe, &spe->link);
  spd->min_connections = min_connections;
  spd->max_connections = max_connections;
  spd->increment_connections = increment_connections;
  spd->idle_timeout = idle_timeout;
  spd->validate_timeout = validate_timeout;
  spd->last_update = now;
  spd->active = true;

  /*
   * Create a number of database connections.
   */
  for (cnt = 0; cnt < min_connections; cnt++) {
    mdb = db_init_database(NULL, db_drivername, db_name, db_user, db_password,
                           db_address, db_port, db_socket, true,
                           disable_batch_insert, try_reconnect, exit_on_fatal);
    if (mdb == NULL) {
      Jmsg(NULL, M_FATAL, 0, "%s", _("Could not init database connection"));
      goto bail_out;
    }

    if (!mdb->OpenDatabase(NULL)) {
      Jmsg(NULL, M_FATAL, 0, "%s", mdb->strerror());
      mdb->CloseDatabase(NULL);
      goto bail_out;
    }

    /*
     * Push this new connection onto the connection pool.
     */
    spe = (SqlPoolEntry*)malloc(sizeof(SqlPoolEntry));
    memset(spe, 0, sizeof(SqlPoolEntry));
    spe->id = spd->nr_connections++;
    spe->last_update = now;
    spe->db_handle = mdb;
    spd->pool_entries->append(spe);
    spe = NULL;
  }

#if defined(HAVE_DYNAMIC_CATS_BACKENDS)
  Dmsg3(100,
        "db_sql_pool_initialize created %d connections to database %s, backend "
        "type %s\n",
        cnt, db_name, db_drivername);
#else
  Dmsg2(100, "db_sql_pool_initialize created %d connections to database %s\n",
        cnt, db_name);
#endif
  db_pooling_descriptors->append(spd);
  retval = true;
  goto ok_out;

bail_out:
  if (spe) { free(spe); }

  if (spd) { DestroyPoolDescriptor(spd, false); }

ok_out:
  V(mutex);
  return retval;
}

/**
 * Cleanup the sql connection pools.
 * This gets called on shutdown.
 */
void DbSqlPoolDestroy(void)
{
  SqlPoolDescriptor *spd, *spd_next;

  /*
   * See if pooling is enabled.
   */
  if (!db_pooling_descriptors) { return; }

  P(mutex);
  spd = (SqlPoolDescriptor*)db_pooling_descriptors->first();
  while (spd) {
    spd_next = (SqlPoolDescriptor*)db_pooling_descriptors->get_next(spd);
    DestroyPoolDescriptor(spd, false);
    spd = spd_next;
  }
  delete db_pooling_descriptors;
  db_pooling_descriptors = NULL;
  V(mutex);
}

/**
 * Flush the sql connection pools.
 * This gets called on config reload. We close all unreferenced connections.
 */
void DbSqlPoolFlush(void)
{
  SqlPoolEntry* spe;
  SqlPoolDescriptor *spd, *spd_next;

  /*
   * See if pooling is enabled.
   */
  if (!db_pooling_descriptors) { return; }

  P(mutex);
  spd = (SqlPoolDescriptor*)db_pooling_descriptors->first();
  while (spd) {
    spd_next = (SqlPoolDescriptor*)db_pooling_descriptors->get_next(spd);
    if (spd->active) {
      /*
       * On a flush all current available pools are invalidated.
       */
      spd->active = false;
      DestroyPoolDescriptor(spd, true);
    }
    spd = spd_next;
  }
  V(mutex);
}

/**
 * Grow the sql connection pool.
 * This function should be called with the mutex held.
 */
static inline void SqlPoolGrow(SqlPoolDescriptor* spd)
{
  int cnt, next_id;
  BareosDb* mdb;
  time_t now;
  SqlPoolEntry* spe;
  BareosDb* db_handle;

  /*
   * Get the first entry from the list to be able to clone it.
   * If the pool is empty its not initialized ok so we cannot really
   * grow its size.
   */
  spe = (SqlPoolEntry*)spd->pool_entries->first();
  if (spe != NULL) {
    /*
     * Save the handle of the first entry so we can clone it later on.
     */
    db_handle = spe->db_handle;

    /*
     * Now that the pool is about to be grown give each entry a new id.
     */
    cnt = 0;
    foreach_dlist (spe, spd->pool_entries) {
      spe->id = cnt++;
    }

    /*
     * Remember the next available id to use.
     */
    next_id = cnt;

    /*
     * Create a number of database connections.
     */
    time(&now);
    for (cnt = 0; cnt < spd->increment_connections; cnt++) {
      /*
       * Get a new non-pooled connection to the database.
       * We want to add a non pooled connection to the pool as otherwise
       * we are creating a deadlock as CloneDatabaseConnection will
       * call sql_pool_get_connection which means a recursive enter into
       * the pooling code and as such the mutex will deadlock.
       */
      mdb = db_handle->CloneDatabaseConnection(NULL, true, false);
      if (mdb == NULL) {
        Jmsg(NULL, M_FATAL, 0, "%s", _("Could not init database connection"));
        break;
      }

      /*
       * Push this new connection onto the connection pool.
       */
      spe = (SqlPoolEntry*)malloc(sizeof(SqlPoolEntry));
      memset(spe, 0, sizeof(SqlPoolEntry));
      spe->id = next_id++;
      spe->last_update = now;
      spe->db_handle = mdb;
      spd->pool_entries->append(spe);
    }
    Dmsg3(
        100,
        "SqlPoolGrow created %d connections to database %s, backend type %s\n",
        cnt, spe->db_handle->get_db_name(), spe->db_handle->GetType());
    spd->last_update = now;
  } else {
    Dmsg0(100, "SqlPoolGrow unable to determine first entry on pool list\n");
  }
}

/**
 * Shrink the sql connection pool.
 * This function should be called with the mutex held.
 */
static inline void SqlPoolShrink(SqlPoolDescriptor* spd)
{
  int cnt;
  time_t now;
  SqlPoolEntry *spe, *spe_next;

  time(&now);
  spd->last_update = now;

  /*
   * See if we want to shrink.
   */
  if (spd->min_connections && spd->nr_connections <= spd->min_connections) {
    Dmsg0(100,
          "SqlPoolShrink cannot shrink connection pool already minimum size\n");
    return;
  }

  /*
   * See how much we should shrink.
   * No need to shrink under min_connections, and when things are greater
   * shrink with increment_connections per shrink run.
   */
  cnt = spd->nr_connections - spd->min_connections;
  if (cnt > spd->increment_connections) { cnt = spd->increment_connections; }

  /*
   * Sanity check.
   */
  if (cnt <= 0) { return; }

  /*
   * For debugging purposes get the first entry on the connection pool.
   */
  spe = (SqlPoolEntry*)spd->pool_entries->first();
  if (spe) {
    Dmsg3(100,
          "SqlPoolShrink shrinking connection pool with %d connections to "
          "database %s, backend type %s\n",
          cnt, spe->db_handle->get_db_name(), spe->db_handle->GetType());
  }

  /*
   * Loop over all entries on the pool and see if the can be removed.
   */
  spe = (SqlPoolEntry*)spd->pool_entries->first();
  while (spe) {
    spe_next = (SqlPoolEntry*)spd->pool_entries->get_next(spe);

    /*
     * See if this is a unreferenced connection.
     * And its been idle for more then idle_timeout seconds.
     */
    if (spe->reference_count == 0 &&
        ((now - spe->last_update) >= spd->idle_timeout)) {
      spd->pool_entries->remove(spe);
      spe->db_handle->CloseDatabase(NULL);
      free(spe);
      spd->nr_connections--;
      cnt--;
      /*
       * See if we have freed enough.
       */
      if (cnt <= 0) { break; }
    }

    spe = spe_next;
  }

  /*
   * Now that the pool has shrunk give each entry a new id.
   */
  cnt = 0;
  foreach_dlist (spe, spd->pool_entries) {
    spe->id = cnt++;
  }
}

/**
 * Find the connection pool with the correct connections.
 * This function should be called with the mutex held.
 */
static inline SqlPoolDescriptor* sql_find_pool_descriptor(
    const char* db_drivername,
    const char* db_name,
    const char* db_address,
    int db_port)
{
  SqlPoolDescriptor* spd;
  SqlPoolEntry* spe;

  foreach_dlist (spd, db_pooling_descriptors) {
    if (spd->active) {
      foreach_dlist (spe, spd->pool_entries) {
        if (spe->db_handle->MatchDatabase(db_drivername, db_name, db_address,
                                          db_port)) {
          return spd;
        }
      }
    }
  }
  return NULL;
}

/**
 * Find a free connection in a certain connection pool.
 * This function should be called with the mutex held.
 */
static inline SqlPoolEntry* sql_find_free_connection(SqlPoolDescriptor* spd)
{
  SqlPoolEntry* spe;

  foreach_dlist (spe, spd->pool_entries) {
    if (spe->reference_count == 0) { return spe; }
  }
  return NULL;
}

/**
 * Find a connection in a certain connection pool.
 * This function should be called with the mutex held.
 */
static inline SqlPoolEntry* sql_find_first_connection(SqlPoolDescriptor* spd)
{
  SqlPoolEntry* spe;

  foreach_dlist (spe, spd->pool_entries) {
    return spe;
  }
  return NULL;
}

/**
 * Get a new connection from the pool.
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
  int cnt = 0;
  SqlPoolDescriptor* wanted_pool;
  SqlPoolEntry* use_connection = NULL;
  BareosDb* db_handle = NULL;
  time_t now;

  now = time(NULL);
#if defined(HAVE_DYNAMIC_CATS_BACKENDS)
  Dmsg2(100,
        "DbSqlGetPooledConnection get new connection for connection to "
        "database %s, backend type %s\n",
        db_name, db_drivername);
#else
  Dmsg1(100,
        "DbSqlGetPooledConnection get new connection for connection to "
        "database %s\n",
        db_name);
#endif

  /*
   * See if pooling is enabled.
   */
  if (!db_pooling_descriptors) {
    return DbSqlGetNonPooledConnection(
        jcr, db_drivername, db_name, db_user, db_password, db_address, db_port,
        db_socket, mult_db_connections, disable_batch_insert, try_reconnect,
        exit_on_fatal, need_private);
  }

  P(mutex);
  /*
   * Try to lookup the pool.
   */
  wanted_pool =
      sql_find_pool_descriptor(db_drivername, db_name, db_address, db_port);
  if (wanted_pool) {
    /*
     * Loop while trying to find a connection.
     */
    while (1) {
      /*
       * If we can return a shared connection and when need_private is not
       * explicitly set try to match an existing connection. Otherwise we
       * want a free connection.
       */
      if (!mult_db_connections && !need_private) {
        use_connection = sql_find_first_connection(wanted_pool);
      } else {
        use_connection = sql_find_free_connection(wanted_pool);
      }
      if (use_connection) {
        /*
         * See if the connection match needs validation.
         */
        if ((now - use_connection->last_update) >=
            wanted_pool->validate_timeout) {
          if (!use_connection->db_handle->ValidateConnection()) {
            /*
             * Connection seems to be dead kill it from the pool.
             */
            wanted_pool->pool_entries->remove(use_connection);
            use_connection->db_handle->CloseDatabase(jcr);
            free(use_connection);
            wanted_pool->nr_connections--;
            continue;
          }
        }
        goto ok_out;
      } else {
        if (mult_db_connections || need_private) {
          /*
           * Cannot find an already open connection that is unused.
           * See if there is still room to grow the pool if not this is it.
           * We just give back a non pooled connection which gets a proper
           * cleanup anyhow when it discarded using DbSqlClosePooledConnection.
           */
          if (wanted_pool->nr_connections >= wanted_pool->max_connections) {
            db_handle = DbSqlGetNonPooledConnection(
                jcr, db_drivername, db_name, db_user, db_password, db_address,
                db_port, db_socket, mult_db_connections, disable_batch_insert,
                try_reconnect, exit_on_fatal, need_private);
            goto bail_out;
          }

          Dmsg0(100,
                "DbSqlGetPooledConnection trying to grow connection pool for "
                "getting free connection\n");
          SqlPoolGrow(wanted_pool);
        } else {
          /*
           * Request for a shared connection and no connection gets through the
           * validation. e.g. all connections in the pool have failed. This
           * should never happen so lets abort things and let the upper layer
           * handle this.
           */
          goto bail_out;
        }
      }
    }
  } else {
    /*
     * Pooling not enabled for this connection use non pooling.
     */
    db_handle = DbSqlGetNonPooledConnection(
        jcr, db_drivername, db_name, db_user, db_password, db_address, db_port,
        db_socket, mult_db_connections, disable_batch_insert, try_reconnect,
        exit_on_fatal, need_private);
    goto bail_out;
  }

ok_out:
  use_connection->reference_count++;
  use_connection->last_update = now;
  db_handle = use_connection->db_handle;

  /*
   * Set the IsPrivate flag of this database connection to the wanted state.
   */
  db_handle->SetPrivate(need_private);

bail_out:
  V(mutex);
  return db_handle;
}

/**
 * Put a connection back onto the pool for reuse.
 *
 * The abort flag is set when we encounter a dead or misbehaving connection
 * which needs to be closed right away and should not be reused.
 */
void DbSqlClosePooledConnection(JobControlRecord* jcr,
                                BareosDb* mdb,
                                bool abort)
{
  SqlPoolEntry *spe, *spe_next;
  SqlPoolDescriptor *spd, *spd_next;
  bool found = false;
  time_t now;

  /*
   * See if pooling is enabled.
   */
  if (!db_pooling_descriptors) {
    mdb->CloseDatabase(jcr);
    return;
  }

  P(mutex);

  /*
   * See what connection is freed.
   */
  now = time(NULL);

  spd = (SqlPoolDescriptor*)db_pooling_descriptors->first();
  while (spd) {
    spd_next = (SqlPoolDescriptor*)db_pooling_descriptors->get_next(spd);

    if (!spd->pool_entries) {
      spd = spd_next;
      continue;
    }

    spe = (SqlPoolEntry*)spd->pool_entries->first();
    while (spe) {
      spe_next = (SqlPoolEntry*)spd->pool_entries->get_next(spe);

      if (spe->db_handle == mdb) {
        found = true;
        if (!abort) {
          /*
           * End any active transactions.
           */
          mdb->EndTransaction(jcr);

          /*
           * Decrement reference count and update last update field.
           */
          spe->reference_count--;
          time(&spe->last_update);

          Dmsg3(100,
                "DbSqlClosePooledConnection decrementing reference count of "
                "connection %d now %d, backend type %s\n",
                spe->id, spe->reference_count, spe->db_handle->GetType());

          /*
           * Clear the IsPrivate flag if this is a free connection again.
           */
          if (spe->reference_count == 0) { mdb->SetPrivate(false); }

          /*
           * See if this is a free on an inactive pool and this was the last
           * reference.
           */
          if (!spd->active && spe->reference_count == 0) {
            spd->pool_entries->remove(spe);
            spe->db_handle->CloseDatabase(jcr);
            free(spe);
            spd->nr_connections--;
          }
        } else {
          Dmsg3(100,
                "DbSqlClosePooledConnection aborting connection to database %s "
                "reference count %d, backend type %s\n",
                spe->db_handle->get_db_name(), spe->reference_count,
                spe->db_handle->GetType());
          spd->pool_entries->remove(spe);
          spe->db_handle->CloseDatabase(jcr);
          free(spe);
          spd->nr_connections--;
        }

        /*
         * No need to search further if we found the item we were looking for.
         */
        break;
      }

      spe = spe_next;
    }

    /*
     * See if this is an inactive pool and it has no connections on it anymore.
     */
    if (!spd->active && spd->nr_connections == 0) {
      db_pooling_descriptors->remove(spd);
      delete spd->pool_entries;
      free(spd);
    } else {
      /*
       * See if we can shrink the connection pool.
       * Only try to shrink when the last update on the pool was more than the
       * validate time ago.
       */
      if ((now - spd->last_update) >= spd->validate_timeout) {
        Dmsg0(100,
              "DbSqlClosePooledConnection trying to shrink connection pool\n");
        SqlPoolShrink(spd);
      }
    }

    /*
     * No need to search further if we found the item we were looking for.
     */
    if (found) { break; }

    spd = spd_next;
  }

  /*
   * If we didn't find this mdb on any pooling chain we are not pooling
   * this connection and we just close the connection.
   */
  if (!found) { mdb->CloseDatabase(jcr); }

  V(mutex);
}

#else /* HAVE_SQL_POOLING */

/**
 * Initialize the sql connection pool.
 * For non pooling this is a no-op.
 */
bool db_sql_pool_initialize(const char* db_drivername,
                            const char* db_name,
                            const char* db_user,
                            const char* db_password,
                            const char* db_address,
                            int db_port,
                            const char* db_socket,
                            bool disable_batch_insert,
                            bool try_reconnect,
                            bool exit_on_fatal,
                            int min_connections,
                            int max_connections,
                            int increment_connections,
                            int idle_timeout,
                            int validate_timeout)
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
                                bool abort)
{
  mdb->CloseDatabase(jcr);
}

#endif /* HAVE_SQL_POOLING */
#endif /* HAVE_SQLITE3 || HAVE_MYSQL || HAVE_POSTGRESQL || HAVE_INGRES || \
          HAVE_DBI */
