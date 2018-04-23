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

bool is_volume_purged(UaContext *ua, MediaDbRecord *mr, bool force = false);
bool mark_media_purged(UaContext *ua, MediaDbRecord *mr);
void purge_files_from_volume(UaContext *ua, MediaDbRecord *mr);
bool purge_jobs_from_volume(UaContext *ua, MediaDbRecord *mr, bool force = false);
void purge_files_from_jobs(UaContext *ua, char *jobs);
void purge_jobs_from_catalog(UaContext *ua, char *jobs);
void purge_job_list_from_catalog(UaContext *ua, del_ctx &del);
void purge_files_from_job_list(UaContext *ua, del_ctx &del);

#endif // BAREOS_DIRD_UA_PURGE_H_
