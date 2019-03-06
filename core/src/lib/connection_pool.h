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

#ifndef BAREOS_LIB_CONNECTION_POOL_H_
#define BAREOS_LIB_CONNECTION_POOL_H_

#include "lib/smartall.h"

class alist;
class BareosSocket;

class Connection : public SmartAlloc {
 public:
  Connection(const char* name,
             int protocol_version,
             BareosSocket* socket,
             bool authenticated = true);
  ~Connection();

  const char* name() { return name_; }
  int protocol_version() { return protocol_version_; }
  BareosSocket* bsock() { return socket_; }
  bool authenticated() { return authenticated_; }
  bool in_use() { return in_use_; }
  time_t ConnectTime() { return connect_time_; }

  bool check(int timeout = 0);
  bool wait(int timeout = 60);
  bool take();
  void lock() { P(mutex_); }
  void unlock() { V(mutex_); }

 private:
  pthread_t tid_;
  BareosSocket* socket_;
  char name_[MAX_NAME_LENGTH];
  int protocol_version_;
  bool authenticated_;
  volatile bool in_use_;
  time_t connect_time_;
  pthread_mutex_t mutex_;
};

class ConnectionPool : public SmartAlloc {
 public:
  ConnectionPool();
  ~ConnectionPool();

  Connection* add_connection(const char* name,
                             int protocol_version,
                             BareosSocket* socket,
                             bool authenticated = true);
  Connection* remove(const char* name, int timeout_in_seconds = 0);
  alist* get_as_alist();

 private:
  alist* connections_;
  int WaitForNewConnection(timespec& timeout);
  bool add(Connection* connection);
  bool remove(Connection* connection);
  bool exists(const char* name);
  void cleanup();
  Connection* get_connection(const char* name);
  Connection* get_connection(const char* name, int timeout_seconds);
  Connection* get_connection(const char* name, timespec& timeout);
  pthread_mutex_t add_mutex_;
  pthread_cond_t add_cond_var_;
};
#endif
