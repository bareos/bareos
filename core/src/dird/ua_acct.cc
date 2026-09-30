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
 * Real, File.LStat-based subscription/accounting size totals -- computed
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

#include <algorithm>
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

  /* The Storage Daemon creates a synthetic regular-file row for the entire
   * NDMP stream (stored/ndmp_tape.cc:BndmpCreateVirtualFile). Its deliberately
   * invalid size and fixed block fields distinguish it from backed-up files;
   * do not count this container as user data. */
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
  bool is_windows{false};
  uint64_t bytes{0};
  uint64_t logical_bytes{0};
  uint64_t files{0};
  bool aborted{false};
  bool saw_virtual_ndmp_archive{false};
};

int FileRowHandler(void* ctx, int, char** row)
{
  // row[0]=JobId row[1]=PathId row[2]=Name row[3]=FileIndex row[4]=LStat
  auto* c = static_cast<FileScanCtx*>(ctx);
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
      c->saw_virtual_ndmp_archive = true;
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
       " File.JobId, File.PathId, File.Name, File.FileIndex, File.LStat FROM "
       "File"
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
       "SELECT DISTINCT ON (Job.ClientId, FileSet.FileSet)"
       " Job.ClientId, Client.Name, Client.Uname,"
       " Job.FileSetId, FileSet.FileSet"
       " FROM Job"
       " JOIN Client ON Client.ClientId = Job.ClientId"
       " JOIN FileSet ON FileSet.FileSetId = Job.FileSetId"
       // No "Job.JobFiles > 0" filter here: it would run before
       // DISTINCT ON picks the newest Full, so a genuinely empty
       // newest Full could be filtered out first, letting an older,
       // non-empty Full under a stale FileSetId win the tuple instead
       // (undoing the very double-counting fix DISTINCT ON provides
       // here). An empty newest Full is a legitimate "0 files, 0
       // bytes" tuple, resolved normally by ResolveAccountingChain().
       " WHERE Job.JobStatus IN ('T','W') AND Job.Type IN (%s)"
       " AND Job.Level='F'",
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
  // DISTINCT ON requires its own columns to lead ORDER BY, so the
  // newest Full per (ClientId, FileSet name) wins the pick here; the
  // result set is re-sorted below for the documented Client.Name/
  // FileSet.FileSet output order.
  PmStrcat(query,
           " ORDER BY Job.ClientId, FileSet.FileSet,"
           " Job.JobTDate DESC, Job.JobId DESC");

  TupleListCtx tuple_list{};
  if (!ua->db->SqlQuery(query.c_str(), TupleRowHandler, &tuple_list)) {
    ua->ErrorMsg("%s\n", ua->db->strerror());
    return false;
  }
  std::sort(tuple_list.tuples.begin(), tuple_list.tuples.end(),
            [](const TupleInfo& a, const TupleInfo& b) {
              if (a.ClientName != b.ClientName) {
                return a.ClientName < b.ClientName;
              }
              return a.FileSetName < b.FileSetName;
            });

  if (tuple_list.tuples.empty()) {
    ua->SendMsg(T_("No matching Client/FileSet combinations found.\n"));
    return true;
  }

  ua->SendMsg(
      T_("\nReal (File.LStat-based) subscription accounting report:\n"));

  uint64_t grand_total_bytes = 0;
  uint64_t grand_total_logical_bytes = 0;
  uint64_t grand_total_files = 0;
  uint32_t accounted_tuples = 0;
  uint32_t excluded_tuples = 0;

  // Structured (.api 2 / WebUI) output, in addition to the plain-text
  // report above. ObjectKeyValue()/ArrayStart()/... calls are no-ops for
  // plain-text consumers when no format string is given, so this does not
  // change the human-readable output produced by ua->SendMsg() above.
  ua->send->ArrayStart("accounting");

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
        ua->send->ArrayEnd("accounting");
        return false;
      case ChainResult::kNotFound:
        ua->SendMsg(T_("%s / %s: no usable backup chain found -- excluded (not "
                       "guessed).\n"),
                    tuple.ClientName.c_str(), tuple.FileSetName.c_str());
        ua->send->ObjectStart();
        ua->send->ObjectKeyValue("client", tuple.ClientName.c_str());
        ua->send->ObjectKeyValue("fileset", tuple.FileSetName.c_str());
        ua->send->ObjectKeyValueBool("excluded", true);
        ua->send->ObjectEnd();
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
    if (!ScanFilesForChain(ua, jobids, &scan)) {
      ua->send->ArrayEnd("accounting");
      return false;
    }

    if (scan.saw_virtual_ndmp_archive && scan.files == 0) {
      ua->SendMsg(T_("%s / %s: no per-file data available (NDMP file history "
                     "may be disabled) -- excluded (not guessed).\n"),
                  tuple.ClientName.c_str(), tuple.FileSetName.c_str());
      ua->send->ObjectStart();
      ua->send->ObjectKeyValue("client", tuple.ClientName.c_str());
      ua->send->ObjectKeyValue("fileset", tuple.FileSetName.c_str());
      ua->send->ObjectKeyValueBool("excluded", true);
      ua->send->ObjectKeyValue("exclusion_reason", "no_per_file_data");
      ua->send->ObjectEnd();
      excluded_tuples++;
      continue;
    }

    uint64_t tuple_bytes = scan.bytes;
    uint64_t tuple_logical_bytes = scan.logical_bytes;
    uint64_t tuple_files = scan.files;
    const char* rule = is_windows ? "st_size" : "st_blocks*512";

    char ec1[50], ec2[50];
    ua->SendMsg(
        T_("%s / %s: %s files, %s bytes accounted (rule: %s, %zu jobs in "
           "chain).\n"),
        tuple.ClientName.c_str(), tuple.FileSetName.c_str(),
        edit_uint64_with_commas(tuple_files, ec1),
        edit_uint64_with_commas(tuple_bytes, ec2), rule, jobids.size());

    ua->SendMsg(T_("  Logical size (st_size): %s bytes.\n"),
                edit_uint64_with_commas(tuple_logical_bytes, ec1));

    ua->send->ObjectStart();
    ua->send->ObjectKeyValue("client", tuple.ClientName.c_str());
    ua->send->ObjectKeyValue("fileset", tuple.FileSetName.c_str());
    ua->send->ObjectKeyValueBool("excluded", false);
    ua->send->ObjectKeyValue("files", tuple_files);
    ua->send->ObjectKeyValue("bytes", tuple_bytes);
    ua->send->ObjectKeyValue("logical_bytes", tuple_logical_bytes);
    ua->send->ObjectKeyValue("rule", rule);
    ua->send->ObjectKeyValue("jobs_in_chain",
                             static_cast<uint64_t>(jobids.size()));
    ua->send->ObjectEnd();

    if (tuple_bytes
        > std::numeric_limits<uint64_t>::max() - grand_total_bytes) {
      ua->ErrorMsg(T_("Grand total byte count overflow -- aborting report.\n"));
      ua->send->ArrayEnd("accounting");
      return false;
    }
    if (tuple_logical_bytes
        > std::numeric_limits<uint64_t>::max() - grand_total_logical_bytes) {
      ua->ErrorMsg(
          T_("Grand total logical byte count overflow -- aborting report.\n"));
      ua->send->ArrayEnd("accounting");
      return false;
    }
    grand_total_bytes += tuple_bytes;
    grand_total_logical_bytes += tuple_logical_bytes;
    grand_total_files += tuple_files;
    accounted_tuples++;
  }

  ua->send->ArrayEnd("accounting");

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

} /* namespace directordaemon */
