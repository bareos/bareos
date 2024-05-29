/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2018-2024 Bareos GmbH & Co. KG

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
#ifndef BAREOS_STORED_READ_RECORD_H_
#define BAREOS_STORED_READ_RECORD_H_

#include "stored/read_ctx.h"

namespace storagedaemon {

READ_CTX* new_read_context(void);
void FreeReadContext(READ_CTX* rctx);
void ReadContextSetRecord(DeviceControlRecord* dcr, READ_CTX* rctx);
bool ReadNextBlockFromDevice(DeviceControlRecord* dcr,
                             READ_CTX* ctx,
                             Session_Label* sessrec,
                             bool RecordCb(DeviceControlRecord* dcr,
                                           DeviceRecord* rec,
                                           void* user_data),
                             bool mount_cb(DeviceControlRecord* dcr),
                             void* user_data,
                             bool* status);
bool ReadNextRecordFromBlock(DeviceControlRecord* dcr,
                             READ_CTX* rctx,
                             bool* done);
bool ReadRecords(DeviceControlRecord* dcr,
                 bool RecordCb(DeviceControlRecord* dcr, DeviceRecord* rec),
                 bool mount_cb(DeviceControlRecord* dcr));

bool ReadRecords(DeviceControlRecord* dcr,
                 bool RecordCb(DeviceControlRecord* dcr,
                               DeviceRecord* rec,
                               void* user_data),
                 bool mount_cb(DeviceControlRecord* dcr),
                 void* user_data);

template <typename T>
inline bool ReadRecords(DeviceControlRecord* dcr,
                        bool RecordCb(DeviceControlRecord* dcr,
                                      DeviceRecord* rec,
                                      T* user_data),
                        bool mount_cb(DeviceControlRecord* dcr),
                        T* user_data)
{
  auto capture = [RecordCb, user_data](DeviceControlRecord* inner_dcr,
                                       DeviceRecord* inner_rec) -> bool {
    return RecordCb(inner_dcr, inner_rec, user_data);
  };

  return ReadRecords(
      dcr,
      +[](DeviceControlRecord* inner_dcr, DeviceRecord* inner_rec,
          void* impl) -> bool {
        auto* lambda = reinterpret_cast<decltype(&capture)>(impl);
        return (*lambda)(inner_dcr, inner_rec);
      },
      mount_cb, reinterpret_cast<void*>(&capture));
}

} /* namespace storagedaemon */

#endif  // BAREOS_STORED_READ_RECORD_H_
