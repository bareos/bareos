/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2026-2026 Bareos GmbH & Co. KG

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
#ifndef BAREOS_FINDLIB_FILELIST_H_
#define BAREOS_FINDLIB_FILELIST_H_

#include <string>
#include <vector>

class JobControlRecord;
struct FindFilesPacket;

class FileList {
 public:
  virtual ~FileList() = default;
  virtual bool Prepare(JobControlRecord*, FindFilesPacket*, const char*) = 0;
  virtual const std::vector<std::string>* Children(const char*) const = 0;
  virtual bool Unchanged(JobControlRecord*, FindFilesPacket*, const char*) = 0;
};

#endif  // BAREOS_FINDLIB_FILELIST_H_
