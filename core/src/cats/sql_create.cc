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
// Kern Sibbald, March 2000
/**
 * @file
 * BAREOS Catalog Database Create record interface routines
 */

#include "include/bareos.h"
#include "include/filetypes.h"
#include "include/streams.h"

static const int dbglevel = 100;

#include "cats.h"
#include "lib/edit.h"

/* -----------------------------------------------------------------------
 *
 *   Generic Routines (or almost generic)
 *
 * -----------------------------------------------------------------------
 */


/**
 * Create a new record for the Job
 * Returns: false on failure
 *          true  on success
 */
bool BareosDb::CreateJobRecord(JobControlRecord* jcr, JobDbRecord* jr)
{
  PoolMem buf;
  char dt[MAX_TIME_LENGTH];
  time_t stime;
  int len;
  utime_t JobTDate;
  char ed1[30], ed2[30];
  char esc_ujobname[MAX_ESCAPE_NAME_LENGTH];
  char esc_jobname[MAX_ESCAPE_NAME_LENGTH];

  DbLocker _{this};

  stime = jr->SchedTime;
  ASSERT(stime != 0);

  bstrutime(dt, sizeof(dt), stime);
  JobTDate = (utime_t)stime;

  len = strlen(jcr->comment); /* TODO: use jr instead of jcr to get comment */
  buf.check_size(len * 2 + 1);

  EscapeString(jcr, buf.c_str(), jcr->comment, len);
  EscapeString(jcr, esc_ujobname, jr->Job, strlen(jr->Job));
  EscapeString(jcr, esc_jobname, jr->Name, strlen(jr->Name));

  /* clang-format off */
  Mmsg(cmd,
       "INSERT INTO Job (Job,Name,Type,Level,JobStatus,SchedTime,JobTDate,"
       "ClientId,Comment) "
       "VALUES ('%s','%s','%c','%c','%c','%s',%s,%s,'%s')",
       esc_ujobname, esc_jobname, (char)(jr->JobType), (char)(jr->JobLevel),
       (char)(jr->JobStatus), dt, edit_uint64(JobTDate, ed1),
       edit_int64(jr->ClientId, ed2), buf.c_str());
  /* clang-format on */

  jr->JobId = SqlInsertAutokeyRecord(cmd, NT_("Job"));
  if (jr->JobId == 0) {
    Mmsg2(errmsg, T_("Create DB Job record %s failed. ERR=%s\n"), cmd,
          sql_strerror());
  } else {
    return true;
  }

  return false;
}

/**
 * Create a JobMedia record for medium used this job
 * Returns: false on failure
 *          true  on success
 */
bool BareosDb::CreateJobmediaRecord(JobControlRecord* jcr, JobMediaDbRecord* jm)
{
  DbLocker _{this};

  Mmsg(cmd, "SELECT count(*) from JobMedia WHERE JobId=%" PRIu32, jm->JobId);
  int count = GetSqlRecordMax(jcr);
  if (count < 0) { count = 0; }
  count++;

  /* clang-format off */
  Mmsg(cmd,
       "INSERT INTO JobMedia (JobId,MediaId,FirstIndex,LastIndex,"
       "StartFile,EndFile,StartBlock,EndBlock,VolIndex,JobBytes) "
       "VALUES (%" PRIu32 ",%" PRIdbid ",%" PRIu32 ",%" PRIu32 ",%" PRIu32 ",%" PRIu32 ",%" PRIu32 ",%" PRIu32 ",%" PRIu32 ",%" PRIu64 ")",
       jm->JobId,
       jm->MediaId,
       jm->FirstIndex, jm->LastIndex,
       jm->StartFile, jm->EndFile,
       jm->StartBlock, jm->EndBlock,
       count,
       jm->JobBytes);
  /* clang-format on */

  Dmsg0(300, "%s", cmd);
  if (InsertDb(jcr, cmd) != 1) {
    Mmsg2(errmsg, T_("Create JobMedia record %s failed: ERR=%s\n"), cmd,
          sql_strerror());
  } else {
    // Worked, now update the Media record with the EndFile and EndBlock
    Mmsg(cmd,
         "UPDATE Media SET EndFile=%" PRIu32 ", EndBlock=%" PRIu32
         " WHERE MediaId=%" PRIdbid,
         jm->EndFile, jm->EndBlock, jm->MediaId);
    if (UpdateDb(jcr, cmd) == -1) {
      Mmsg2(errmsg, T_("Update Media record %s failed: ERR=%s\n"), cmd,
            sql_strerror());
    } else {
      return true;
    }
  }

  return false;
}

/**
 * Create Unique Pool record
 * Returns: false on failure
 *          true  on success
 */
bool BareosDb::CreatePoolRecord(JobControlRecord* jcr, PoolDbRecord* pr)
{
  bool retval = false;
  char ed1[30], ed2[30], ed3[50], ed4[50], ed5[50];
  char esc_poolname[MAX_ESCAPE_NAME_LENGTH];
  char esc_lf[MAX_ESCAPE_NAME_LENGTH];
  int num_rows;

  Dmsg0(200, "In create pool\n");
  DbLocker _{this};
  EscapeString(jcr, esc_poolname, pr->Name, strlen(pr->Name));
  EscapeString(jcr, esc_lf, pr->LabelFormat, strlen(pr->LabelFormat));
  Mmsg(cmd, "SELECT PoolId,Name FROM Pool WHERE Name='%s'", esc_poolname);
  Dmsg1(200, "selectpool: %s\n", cmd);

  if (QueryDb(jcr, cmd)) {
    num_rows = SqlNumRows();
    if (num_rows > 0) {
      Mmsg1(errmsg, T_("pool record %s already exists\n"), pr->Name);
      SqlFreeResult();
      Dmsg0(500, "Create Pool: done\n");
      return retval;
    }
    SqlFreeResult();
  }

  /* clang-format off */
  Mmsg(cmd,
       "INSERT INTO Pool (Name,NumVols,MaxVols,UseOnce,UseCatalog,"
       "AcceptAnyVolume,AutoPrune,Recycle,VolRetention,VolUseDuration,"
       "MaxVolJobs,MaxVolFiles,MaxVolBytes,PoolType,LabelType,LabelFormat,"
       "RecyclePoolId,ScratchPoolId,ActionOnPurge,MinBlocksize,MaxBlocksize) "
       "VALUES ('%s',%u,%u,%d,%d,%d,%d,%d,%s,%s,%u,%u,%s,'%s',%d,'%s',%s,%s,%d,%d,%d)",
       esc_poolname,
       pr->NumVols, pr->MaxVols,
       pr->UseOnce, pr->UseCatalog,
       pr->AcceptAnyVolume,
       pr->AutoPrune, pr->Recycle,
       edit_uint64(pr->VolRetention, ed1),
       edit_uint64(pr->VolUseDuration, ed2),
       pr->MaxVolJobs, pr->MaxVolFiles,
       edit_uint64(pr->MaxVolBytes, ed3),
       pr->PoolType, pr->LabelType, esc_lf,
       edit_int64(pr->RecyclePoolId,ed4),
       edit_int64(pr->ScratchPoolId,ed5),
       pr->ActionOnPurge,
       pr->MinBlocksize,
       pr->MaxBlocksize);
  /* clang-format on */

  Dmsg1(200, "Create Pool: %s\n", cmd);
  pr->PoolId = SqlInsertAutokeyRecord(cmd, NT_("Pool"));
  if (pr->PoolId == 0) {
    Mmsg2(errmsg, T_("Create db Pool record %s failed: ERR=%s\n"), cmd,
          sql_strerror());
  } else {
    retval = true;
  }

  Dmsg0(500, "Create Pool: done\n");
  return retval;
}

/**
 * Create Unique Device record
 * Returns: false on failure
 *          true  on success
 */
bool BareosDb::CreateDeviceRecord(JobControlRecord* jcr, DeviceDbRecord* dr)
{
  SQL_ROW row;
  char ed1[30], ed2[30];
  char esc[MAX_ESCAPE_NAME_LENGTH];
  int num_rows;

  Dmsg0(200, "In create Device\n");
  DbLocker _{this};
  EscapeString(jcr, esc, dr->Name, strlen(dr->Name));
  Mmsg(cmd,
       "SELECT DeviceId,Name FROM Device WHERE Name='%s' AND StorageId = %s",
       esc, edit_int64(dr->StorageId, ed1));
  Dmsg1(200, "selectdevice: %s\n", cmd);

  if (QueryDb(jcr, cmd)) {
    num_rows = SqlNumRows();

    if (num_rows > 1) {
      Mmsg1(errmsg, T_("More than one Device!: %d\n"), num_rows);
      Jmsg(jcr, M_ERROR, 0, "%s", errmsg);
    }
    if (num_rows >= 1) {
      if ((row = SqlFetchRow()) == NULL) {
        Mmsg1(errmsg, T_("error fetching Device row: %s\n"), sql_strerror());
        Jmsg(jcr, M_ERROR, 0, "%s", errmsg);
        SqlFreeResult();
        return false;
      }
      dr->DeviceId = str_to_int64(row[0]);
      if (row[1]) {
        bstrncpy(dr->Name, row[1], sizeof(dr->Name));
      } else {
        dr->Name[0] = 0; /* no name */
      }
      SqlFreeResult();
      return true;
    }
    SqlFreeResult();
  }

  Mmsg(cmd,
       "INSERT INTO Device (Name,MediaTypeId,StorageId) VALUES ('%s',%s,%s)",
       esc, edit_uint64(dr->MediaTypeId, ed1), edit_int64(dr->StorageId, ed2));
  Dmsg1(200, "Create Device: %s\n", cmd);
  dr->DeviceId = SqlInsertAutokeyRecord(cmd, NT_("Device"));
  if (dr->DeviceId == 0) {
    Mmsg2(errmsg, T_("Create db Device record %s failed: ERR=%s\n"), cmd,
          sql_strerror());
  } else {
    return true;
  }

  return false;
}

/**
 * Create a Unique record for Storage -- no duplicates
 * Returns: false on failure
 *          true  on success with id in sr->StorageId
 */
bool BareosDb::CreateStorageRecord(JobControlRecord* jcr, StorageDbRecord* sr)
{
  SQL_ROW row;
  int num_rows;
  char esc[MAX_ESCAPE_NAME_LENGTH];

  DbLocker _{this};
  EscapeString(jcr, esc, sr->Name, strlen(sr->Name));
  Mmsg(cmd, "SELECT StorageId,AutoChanger FROM Storage WHERE Name='%s'", esc);

  sr->StorageId = 0;
  sr->created = false;
  // Check if it already exists
  if (QueryDb(jcr, cmd)) {
    num_rows = SqlNumRows();
    if (num_rows > 1) {
      Mmsg1(errmsg, T_("More than one Storage record!: %d\n"), num_rows);
      Jmsg(jcr, M_ERROR, 0, "%s", errmsg);
    }
    if (num_rows >= 1) {
      if ((row = SqlFetchRow()) == NULL) {
        Mmsg1(errmsg, T_("error fetching Storage row: %s\n"), sql_strerror());
        Jmsg(jcr, M_ERROR, 0, "%s", errmsg);
        SqlFreeResult();
        return false;
      }
      sr->StorageId = str_to_int64(row[0]);
      sr->AutoChanger = atoi(row[1]); /* bool */
      SqlFreeResult();
      return true;
    }
    SqlFreeResult();
  }

  Mmsg(cmd,
       "INSERT INTO Storage (Name,AutoChanger)"
       " VALUES ('%s',%d)",
       esc, sr->AutoChanger);

  sr->StorageId = SqlInsertAutokeyRecord(cmd, NT_("Storage"));
  if (sr->StorageId == 0) {
    Mmsg2(errmsg, T_("Create DB Storage record %s failed. ERR=%s\n"), cmd,
          sql_strerror());
    Jmsg(jcr, M_ERROR, 0, "%s", errmsg);
  } else {
    sr->created = true;
    return true;
  }

  return false;
}

/**
 * Create Unique MediaType record
 * Returns: false on failure
 *          true  on success
 */
bool BareosDb::CreateMediatypeRecord(JobControlRecord* jcr,
                                     MediaTypeDbRecord* mr)
{
  int num_rows;
  char esc[MAX_ESCAPE_NAME_LENGTH];

  Dmsg0(200, "In create mediatype\n");
  DbLocker _{this};
  EscapeString(jcr, esc, mr->MediaType, strlen(mr->MediaType));
  Mmsg(cmd, "SELECT MediaTypeId,MediaType FROM MediaType WHERE MediaType='%s'",
       esc);
  Dmsg1(200, "selectmediatype: %s\n", cmd);

  if (QueryDb(jcr, cmd)) {
    num_rows = SqlNumRows();
    if (num_rows > 0) {
      Mmsg1(errmsg, T_("mediatype record %s already exists\n"), mr->MediaType);
      SqlFreeResult();
      return false;
    }
    SqlFreeResult();
  }

  /* clang-format off */
  Mmsg(cmd,
       "INSERT INTO MediaType (MediaType,ReadOnly) "
       "VALUES ('%s',%d)",
       mr->MediaType,
       mr->ReadOnly);
  /* clang-format on */

  Dmsg1(200, "Create mediatype: %s\n", cmd);
  mr->MediaTypeId = SqlInsertAutokeyRecord(cmd, NT_("MediaType"));
  if (mr->MediaTypeId == 0) {
    Mmsg2(errmsg, T_("Create db mediatype record %s failed: ERR=%s\n"), cmd,
          sql_strerror());
    return false;
  } else {
    return true;
  }
}

/**
 * Create Media record. VolumeName and non-zero Slot must be unique
 * Returns: false on failure
 *          true on success with id in mr->MediaId
 */
bool BareosDb::CreateMediaRecord(JobControlRecord* jcr, MediaDbRecord* mr)
{
  bool retval = false;
  char ed1[50], ed2[50], ed3[50], ed4[50], ed5[50], ed6[50], ed7[50], ed8[50];
  char ed9[50], ed10[50], ed11[50], ed12[50];
  int num_rows;
  char esc_medianame[MAX_ESCAPE_NAME_LENGTH];
  char esc_mtype[MAX_ESCAPE_NAME_LENGTH];
  char esc_status[MAX_ESCAPE_NAME_LENGTH];

  DbLocker _{this};
  EscapeString(jcr, esc_medianame, mr->VolumeName, strlen(mr->VolumeName));
  EscapeString(jcr, esc_mtype, mr->MediaType, strlen(mr->MediaType));
  EscapeString(jcr, esc_status, mr->VolStatus, strlen(mr->VolStatus));

  Mmsg(cmd, "SELECT MediaId FROM Media WHERE VolumeName='%s'", esc_medianame);
  Dmsg1(500, "selectpool: %s\n", cmd);

  if (QueryDb(jcr, cmd)) {
    num_rows = SqlNumRows();
    if (num_rows > 0) {
      Mmsg1(errmsg, T_("Volume \"%s\" already exists.\n"), mr->VolumeName);
      SqlFreeResult();
      return retval;
    }
    SqlFreeResult();
  }

  /* clang-format off */
  Mmsg(cmd,
       "INSERT INTO Media (VolumeName,MediaType,MediaTypeId,PoolId,MaxVolBytes,"
       "VolCapacityBytes,Recycle,VolRetention,VolUseDuration,MaxVolJobs,MaxVolFiles,"
       "VolStatus,Slot,VolBytes,InChanger,VolReadTime,VolWriteTime,"
       "EndFile,EndBlock,LabelType,StorageId,DeviceId,LocationId,"
       "ScratchPoolId,RecyclePoolId,Enabled,ActionOnPurge,EncryptionKey,"
       "MinBlocksize,MaxBlocksize,VolFiles) "
       "VALUES ('%s','%s',0,%u,%s,%s,%d,%s,%s,%u,%u,'%s',%d,%s,%d,%s,%s,0,0,%d,%s,"
       "%s,%s,%s,%s,%d,%d,'%s',%d,%d,%d)",
       esc_medianame,
       esc_mtype, mr->PoolId,
       edit_uint64(mr->MaxVolBytes,ed1),
       edit_uint64(mr->VolCapacityBytes, ed2),
       mr->Recycle,
       edit_uint64(mr->VolRetention, ed3),
       edit_uint64(mr->VolUseDuration, ed4),
       mr->MaxVolJobs,
       mr->MaxVolFiles,
       esc_status,
       mr->Slot,
       edit_uint64(mr->VolBytes, ed5),
       mr->InChanger,
       edit_int64(mr->VolReadTime, ed6),
       edit_int64(mr->VolWriteTime, ed7),
       mr->LabelType,
       edit_int64(mr->StorageId, ed8),
       edit_int64(mr->DeviceId, ed9),
       edit_int64(mr->LocationId, ed10),
       edit_int64(mr->ScratchPoolId, ed11),
       edit_int64(mr->RecyclePoolId, ed12),
       mr->Enabled, mr->ActionOnPurge,
       mr->EncrKey, mr->MinBlocksize,
       mr->MaxBlocksize, mr->VolFiles);
  /* clang-format on */

  Dmsg1(500, "Create Volume: %s\n", cmd);
  mr->MediaId = SqlInsertAutokeyRecord(cmd, NT_("Media"));
  if (mr->MediaId == 0) {
    Mmsg2(errmsg, T_("Create DB Media record %s failed. ERR=%s\n"), cmd,
          sql_strerror());
  } else {
    retval = true;
    if (mr->set_label_date) {
      char dt[MAX_TIME_LENGTH];
      if (mr->LabelDate == 0) { mr->LabelDate = time(NULL); }

      bstrutime(dt, sizeof(dt), mr->LabelDate);
      Mmsg(cmd,
           "UPDATE Media SET LabelDate='%s' "
           "WHERE MediaId=%d",
           dt, mr->MediaId);
      retval = UpdateDb(jcr, cmd) > 0;
    }
    /* Make sure that if InChanger is non-zero any other identical slot
     * has InChanger zero. */
    MakeInchangerUnique(jcr, mr);
  }

  return retval;
}

/**
 * Create a Unique record for the client -- no duplicates
 * Returns: false on failure
 *          true on success with id in cr->ClientId
 */
bool BareosDb::CreateClientRecord(JobControlRecord* jcr, ClientDbRecord* cr)
{
  SQL_ROW row;
  char ed1[50], ed2[50];
  int num_rows;
  char esc_clientname[MAX_ESCAPE_NAME_LENGTH];
  char esc_uname[MAX_ESCAPE_NAME_LENGTH];

  DbLocker _{this};
  EscapeString(jcr, esc_clientname, cr->Name, strlen(cr->Name));
  EscapeString(jcr, esc_uname, cr->Uname, strlen(cr->Uname));
  Mmsg(cmd, "SELECT ClientId,Uname FROM Client WHERE Name='%s'",
       esc_clientname);

  cr->ClientId = 0;
  if (QueryDb(jcr, cmd)) {
    num_rows = SqlNumRows();
    if (num_rows > 1) {
      Mmsg1(errmsg, T_("More than one Client!: %d\n"), num_rows);
      Jmsg(jcr, M_ERROR, 0, "%s", errmsg);
    }
    if (num_rows >= 1) {
      if ((row = SqlFetchRow()) == NULL) {
        Mmsg1(errmsg, T_("error fetching Client row: %s\n"), sql_strerror());
        Jmsg(jcr, M_ERROR, 0, "%s", errmsg);
        SqlFreeResult();
        return false;
      }
      cr->ClientId = str_to_int64(row[0]);
      if (row[1]) {
        bstrncpy(cr->Uname, row[1], sizeof(cr->Uname));
      } else {
        cr->Uname[0] = 0; /* no name */
      }
      SqlFreeResult();
      return true;
    }
    SqlFreeResult();
  }

  /* clang-format off */
  Mmsg(cmd,
       "INSERT INTO Client (Name,Uname,AutoPrune,"
       "FileRetention,JobRetention) VALUES "
       "('%s','%s',%d,%s,%s)",
       esc_clientname, esc_uname, cr->AutoPrune,
       edit_uint64(cr->FileRetention, ed1),
       edit_uint64(cr->JobRetention, ed2));
  /* clang-format on */

  cr->ClientId = SqlInsertAutokeyRecord(cmd, NT_("Client"));
  if (cr->ClientId == 0) {
    Mmsg2(errmsg, T_("Create DB Client record %s failed. ERR=%s\n"), cmd,
          sql_strerror());
    Jmsg(jcr, M_ERROR, 0, "%s", errmsg);
  } else {
    return true;
  }

  return false;
}

/**
 * Create a Unique record for the Path -- no duplicates
 * Returns: false on failure
 *          true on success with id in cr->ClientId
 */
bool BareosDb::CreatePathRecord(JobControlRecord* jcr, AttributesDbRecord* ar)
{
  bool retval = false;
  SQL_ROW row;
  int num_rows;

  errmsg[0] = 0;
  esc_name = CheckPoolMemorySize(esc_name, 2 * pnl + 2);
  EscapeString(jcr, esc_name, path, pnl);

  if (cached_path_id != 0 && cached_path_len == pnl
      && bstrcmp(cached_path, path)) {
    ar->PathId = cached_path_id;
    return true;
  }

  Mmsg(cmd, "SELECT PathId FROM Path WHERE Path='%s'", esc_name);

  if (QueryDb(jcr, cmd)) {
    num_rows = SqlNumRows();
    if (num_rows > 1) {
      char ed1[30];
      Mmsg2(errmsg, T_("More than one Path!: %s for path: %s\n"),
            edit_uint64(num_rows, ed1), path);
      Jmsg(jcr, M_WARNING, 0, "%s", errmsg);
    }
    // Even if there are multiple paths, take the first one
    if (num_rows >= 1) {
      if ((row = SqlFetchRow()) == NULL) {
        Mmsg1(errmsg, T_("error fetching row: %s\n"), sql_strerror());
        Jmsg(jcr, M_ERROR, 0, "%s", errmsg);
        SqlFreeResult();
        ar->PathId = 0;
        ASSERT(ar->PathId);
        goto bail_out;
      }
      ar->PathId = str_to_int64(row[0]);
      SqlFreeResult();
      if (ar->PathId != cached_path_id) {
        cached_path_id = ar->PathId;
        cached_path_len = pnl;
        PmStrcpy(cached_path, path);
      }
      ASSERT(ar->PathId);
      retval = true;
      goto bail_out;
    }
    SqlFreeResult();
  }

  Mmsg(cmd, "INSERT INTO Path (Path) VALUES ('%s')", esc_name);

  ar->PathId = SqlInsertAutokeyRecord(cmd, NT_("Path"));
  if (ar->PathId == 0) {
    Mmsg2(errmsg, T_("Create db Path record %s failed. ERR=%s\n"), cmd,
          sql_strerror());
    Jmsg(jcr, M_FATAL, 0, "%s", errmsg);
    ar->PathId = 0;
    goto bail_out;
  }

  if (ar->PathId != cached_path_id) {
    cached_path_id = ar->PathId;
    cached_path_len = pnl;
    PmStrcpy(cached_path, path);
  }
  retval = true;

bail_out:
  return retval;
}

/**
 * Create a Unique record for the counter -- no duplicates
 * Returns: false on failure
 *          true on success with counter filled in
 */
bool BareosDb::CreateCounterRecord(JobControlRecord* jcr, CounterDbRecord* cr)
{
  char esc[MAX_ESCAPE_NAME_LENGTH];
  CounterDbRecord mcr;

  DbLocker _{this};
  bstrncpy(mcr.Counter, cr->Counter, sizeof(mcr.Counter));
  if (GetCounterRecord(jcr, &mcr)) {
    memcpy(cr, &mcr, sizeof(CounterDbRecord));
    return true;
  }
  EscapeString(jcr, esc, cr->Counter, strlen(cr->Counter));

  FillQuery(SQL_QUERY::insert_counter_values, esc, cr->MinValue, cr->MaxValue,
            cr->CurrentValue, cr->WrapCounter);

  if (InsertDb(jcr, cmd) != 1) {
    Mmsg2(errmsg, T_("Create DB Counters record %s failed. ERR=%s\n"), cmd,
          sql_strerror());
    Jmsg(jcr, M_ERROR, 0, "%s", errmsg);
  } else {
    return true;
  }

  return false;
}

/**
 * Create a FileSet record. This record is unique in the
 * name and the MD5 signature of the include/exclude sets.
 * Returns: false on failure
 *          true on success with FileSetId in record
 */
bool BareosDb::CreateFilesetRecord(JobControlRecord* jcr, FileSetDbRecord* fsr)
{
  SQL_ROW row;
  int num_rows, len;
  char esc_fs[MAX_ESCAPE_NAME_LENGTH];
  char esc_md5[MAX_ESCAPE_NAME_LENGTH];

  DbLocker _{this};
  fsr->created = false;
  EscapeString(jcr, esc_fs, fsr->FileSet, strlen(fsr->FileSet));
  EscapeString(jcr, esc_md5, fsr->MD5, strlen(fsr->MD5));
  Mmsg(cmd,
       "SELECT FileSetId,CreateTime FROM FileSet WHERE "
       "FileSet='%s' AND MD5='%s'",
       esc_fs, esc_md5);

  fsr->FileSetId = 0;
  if (QueryDb(jcr, cmd)) {
    num_rows = SqlNumRows();

    if (num_rows > 1) {
      Mmsg2(errmsg, T_("More than one FileSet! %s: %d\n"), esc_fs, num_rows);
      Jmsg(jcr, M_ERROR, 0, "%s", errmsg);
    }
    if (num_rows >= 1) {
      // fileset record found
      if ((row = SqlFetchRow()) == NULL) {
        Mmsg1(errmsg, T_("error fetching FileSet row: ERR=%s\n"),
              sql_strerror());
        Jmsg(jcr, M_ERROR, 0, "%s", errmsg);
        SqlFreeResult();
        return false;
      }
      fsr->FileSetId = str_to_int64(row[0]);
      if (row[1] == NULL) {
        fsr->cCreateTime[0] = 0;
      } else {
        bstrncpy(fsr->cCreateTime, row[1], sizeof(fsr->cCreateTime));
      }
      // Update existing fileset record to make sure the fileset text is
      // inserted
      PoolMem esc_filesettext(PM_MESSAGE);

      len = strlen(fsr->FileSetText);
      esc_filesettext.check_size(len * 2 + 1);
      EscapeString(jcr, esc_filesettext.c_str(), fsr->FileSetText, len);

      Mmsg(cmd,
           "UPDATE FileSet SET (FileSet,MD5,CreateTime,FileSetText) "
           "= ('%s','%s','%s','%s') WHERE FileSet='%s' AND MD5='%s' ",
           esc_fs, esc_md5, fsr->cCreateTime, esc_filesettext.c_str(), esc_fs,
           esc_md5);
      if (!QueryDb(jcr, cmd)) {
        Mmsg1(errmsg, T_("error updating FileSet row: ERR=%s\n"),
              sql_strerror());
        Jmsg(jcr, M_ERROR, 0, "%s", errmsg);
        SqlFreeResult();
        return false;
      }
      SqlFreeResult();
      return true;
    } else {
      SqlFreeResult();
    }
  }

  if (fsr->CreateTime == 0 && fsr->cCreateTime[0] == 0) {
    fsr->CreateTime = time(NULL);
  }

  bstrutime(fsr->cCreateTime, sizeof(fsr->cCreateTime), fsr->CreateTime);
  if (fsr->FileSetText) {
    PoolMem esc_filesettext(PM_MESSAGE);

    len = strlen(fsr->FileSetText);
    esc_filesettext.check_size(len * 2 + 1);
    EscapeString(jcr, esc_filesettext.c_str(), fsr->FileSetText, len);
    Mmsg(cmd,
         "INSERT INTO FileSet (FileSet,MD5,CreateTime,FileSetText) "
         "VALUES ('%s','%s','%s','%s')",
         esc_fs, esc_md5, fsr->cCreateTime, esc_filesettext.c_str());
  } else {
    Mmsg(cmd,
         "INSERT INTO FileSet (FileSet,MD5,CreateTime,FileSetText) "
         "VALUES ('%s','%s','%s','')",
         esc_fs, esc_md5, fsr->cCreateTime);
  }

  fsr->FileSetId = SqlInsertAutokeyRecord(cmd, NT_("FileSet"));
  if (fsr->FileSetId == 0) {
    Mmsg2(errmsg, T_("Create DB FileSet record %s failed. ERR=%s\n"), cmd,
          sql_strerror());
    Jmsg(jcr, M_ERROR, 0, "%s", errmsg);
    return false;
  } else {
    fsr->created = true;
    return true;
  }
}

/**
 * All sql_batch_* functions are used to do bulk batch insert in
 * File/Filename/Path tables.
 *
 * To sum up :
 *  - bulk load a temp table
 *  - insert missing paths into path with another single query (lock Path table
 * to avoid duplicates).
 *  - then insert the join between the temp, filename and path tables into file.
 *
 * Returns: false on failure
 *          true on success
 */
bool BareosDb::WriteBatchFileRecords(JobControlRecord* jcr)
{
  bool retval = false;
  int JobStatus = jcr->getJobStatus();

  if (!jcr->batch_started) { /* no files to backup ? */
    Dmsg0(50, "db_create_file_record : no files\n");
    return true;
  }

  DbLocker _{jcr->db_batch};

  Dmsg1(50, "db_create_file_record changes=%u\n", changes);

  jcr->setJobStatus(JS_AttrInserting);

  Jmsg(jcr, M_INFO, 0,
       "Insert of attributes batch table with %u entries start\n",
       jcr->db_batch->changes);

  if (!jcr->db_batch->SqlBatchEndFileTable(jcr, NULL)) {
    Jmsg1(jcr, M_FATAL, 0, "Batch end %s\n", errmsg);
    goto bail_out;
  }

  if (!jcr->db_batch->SqlQuery(SQL_QUERY::batch_lock_path_query)) {
    Jmsg1(jcr, M_FATAL, 0, "Lock Path table %s\n", errmsg);
    goto bail_out;
  }

  if (!jcr->db_batch->SqlQuery(SQL_QUERY::batch_fill_path_query)) {
    Jmsg1(jcr, M_FATAL, 0, "Fill Path table %s\n", errmsg);
    jcr->db_batch->SqlQuery(SQL_QUERY::batch_unlock_tables_query);
    goto bail_out;
  }

  if (!jcr->db_batch->SqlQuery(SQL_QUERY::batch_unlock_tables_query)) {
    Jmsg1(jcr, M_FATAL, 0, "Unlock Path table %s\n", errmsg);
    goto bail_out;
  }

  /* clang-format off */
  if (!jcr->db_batch->SqlQuery(
        "INSERT INTO File (FileIndex, JobId, PathId, Name, LStat, MD5, DeltaSeq, Fhinfo, Fhnode) "
        "SELECT batch.FileIndex, batch.JobId, Path.PathId, "
        "batch.Name, batch.LStat, batch.MD5, batch.DeltaSeq, batch.Fhinfo, batch.Fhnode "
        "FROM batch "
        "JOIN Path ON (batch.Path = Path.Path) ")) {
     Jmsg1(jcr, M_FATAL, 0, "Fill File table %s\n", errmsg);
     goto bail_out;
  }
  /* clang-format on */

  jcr->setJobStatus(JobStatus); /* reset entry status */
  Jmsg(jcr, M_INFO, 0, "Insert of attributes batch table done\n");
  retval = true;


bail_out:
  SqlQuery("DROP TABLE IF EXISTS batch");
  jcr->batch_started = false;
  changes = 0;

  return retval;
}

/**
 * Create File record in BareosDb
 *
 * In order to reduce database size, we store the File attributes,
 * the FileName, and the Path separately.  In principle, there
 * is a single FileName record and a single Path record, no matter
 * how many times it occurs.  This is this subroutine, we separate
 * the file and the path and fill temporary tables with this three records.
 *
 * Note: all routines that call this expect to be able to call
 *   db_strerror(mdb) to get the error message, so the error message
 *   MUST be edited into mdb->errmsg before returning an error status.
 *
 * Returns: false on failure
 *          true on success
 */
bool BareosDb::CreateBatchFileAttributesRecord(JobControlRecord* jcr,
                                               AttributesDbRecord* ar)
{
  ASSERT(ar->FileType != FT_BASE);

  Dmsg1(dbglevel, "Fname=%s\n", ar->fname);
  Dmsg0(dbglevel, "put_file_into_catalog\n");

  if (jcr->batch_started && jcr->db_batch->changes > BATCH_FLUSH) {
    jcr->db_batch->WriteBatchFileRecords(jcr);
  }

  if (!jcr->batch_started) {
    if (!OpenBatchConnection(jcr)) { return false; /* error already printed */ }
    DbLocker batch_lock{jcr->db_batch};
    if (!jcr->db_batch->SqlBatchStartFileTable(jcr)) {
      Mmsg1(errmsg, "Can't start batch mode: ERR=%s",
            jcr->db_batch->strerror());
      Jmsg(jcr, M_FATAL, 0, "%s", errmsg);
      return false;
    }
    jcr->batch_started = true;
  }

  DbLocker batch_lock{jcr->db_batch};
  jcr->db_batch->SplitPathAndFile(jcr, ar->fname);

  return jcr->db_batch->SqlBatchInsertFileTable(jcr, ar);
}

/**
 * Create File record in BareosDb
 *
 * In order to reduce database size, we store the File attributes,
 * the FileName, and the Path separately.  In principle, there
 * is a single Path record, no matter how many times it occurs.
 * This is this subroutine, we separate
 * the file name and the path and create two database records.
 *
 * Returns: false on failure
 *          true on success
 */
bool BareosDb::CreateFileAttributesRecord(JobControlRecord* jcr,
                                          AttributesDbRecord* ar)
{
  DbLocker _{this};
  Dmsg1(dbglevel, "Fname=%s\n", ar->fname);
  Dmsg0(dbglevel, "put_file_into_catalog\n");
  SplitPathAndFile(jcr, ar->fname);

  if (!CreatePathRecord(jcr, ar)) { return false; }
  Dmsg1(dbglevel, "CreatePathRecord: %s\n", esc_name);

  /* Now create master File record */
  if (!CreateFileRecord(jcr, ar)) { return false; }
  Dmsg0(dbglevel, "CreateFileRecord OK\n");

  Dmsg2(dbglevel, "CreateAttributes Path=%s File=%s\n", path, fname);

  return true;
}

/**
 * This is the master File entry containing the attributes.
 * The filename and path records have already been created.
 * Returns: false on failure
 *          true on success with fileid filled in
 */
bool BareosDb::CreateFileRecord(JobControlRecord* jcr, AttributesDbRecord* ar)
{
  bool retval = false;
  static const char* no_digest = "0";
  const char* digest;

  ASSERT(ar->JobId);
  ASSERT(ar->PathId);

  esc_name = CheckPoolMemorySize(esc_name, 2 * fnl + 2);
  EscapeString(jcr, esc_name, fname, fnl);

  if (ar->Digest == NULL || ar->Digest[0] == 0) {
    digest = no_digest;
  } else {
    digest = ar->Digest;
  }

  /* clang-format off */
  Mmsg(cmd,
       "INSERT INTO File (FileIndex,JobId,PathId,Name,"
       "LStat,MD5,DeltaSeq,Fhinfo,Fhnode) VALUES (%u,%u,%u,'%s','%s','%s',%u,%" PRIu64 ",%" PRIu64 ")",
       ar->FileIndex, ar->JobId, ar->PathId, esc_name,
       ar->attr, digest, ar->DeltaSeq, ar->Fhinfo, ar->Fhnode);
  /* clang-format on */

  ar->FileId = SqlInsertAutokeyRecord(cmd, NT_("File"));
  if (ar->FileId == 0) {
    Mmsg2(errmsg, T_("Create db File record %s failed. ERR=%s"), cmd,
          sql_strerror());
    Jmsg(jcr, M_FATAL, 0, "%s", errmsg);
  } else {
    retval = true;
  }
  return retval;
}


/**
 * Create file attributes record, or base file attributes record
 * Returns: false on failure
 *          true on success
 */
bool BareosDb::CreateAttributesRecord(JobControlRecord* jcr,
                                      AttributesDbRecord* ar)
{
  bool retval;

  DbLocker _{this};

  errmsg[0] = 0;
  // Make sure we have an acceptable attributes record.
  if (!ar) {
    Mmsg0(errmsg,
          T_("Attempt to create file attributes record with no data\n"));
    Jmsg(jcr, M_FATAL, 0, "%s", errmsg);
    return false;
  }
  if (!(ar->Stream == STREAM_UNIX_ATTRIBUTES
        || ar->Stream == STREAM_UNIX_ATTRIBUTES_EX)) {
    Mmsg1(errmsg, T_("Attempt to put non-attributes into catalog. Stream=%d\n"),
          ar->Stream);
    Jmsg(jcr, M_FATAL, 0, "%s", errmsg);
    return false;
  }

  if (ar->FileType != FT_BASE) {
    if (BatchInsertAvailable()) {
      retval = CreateBatchFileAttributesRecord(jcr, ar);
      // Error message already printed
    } else {
      retval = CreateFileAttributesRecord(jcr, ar);
    }
  } else if (jcr->HasBase) {
    retval = CreateBaseFileAttributesRecord(jcr, ar);
  } else {
    Mmsg0(errmsg, T_("Cannot Copy/Migrate job using BaseJob.\n"));
    Jmsg(jcr, M_FATAL, 0, "%s", errmsg);
    retval = true; /* in copy/migration what do we do ? */
  }

  return retval;
}

/**
 * Create Base File record in BareosDb
 * Returns: false on failure
 *          true on success
 */
bool BareosDb::CreateBaseFileAttributesRecord(JobControlRecord* jcr,
                                              AttributesDbRecord* ar)
{
  Dmsg1(dbglevel, "create_base_file Fname=%s\n", ar->fname);
  Dmsg0(dbglevel, "put_base_file_into_catalog\n");

  DbLocker _{this};
  SplitPathAndFile(jcr, ar->fname);

  esc_name = CheckPoolMemorySize(esc_name, fnl * 2 + 1);
  EscapeString(jcr, esc_name, fname, fnl);

  esc_path = CheckPoolMemorySize(esc_path, pnl * 2 + 1);
  EscapeString(jcr, esc_path, path, pnl);

  Mmsg(cmd, "INSERT INTO basefile%" PRIu32 " (Path, Name) VALUES ('%s','%s')",
       jcr->JobId, esc_path, esc_name);

  return InsertDb(jcr, cmd) == 1;
}

void BareosDb::CleanupBaseFile(JobControlRecord* jcr)
{
  PoolMem buf(PM_MESSAGE);
  Mmsg(buf, "DROP TABLE IF EXISTS new_basefile%" PRIu32, jcr->JobId);
  SqlQuery(buf.c_str());

  Mmsg(buf, "DROP TABLE IF EXISTS basefile%" PRIu32, jcr->JobId);
  SqlQuery(buf.c_str());
}

/**
 * Put all base file seen in the backup to the BaseFile table
 * and cleanup temporary tables
 * Returns: false on failure
 *          true on success
 */
bool BareosDb::CommitBaseFileAttributesRecord(JobControlRecord* jcr)
{
  bool retval;
  char ed1[50];

  DbLocker _{this};

  /* clang-format off */
  Mmsg(cmd,
       "INSERT INTO BaseFiles (BaseJobId, JobId, FileId, FileIndex) "
       "SELECT B.JobId AS BaseJobId, %s AS JobId, "
       "B.FileId, B.FileIndex "
       "FROM basefile%s AS A, new_basefile%s AS B "
       "WHERE A.Path = B.Path "
       "AND A.Name = B.Name "
       "ORDER BY B.FileId",
       edit_uint64(jcr->JobId, ed1), ed1, ed1);
  /* clang-format on */

  retval = SqlQuery(cmd);
  jcr->nb_base_files_used = SqlAffectedRows();
  CleanupBaseFile(jcr);

  return retval;
}

/**
 * Find the last "accurate" backup state with Base jobs
 * 1) Get all files with jobid in list (F subquery)
 * 2) Take only the last version of each file (Temp subquery) => accurate list
 * is ok 3) Put the result in a temporary table for the end of job
 *
 * Returns: false on failure
 *          true on success
 */
bool BareosDb::CreateBaseFileList(JobControlRecord* jcr, const char* jobids)
{
  DbLocker _{this};

  if (!*jobids) {
    Mmsg(errmsg, T_("ERR=JobIds are empty\n"));
    return false;
  }

  PoolMem buf(PM_MESSAGE);

  FillQuery(SQL_QUERY::create_temp_basefile, (uint64_t)jcr->JobId);
  if (!SqlQuery(cmd)) { return false; }

  FillQuery(buf, SQL_QUERY::select_recent_version, jobids, jobids);
  FillQuery(SQL_QUERY::create_temp_new_basefile, (uint64_t)jcr->JobId,
            buf.c_str());

  return SqlQuery(cmd);
}

/**
 * Create Restore Object record in BareosDb
 * Returns: false on failure
 *          true on success
 */
bool BareosDb::CreateRestoreObjectRecord(JobControlRecord* jcr,
                                         RestoreObjectDbRecord* ro)
{
  bool retval = false;
  int plug_name_len;
  POOLMEM* esc_plug_name = GetPoolMemory(PM_MESSAGE);

  DbLocker _{this};

  Dmsg1(dbglevel, "Oname=%s\n", ro->object_name);
  Dmsg0(dbglevel, "put_object_into_catalog\n");

  fnl = strlen(ro->object_name);
  esc_name = CheckPoolMemorySize(esc_name, fnl * 2 + 1);
  EscapeString(jcr, esc_name, ro->object_name, fnl);

  EscapeObject(jcr, ro->object, ro->object_len);

  plug_name_len = strlen(ro->plugin_name);
  esc_plug_name = CheckPoolMemorySize(esc_plug_name, plug_name_len * 2 + 1);
  EscapeString(jcr, esc_plug_name, ro->plugin_name, plug_name_len);

  /* clang-format off */
  Mmsg(cmd,
       "INSERT INTO RestoreObject (ObjectName,PluginName,RestoreObject,"
       "ObjectLength,ObjectFullLength,ObjectIndex,ObjectType,"
       "ObjectCompression,FileIndex,JobId) "
       "VALUES ('%s','%s','%s',%d,%d,%d,%d,%d,%d,%u)",
       esc_name, esc_plug_name, esc_obj,
       ro->object_len, ro->object_full_len, ro->object_index,
       ro->FileType, ro->object_compression, ro->FileIndex, ro->JobId);
  /* clang-format on */

  ro->RestoreObjectId = SqlInsertAutokeyRecord(cmd, NT_("RestoreObject"));
  if (ro->RestoreObjectId == 0) {
    Mmsg2(errmsg, T_("Create db Object record %s failed. ERR=%s"), cmd,
          sql_strerror());
    Jmsg(jcr, M_FATAL, 0, "%s", errmsg);
  } else {
    retval = true;
  }
  FreePoolMemory(esc_plug_name);
  return retval;
}

/**
 * Create a quota record if it does not exist.
 * Returns: false on failure
 *          true on success
 */
bool BareosDb::CreateQuotaRecord(JobControlRecord* jcr, ClientDbRecord* cr)
{
  char ed1[50];
  int num_rows;

  DbLocker _{this};
  Mmsg(cmd, "SELECT ClientId FROM Quota WHERE ClientId='%s'",
       edit_uint64(cr->ClientId, ed1));

  if (QueryDb(jcr, cmd)) {
    num_rows = SqlNumRows();
    if (num_rows == 1) {
      SqlFreeResult();
      return true;
    }
    SqlFreeResult();
  }

  Mmsg(cmd,
       "INSERT INTO Quota (ClientId, GraceTime, QuotaLimit)"
       " VALUES ('%s', '%s', %s)",
       edit_uint64(cr->ClientId, ed1), "0", "0");

  if (InsertDb(jcr, cmd) != 1) {
    Mmsg2(errmsg, T_("Create DB Quota record %s failed. ERR=%s\n"), cmd,
          sql_strerror());
    Jmsg(jcr, M_ERROR, 0, "%s", errmsg);
    return false;
  } else {
    return true;
  }
}

/**
 * Create a NDMP level mapping if it does not exist.
 * Returns: false on failure
 *          true on success
 */
bool BareosDb::CreateNdmpLevelMapping(JobControlRecord* jcr,
                                      JobDbRecord* jr,
                                      char* filesystem)
{
  char ed1[50], ed2[50];
  int num_rows;

  DbLocker _{this};

  esc_name = CheckPoolMemorySize(esc_name, strlen(filesystem) * 2 + 1);
  EscapeString(jcr, esc_name, filesystem, strlen(filesystem));

  Mmsg(cmd,
       "SELECT ClientId FROM NDMPLevelMap WHERE "
       "ClientId='%s' AND FileSetId='%s' AND FileSystem='%s'",
       edit_uint64(jr->ClientId, ed1), edit_uint64(jr->FileSetId, ed2),
       esc_name);

  if (QueryDb(jcr, cmd)) {
    num_rows = SqlNumRows();
    if (num_rows == 1) {
      SqlFreeResult();
      return true;
    }
    SqlFreeResult();
  }

  Mmsg(cmd,
       "INSERT INTO NDMPLevelMap (ClientId, FilesetId, FileSystem, DumpLevel)"
       " VALUES ('%s', '%s', '%s', %s)",
       edit_uint64(jr->ClientId, ed1), edit_uint64(jr->FileSetId, ed2),
       esc_name, "0");
  if (InsertDb(jcr, cmd) != 1) {
    Mmsg2(errmsg, T_("Create DB NDMP Level Map record %s failed. ERR=%s\n"),
          cmd, sql_strerror());
    Jmsg(jcr, M_ERROR, 0, "%s", errmsg);
    return false;
  } else {
    return true;
  }
}

/**
 * Create a NDMP Job Environment String
 * Returns: false on failure
 *          true on success
 */
bool BareosDb::CreateNdmpEnvironmentString(JobControlRecord* jcr,
                                           JobDbRecord* jr,
                                           char* name,
                                           char* value)
{
  char ed1[50], ed2[50];
  char esc_envname[MAX_ESCAPE_NAME_LENGTH];
  char esc_envvalue[MAX_ESCAPE_NAME_LENGTH];

  Jmsg(jcr, M_INFO, 0, "NDMP Environment: %s=%s\n", name, value);

  DbLocker _{this};

  EscapeString(jcr, esc_envname, name, strlen(name));
  EscapeString(jcr, esc_envvalue, value, strlen(value));
  Mmsg(cmd,
       "INSERT INTO NDMPJobEnvironment (JobId, FileIndex, EnvName, EnvValue)"
       " VALUES ('%s', '%s', '%s', '%s')"
       " ON CONFLICT (JobId, FileIndex, EnvName)"
       " DO UPDATE SET"
       " EnvValue='%s'",
       edit_int64(jr->JobId, ed1), edit_uint64(jr->FileIndex, ed2), esc_envname,
       esc_envvalue, esc_envvalue);
  if (InsertDb(jcr, cmd) != 1) {
    Mmsg2(errmsg,
          T_("Create DB NDMP Job Environment record %s failed. ERR=%s\n"), cmd,
          sql_strerror());
    Jmsg(jcr, M_ERROR, 0, "%s", errmsg);
    return false;
  } else {
    return true;
  }
}

/**
 * Create a Job Statistics record.
 * Returns: false on failure
 *          true on success
 */
bool BareosDb::CreateJobStatistics(JobControlRecord* jcr,
                                   JobStatisticsDbRecord* jsr)
{
  time_t stime;
  char dt[MAX_TIME_LENGTH];
  char ed1[50], ed2[50], ed3[50], ed4[50];

  DbLocker _{this};

  stime = jsr->SampleTime;
  ASSERT(stime != 0);

  bstrutime(dt, sizeof(dt), stime);

  Mmsg(cmd,
       "INSERT INTO JobStats (SampleTime, JobId, JobFiles, JobBytes, DeviceId)"
       " VALUES ('%s', %s, %s, %s, %s)",
       dt, edit_int64(jsr->JobId, ed1), edit_uint64(jsr->JobFiles, ed2),
       edit_uint64(jsr->JobBytes, ed3), edit_int64(jsr->DeviceId, ed4));
  Dmsg1(200, "Create job stats: %s\n", cmd);

  if (InsertDb(jcr, cmd) != 1) {
    Mmsg2(errmsg, T_("Create DB JobStats record %s failed. ERR=%s\n"), cmd,
          sql_strerror());
    Jmsg(jcr, M_ERROR, 0, "%s", errmsg);
    return false;
  } else {
    return true;
  }
}

/**
 * Create a Device Statistics record.
 * Returns: false on failure
 *          true on success
 */
bool BareosDb::CreateDeviceStatistics(JobControlRecord* jcr,
                                      DeviceStatisticsDbRecord* dsr)
{
  time_t stime;
  char dt[MAX_TIME_LENGTH];
  char ed1[50], ed2[50], ed3[50], ed4[50], ed5[50], ed6[50];
  char ed7[50], ed8[50], ed9[50], ed10[50], ed11[50], ed12[50];

  DbLocker _{this};

  stime = dsr->SampleTime;
  ASSERT(stime != 0);

  bstrutime(dt, sizeof(dt), stime);

  /* clang-format off */
  Mmsg(cmd,
       "INSERT INTO DeviceStats (DeviceId, SampleTime, ReadTime, WriteTime,"
       " ReadBytes, WriteBytes, SpoolSize, NumWaiting, NumWriters, MediaId,"
       " VolCatBytes, VolCatFiles, VolCatBlocks)"
       " VALUES (%s, '%s', %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s)",
       edit_int64(dsr->DeviceId, ed1),
       dt,
       edit_uint64(dsr->ReadTime, ed2),
       edit_uint64(dsr->WriteTime, ed3),
       edit_uint64(dsr->ReadBytes, ed4),
       edit_uint64(dsr->WriteBytes, ed5),
       edit_uint64(dsr->SpoolSize, ed6),
       edit_uint64(dsr->NumWaiting, ed7),
       edit_uint64(dsr->NumWriters, ed8),
       edit_int64(dsr->MediaId, ed9),
       edit_uint64(dsr->VolCatBytes, ed10),
       edit_uint64(dsr->VolCatFiles, ed11),
       edit_uint64(dsr->VolCatBlocks, ed12));
  /* clang-format on */

  Dmsg1(200, "Create device stats: %s\n", cmd);

  if (InsertDb(jcr, cmd) != 1) {
    Mmsg2(errmsg, T_("Create DB DeviceStats record %s failed. ERR=%s\n"), cmd,
          sql_strerror());
    Jmsg(jcr, M_ERROR, 0, "%s", errmsg);

    return false;
  } else {
    return true;
  }
}

/**
 * Create a tapealert record.
 * Returns: false on failure
 *          true on success
 */
bool BareosDb::CreateTapealertStatistics(JobControlRecord* jcr,
                                         TapealertStatsDbRecord* tsr)
{
  time_t stime;
  char dt[MAX_TIME_LENGTH];
  char ed1[50], ed2[50];

  DbLocker _{this};

  stime = tsr->SampleTime;
  ASSERT(stime != 0);

  bstrutime(dt, sizeof(dt), stime);

  /* clang-format off */
  Mmsg(cmd,
       "INSERT INTO TapeAlerts (DeviceId, SampleTime, AlertFlags)"
       " VALUES (%s, '%s', %s)",
       edit_int64(tsr->DeviceId, ed1),
       dt,
       edit_uint64(tsr->AlertFlags, ed2));
  /* clang-format on */

  Dmsg1(200, "Create tapealert: %s\n", cmd);

  if (InsertDb(jcr, cmd) != 1) {
    Mmsg2(errmsg, T_("Create DB TapeAlerts record %s failed. ERR=%s\n"), cmd,
          sql_strerror());
    Jmsg(jcr, M_ERROR, 0, "%s", errmsg);

    return false;
  } else {
    return true;
  }
}
