/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2012 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2016 Planets Communications B.V.
   Copyright (C) 2013-2025 Bareos GmbH & Co. KG

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
// Kern Sibbald, December 2000
/**
 * @file
 * BAREOS Catalog Database Find record interface routines
 *
 * Note, generally, these routines are more complicated
 * that a simple search by name or id. Such simple
 * request are in get.c
 */

#include "include/bareos.h"

#include "cats.h"
#include "lib/edit.h"

/* -----------------------------------------------------------------------
 *
 *   Generic Routines (or almost generic)
 *
 * -----------------------------------------------------------------------
 */

/**
 * Find job start time if JobId specified, otherwise
 * find last Job start time Incremental and Differential saves.
 *
 *  StartTime is returned in stime
 *  Job name is returned in job (MAX_NAME_LENGTH)
 *
 * Returns: false on failure
 *          true on success, jr is unchanged, but stime and job are set
 */
bool BareosDb::FindJobStartTime(JobControlRecord* jcr,
                                JobDbRecord* jr,
                                POOLMEM*& stime,
                                char* job)
{
  SQL_ROW row;
  char ed1[50], ed2[50];
  char esc_jobname[MAX_ESCAPE_NAME_LENGTH];

  DbLocker _{this};
  EscapeString(jcr, esc_jobname, jr->Name, strlen(jr->Name));
  PmStrcpy(stime, "0000-00-00 00:00:00"); /* default */
  job[0] = 0;

  /* If no Id given, we must find corresponding job */
  if (jr->JobId == 0) {
    /* Differential is since last Full backup */
    Mmsg(cmd,
         "SELECT StartTime, Job FROM Job WHERE JobStatus IN ('T','W') AND "
         "Type='%c' AND "
         "Level='%c' AND Name='%s' AND ClientId=%s AND FileSetId=%s "
         "ORDER BY StartTime DESC LIMIT 1",
         jr->JobType, L_FULL, esc_jobname, edit_int64(jr->ClientId, ed1),
         edit_int64(jr->FileSetId, ed2));

    if (jr->JobLevel == L_DIFFERENTIAL) {
      /* SQL cmd for Differential backup already edited above */

      /* Incremental is since last Full, Incremental, or Differential */
    } else if (jr->JobLevel == L_INCREMENTAL) {
      /* For an Incremental job, we must first ensure
       *  that a Full backup was done (cmd edited above)
       *  then we do a second look to find the most recent
       *  backup */
      if (!QueryDb(jcr, cmd)) {
        Mmsg2(errmsg,
              T_("Query error for start time request: ERR=%s\nCMD=%s\n"),
              sql_strerror(), cmd);
        return false;
      }
      if ((row = SqlFetchRow()) == NULL) {
        SqlFreeResult();
        Mmsg(errmsg, T_("No prior Full backup Job record found.\n"));
        return false;
      }
      SqlFreeResult();
      /* Now edit SQL command for Incremental Job */
      Mmsg(cmd,
           "SELECT StartTime, Job FROM Job WHERE JobStatus IN ('T','W') AND "
           "Type='%c' AND "
           "Level IN ('%c','%c','%c') AND Name='%s' AND ClientId=%s "
           "AND FileSetId=%s ORDER BY StartTime DESC LIMIT 1",
           jr->JobType, L_INCREMENTAL, L_DIFFERENTIAL, L_FULL, esc_jobname,
           edit_int64(jr->ClientId, ed1), edit_int64(jr->FileSetId, ed2));
    } else {
      Mmsg1(errmsg, T_("Unknown level=%d\n"), jr->JobLevel);
      return false;
    }
  } else {
    Dmsg1(100, "Submitting: %s\n", cmd);
    Mmsg(cmd, "SELECT StartTime, Job FROM Job WHERE Job.JobId=%s",
         edit_int64(jr->JobId, ed1));
  }

  if (!QueryDb(jcr, cmd)) {
    PmStrcpy(stime, ""); /* set EOS */
    Mmsg2(errmsg, T_("Query error for start time request: ERR=%s\nCMD=%s\n"),
          sql_strerror(), cmd);
    return false;
  }

  if ((row = SqlFetchRow()) == NULL) {
    Mmsg2(errmsg, T_("No Job record found: ERR=%s\nCMD=%s\n"), sql_strerror(),
          cmd);
    SqlFreeResult();
    return false;
  }
  Dmsg2(100, "Got start time: %s, job: %s\n", row[0], row[1]);
  PmStrcpy(stime, row[0]);
  bstrncpy(job, row[1], MAX_NAME_LENGTH);

  SqlFreeResult();

  return true;
}

/**
 * Find the last job start time for specified job and client.
 * Used in RunOnIncomingConnectInterval.
 *
 * The tricky part is that we can have a job in state 'C' not yet started
 * having a starttime of NULL.
 *
 * As NULL values are regarded as greater than any non-null value on sorting,
 * the job with the NULL value starttime would be returned by the query below.
 *
 * To avoid this, we use NOW()::timestamp so that a valid timestamp is returned
 * for jobs with NULL timestamp. Casting is required to remove the DST.
 */
BareosDb::SqlFindResult BareosDb::FindLastJobStartTimeForJobAndClient(
    JobControlRecord* jcr,
    std::string job_basename,
    std::string client_name,
    std::vector<char>& stime_out)
{
  std::vector<char> esc_jobname(MAX_ESCAPE_NAME_LENGTH);
  std::vector<char> esc_clientname(MAX_ESCAPE_NAME_LENGTH);

  DbLocker _{this};
  EscapeString(nullptr, esc_jobname.data(), job_basename.c_str(),
               job_basename.size());
  EscapeString(nullptr, esc_clientname.data(), client_name.c_str(),
               client_name.size());

  constexpr const char* default_time{"0000-00-00 00:00:00"};
  stime_out.resize(strlen(default_time) + 1);
  strcpy(stime_out.data(), default_time);

  Mmsg(cmd,
       "SELECT "
       " CASE WHEN StartTime is NULL THEN NOW()::timestamp "
       " ELSE StartTime::timestamp "
       "END"
       " FROM Job"
       " WHERE Job.Name='%s'"
       " AND Job.JobStatus IN ('T','W','C','R')"
       " AND Job.ClientId=(SELECT ClientId"
       "                   FROM Client WHERE Client.Name='%s')"
       " ORDER BY StartTime DESC LIMIT 1",
       esc_jobname.data(), esc_clientname.data());

  if (!QueryDb(jcr, cmd)) {
    Mmsg2(errmsg, T_("Query error for start time request: ERR=%s\nCMD=%s\n"),
          sql_strerror(), cmd);
    return SqlFindResult::kError;
  }

  SQL_ROW row;
  if ((row = SqlFetchRow()) == NULL) {
    Mmsg2(errmsg, T_("No Job record found: ERR=%s\nCMD=%s\n"), sql_strerror(),
          cmd);
    SqlFreeResult();
    return SqlFindResult::kEmptyResultSet;
  }

  Dmsg2(100, "Got start time: %s\n", row[0]);

  {
    std::size_t len = strlen(row[0]);
    stime_out.resize(len + 1);
    strcpy(stime_out.data(), row[0]);
  }

  SqlFreeResult();

  return SqlFindResult::kSuccess;
}

/**
 * Find the last job start time for the specified JobLevel
 *
 *  StartTime is returned in stime
 *  Job name is returned in job (MAX_NAME_LENGTH)
 *
 * Returns: false on failure
 *          true  on success, jr is unchanged, but stime and job are set
 */
bool BareosDb::FindLastJobStartTime(JobControlRecord* jcr,
                                    JobDbRecord* jr,
                                    POOLMEM*& stime,
                                    char* job,
                                    int JobLevel)
{
  SQL_ROW row;
  char ed1[50], ed2[50];
  char esc_jobname[MAX_ESCAPE_NAME_LENGTH];

  DbLocker _{this};
  EscapeString(jcr, esc_jobname, jr->Name, strlen(jr->Name));
  PmStrcpy(stime, "0000-00-00 00:00:00"); /* default */
  job[0] = 0;

  Mmsg(cmd,
       "SELECT StartTime, Job FROM Job WHERE JobStatus IN ('T','W') AND "
       "Type='%c' AND "
       "Level='%c' AND Name='%s' AND ClientId=%s AND FileSetId=%s "
       "ORDER BY StartTime DESC LIMIT 1",
       jr->JobType, JobLevel, esc_jobname, edit_int64(jr->ClientId, ed1),
       edit_int64(jr->FileSetId, ed2));
  if (!QueryDb(jcr, cmd)) {
    Mmsg2(errmsg, T_("Query error for start time request: ERR=%s\nCMD=%s\n"),
          sql_strerror(), cmd);
    return false;
  }
  if ((row = SqlFetchRow()) == NULL) {
    SqlFreeResult();
    Mmsg(errmsg, T_("No prior Full backup Job record found.\n"));
    return false;
  }
  Dmsg1(100, "Got start time: %s\n", row[0]);
  PmStrcpy(stime, row[0]);
  bstrncpy(job, row[1], MAX_NAME_LENGTH);

  SqlFreeResult();

  return true;
}

/**
 * Find last failed job since given start-time
 *   it must be either Full or Diff.
 *
 * Returns: false on failure
 *          true  on success, jr is unchanged and stime unchanged
 *                level returned in JobLevel
 */
bool BareosDb::FindFailedJobSince(JobControlRecord* jcr,
                                  JobDbRecord* jr,
                                  POOLMEM* stime,
                                  int& JobLevel)
{
  SQL_ROW row;
  char ed1[50], ed2[50];
  char esc_jobname[MAX_ESCAPE_NAME_LENGTH];

  DbLocker _{this};
  EscapeString(jcr, esc_jobname, jr->Name, strlen(jr->Name));

  /* Differential is since last Full backup */
  Mmsg(cmd,
       "SELECT Level FROM Job WHERE JobStatus NOT IN ('T','W') AND "
       "Type='%c' AND Level IN ('%c','%c') AND Name='%s' AND ClientId=%s "
       "AND FileSetId=%s AND StartTime>'%s' "
       "ORDER BY StartTime DESC LIMIT 1",
       jr->JobType, L_FULL, L_DIFFERENTIAL, esc_jobname,
       edit_int64(jr->ClientId, ed1), edit_int64(jr->FileSetId, ed2), stime);
  if (!QueryDb(jcr, cmd)) { return false; }

  if ((row = SqlFetchRow()) == NULL) {
    SqlFreeResult();
    return false;
  }
  JobLevel = (int)*row[0];
  SqlFreeResult();

  return true;
}

/**
 * Find JobId of last job that ran.  E.g. for
 *   VERIFY_CATALOG we want the JobId of the last INIT.
 *   For VERIFY_VOLUME_TO_CATALOG, we want the JobId of the last Job.
 *
 * Returns: true  on success
 *          false on failure
 */
bool BareosDb::FindLastJobid(JobControlRecord* jcr,
                             const char* Name,
                             JobDbRecord* jr)
{
  SQL_ROW row;
  char ed1[50];
  char esc_jobname[MAX_ESCAPE_NAME_LENGTH];

  DbLocker _{this};
  /* Find last full */
  Dmsg2(100, "JobLevel=%d JobType=%d\n", jr->JobLevel, jr->JobType);
  if (jr->JobLevel == L_VERIFY_CATALOG) {
    EscapeString(jcr, esc_jobname, jr->Name, strlen(jr->Name));
    Mmsg(cmd,
         "SELECT JobId FROM Job WHERE Type='V' AND Level='%c' AND "
         " JobStatus IN ('T','W') AND Name='%s' AND "
         "ClientId=%s ORDER BY StartTime DESC LIMIT 1",
         L_VERIFY_INIT, esc_jobname, edit_int64(jr->ClientId, ed1));
  } else if (jr->JobLevel == L_VERIFY_VOLUME_TO_CATALOG
             || jr->JobLevel == L_VERIFY_DISK_TO_CATALOG
             || jr->JobType == JT_BACKUP) {
    if (Name) {
      EscapeString(jcr, esc_jobname, (char*)Name,
                   MIN(strlen(Name), sizeof(esc_jobname)));
      Mmsg(
          cmd,
          "SELECT JobId FROM Job WHERE Type='B' AND JobStatus IN ('T','W') AND "
          "Name='%s' ORDER BY StartTime DESC LIMIT 1",
          esc_jobname);
    } else {
      Mmsg(
          cmd,
          "SELECT JobId FROM Job WHERE Type='B' AND JobStatus IN ('T','W') AND "
          "ClientId=%s ORDER BY StartTime DESC LIMIT 1",
          edit_int64(jr->ClientId, ed1));
    }
  } else {
    Mmsg1(errmsg, T_("Unknown Job level=%d\n"), jr->JobLevel);
    return false;
  }
  Dmsg1(100, "Query: %s\n", cmd);
  if (!QueryDb(jcr, cmd)) { return false; }
  if ((row = SqlFetchRow()) == NULL) {
    Mmsg1(errmsg, T_("No Job found for: %s.\n"), cmd);
    SqlFreeResult();
    return false;
  }

  jr->JobId = str_to_int64(row[0]);
  SqlFreeResult();

  Dmsg1(100, "db_get_last_jobid: got JobId=%d\n", jr->JobId);
  if (jr->JobId <= 0) {
    Mmsg1(errmsg, T_("No Job found for: %s\n"), cmd);
    return false;
  }

  return true;
}

/**
 * @brief BareosDb::FindJobById
 * @param jcr
 * @param id id of job to look for
 * @return returns true if job exists in db, false if not
 */
bool BareosDb::FindJobById(JobControlRecord* jcr, std::string id)
{
  DbLocker _{this};
  std::string query = "SELECT JobId FROM Job WHERE JobId=" + id;
  Dmsg1(100, "Query: %s\n", query.c_str());
  if (!QueryDb(jcr, query.c_str())) { return false; }
  if (SqlFetchRow() == NULL) {
    Mmsg1(errmsg, T_("No Job found with id: %s.\n"), id.c_str());
    SqlFreeResult();
    return false;
  } else {
    return true;
  }
}

/**
 * Search a comma separated list of unwanted volume names and see if given
 * VolumeName is on it.
 */
static inline bool is_on_unwanted_volumes_list(const char* VolumeName,
                                               const char* unwanted_volumes)
{
  bool retval = false;
  char *list_item, *bp, *unwanted_volumes_list;

  unwanted_volumes_list = strdup(unwanted_volumes);
  list_item = unwanted_volumes_list;
  while (list_item) {
    bp = strchr(list_item, ',');
    if (bp) { *bp++ = '\0'; }

    if (bstrcmp(VolumeName, list_item)) {
      retval = true;
      goto bail_out;
    }
    list_item = bp;
  }

bail_out:
  free(unwanted_volumes_list);
  return retval;
}

/**
 * Find Available Media (Volume) for Pool
 *
 * Find a Volume for a given PoolId, MediaType, and Status.
 * The unwanted_volumes variable lists the VolumeNames which we should skip if
 * any.
 *
 * Returns: 0 on failure
 *          numrows on success
 */
int BareosDb::FindNextVolume(JobControlRecord* jcr,
                             int item,
                             bool InChanger,
                             MediaDbRecord* mr,
                             const char* unwanted_volumes)
{
  char ed1[50];
  int num_rows = 0;
  SQL_ROW row = NULL;
  bool find_oldest = false;
  bool found_candidate = false;
  char esc_type[MAX_ESCAPE_NAME_LENGTH];
  char esc_status[MAX_ESCAPE_NAME_LENGTH];

  DbLocker _{this};

  EscapeString(jcr, esc_type, mr->MediaType, strlen(mr->MediaType));
  EscapeString(jcr, esc_status, mr->VolStatus, strlen(mr->VolStatus));

  if (item == -1) {
    find_oldest = true;
    item = 1;
  }

retry_fetch:
  if (find_oldest) {
    Mmsg(cmd,
         "SELECT MediaId,VolumeName,VolJobs,VolFiles,VolBlocks,"
         "VolBytes,VolMounts,VolErrors,VolWrites,MaxVolBytes,VolCapacityBytes,"
         "MediaType,VolStatus,PoolId,VolRetention,VolUseDuration,MaxVolJobs,"
         "MaxVolFiles,Recycle,Slot,FirstWritten,LastWritten,InChanger,"
         "EndFile,EndBlock,LabelType,LabelDate,StorageId,"
         "Enabled,LocationId,RecycleCount,InitialWrite,"
         "ScratchPoolId,RecyclePoolId,VolReadTime,VolWriteTime,"
         "ActionOnPurge,EncryptionKey,MinBlocksize,MaxBlocksize "
         "FROM Media WHERE PoolId=%s AND MediaType='%s' AND VolStatus IN "
         "('Unlabeled', 'Full',"
         "'Recycle','Purged','Used','Append') AND Enabled=1 "
         "ORDER BY LastWritten LIMIT %d",
         edit_int64(mr->PoolId, ed1), esc_type, item);
  } else {
    PoolMem changer(PM_MESSAGE);
    PoolMem order(PM_MESSAGE);

    // Find next available volume
    if (InChanger) {
      Mmsg(changer, "AND InChanger=1 AND StorageId=%s",
           edit_int64(mr->StorageId, ed1));
    }

    if (bstrcmp(mr->VolStatus, "Recycle") || bstrcmp(mr->VolStatus, "Purged")) {
      PmStrcpy(order,
               "AND Recycle=1 ORDER BY LastWritten ASC,MediaId"); /* Take oldest
                                                                     that can be
                                                                     recycled */
    } else {
      FillQuery(
          order,
          SQL_QUERY::sql_media_order_most_recently_written); /* Take
                                                               most recently
                                                               written */
    }

    Mmsg(cmd,
         "SELECT MediaId,VolumeName,VolJobs,VolFiles,VolBlocks,"
         "VolBytes,VolMounts,VolErrors,VolWrites,MaxVolBytes,VolCapacityBytes,"
         "MediaType,VolStatus,PoolId,VolRetention,VolUseDuration,MaxVolJobs,"
         "MaxVolFiles,Recycle,Slot,FirstWritten,LastWritten,InChanger,"
         "EndFile,EndBlock,LabelType,LabelDate,StorageId,"
         "Enabled,LocationId,RecycleCount,InitialWrite,"
         "ScratchPoolId,RecyclePoolId,VolReadTime,VolWriteTime,"
         "ActionOnPurge,EncryptionKey,MinBlocksize,MaxBlocksize "
         "FROM Media WHERE PoolId=%s AND MediaType='%s' AND Enabled=1 "
         "AND VolStatus='%s' "
         "%s "
         "%s LIMIT %d",
         edit_int64(mr->PoolId, ed1), esc_type, esc_status, changer.c_str(),
         order.c_str(), item);
  }

  Dmsg1(100, "fnextvol=%s\n", cmd);
  if (!QueryDb(jcr, cmd)) {
    Dmsg1(050, "Rtn numrows=%d\n", num_rows);
    return num_rows;
  }

  num_rows = SqlNumRows();
  if (item > num_rows || item < 1) {
    Dmsg2(050, "item=%d got=%d\n", item, num_rows);
    Mmsg2(errmsg,
          T_("Request for Volume item %d greater than max %d or less than 1\n"),
          item, num_rows);
    num_rows = 0;
    Dmsg1(050, "Rtn numrows=%d\n", num_rows);
    return num_rows;
  }

  for (int i = 0; i < item; i++) {
    if ((row = SqlFetchRow()) == NULL) {
      Dmsg1(050, "Fail fetch item=%d\n", i);
      Mmsg1(errmsg, T_("No Volume record found for item %d.\n"), i);
      SqlFreeResult();
      num_rows = 0;
      Dmsg1(050, "Rtn numrows=%d\n", num_rows);
      return num_rows;
    }

    /* Skip if volume is on unwanted volumes list.
     * We need to reduce the number of rows by one
     * and jump out if no more rows are available */
    if (unwanted_volumes
        && is_on_unwanted_volumes_list(row[1], unwanted_volumes)) {
      Dmsg1(50, "Volume %s is on unwanted_volume_list, skipping\n", row[1]);
      num_rows--;
      if (num_rows <= 0) {
        Dmsg1(50, "No more volumes in result, bailing out\n");
        SqlFreeResult();
        Dmsg1(050, "Rtn numrows=%d\n", num_rows);
        return num_rows;
      }
      continue;
    }

    // Return fields in Media Record
    mr->MediaId = str_to_int64(row[0]);
    bstrncpy(mr->VolumeName, (row[1] != NULL) ? row[1] : "",
             sizeof(mr->VolumeName));
    mr->VolJobs = str_to_int64(row[2]);
    mr->VolFiles = str_to_int64(row[3]);
    mr->VolBlocks = str_to_int64(row[4]);
    mr->VolBytes = str_to_uint64(row[5]);
    mr->VolMounts = str_to_int64(row[6]);
    mr->VolErrors = str_to_int64(row[7]);
    mr->VolWrites = str_to_int64(row[8]);
    mr->MaxVolBytes = str_to_uint64(row[9]);
    mr->VolCapacityBytes = str_to_uint64(row[10]);
    bstrncpy(mr->MediaType, (row[11] != NULL) ? row[11] : "",
             sizeof(mr->MediaType));
    bstrncpy(mr->VolStatus, (row[12] != NULL) ? row[12] : "",
             sizeof(mr->VolStatus));
    mr->PoolId = str_to_int64(row[13]);
    mr->VolRetention = str_to_uint64(row[14]);
    mr->VolUseDuration = str_to_uint64(row[15]);
    mr->MaxVolJobs = str_to_int64(row[16]);
    mr->MaxVolFiles = str_to_int64(row[17]);
    mr->Recycle = str_to_int64(row[18]);
    mr->Slot = str_to_int64(row[19]);
    bstrncpy(mr->cFirstWritten, (row[20] != NULL) ? row[20] : "",
             sizeof(mr->cFirstWritten));
    mr->FirstWritten = (time_t)StrToUtime(mr->cFirstWritten);
    bstrncpy(mr->cLastWritten, (row[21] != NULL) ? row[21] : "",
             sizeof(mr->cLastWritten));
    mr->LastWritten = (time_t)StrToUtime(mr->cLastWritten);
    mr->InChanger = str_to_uint64(row[22]);
    mr->EndFile = str_to_uint64(row[23]);
    mr->EndBlock = str_to_uint64(row[24]);
    mr->LabelType = str_to_int64(row[25]);
    bstrncpy(mr->cLabelDate, (row[26] != NULL) ? row[26] : "",
             sizeof(mr->cLabelDate));
    mr->LabelDate = (time_t)StrToUtime(mr->cLabelDate);
    mr->StorageId = str_to_int64(row[27]);
    mr->Enabled = str_to_int64(row[28]);
    mr->LocationId = str_to_int64(row[29]);
    mr->RecycleCount = str_to_int64(row[30]);
    bstrncpy(mr->cInitialWrite, (row[31] != NULL) ? row[31] : "",
             sizeof(mr->cInitialWrite));
    mr->InitialWrite = (time_t)StrToUtime(mr->cInitialWrite);
    mr->ScratchPoolId = str_to_int64(row[32]);
    mr->RecyclePoolId = str_to_int64(row[33]);
    mr->VolReadTime = str_to_int64(row[34]);
    mr->VolWriteTime = str_to_int64(row[35]);
    mr->ActionOnPurge = str_to_int64(row[36]);
    bstrncpy(mr->EncrKey, (row[37] != NULL) ? row[37] : "",
             sizeof(mr->EncrKey));
    mr->MinBlocksize = str_to_int32(row[38]);
    mr->MaxBlocksize = str_to_int32(row[39]);

    SqlFreeResult();
    found_candidate = true;
    break;
  }

  if (!found_candidate && find_oldest) {
    item++;
    goto retry_fetch;
  }

  Dmsg1(050, "Rtn numrows=%d\n", num_rows);
  return num_rows;
}
