/*
   BAREOS® - Backup Archiving REcovery Open Sourced

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

/*
 * Connection and Connection Pool
 */

#include "bareos.h"
#include "connection_pool.h"

/*
 * Connection
 */
CONNECTION::CONNECTION(const char* name, int protocol_version, BSOCK* socket, bool authenticated)
{
   m_tid = pthread_self();
   m_connect_time = time(NULL);
   m_in_use = false;
   m_authenticated = authenticated;
   bstrncpy(m_name, name, sizeof(m_name));
   m_protocol_version = protocol_version;
   m_socket = socket;
   pthread_mutex_init(&m_mutex, NULL);
}

CONNECTION::~CONNECTION()
{
   pthread_mutex_destroy(&m_mutex);
}

/*
 * Check if connection is still active.
 */
bool CONNECTION::check(int timeout_data)
{
   int data_available = 0;
   bool ok = true;

   /*
    * Returns: 1 if data available, 0 if timeout, -1 if error
    */
   data_available = m_socket->wait_data_intr(timeout_data);

   /*
    * Use lock to prevent that data is read for job thread.
    */
   lock();
   if (data_available < 0) {
      ok = false;
   } else if ((data_available > 0) && (!m_in_use)) {
      if (m_socket->recv() <= 0) {
         ok = false;
      }

      if (m_socket->is_error()) {
         ok = false;
      }
   }
   unlock();

   return ok;
}

/*
 * Wait until an error occurs
 * or the connection is requested by some other thread.
 */
bool CONNECTION::wait(int timeout)
{
   bool ok = true;

   while (ok && (!m_in_use)) {
      ok = check(timeout);
   }
   return ok;
}

/*
 * Request to take over the connection (socket) from another thread.
 */
bool CONNECTION::take()
{
   bool result = false;
   lock();
   if (!m_in_use) {
      m_in_use = true;
      result = true;
   }
   unlock();

   return result;
}

/*
 * Connection Pool
 */
CONNECTION_POOL::CONNECTION_POOL()
{
   m_connections = New(alist(10, false));
   /*
    * Initialize mutex and condition variable objects.
    */
   pthread_mutex_init(&m_add_mutex, NULL);
   pthread_cond_init(&m_add_cond_var, NULL);
}

CONNECTION_POOL::~CONNECTION_POOL()
{
   delete(m_connections);
   pthread_mutex_destroy(&m_add_mutex);
   pthread_cond_destroy(&m_add_cond_var);
}

void CONNECTION_POOL::cleanup()
{
   CONNECTION *connection = NULL;
   int i = 0;
   for(i=m_connections->size()-1; i>=0; i--) {
      connection = (CONNECTION *)m_connections->get(i);
      Dmsg2(120, "checking connection %s (%d)\n", connection->name(), i);
      if (!connection->check()) {
         Dmsg2(120, "connection %s (%d) is terminated => removed\n", connection->name(), i);
         m_connections->remove(i);
         delete(connection);
      }
   }
}

alist *CONNECTION_POOL::get_as_alist()
{
   this->cleanup();
   return m_connections;
}

bool CONNECTION_POOL::add(CONNECTION* connection)
{
   this->cleanup();
   Dmsg1(120, "add connection: %s\n", connection->name());
   P(m_add_mutex);
   m_connections->append(connection);
   pthread_cond_broadcast(&m_add_cond_var);
   V(m_add_mutex);
   return true;
}

CONNECTION *CONNECTION_POOL::add_connection(const char* name, int fd_protocol_version, BSOCK* socket, bool authenticated)
{
   CONNECTION *connection = New(CONNECTION(name, fd_protocol_version, socket, authenticated));
   if (!add(connection)) {
      delete(connection);
      return NULL;
   }
   return connection;
}

bool CONNECTION_POOL::exists(const char *name)
{
   return (get_connection(name) != NULL);
}

CONNECTION *CONNECTION_POOL::get_connection(const char *name)
{
   CONNECTION *connection = NULL;
   if (!name) {
      return NULL;
   }
   foreach_alist(connection, m_connections) {
      if (connection->check()
          && connection->authenticated()
          && connection->bsock()
          && (!connection->in_use())
          && bstrcmp(name, connection->name())) {
         Dmsg1(120, "found connection from client %s\n", connection->name());
         return connection;
      }
   }
   return NULL;
}

CONNECTION *CONNECTION_POOL::get_connection(const char *name, int timeout_in_seconds)
{
   struct timespec timeout;

   convert_timeout_to_timespec(timeout, timeout_in_seconds);
   return get_connection(name, timeout);
}

CONNECTION *CONNECTION_POOL::get_connection(const char *name, timespec &timeout)
{
   CONNECTION *connection = NULL;
   int errstat = 0;

   if (!name) {
      return NULL;
   }

   while ((!connection) && (errstat == 0)) {
      connection = get_connection(name);
      if (!connection) {
         Dmsg0(120, "waiting for new connections.\n");
         errstat = wait_for_new_connection(timeout);
         if (errstat == ETIMEDOUT) {
            Dmsg0(120, "timeout while waiting for new connections.\n");
         }
      }
   }

   return connection;
}

int CONNECTION_POOL::wait_for_new_connection(timespec &timeout)
{
   int errstat;

   P(m_add_mutex);
   errstat = pthread_cond_timedwait(&m_add_cond_var, &m_add_mutex, &timeout);
   V(m_add_mutex);
   if (errstat == 0) {
      Dmsg0(120, "new connection available.\n");
   } else if (errstat == ETIMEDOUT) {
      Dmsg0(120, "timeout.\n");
   } else {
      Emsg1(M_ERROR, 0, "error: %d\n", errstat);
   }
   return errstat;
}

bool CONNECTION_POOL::remove(CONNECTION *connection)
{
   bool removed = false;
   int i = 0;
   for(i=m_connections->size()-1; i>=0; i--) {
      if (m_connections->get(i) == connection) {
         m_connections->remove(i);
         removed = true;
         Dmsg0(120, "removed connection.\n");
         break;
      }
   }
   return removed;
}

CONNECTION *CONNECTION_POOL::remove(const char *name, int timeout_in_seconds)
{
   bool done = false;
   CONNECTION *result = NULL;
   CONNECTION *connection = NULL;
   struct timespec timeout;

   convert_timeout_to_timespec(timeout, timeout_in_seconds);

   Dmsg2(120, "waiting for connection from client %s. Timeout: %ds.\n", name, timeout_in_seconds);

   while (!done) {
      connection = get_connection(name, timeout);
      if (!connection) {
         /*
          * NULL is returned only on timeout (or other internal errors).
          */
         return NULL;
      }
      if (connection->take()) {
         result = connection;
         remove(connection);
         done = true;
      } else {
         /*
          * As we can not take it, we assume it is already taken by another thread.
          * In any case, we remove it, to prevent to get stuck in this while loop.
          */
         remove(connection);
      }
   }
   return result;
}
