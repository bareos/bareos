/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2009 Free Software Foundation Europe e.V.
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
 * Database routines that are exported by the cats library for
 * use elsewhere in BAREOS (mainly the Director).
 */

#ifndef __SQL_PROTOS_H
#define __SQL_PROTOS_H

#include "cats.h"

/* Database prototypes */

/* cats_backends.c */
#if defined(HAVE_DYNAMIC_CATS_BACKENDS)
void db_set_backend_dirs(alist *new_backend_dirs);
#endif
void db_flush_backends(void);
B_DB *db_init_database(JCR *jcr,
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
                       bool need_private = false);

/* sql.c */
int db_int64_handler(void *ctx, int num_fields, char **row);
int db_strtime_handler(void *ctx, int num_fields, char **row);
int db_list_handler(void *ctx, int num_fields, char **row);
void db_debug_print(JCR *jcr, FILE *fp);
int db_int_handler(void *ctx, int num_fields, char **row);

/* sql_pooling.c */
bool db_sql_pool_initialize(const char *db_drivername,
                            const char *db_name,
                            const char *db_user,
                            const char *db_password,
                            const char *db_address,
                            int db_port,
                            const char *db_socket,
                            bool disable_batch_insert,
                            bool try_reconnect,
                            bool exit_on_fatal,
                            int min_connections,
                            int max_connections,
                            int increment_connections,
                            int idle_timeout,
                            int validate_timeout);
void db_sql_pool_destroy(void);
void db_sql_pool_flush(void);
B_DB *db_sql_get_non_pooled_connection(JCR *jcr,
                                       const char *db_drivername,
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
                                       bool need_private = false);
B_DB *db_sql_get_pooled_connection(JCR *jcr,
                                   const char *db_drivername,
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
                                   bool need_private = false);
void db_sql_close_pooled_connection(JCR *jcr, B_DB *mdb, bool abort = false);
#endif /* __SQL_PROTOS_H */
