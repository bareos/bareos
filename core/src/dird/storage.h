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

#ifndef DIRD_STORAGE_H_
#define DIRD_STORAGE_H_

void copy_rwstorage(JobControlRecord *jcr, alist *storage, const char *where);
void set_rwstorage(JobControlRecord *jcr, UnifiedStoreResource *store);
void free_rwstorage(JobControlRecord *jcr);
void copy_wstorage(JobControlRecord *jcr, alist *storage, const char *where);
void set_wstorage(JobControlRecord *jcr, UnifiedStoreResource *store);
void free_wstorage(JobControlRecord *jcr);
void copy_rstorage(JobControlRecord *jcr, alist *storage, const char *where);
void set_rstorage(JobControlRecord *jcr, UnifiedStoreResource *store);
void free_rstorage(JobControlRecord *jcr);
void set_paired_storage(JobControlRecord *jcr);
void free_paired_storage(JobControlRecord *jcr);
bool has_paired_storage(JobControlRecord *jcr);
bool select_next_rstore(JobControlRecord *jcr, bootstrap_info &info);
void storage_status(UaContext *ua, StoreResource *store, char *cmd);
int storage_compare_vol_list_entry(void *e1, void *e2);
changer_vol_list_t *get_vol_list_from_storage(UaContext *ua, StoreResource *store,
                                              bool listall, bool scan, bool cached = true);
slot_number_t get_num_slots(UaContext *ua, StoreResource *store);
drive_number_t get_num_drives(UaContext *ua, StoreResource *store);
bool transfer_volume(UaContext *ua, StoreResource *store,
                     slot_number_t src_slot, slot_number_t dst_slot);
bool do_autochanger_volume_operation(UaContext *ua, StoreResource *store,
                                     const char *operation,
                                     drive_number_t drive, slot_number_t slot);
vol_list_t *vol_is_loaded_in_drive(StoreResource *store, changer_vol_list_t *vol_list, slot_number_t slot);
void storage_release_vol_list(StoreResource *store, changer_vol_list_t *vol_list);
void storage_free_vol_list(StoreResource *store, changer_vol_list_t *vol_list);
void invalidate_vol_list(StoreResource *store);
int compare_storage_mapping(void *e1, void *e2);
slot_number_t lookup_storage_mapping(StoreResource *store, slot_type slot_type,
                                     s_mapping_type map_type, slot_number_t slot);
#endif // DIRD_STORAGE_H_
