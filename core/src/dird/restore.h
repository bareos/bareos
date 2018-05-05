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

#ifndef BAREOS_DIRD_RESTORE_H_
#define BAREOS_DIRD_RESTORE_H_

bool DoNativeRestore(JobControlRecord *jcr);
bool DoNativeRestoreInit(JobControlRecord *jcr);
void NativeRestoreCleanup(JobControlRecord *jcr, int TermCode);
void GenerateRestoreSummary(JobControlRecord *jcr, int msg_type, const char *TermMsg);

#endif // BAREOS_DIRD_RESTORE_H_
