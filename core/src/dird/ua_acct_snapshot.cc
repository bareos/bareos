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
/**
 * @file
 * Catalog snapshot (load/save) of the subscription accounting rows.
 */

#include "include/bareos.h"
#include "dird.h"
#include "dird/ua_db.h"
#include "dird/ua_acct_internal.h"
#include "lib/edit.h"

#include <string>
#include <vector>

namespace directordaemon {

namespace {
int SnapshotMetadataHandler(void* ctx, int, char** row)
{
  auto* metadata = static_cast<SnapshotMetadata*>(ctx);
  metadata->timestamp = row[0] ? row[0] : "";
  metadata->last_error = row[1] ? row[1] : "";
  metadata->has_snapshot = !metadata->timestamp.empty();
  metadata->stale = row[2] == nullptr || bstrcmp(row[2], "true");
  metadata->full_completed_at = row[3] ? row[3] : "";
  if (row[4] && row[4][0]) {
    metadata->full_duration_ms = str_to_uint64(row[4]);
  }
  return 0;
}

int SnapshotRowHandler(void* ctx, int, char** row)
{
  auto* rows = static_cast<std::vector<AccountingRow>*>(ctx);
  AccountingRow result;
  result.JobId = row[0] ? str_to_int64(row[0]) : 0;
  result.ClientName = row[1] ? row[1] : "";
  result.FileSetName = row[2] ? row[2] : "";
  result.excluded = row[3] && bstrcmp(row[3], "t");
  result.exclusion_reason = row[4] ? row[4] : "";
  result.files = row[5] ? str_to_uint64(row[5]) : 0;
  result.bytes = row[6] ? str_to_uint64(row[6]) : 0;
  result.logical_bytes = row[7] ? str_to_uint64(row[7]) : 0;
  result.opaque_files = row[8] ? str_to_uint64(row[8]) : 0;
  result.ndmp_archives = row[9] ? str_to_uint64(row[9]) : 0;
  result.rule = row[10] ? row[10] : "";
  result.jobs_in_chain = row[11] ? str_to_uint64(row[11]) : 0;
  result.baseline_full_jobid = row[12] ? str_to_uint64(row[12]) : 0;
  rows->push_back(std::move(result));
  return 0;
}

bool EscapeForSql(UaContext* ua, const std::string& value, std::string* escaped)
{
  auto result = ua->db->EscapeString(ua->jcr, value);
  if (!result) {
    ua->ErrorMsg(T_("Could not escape subscription accounting value.\n"));
    return false;
  }
  *escaped = *result;
  return true;
}

}  // namespace

bool LoadSubscriptionAccountingSnapshot(UaContext* ua,
                                        const char* client_filter,
                                        const char* fileset_filter,
                                        SnapshotMetadata* metadata,
                                        std::vector<AccountingRow>* rows,
                                        bool anonymize)
{
  if (!ua->db->SqlQuery(
          "SELECT COALESCE(to_char(LastSuccess, 'YYYY-MM-DD HH24:MI:SS'), "
          "''), COALESCE(LastError, ''), "
          "(LastSuccess IS NULL OR LastSuccess < CURRENT_TIMESTAMP - "
          "INTERVAL '24 hours')::text, "
          "to_char(LastFullSuccess, 'YYYY-MM-DD HH24:MI:SS'), "
          "LastFullDurationMs::text "
          "FROM SubscriptionAccountingSnapshot WHERE SnapshotId=1",
          SnapshotMetadataHandler, metadata)) {
    ua->ErrorMsg("%s\n", ua->db->strerror());
    return false;
  }

  PoolMem query(PM_MESSAGE);
  Mmsg(query,
       "WITH latest_full AS ("
       " SELECT DISTINCT ON (full_job.ClientId, full_fileset.FileSet) "
       "full_job.ClientId, full_fileset.FileSet, full_job.FileSetId "
       "FROM Job full_job JOIN FileSet full_fileset "
       "USING (FileSetId) "
       "WHERE full_job.JobStatus IN ('T','W') "
       "AND full_job.Type IN ('B','C') AND full_job.Level='F' "
       "ORDER BY full_job.ClientId, full_fileset.FileSet, "
       "full_job.JobTDate DESC, full_job.JobId DESC) "
       "SELECT DISTINCT ON (Job.ClientId, FileSet.FileSet) Job.JobId, "
       "%s, %s, Job.AccountingExcluded, "
       "COALESCE(Job.AccountingExclusionReason, ''), "
       "Job.AccountingFiles::text, Job.AccountingBytes::text, "
       "Job.AccountingLogicalBytes::text, "
       "Job.AccountingOpaqueFiles::text, "
       "Job.AccountingNdmpArchives::text, "
       "COALESCE(Job.AccountingRule, ''), "
       "Job.AccountingJobsInChain::text, "
       "(SELECT baseline.JobId FROM Job baseline "
       "WHERE baseline.ClientId=Job.ClientId "
       "AND baseline.FileSetId=Job.FileSetId AND baseline.Level='F' "
       "AND baseline.JobStatus IN ('T','W') "
       "AND baseline.Type IN ('B','C') "
       "AND (baseline.JobTDate,baseline.JobId)<=(Job.JobTDate,Job.JobId) "
       "ORDER BY baseline.JobTDate DESC,baseline.JobId DESC LIMIT 1)::text "
       "FROM Job JOIN Client USING (ClientId) "
       "JOIN FileSet USING (FileSetId) "
       "JOIN latest_full ON latest_full.ClientId=Job.ClientId "
       "AND latest_full.FileSet=FileSet.FileSet "
       "WHERE Job.AccountingValid AND Job.JobStatus IN ('T','W') "
       "AND Job.Type IN ('B','C')",
       anonymize ? "'Client ' || Job.ClientId" : "Client.Name",
       anonymize ? "'FileSet ' || latest_full.FileSetId" : "FileSet.FileSet");
  std::string escaped;
  if (client_filter) {
    if (!EscapeForSql(ua, client_filter, &escaped)) { return false; }
    PmStrcat(query, " AND Client.Name='");
    PmStrcat(query, escaped.c_str());
    PmStrcat(query, "'");
  }
  if (fileset_filter) {
    if (!EscapeForSql(ua, fileset_filter, &escaped)) { return false; }
    PmStrcat(query, " AND FileSet.FileSet='");
    PmStrcat(query, escaped.c_str());
    PmStrcat(query, "'");
  }
  PmStrcat(query,
           " ORDER BY Job.ClientId, FileSet.FileSet, "
           "(Job.FileSetId=latest_full.FileSetId) DESC, Job.JobTDate DESC, "
           "Job.JobId DESC");

  if (!ua->db->SqlQuery(query.c_str(), SnapshotRowHandler, rows)) {
    ua->ErrorMsg("%s\n", ua->db->strerror());
    return false;
  }
  return true;
}

bool RecordSubscriptionAccountingFailure(UaContext* ua,
                                         const std::string& error)
{
  std::string escaped;
  if (!EscapeForSql(ua, error, &escaped)) { return false; }
  PoolMem query(PM_MESSAGE);
  Mmsg(query,
       "UPDATE SubscriptionAccountingSnapshot SET "
       "LastAttempt=CURRENT_TIMESTAMP, LastError='%s' WHERE SnapshotId=1",
       escaped.c_str());
  return ua->db->SqlExec(query.c_str());
}

bool SaveSubscriptionAccountingSnapshot(UaContext* ua,
                                        const std::vector<AccountingRow>& rows)
{
  // Keep other users of a shared catalog connection out of the transaction
  DbLocker lock_{ua->db};
  if (!ua->db->SqlExec("BEGIN")) {
    ua->ErrorMsg("%s\n", ua->db->strerror());
    return false;
  }
  bool success = true;
  for (const auto& row : rows) {
    std::string reason, rule;
    if (!success || row.JobId == 0
        || !EscapeForSql(ua, row.exclusion_reason, &reason)
        || !EscapeForSql(ua, row.rule, &rule)) {
      success = false;
      break;
    }
    PoolMem query(PM_MESSAGE);
    Mmsg(query,
         "UPDATE Job SET AccountingValid=TRUE, AccountingExcluded=%s, "
         "AccountingExclusionReason=%s, AccountingFiles=%llu, "
         "AccountingBytes=%llu, AccountingLogicalBytes=%llu, "
         "AccountingOpaqueFiles=%llu, AccountingNdmpArchives=%llu, "
         "AccountingRule=%s, AccountingJobsInChain=%llu WHERE JobId=%llu "
         "AND (PurgedFiles=0 OR %s)",
         row.excluded ? "TRUE" : "FALSE",
         row.exclusion_reason.empty() ? "NULL" : ("'" + reason + "'").c_str(),
         static_cast<unsigned long long>(row.files),
         static_cast<unsigned long long>(row.bytes),
         static_cast<unsigned long long>(row.logical_bytes),
         static_cast<unsigned long long>(row.opaque_files),
         static_cast<unsigned long long>(row.ndmp_archives),
         row.rule.empty() ? "NULL" : ("'" + rule + "'").c_str(),
         static_cast<unsigned long long>(row.jobs_in_chain),
         static_cast<unsigned long long>(row.JobId),
         row.excluded ? "TRUE" : "FALSE");
    success = ua->db->SqlExec(query.c_str());
  }

  if (success) {
    success = ua->db->SqlExec(
        "UPDATE SubscriptionAccountingSnapshot SET "
        "LastAttempt=CURRENT_TIMESTAMP, LastSuccess=CURRENT_TIMESTAMP, "
        "LastError=NULL WHERE SnapshotId=1");
  }
  if (success) { success = ua->db->SqlExec("COMMIT"); }
  if (!success) {
    ua->db->SqlExec("ROLLBACK");
    ua->ErrorMsg("%s\n", ua->db->strerror());
  }
  return success;
}

} /* namespace directordaemon */
