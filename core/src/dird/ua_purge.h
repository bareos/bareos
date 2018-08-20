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

#ifndef BAREOS_DIRD_UA_PURGE_H_
#define BAREOS_DIRD_UA_PURGE_H_

#include "dird/ua.h"

namespace directordaemon {

bool IsVolumePurged(UaContext *ua, MediaDbRecord *mr, bool force = false);
bool MarkMediaPurged(UaContext *ua, MediaDbRecord *mr);
void PurgeFilesFromVolume(UaContext *ua, MediaDbRecord *mr);
bool PurgeJobsFromVolume(UaContext *ua, MediaDbRecord *mr, bool force = false);
void PurgeFilesFromJobs(UaContext *ua, char *jobs);
void PurgeJobsFromCatalog(UaContext *ua, char *jobs);
void PurgeJobListFromCatalog(UaContext *ua, del_ctx &del);
void PurgeFilesFromJobList(UaContext *ua, del_ctx &del);

} /* namespace directordaemon */
#endif // BAREOS_DIRD_UA_PURGE_H_
