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

#ifndef BAREOS_DIRD_STORAGE_H_
#define BAREOS_DIRD_STORAGE_H_

void CopyRwstorage(JobControlRecord *jcr, alist *storage, const char *where);
void SetRwstorage(JobControlRecord *jcr, UnifiedStorageResource *store);
void FreeRwstorage(JobControlRecord *jcr);
void CopyWstorage(JobControlRecord *jcr, alist *storage, const char *where);
void SetWstorage(JobControlRecord *jcr, UnifiedStorageResource *store);
void FreeWstorage(JobControlRecord *jcr);
void CopyRstorage(JobControlRecord *jcr, alist *storage, const char *where);
void SetRstorage(JobControlRecord *jcr, UnifiedStorageResource *store);
void FreeRstorage(JobControlRecord *jcr);
void SetPairedStorage(JobControlRecord *jcr);
void FreePairedStorage(JobControlRecord *jcr);
bool HasPairedStorage(JobControlRecord *jcr);
bool SelectNextRstore(JobControlRecord *jcr, bootstrap_info &info);
void StorageStatus(UaContext *ua, StorageResource *store, char *cmd);
int StorageCompareVolListEntry(void *e1, void *e2);
changer_vol_list_t *get_vol_list_from_storage(UaContext *ua, StorageResource *store,
                                              bool listall, bool scan, bool cached = true);
slot_number_t GetNumSlots(UaContext *ua, StorageResource *store);
drive_number_t GetNumDrives(UaContext *ua, StorageResource *store);
bool transfer_volume(UaContext *ua, StorageResource *store,
                     slot_number_t src_slot, slot_number_t dst_slot);
bool DoAutochangerVolumeOperation(UaContext *ua, StorageResource *store,
                                     const char *operation,
                                     drive_number_t drive, slot_number_t slot);
vol_list_t *vol_is_loaded_in_drive(StorageResource *store, changer_vol_list_t *vol_list, slot_number_t slot);
void StorageReleaseVolList(StorageResource *store, changer_vol_list_t *vol_list);
void StorageFreeVolList(StorageResource *store, changer_vol_list_t *vol_list);
void InvalidateVolList(StorageResource *store);
int CompareStorageMapping(void *e1, void *e2);
slot_number_t LookupStorageMapping(StorageResource *store, slot_type slot_type,
                                     s_mapping_type map_type, slot_number_t slot);
#endif // BAREOS_DIRD_STORAGE_H_
