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
/**
 * @file
 * Connection Pool
 *
 * handle/store multiple connections
 */

#ifndef __CONNECTION_POOL_H_
#define __CONNECTION_POOL_H_

class DLL_IMP_EXP CONNECTION : public SMARTALLOC {
public:
   CONNECTION(const char* name, int protocol_version, BSOCK* socket, bool authenticated = true);
   ~CONNECTION();

   const char *name() { return m_name; };
   int protocol_version() { return m_protocol_version; };
   BSOCK *bsock() { return m_socket; };
   bool authenticated() { return m_authenticated; }
   bool in_use() { return m_in_use; }
   time_t connect_time() { return m_connect_time; }

   bool check(int timeout = 0);
   bool wait(int timeout = 60);
   bool take();
   void lock() { P(m_mutex); };
   void unlock() { V(m_mutex); };

private:
   pthread_t m_tid;
   BSOCK *m_socket;
   char m_name[MAX_NAME_LENGTH];
   int  m_protocol_version;
   bool m_authenticated;
   volatile bool m_in_use;
   time_t m_connect_time;
   pthread_mutex_t m_mutex;
};

class DLL_IMP_EXP CONNECTION_POOL : public SMARTALLOC {
public:
   CONNECTION_POOL();
   ~CONNECTION_POOL();

   void cleanup();
   alist *get_as_alist();
   bool add(CONNECTION *connection);
   CONNECTION *add_connection(const char* name, int protocol_version, BSOCK* socket, bool authenticated = true);
   bool exists(const char *name);
   CONNECTION *get_connection(const char *name);
   CONNECTION *get_connection(const char *name, int timeout_seconds);
   CONNECTION *get_connection(const char *name, timespec &timeout);
   int wait_for_new_connection(timespec &timeout);
   /*
    * remove specific connection.
    */
   bool remove(CONNECTION *connection);
   /*
    * remove connection from pool to be used by some other component.
    */
   CONNECTION *remove(const char* name, int timeout_in_seconds = 0);

private:
   alist *m_connections;
   pthread_mutex_t m_add_mutex;
   pthread_cond_t m_add_cond_var;
};
#endif
