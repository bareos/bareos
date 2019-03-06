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

#ifndef BAREOS_DIRD_SD_CMDS_H_
#define BAREOS_DIRD_SD_CMDS_H_

class JobControlRecord;
class dlist;

namespace directordaemon {

class UaContext;
class StorageResource;

bool ConnectToStorageDaemon(JobControlRecord* jcr,
                            int retry_interval,
                            int max_retry_time,
                            bool verbose);
BareosSocket* open_sd_bsock(UaContext* ua);
void CloseSdBsock(UaContext* ua);
char* get_volume_name_from_SD(UaContext* ua,
                              slot_number_t Slot,
                              drive_number_t drive);
dlist* native_get_vol_list(UaContext* ua,
                           StorageResource* store,
                           bool listall,
                           bool scan);
slot_number_t NativeGetNumSlots(UaContext* ua, StorageResource* store);
drive_number_t NativeGetNumDrives(UaContext* ua, StorageResource* store);
bool CancelStorageDaemonJob(UaContext* ua, StorageResource* store, char* JobId);
bool CancelStorageDaemonJob(UaContext* ua,
                            JobControlRecord* jcr,
                            bool interactive = true);
void CancelStorageDaemonJob(JobControlRecord* jcr);
void DoNativeStorageStatus(UaContext* ua, StorageResource* store, char* cmd);
bool NativeTransferVolume(UaContext* ua,
                          StorageResource* store,
                          slot_number_t src_slot,
                          slot_number_t dst_slot);
bool NativeAutochangerVolumeOperation(UaContext* ua,
                                      StorageResource* store,
                                      const char* operation,
                                      drive_number_t drive,
                                      slot_number_t slot);
bool SendBwlimitToSd(JobControlRecord* jcr, const char* Job);
bool SendSecureEraseReqToSd(JobControlRecord* jcr);
bool DoStorageResolve(UaContext* ua, StorageResource* store);
bool SendStoragePluginOptions(JobControlRecord* jcr);

} /* namespace directordaemon */
#endif  // BAREOS_DIRD_SD_CMDS_H_
