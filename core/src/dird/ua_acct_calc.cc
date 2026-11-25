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

/* Migration and consolidation control jobs do not carry the resulting file
 * rows. Their output jobs are backups or copies and use 'B' or 'C'. */
constexpr const char* kAccountableJobTypes = "'B','C'";

/**
 * Per-platform accounting rule (see DeveloperGuide/SubscriptionAccounting.rst):
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
  struct stat statp {};
  const char* invalid_reason{nullptr};
};

FileAccountingResult AccountedBytesForFile(const std::string& lstat,
                                           bool is_windows,
                                           std::string_view filename)
{
  if (lstat.empty()) {
    return {FileAccountingKind::kInvalidStat, 0, 0, {}, "LStat is empty"};
  }

  struct stat statp {};
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
  JobId_t LatestFullJobId{0};
  DBId_t ClientId{0};
  std::string ClientName;
  std::string Uname;
  DBId_t FileSetId{0};
  std::string FileSetName;
};

struct AccountingJobInfo {
  JobId_t JobId{0};
  DBId_t ClientId{0};
  DBId_t FileSetId{0};
  char Level{0};
  std::string ClientName;
  std::string Uname;
  std::string FileSetName;
  bool found{false};
};

int AccountingJobInfoHandler(void* ctx, int, char** row)
{
  auto* info = static_cast<AccountingJobInfo*>(ctx);
  info->JobId = static_cast<JobId_t>(str_to_int64(row[0]));
  info->ClientId = static_cast<DBId_t>(str_to_int64(row[1]));
  info->ClientName = row[2] ? row[2] : "";
  info->Uname = row[3] ? row[3] : "";
  info->FileSetId = static_cast<DBId_t>(str_to_int64(row[4]));
  info->FileSetName = row[5] ? row[5] : "";
  info->Level = row[6] ? row[6][0] : 0;
  info->found = true;
  return 0;
}

struct StoredAccountingCtx {
  AccountingRow row;
  bool found{false};
};

int StoredAccountingHandler(void* ctx, int, char** row)
{
  auto* stored = static_cast<StoredAccountingCtx*>(ctx);
  stored->found = row[0] && (bstrcmp(row[0], "true") || bstrcmp(row[0], "t"));
  if (!stored->found) { return 0; }
  stored->row.JobId = static_cast<JobId_t>(str_to_int64(row[1]));
  stored->row.excluded
      = row[2] && (bstrcmp(row[2], "true") || bstrcmp(row[2], "t"));
  stored->row.exclusion_reason = row[3] ? row[3] : "";
  stored->row.files = row[4] ? str_to_uint64(row[4]) : 0;
  stored->row.bytes = row[5] ? str_to_uint64(row[5]) : 0;
  stored->row.logical_bytes = row[6] ? str_to_uint64(row[6]) : 0;
  stored->row.opaque_files = row[7] ? str_to_uint64(row[7]) : 0;
  stored->row.ndmp_archives = row[8] ? str_to_uint64(row[8]) : 0;
  stored->row.rule = row[9] ? row[9] : "";
  stored->row.jobs_in_chain = row[10] ? str_to_uint64(row[10]) : 0;
  return 0;
}


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
  std::optional<SubscriptionAccountingDiagnostic>* diagnostic{nullptr};
  bool is_windows{false};
  uint64_t bytes{0};
  uint64_t logical_bytes{0};
  uint64_t files{0};
  bool aborted{false};
  bool saw_virtual_ndmp_archive{false};
  bool saw_opaque_plugin_image{false};
  uint64_t opaque_files{0};
  uint64_t ndmp_archives{0};
  std::string error;
};

SubscriptionAccountingDiagnostic MakeInvalidLstatDiagnostic(
    JobId_t jobid,
    uint32_t fileindex,
    const char* path,
    const TupleInfo& tuple,
    const char* reason)
{
  return {"invalid_lstat",     jobid,
          fileindex,           path ? path : "",
          tuple.ClientName,    tuple.FileSetName,
          reason ? reason : ""};
}

std::string EscapeDiagnosticText(std::string_view value)
{
  constexpr char kHex[] = "0123456789ABCDEF";
  std::string escaped;
  for (unsigned char ch : value) {
    if (ch == '\\') {
      escaped += "\\\\";
    } else if (ch < 0x20 || ch == 0x7f) {
      escaped += "\\x";
      escaped += kHex[ch >> 4];
      escaped += kHex[ch & 0x0f];
    } else {
      escaped += static_cast<char>(ch);
    }
  }
  return escaped;
}

int FileRowHandler(void* ctx, int, char** row)
{
  // row[0]=JobId row[3]=FileIndex row[4]=LStat row[5]=full catalog filename
  auto* c = static_cast<FileScanCtx*>(ctx);
  if (c->cancel && c->cancel->load()) {
    c->aborted = true;
    return 1;
  }
  auto JobId = row[0] ? row[0] : "";
  auto FileIndex = static_cast<uint32_t>(str_to_int64(row[3]));

  /* FileIndex==0 marks an accurate-mode "this file was deleted since the
   * last backup" entry -- exclude it, don't count stale bytes. */
  if (FileIndex == 0) { return 0; }

  FileAccountingResult file_result = AccountedBytesForFile(
      row[4] ? row[4] : "", c->is_windows, row[5] ? row[5] : "");
  if (file_result.kind == FileAccountingKind::kInvalidStat) {
    auto invalid = MakeInvalidLstatDiagnostic(
        static_cast<JobId_t>(str_to_int64(JobId)), FileIndex, row[5], *c->tuple,
        file_result.invalid_reason);
    c->error = FormatSubscriptionAccountingDiagnostic(invalid);
    if (c->diagnostic) { *c->diagnostic = std::move(invalid); }
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
        c->ndmp_archives++;
      } else {
        c->saw_opaque_plugin_image = true;
        c->opaque_files++;
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
  // row[3]=FileSetId row[4]=FileSet.FileSet row[5]=JobId
  auto* c = static_cast<TupleListCtx*>(ctx);
  TupleInfo t;
  t.ClientId = static_cast<DBId_t>(str_to_int64(row[0]));
  t.ClientName = row[1] ? row[1] : "";
  t.Uname = row[2] ? row[2] : "";
  t.FileSetId = static_cast<DBId_t>(str_to_int64(row[3]));
  t.FileSetName = row[4] ? row[4] : "";
  t.LatestFullJobId = static_cast<JobId_t>(str_to_int64(row[5]));
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
 * DISTINCT ON picks the row with the highest JobTDate (JobId, then FileId as
 * tie-breakers) per (PathId, Name) directly in the query, replacing the
 * previous approach of fetching every row and de-duplicating in an
 * application-side std::unordered_map (which held the full distinct
 * file set of the chain resident in Director memory -- many GB on
 * large catalogs).
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
       " File.JobId DESC, File.FileId DESC",
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
  bool has_ndmp_environment{false};
};

int FullJobSizeHandler(void* ctx, int, char** row)
{
  auto* size = static_cast<FullJobSizeCtx*>(ctx);
  size->found = true;
  size->bytes = row[0] ? str_to_uint64(row[0]) : 0;
  size->files = row[1] ? str_to_uint64(row[1]) : 0;
  size->has_ndmp_environment = row[2] && row[2][0] == '1';
  return 0;
}

/* Job-level size (max(ReadBytes, JobBytes), as in the legacy report) of the
 * chain's baseline Full. Used for opaque plugin images and NDMP archives
 * without file history whose File rows carry no real size. */
bool FetchBaselineFullJobSize(UaContext* ua,
                              JobId_t full_jobid,
                              FullJobSizeCtx* size)
{
  PoolMem query(PM_MESSAGE);
  char ed1[50];
  Mmsg(query,
       "SELECT GREATEST(ReadBytes, JobBytes)::text, JobFiles::text,"
       " CASE WHEN EXISTS (SELECT 1 FROM NDMPJobEnvironment e"
       " WHERE e.JobId=Job.JobId) THEN '1' ELSE '0' END FROM Job"
       " WHERE JobId=%s",
       edit_int64(full_jobid, ed1));
  if (!ua->db->SqlQuery(query.c_str(), FullJobSizeHandler, size)) {
    ua->ErrorMsg("%s\n", ua->db->strerror());
    return false;
  }
  return true;
}

bool LoadAccountingJobInfo(UaContext* ua,
                           JobId_t jobid,
                           AccountingJobInfo* info,
                           std::string* error)
{
  PoolMem query(PM_MESSAGE);
  char ed1[50];
  Mmsg(query,
       "SELECT Job.JobId, Client.ClientId, Client.Name, Client.Uname, "
       "FileSet.FileSetId, FileSet.FileSet, Job.Level FROM Job "
       "JOIN Client USING (ClientId) JOIN FileSet USING (FileSetId) "
       "WHERE Job.JobId=%s AND Job.JobStatus IN ('T','W') "
       "AND Job.Type IN (%s)",
       edit_int64(jobid, ed1), kAccountableJobTypes);
  if (!ua->db->SqlQuery(query.c_str(), AccountingJobInfoHandler, info)) {
    ua->ErrorMsg("%s\n", ua->db->strerror());
    return false;
  }
  if (!info->found) {
    *error = "completed accounting job was not found";
    return false;
  }
  return true;
}

bool LoadStoredAccounting(UaContext* ua,
                          JobId_t jobid,
                          StoredAccountingCtx* stored)
{
  PoolMem query(PM_MESSAGE);
  char ed1[50];
  Mmsg(query,
       "SELECT AccountingValid::text, JobId, "
       "COALESCE(AccountingExcluded, FALSE)::text, "
       "COALESCE(AccountingExclusionReason, ''), "
       "AccountingFiles::text, AccountingBytes::text, "
       "AccountingLogicalBytes::text, AccountingOpaqueFiles::text, "
       "AccountingNdmpArchives::text, COALESCE(AccountingRule, ''), "
       "AccountingJobsInChain::text FROM Job WHERE JobId=%s",
       edit_int64(jobid, ed1));
  if (!ua->db->SqlQuery(query.c_str(), StoredAccountingHandler, stored)) {
    ua->ErrorMsg("%s\n", ua->db->strerror());
    return false;
  }
  return true;
}

bool AddOrSubtract(uint64_t* value, uint64_t amount, bool add)
{
  if (add) {
    if (amount > std::numeric_limits<uint64_t>::max() - *value) {
      return false;
    }
    *value += amount;
    return true;
  }
  if (amount > *value) { return false; }
  *value -= amount;
  return true;
}

struct DeltaScanCtx {
  AccountingRow* row{nullptr};
  const TupleInfo* tuple{nullptr};
  JobId_t current_jobid{0};
  std::optional<SubscriptionAccountingDiagnostic>* diagnostic{nullptr};
  bool is_windows{false};
  bool requires_full_scan{false};
  bool aborted{false};
  std::string error;
};

bool ApplyFileDelta(DeltaScanCtx* c,
                    JobId_t jobid,
                    uint32_t fileindex,
                    const char* lstat,
                    const char* filename,
                    bool add)
{
  FileAccountingResult result = AccountedBytesForFile(
      lstat ? lstat : "", c->is_windows, filename ? filename : "");
  if (result.kind == FileAccountingKind::kInvalidStat) {
    auto invalid = MakeInvalidLstatDiagnostic(jobid, fileindex, filename,
                                              *c->tuple, result.invalid_reason);
    c->error = FormatSubscriptionAccountingDiagnostic(invalid);
    if (c->diagnostic) { *c->diagnostic = std::move(invalid); }
    c->aborted = true;
    return false;
  }
  if (result.kind == FileAccountingKind::kNonRegular) { return true; }
  if (result.kind == FileAccountingKind::kUnmeasurableStream) {
    if (filename && std::string_view(filename).starts_with("/@NDMP/")) {
      if (!AddOrSubtract(&c->row->ndmp_archives, 1, add)) {
        c->error = "NDMP archive count underflow or overflow";
        c->aborted = true;
        return false;
      }
    } else {
      c->requires_full_scan = true;
      if (!AddOrSubtract(&c->row->opaque_files, 1, add)) {
        c->error = "opaque stream count underflow or overflow";
        c->aborted = true;
        return false;
      }
    }
    return true;
  }
  if (!AddOrSubtract(&c->row->files, 1, add)
      || !AddOrSubtract(&c->row->bytes, result.bytes, add)
      || !AddOrSubtract(&c->row->logical_bytes, result.logical_bytes, add)) {
    c->error = "accounting total underflow or overflow";
    c->aborted = true;
    return false;
  }
  return true;
}

int ChangedFileRowHandler(void* ctx, int, char** row)
{
  // row[0..2] are the current FileIndex, LStat and full path.
  // row[3..5] are the previous effective FileIndex, LStat and JobId.
  auto* c = static_cast<DeltaScanCtx*>(ctx);
  const bool had_previous
      = row[3] && static_cast<uint32_t>(str_to_int64(row[3])) != 0;
  const bool has_current
      = row[0] && static_cast<uint32_t>(str_to_int64(row[0])) != 0;
  if (had_previous
      && !ApplyFileDelta(c, static_cast<JobId_t>(str_to_int64(row[5])),
                         static_cast<uint32_t>(str_to_int64(row[3])), row[4],
                         row[2], false)) {
    return 1;
  }
  if (has_current
      && !ApplyFileDelta(c, c->current_jobid,
                         static_cast<uint32_t>(str_to_int64(row[0])), row[1],
                         row[2], true)) {
    return 1;
  }
  return 0;
}

bool ScanChangedFiles(UaContext* ua,
                      JobId_t current_jobid,
                      const std::vector<JobId_t>& previous_jobids,
                      DeltaScanCtx* delta)
{
  if (previous_jobids.empty()) { return true; }
  PoolMem previous_ids(PM_MESSAGE);
  char ed1[50], ed2[50];
  for (size_t i = 0; i < previous_jobids.size(); i++) {
    if (i > 0) { PmStrcat(previous_ids, ","); }
    PmStrcat(previous_ids, edit_uint64(previous_jobids[i], ed1));
  }
  PoolMem query(PM_MESSAGE);
  Mmsg(
      query,
      "SELECT current_paths.FileIndex, current_paths.LStat, "
      "current_paths.Filename, previous_paths.FileIndex, previous_paths.LStat, "
      "previous_paths.JobId "
      "FROM ("
      " SELECT DISTINCT ON (File.PathId, File.Name) File.PathId, "
      "File.Name, File.FileIndex, File.LStat, Path.Path || File.Name AS "
      "Filename FROM File JOIN Path USING (PathId) WHERE File.JobId=%s "
      "ORDER BY File.PathId, File.Name, File.FileId DESC) current_paths "
      "LEFT JOIN ("
      " SELECT DISTINCT ON (File.PathId, File.Name) File.PathId, "
      "File.Name, File.FileIndex, File.LStat, File.JobId FROM File "
      "JOIN Job USING (JobId) "
      "WHERE File.JobId IN (%s) "
      "ORDER BY File.PathId, File.Name, Job.JobTDate DESC, "
      "File.JobId DESC, File.FileId DESC) previous_paths USING (PathId, Name)",
      edit_int64(current_jobid, ed2), previous_ids.c_str());
  if (!ua->db->BigSqlQuery(query.c_str(), ChangedFileRowHandler, delta)) {
    ua->ErrorMsg("%s\n", ua->db->strerror());
    return false;
  }
  return !delta->aborted;
}

bool MakeAccountingRowFromScan(UaContext* ua,
                               const std::vector<JobId_t>& jobids,
                               const FileScanCtx& scan,
                               AccountingRow* row,
                               std::string* error)
{
  FullJobSizeCtx full_size{};
  if (scan.saw_opaque_plugin_image || scan.files == 0) {
    if (!FetchBaselineFullJobSize(ua, jobids.front(), &full_size)) {
      *error = "failed to query baseline job size";
      return false;
    }
  }
  if (scan.saw_opaque_plugin_image
      || (scan.files == 0
          && (scan.saw_virtual_ndmp_archive
              || full_size.has_ndmp_environment))) {
    if (full_size.bytes == 0) {
      row->excluded = true;
      row->exclusion_reason = scan.saw_opaque_plugin_image
                                  ? "opaque_backup_image"
                                  : "ndmp_no_file_history";
    } else {
      row->files = full_size.files;
      row->bytes = full_size.bytes;
      row->logical_bytes = full_size.bytes;
      row->rule = "job_bytes";
    }
  } else {
    row->files = scan.files;
    row->bytes = scan.bytes;
    row->logical_bytes = scan.logical_bytes;
    row->rule = scan.is_windows ? "st_size" : "st_blocks*512";
  }
  row->opaque_files = scan.opaque_files;
  row->ndmp_archives = scan.ndmp_archives;
  row->jobs_in_chain = jobids.size();
  return true;
}

}  // namespace

std::string FormatSubscriptionAccountingDiagnostic(
    const SubscriptionAccountingDiagnostic& diagnostic)
{
  return "Invalid file attributes: JobId=" + std::to_string(diagnostic.jobid)
         + " FileIndex=" + std::to_string(diagnostic.fileindex)
         + " Client=" + EscapeDiagnosticText(diagnostic.client)
         + " FileSet=" + EscapeDiagnosticText(diagnostic.fileset) + " Path='"
         + EscapeDiagnosticText(diagnostic.path)
         + "': " + EscapeDiagnosticText(diagnostic.reason);
}

bool CalculateSubscriptionAccounting(
    UaContext* ua,
    std::vector<AccountingRow>* rows,
    std::string* error,
    const std::atomic_bool* cancel,
    std::optional<SubscriptionAccountingDiagnostic>* diagnostic)
{
  if (diagnostic) { diagnostic->reset(); }
  PoolMem query(PM_MESSAGE);
  Mmsg(query,
       "SELECT DISTINCT ON (Job.ClientId, FileSet.FileSet)"
       " Job.ClientId, Client.Name, Client.Uname,"
       " Job.FileSetId, FileSet.FileSet, Job.JobId"
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
    row.JobId = tuple.LatestFullJobId;
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
    row.JobId = jobids.back();

    FileScanCtx scan{};
    scan.ua = ua;
    scan.tuple = &tuple;
    scan.cancel = cancel;
    scan.diagnostic = diagnostic;
    scan.is_windows = UnameLooksLikeWindows(tuple.Uname);
    if (!ScanFilesForChain(ua, jobids, &scan)) {
      *error = scan.aborted && cancel && cancel->load()
                   ? "accounting refresh cancelled"
               : scan.error.empty() ? "failed to scan file attributes"
                                    : scan.error;
      return false;
    }

    if (!MakeAccountingRowFromScan(ua, jobids, scan, &row, error)) {
      return false;
    }
    rows->push_back(std::move(row));
  }
  return true;
}

bool CalculateSubscriptionAccountingForJob(
    UaContext* ua,
    JobId_t jobid,
    AccountingRow* row,
    std::string* error,
    const std::atomic_bool* cancel,
    std::optional<SubscriptionAccountingDiagnostic>* diagnostic)
{
  if (diagnostic) { diagnostic->reset(); }
  AccountingJobInfo job{};
  if (!LoadAccountingJobInfo(ua, jobid, &job, error)) { return false; }

  TupleInfo tuple;
  tuple.LatestFullJobId = jobid;
  tuple.ClientId = job.ClientId;
  tuple.ClientName = job.ClientName;
  tuple.Uname = job.Uname;
  tuple.FileSetId = job.FileSetId;
  tuple.FileSetName = job.FileSetName;

  *row = AccountingRow{};
  row->JobId = jobid;
  row->ClientName = job.ClientName;
  row->FileSetName = job.FileSetName;

  std::vector<JobId_t> jobids;
  switch (ResolveAccountingChain(ua, job.ClientId, job.FileSetId, &jobids)) {
    case ChainResult::kError:
      *error = "failed to resolve backup chain";
      return false;
    case ChainResult::kNotFound:
      row->excluded = true;
      row->exclusion_reason = "no_usable_chain";
      return true;
    case ChainResult::kFound:
      break;
  }
  if (jobids.back() != jobid) {
    row->JobId = 0;
    return true;
  }

  auto full_scan = [&]() {
    AccountingRow full_row;
    full_row.JobId = jobid;
    full_row.ClientName = job.ClientName;
    full_row.FileSetName = job.FileSetName;
    FileScanCtx scan{};
    scan.ua = ua;
    scan.tuple = &tuple;
    scan.cancel = cancel;
    scan.diagnostic = diagnostic;
    scan.is_windows = UnameLooksLikeWindows(job.Uname);
    if (!ScanFilesForChain(ua, jobids, &scan)) {
      *error = scan.aborted && cancel && cancel->load()
                   ? "accounting calculation cancelled"
               : scan.error.empty() ? "failed to scan file attributes"
                                    : scan.error;
      return false;
    }
    if (!MakeAccountingRowFromScan(ua, jobids, scan, &full_row, error)) {
      return false;
    }
    *row = std::move(full_row);
    return true;
  };

  if (jobids.size() == 1) { return full_scan(); }
  if (cancel && cancel->load()) {
    *error = "accounting calculation cancelled";
    return false;
  }

  const JobId_t previous_jobid = jobids[jobids.size() - 2];
  StoredAccountingCtx previous{};
  if (!LoadStoredAccounting(ua, previous_jobid, &previous)) {
    *error = "failed to load previous job accounting";
    return false;
  }
  const std::string expected_rule
      = UnameLooksLikeWindows(job.Uname) ? "st_size" : "st_blocks*512";
  if (!previous.found || previous.row.excluded
      || previous.row.rule != expected_rule || previous.row.opaque_files > 0) {
    return full_scan();
  }

  *row = previous.row;
  row->JobId = jobid;
  row->ClientName = job.ClientName;
  row->FileSetName = job.FileSetName;
  row->jobs_in_chain = jobids.size();

  std::vector<JobId_t> previous_jobids(jobids.begin(), jobids.end() - 1);
  DeltaScanCtx delta{};
  delta.row = row;
  delta.tuple = &tuple;
  delta.current_jobid = jobid;
  delta.diagnostic = diagnostic;
  delta.is_windows = UnameLooksLikeWindows(job.Uname);
  if (!ScanChangedFiles(ua, jobid, previous_jobids, &delta)) {
    *error = delta.error.empty() ? "failed to scan changed file attributes"
                                 : delta.error;
    return false;
  }
  if (delta.requires_full_scan || row->opaque_files > 0) { return full_scan(); }
  row->rule = expected_rule;
  return true;
}

} /* namespace directordaemon */
