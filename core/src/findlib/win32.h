
/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2018-2025 Bareos GmbH & Co. KG

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
#ifndef BAREOS_FINDLIB_WIN32_H_
#define BAREOS_FINDLIB_WIN32_H_

#include <vector>
#include <string>
#include "find.h"
class JobControlRecord;

bool expand_win32_fileset(findFILESET* fileset);
bool exclude_win32_not_to_backup_registry_entries(JobControlRecord* jcr,
                                                  FindFilesPacket* ff);
std::vector<std::wstring> get_win32_volumes(findFILESET* fileset);
bool win32_onefs_is_disabled(findFILESET* fileset);
void win32_cleanup_copy_thread(JobControlRecord* jcr);
void win32_flush_copy_thread(JobControlRecord* jcr);
int win32_send_to_copy_thread(JobControlRecord* jcr,
                              BareosFilePacket* bfd,
                              char* data,
                              const int32_t length);

#endif  // BAREOS_FINDLIB_WIN32_H_
