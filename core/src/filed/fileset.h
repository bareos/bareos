/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2018-2023 Bareos GmbH & Co. KG

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

#include "findlib/find.h"

namespace filedaemon {

bool InitFileset(JobControlRecord* jcr);
void AddFileToFileset(JobControlRecord* jcr,
                      const char* fname,
                      bool IsFile,
                      findIncludeExcludeItem* incexe);
findIncludeExcludeItem* get_incexe(JobControlRecord* jcr);
void SetIncexe(JobControlRecord* jcr, findIncludeExcludeItem* incexe);
int AddRegexToFileset(JobControlRecord* jcr, const char* item, int type);
int AddWildToFileset(JobControlRecord* jcr, const char* item, int type);
int AddOptionsFlagsToFileset(JobControlRecord* jcr, const char* item);
void AddFileset(JobControlRecord* jcr, const char* item);
bool TermFileset(JobControlRecord* jcr);
std::optional<std::string> job_code_callback_filed(JobControlRecord* jcr,
                                                   const char*);

} /* namespace filedaemon */

#endif  // BAREOS_FILED_FILESET_H_
