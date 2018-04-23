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

bool connect_to_storage_daemon(JobControlRecord *jcr, int retry_interval,
                               int max_retry_time, bool verbose);
BareosSocket *open_sd_bsock(UaContext *ua);
void close_sd_bsock(UaContext *ua);
char *get_volume_name_from_SD(UaContext *ua, slot_number_t Slot, drive_number_t drive);
dlist *native_get_vol_list(UaContext *ua, StoreResource *store, bool listall, bool scan);
slot_number_t native_get_num_slots(UaContext *ua, StoreResource *store);
drive_number_t native_get_num_drives(UaContext *ua, StoreResource *store);
bool cancel_storage_daemon_job(UaContext *ua, StoreResource *store, char *JobId);
bool cancel_storage_daemon_job(UaContext *ua, JobControlRecord *jcr, bool interactive = true);
void cancel_storage_daemon_job(JobControlRecord *jcr);
void do_native_storage_status(UaContext *ua, StoreResource *store, char *cmd);
bool native_transfer_volume(UaContext *ua, StoreResource *store,
                            slot_number_t src_slot, slot_number_t dst_slot);
bool native_autochanger_volume_operation(UaContext *ua, StoreResource *store, const char *operation,
                                         drive_number_t drive, slot_number_t slot);
bool send_bwlimit_to_sd(JobControlRecord *jcr, const char *Job);
bool send_secure_erase_req_to_sd(JobControlRecord *jcr);
bool do_storage_resolve(UaContext *ua, StoreResource *store);
bool do_storage_plugin_options(JobControlRecord *jcr);

#endif // BAREOS_DIRD_SD_CMDS_H_
