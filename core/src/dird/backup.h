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

#ifndef BAREOS_DIRD_BACKUP_H_
#define BAREOS_DIRD_BACKUP_H_

namespace directordaemon {

int WaitForJobTermination(JobControlRecord* jcr, int timeout = 0);
bool DoNativeBackupInit(JobControlRecord* jcr);
bool DoNativeBackup(JobControlRecord* jcr);
void NativeBackupCleanup(JobControlRecord* jcr, int TermCode);
void UpdateBootstrapFile(JobControlRecord* jcr);
bool SendAccurateCurrentFiles(JobControlRecord* jcr);
void GenerateBackupSummary(JobControlRecord* jcr,
                           ClientDbRecord* cr,
                           int msg_type,
                           const char* TermMsg);

char* StorageAddressToContact(ClientResource* client, StorageResource* store);
char* ClientAddressToContact(ClientResource* client, StorageResource* store);
char* StorageAddressToContact(StorageResource* read_storage,
                              StorageResource* write_storage);

} /* namespace directordaemon */
#endif  // BAREOS_DIRD_BACKUP_H_
