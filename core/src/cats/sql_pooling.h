/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2018-2018 Bareos GmbH & Co. KG

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

DLL_IMP_EXP bool db_sql_pool_initialize(const char *db_drivername,
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
DLL_IMP_EXP void db_sql_pool_destroy(void);
DLL_IMP_EXP void db_sql_pool_flush(void);
DLL_IMP_EXP BareosDb *db_sql_get_non_pooled_connection(JobControlRecord *jcr,
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
DLL_IMP_EXP BareosDb *db_sql_get_pooled_connection(JobControlRecord *jcr,
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
DLL_IMP_EXP void db_sql_close_pooled_connection(JobControlRecord *jcr, BareosDb *mdb, bool abort = false);

