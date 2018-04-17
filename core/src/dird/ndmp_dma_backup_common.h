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

#ifndef DIRD_NDMP_DMA_BACKUP_COMMON_H_
#define DIRD_NDMP_DMA_BACKUP_COMMON_H_

bool fill_backup_environment(JobControlRecord *jcr,
                             IncludeExcludeItem *ie,
                             char *filesystem,
                             struct ndm_job_param *job);
int native_to_ndmp_level(JobControlRecord *jcr, char *filesystem);
void register_callback_hooks(struct ndmlog *ixlog);
void unregister_callback_hooks(struct ndmlog *ixlog);
void process_fhdb(struct ndmlog *ixlog);
void ndmp_backup_cleanup(JobControlRecord *jcr, int TermCode);

#endif // DIRD_NDMP_DMA_BACKUP_COMMON_H_
