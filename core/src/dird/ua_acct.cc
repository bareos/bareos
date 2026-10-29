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
 * 'status subscriptions accounting [client=<name>] [fileset=<name>]' and the
 * background thread refreshing the snapshot it displays (see
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
#include <limits>
#include <pthread.h>
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

void RefreshSubscriptionAccounting(UaContext* ua)
{
  std::vector<AccountingRow> rows;
  std::string error;
  if (!CalculateSubscriptionAccounting(ua, &rows, &error, &accounting_quit)) {
    if (accounting_quit.load()) { return; }
    if (!RecordSubscriptionAccountingFailure(ua, error)) {
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
  Jmsg(ua->jcr, M_INFO, 0,
       T_("Subscription accounting snapshot refreshed with %zu "
          "Client/FileSet combination(s).\n"),
       rows.size());
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
    while (!accounting_quit.load()) {
      pthread_mutex_lock(&accounting_mutex);
      while (!accounting_quit.load() && !accounting_refresh_requested) {
        pthread_cond_wait(&accounting_cond, &accounting_mutex);
      }
      const bool refresh_due
          = !accounting_quit.load() && accounting_refresh_requested;
      if (refresh_due) {
        accounting_refresh_requested = false;
        accounting_refresh_running = true;
      }
      pthread_mutex_unlock(&accounting_mutex);
      if (accounting_quit.load()) { break; }
      if (refresh_due) {
        RefreshSubscriptionAccounting(&ua);
        pthread_mutex_lock(&accounting_mutex);
        accounting_refresh_running = false;
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

bool DoSubscriptionAccounting(UaContext* ua)
{
  if (ua->AclHasRestrictions(Client_ACL) || ua->AclHasRestrictions(Job_ACL)
      || ua->AclHasRestrictions(FileSet_ACL)) {
    ua->ErrorMsg(T_("%s %s: needs access to all client, job"
                    " and fileset resources.\n"),
                 ua->argk[0], ua->argk[1]);
    return false;
  }
  if (!OpenDb(ua)) {
    ua->ErrorMsg("Failed to open db.\n");
    return false;
  }
  auto config = my_config->GetCurrentConfiguration();
  auto* first_catalog
      = static_cast<CatalogResource*>(config->GetNextRes(R_CATALOG, nullptr));
  if (first_catalog && config->GetNextRes(R_CATALOG, first_catalog)) {
    ua->WarningMsg(
        T_("Subscription accounting does not support multiple catalogs; "
           "only the first configured catalog is refreshed. This report "
           "reads the selected catalog and may be empty or stale.\n"));
  }

  const char* client_filter = GetArgValue(ua, NT_("client"));
  const char* fileset_filter = GetArgValue(ua, NT_("fileset"));

  if (client_filter && !IsNameValid(client_filter)) {
    ua->ErrorMsg(T_("Invalid client name: %s\n"), client_filter);
    return false;
  }
  if (fileset_filter && !IsNameValid(fileset_filter)) {
    ua->ErrorMsg(T_("Invalid fileset name: %s\n"), fileset_filter);
    return false;
  }

  std::vector<AccountingRow> rows;
  SnapshotMetadata metadata{};
  std::string error;
  if (!LoadSubscriptionAccountingSnapshot(ua, client_filter, fileset_filter,
                                          &metadata, &rows)) {
    return false;
  }
  const auto thread_status = GetSubscriptionAccountingThreadStatus();
  const char* thread_state = !thread_status.available ? T_("unavailable")
                             : thread_status.running  ? T_("running")
                             : thread_status.queued   ? T_("queued")
                                                      : T_("idle");
  ua->SendMsg(T_("Background refresh worker: %s.\n"), thread_state);

  ua->send->ObjectStart("accounting_snapshot");
  ua->send->ObjectKeyValueBool("available", metadata.has_snapshot);
  ua->send->ObjectKeyValue("calculated_at", metadata.timestamp.c_str());
  ua->send->ObjectKeyValueBool("stale", metadata.stale);
  ua->send->ObjectKeyValue("refresh_thread_state", thread_state);
  if (!metadata.last_error.empty()) {
    ua->send->ObjectKeyValue("last_refresh_error", metadata.last_error.c_str());
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

  // Structured (.api 2 / WebUI) output, in addition to the plain-text
  // report above. ObjectKeyValue()/ArrayStart()/... calls are no-ops for
  // plain-text consumers when no format string is given, so this does not
  // change the human-readable output produced by ua->SendMsg() above.
  ua->send->ArrayStart("accounting");

  for (const AccountingRow& row : rows) {
    if (row.excluded) {
      const char* reason = row.exclusion_reason == "ndmp_no_file_history"
                               ? T_("no per-file data available (NDMP file "
                                    "history may be disabled)")
                           : row.exclusion_reason == "no_per_file_data"
                               ? T_("no measurable per-file data available")
                           : row.exclusion_reason == "opaque_backup_image"
                               ? T_("opaque plugin image without a job size")
                               : T_("no usable backup chain found");
      table_rows.push_back({row.FileSetName + "@" + row.ClientName, "-", "-",
                            "-", "-", "-",
                            std::string(T_("Excluded: ")) + reason});
      ua->send->ObjectStart();
      ua->send->ObjectKeyValue("client", row.ClientName.c_str());
      ua->send->ObjectKeyValue("fileset", row.FileSetName.c_str());
      ua->send->ObjectKeyValueBool("excluded", true);
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
  const auto table = FormatSubscriptionAccountingTable(
      {T_("FileSet@Client"), T_("Files"), T_("Accounted size"),
       T_("Logical size"), T_("Rule"), T_("Chain jobs"), T_("Status / Reason")},
      table_rows,
      {T_("TOTAL"), edit_uint64_with_commas(grand_total_files, total_files),
       std::string(edit_uint64_with_suffix(grand_total_bytes, total_bytes))
           + "B",
       std::string(
           edit_uint64_with_suffix(grand_total_logical_bytes, total_logical))
           + "B",
       "", "", ""},
      excluded_tuples > 0);
  ua->SendMsg("%s", table.c_str());
  ua->SendMsg(
      T_("\nRule:\n"
         "  B = allocated size (st_blocks * 512 bytes)\n"
         "  S = logical size (st_size)\n"
         "  J = job size of the latest Full (estimated; opaque plugin "
         "image)\n"
         "Size units are decimal (1 KB = 1,000 bytes).\n"));

  char ec1[50], ec2[50];
  ua->SendMsg(T_("\nGrand total: %s files, %s bytes across %u accounted "
                 "Client/FileSet combination(s)"),
              edit_uint64_with_commas(grand_total_files, ec1),
              edit_uint64_with_commas(grand_total_bytes, ec2),
              accounted_tuples);
  if (excluded_tuples > 0) {
    ua->SendMsg(T_(" (%u excluded for lack of file information; totals are "
                   "INCOMPLETE)"),
                excluded_tuples);
  }
  ua->SendMsg(T_("\nLogical size total (st_size): %s bytes\n"),
              edit_uint64_with_commas(grand_total_logical_bytes, ec1));

  ua->send->ObjectStart("summary");
  ua->send->ObjectKeyValue("total_files", grand_total_files);
  ua->send->ObjectKeyValue("total_bytes", grand_total_bytes);
  ua->send->ObjectKeyValue("total_logical_bytes", grand_total_logical_bytes);
  ua->send->ObjectKeyValue("accounted_tuples",
                           static_cast<uint64_t>(accounted_tuples));
  ua->send->ObjectKeyValue("excluded_tuples",
                           static_cast<uint64_t>(excluded_tuples));
  ua->send->ObjectKeyValueBool("incomplete", excluded_tuples > 0);
  ua->send->ObjectEnd("summary");

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
  accounting_worker_available = false;
  accounting_thread_initialized = false;
  pthread_mutex_unlock(&accounting_mutex);
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
  status.queued = accounting_refresh_requested;
  status.running = accounting_refresh_running;
  pthread_mutex_unlock(&accounting_mutex);
  return status;
}

} /* namespace directordaemon */
