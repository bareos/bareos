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

#ifndef DIRD_FD_CMDS_H_
#define DIRD_FD_CMDS_H_

bool connect_to_file_daemon(JobControlRecord *jcr, int retry_interval, int max_retry_time, bool verbose);
int  send_job_info(JobControlRecord *jcr);
bool send_include_list(JobControlRecord *jcr);
bool send_exclude_list(JobControlRecord *jcr);
bool send_level_command(JobControlRecord *jcr);
bool send_bwlimit_to_fd(JobControlRecord *jcr, const char *Job);
bool send_secure_erase_req_to_fd(JobControlRecord *jcr);
bool send_previous_restore_objects(JobControlRecord *jcr);
int get_attributes_and_put_in_catalog(JobControlRecord *jcr);
void get_attributes_and_compare_to_catalog(JobControlRecord *jcr, JobId_t JobId);
int put_file_into_catalog(JobControlRecord *jcr, long file_index, char *fname,
                          char *link, char *attr, int stream);
int send_runscripts_commands(JobControlRecord *jcr);
bool send_plugin_options(JobControlRecord *jcr);
bool send_restore_objects(JobControlRecord *jcr, JobId_t JobId, bool send_global);
bool cancel_file_daemon_job(UaContext *ua, JobControlRecord *jcr);
void do_native_client_status(UaContext *ua, ClientResource *client, char *cmd);
void do_client_resolve(UaContext *ua, ClientResource *client);
void *handle_filed_connection(ConnectionPool *connections, BareosSocket *fd,
                              char *client_name, int fd_protocol_version);

ConnectionPool *get_client_connections();
bool is_connecting_to_client_allowed(ClientResource *res);
bool is_connecting_to_client_allowed(JobControlRecord *jcr);
bool is_connect_from_client_allowed(ClientResource *res);
bool is_connect_from_client_allowed(JobControlRecord *jcr);
bool use_waiting_client(JobControlRecord *jcr_job, int timeout);

#endif // DIRD_FD_CMDS_H_
