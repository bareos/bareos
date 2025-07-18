/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2012 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2024 Bareos GmbH & Co. KG

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

#ifndef BAREOS_STORED_READ_CTX_H_
#define BAREOS_STORED_READ_CTX_H_

#include "stored/record.h"

namespace storagedaemon {

struct DeviceRecord;

/* clang-format off */
struct Read_Context {
  DeviceRecord* rec = nullptr;    /**< Record currently being processed */
  dlist<DeviceRecord>* recs = nullptr;          /**< Linked list of record packets open */
  Session_Label sessrec;          /**< Start Of Session record info */
  uint32_t records_processed = 0; /**< Number of records processed from this block */
  int32_t lastFileIndex = 0;      /**< Last File Index processed */

  BootStrapRecord* current = nullptr;
};
/* clang-format on */

typedef struct Read_Context READ_CTX;

} /* namespace storagedaemon  */

#endif  // BAREOS_STORED_READ_CTX_H_
