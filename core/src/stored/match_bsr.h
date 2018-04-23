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
#ifndef BAREOS_STORED_MATCH_BSR_H_
#define BAREOS_STORED_MATCH_BSR_H_

int match_bsr(BootStrapRecord *bsr, DeviceRecord *rec, VOLUME_LABEL *volrec,
              SESSION_LABEL *sesrec, JobControlRecord *jcr);
int match_bsr_block(BootStrapRecord *bsr, DeviceBlock *block);
void position_bsr_block(BootStrapRecord *bsr, DeviceBlock *block);
BootStrapRecord *find_next_bsr(BootStrapRecord *root_bsr, Device *dev);
bool is_this_bsr_done(BootStrapRecord *bsr, DeviceRecord *rec);
uint64_t get_bsr_start_addr(BootStrapRecord *bsr,
                            uint32_t *file = NULL,
                            uint32_t *block = NULL);

#endif // BAREOS_STORED_MATCH_BSR_H_
