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
#ifndef BAREOS_FINDLIB_FIND_ONE_H_
#define BAREOS_FINDLIB_FIND_ONE_H_

DLL_IMP_EXP int FindOneFile(JobControlRecord *jcr, FindFilesPacket *ff,
                  int HandleFile(JobControlRecord *jcr, FindFilesPacket *ff_pkt, bool top_level),
                  char *p, dev_t parent_device, bool top_level);
DLL_IMP_EXP int TermFindOne(FindFilesPacket *ff);
DLL_IMP_EXP bool HasFileChanged(JobControlRecord *jcr, FindFilesPacket *ff_pkt);
DLL_IMP_EXP bool CheckChanges(JobControlRecord *jcr, FindFilesPacket *ff_pkt);

#endif // BAREOS_FINDLIB_FIND_ONE_H_
