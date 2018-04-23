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

#ifndef BAREOS_DIRD_NDMP_DMA_STORAGE_H_
#define BAREOS_DIRD_NDMP_DMA_STORAGE_H_

void do_ndmp_storage_status(UaContext *ua, StoreResource *store, char *cmd);
dlist *ndmp_get_vol_list(UaContext *ua, StoreResource *store, bool listall, bool scan);
slot_number_t ndmp_get_num_slots(UaContext *ua, StoreResource *store);
drive_number_t ndmp_get_num_drives(UaContext *ua, StoreResource *store);
bool ndmp_transfer_volume(UaContext *ua, StoreResource *store,
                          slot_number_t src_slot, slot_number_t dst_slot);
bool ndmp_autochanger_volume_operation(UaContext *ua, StoreResource *store, const char *operation,
                                       drive_number_t drive, slot_number_t slot);
bool ndmp_send_label_request(UaContext *ua, StoreResource *store, MediaDbRecord *mr,
                             MediaDbRecord *omr, PoolDbRecord *pr, bool relabel,
                             drive_number_t drive, slot_number_t slot);
char *lookup_ndmp_drive(StoreResource *store, drive_number_t drive);
bool ndmp_update_storage_mappings(JobControlRecord* jcr, StoreResource *store);

#endif // BAREOS_DIRD_NDMP_DMA_STORAGE_H_
