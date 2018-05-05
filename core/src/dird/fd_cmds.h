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

#ifndef BAREOS_DIRD_FD_CMDS_H_
#define BAREOS_DIRD_FD_CMDS_H_

bool ConnectToFileDaemon(JobControlRecord *jcr, int retry_interval, int max_retry_time, bool verbose);
int  SendJobInfo(JobControlRecord *jcr);
bool SendIncludeList(JobControlRecord *jcr);
bool SendExcludeList(JobControlRecord *jcr);
bool SendLevelCommand(JobControlRecord *jcr);
bool SendBwlimitToFd(JobControlRecord *jcr, const char *Job);
bool SendSecureEraseReqToFd(JobControlRecord *jcr);
bool SendPreviousRestoreObjects(JobControlRecord *jcr);
int GetAttributesAndPutInCatalog(JobControlRecord *jcr);
void GetAttributesAndCompareToCatalog(JobControlRecord *jcr, JobId_t JobId);
int put_file_into_catalog(JobControlRecord *jcr, long file_index, char *fname,
                          char *link, char *attr, int stream);
int SendRunscriptsCommands(JobControlRecord *jcr);
bool SendPluginOptions(JobControlRecord *jcr);
bool SendRestoreObjects(JobControlRecord *jcr, JobId_t JobId, bool send_global);
bool CancelFileDaemonJob(UaContext *ua, JobControlRecord *jcr);
void DoNativeClientStatus(UaContext *ua, ClientResource *client, char *cmd);
void DoClientResolve(UaContext *ua, ClientResource *client);
void *handle_filed_connection(ConnectionPool *connections, BareosSocket *fd,
                              char *client_name, int fd_protocol_version);

ConnectionPool *get_client_connections();
bool IsConnectingToClientAllowed(ClientResource *res);
bool IsConnectingToClientAllowed(JobControlRecord *jcr);
bool IsConnectFromClientAllowed(ClientResource *res);
bool IsConnectFromClientAllowed(JobControlRecord *jcr);
bool UseWaitingClient(JobControlRecord *jcr_job, int timeout);

#endif // BAREOS_DIRD_FD_CMDS_H_
