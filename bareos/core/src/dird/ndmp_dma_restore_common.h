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

#ifndef BAREOS_DIRD_NDMP_DMA_RESTORE_COMMON_H_
#define BAREOS_DIRD_NDMP_DMA_RESTORE_COMMON_H_

struct ndm_job_param;
struct ndm_session;
class JobControlRecord;

namespace directordaemon {

void AddToNamelist(struct ndm_job_param* job,
                   char* filename,
                   const char* restore_prefix,
                   char* name,
                   char* other_name,
                   uint64_t node,
                   uint64_t fhinfo);
int SetFilesToRestoreNdmpNative(JobControlRecord* jcr,
                                struct ndm_job_param* job,
                                int32_t FileIndex,
                                const char* restore_prefix,
                                const char* ndmp_filesystem);
int NdmpEnvHandler(void* ctx, int num_fields, char** row);
bool ExtractPostRestoreStats(JobControlRecord* jcr, struct ndm_session* sess);
void NdmpRestoreCleanup(JobControlRecord* jcr, int TermCode);

} /* namespace directordaemon */
#endif  // BAREOS_DIRD_NDMP_DMA_RESTORE_COMMON_H_
