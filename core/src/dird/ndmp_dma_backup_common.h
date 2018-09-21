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

#ifndef BAREOS_DIRD_NDMP_DMA_BACKUP_COMMON_H_
#define BAREOS_DIRD_NDMP_DMA_BACKUP_COMMON_H_

struct ndm_job_param;
struct ndmlog;

namespace directordaemon {

bool FillBackupEnvironment(JobControlRecord *jcr,
                             IncludeExcludeItem *ie,
                             char *filesystem,
                             struct ndm_job_param *job);
int NativeToNdmpLevel(JobControlRecord *jcr, char *filesystem);
void RegisterCallbackHooks(struct ndmlog *ixlog);
void UnregisterCallbackHooks(struct ndmlog *ixlog);
void ProcessFhdb(struct ndmlog *ixlog);
void NdmpBackupCleanup(JobControlRecord *jcr, int TermCode);

} /* namespace directordaemon */
#endif // BAREOS_DIRD_NDMP_DMA_BACKUP_COMMON_H_
