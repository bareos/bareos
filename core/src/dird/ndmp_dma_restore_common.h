/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2018-2026 Bareos GmbH & Co. KG

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

#include <cstdint>
#include <vector>

struct ndm_job_param;
struct ndm_session;
struct s_tree_root;
class JobControlRecord;

namespace directordaemon {

void AddToNamelist(struct ndm_job_param* job,
                   char* filename,
                   const char* restore_prefix,
                   char* name,
                   char* other_name,
                   uint64_t node,
                   uint64_t fhinfo,
                   bool set_zero_for_invalid_u_quad = false);
int SetFilesToRestoreNdmpNative(JobControlRecord* jcr,
                                struct ndm_job_param* job,
                                std::uint32_t JobId,
                                const char* restore_prefix,
                                const char* ndmp_filesystem);
int NdmpEnvHandler(void* ctx, int num_fields, char** row);
/* Return the jobs from the comma separated JobIds whose images have to be
 * recovered, in the order of the list. A job is needed when at least one
 * selected file in the restore tree has its version from that job. When no
 * file is selected at all, every image is needed. */
std::vector<std::uint32_t> NdmpNativeJobsToRestore(
    const char* JobIds,
    s_tree_root* restore_tree_root);
bool ExtractPostRestoreStats(JobControlRecord* jcr, struct ndm_session* sess);

struct NdmpRecoveredFiles {
  std::uint32_t restored{};
  std::uint32_t failed{};
};

/* The data server reports the result of each name list entry with
 * NDMP_LOG_FILE. Data servers that do not send it are counted as one
 * restored file per successfully recovered image. */
constexpr NdmpRecoveredFiles NdmpNativeRecoveredFiles(int log_file_count,
                                                      int log_file_ok,
                                                      bool session_ok)
{
  if (log_file_count <= 0) { return {session_ok ? 1u : 0u, 0}; }
  std::uint32_t ok = log_file_ok > 0 ? log_file_ok : 0;
  std::uint32_t failed
      = log_file_count > log_file_ok
            ? static_cast<std::uint32_t>(log_file_count - log_file_ok)
            : 0;
  return {ok, failed};
}

/* Update the job statistics of a NDMP_NATIVE restore image, also for
 * sessions that failed. Returns false if the session failed. */
bool ExtractPostRestoreStatsNdmpNative(JobControlRecord* jcr,
                                       struct ndm_session* sess,
                                       bool session_ok);
void NdmpRestoreCleanup(JobControlRecord* jcr, int TermCode);

} /* namespace directordaemon */
#endif  // BAREOS_DIRD_NDMP_DMA_RESTORE_COMMON_H_
