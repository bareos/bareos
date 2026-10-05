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
 * Real, File.LStat-based subscription/accounting size totals -- refreshed
 * in the background per (Client, FileSet) tuple from actual catalog File
 * rows rather than the guessed numbers used by the rest of 'status
 * subscriptions'. Implements 'status subscriptions accounting
 * [client=<name>] [fileset=<name>]'.
 */

#include "include/bareos.h"
#include "dird.h"
#include "dird/dird_globals.h"
#include "dird/ua_db.h"
#include "dird/ua_select.h"
#include "dird/ua_acct.h"
#include "dird/subscription_accounting_table.h"
#include "lib/attribs.h"
#include "lib/edit.h"
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

/* Job Types that can carry real File attribute rows usable for accounting:
 *   JT_BACKUP       ('B') - ordinary/Virtual Full backups
 *   JT_JOB_COPY     ('C') - the resulting job of a Copy
 *   JT_MIGRATE      ('g') - the resulting job of a Migrate
 *   JT_CONSOLIDATE  ('O') - the resulting job of an Always-Incremental
 *                           consolidation
 * Explicitly excluded: JT_MIGRATED_JOB ('M') -- the superseded original
 * job after a migrate, whose File rows are normally purged and which is
 * no longer the current on-disk representation of this Client/FileSet. */
constexpr const char* kAccountableJobTypes = "'B','C','g','O'";

/**
 * Per-platform accounting rule (see plan.md for the full rationale):
 *  - Windows clients (Client.Uname contains "Windows"): use st_size
 *    for both totals -- Windows st_blocks is synthetic.
 *  - Everyone else: use st_blocks * 512 for allocated bytes and st_size
 *    for logical bytes.
 */
template <typename T> constexpr bool IsUnknownStatField(T value)
{
  if constexpr (std::numeric_limits<T>::is_signed) {
    return value < 0;
  } else {
    return value == std::numeric_limits<T>::max();
  }
}

static_assert(IsUnknownStatField(int64_t{-1}));
static_assert(!IsUnknownStatField(int64_t{0}));
static_assert(IsUnknownStatField(std::numeric_limits<uint64_t>::max()));
static_assert(!IsUnknownStatField(uint64_t{0}));

/**
 * kRegular      -- a countable file; both byte counts are meaningful.
 * kNonRegular   -- a directory, symlink or special file (FIFO/device/
 *                  socket) catalog row -- not billable subscription
 *                  data, silently excluded from the report (not an
 *                  error).
 * kVirtualNdmpArchive -- the synthetic whole-stream NDMP file, which has
 *                  placeholder attributes and is not a user file.
 * kInvalidStat  -- the stat field the platform's rule needs is unusable
 *                  (unknown/overflowing); the caller must abort the
 *                  report rather than under-report silently.
 */
enum class FileAccountingKind
{
  kRegular,
  kNonRegular,
  kVirtualNdmpArchive,
  kInvalidStat
};

struct FileAccountingResult {
  FileAccountingKind kind{FileAccountingKind::kInvalidStat};
  uint64_t bytes{0};
  uint64_t logical_bytes{0};
  struct stat statp{};
  const char* invalid_reason{nullptr};
};

FileAccountingResult AccountedBytesForFile(const std::string& lstat,
                                           bool is_windows)
{
  if (lstat.empty()) { return {FileAccountingKind::kRegular, 0}; }

  struct stat statp{};
  int32_t LinkFI;
  /* DecodeStat() takes a non-const char* but does not modify the
   * underlying data. */
  std::string mutable_lstat = lstat;
  DecodeStat(mutable_lstat.data(), &statp, sizeof(statp), &LinkFI);

  /* Only regular files count toward the report -- directories, symlinks
   * and special files (FIFOs, device/socket nodes) are catalog metadata,
   * not billable subscription data. A hard-linked file's FT_LNKSAVED row
   * still encodes the underlying regular file's stat (see
   * filed/backup.cc), so it is counted like any other regular file, once
   * per catalog name. */
  if (!S_ISREG(statp.st_mode)) {
    return {FileAccountingKind::kNonRegular, 0, 0};
  }

  /* NDMP and barri share this synthetic stat. The caller distinguishes
   * NDMP containers with per-file history from opaque plugin images. */
  if ((statp.st_mode & 07777) == 0700 && statp.st_size == -1
      && statp.st_blksize == 4096 && statp.st_blocks == 1) {
    return {FileAccountingKind::kVirtualNdmpArchive, 0, 0};
  }

  if (IsUnknownStatField(statp.st_size)) {
    return {FileAccountingKind::kInvalidStat, 0, 0, statp,
            "st_size is unknown or negative"};
  }
  uint64_t logical_bytes = static_cast<uint64_t>(statp.st_size);

  if (is_windows) {
    return {FileAccountingKind::kRegular, logical_bytes, logical_bytes};
  }
  if (IsUnknownStatField(statp.st_blocks)) {
    return {FileAccountingKind::kInvalidStat, 0, 0, statp,
            "st_blocks is unknown or negative"};
  }
  uint64_t blocks = static_cast<uint64_t>(statp.st_blocks);
  if (blocks > std::numeric_limits<uint64_t>::max() / 512) {
    return {FileAccountingKind::kInvalidStat, 0, 0, statp,
            "st_blocks * 512 would overflow"};
  }
  return {FileAccountingKind::kRegular, blocks * 512, logical_bytes};
}

bool UnameLooksLikeWindows(const std::string& uname)
{
  return uname.find("Windows") != std::string::npos;
}

struct TupleInfo {
  DBId_t ClientId{0};
  std::string ClientName;
  std::string Uname;
  DBId_t FileSetId{0};
  std::string FileSetName;
};

struct AccountingRow {
  std::string ClientName;
  std::string FileSetName;
  bool excluded{false};
  std::string exclusion_reason;
  uint64_t files{0};
  uint64_t bytes{0};
  uint64_t logical_bytes{0};
  std::string rule;
  uint64_t jobs_in_chain{0};
};

struct ChainResolveCtx {
  JobId_t JobId{0};
  utime_t JobTDate{0};
  bool purged{false};
  bool found{false};
};

int ChainJobHandler(void* ctx, int, char** row)
{
  // row[0]=JobId row[1]=JobTDate row[2]=PurgedFiles
  auto* c = static_cast<ChainResolveCtx*>(ctx);
  c->JobId = static_cast<JobId_t>(str_to_int64(row[0]));
  c->JobTDate = static_cast<utime_t>(str_to_int64(row[1]));
  c->purged = row[2] && str_to_int64(row[2]) != 0;
  c->found = true;
  return 0;
}

struct JobIdListCtx {
  std::vector<JobId_t> jobids;
  bool any_purged{false};
};

int JobIdListHandler(void* ctx, int, char** row)
{
  // row[0]=JobId row[1]=PurgedFiles
  auto* c = static_cast<JobIdListCtx*>(ctx);
  c->jobids.push_back(static_cast<JobId_t>(str_to_int64(row[0])));
  if (row[1] && str_to_int64(row[1]) != 0) { c->any_purged = true; }
  return 0;
}

/**
 * Per (PathId, Name) de-duplication ("latest JobTDate, JobId tie-break,
 * wins") is done by the catalog query itself (PostgreSQL DISTINCT ON --
 * see ScanFilesForChain()), so this handler sees each catalog path
 * exactly once and can accumulate running totals directly. This avoids
 * holding an application-side map of every distinct file in the chain
 * in Director memory (see plan.md benchmark for the rationale).
 */
struct FileScanCtx {
  UaContext* ua{nullptr};
  const TupleInfo* tuple{nullptr};
  const std::atomic_bool* cancel{nullptr};
  bool is_windows{false};
  uint64_t bytes{0};
  uint64_t logical_bytes{0};
  uint64_t files{0};
  bool aborted{false};
  bool saw_virtual_ndmp_archive{false};
  bool saw_opaque_plugin_image{false};
};

int FileRowHandler(void* ctx, int, char** row)
{
  // row[0]=JobId row[1]=PathId row[2]=Name row[3]=FileIndex row[4]=LStat
  // row[5]=full catalog filename
  auto* c = static_cast<FileScanCtx*>(ctx);
  if (c->cancel && c->cancel->load()) {
    c->aborted = true;
    return 1;
  }
  auto JobId = row[0] ? row[0] : "";
  auto PathId = row[1] ? row[1] : "";
  auto Name = row[2] ? row[2] : "";
  auto FileIndex = static_cast<uint32_t>(str_to_int64(row[3]));

  /* FileIndex==0 marks an accurate-mode "this file was deleted since the
   * last backup" entry -- exclude it, don't count stale bytes. */
  if (FileIndex == 0) { return 0; }

  FileAccountingResult file_result
      = AccountedBytesForFile(row[4] ? row[4] : "", c->is_windows);
  if (file_result.kind == FileAccountingKind::kInvalidStat) {
    c->ua->ErrorMsg(
        T_("%s / %s: invalid file attributes for FileIndex=%u "
           "(JobId=%s PathId=%s Name='%s'): %s; decoded st_mode=%s "
           "st_size=%s st_blocks=%s; raw LStat='%s' -- aborting report.\n"),
        c->tuple->ClientName.c_str(), c->tuple->FileSetName.c_str(), FileIndex,
        JobId, PathId, Name, file_result.invalid_reason,
        std::to_string(file_result.statp.st_mode).c_str(),
        std::to_string(file_result.statp.st_size).c_str(),
        std::to_string(file_result.statp.st_blocks).c_str(),
        row[4] ? row[4] : "");
    c->aborted = true;
    return 1;
  }
  if (file_result.kind == FileAccountingKind::kNonRegular
      || file_result.kind == FileAccountingKind::kVirtualNdmpArchive) {
    /* Non-regular catalog metadata and the synthetic NDMP stream container
     * are not billable user files. */
    if (file_result.kind == FileAccountingKind::kVirtualNdmpArchive) {
      const std::string_view filename = row[5] ? row[5] : "";
      if (filename.starts_with("/@NDMP/")) {
        c->saw_virtual_ndmp_archive = true;
      } else {
        c->saw_opaque_plugin_image = true;
      }
    }
    return 0;
  }
  if (file_result.bytes > std::numeric_limits<uint64_t>::max() - c->bytes) {
    c->ua->ErrorMsg(T_("%s / %s: byte total overflow -- aborting report.\n"),
                    c->tuple->ClientName.c_str(),
                    c->tuple->FileSetName.c_str());
    c->aborted = true;
    return 1;
  }
  if (file_result.logical_bytes
      > std::numeric_limits<uint64_t>::max() - c->logical_bytes) {
    c->ua->ErrorMsg(
        T_("%s / %s: logical byte total overflow -- aborting report.\n"),
        c->tuple->ClientName.c_str(), c->tuple->FileSetName.c_str());
    c->aborted = true;
    return 1;
  }
  c->bytes += file_result.bytes;
  c->logical_bytes += file_result.logical_bytes;
  c->files++;
  return 0;
}

struct TupleListCtx {
  std::vector<TupleInfo> tuples;
};

int TupleRowHandler(void* ctx, int, char** row)
{
  // row[0]=ClientId row[1]=Client.Name row[2]=Client.Uname
  // row[3]=FileSetId row[4]=FileSet.FileSet
  auto* c = static_cast<TupleListCtx*>(ctx);
  TupleInfo t;
  t.ClientId = static_cast<DBId_t>(str_to_int64(row[0]));
  t.ClientName = row[1] ? row[1] : "";
  t.Uname = row[2] ? row[2] : "";
  t.FileSetId = static_cast<DBId_t>(str_to_int64(row[3]));
  t.FileSetName = row[4] ? row[4] : "";
  c->tuples.push_back(std::move(t));
  return 0;
}

/**
 * Distinguishes "nothing to account for this tuple" (a normal, expected
 * outcome -- the tuple is excluded, not guessed) from "a catalog query
 * failed" (an actual error -- the report must not silently continue and
 * report an incomplete grand total as if it were complete).
 */
enum class ChainResult
{
  kFound,
  kNotFound,
  kError
};

/**
 * Resolve the latest backup chain (Full [+ Differential] + subsequent
 * Incrementals) for one (Client, FileSet) tuple -- same "latest wins"
 * semantics as the interactive restore-chain resolution in ua_restore.cc,
 * but without any JobMedia/Media join (accounting only cares whether File
 * rows exist, not whether the backing media is still available).
 *
 * Returns kNotFound if no Full backup could be found at all (nothing to
 * account for this tuple -- excluded, not guessed). Returns kError if any
 * of the catalog queries themselves failed -- callers must treat this
 * differently from kNotFound and abort the report instead of silently
 * excluding the tuple.
 */
ChainResult ResolveAccountingChain(UaContext* ua,
                                   DBId_t client_id,
                                   DBId_t fileset_id,
                                   std::vector<JobId_t>* jobids)
{
  PoolMem query(PM_MESSAGE);
  char ed1[50], ed2[50], ed3[50], ed4[50];

  // 1. Latest Full backup for this Client/FileSet.
  ChainResolveCtx full{};
  Mmsg(query,
       "SELECT JobId, JobTDate, PurgedFiles FROM Job"
       " WHERE ClientId=%s AND FileSetId=%s AND Level='F'"
       " AND JobStatus IN ('T','W') AND Type IN (%s)"
       " ORDER BY JobTDate DESC, JobId DESC LIMIT 1",
       edit_int64(client_id, ed1), edit_int64(fileset_id, ed2),
       kAccountableJobTypes);
  if (!ua->db->SqlQuery(query.c_str(), ChainJobHandler, &full)) {
    ua->ErrorMsg("%s\n", ua->db->strerror());
    return ChainResult::kError;
  }
  if (!full.found) { return ChainResult::kNotFound; /* no baseline */ }
  // File retention usually expires before Job retention, so the Full's
  // Job row can survive with its File rows already purged. Counting
  // only the later Incrementals in that case would silently under-
  // report instead of excluding the tuple as documented -- treat a
  // purged baseline the same as "no usable chain found".
  if (full.purged) { return ChainResult::kNotFound; /* File rows pruned */ }

  jobids->push_back(full.JobId);
  utime_t baseline = full.JobTDate;
  JobId_t baseline_jobid = full.JobId;

  // 2. Most recent Differential after the Full, if any -- moves the
  // baseline forward so only Incrementals after it are collected.
  ChainResolveCtx diff{};
  edit_uint64(baseline, ed3);
  edit_int64(baseline_jobid, ed4);
  Mmsg(query,
       "SELECT JobId, JobTDate, PurgedFiles FROM Job"
       " WHERE ClientId=%s AND FileSetId=%s AND Level='D'"
       " AND (JobTDate>%s OR (JobTDate=%s AND JobId>%s))"
       " AND JobStatus IN ('T','W') AND Type IN (%s)"
       " ORDER BY JobTDate DESC, JobId DESC LIMIT 1",
       edit_int64(client_id, ed1), edit_int64(fileset_id, ed2), ed3, ed3, ed4,
       kAccountableJobTypes);
  if (!ua->db->SqlQuery(query.c_str(), ChainJobHandler, &diff)) {
    ua->ErrorMsg("%s\n", ua->db->strerror());
    return ChainResult::kError;
  }
  if (diff.found) {
    if (diff.purged) { return ChainResult::kNotFound; /* File rows pruned */ }
    jobids->push_back(diff.JobId);
    baseline = diff.JobTDate;
    baseline_jobid = diff.JobId;
  }

  // 3. All Incrementals after the current baseline.
  JobIdListCtx incs{};
  edit_uint64(baseline, ed3);
  edit_int64(baseline_jobid, ed4);
  Mmsg(query,
       "SELECT JobId, PurgedFiles FROM Job"
       " WHERE ClientId=%s AND FileSetId=%s AND Level='I'"
       " AND (JobTDate>%s OR (JobTDate=%s AND JobId>%s))"
       " AND JobStatus IN ('T','W') AND Type IN (%s)"
       " ORDER BY JobTDate, JobId",
       edit_int64(client_id, ed1), edit_int64(fileset_id, ed2), ed3, ed3, ed4,
       kAccountableJobTypes);
  if (!ua->db->SqlQuery(query.c_str(), JobIdListHandler, &incs)) {
    ua->ErrorMsg("%s\n", ua->db->strerror());
    return ChainResult::kError;
  }
  if (incs.any_purged) { return ChainResult::kNotFound; /* File rows pruned */ }
  for (JobId_t id : incs.jobids) { jobids->push_back(id); }

  return ChainResult::kFound;
}

/**
 * Fetch the File rows for the resolved JobId chain, one per distinct
 * catalog path, already de-duplicated server-side: PostgreSQL's
 * DISTINCT ON picks the row with the highest JobTDate (JobId as
 * tie-breaker) per (PathId, Name) directly in the query, replacing the
 * previous approach of fetching every row and de-duplicating in an
 * application-side std::unordered_map (which held the full distinct
 * file set of the chain resident in Director memory -- many GB on
 * large catalogs, see plan.md benchmark).
 *
 * The query result itself is streamed via BigSqlQuery() (server-side
 * cursor, fetched in batches), and FileRowHandler() folds each row
 * directly into the running per-tuple totals in *scan* instead of
 * retaining it, so Director memory use no longer scales with the
 * number of distinct files in the chain.
 *
 * The background refresh can be cancelled between rows when Director is
 * shutting down or reloading its configuration.
 */
bool ScanFilesForChain(UaContext* ua,
                       const std::vector<JobId_t>& jobids,
                       FileScanCtx* scan)
{
  if (jobids.empty()) { return true; }

  PoolMem jobid_list(PM_MESSAGE);
  char ed1[50];
  for (size_t i = 0; i < jobids.size(); i++) {
    if (i > 0) { PmStrcat(jobid_list, ","); }
    PmStrcat(jobid_list, edit_uint64(jobids[i], ed1));
  }

  PoolMem query(PM_MESSAGE);
  Mmsg(query,
       "SELECT DISTINCT ON (File.PathId, File.Name)"
       " File.JobId, File.PathId, File.Name, File.FileIndex, File.LStat,"
       " Path.Path || File.Name FROM "
       "File"
       " JOIN Job USING (JobId) JOIN Path USING (PathId)"
       " WHERE File.JobId IN (%s)"
       " ORDER BY File.PathId, File.Name, Job.JobTDate DESC,"
       " File.JobId DESC",
       jobid_list.c_str());

  if (!ua->db->BigSqlQuery(query.c_str(), FileRowHandler, scan)) {
    ua->ErrorMsg("%s\n", ua->db->strerror());
    return false;
  }
  return !scan->aborted;
}

bool CalculateSubscriptionAccounting(UaContext* ua,
                                     std::vector<AccountingRow>* rows,
                                     std::string* error,
                                     const std::atomic_bool* cancel = nullptr)
{
  PoolMem query(PM_MESSAGE);
  Mmsg(query,
       "SELECT DISTINCT ON (Job.ClientId, FileSet.FileSet)"
       " Job.ClientId, Client.Name, Client.Uname,"
       " Job.FileSetId, FileSet.FileSet"
       " FROM Job"
       " JOIN Client ON Client.ClientId = Job.ClientId"
       " JOIN FileSet ON FileSet.FileSetId = Job.FileSetId"
       " WHERE Job.JobStatus IN ('T','W') AND Job.Type IN (%s)"
       " AND Job.Level='F'"
       " ORDER BY Job.ClientId, FileSet.FileSet,"
       " Job.JobTDate DESC, Job.JobId DESC",
       kAccountableJobTypes);

  TupleListCtx tuple_list{};
  if (!ua->db->SqlQuery(query.c_str(), TupleRowHandler, &tuple_list)) {
    *error = ua->db->strerror();
    ua->ErrorMsg("%s\n", error->c_str());
    return false;
  }
  std::sort(tuple_list.tuples.begin(), tuple_list.tuples.end(),
            [](const TupleInfo& a, const TupleInfo& b) {
              if (a.ClientName != b.ClientName) {
                return a.ClientName < b.ClientName;
              }
              return a.FileSetName < b.FileSetName;
            });

  rows->clear();
  for (const TupleInfo& tuple : tuple_list.tuples) {
    if (cancel && cancel->load()) {
      *error = "accounting refresh cancelled";
      return false;
    }

    AccountingRow row;
    row.ClientName = tuple.ClientName;
    row.FileSetName = tuple.FileSetName;

    std::vector<JobId_t> jobids;
    switch (
        ResolveAccountingChain(ua, tuple.ClientId, tuple.FileSetId, &jobids)) {
      case ChainResult::kError:
        *error = "failed to resolve backup chain";
        ua->ErrorMsg(T_("%s / %s: failed to resolve backup chain.\n"),
                     tuple.ClientName.c_str(), tuple.FileSetName.c_str());
        return false;
      case ChainResult::kNotFound:
        row.excluded = true;
        row.exclusion_reason = "no_usable_chain";
        rows->push_back(std::move(row));
        continue;
      case ChainResult::kFound:
        break;
    }

    FileScanCtx scan{};
    scan.ua = ua;
    scan.tuple = &tuple;
    scan.cancel = cancel;
    scan.is_windows = UnameLooksLikeWindows(tuple.Uname);
    if (!ScanFilesForChain(ua, jobids, &scan)) {
      *error = scan.aborted && cancel && cancel->load()
                   ? "accounting refresh cancelled"
                   : "failed to scan file attributes";
      return false;
    }

    if (scan.saw_opaque_plugin_image) {
      row.excluded = true;
      row.exclusion_reason = "opaque_backup_image";
    } else if (scan.saw_virtual_ndmp_archive && scan.files == 0) {
      row.excluded = true;
      row.exclusion_reason = "ndmp_no_file_history";
    } else {
      row.files = scan.files;
      row.bytes = scan.bytes;
      row.logical_bytes = scan.logical_bytes;
      row.rule = scan.is_windows ? "st_size" : "st_blocks*512";
      row.jobs_in_chain = jobids.size();
    }
    rows->push_back(std::move(row));
  }
  return true;
}

struct SnapshotMetadata {
  std::string timestamp;
  std::string last_error;
  bool has_snapshot{false};
  bool stale{true};
};

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
      const char* reason
          = row.exclusion_reason == "ndmp_no_file_history"
                ? T_("no per-file data available (NDMP file "
                     "history may be disabled)")
            : row.exclusion_reason == "no_per_file_data"
                ? T_("no measurable per-file data available")
            : row.exclusion_reason == "opaque_backup_image"
                ? T_("opaque backup image; using job-level estimate")
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
         "Size units are decimal (1 KB = 1,000 bytes).\n"));

  char ec1[50], ec2[50];
  ua->SendMsg(T_("\nGrand total: %s files, %s bytes across %u accounted "
                 "Client/FileSet combination(s)"),
              edit_uint64_with_commas(grand_total_files, ec1),
              edit_uint64_with_commas(grand_total_bytes, ec2),
              accounted_tuples);
  if (excluded_tuples > 0) {
    ua->SendMsg(T_(" (%u excluded for lack of file information)"),
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
