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
  return 0;
}

int SnapshotRowHandler(void* ctx, int, char** row)
{
  auto* rows = static_cast<std::vector<AccountingRow>*>(ctx);
  AccountingRow result;
  result.ClientName = row[0] ? row[0] : "";
  result.FileSetName = row[1] ? row[1] : "";
  result.excluded = row[2] && bstrcmp(row[2], "t");
  result.exclusion_reason = row[3] ? row[3] : "";
  result.files = row[4] ? str_to_uint64(row[4]) : 0;
  result.bytes = row[5] ? str_to_uint64(row[5]) : 0;
  result.logical_bytes = row[6] ? str_to_uint64(row[6]) : 0;
  result.rule = row[7] ? row[7] : "";
  result.jobs_in_chain = row[8] ? str_to_uint64(row[8]) : 0;
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
                                        std::vector<AccountingRow>* rows)
{
  if (!ua->db->SqlQuery(
          "SELECT COALESCE(to_char(LastSuccess, 'YYYY-MM-DD HH24:MI:SS'), "
          "''), COALESCE(LastError, ''), "
          "(LastSuccess IS NULL OR LastSuccess < CURRENT_TIMESTAMP - "
          "INTERVAL '24 hours')::text "
          "FROM SubscriptionAccountingSnapshot WHERE SnapshotId=1",
          SnapshotMetadataHandler, metadata)) {
    ua->ErrorMsg("%s\n", ua->db->strerror());
    return false;
  }

  PoolMem query(PM_MESSAGE);
  PmStrcpy(query,
           "SELECT ClientName, FileSetName, Excluded, "
           "COALESCE(ExclusionReason, ''), Files::text, Bytes::text, "
           "LogicalBytes::text, COALESCE(Rule, ''), JobsInChain::text "
           "FROM SubscriptionAccounting");
  if (client_filter || fileset_filter) { PmStrcat(query, " WHERE "); }
  bool needs_and = false;
  std::string escaped;
  if (client_filter) {
    if (!EscapeForSql(ua, client_filter, &escaped)) { return false; }
    PmStrcat(query, "ClientName='");
    PmStrcat(query, escaped.c_str());
    PmStrcat(query, "'");
    needs_and = true;
  }
  if (fileset_filter) {
    if (needs_and) { PmStrcat(query, " AND "); }
    if (!EscapeForSql(ua, fileset_filter, &escaped)) { return false; }
    PmStrcat(query, "FileSetName='");
    PmStrcat(query, escaped.c_str());
    PmStrcat(query, "'");
  }
  PmStrcat(query, " ORDER BY ClientName, FileSetName");

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
  bool success = ua->db->SqlExec("DELETE FROM SubscriptionAccounting");
  for (const auto& row : rows) {
    std::string client_name, fileset_name, reason, rule;
    if (!success || !EscapeForSql(ua, row.ClientName, &client_name)
        || !EscapeForSql(ua, row.FileSetName, &fileset_name)
        || !EscapeForSql(ua, row.exclusion_reason, &reason)
        || !EscapeForSql(ua, row.rule, &rule)) {
      success = false;
      break;
    }
    PoolMem query(PM_MESSAGE);
    Mmsg(query,
         "INSERT INTO SubscriptionAccounting "
         "(ClientName, FileSetName, Excluded, ExclusionReason, Files, Bytes, "
         "LogicalBytes, Rule, JobsInChain) VALUES "
         "('%s', '%s', %s, %s, %llu, %llu, %llu, %s, %llu)",
         client_name.c_str(), fileset_name.c_str(),
         row.excluded ? "true" : "false",
         row.exclusion_reason.empty() ? "NULL" : ("'" + reason + "'").c_str(),
         static_cast<unsigned long long>(row.files),
         static_cast<unsigned long long>(row.bytes),
         static_cast<unsigned long long>(row.logical_bytes),
         row.rule.empty() ? "NULL" : ("'" + rule + "'").c_str(),
         static_cast<unsigned long long>(row.jobs_in_chain));
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
