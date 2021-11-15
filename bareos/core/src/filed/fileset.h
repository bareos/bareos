/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2018-2021 Bareos GmbH & Co. KG

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

#ifndef BAREOS_FILED_FILESET_H_
#define BAREOS_FILED_FILESET_H_

namespace filedaemon {

bool InitFileset(JobControlRecord* jcr);
void AddFileToFileset(JobControlRecord* jcr, const char* fname, bool IsFile);
findIncludeExcludeItem* get_incexe(JobControlRecord* jcr);
void SetIncexe(JobControlRecord* jcr, findIncludeExcludeItem* incexe);
int AddRegexToFileset(JobControlRecord* jcr, const char* item, int type);
int AddWildToFileset(JobControlRecord* jcr, const char* item, int type);
int AddOptionsToFileset(JobControlRecord* jcr, const char* item);
void AddFileset(JobControlRecord* jcr, const char* item);
bool TermFileset(JobControlRecord* jcr);

} /* namespace filedaemon */

#endif  // BAREOS_FILED_FILESET_H_
