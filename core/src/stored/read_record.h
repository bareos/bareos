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
#ifndef STORED_READ_RECORD_H_
#define STORED_READ_RECORD_H_

DLL_IMP_EXP READ_CTX *new_read_context(void);
DLL_IMP_EXP void free_read_context(READ_CTX *rctx);
DLL_IMP_EXP void read_context_set_record(DeviceControlRecord *dcr, READ_CTX *rctx);
DLL_IMP_EXP bool read_next_block_from_device(DeviceControlRecord *dcr,
                                 SESSION_LABEL *sessrec,
                                 bool record_cb(DeviceControlRecord *dcr, DeviceRecord *rec),
                                 bool mount_cb(DeviceControlRecord *dcr),
                                 bool *status);
DLL_IMP_EXP bool read_next_record_from_block(DeviceControlRecord *dcr,
                                 READ_CTX *rctx,
                                 bool *done);
DLL_IMP_EXP bool read_records(DeviceControlRecord *dcr,
                  bool record_cb(DeviceControlRecord *dcr, DeviceRecord *rec),
                  bool mount_cb(DeviceControlRecord *dcr));


#endif // STORED_READ_RECORD_H_
