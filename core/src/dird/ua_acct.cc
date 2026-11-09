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
 * 'status subscriptions [client=<name>]' and the
 * background worker maintaining the per-job results it displays (see
 * ua_acct_calc.cc and ua_acct_snapshot.cc).
 */

#include "include/bareos.h"
#include "dird.h"
#include "dird/dird_globals.h"
#include "dird/ua_db.h"
#include "dird/ua_select.h"
#include "dird/ua_acct.h"
#include "dird/ua_acct_internal.h"
#include "dird/subscription_accounting_table.h"
#include "lib/attribs.h"
#include "lib/edit.h"
#include "lib/stat_utils.h"
#include "dird/director_jcr_impl.h"
#include "dird/get_database_connection.h"
#include "dird/jcr_util.h"
#include "cats/sql_pooling.h"
#include "lib/berrno.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <limits>
#include <pthread.h>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace directordaemon {

namespace {

static std::atomic_bool accounting_quit{false};
static pthread_t accounting_thread_id;
static pthread_mutex_t accounting_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t accounting_cond = PTHREAD_COND_INITIALIZER;
static bool accounting_thread_initialized{false};
static bool accounting_worker_available{false};
static bool accounting_refresh_requested{false};
static bool accounting_refresh_running{false};
static bool accounting_job_running{false};
static uint64_t accounting_generation{0};
static std::set<JobId_t> accounting_jobs;

struct MissingAccountingJobsCtx {
  std::vector<JobId_t> jobids;
};

int MissingAccountingJobHandler(void* ctx, int, char** row)
{
  auto* jobs = static_cast<MissingAccountingJobsCtx*>(ctx);
  jobs->jobids.push_back(static_cast<JobId_t>(str_to_int64(row[0])));
  return 0;
}

const char* SubscriptionAccountingThreadState(
    const SubscriptionAccountingThreadStatus& status)
{
  return !status.available ? T_("unavailable")
         : status.running  ? T_("running")
         : status.queued   ? T_("queued")
                           : T_("idle");
}

void RefreshSubscriptionAccounting(UaContext* ua)
{
  const auto started = std::chrono::steady_clock::now();
  std::vector<AccountingRow> rows;
  std::string error;
  std::optional<SubscriptionAccountingDiagnostic> diagnostic;
  if (!CalculateSubscriptionAccounting(ua, &rows, &error, &accounting_quit,
                                       &diagnostic)) {
    if (accounting_quit.load()) { return; }
    if (!RecordSubscriptionAccountingFailure(
            ua, error, diagnostic ? &*diagnostic : nullptr)) {
      Jmsg(ua->jcr, M_ERROR, 0,
           T_("Could not record subscription accounting refresh failure: %s\n"),
           ua->db->strerror());
    }
    Jmsg(ua->jcr, M_ERROR, 0,
         T_("Subscription accounting refresh failed: %s\n"), error.c_str());
    return;
  }
  if (accounting_quit.load()) { return; }
  if (!SaveSubscriptionAccountingSnapshot(ua, rows)) {
    std::string save_error = ua->db->strerror();
    RecordSubscriptionAccountingFailure(ua, save_error);
    Jmsg(ua->jcr, M_ERROR, 0,
         T_("Could not save subscription accounting snapshot: %s\n"),
         save_error.c_str());
    return;
  }
  const auto duration_ms
      = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - started)
            .count();
  PoolMem timing_query(PM_MESSAGE);
  Mmsg(timing_query,
       "UPDATE SubscriptionAccountingSnapshot SET "
       "LastFullSuccess=clock_timestamp(), LastFullDurationMs=%llu "
       "WHERE SnapshotId=1",
       static_cast<unsigned long long>(duration_ms));
  if (!ua->db->SqlExec(timing_query.c_str())) {
    Jmsg(ua->jcr, M_ERROR, 0,
         T_("Could not save subscription recalculation duration: %s\n"),
         ua->db->strerror());
  }
  Jmsg(ua->jcr, M_INFO, 0,
       T_("Subscription accounting snapshot refreshed with %zu "
          "Client/FileSet combination(s).\n"),
       rows.size());
}

void RefreshSubscriptionAccountingJob(UaContext* ua, JobId_t jobid)
{
  AccountingRow row;
  std::string error;
  std::optional<SubscriptionAccountingDiagnostic> diagnostic;
  if (!CalculateSubscriptionAccountingForJob(ua, jobid, &row, &error,
                                             &accounting_quit, &diagnostic)) {
    if (accounting_quit.load()) { return; }
    if (!RecordSubscriptionAccountingFailure(
            ua, error, diagnostic ? &*diagnostic : nullptr)) {
      Jmsg(ua->jcr, M_ERROR, 0,
           T_("Could not record subscription accounting job failure: %s\n"),
           ua->db->strerror());
    }
    Jmsg(ua->jcr, M_ERROR, 0,
         T_("Subscription accounting for JobId %u failed: %s\n"), jobid,
         error.c_str());
    return;
  }
  if (accounting_quit.load() || row.JobId == 0) { return; }
  if (!SaveSubscriptionAccountingSnapshot(ua, {row})) {
    std::string save_error = ua->db->strerror();
    RecordSubscriptionAccountingFailure(ua, save_error);
    Jmsg(ua->jcr, M_ERROR, 0,
         T_("Could not save subscription accounting for JobId %u: %s\n"), jobid,
         save_error.c_str());
  }
}

void* SubscriptionAccountingThread(void*)
{
  auto config = my_config->GetCurrentConfiguration();
  auto* catalog
      = static_cast<CatalogResource*>(config->GetNextRes(R_CATALOG, nullptr));
  if (!catalog) {
    Jmsg(nullptr, M_ERROR, 0,
         T_("No catalog resource for subscription accounting refresh.\n"));
    pthread_mutex_lock(&accounting_mutex);
    accounting_worker_available = false;
    pthread_mutex_unlock(&accounting_mutex);
    return nullptr;
  }
  if (config->GetNextRes(R_CATALOG, catalog)) {
    Jmsg(nullptr, M_WARNING, 0,
         T_("Subscription accounting does not support multiple catalogs; "
            "only the first configured catalog will be refreshed.\n"));
  }

  JobControlRecord* jcr = NewDirectorJcr(config);
  jcr->dir_impl->res.catalog = catalog;
  jcr->db = GetDatabaseConnection(jcr);
  if (!jcr->db) {
    Jmsg(jcr, M_ERROR, 0,
         T_("Could not open catalog for subscription "
            "accounting refresh.\n"));
    FreeJcr(jcr);
    pthread_mutex_lock(&accounting_mutex);
    accounting_worker_available = false;
    pthread_mutex_unlock(&accounting_mutex);
    return nullptr;
  }

  {
    UaContext ua(jcr);
    ua.SetConsoleConnected(false);
    pthread_mutex_lock(&accounting_mutex);
    accounting_worker_available = true;
    pthread_mutex_unlock(&accounting_mutex);
    QueueMissingSubscriptionAccounting(&ua);
    while (!accounting_quit.load()) {
      pthread_mutex_lock(&accounting_mutex);
      while (!accounting_quit.load() && !accounting_refresh_requested
             && accounting_jobs.empty()) {
        pthread_cond_wait(&accounting_cond, &accounting_mutex);
      }
      const bool refresh_due
          = !accounting_quit.load() && accounting_refresh_requested;
      JobId_t jobid = 0;
      if (refresh_due) {
        accounting_refresh_requested = false;
        accounting_refresh_running = true;
      } else if (!accounting_quit.load() && !accounting_jobs.empty()) {
        auto it = accounting_jobs.begin();
        jobid = *it;
        accounting_jobs.erase(it);
        accounting_job_running = true;
      }
      pthread_mutex_unlock(&accounting_mutex);
      if (accounting_quit.load()) { break; }
      if (refresh_due) {
        RefreshSubscriptionAccounting(&ua);
        pthread_mutex_lock(&accounting_mutex);
        accounting_refresh_running = false;
        ++accounting_generation;
        pthread_mutex_unlock(&accounting_mutex);
      } else if (jobid != 0) {
        RefreshSubscriptionAccountingJob(&ua, jobid);
        pthread_mutex_lock(&accounting_mutex);
        accounting_job_running = false;
        ++accounting_generation;
        pthread_mutex_unlock(&accounting_mutex);
      }
    }
  }

  DbSqlClosePooledConnection(jcr, jcr->db);
  jcr->db = nullptr;
  FreeJcr(jcr);
  pthread_mutex_lock(&accounting_mutex);
  accounting_worker_available = false;
  pthread_mutex_unlock(&accounting_mutex);
  return nullptr;
}

}  // namespace

bool ReportSubscriptionAccounting(UaContext* ua,
                                  bool anonymize,
                                  size_t pending_jobs)
{
  const auto thread_status = GetSubscriptionAccountingThreadStatus();
  const char* client_filter = GetArgValue(ua, NT_("client"));
  std::vector<AccountingRow> rows;
  SnapshotMetadata metadata{};
  if (!LoadSubscriptionAccountingSnapshot(ua, client_filter, nullptr, &metadata,
                                          &rows, anonymize)) {
    return false;
  }
  if (anonymize) {
    if (!metadata.last_error.empty()) {
      metadata.last_error = T_("Last recalculation failed.");
    }
  }
  const char* thread_state = SubscriptionAccountingThreadState(thread_status);
  ua->SendMsg(T_("Background refresh worker: %s.\n"), thread_state);

  ua->send->ObjectStart("accounting_snapshot");
  ua->send->ObjectKeyValue("generation", thread_status.generation);
  ua->send->ObjectKeyValueBool("worker_available", thread_status.available);
  ua->send->ObjectKeyValueBool("queued", thread_status.queued);
  ua->send->ObjectKeyValueBool("running", thread_status.running);
  ua->send->ObjectKeyValueBool("available", metadata.has_snapshot);
  ua->send->ObjectKeyValue("calculated_at", metadata.timestamp.c_str());
  ua->send->ObjectKeyValueBool("stale", metadata.stale);
  ua->send->ObjectKeyValue("refresh_thread_state", thread_state);
  ua->send->ObjectKeyValue("pending_jobs", static_cast<uint64_t>(pending_jobs));
  if (metadata.full_duration_ms) {
    ua->send->ObjectKeyValue("last_full_duration_ms",
                             *metadata.full_duration_ms);
    ua->send->ObjectKeyValue("last_full_completed_at",
                             metadata.full_completed_at.c_str());
    ua->SendMsg(T_("Last successful full recalculation: %llu ms (%s).\n"),
                static_cast<unsigned long long>(*metadata.full_duration_ms),
                metadata.full_completed_at.c_str());
  }
  if (!metadata.last_error.empty()) {
    ua->send->ObjectKeyValue("last_refresh_error", metadata.last_error.c_str());
  }
  if (!anonymize && metadata.last_error_diagnostic) {
    const auto& diagnostic = *metadata.last_error_diagnostic;
    ua->send->ObjectStart("last_refresh_diagnostic");
    ua->send->ObjectKeyValue("version", 1);
    ua->send->ObjectKeyValue("type", diagnostic.type.c_str());
    ua->send->ObjectKeyValue("jobid", diagnostic.jobid);
    ua->send->ObjectKeyValue("fileindex", diagnostic.fileindex);
    ua->send->ObjectKeyValue("path", diagnostic.path.c_str());
    ua->send->ObjectKeyValue("client", diagnostic.client.c_str());
    ua->send->ObjectKeyValue("fileset", diagnostic.fileset.c_str());
    ua->send->ObjectKeyValue("reason", diagnostic.reason.c_str());
    ua->send->ObjectEnd();
  }
  ua->send->ObjectEnd();

  if (!metadata.has_snapshot) {
    ua->SendMsg(T_("Subscription accounting has not been calculated yet.\n"));
    if (!metadata.last_error.empty()) {
      ua->SendMsg(T_("Last refresh failed: %s\n"), metadata.last_error.c_str());
    }
    return true;
  }

  ua->SendMsg(T_("\nReal (File.LStat-based) subscription accounting report "
                 "from snapshot at %s:\n"),
              metadata.timestamp.c_str());
  if (metadata.stale) {
    ua->WarningMsg(T_("WARNING: this snapshot is more than 24 hours old.\n"));
  }
  if (!metadata.last_error.empty()) {
    ua->WarningMsg(T_("WARNING: the latest refresh failed; showing the last "
                      "successful snapshot: %s\n"),
                   metadata.last_error.c_str());
  }
  if (rows.empty()) {
    ua->SendMsg(T_("No matching Client/FileSet combinations found.\n"));
    return true;
  }

  uint64_t grand_total_bytes = 0;
  uint64_t grand_total_logical_bytes = 0;
  uint64_t grand_total_files = 0;
  uint32_t accounted_tuples = 0;
  uint32_t excluded_tuples = 0;
  std::vector<SubscriptionAccountingTableRow> table_rows;
  const char* job_size_explanation = T_(
      "Estimated from the baseline Full job: used the larger of bytes "
      "read and bytes stored because per-file sizes are unavailable.");

  // Structured (.api 2 / WebUI) output, in addition to the plain-text
  // report above. ObjectKeyValue()/ArrayStart()/... calls are no-ops for
  // plain-text consumers when no format string is given, so this does not
  // change the human-readable output produced by ua->SendMsg() above.
  ua->send->ArrayStart("accounting");

  for (const AccountingRow& row : rows) {
    if (row.excluded) {
      const char* reason
          = row.exclusion_reason == "ndmp_no_file_history"
                ? T_("Size unavailable: NDMP has neither file "
                     "history nor a usable Full-job byte count.")
            : row.exclusion_reason == "opaque_backup_image"
                ? T_("Size unavailable: per-file sizes and a usable "
                     "baseline Full-job byte count are both missing.")
            : row.exclusion_reason == "no_usable_chain"
                ? T_("Excluded: no usable backup chain starting "
                     "with a Full backup could be found; required "
                     "catalog records may have been purged.")
                : T_("Excluded: unrecognized accounting issue.");
      std::string issue = reason;
      std::string issue_before_jobid;
      std::string issue_after_jobid;
      if (row.baseline_full_jobid != 0) {
        const char* linked_reason
            = row.exclusion_reason == "opaque_backup_image"
                  ? T_("Size unavailable: per-file sizes and a usable baseline "
                       "Full-job (id {jobid}) byte count are both missing.")
              : row.exclusion_reason == "ndmp_no_file_history"
                  ? T_("Size unavailable: NDMP has neither file history nor a "
                       "usable baseline Full-job (id {jobid}) byte count.")
                  : nullptr;
        if (linked_reason) {
          std::string message = linked_reason;
          auto offset = message.find("{jobid}");
          if (offset != std::string::npos) {
            issue_before_jobid = message.substr(0, offset);
            issue_after_jobid = message.substr(offset + 7);
            issue = issue_before_jobid + std::to_string(row.baseline_full_jobid)
                    + issue_after_jobid;
          }
        }
      }
      table_rows.push_back({row.FileSetName + "@" + row.ClientName, "-", "-",
                            "-", "-", "-", issue});
      ua->send->ObjectStart();
      ua->send->ObjectKeyValue("client", row.ClientName.c_str());
      ua->send->ObjectKeyValue("fileset", row.FileSetName.c_str());
      ua->send->ObjectKeyValueBool("excluded", true);
      ua->send->ObjectKeyValue("accounting_issue", issue.c_str());
      if (!issue_before_jobid.empty()) {
        ua->send->ObjectKeyValue("accounting_issue_before_jobid",
                                 issue_before_jobid.c_str());
        ua->send->ObjectKeyValue("accounting_issue_after_jobid",
                                 issue_after_jobid.c_str());
      }
      if (row.baseline_full_jobid != 0) {
        ua->send->ObjectKeyValue("baseline_full_jobid",
                                 row.baseline_full_jobid);
      }
      if (!row.exclusion_reason.empty()) {
        ua->send->ObjectKeyValue("exclusion_reason",
                                 row.exclusion_reason.c_str());
      }
      ua->send->ObjectEnd();
      excluded_tuples++;
      continue;
    }

    char files[50], bytes[50], logical[50], jobs[50];
    table_rows.push_back(
        {row.FileSetName + "@" + row.ClientName,
         edit_uint64_with_commas(row.files, files),
         std::string(edit_uint64_with_suffix(row.bytes, bytes)) + "B",
         std::string(edit_uint64_with_suffix(row.logical_bytes, logical)) + "B",
         SubscriptionAccountingRuleCode(row.rule),
         edit_uint64_with_commas(row.jobs_in_chain, jobs), ""});

    ua->send->ObjectStart();
    ua->send->ObjectKeyValue("client", row.ClientName.c_str());
    ua->send->ObjectKeyValue("fileset", row.FileSetName.c_str());
    ua->send->ObjectKeyValueBool("excluded", false);
    ua->send->ObjectKeyValueBool("estimated", row.rule == "job_bytes");
    ua->send->ObjectKeyValue("files", row.files);
    ua->send->ObjectKeyValue("bytes", row.bytes);
    ua->send->ObjectKeyValue("logical_bytes", row.logical_bytes);
    ua->send->ObjectKeyValue("rule", row.rule.c_str());
    ua->send->ObjectKeyValue("rule_code",
                             SubscriptionAccountingRuleCode(row.rule).c_str());
    ua->send->ObjectKeyValue("rule_explanation", row.rule == "job_bytes"
                                                     ? job_size_explanation
                                                     : "");
    ua->send->ObjectKeyValue("jobs_in_chain", row.jobs_in_chain);
    ua->send->ObjectEnd();

    if (row.bytes > std::numeric_limits<uint64_t>::max() - grand_total_bytes) {
      ua->ErrorMsg(T_("Grand total byte count overflow -- aborting report.\n"));
      ua->send->ArrayEnd("accounting");
      return false;
    }
    if (row.logical_bytes
        > std::numeric_limits<uint64_t>::max() - grand_total_logical_bytes) {
      ua->ErrorMsg(
          T_("Grand total logical byte count overflow -- aborting report.\n"));
      ua->send->ArrayEnd("accounting");
      return false;
    }
    if (row.files > std::numeric_limits<uint64_t>::max() - grand_total_files) {
      ua->ErrorMsg(T_("Grand total file count overflow -- aborting report.\n"));
      ua->send->ArrayEnd("accounting");
      return false;
    }
    grand_total_bytes += row.bytes;
    grand_total_logical_bytes += row.logical_bytes;
    grand_total_files += row.files;
    accounted_tuples++;
  }

  ua->send->ArrayEnd("accounting");

  char total_files[50], total_bytes[50], total_logical[50];
  const bool show_logical = FindArg(ua, NT_("logical")) > 0;
  const auto table = FormatSubscriptionAccountingTable(
      {T_("FileSet@Client"), T_("Files"), T_("Accounted size"),
       T_("Logical size"), T_("Rule"), T_("Chain jobs"),
       T_("Accounting issues")},
      table_rows,
      {T_("TOTAL"), edit_uint64_with_commas(grand_total_files, total_files),
       std::string(edit_uint64_with_suffix(grand_total_bytes, total_bytes))
           + "B",
       std::string(
           edit_uint64_with_suffix(grand_total_logical_bytes, total_logical))
           + "B",
       "", "", ""},
      excluded_tuples > 0, show_logical);
  ua->SendMsg("%s", table.c_str());
  const std::string rule_legend
      = std::string(
            T_("\nRule:\n"
               "  A = allocated size (st_blocks * 512 bytes)\n"
               "  L = logical size (st_size)\n"
               "  J = job size of the latest Full (estimated; opaque plugin "
               "image or NDMP without file history)\n"
               "Size units are decimal (1 KB = 1,000 bytes).\n"))
        + job_size_explanation;
  ua->SendMsg("%s\n", rule_legend.c_str());
  ua->send->ObjectKeyValue("accounting_rule_legend", rule_legend.c_str());

  char ec1[50], ec2[50], human_bytes[50], human_logical[50];
  ua->SendMsg(T_("\nGrand total: %s files, %sB (%s bytes) across %u accounted "
                 "Client/FileSet combination(s)"),
              edit_uint64_with_commas(grand_total_files, ec1),
              edit_uint64_with_suffix(grand_total_bytes, human_bytes),
              edit_uint64_with_commas(grand_total_bytes, ec2),
              accounted_tuples);
  if (excluded_tuples > 0) {
    ua->SendMsg(T_(" (%u excluded for lack of file information; totals are "
                   "INCOMPLETE)"),
                excluded_tuples);
  }
  ua->SendMsg("\n");
  if (show_logical) {
    ua->SendMsg(
        T_("Logical size total (st_size): %sB (%s bytes)\n"),
        edit_uint64_with_suffix(grand_total_logical_bytes, human_logical),
        edit_uint64_with_commas(grand_total_logical_bytes, ec1));
  }

  ua->send->ObjectStart("summary");
  ua->send->ObjectKeyValue("total_files", grand_total_files);
  ua->send->ObjectKeyValue("total_bytes", grand_total_bytes);
  ua->send->ObjectKeyValue("total_logical_bytes", grand_total_logical_bytes);
  ua->send->ObjectKeyValue("accounted_tuples",
                           static_cast<uint64_t>(accounted_tuples));
  ua->send->ObjectKeyValue("excluded_tuples",
                           static_cast<uint64_t>(excluded_tuples));
  ua->send->ObjectKeyValueBool("incomplete", excluded_tuples > 0);
  PoolMem warning;
  if (excluded_tuples > 0) {
    Mmsg(warning,
         T_("Accounting incomplete: size unavailable for %u Client/FileSet "
            "combination(s). Totals exclude these entries."),
         excluded_tuples);
    ua->SendMsg("%s\n", warning.c_str());
  }
  ua->send->ObjectKeyValue("warning",
                           excluded_tuples > 0 ? warning.c_str() : "");
  ua->send->ObjectEnd("summary");

  return true;
}

bool ReportSubscriptionAccountingStatus(UaContext* ua)
{
  const auto status = GetSubscriptionAccountingThreadStatus();
  const char* thread_state = SubscriptionAccountingThreadState(status);

  ua->send->ObjectStart("accounting_status");
  ua->send->ObjectKeyValue("generation", status.generation);
  ua->send->ObjectKeyValueBool("worker_available", status.available);
  ua->send->ObjectKeyValueBool("queued", status.queued);
  ua->send->ObjectKeyValueBool("running", status.running);
  ua->send->ObjectKeyValue("refresh_thread_state", thread_state);
  ua->send->ObjectEnd();

  ua->SendMsg(T_("Subscription accounting generation: %llu; worker: %s.\n"),
              static_cast<unsigned long long>(status.generation), thread_state);
  return true;
}

bool StartSubscriptionAccountingThread()
{
  pthread_mutex_lock(&accounting_mutex);
  if (accounting_thread_initialized) {
    pthread_mutex_unlock(&accounting_mutex);
    return true;
  }
  accounting_quit = false;
  pthread_mutex_unlock(&accounting_mutex);
  int status = pthread_create(&accounting_thread_id, nullptr,
                              SubscriptionAccountingThread, nullptr);
  if (status != 0) {
    BErrNo be;
    Jmsg(nullptr, M_ERROR, 0,
         T_("Subscription accounting thread could not be started: %s\n"),
         be.bstrerror());
    return false;
  }
  pthread_mutex_lock(&accounting_mutex);
  accounting_thread_initialized = true;
  pthread_mutex_unlock(&accounting_mutex);
  return true;
}

void StopSubscriptionAccountingThread()
{
  pthread_mutex_lock(&accounting_mutex);
  if (!accounting_thread_initialized) {
    pthread_mutex_unlock(&accounting_mutex);
    return;
  }
  accounting_quit = true;
  pthread_cond_broadcast(&accounting_cond);
  pthread_mutex_unlock(&accounting_mutex);
  if (!pthread_equal(accounting_thread_id, pthread_self())) {
    pthread_join(accounting_thread_id, nullptr);
  }
  pthread_mutex_lock(&accounting_mutex);
  accounting_refresh_running = false;
  accounting_job_running = false;
  accounting_worker_available = false;
  accounting_thread_initialized = false;
  pthread_mutex_unlock(&accounting_mutex);
}

bool QueueSubscriptionAccountingJob(JobId_t jobid)
{
  if (jobid == 0) { return false; }
  pthread_mutex_lock(&accounting_mutex);
  if (accounting_quit.load()) {
    pthread_mutex_unlock(&accounting_mutex);
    return false;
  }
  const bool inserted = accounting_jobs.insert(jobid).second;
  if (inserted) {
    ++accounting_generation;
    pthread_cond_signal(&accounting_cond);
  }
  pthread_mutex_unlock(&accounting_mutex);
  return inserted;
}

size_t QueueMissingSubscriptionAccounting(UaContext* ua,
                                          const char* client_filter,
                                          const char* fileset_filter)
{
  PoolMem query(PM_MESSAGE);
  PmStrcpy(query,
           "WITH latest_full AS ("
           " SELECT DISTINCT ON (Job.ClientId, FileSet.FileSet) "
           "Job.ClientId, FileSet.FileSet, Job.FileSetId FROM Job "
           "JOIN FileSet USING (FileSetId) "
           "WHERE Job.JobStatus IN ('T','W') AND Job.Type IN ('B','C') "
           "AND Job.Level='F' "
           "ORDER BY Job.ClientId, FileSet.FileSet, Job.JobTDate DESC, "
           "Job.JobId DESC) "
           "SELECT JobId FROM ("
           " SELECT DISTINCT ON (Job.ClientId, FileSet.FileSet) "
           "Job.JobId, Job.AccountingValid FROM Job "
           "JOIN Client USING (ClientId) JOIN FileSet USING (FileSetId) "
           "JOIN latest_full ON latest_full.ClientId=Job.ClientId "
           "AND latest_full.FileSet=FileSet.FileSet "
           "AND latest_full.FileSetId=Job.FileSetId "
           "WHERE Job.JobStatus IN ('T','W') AND Job.Type IN ('B','C') "
           "AND Job.Level IN ('F','D','I')");
  std::string escaped;
  if (client_filter) {
    auto value = ua->db->EscapeString(ua->jcr, client_filter);
    if (!value) {
      ua->ErrorMsg(T_("Could not escape subscription client name.\n"));
      return 0;
    }
    escaped = *value;
    PmStrcat(query, " AND Client.Name='");
    PmStrcat(query, escaped.c_str());
    PmStrcat(query, "'");
  }
  if (fileset_filter) {
    auto value = ua->db->EscapeString(ua->jcr, fileset_filter);
    if (!value) {
      ua->ErrorMsg(T_("Could not escape subscription fileset name.\n"));
      return 0;
    }
    escaped = *value;
    PmStrcat(query, " AND FileSet.FileSet='");
    PmStrcat(query, escaped.c_str());
    PmStrcat(query, "'");
  }
  PmStrcat(query,
           " ORDER BY Job.ClientId, FileSet.FileSet, Job.JobTDate DESC, "
           "Job.JobId DESC) latest WHERE NOT latest.AccountingValid");

  MissingAccountingJobsCtx missing;
  if (!ua->db->SqlQuery(query.c_str(), MissingAccountingJobHandler, &missing)) {
    ua->ErrorMsg("%s\n", ua->db->strerror());
    return 0;
  }

  pthread_mutex_lock(&accounting_mutex);
  size_t inserted = 0;
  if (!accounting_quit.load()) {
    for (JobId_t jobid : missing.jobids) {
      inserted += accounting_jobs.insert(jobid).second ? 1 : 0;
    }
    if (inserted > 0) {
      ++accounting_generation;
      pthread_cond_signal(&accounting_cond);
    }
  }
  pthread_mutex_unlock(&accounting_mutex);
  return missing.jobids.size();
}

bool RequestSubscriptionAccountingRefresh(UaContext* ua)
{
  pthread_mutex_lock(&accounting_mutex);
  if (!accounting_worker_available || accounting_quit.load()) {
    pthread_mutex_unlock(&accounting_mutex);
    ua->ErrorMsg(
        T_("Subscription accounting background worker is not "
           "available.\n"));
    return false;
  }
  if (accounting_refresh_running || accounting_refresh_requested) {
    pthread_mutex_unlock(&accounting_mutex);
    ua->SendMsg(
        T_("A subscription accounting refresh is already running or "
           "queued.\n"));
    return true;
  }
  accounting_refresh_requested = true;
  ++accounting_generation;
  pthread_cond_signal(&accounting_cond);
  pthread_mutex_unlock(&accounting_mutex);
  ua->SendMsg(
      T_("Subscription accounting refresh requested in the "
         "background.\n"));
  return true;
}

SubscriptionAccountingThreadStatus GetSubscriptionAccountingThreadStatus()
{
  pthread_mutex_lock(&accounting_mutex);
  SubscriptionAccountingThreadStatus status;
  status.available = accounting_worker_available;
  status.queued = accounting_refresh_requested || !accounting_jobs.empty();
  status.running = accounting_refresh_running || accounting_job_running;
  status.generation = accounting_generation;
  pthread_mutex_unlock(&accounting_mutex);
  return status;
}

} /* namespace directordaemon */
