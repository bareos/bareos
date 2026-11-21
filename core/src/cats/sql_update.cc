/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2012 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2016 Planets Communications B.V.
   Copyright (C) 2013-2026 Bareos GmbH & Co. KG

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
// Kern Sibbald, March 2000
/**
 * @file
 * BAREOS Catalog Database Update record interface routines
 */

#include "include/bareos.h"

#include "cats.h"
#include "lib/edit.h"

#include <string>

/* -----------------------------------------------------------------------
 *
 *   Generic Routines (or almost generic)
 *
 * -----------------------------------------------------------------------
 */

/* -----------------------------------------------------------------------
 *
 *   Generic Routines (or almost generic)
 *
 * -----------------------------------------------------------------------
 */
/* Update the attributes record by adding the file digest */
bool BareosDb::AddDigestToFileRecord(JobControlRecord* jcr,
                                     FileId_t FileId,
                                     char* digest,
                                     int)
{
  DbLocker _{this};
  auto esc_digest = EscapeString(jcr, digest);
  if (!esc_digest) { return false; }
  Mmsg(cmd, "UPDATE File SET MD5='%s' WHERE FileId=%" PRIu64,
       esc_digest->c_str(), FileId);

  return UpdateDb(jcr, cmd) > 0;
}

/* Mark the file record as being visited during database
 * verify compare. Stuff JobId into the MarkId field
 */
bool BareosDb::MarkFileRecord(JobControlRecord* jcr,
                              FileId_t FileId,
                              JobId_t JobId)
{
  DbLocker _{this};
  Mmsg(cmd, "UPDATE File SET MarkId=%" PRIu32 " WHERE FileId=%" PRIu64, JobId,
       FileId);

  return UpdateDb(jcr, cmd) > 0;
}

/**
 * Update the Job record at start of Job
 *
 * Returns: false on failure
 *          true  on success
 */
bool BareosDb::UpdateJobStartRecord(JobControlRecord* jcr, JobDbRecord* jr)
{
  char dt[MAX_TIME_LENGTH];
  time_t stime;
  btime_t JobTDate;
  char ed1[50], ed2[50], ed3[50], ed4[50], ed5[50];

  stime = jr->StartTime;
  bstrutime(dt, sizeof(dt), stime);
  JobTDate = (btime_t)stime;

  DbLocker _{this};
  Mmsg(cmd,
       "UPDATE Job SET JobStatus='%c',Level='%c',StartTime='%s',"
       "ClientId=%s,JobTDate=%s,PoolId=%s,FileSetId=%s,VolSessionId=%" PRIu32
       ", VolSessionTime=%" PRIu32 " WHERE JobId=%s",
       (char)(jcr->getJobStatus()), (char)(jr->JobLevel), dt,
       edit_int64(jr->ClientId, ed1), edit_uint64(JobTDate, ed2),
       edit_int64(jr->PoolId, ed3), edit_int64(jr->FileSetId, ed4),
       jcr->VolSessionId, jcr->VolSessionTime, edit_int64(jr->JobId, ed5));

  changes = 0;
  return UpdateDb(jcr, cmd) > 0;
}

bool BareosDb::UpdateRunningJobRecord(JobControlRecord* jcr)
{
  DbLocker _{this};
  Mmsg(cmd,
       "UPDATE Job SET JobBytes=%" PRIu64 ",JobFiles=%" PRIu32
       " WHERE JobId=%" PRIu32,
       jcr->JobBytes, jcr->JobFiles, jcr->JobId);

  return UpdateDb(jcr, cmd) > 0;
}

// Update Long term statistics with all jobs that were run before age seconds
int BareosDb::UpdateStats(JobControlRecord* jcr, utime_t age)
{
  char ed1[30];
  int rows;
  utime_t now = (utime_t)time(NULL);

  DbLocker _{this};

  edit_uint64(now - age, ed1);
  FillQuery<SQL_QUERY::fill_jobhisto>(cmd, ed1);
  if (QueryDb(jcr, cmd)) {
    rows = SqlAffectedRows();
  } else {
    rows = -1;
  }

  return rows;
}

/**
 * Update the Job record at end of Job
 *
 * Returns: false on failure
 *          true on success
 */
bool BareosDb::UpdateJobEndRecord(JobControlRecord* jcr, JobDbRecord* jr)
{
  char dt[MAX_TIME_LENGTH];
  char rdt[MAX_TIME_LENGTH];
  time_t ttime;
  char ed1[30], ed2[30], ed3[50], ed4[50];
  btime_t JobTDate;
  char PriorJobId[50];

  if (jr->PriorJobId) {
    bstrncpy(PriorJobId, edit_int64(jr->PriorJobId, ed1), sizeof(PriorJobId));
  } else {
    bstrncpy(PriorJobId, "0", sizeof(PriorJobId));
  }

  ttime = jr->EndTime;
  bstrutime(dt, sizeof(dt), ttime);

  if (jr->RealEndTime < jr->EndTime) { jr->RealEndTime = jr->EndTime; }
  ttime = jr->RealEndTime;
  bstrutime(rdt, sizeof(rdt), ttime);

  JobTDate = ttime;

  DbLocker _{this};

  Mmsg(cmd,
       "UPDATE Job SET JobStatus='%c',Level='%c',EndTime='%s',"
       "ClientId=%u,JobBytes=%s,ReadBytes=%s,JobFiles=%u,JobErrors=%u,"
       "VolSessionId=%u,"
       "VolSessionTime=%u,PoolId=%u,FileSetId=%u,JobTDate=%s,"
       "RealEndTime='%s',PriorJobId=%s,PurgedFiles=%d WHERE JobId=%s",
       (char)(jr->JobStatus), (char)(jr->JobLevel), dt, jr->ClientId,
       edit_uint64(jr->JobBytes, ed1), edit_uint64(jr->ReadBytes, ed4),
       jr->JobFiles, jr->JobErrors, jr->VolSessionId, jr->VolSessionTime,
       jr->PoolId, jr->FileSetId, edit_uint64(JobTDate, ed2), rdt, PriorJobId,
       jr->PurgedFiles, edit_int64(jr->JobId, ed3));

  return UpdateDb(jcr, cmd) > 0;
}

/**
 * Update Client record
 * Returns: false on failure
 *          true on success
 */
bool BareosDb::UpdateClientRecord(JobControlRecord* jcr, ClientDbRecord* cr)
{
  ClientDbRecord tcr;

  DbLocker _{this};
  tcr = *cr;
  if (!CreateClientRecord(jcr, &tcr)) { return false; }

  auto esc_clientname = EscapeString(jcr, cr->Name);
  if (!esc_clientname) { return false; }
  auto esc_uname = EscapeString(jcr, cr->Uname);
  if (!esc_uname) { return false; }
  Mmsg(cmd,
       "UPDATE Client SET AutoPrune=%d,FileRetention=%" PRIu64
       ",JobRetention=%" PRIu64
       ","
       "Uname='%s' WHERE Name='%s'",
       cr->AutoPrune, cr->FileRetention, cr->JobRetention, esc_uname->c_str(),
       esc_clientname->c_str());

  return UpdateDb(jcr, cmd) > 0;
}

/**
 * Update Counters record
 * Returns: false on failure
 *          true on success
 */
bool BareosDb::UpdateCounterRecord(JobControlRecord* jcr, CounterDbRecord* cr)
{
  DbLocker _{this};

  auto esc = EscapeString(jcr, cr->Counter);
  if (!esc) { return false; }
  FillQuery<SQL_QUERY::update_counter_values>(cmd, cr->MinValue, cr->MaxValue,
                                              cr->CurrentValue, cr->WrapCounter,
                                              esc->c_str());
  bool retval = UpdateDb(jcr, cmd) > 0;

  return retval;
}

bool BareosDb::UpdatePoolRecord(JobControlRecord* jcr, PoolDbRecord* pr)
{
  char ed1[50], ed2[50], ed3[50], ed4[50], ed5[50], ed6[50];

  DbLocker _{this};
  auto esc = EscapeString(jcr, pr->LabelFormat);
  if (!esc) { return false; }

  Mmsg(cmd, "SELECT count(*) from Media WHERE PoolId=%s",
       edit_int64(pr->PoolId, ed4));
  pr->NumVols = GetSqlRecordMax(jcr);
  Dmsg1(400, "NumVols=%" PRIu32 "\n", pr->NumVols);

  Mmsg(cmd,
       "UPDATE Pool SET NumVols=%u,MaxVols=%u,UseOnce=%d,UseCatalog=%d,"
       "AcceptAnyVolume=%d,VolRetention='%s',VolUseDuration='%s',"
       "MaxVolJobs=%u,MaxVolFiles=%u,MaxVolBytes=%s,Recycle=%d,"
       "AutoPrune=%d,LabelType=%d,LabelFormat='%s',RecyclePoolId=%s,"
       "ScratchPoolId=%s,ActionOnPurge=%u,MinBlockSize=%u,MaxBlockSize=%u "
       "WHERE PoolId=%s",
       pr->NumVols, pr->MaxVols, pr->UseOnce, pr->UseCatalog,
       pr->AcceptAnyVolume, edit_uint64(pr->VolRetention, ed1),
       edit_uint64(pr->VolUseDuration, ed2), pr->MaxVolJobs, pr->MaxVolFiles,
       edit_uint64(pr->MaxVolBytes, ed3), pr->Recycle, pr->AutoPrune,
       pr->LabelType, esc->c_str(), edit_int64(pr->RecyclePoolId, ed5),
       edit_int64(pr->ScratchPoolId, ed6), pr->ActionOnPurge, pr->MinBlocksize,
       pr->MaxBlocksize, ed4);
  return UpdateDb(jcr, cmd) > 0;
}

bool BareosDb::UpdateStorageRecord(JobControlRecord* jcr, StorageDbRecord* sr)
{
  char ed1[50];

  DbLocker _{this};
  Mmsg(cmd, "UPDATE Storage SET AutoChanger=%d WHERE StorageId=%s",
       sr->AutoChanger, edit_int64(sr->StorageId, ed1));

  return UpdateDb(jcr, cmd) > 0;
}


/**
 * Update the Media Record at end of Session
 *
 * Returns: false on failure
 *          true on success
 */
bool BareosDb::UpdateMediaRecord(JobControlRecord* jcr,
                                 MediaDbRecord* mr,
                                 bool preserve_status_and_moved_pool,
                                 bool require_same_pool,
                                 bool* pool_changed,
                                 bool preserve_pool_defaults)
{
  char dt[MAX_TIME_LENGTH];
  time_t ttime;
  char ed1[50], ed3[50], ed4[50], ed5[50];
  char ed9[50], ed10[50], ed12[50];

  if (pool_changed) { *pool_changed = false; }
  Dmsg1(100, "update_media: FirstWritten=%" PRItime "\n", mr->FirstWritten);
  DbLocker _{this};
  auto esc_medianame = EscapeString(jcr, mr->VolumeName);
  if (!esc_medianame) { return false; }
  auto esc_status = EscapeString(jcr, mr->VolStatus);
  if (!esc_status) { return false; }

  // Interactive updates do not change write timestamps and must not perform
  // unguarded preliminary writes before checking the observed pool.
  if (!require_same_pool && mr->set_first_written) {
    Dmsg1(400, "Set FirstWritten Vol=%s\n", mr->VolumeName);
    ttime = mr->FirstWritten;
    bstrutime(dt, sizeof(dt), ttime);
    Mmsg(cmd,
         "UPDATE Media SET FirstWritten='%s' "
         "WHERE VolumeName='%s'",
         dt, esc_medianame->c_str());
    UpdateDb(jcr, cmd);
    Dmsg1(400, "Firstwritten=%" PRItime "\n", mr->FirstWritten);
  }

  /* Label just done? */
  if (!require_same_pool && mr->set_label_date) {
    ttime = mr->LabelDate;
    if (ttime == 0) { ttime = time(NULL); }
    bstrutime(dt, sizeof(dt), ttime);
    Mmsg(cmd,
         "UPDATE Media SET LabelDate='%s' "
         "WHERE VolumeName='%s'",
         dt, esc_medianame->c_str());
    UpdateDb(jcr, cmd);
  }

  if (!require_same_pool && mr->LastWritten != 0) {
    ttime = mr->LastWritten;
    bstrutime(dt, sizeof(dt), ttime);
    Mmsg(cmd,
         "UPDATE Media Set LastWritten='%s' "
         "WHERE VolumeName='%s'",
         dt, esc_medianame->c_str());
    UpdateDb(jcr, cmd);
  }

  // Compare against the pool in the retained record at update time, not at
  // read time: a pool move can commit between the read and this statement.
  const auto pool_value = [mr, preserve_status_and_moved_pool](
                              const char* column, auto value) {
          auto encoded = std::to_string(value);
          if (!preserve_status_and_moved_pool) { return encoded; }
          return "CASE WHEN PoolId=" + std::to_string(mr->PoolId) + " THEN "
                 + encoded + " ELSE " + column + " END";
        };

  std::string pool_default_assignments;
  if (!preserve_pool_defaults) {
    pool_default_assignments
        = "MaxVolBytes=" + pool_value("MaxVolBytes", mr->MaxVolBytes)
          + ",VolRetention=" + pool_value("VolRetention", mr->VolRetention)
          + ",VolUseDuration="
          + pool_value("VolUseDuration", mr->VolUseDuration)
          + ",MaxVolJobs=" + pool_value("MaxVolJobs", mr->MaxVolJobs)
          + ",MaxVolFiles=" + pool_value("MaxVolFiles", mr->MaxVolFiles)
          + ",RecyclePoolId="
          + pool_value("RecyclePoolId", mr->RecyclePoolId)
          + ",Recycle=" + pool_value("Recycle", mr->Recycle)
          + ",ActionOnPurge="
          + pool_value("ActionOnPurge", mr->ActionOnPurge)
          + ",MinBlocksize=" + pool_value("MinBlocksize", mr->MinBlocksize)
          + ",MaxBlocksize=" + pool_value("MaxBlocksize", mr->MaxBlocksize)
          + ",";
  }

  Mmsg(cmd,
       "UPDATE Media SET VolJobs=%u,"
       "VolFiles=%u,VolBlocks=%u,VolBytes=%s,VolMounts=%u,VolErrors=%u,"
       "VolWrites=%u,%sVolStatus=%s'%s'%s,"
       "Slot=%d,InChanger=%d,VolReadTime=%s,VolWriteTime=%s,"
       "LabelType=%d,StorageId=%s,PoolId=%s,Enabled=%d,LocationId=%s,"
       "ScratchPoolId=%s,RecycleCount=%u,"
       "EncryptionKey='%s' "
       "WHERE VolumeName='%s'%s%s%s",
       mr->VolJobs, mr->VolFiles, mr->VolBlocks, edit_uint64(mr->VolBytes, ed1),
       mr->VolMounts, mr->VolErrors, mr->VolWrites,
       pool_default_assignments.c_str(),
       preserve_status_and_moved_pool
           ? "CASE WHEN VolStatus IN ('Cleaning','Error') THEN VolStatus ELSE "
           : "",
       esc_status->c_str(), preserve_status_and_moved_pool ? " END" : "",
       mr->Slot, mr->InChanger, edit_int64(mr->VolReadTime, ed3),
       edit_int64(mr->VolWriteTime, ed4), mr->LabelType,
       edit_int64(mr->StorageId, ed5), pool_value("PoolId", mr->PoolId).c_str(),
       mr->Enabled, edit_uint64(mr->LocationId, ed9),
       edit_uint64(mr->ScratchPoolId, ed10), mr->RecycleCount, mr->EncrKey,
       esc_medianame->c_str(), require_same_pool ? " AND PoolId=" : "",
       require_same_pool ? edit_int64(mr->PoolId, ed12) : "",
       preserve_status_and_moved_pool
           ? " RETURNING VolStatus,PoolId,MaxVolBytes,VolRetention,"
             "VolUseDuration,MaxVolJobs,MaxVolFiles,RecyclePoolId,Recycle,"
             "ActionOnPurge,MinBlocksize,MaxBlocksize"
           : "");

  Dmsg1(400, "%s\n", cmd);

  const int affected = UpdateDb(jcr, cmd);
  bool retval = affected > 0;
  if (require_same_pool && affected == 0) {
    if (pool_changed) { *pool_changed = true; }
    Mmsg(errmsg, T_("Volume pool changed; refresh and try again.\n"));
  }
  if (retval && preserve_status_and_moved_pool) {
    SQL_ROW row = SqlFetchRow();
    if (row && row[0]) {
      bstrncpy(mr->VolStatus, row[0], sizeof(mr->VolStatus));
      mr->PoolId = str_to_int64(row[1]);
      mr->MaxVolBytes = str_to_uint64(row[2]);
      mr->VolRetention = str_to_uint64(row[3]);
      mr->VolUseDuration = str_to_uint64(row[4]);
      mr->MaxVolJobs = str_to_uint32(row[5]);
      mr->MaxVolFiles = str_to_uint32(row[6]);
      mr->RecyclePoolId = str_to_int64(row[7]);
      mr->Recycle = str_to_int32(row[8]);
      mr->ActionOnPurge = str_to_uint32(row[9]);
      mr->MinBlocksize = str_to_uint32(row[10]);
      mr->MaxBlocksize = str_to_uint32(row[11]);
    } else {
      Mmsg(errmsg, T_("Could not retrieve updated Volume status.\n"));
      retval = false;
    }
    SqlFreeResult();
  }

  // Make sure InChanger is 0 for any record having the same Slot
  if (retval || !require_same_pool) { retval &= MakeInchangerUnique(jcr, mr); }

  return retval;
}

bool BareosDb::UpdateMediaStatisticsRecord(JobControlRecord* jcr,
                                           MediaDbRecord* mr)
{
  return UpdateMediaRecord(jcr, mr, true, false, nullptr, true);
}

bool BareosDb::UpdateMediaSlotRecord(JobControlRecord* jcr, MediaDbRecord* mr)
{
  char ed1[50], ed2[50];

  DbLocker _{this};
  auto esc = EscapeString(jcr, mr->VolumeName);
  if (!esc) { return false; }

  Mmsg(cmd,
       "UPDATE Media SET Slot=%d,InChanger=%d,StorageId=%s "
       "WHERE VolumeName='%s' AND PoolId=%s",
       mr->Slot, mr->InChanger, edit_int64(mr->StorageId, ed1), esc->c_str(),
       edit_int64(mr->PoolId, ed2));

  const int affected = UpdateDb(jcr, cmd);
  if (affected == 0) {
    Mmsg(errmsg, T_("Volume pool changed; refresh and try again.\n"));
    return false;
  }
  if (affected < 0) { return false; }

  return MakeInchangerUnique(jcr, mr);
}

/**
 * Update the Media Record Default values from Pool
 *
 * Returns: false on failure
 *          true on success
 */
bool BareosDb::UpdateMediaDefaults(JobControlRecord* jcr,
                                   MediaDbRecord* mr,
                                   int64_t expected_pool_id)
{
  char ed1[50], ed2[50], ed3[50], ed4[50], ed5[50];
  const int64_t pool_id
      = expected_pool_id > 0 ? expected_pool_id : mr->PoolId;

  DbLocker _{this};
  if (mr->VolumeName[0]) {
    auto esc = EscapeString(jcr, mr->VolumeName);
    if (!esc) { return false; }
    Mmsg(cmd,
         "UPDATE Media SET "
         "ActionOnPurge=%u,Recycle=%d,VolRetention=%s,VolUseDuration=%s,"
         "MaxVolJobs=%u,MaxVolFiles=%u,MaxVolBytes=%s,RecyclePoolId=%s,"
         "MinBlocksize=%u,MaxBlocksize=%u "
         "WHERE VolumeName='%s' AND PoolId=%s",
         mr->ActionOnPurge, mr->Recycle, edit_uint64(mr->VolRetention, ed1),
         edit_uint64(mr->VolUseDuration, ed2), mr->MaxVolJobs, mr->MaxVolFiles,
         edit_uint64(mr->MaxVolBytes, ed3), edit_uint64(mr->RecyclePoolId, ed4),
         mr->MinBlocksize, mr->MaxBlocksize, esc->c_str(),
         edit_int64(pool_id, ed5));
  } else {
    Mmsg(cmd,
         "UPDATE Media SET "
         "ActionOnPurge=%u,Recycle=%d,VolRetention=%s,VolUseDuration=%s,"
         "MaxVolJobs=%u,MaxVolFiles=%u,MaxVolBytes=%s,RecyclePoolId=%s,"
         "MinBlocksize=%u,MaxBlocksize=%u "
         "WHERE PoolId=%s",
         mr->ActionOnPurge, mr->Recycle, edit_uint64(mr->VolRetention, ed1),
         edit_uint64(mr->VolUseDuration, ed2), mr->MaxVolJobs, mr->MaxVolFiles,
         edit_uint64(mr->MaxVolBytes, ed3), edit_int64(mr->RecyclePoolId, ed4),
         mr->MinBlocksize, mr->MaxBlocksize, edit_int64(mr->PoolId, ed5));
  }

  Dmsg1(400, "%s\n", cmd);

  const int affected = UpdateDb(jcr, cmd);
  if (affected == 0 && mr->VolumeName[0]) {
    Mmsg(errmsg, T_("Volume pool changed; refresh and try again.\n"));
    return false;
  }
  return affected != -1;
}


/**
 * If we have a non-zero InChanger, ensure that no other Media
 *  record has InChanger set on the same Slot.
 *
 * This routine assumes the database is already locked.
 */
bool BareosDb::MakeInchangerUnique(JobControlRecord* jcr, MediaDbRecord* mr)
{
  CheckOwnership();
  char ed1[50], ed2[50];
  if (mr->InChanger != 0 && mr->Slot != 0 && mr->StorageId != 0) {
    if (mr->MediaId != 0) {
      Mmsg(cmd,
           "UPDATE Media SET InChanger=0, Slot=0 WHERE "
           "Slot=%d AND StorageId=%s AND MediaId!=%s",
           mr->Slot, edit_int64(mr->StorageId, ed1),
           edit_int64(mr->MediaId, ed2));

    } else if (*mr->VolumeName) {
      auto esc = EscapeString(jcr, mr->VolumeName);
      if (!esc) { return false; }
      Mmsg(cmd,
           "UPDATE Media SET InChanger=0, Slot=0 WHERE "
           "Slot=%d AND StorageId=%s AND VolumeName!='%s'",
           mr->Slot, edit_int64(mr->StorageId, ed1), esc->c_str());

    } else { /* used by ua_label to reset all volume with this slot */
      Mmsg(cmd,
           "UPDATE Media SET InChanger=0, Slot=0 WHERE "
           "Slot=%d AND StorageId=%s",
           mr->Slot, edit_int64(mr->StorageId, ed1));
    }
    Dmsg1(100, "%s\n", cmd);
    return UpdateDb(jcr, cmd) >= 0;
  }

  return true;
}

/**
 * Update Quota record
 *
 * Returns: false on failure
 *          true on success
 */
bool BareosDb::UpdateQuotaGracetime(JobControlRecord* jcr, JobDbRecord* jr)
{
  char ed1[50], ed2[50];
  time_t now = time(NULL);

  DbLocker _{this};

  Mmsg(cmd, "UPDATE Quota SET GraceTime=%s WHERE ClientId='%s'",
       edit_uint64(now, ed1), edit_uint64(jr->ClientId, ed2));

  return UpdateDb(jcr, cmd) > 0;
}

/**
 * Update Quota Softlimit
 *
 * Returns: false on failure
 *          true on success
 */
bool BareosDb::UpdateQuotaSoftlimit(JobControlRecord* jcr, JobDbRecord* jr)
{
  char ed1[50], ed2[50];

  DbLocker _{this};

  Mmsg(cmd, "UPDATE Quota SET QuotaLimit=%s WHERE ClientId='%s'",
       edit_uint64((jr->JobSumTotalBytes + jr->JobBytes), ed1),
       edit_uint64(jr->ClientId, ed2));

  return UpdateDb(jcr, cmd) > 0;
}

/**
 * Reset Quota Gracetime
 *
 * Returns: false on failure
 *          true on success
 */
bool BareosDb::ResetQuotaRecord(JobControlRecord* jcr, ClientDbRecord* cr)
{
  char ed1[50];

  DbLocker _{this};

  Mmsg(cmd,
       "UPDATE Quota SET GraceTime='0', QuotaLimit='0' WHERE ClientId='%s'",
       edit_uint64(cr->ClientId, ed1));

  return UpdateDb(jcr, cmd) > 0;
}

/**
 * Update NDMP level mapping
 *
 * Returns: false on failure
 *          true on success
 */
bool BareosDb::UpdateNdmpLevelMapping(JobControlRecord* jcr,
                                      JobDbRecord* jr,
                                      char* filesystem,
                                      int level)
{
  char ed1[50], ed2[50], ed3[50];

  DbLocker _{this};

  auto esc_filesystem = EscapeString(jcr, filesystem);
  if (!esc_filesystem) { return false; }

  Mmsg(cmd,
       "UPDATE NDMPLevelMap SET DumpLevel='%s' WHERE "
       "ClientId='%s' AND FileSetId='%s' AND FileSystem='%s'",
       edit_uint64(level, ed1), edit_uint64(jr->ClientId, ed2),
       edit_uint64(jr->FileSetId, ed3), esc_filesystem->c_str());

  return UpdateDb(jcr, cmd) > 0;
}

/**
 * Change the type of the next copy job to backup.
 * We need to upgrade the next copy of a normal job,
 * and also upgrade the next copy when the normal job
 * already have been purged.
 *
 *   JobId: 1   PriorJobId: 0    (original)
 *   JobId: 2   PriorJobId: 1    (first copy)
 *   JobId: 3   PriorJobId: 1    (second copy)
 *
 *   JobId: 2   PriorJobId: 1    (first copy, now regular backup)
 *   JobId: 3   PriorJobId: 1    (second copy)
 *
 *  => Search through PriorJobId in jobid and
 *                    PriorJobId in PriorJobId (jobid)
 */
void BareosDb::UpgradeCopies(const char* jobids)
{
  PoolMem query(PM_MESSAGE);

  DbLocker _{this};

  /* Do it in two times for mysql */
  FillQuery<BareosDb::SQL_QUERY::uap_upgrade_copies_oldest_job>(
      query, JT_JOB_COPY, jobids, jobids);

  SqlQuery(query.c_str());

  /* Now upgrade first copy to Backup */
  Mmsg(query,
       "UPDATE Job SET Type='B' " /* JT_JOB_COPY => JT_BACKUP  */
       "WHERE JobId IN ( SELECT JobId FROM cpy_tmp )");

  SqlQuery(query.c_str());

  // remove the worker job that copied it
  Mmsg(query,
       "DELETE FROM Job "
       "WHERE Type='%c' "
       "AND priorjobid IN ( SELECT JobId FROM cpy_tmp )",
       JT_COPY);
  SqlQuery(query.c_str());

  SqlQuery("DROP TABLE IF EXISTS cpy_tmp");
}
