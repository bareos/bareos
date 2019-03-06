/*
   BAREOS® - Backup Archiving REcovery Open Sourced

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
 * Marco van Wieringen, November 2010
 */
/**
 * @file
 * Dynamic loading of catalog plugins.
 */

#ifndef BAREOS_CATS_CATS_BACKENDS_H_
#define BAREOS_CATS_CATS_BACKENDS_H_ 1

extern "C" {
typedef BareosDb* (*t_backend_instantiate)(JobControlRecord* jcr,
                                           const char* db_driver,
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
                                           bool need_private);

typedef void (*t_flush_backend)(void);
}

/**
 * Loaded shared library with a certain backend interface type.
 */
struct backend_shared_library_t {
  int interface_type_id;
  void* handle;
  /*
   * Entry points into loaded shared library.
   */
  t_backend_instantiate backend_instantiate;
  t_flush_backend flush_backend;
};

#if defined(HAVE_WIN32)
#define DYN_LIB_EXTENSION ".dll"
#elif defined(HAVE_DARWIN_OS)
/* cmake MODULE creates a .so files; cmake SHARED creates .dylib */
// #define DYN_LIB_EXTENSION ".dylib"
#define DYN_LIB_EXTENSION ".so"
#else
#define DYN_LIB_EXTENSION ".so"
#endif

#if defined(HAVE_DYNAMIC_CATS_BACKENDS)
class alist;
void DbSetBackendDirs(alist* new_backend_dirs);
#endif
void DbFlushBackends(void);
BareosDb* db_init_database(JobControlRecord* jcr,
                           const char* db_driver,
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
                           bool need_private = false);


#endif /* BAREOS_CATS_CATS_BACKENDS_H_ */
