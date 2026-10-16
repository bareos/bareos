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
 * Computes the File.LStat based subscription accounting per Client/FileSet.
 */

#include "include/bareos.h"
#include "dird.h"
#include "dird/dird_globals.h"
#include "dird/ua_db.h"
#include "dird/ua_acct_internal.h"
#include "lib/attribs.h"
#include "lib/edit.h"
#include "lib/stat_utils.h"

#include <atomic>
#include <limits>
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

/**
 * kRegular      -- a countable file; both byte counts are meaningful.
 * kNonRegular   -- a directory, symlink or special file (FIFO/device/
 *                  socket) catalog row -- not billable subscription
 *                  data, silently excluded from the report (not an
 *                  error).
 * kUnmeasurableStream -- a virtual backup stream with placeholder size
 *                  attributes; the caller determines whether per-file
 *                  history is available or a job-level fallback is needed.
 * kInvalidStat  -- the stat field the platform's rule needs is unusable
 *                  (unknown/overflowing); the caller must abort the
 *                  report rather than under-report silently.
 */
enum class FileAccountingKind
{
  kRegular,
  kNonRegular,
  kUnmeasurableStream,
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
                                           bool is_windows,
                                           std::string_view filename)
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
    return {FileAccountingKind::kUnmeasurableStream, 0, 0};
  }

  // MSSQL VDI uses zero as a placeholder, not as the backup stream size.
  if (filename.starts_with("/@MSSQL/") && (statp.st_mode & 07777) == 0700
      && statp.st_size == 0 && statp.st_blksize == 65536
      && statp.st_blocks == 1) {
    return {FileAccountingKind::kUnmeasurableStream, 0, 0};
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

  FileAccountingResult file_result = AccountedBytesForFile(
      row[4] ? row[4] : "", c->is_windows, row[5] ? row[5] : "");
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
      || file_result.kind == FileAccountingKind::kUnmeasurableStream) {
    /* Non-regular metadata and opaque streams cannot provide per-file
     * measurements. NDMP may supply separate file-history rows. */
    if (file_result.kind == FileAccountingKind::kUnmeasurableStream) {
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

struct FullJobSizeCtx {
  bool found{false};
  uint64_t bytes{0};
  uint64_t files{0};
};

int FullJobSizeHandler(void* ctx, int, char** row)
{
  auto* size = static_cast<FullJobSizeCtx*>(ctx);
  size->found = true;
  size->bytes = row[0] ? str_to_uint64(row[0]) : 0;
  size->files = row[1] ? str_to_uint64(row[1]) : 0;
  return 0;
}

/* Job-level size (max(ReadBytes, JobBytes), as in the legacy report) of the
 * chain's baseline Full. Used only when the chain holds an opaque plugin
 * image whose File row carries no real size. */
bool FetchBaselineFullJobSize(UaContext* ua,
                              JobId_t full_jobid,
                              FullJobSizeCtx* size)
{
  PoolMem query(PM_MESSAGE);
  char ed1[50];
  Mmsg(query,
       "SELECT GREATEST(ReadBytes, JobBytes)::text, JobFiles::text FROM Job"
       " WHERE JobId=%s",
       edit_int64(full_jobid, ed1));
  if (!ua->db->SqlQuery(query.c_str(), FullJobSizeHandler, size)) {
    ua->ErrorMsg("%s\n", ua->db->strerror());
    return false;
  }
  return true;
}

}  // namespace

bool CalculateSubscriptionAccounting(UaContext* ua,
                                     std::vector<AccountingRow>* rows,
                                     std::string* error,
                                     const std::atomic_bool* cancel)
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
      FullJobSizeCtx full_size{};
      if (!FetchBaselineFullJobSize(ua, jobids.front(), &full_size)) {
        *error = "failed to query baseline job size";
        return false;
      }
      if (full_size.bytes == 0) {
        row.excluded = true;
        row.exclusion_reason = "opaque_backup_image";
      } else {
        row.files = full_size.files;
        row.bytes = full_size.bytes;
        row.logical_bytes = full_size.bytes;
        row.rule = "job_bytes";
        row.jobs_in_chain = jobids.size();
      }
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

} /* namespace directordaemon */
