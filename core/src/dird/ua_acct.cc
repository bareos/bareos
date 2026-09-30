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
 * Real, File.LStat-based subscription/accounting byte totals -- computed
 * on demand, per (Client, FileSet) tuple, from actual catalog File rows
 * rather than the guessed numbers used by the rest of 'status
 * subscriptions'. Implements 'status subscriptions accounting
 * [client=<name>] [fileset=<name>]'.
 */

#include "include/bareos.h"
#include "dird.h"
#include "dird/dird_globals.h"
#include "dird/ua_db.h"
#include "dird/ua_select.h"
#include "dird/ua_acct.h"
#include "lib/attribs.h"
#include "lib/edit.h"

#include <limits>
#include <string>
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
 *    directly -- Windows st_blocks is a synthetic value derived purely
 *    from st_size and adds no real accuracy.
 *  - Everyone else: use st_blocks * 512 (real block-allocation data,
 *    sparse-file aware).
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
 * kRegular      -- a countable, billable file; bytes is meaningful.
 * kNonRegular   -- a directory, symlink or special file (FIFO/device/
 *                  socket) catalog row -- not billable subscription
 *                  data, silently excluded from the report (not an
 *                  error).
 * kInvalidStat  -- the stat field the platform's rule needs is unusable
 *                  (unknown/overflowing); the caller must abort the
 *                  report rather than under-report silently.
 */
enum class FileAccountingKind
{
  kRegular,
  kNonRegular,
  kInvalidStat
};

struct FileAccountingResult {
  FileAccountingKind kind{FileAccountingKind::kInvalidStat};
  uint64_t bytes{0};
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
  if (!S_ISREG(statp.st_mode)) { return {FileAccountingKind::kNonRegular, 0}; }

  if (is_windows) {
    if (IsUnknownStatField(statp.st_size)) {
      return {FileAccountingKind::kInvalidStat, 0};
    }
    return {FileAccountingKind::kRegular, static_cast<uint64_t>(statp.st_size)};
  }

  if (IsUnknownStatField(statp.st_blocks)) {
    return {FileAccountingKind::kInvalidStat, 0};
  }
  uint64_t blocks = static_cast<uint64_t>(statp.st_blocks);
  if (blocks > std::numeric_limits<uint64_t>::max() / 512) {
    return {FileAccountingKind::kInvalidStat, 0};
  }
  return {FileAccountingKind::kRegular, blocks * 512};
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

struct ChainResolveCtx {
  JobId_t JobId{0};
  utime_t JobTDate{0};
  bool found{false};
};

int ChainJobHandler(void* ctx, int, char** row)
{
  auto* c = static_cast<ChainResolveCtx*>(ctx);
  c->JobId = static_cast<JobId_t>(str_to_int64(row[0]));
  c->JobTDate = static_cast<utime_t>(str_to_int64(row[1]));
  c->found = true;
  return 0;
}

struct JobIdListCtx {
  std::vector<JobId_t> jobids;
};

int JobIdListHandler(void* ctx, int, char** row)
{
  auto* c = static_cast<JobIdListCtx*>(ctx);
  c->jobids.push_back(static_cast<JobId_t>(str_to_int64(row[0])));
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
  bool is_windows{false};
  uint64_t bytes{0};
  uint64_t files{0};
  bool aborted{false};
};

int FileRowHandler(void* ctx, int, char** row)
{
  // row[0]=FileIndex row[1]=LStat
  auto* c = static_cast<FileScanCtx*>(ctx);
  auto FileIndex = static_cast<uint32_t>(str_to_int64(row[0]));

  /* FileIndex==0 marks an accurate-mode "this file was deleted since the
   * last backup" entry -- exclude it, don't count stale bytes. */
  if (FileIndex == 0) { return 0; }

  FileAccountingResult file_result
      = AccountedBytesForFile(row[1] ? row[1] : "", c->is_windows);
  if (file_result.kind == FileAccountingKind::kInvalidStat) {
    c->ua->ErrorMsg(T_("%s / %s: invalid file attributes for FileIndex=%u "
                       "-- aborting report.\n"),
                    c->tuple->ClientName.c_str(), c->tuple->FileSetName.c_str(),
                    FileIndex);
    c->aborted = true;
    return 1;
  }
  if (file_result.kind == FileAccountingKind::kNonRegular) {
    /* Directory, symlink or special file catalog row -- not billable
     * subscription data, exclude it from the count. */
    return 0;
  }
  if (file_result.bytes > std::numeric_limits<uint64_t>::max() - c->bytes) {
    c->ua->ErrorMsg(T_("%s / %s: byte total overflow -- aborting report.\n"),
                    c->tuple->ClientName.c_str(),
                    c->tuple->FileSetName.c_str());
    c->aborted = true;
    return 1;
  }
  c->bytes += file_result.bytes;
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
       "SELECT JobId, JobTDate FROM Job"
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

  jobids->push_back(full.JobId);
  utime_t baseline = full.JobTDate;
  JobId_t baseline_jobid = full.JobId;

  // 2. Most recent Differential after the Full, if any -- moves the
  // baseline forward so only Incrementals after it are collected.
  ChainResolveCtx diff{};
  edit_uint64(baseline, ed3);
  edit_int64(baseline_jobid, ed4);
  Mmsg(query,
       "SELECT JobId, JobTDate FROM Job"
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
    jobids->push_back(diff.JobId);
    baseline = diff.JobTDate;
    baseline_jobid = diff.JobId;
  }

  // 3. All Incrementals after the current baseline.
  JobIdListCtx incs{};
  edit_uint64(baseline, ed3);
  edit_int64(baseline_jobid, ed4);
  Mmsg(query,
       "SELECT JobId FROM Job"
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
 * There is currently no way for an operator to proactively cancel a
 * running report: "status subscriptions accounting" executes
 * synchronously inside the console's single command loop, with no
 * side channel available while it runs. The only way to stop it is to
 * terminate the console connection itself (e.g. killing bconsole),
 * which the Director will notice on its next socket I/O once the
 * report completes -- there is no active mid-query polling for a
 * closed console connection.
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
       " File.FileIndex, File.LStat FROM File"
       " JOIN Job USING (JobId) WHERE File.JobId IN (%s)"
       " ORDER BY File.PathId, File.Name, Job.JobTDate DESC,"
       " File.JobId DESC",
       jobid_list.c_str());

  if (!ua->db->BigSqlQuery(query.c_str(), FileRowHandler, scan)) {
    ua->ErrorMsg("%s\n", ua->db->strerror());
    return false;
  }
  return !scan->aborted;
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

  PoolMem query(PM_MESSAGE);
  Mmsg(query,
       "SELECT DISTINCT Job.ClientId, Client.Name, Client.Uname,"
       " Job.FileSetId, FileSet.FileSet"
       " FROM Job"
       " JOIN Client ON Client.ClientId = Job.ClientId"
       " JOIN FileSet ON FileSet.FileSetId = Job.FileSetId"
       " WHERE Job.JobStatus IN ('T','W') AND Job.Type IN (%s)"
       " AND Job.JobFiles > 0",
       kAccountableJobTypes);
  if (client_filter) {
    PmStrcat(query, " AND Client.Name='");
    PmStrcat(query, client_filter);
    PmStrcat(query, "'");
  }
  if (fileset_filter) {
    PmStrcat(query, " AND FileSet.FileSet='");
    PmStrcat(query, fileset_filter);
    PmStrcat(query, "'");
  }
  PmStrcat(query, " ORDER BY Client.Name, FileSet.FileSet");

  TupleListCtx tuple_list{};
  if (!ua->db->SqlQuery(query.c_str(), TupleRowHandler, &tuple_list)) {
    ua->ErrorMsg("%s\n", ua->db->strerror());
    return false;
  }

  if (tuple_list.tuples.empty()) {
    ua->SendMsg(T_("No matching Client/FileSet combinations found.\n"));
    return true;
  }

  ua->SendMsg(
      T_("\nReal (File.LStat-based) subscription accounting report:\n"));

  uint64_t grand_total_bytes = 0;
  uint64_t grand_total_files = 0;
  uint32_t accounted_tuples = 0;
  uint32_t excluded_tuples = 0;

  for (const TupleInfo& tuple : tuple_list.tuples) {
    std::vector<JobId_t> jobids;
    switch (
        ResolveAccountingChain(ua, tuple.ClientId, tuple.FileSetId, &jobids)) {
      case ChainResult::kError:
        // A catalog query failed -- abort the report rather than silently
        // sending an incomplete grand total that looks complete.
        ua->ErrorMsg(T_("%s / %s: failed to resolve backup chain -- aborting "
                        "report.\n"),
                     tuple.ClientName.c_str(), tuple.FileSetName.c_str());
        return false;
      case ChainResult::kNotFound:
        ua->SendMsg(T_("%s / %s: no usable backup chain found -- excluded (not "
                       "guessed).\n"),
                    tuple.ClientName.c_str(), tuple.FileSetName.c_str());
        excluded_tuples++;
        continue;
      case ChainResult::kFound:
        break;
    }

    bool is_windows = UnameLooksLikeWindows(tuple.Uname);

    FileScanCtx scan{};
    scan.ua = ua;
    scan.tuple = &tuple;
    scan.is_windows = is_windows;
    if (!ScanFilesForChain(ua, jobids, &scan)) { return false; }

    uint64_t tuple_bytes = scan.bytes;
    uint64_t tuple_files = scan.files;

    char ec1[50], ec2[50];
    ua->SendMsg(
        T_("%s / %s: %s files, %s bytes accounted (rule: %s, %zu jobs in "
           "chain).\n"),
        tuple.ClientName.c_str(), tuple.FileSetName.c_str(),
        edit_uint64_with_commas(tuple_files, ec1),
        edit_uint64_with_commas(tuple_bytes, ec2),
        is_windows ? "st_size" : "st_blocks*512", jobids.size());

    if (tuple_bytes
        > std::numeric_limits<uint64_t>::max() - grand_total_bytes) {
      ua->ErrorMsg(T_("Grand total byte count overflow -- aborting report.\n"));
      return false;
    }
    grand_total_bytes += tuple_bytes;
    grand_total_files += tuple_files;
    accounted_tuples++;
  }

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
  ua->SendMsg("\n");

  return true;
}

} /* namespace directordaemon */
