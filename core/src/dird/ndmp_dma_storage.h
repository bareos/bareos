/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2018-2019 Bareos GmbH & Co. KG

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

#ifndef BAREOS_DIRD_NDMP_DMA_STORAGE_H_
#define BAREOS_DIRD_NDMP_DMA_STORAGE_H_

struct ndm_job_param;

namespace directordaemon {

void DoNdmpStorageStatus(UaContext *ua, StorageResource *store, char *cmd);
dlist *ndmp_get_vol_list(UaContext *ua, StorageResource *store, bool listall, bool scan);
slot_number_t NdmpGetNumSlots(UaContext *ua, StorageResource *store);
drive_number_t NdmpGetNumDrives(UaContext *ua, StorageResource *store);
bool NdmpTransferVolume(UaContext *ua, StorageResource *store,
                          slot_number_t src_slot, slot_number_t dst_slot);
bool NdmpAutochangerVolumeOperation(UaContext *ua, StorageResource *store, const char *operation,
                                       drive_number_t drive, slot_number_t slot);
bool NdmpSendLabelRequest(UaContext *ua, StorageResource *store, MediaDbRecord *mr,
                             MediaDbRecord *omr, PoolDbRecord *pr, bool relabel,
                             drive_number_t drive, slot_number_t slot);
char *lookup_ndmp_drive(StorageResource *store, drive_number_t drive);
bool NdmpUpdateStorageMappings(JobControlRecord* jcr, StorageResource *store);
bool ndmp_native_setup_robot_and_tape_for_native_backup_job(JobControlRecord* jcr, StorageResource* store,
      ndm_job_param& ndmp_job);
bool unreserve_ndmp_tapedevice_for_job(StorageResource *store, JobControlRecord *jcr);
} /* namespace directordaemon */
#endif // BAREOS_DIRD_NDMP_DMA_STORAGE_H_
