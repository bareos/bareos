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
#ifndef BAREOS_STORED_MATCH_BSR_H_
#define BAREOS_STORED_MATCH_BSR_H_

namespace storagedaemon {

int MatchBsr(BootStrapRecord* bsr,
             DeviceRecord* rec,
             Volume_Label* volrec,
             Session_Label* sesrec,
             JobControlRecord* jcr);
int MatchBsrBlock(BootStrapRecord* bsr, DeviceBlock* block);
BootStrapRecord* find_next_bsr(BootStrapRecord* root_bsr,
                               BootStrapRecord* bsr,
                               Device* dev);
bool IsThisBsrDone(BootStrapRecord* bsr, DeviceRecord* rec);
uint64_t GetBsrStartAddr(BootStrapRecord* bsr,
                         uint32_t* file = NULL,
                         uint32_t* block = NULL);

} /* namespace storagedaemon */

#endif  // BAREOS_STORED_MATCH_BSR_H_
