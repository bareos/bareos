/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2012 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2016 Planets Communications B.V.
   Copyright (C) 2013-2020 Bareos GmbH & Co. KG

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
/*
 * Kern Sibbald, March 2000
 */
/**
 * @file
 * BAREOS Catalog Database Get record interface routines
 *
 * Note, these routines generally get a record by id or
 * by name.  If more logic is involved, the routine
 * should be in find.c
 */

#include "include/bareos.h"

#if HAVE_SQLITE3 || HAVE_MYSQL || HAVE_POSTGRESQL || HAVE_INGRES || HAVE_DBI

#include "cats.h"
#include "sql.h"
#include "lib/edit.h"
#include "lib/volume_session_info.h"
#include "include/make_unique.h"

/* -----------------------------------------------------------------------
 *
 *   Generic Routines (or almost generic)
 *
 * -----------------------------------------------------------------------
 */

/* Forward referenced functions */

/**
 * Given a full filename (with path), look up the File record
 * (with attributes) in the database.
 *
 *  Returns: 0 on failure
 *           1 on success with the File record in FileDbRecord
 */
bool BareosDb::GetFileAttributesRecord(JobControlRecord* jcr,
                                       char* filename,
                                       JobDbRecord* jr,
                                       FileDbRecord* fdbr)
{
  bool retval;
  Dmsg1(100, "db_get_file_attributes_record filename=%s \n", filename);

  DbLock(this);

  SplitPathAndFile(jcr, filename);
  fdbr->PathId = GetPathRecord(jcr);
  retval = GetFileRecord(jcr, jr, fdbr);

  DbUnlock(this);
  return retval;
}

/**
 * Get a File record
 * Returns: false on failure
 *          true on success
 *
 *  DO NOT use Jmsg in this routine.
 *
 *  Note in this routine, we do not use Jmsg because it may be
 *    called to get attributes of a non-existent file, which is
 *    "normal" if a new file is found during Verify.
 *
 *  The following is a bit of a kludge: because we always backup a
 *    directory entry, we can end up with two copies of the directory
 *    in the backup. One is when we encounter the directory and find
 *    we cannot recurse into it, and the other is when we find an
 *    explicit mention of the directory. This can also happen if the
 *    use includes the directory twice.  In this case, Verify
 *    VolumeToCatalog fails because we have two copies in the catalog,
 *    and only the first one is marked (twice).  So, when calling from Verify,
 *    VolumeToCatalog jr is not NULL and we know jr->FileIndex is the fileindex
 *    of the version of the directory/file we actually want and do
 *    a more explicit SQL search.
 */
bool BareosDb::GetFileRecord(JobControlRecord* jcr,
                             JobDbRecord* jr,
                             FileDbRecord* fdbr)
{
  bool retval = false;
  SQL_ROW row;
  char ed1[50], ed2[50], ed3[50];
  int num_rows;

  esc_name = CheckPoolMemorySize(esc_name, 2 * fnl + 2);
  EscapeString(jcr, esc_name, fname, fnl);

  if (jcr->getJobLevel() == L_VERIFY_DISK_TO_CATALOG) {
    Mmsg(cmd,
         "SELECT FileId, LStat, MD5, Fhinfo, Fhnode FROM File,Job WHERE "
         "File.JobId=Job.JobId AND File.PathId=%s AND "
         "File.Name='%s' AND Job.Type='B' AND Job.JobStatus IN ('T','W') AND "
         "ClientId=%s ORDER BY StartTime DESC LIMIT 1",
         edit_int64(fdbr->PathId, ed1), esc_name,
         edit_int64(jr->ClientId, ed3));
  } else if (jcr->getJobLevel() == L_VERIFY_VOLUME_TO_CATALOG) {
    Mmsg(cmd,
         "SELECT FileId, LStat, MD5, Fhinfo, Fhnode FROM File WHERE "
         "File.JobId=%s AND File.PathId=%s AND "
         "File.Name='%s' AND File.FileIndex=%u",
         edit_int64(fdbr->JobId, ed1), edit_int64(fdbr->PathId, ed2), esc_name,
         jr->FileIndex);
  } else {
    Mmsg(cmd,
         "SELECT FileId, LStat, MD5, Fhinfo, Fhnode FROM File WHERE "
         "File.JobId=%s AND File.PathId=%s AND "
         "File.Name='%s'",
         edit_int64(fdbr->JobId, ed1), edit_int64(fdbr->PathId, ed2), esc_name);
  }
  Dmsg3(450, "Get_file_record JobId=%u Filename=%s PathId=%u\n", fdbr->JobId,
        esc_name, fdbr->PathId);

  Dmsg1(100, "Query=%s\n", cmd);

  if (QUERY_DB(jcr, cmd)) {
    num_rows = SqlNumRows();
    Dmsg1(050, "GetFileRecord num_rows=%d\n", num_rows);
    if (num_rows >= 1) {
      if ((row = SqlFetchRow()) == NULL) {
        Mmsg1(errmsg, _("Error fetching row: %s\n"), sql_strerror());
      } else {
        fdbr->FileId = (FileId_t)str_to_int64(row[0]);
        bstrncpy(fdbr->LStat, row[1], sizeof(fdbr->LStat));
        bstrncpy(fdbr->Digest, row[2], sizeof(fdbr->Digest));
        retval = true;
        if (num_rows > 1) {
          Mmsg3(errmsg,
                _("GetFileRecord want 1 got rows=%d PathId=%s Filename=%s\n"),
                num_rows, edit_int64(fdbr->PathId, ed1), esc_name);
          Dmsg1(000, "=== Problem!  %s", errmsg);
        }
      }
    } else {
      Mmsg2(errmsg, _("File record for PathId=%s Filename=%s not found.\n"),
            edit_int64(fdbr->PathId, ed1), esc_name);
    }
    SqlFreeResult();
  } else {
    Mmsg(errmsg, _("File record not found in Catalog.\n"));
  }
  return retval;
}


/**
 * Get path record
 * Returns: 0 on failure
 *          PathId on success
 *
 *   DO NOT use Jmsg in this routine (see notes for GetFileRecord)
 */
int BareosDb::GetPathRecord(JobControlRecord* jcr)
{
  SQL_ROW row;
  DBId_t PathId = 0;
  int num_rows;

  esc_name = CheckPoolMemorySize(esc_name, 2 * pnl + 2);
  EscapeString(jcr, esc_name, path, pnl);

  if (cached_path_id != 0 && cached_path_len == pnl &&
      bstrcmp(cached_path, path)) {
    return cached_path_id;
  }

  Mmsg(cmd, "SELECT PathId FROM Path WHERE Path='%s'", esc_name);
  if (QUERY_DB(jcr, cmd)) {
    char ed1[30];
    num_rows = SqlNumRows();
    if (num_rows > 1) {
      Mmsg2(errmsg, _("More than one Path!: %s for path: %s\n"),
            edit_uint64(num_rows, ed1), path);
      Jmsg(jcr, M_WARNING, 0, "%s", errmsg);
    }
    /* Even if there are multiple paths, take the first one */
    if (num_rows >= 1) {
      if ((row = SqlFetchRow()) == NULL) {
        Mmsg1(errmsg, _("error fetching row: %s\n"), sql_strerror());
      } else {
        PathId = str_to_int64(row[0]);
        if (PathId <= 0) {
          Mmsg2(errmsg, _("Get DB path record %s found bad record: %s\n"), cmd,
                edit_int64(PathId, ed1));
          PathId = 0;
        } else {
          /*
           * Cache path
           */
          if (PathId != cached_path_id) {
            cached_path_id = PathId;
            cached_path_len = pnl;
            PmStrcpy(cached_path, path);
          }
        }
      }
    } else {
      Mmsg1(errmsg, _("Path record: %s not found.\n"), path);
    }
    SqlFreeResult();
  } else {
    Mmsg(errmsg, _("Path record: %s not found in Catalog.\n"), path);
  }
  return PathId;
}

int BareosDb::GetPathRecord(JobControlRecord* jcr, const char* new_path)
{
  PmStrcpy(path, new_path);
  pnl = strlen(path);
  return GetPathRecord(jcr);
}

/**
 * Get Job record for given JobId or Job name
 * Returns: false on failure
 *          true  on success
 */
bool BareosDb::GetJobRecord(JobControlRecord* jcr, JobDbRecord* jr)
{
  bool retval = false;
  SQL_ROW row;
  char ed1[50];
  char esc[MAX_ESCAPE_NAME_LENGTH];

  DbLock(this);
  if (jr->JobId == 0) {
    EscapeString(jcr, esc, jr->Job, strlen(jr->Job));
    Mmsg(cmd,
         "SELECT VolSessionId,VolSessionTime,"
         "PoolId,StartTime,EndTime,JobFiles,JobBytes,JobTDate,Job,JobStatus,"
         "Type,Level,ClientId,Name,PriorJobId,RealEndTime,JobId,FileSetId,"
         "SchedTime,RealEndTime,ReadBytes,HasBase,PurgedFiles "
         "FROM Job WHERE Job='%s'",
         esc);
  } else {
    Mmsg(cmd,
         "SELECT VolSessionId,VolSessionTime,"
         "PoolId,StartTime,EndTime,JobFiles,JobBytes,JobTDate,Job,JobStatus,"
         "Type,Level,ClientId,Name,PriorJobId,RealEndTime,JobId,FileSetId,"
         "SchedTime,RealEndTime,ReadBytes,HasBase,PurgedFiles "
         "FROM Job WHERE JobId=%s",
         edit_int64(jr->JobId, ed1));
  }

  if (!QUERY_DB(jcr, cmd)) { goto bail_out; }
  if ((row = SqlFetchRow()) == NULL) {
    Mmsg1(errmsg, _("No Job found for JobId %s\n"), edit_int64(jr->JobId, ed1));
    SqlFreeResult();
    goto bail_out;
  }

  jr->VolSessionId = str_to_uint64(row[0]);
  jr->VolSessionTime = str_to_uint64(row[1]);
  jr->PoolId = str_to_int64(row[2]);
  bstrncpy(jr->cStartTime, (row[3] != NULL) ? row[3] : "",
           sizeof(jr->cStartTime));
  bstrncpy(jr->cEndTime, (row[4] != NULL) ? row[4] : "", sizeof(jr->cEndTime));
  jr->JobFiles = str_to_int64(row[5]);
  jr->JobBytes = str_to_int64(row[6]);
  jr->JobTDate = str_to_int64(row[7]);
  bstrncpy(jr->Job, (row[8] != NULL) ? row[8] : "", sizeof(jr->Job));
  jr->JobStatus = (row[9] != NULL) ? (int)*row[9] : JS_FatalError;
  jr->JobType = (row[10] != NULL) ? (int)*row[10] : JT_BACKUP;
  jr->JobLevel = (row[11] != NULL) ? (int)*row[11] : L_NONE;
  jr->ClientId = str_to_uint64((row[12] != NULL) ? row[12] : (char*)"");
  bstrncpy(jr->Name, (row[13] != NULL) ? row[13] : "", sizeof(jr->Name));
  jr->PriorJobId = str_to_uint64((row[14] != NULL) ? row[14] : (char*)"");
  bstrncpy(jr->cRealEndTime, (row[15] != NULL) ? row[15] : "",
           sizeof(jr->cRealEndTime));
  if (jr->JobId == 0) { jr->JobId = str_to_int64(row[16]); }
  jr->FileSetId = str_to_int64(row[17]);
  bstrncpy(jr->cSchedTime, (row[18] != NULL) ? row[18] : "",
           sizeof(jr->cSchedTime));
  bstrncpy(jr->cRealEndTime, (row[19] != NULL) ? row[19] : "",
           sizeof(jr->cRealEndTime));
  jr->ReadBytes = str_to_int64(row[20]);
  jr->StartTime = StrToUtime(jr->cStartTime);
  jr->SchedTime = StrToUtime(jr->cSchedTime);
  jr->EndTime = StrToUtime(jr->cEndTime);
  jr->RealEndTime = StrToUtime(jr->cRealEndTime);
  jr->HasBase = str_to_int64(row[21]);
  jr->PurgedFiles = str_to_int64(row[22]);

  SqlFreeResult();
  retval = true;

bail_out:
  DbUnlock(this);
  return retval;
}

/**
 * Find VolumeNames for a given JobId
 * Returns: 0 on error or no Volumes found
 *          number of volumes on success
 *             Volumes are concatenated in VolumeNames
 *             separated by a vertical bar (|) in the order
 *             that they were written.
 *
 * Returns: number of volumes on success
 */
int BareosDb::GetJobVolumeNames(JobControlRecord* jcr,
                                JobId_t JobId,
                                POOLMEM*& VolumeNames)
{
  SQL_ROW row;
  char ed1[50];
  int retval = 0;
  int i;
  int num_rows;

  DbLock(this);

  /*
   * Get one entry per VolumeName, but "sort" by VolIndex
   */
  Mmsg(cmd,
       "SELECT VolumeName,MAX(VolIndex) FROM JobMedia,Media WHERE "
       "JobMedia.JobId=%s AND JobMedia.MediaId=Media.MediaId "
       "GROUP BY VolumeName "
       "ORDER BY 2 ASC",
       edit_int64(JobId, ed1));

  Dmsg1(130, "VolNam=%s\n", cmd);
  VolumeNames[0] = '\0';
  if (QUERY_DB(jcr, cmd)) {
    num_rows = SqlNumRows();
    Dmsg1(130, "Num rows=%d\n", num_rows);
    if (num_rows <= 0) {
      Mmsg1(errmsg, _("No volumes found for JobId=%d\n"), JobId);
      retval = 0;
    } else {
      retval = num_rows;
      for (i = 0; i < retval; i++) {
        if ((row = SqlFetchRow()) == NULL) {
          Mmsg2(errmsg, _("Error fetching row %d: ERR=%s\n"), i,
                sql_strerror());
          Jmsg(jcr, M_ERROR, 0, "%s", errmsg);
          retval = 0;
          break;
        } else {
          if (VolumeNames[0] != '\0') { PmStrcat(VolumeNames, "|"); }
          PmStrcat(VolumeNames, row[0]);
        }
      }
    }
    SqlFreeResult();
  } else {
    Mmsg(errmsg, _("No Volume for JobId %d found in Catalog.\n"), JobId);
  }
  DbUnlock(this);

  return retval;
}

/**
 * Find Volume parameters for a given JobId
 * Returns: 0 on error or no Volumes found
 *          number of volumes on success
 *          List of Volumes and start/end file/blocks (malloced structure!)
 *
 * Returns: number of volumes on success
 */
int BareosDb::GetJobVolumeParameters(JobControlRecord* jcr,
                                     JobId_t JobId,
                                     VolumeParameters** VolParams)
{
  SQL_ROW row;
  char ed1[50];
  int retval = 0;
  int i;
  VolumeParameters* Vols = NULL;
  int num_rows;

  DbLock(this);
  Mmsg(cmd,
       "SELECT VolumeName,MediaType,FirstIndex,LastIndex,StartFile,"
       "JobMedia.EndFile,StartBlock,JobMedia.EndBlock,"
       "Slot,StorageId,InChanger,"
       "JobBytes"
       " FROM JobMedia,Media WHERE JobMedia.JobId=%s"
       " AND JobMedia.MediaId=Media.MediaId ORDER BY VolIndex,JobMediaId",
       edit_int64(JobId, ed1));

  Dmsg1(130, "VolNam=%s\n", cmd);
  if (QUERY_DB(jcr, cmd)) {
    num_rows = SqlNumRows();
    Dmsg1(200, "Num rows=%d\n", num_rows);
    if (num_rows <= 0) {
      Mmsg1(errmsg, _("No volumes found for JobId=%d\n"), JobId);
      retval = 0;
    } else {
      retval = num_rows;
      DBId_t* SId = NULL;
      if (retval > 0) {
        *VolParams = Vols =
            (VolumeParameters*)malloc(retval * sizeof(VolumeParameters));
        SId = (DBId_t*)malloc(retval * sizeof(DBId_t));
      }
      for (i = 0; i < retval; i++) {
        if ((row = SqlFetchRow()) == NULL) {
          Mmsg2(errmsg, _("Error fetching row %d: ERR=%s\n"), i,
                sql_strerror());
          Jmsg(jcr, M_ERROR, 0, "%s", errmsg);
          retval = 0;
          break;
        } else {
          DBId_t StorageId;
          uint32_t StartBlock, EndBlock, StartFile, EndFile;

          bstrncpy(Vols[i].VolumeName, row[0], MAX_NAME_LENGTH);
          bstrncpy(Vols[i].MediaType, row[1], MAX_NAME_LENGTH);
          Vols[i].FirstIndex = str_to_uint64(row[2]);
          Vols[i].LastIndex = str_to_uint64(row[3]);
          StartFile = str_to_uint64(row[4]);
          EndFile = str_to_uint64(row[5]);
          StartBlock = str_to_uint64(row[6]);
          EndBlock = str_to_uint64(row[7]);
          Vols[i].Slot = str_to_uint64(row[8]);
          StorageId = str_to_uint64(row[9]);
          Vols[i].InChanger = str_to_uint64(row[10]);
          Vols[i].JobBytes = str_to_uint64(row[11]);

          Vols[i].StartAddr = (((uint64_t)StartFile) << 32) | StartBlock;
          Vols[i].EndAddr = (((uint64_t)EndFile) << 32) | EndBlock;
          Vols[i].Storage[0] = 0;
          SId[i] = StorageId;
        }
      }
      for (i = 0; i < retval; i++) {
        if (SId[i] != 0) {
          Mmsg(cmd, "SELECT Name from Storage WHERE StorageId=%s",
               edit_int64(SId[i], ed1));
          if (QUERY_DB(jcr, cmd)) {
            if ((row = SqlFetchRow()) && row[0]) {
              bstrncpy(Vols[i].Storage, row[0], MAX_NAME_LENGTH);
            }
          }
        }
      }
      if (SId) { free(SId); }
    }
    SqlFreeResult();
  }
  DbUnlock(this);
  return retval;
}

/**
 * Get the number of pool records
 *
 * Returns: -1 on failure
 *          number on success
 */
int BareosDb::GetNumPoolRecords(JobControlRecord* jcr)
{
  int retval = 0;

  DbLock(this);
  Mmsg(cmd, "SELECT count(*) from Pool");
  retval = GetSqlRecordMax(jcr);
  DbUnlock(this);

  return retval;
}

/**
 * This function returns a list of all the Pool record ids.
 * The caller must free ids if non-NULL.
 *
 * Returns 0: on failure
 *         1: on success
 */
int BareosDb::GetPoolIds(JobControlRecord* jcr, int* num_ids, DBId_t** ids)
{
  SQL_ROW row;
  int retval = 0;
  int i = 0;
  DBId_t* id;

  DbLock(this);
  *ids = NULL;
  Mmsg(cmd, "SELECT PoolId FROM Pool");
  if (QUERY_DB(jcr, cmd)) {
    *num_ids = SqlNumRows();
    if (*num_ids > 0) {
      id = (DBId_t*)malloc(*num_ids * sizeof(DBId_t));
      while ((row = SqlFetchRow()) != NULL) { id[i++] = str_to_uint64(row[0]); }
      *ids = id;
    }
    SqlFreeResult();
    retval = 1;
  } else {
    Mmsg(errmsg, _("Pool id select failed: ERR=%s\n"), sql_strerror());
    Jmsg(jcr, M_ERROR, 0, "%s", errmsg);
    retval = 0;
  }

  DbUnlock(this);
  return retval;
}

/**
 * This function returns a list of all the Storage record ids.
 *  The caller must free ids if non-NULL.
 *
 *  Returns 0: on failure
 *          1: on success
 */
int BareosDb::GetStorageIds(JobControlRecord* jcr, int* num_ids, DBId_t* ids[])
{
  SQL_ROW row;
  int retval = 0;
  int i = 0;
  DBId_t* id;

  DbLock(this);
  *ids = NULL;
  Mmsg(cmd, "SELECT StorageId FROM Storage");
  if (QUERY_DB(jcr, cmd)) {
    *num_ids = SqlNumRows();
    if (*num_ids > 0) {
      id = (DBId_t*)malloc(*num_ids * sizeof(DBId_t));
      while ((row = SqlFetchRow()) != NULL) { id[i++] = str_to_uint64(row[0]); }
      *ids = id;
    }
    SqlFreeResult();
    retval = 1;
  } else {
    Mmsg(errmsg, _("Storage id select failed: ERR=%s\n"), sql_strerror());
    Jmsg(jcr, M_ERROR, 0, "%s", errmsg);
    retval = 0;
  }

  DbUnlock(this);
  return retval;
}

/**
 * This function returns a list of all the Client record ids.
 * The caller must free ids if non-NULL.
 *
 * Returns false: on failure
 *         true: on success
 */
bool BareosDb::GetClientIds(JobControlRecord* jcr, int* num_ids, DBId_t* ids[])
{
  bool retval = false;
  SQL_ROW row;
  int i = 0;
  DBId_t* id;

  DbLock(this);
  *ids = NULL;
  Mmsg(cmd, "SELECT ClientId FROM Client ORDER BY Name");
  if (QUERY_DB(jcr, cmd)) {
    *num_ids = SqlNumRows();
    if (*num_ids > 0) {
      id = (DBId_t*)malloc(*num_ids * sizeof(DBId_t));
      while ((row = SqlFetchRow()) != NULL) { id[i++] = str_to_uint64(row[0]); }
      *ids = id;
    }
    SqlFreeResult();
    retval = true;
  } else {
    Mmsg(errmsg, _("Client id select failed: ERR=%s\n"), sql_strerror());
    Jmsg(jcr, M_ERROR, 0, "%s", errmsg);
  }
  DbUnlock(this);
  return retval;
}

/**
 * Get Pool Record
 * If the PoolId is non-zero, we get its record,
 * otherwise, we search on the PoolName
 *
 * Returns: false on failure
 *          true on success
 */
bool BareosDb::GetPoolRecord(JobControlRecord* jcr, PoolDbRecord* pdbr)
{
  SQL_ROW row;
  bool ok = false;
  char ed1[50];
  int num_rows;
  char esc[MAX_ESCAPE_NAME_LENGTH];

  DbLock(this);
  if (pdbr->PoolId != 0) { /* find by id */
    Mmsg(
        cmd,
        "SELECT PoolId,Name,NumVols,MaxVols,UseOnce,UseCatalog,AcceptAnyVolume,"
        "AutoPrune,Recycle,VolRetention,VolUseDuration,MaxVolJobs,MaxVolFiles,"
        "MaxVolBytes,PoolType,LabelType,LabelFormat,RecyclePoolId,"
        "ScratchPoolId,"
        "ActionOnPurge,MinBlocksize,MaxBlocksize FROM Pool WHERE "
        "Pool.PoolId=%s",
        edit_int64(pdbr->PoolId, ed1));
  } else { /* find by name */
    EscapeString(jcr, esc, pdbr->Name, strlen(pdbr->Name));
    Mmsg(
        cmd,
        "SELECT PoolId,Name,NumVols,MaxVols,UseOnce,UseCatalog,AcceptAnyVolume,"
        "AutoPrune,Recycle,VolRetention,VolUseDuration,MaxVolJobs,MaxVolFiles,"
        "MaxVolBytes,PoolType,LabelType,LabelFormat,RecyclePoolId,"
        "ScratchPoolId,"
        "ActionOnPurge,MinBlocksize,MaxBlocksize FROM Pool WHERE "
        "Pool.Name='%s'",
        esc);
  }
  if (QUERY_DB(jcr, cmd)) {
    num_rows = SqlNumRows();
    if (num_rows > 1) {
      char ed1[30];
      Mmsg1(errmsg, _("More than one Pool!: %s\n"), edit_uint64(num_rows, ed1));
      Jmsg(jcr, M_ERROR, 0, "%s", errmsg);
    } else if (num_rows == 1) {
      if ((row = SqlFetchRow()) == NULL) {
        Mmsg1(errmsg, _("error fetching row: %s\n"), sql_strerror());
        Jmsg(jcr, M_ERROR, 0, "%s", errmsg);
      } else {
        pdbr->PoolId = str_to_int64(row[0]);
        bstrncpy(pdbr->Name, (row[1] != NULL) ? row[1] : "",
                 sizeof(pdbr->Name));
        pdbr->NumVols = str_to_int64(row[2]);
        pdbr->MaxVols = str_to_int64(row[3]);
        pdbr->UseOnce = str_to_int64(row[4]);
        pdbr->UseCatalog = str_to_int64(row[5]);
        pdbr->AcceptAnyVolume = str_to_int64(row[6]);
        pdbr->AutoPrune = str_to_int64(row[7]);
        pdbr->Recycle = str_to_int64(row[8]);
        pdbr->VolRetention = str_to_int64(row[9]);
        pdbr->VolUseDuration = str_to_int64(row[10]);
        pdbr->MaxVolJobs = str_to_int64(row[11]);
        pdbr->MaxVolFiles = str_to_int64(row[12]);
        pdbr->MaxVolBytes = str_to_uint64(row[13]);
        bstrncpy(pdbr->PoolType, (row[14] != NULL) ? row[14] : "",
                 sizeof(pdbr->PoolType));
        pdbr->LabelType = str_to_int64(row[15]);
        bstrncpy(pdbr->LabelFormat, (row[16] != NULL) ? row[16] : "",
                 sizeof(pdbr->LabelFormat));
        pdbr->RecyclePoolId = str_to_int64(row[17]);
        pdbr->ScratchPoolId = str_to_int64(row[18]);
        pdbr->ActionOnPurge = str_to_int32(row[19]);
        pdbr->MinBlocksize = str_to_int32(row[20]);
        pdbr->MaxBlocksize = str_to_int32(row[21]);
        ok = true;
      }
    }
    SqlFreeResult();
  }

  if (ok) {
    uint32_t NumVols;

    Mmsg(cmd, "SELECT count(*) from Media WHERE PoolId=%s",
         edit_int64(pdbr->PoolId, ed1));
    NumVols = GetSqlRecordMax(jcr);
    Dmsg2(400, "Actual NumVols=%d Pool NumVols=%d\n", NumVols, pdbr->NumVols);
    if (NumVols != pdbr->NumVols) {
      pdbr->NumVols = NumVols;
      ok = UpdatePoolRecord(jcr, pdbr);
    }
  } else {
    Mmsg(errmsg, _("Pool record not found in Catalog.\n"));
  }

  DbUnlock(this);
  return ok;
}

/**
 * Get Storage Record
 * If the StorageId is non-zero, we get its record, otherwise, we search on the
 * StorageName
 *
 * Returns: false on failure
 *          true on success
 */
bool BareosDb::GetStorageRecord(JobControlRecord* jcr, StorageDbRecord* sdbr)
{
  SQL_ROW row;
  bool ok = false;
  char ed1[50];
  int num_rows;
  char esc[MAX_ESCAPE_NAME_LENGTH];

  DbLock(this);
  if (sdbr->StorageId != 0) { /* find by id */
    Mmsg(cmd,
         "SELECT StorageId,Name,AutoChanger FROM Storage WHERE "
         "Storage.StorageId=%s",
         edit_int64(sdbr->StorageId, ed1));
  } else { /* find by name */
    EscapeString(jcr, esc, sdbr->Name, strlen(sdbr->Name));
    Mmsg(cmd,
         "SELECT StorageId,Name,Autochanger FROM Storage WHERE "
         "Storage.Name='%s'",
         esc);
  }
  if (QUERY_DB(jcr, cmd)) {
    num_rows = SqlNumRows();
    if (num_rows > 1) {
      char ed1[30];

      Mmsg1(errmsg, _("More than one Storage!: %s\n"),
            edit_uint64(num_rows, ed1));
      Jmsg(jcr, M_ERROR, 0, "%s", errmsg);
    } else if (num_rows == 1) {
      if ((row = SqlFetchRow()) == NULL) {
        Mmsg1(errmsg, _("error fetching row: %s\n"), sql_strerror());
        Jmsg(jcr, M_ERROR, 0, "%s", errmsg);
      } else {
        sdbr->StorageId = str_to_int64(row[0]);
        bstrncpy(sdbr->Name, (row[1] != NULL) ? row[1] : "",
                 sizeof(sdbr->Name));
        sdbr->AutoChanger = str_to_int64(row[2]);
        ok = true;
      }
    }
    SqlFreeResult();
  }

  DbUnlock(this);
  return ok;
}

/**
 * Get Client Record
 * If the ClientId is non-zero, we get its record, otherwise, we search on the
 * Client Name
 *
 * Returns: false on failure
 *          true on success
 */
bool BareosDb::GetClientRecord(JobControlRecord* jcr, ClientDbRecord* cdbr)
{
  bool retval = false;
  SQL_ROW row;
  char ed1[50];
  int num_rows;
  char esc[MAX_ESCAPE_NAME_LENGTH];

  DbLock(this);
  if (cdbr->ClientId != 0) { /* find by id */
    Mmsg(cmd,
         "SELECT ClientId,Name,Uname,AutoPrune,FileRetention,JobRetention "
         "FROM Client WHERE Client.ClientId=%s",
         edit_int64(cdbr->ClientId, ed1));
  } else { /* find by name */
    EscapeString(jcr, esc, cdbr->Name, strlen(cdbr->Name));
    Mmsg(cmd,
         "SELECT ClientId,Name,Uname,AutoPrune,FileRetention,JobRetention "
         "FROM Client WHERE Client.Name='%s'",
         esc);
  }

  if (QUERY_DB(jcr, cmd)) {
    num_rows = SqlNumRows();
    if (num_rows > 1) {
      Mmsg1(errmsg, _("More than one Client!: %s\n"),
            edit_uint64(num_rows, ed1));
      Jmsg(jcr, M_ERROR, 0, "%s", errmsg);
    } else if (num_rows == 1) {
      if ((row = SqlFetchRow()) == NULL) {
        Mmsg1(errmsg, _("error fetching row: %s\n"), sql_strerror());
        Jmsg(jcr, M_ERROR, 0, "%s", errmsg);
      } else {
        cdbr->ClientId = str_to_int64(row[0]);
        bstrncpy(cdbr->Name, (row[1] != NULL) ? row[1] : "",
                 sizeof(cdbr->Name));
        bstrncpy(cdbr->Uname, (row[2] != NULL) ? row[2] : "",
                 sizeof(cdbr->Uname));
        cdbr->AutoPrune = str_to_int64(row[3]);
        cdbr->FileRetention = str_to_int64(row[4]);
        cdbr->JobRetention = str_to_int64(row[5]);
        retval = true;
      }
    } else {
      Mmsg(errmsg, _("Client record not found in Catalog.\n"));
    }
    SqlFreeResult();
  } else {
    Mmsg(errmsg, _("Client record not found in Catalog.\n"));
  }

  DbUnlock(this);
  return retval;
}

/**
 * Get Counter Record
 *
 * Returns: false on failure
 *          true on success
 */
bool BareosDb::GetCounterRecord(JobControlRecord* jcr, CounterDbRecord* cr)
{
  bool retval = false;
  SQL_ROW row;
  int num_rows;
  char esc[MAX_ESCAPE_NAME_LENGTH];

  DbLock(this);
  EscapeString(jcr, esc, cr->Counter, strlen(cr->Counter));

  FillQuery(SQL_QUERY::select_counter_values, esc);
  if (QUERY_DB(jcr, cmd)) {
    num_rows = SqlNumRows();

    /*
     * If more than one, report error, but return first row
     */
    if (num_rows > 1) {
      Mmsg1(errmsg, _("More than one Counter!: %d\n"), num_rows);
      Jmsg(jcr, M_ERROR, 0, "%s", errmsg);
    }
    if (num_rows >= 1) {
      if ((row = SqlFetchRow()) == NULL) {
        Mmsg1(errmsg, _("error fetching Counter row: %s\n"), sql_strerror());
        Jmsg(jcr, M_ERROR, 0, "%s", errmsg);
        SqlFreeResult();
        goto bail_out;
      }
      cr->MinValue = str_to_int64(row[0]);
      cr->MaxValue = str_to_int64(row[1]);
      cr->CurrentValue = str_to_int64(row[2]);
      if (row[3]) {
        bstrncpy(cr->WrapCounter, row[3], sizeof(cr->WrapCounter));
      } else {
        cr->WrapCounter[0] = 0;
      }
      SqlFreeResult();
      retval = true;
      goto bail_out;
    }
    SqlFreeResult();
  } else {
    Mmsg(errmsg, _("Counter record: %s not found in Catalog.\n"), cr->Counter);
  }

bail_out:
  DbUnlock(this);
  return retval;
}

/**
 * Get FileSet Record
 * If the FileSetId is non-zero, we get its record,
 *  otherwise, we search on the name
 *
 * Returns: 0 on failure
 *          id on success
 */
int BareosDb::GetFilesetRecord(JobControlRecord* jcr, FileSetDbRecord* fsr)
{
  SQL_ROW row;
  int retval = 0;
  char ed1[50];
  int num_rows;
  char esc[MAX_ESCAPE_NAME_LENGTH];

  DbLock(this);
  if (fsr->FileSetId != 0) { /* find by id */
    Mmsg(cmd,
         "SELECT FileSetId,FileSet,MD5,CreateTime FROM FileSet "
         "WHERE FileSetId=%s",
         edit_int64(fsr->FileSetId, ed1));
  } else { /* find by name */
    EscapeString(jcr, esc, fsr->FileSet, strlen(fsr->FileSet));
    Mmsg(cmd,
         "SELECT FileSetId,FileSet,MD5,CreateTime FROM FileSet "
         "WHERE FileSet='%s' ORDER BY CreateTime DESC LIMIT 1",
         esc);
  }

  if (QUERY_DB(jcr, cmd)) {
    num_rows = SqlNumRows();
    if (num_rows > 1) {
      char ed1[30];
      Mmsg1(errmsg, _("Error got %s FileSets but expected only one!\n"),
            edit_uint64(num_rows, ed1));
      SqlDataSeek(num_rows - 1);
    }
    if ((row = SqlFetchRow()) == NULL) {
      Mmsg1(errmsg, _("FileSet record \"%s\" not found.\n"), fsr->FileSet);
    } else {
      fsr->FileSetId = str_to_int64(row[0]);
      bstrncpy(fsr->FileSet, (row[1] != NULL) ? row[1] : "",
               sizeof(fsr->FileSet));
      bstrncpy(fsr->MD5, (row[2] != NULL) ? row[2] : "", sizeof(fsr->MD5));
      bstrncpy(fsr->cCreateTime, (row[3] != NULL) ? row[3] : "",
               sizeof(fsr->cCreateTime));
      retval = fsr->FileSetId;
    }
    SqlFreeResult();
  } else {
    Mmsg(errmsg, _("FileSet record not found in Catalog.\n"));
  }
  DbUnlock(this);
  return retval;
}

/**
 * Get the number of Media records
 *
 * Returns: -1 on failure
 *          number on success
 */
int BareosDb::GetNumMediaRecords(JobControlRecord* jcr)
{
  int retval = 0;

  DbLock(this);
  Mmsg(cmd, "SELECT count(*) from Media");
  retval = GetSqlRecordMax(jcr);
  DbUnlock(this);
  return retval;
}

bool BareosDb::PrepareMediaSqlQuery(JobControlRecord* jcr,
                                    MediaDbRecord* mr,
                                    PoolMem& volumes)
{
  bool ok = true;
  char ed1[50];
  char esc[MAX_NAME_LENGTH * 2 + 1];
  PoolMem buf(PM_MESSAGE);

  Mmsg(cmd,
       "SELECT DISTINCT MediaId FROM Media WHERE Recycle=%d AND Enabled=%d ",
       mr->Recycle, mr->Enabled);

  if (*mr->MediaType) {
    EscapeString(jcr, esc, mr->MediaType, strlen(mr->MediaType));
    Mmsg(buf, "AND MediaType='%s' ", esc);
    PmStrcat(cmd, buf.c_str());
  }

  if (mr->StorageId) {
    Mmsg(buf, "AND StorageId=%s ", edit_uint64(mr->StorageId, ed1));
    PmStrcat(cmd, buf.c_str());
  }

  if (mr->PoolId) {
    Mmsg(buf, "AND PoolId=%s ", edit_uint64(mr->PoolId, ed1));
    PmStrcat(cmd, buf.c_str());
  }

  if (mr->VolBytes) {
    Mmsg(buf, "AND VolBytes > %s ", edit_uint64(mr->VolBytes, ed1));
    PmStrcat(cmd, buf.c_str());
  }

  if (*mr->VolStatus) {
    EscapeString(jcr, esc, mr->VolStatus, strlen(mr->VolStatus));
    Mmsg(buf, "AND VolStatus = '%s' ", esc);
    PmStrcat(cmd, buf.c_str());
  }

  if (volumes.strlen() > 0) {
    /* extra list of volumes given */
    Mmsg(buf, "AND VolumeName IN (%s) ", volumes.c_str());
    PmStrcat(cmd, buf.c_str());
  } else if (*mr->VolumeName) {
    /* single volume given in media record */
    EscapeString(jcr, esc, mr->VolumeName, strlen(mr->VolumeName));
    Mmsg(buf, "AND VolumeName = '%s' ", esc);
    PmStrcat(cmd, buf.c_str());
  }

  Dmsg1(100, "query=%s\n", cmd);

  return ok;
}

/**
 * This function creates a sql query string at cmd to return a list of all the
 * Media records for the current Pool, the correct Media Type, Recyle, Enabled,
 * StorageId, VolBytes and volumes or VolumeName if specified. Comma separated
 * list of volumes takes precedence over VolumeName. The caller must free ids if
 * non-NULL.
 */
bool BareosDb::GetMediaIds(JobControlRecord* jcr,
                           MediaDbRecord* mr,
                           PoolMem& volumes,
                           int* num_ids,
                           DBId_t* ids[])
{
  bool ok = false;
  SQL_ROW row;
  int i = 0;
  DBId_t* id;

  DbLock(this);
  *ids = NULL;

  if (!PrepareMediaSqlQuery(jcr, mr, volumes)) {
    Mmsg(errmsg, _("Media id select failed: invalid parameter"));
    Jmsg(jcr, M_ERROR, 0, "%s", errmsg);
    goto bail_out;
  }

  if (!QUERY_DB(jcr, cmd)) {
    Mmsg(errmsg, _("Media id select failed: ERR=%s\n"), sql_strerror());
    Jmsg(jcr, M_ERROR, 0, "%s", errmsg);
    goto bail_out;
  }

  *num_ids = SqlNumRows();
  if (*num_ids > 0) {
    id = (DBId_t*)malloc(*num_ids * sizeof(DBId_t));
    while ((row = SqlFetchRow()) != NULL) { id[i++] = str_to_uint64(row[0]); }
    *ids = id;
  }
  SqlFreeResult();
  ok = true;
bail_out:
  DbUnlock(this);
  return ok;
}


/**
 * This function returns a list of all the DBIds that are returned for the
 * query.
 *
 * Returns false: on failure
 *         true:  on success
 */
bool BareosDb::GetQueryDbids(JobControlRecord* jcr,
                             PoolMem& query,
                             dbid_list& ids)
{
  SQL_ROW row;
  int i = 0;
  bool ok = false;

  DbLock(this);
  ids.num_ids = 0;
  if (QUERY_DB(jcr, query.c_str())) {
    ids.num_ids = SqlNumRows();
    if (ids.num_ids > 0) {
      if (ids.max_ids < ids.num_ids) {
        free(ids.DBId);
        ids.DBId = (DBId_t*)malloc(ids.num_ids * sizeof(DBId_t));
      }
      while ((row = SqlFetchRow()) != NULL) {
        ids.DBId[i++] = str_to_uint64(row[0]);
      }
    }
    SqlFreeResult();
    ok = true;
  } else {
    Mmsg(errmsg, _("query dbids failed: ERR=%s\n"), sql_strerror());
    Jmsg(jcr, M_ERROR, 0, "%s", errmsg);
    ok = false;
  }
  DbUnlock(this);
  return ok;
}


/**
 * Get Media Record
 *
 * Returns: false: on failure
 *          true:  on success
 */
bool BareosDb::GetMediaRecord(JobControlRecord* jcr, MediaDbRecord* mr)
{
  bool retval = false;
  SQL_ROW row;
  char ed1[50];
  int num_rows;
  char esc[MAX_ESCAPE_NAME_LENGTH];

  DbLock(this);
  if (mr->MediaId == 0 && mr->VolumeName[0] == 0) {
    Mmsg(cmd, "SELECT count(*) from Media");
    mr->MediaId = GetSqlRecordMax(jcr);
    retval = true;
    goto bail_out;
  }
  if (mr->MediaId != 0) { /* find by id */
    Mmsg(cmd,
         "SELECT MediaId,VolumeName,VolJobs,VolFiles,VolBlocks,"
         "VolBytes,VolMounts,VolErrors,VolWrites,MaxVolBytes,VolCapacityBytes,"
         "MediaType,VolStatus,PoolId,VolRetention,VolUseDuration,MaxVolJobs,"
         "MaxVolFiles,Recycle,Slot,FirstWritten,LastWritten,InChanger,"
         "EndFile,EndBlock,LabelType,LabelDate,StorageId,"
         "Enabled,LocationId,RecycleCount,InitialWrite,"
         "ScratchPoolId,RecyclePoolId,VolReadTime,VolWriteTime,"
         "ActionOnPurge,EncryptionKey,MinBlocksize,MaxBlocksize "
         "FROM Media WHERE MediaId=%s",
         edit_int64(mr->MediaId, ed1));
  } else { /* find by name */
    EscapeString(jcr, esc, mr->VolumeName, strlen(mr->VolumeName));
    Mmsg(cmd,
         "SELECT MediaId,VolumeName,VolJobs,VolFiles,VolBlocks,"
         "VolBytes,VolMounts,VolErrors,VolWrites,MaxVolBytes,VolCapacityBytes,"
         "MediaType,VolStatus,PoolId,VolRetention,VolUseDuration,MaxVolJobs,"
         "MaxVolFiles,Recycle,Slot,FirstWritten,LastWritten,InChanger,"
         "EndFile,EndBlock,LabelType,LabelDate,StorageId,"
         "Enabled,LocationId,RecycleCount,InitialWrite,"
         "ScratchPoolId,RecyclePoolId,VolReadTime,VolWriteTime,"
         "ActionOnPurge,EncryptionKey,MinBlocksize,MaxBlocksize "
         "FROM Media WHERE VolumeName='%s'",
         esc);
  }

  if (QUERY_DB(jcr, cmd)) {
    char ed1[50];
    num_rows = SqlNumRows();
    if (num_rows > 1) {
      Mmsg1(errmsg, _("More than one Volume!: %s\n"),
            edit_uint64(num_rows, ed1));
      Jmsg(jcr, M_ERROR, 0, "%s", errmsg);
    } else if (num_rows == 1) {
      if ((row = SqlFetchRow()) == NULL) {
        Mmsg1(errmsg, _("error fetching row: %s\n"), sql_strerror());
        Jmsg(jcr, M_ERROR, 0, "%s", errmsg);
      } else {
        /* return values */
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
        mr->ActionOnPurge = str_to_int32(row[36]);
        bstrncpy(mr->EncrKey, (row[37] != NULL) ? row[37] : "",
                 sizeof(mr->EncrKey));
        mr->MinBlocksize = str_to_int32(row[38]);
        mr->MaxBlocksize = str_to_int32(row[39]);
        retval = true;
      }
    } else {
      if (mr->MediaId != 0) {
        Mmsg1(errmsg, _("Media record MediaId=%s not found.\n"),
              edit_int64(mr->MediaId, ed1));
      } else {
        Mmsg1(errmsg, _("Media record for Volume \"%s\" not found.\n"),
              mr->VolumeName);
      }
    }
    SqlFreeResult();
  } else {
    if (mr->MediaId != 0) {
      Mmsg(errmsg, _("Media record for MediaId=%u not found in Catalog.\n"),
           mr->MediaId);
    } else {
      Mmsg(errmsg, _("Media record for Vol=%s not found in Catalog.\n"),
           mr->VolumeName);
    }
  }

bail_out:
  DbUnlock(this);
  return retval;
}

/**
 * Remove all MD5 from a query (can save lot of memory with many files)
 */
static void strip_md5(char* q)
{
  char* p = q;
  while ((p = strstr(p, ", MD5"))) { memset(p, ' ', 5 * sizeof(char)); }
}

/**
 * Find the last "accurate" backup state (that can take deleted files in
 * account)
 * 1) Get all files with jobid in list (F subquery)
 *    Get all files in BaseFiles with jobid in list
 * 2) Take only the last version of each file (Temp subquery) => accurate list
 *    is ok
 * 3) Join the result to file table to get fileindex, jobid and lstat
 * information
 *
 * TODO: See if we can do the SORT only if needed (as an argument)
 */
bool BareosDb::GetFileList(JobControlRecord* jcr,
                           const char* jobids,
                           bool use_md5,
                           bool use_delta,
                           DB_RESULT_HANDLER* ResultHandler,
                           void* ctx)
{
  PoolMem query(PM_MESSAGE);
  PoolMem query2(PM_MESSAGE);

  if (!*jobids) {
    DbLock(this);
    Mmsg(errmsg, _("ERR=JobIds are empty\n"));
    DbUnlock(this);
    return false;
  }

  if (use_delta) {
    FillQuery(query2, SQL_QUERY::select_recent_version_with_basejob_and_delta,
              jobids, jobids, jobids, jobids);
  } else {
    FillQuery(query2, SQL_QUERY::select_recent_version_with_basejob, jobids,
              jobids, jobids, jobids);
  }

  /*
   * BootStrapRecord code is optimized for JobId sorted, with Delta, we need to
   * get them ordered by date. JobTDate and JobId can be mixed if using Copy or
   * Migration
   */
  Mmsg(query,
       "SELECT Path.Path, T1.Name, T1.FileIndex, T1.JobId, LStat, DeltaSeq, "
       "MD5, Fhinfo, Fhnode "
       "FROM ( %s ) AS T1 "
       "JOIN Path ON (Path.PathId = T1.PathId) "
       "WHERE FileIndex > 0 "
       "ORDER BY T1.JobTDate, FileIndex ASC", /* Return sorted by JobTDate */
                                              /* FileIndex for restore code */
       query2.c_str());

  if (!use_md5) { strip_md5(query.c_str()); }

  Dmsg1(100, "q=%s\n", query.c_str());

  return BigSqlQuery(query.c_str(), ResultHandler, ctx);
}

/**
 * This procedure gets the base jobid list used by jobids,
 */
bool BareosDb::GetUsedBaseJobids(JobControlRecord* jcr,
                                 const char* jobids,
                                 db_list_ctx* result)
{
  PoolMem query(PM_MESSAGE);

  Mmsg(query,
       "SELECT DISTINCT BaseJobId "
       "  FROM Job JOIN BaseFiles USING (JobId) "
       " WHERE Job.HasBase = 1 "
       "   AND Job.JobId IN (%s) ",
       jobids);
  return SqlQueryWithHandler(query.c_str(), DbListHandler, result);
}

/**
 * The decision do change an incr/diff was done before
 * Full : do nothing
 * Differential : get the last full id
 * Incremental : get the last full + last diff + last incr(s) ids
 *
 * If you specify jr->StartTime, it will be used to limit the search
 * in the time. (usually now)
 *
 * If you specify jr->limit, it will be used to limit the list of jobids
 * to a that number
 *
 * TODO: look and merge from ua_restore.c
 */
bool BareosDb::AccurateGetJobids(JobControlRecord* jcr,
                                 JobDbRecord* jr,
                                 db_list_ctx* jobids)
{
  bool retval = false;
  char clientid[50], jobid[50], filesetid[50];
  char date[MAX_TIME_LENGTH];
  PoolMem query(PM_MESSAGE);

  /* Take the current time as upper limit if nothing else specified */
  utime_t StartTime = (jr->StartTime) ? jr->StartTime : time(NULL);

  bstrutime(date, sizeof(date), StartTime + 1);
  jobids->clear();

  /*
   * First, find the last good Full backup for this job/client/fileset
   */
  FillQuery(query, SQL_QUERY::create_temp_accurate_jobids,
            edit_uint64(jcr->JobId, jobid), edit_uint64(jr->ClientId, clientid),
            date, edit_uint64(jr->FileSetId, filesetid));

  if (!SqlQuery(query.c_str())) { goto bail_out; }

  if (jr->JobLevel == L_INCREMENTAL || jr->JobLevel == L_VIRTUAL_FULL) {
    /*
     * Now, find the last differential backup after the last full
     */
    Mmsg(query,
         "INSERT INTO btemp3%s (JobId, StartTime, EndTime, JobTDate, "
         "PurgedFiles) "
         "SELECT JobId, StartTime, EndTime, JobTDate, PurgedFiles "
         "FROM Job JOIN FileSet USING (FileSetId) "
         "WHERE ClientId = %s "
         "AND JobFiles > 0 "
         "AND Level='D' AND JobStatus IN ('T','W') AND Type='B' "
         "AND StartTime > (SELECT EndTime FROM btemp3%s ORDER BY EndTime DESC "
         "LIMIT 1) "
         "AND StartTime < '%s' "
         "AND FileSet.FileSet= (SELECT FileSet FROM FileSet WHERE FileSetId = "
         "%s) "
         "ORDER BY Job.JobTDate DESC LIMIT 1 ",
         jobid, clientid, jobid, date, filesetid);

    if (!SqlQuery(query.c_str())) { goto bail_out; }

    /*
     * We just have to take all incremental after the last Full/Diff
     *
     * If we are doing always incremental, we need to limit the search to
     * only include incrementals that are older than (now -
     * AlwaysIncrementalInterval) and leave AlwaysIncrementalNumber incrementals
     */
    Mmsg(query,
         "INSERT INTO btemp3%s (JobId, StartTime, EndTime, JobTDate, "
         "PurgedFiles) "
         "SELECT JobId, StartTime, EndTime, JobTDate, PurgedFiles "
         "FROM Job JOIN FileSet USING (FileSetId) "
         "WHERE ClientId = %s "
         "AND JobFiles > 0 "
         "AND Level='I' AND JobStatus IN ('T','W') AND Type='B' "
         "AND StartTime > (SELECT EndTime FROM btemp3%s ORDER BY EndTime DESC "
         "LIMIT 1) "
         "AND StartTime < '%s' "
         "AND FileSet.FileSet= (SELECT FileSet FROM FileSet WHERE FileSetId = "
         "%s) "
         "ORDER BY Job.JobTDate DESC ",
         jobid, clientid, jobid, date, filesetid);
    if (!SqlQuery(query.c_str())) { goto bail_out; }
  }

  /*
   * Build a jobid list ie: 1,2,3,4
   */
  if (jr->limit) {
    Mmsg(query, "SELECT JobId FROM btemp3%s ORDER by JobTDate LIMIT %d", jobid,
         jr->limit);
  } else {
    Mmsg(query, "SELECT JobId FROM btemp3%s ORDER by JobTDate", jobid);
  }
  SqlQueryWithHandler(query.c_str(), DbListHandler, jobids);
  Dmsg1(1, "db_accurate_get_jobids=%s\n", jobids->GetAsString().c_str());
  retval = true;

bail_out:
  Mmsg(query, "DROP TABLE btemp3%s", jobid);
  SqlQuery(query.c_str());
  return retval;
}

bool BareosDb::GetBaseFileList(JobControlRecord* jcr,
                               bool use_md5,
                               DB_RESULT_HANDLER* ResultHandler,
                               void* ctx)
{
  PoolMem query(PM_MESSAGE);

  Mmsg(query,
       "SELECT Path, Name, FileIndex, JobId, LStat, 0 As DeltaSeq, MD5, "
       "Fhinfo, Fhnode "
       "FROM new_basefile%lld ORDER BY JobId, FileIndex ASC",
       (uint64_t)jcr->JobId);

  if (!use_md5) { strip_md5(query.c_str()); }
  return BigSqlQuery(query.c_str(), ResultHandler, ctx);
}

bool BareosDb::GetBaseJobid(JobControlRecord* jcr,
                            JobDbRecord* jr,
                            JobId_t* jobid)
{
  PoolMem query(PM_MESSAGE);
  utime_t StartTime;
  db_int64_ctx lctx;
  char date[MAX_TIME_LENGTH];
  char esc[MAX_ESCAPE_NAME_LENGTH];
  bool retval = false;
  // char clientid[50], filesetid[50];

  *jobid = 0;
  lctx.count = 0;
  lctx.value = 0;

  StartTime = (jr->StartTime) ? jr->StartTime : time(NULL);
  bstrutime(date, sizeof(date), StartTime + 1);
  EscapeString(jcr, esc, jr->Name, strlen(jr->Name));

  /* we can take also client name, fileset, etc... */

  Mmsg(query,
       "SELECT JobId, Job, StartTime, EndTime, JobTDate, PurgedFiles "
       "FROM Job "
       // "JOIN FileSet USING (FileSetId) JOIN Client USING (ClientId) "
       "WHERE Job.Name = '%s' "
       "AND Level='B' AND JobStatus IN ('T','W') AND Type='B' "
       //    "AND FileSet.FileSet= '%s' "
       //    "AND Client.Name = '%s' "
       "AND StartTime<'%s' "
       "ORDER BY Job.JobTDate DESC LIMIT 1",
       esc,
       //      edit_uint64(jr->ClientId, clientid),
       //      edit_uint64(jr->FileSetId, filesetid));
       date);

  Dmsg1(10, "GetBaseJobid q=%s\n", query.c_str());
  if (!SqlQueryWithHandler(query.c_str(), db_int64_handler, &lctx)) {
    goto bail_out;
  }
  *jobid = (JobId_t)lctx.value;

  Dmsg1(10, "GetBaseJobid=%lld\n", *jobid);
  retval = true;

bail_out:
  return retval;
}

/**
 * Get JobIds associated with a volume
 */
bool BareosDb::GetVolumeJobids(JobControlRecord* jcr,
                               MediaDbRecord* mr,
                               db_list_ctx* lst)
{
  char ed1[50];
  bool retval;

  DbLock(this);
  Mmsg(cmd, "SELECT DISTINCT JobId FROM JobMedia WHERE MediaId=%s",
       edit_int64(mr->MediaId, ed1));
  retval = SqlQueryWithHandler(cmd, DbListHandler, lst);
  DbUnlock(this);
  return retval;
}

/**
 * This function returns the sum of all the Clients JobBytes.
 *
 * Returns false: on failure
 *         true: on success
 */
bool BareosDb::get_quota_jobbytes(JobControlRecord* jcr,
                                  JobDbRecord* jr,
                                  utime_t JobRetention)
{
  SQL_ROW row;
  int num_rows;
  char dt[MAX_TIME_LENGTH];
  char ed1[50], ed2[50];
  bool retval = false;
  time_t now, schedtime;

  /*
   * Determine the first schedtime we are interested in.
   */
  now = time(NULL);
  schedtime = now - JobRetention;

  /*
   * Bugfix, theres a small timing bug in the scheduler.
   * Add 5 seconds to the schedtime to ensure the
   * last job from the job retention gets excluded.
   */
  schedtime += 5;

  bstrutime(dt, sizeof(dt), schedtime);

  DbLock(this);

  FillQuery(SQL_QUERY::get_quota_jobbytes, edit_uint64(jr->ClientId, ed1),
            edit_uint64(jr->JobId, ed2), dt);
  if (QUERY_DB(jcr, cmd)) {
    num_rows = SqlNumRows();
    if (num_rows == 1) {
      row = SqlFetchRow();
      jr->JobSumTotalBytes = str_to_uint64(row[0]);
    } else if (num_rows < 1) {
      jr->JobSumTotalBytes = 0;
    }
    SqlFreeResult();
    retval = true;
  } else {
    Mmsg(errmsg, _("JobBytes sum select failed: ERR=%s\n"), sql_strerror());
    Jmsg(jcr, M_ERROR, 0, "%s", errmsg);
  }

  DbUnlock(this);
  return retval;
}

/**
 * This function returns the sum of all the Clients JobBytes of non failed jobs.
 *
 * Returns false: on failure
 *         true: on success
 */
bool BareosDb::get_quota_jobbytes_nofailed(JobControlRecord* jcr,
                                           JobDbRecord* jr,
                                           utime_t JobRetention)
{
  SQL_ROW row;
  char ed1[50], ed2[50];
  int num_rows;
  char dt[MAX_TIME_LENGTH];
  bool retval = false;
  time_t now, schedtime;

  /*
   * Determine the first schedtime we are interested in.
   */
  now = time(NULL);
  schedtime = now - JobRetention;

  /*
   * Bugfix, theres a small timing bug in the scheduler.
   * Add 5 seconds to the schedtime to ensure the
   * last job from the job retention gets excluded.
   */
  schedtime += 5;

  bstrutime(dt, sizeof(dt), schedtime);

  DbLock(this);

  FillQuery(SQL_QUERY::get_quota_jobbytes_nofailed,
            edit_uint64(jr->ClientId, ed1), edit_uint64(jr->JobId, ed2), dt);
  if (QUERY_DB(jcr, cmd)) {
    num_rows = SqlNumRows();
    if (num_rows == 1) {
      row = SqlFetchRow();
      jr->JobSumTotalBytes = str_to_uint64(row[0]);
    } else if (num_rows < 1) {
      jr->JobSumTotalBytes = 0;
    }
    SqlFreeResult();
    retval = true;
  } else {
    Mmsg(errmsg, _("JobBytes sum select failed: ERR=%s\n"), sql_strerror());
    Jmsg(jcr, M_ERROR, 0, "%s", errmsg);
  }

  DbUnlock(this);
  return retval;
}

/**
 * Fetch the quota value and grace time for a quota.
 * Returns false: on failure
 *         true: on success
 */
bool BareosDb::GetQuotaRecord(JobControlRecord* jcr, ClientDbRecord* cdbr)
{
  SQL_ROW row;
  char ed1[50];
  int num_rows;
  bool retval = false;

  DbLock(this);
  Mmsg(cmd,
       "SELECT GraceTime, QuotaLimit "
       "FROM Quota "
       "WHERE ClientId = %s",
       edit_int64(cdbr->ClientId, ed1));
  if (QUERY_DB(jcr, cmd)) {
    num_rows = SqlNumRows();
    if (num_rows == 1) {
      if ((row = SqlFetchRow()) == NULL) {
        Mmsg1(errmsg, _("error fetching row: %s\n"), sql_strerror());
        Jmsg(jcr, M_ERROR, 0, "%s", errmsg);
        SqlFreeResult();
      } else {
        cdbr->GraceTime = str_to_uint64(row[0]);
        cdbr->QuotaLimit = str_to_int64(row[1]);
        SqlFreeResult();
        retval = true;
      }
    } else {
      Mmsg(errmsg, _("Quota record not found in Catalog.\n"));
      SqlFreeResult();
    }
  } else {
    Mmsg(errmsg, _("Quota record not found in Catalog.\n"));
  }

  DbUnlock(this);
  return retval;
}

/**
 * Fetch the NDMP Dump Level value.
 *
 * Returns dumplevel on success
 *         0: on failure
 */
int BareosDb::GetNdmpLevelMapping(JobControlRecord* jcr,
                                  JobDbRecord* jr,
                                  char* filesystem)
{
  SQL_ROW row;
  char ed1[50], ed2[50];
  int num_rows;
  int dumplevel = 0;

  DbLock(this);

  esc_name = CheckPoolMemorySize(esc_name, strlen(filesystem) * 2 + 1);
  EscapeString(jcr, esc_name, filesystem, strlen(filesystem));

  Mmsg(cmd,
       "SELECT DumpLevel FROM NDMPLevelMap WHERE "
       "ClientId='%s' AND FileSetId='%s' AND FileSystem='%s'",
       edit_uint64(jr->ClientId, ed1), edit_uint64(jr->FileSetId, ed2),
       esc_name);

  if (QUERY_DB(jcr, cmd)) {
    num_rows = SqlNumRows();
    if (num_rows == 1) {
      if ((row = SqlFetchRow()) == NULL) {
        Mmsg1(errmsg, _("error fetching row: %s\n"), sql_strerror());
        Jmsg(jcr, M_ERROR, 0, "%s", errmsg);
        SqlFreeResult();
        goto bail_out;
      } else {
        dumplevel = str_to_uint64(row[0]);
        dumplevel++; /* select next dumplevel */
        SqlFreeResult();
        goto bail_out;
      }
    } else {
      Mmsg(errmsg, _("NDMP Dump Level record not found in Catalog.\n"));
      SqlFreeResult();
      goto bail_out;
    }
  } else {
    Mmsg(errmsg, _("NDMP Dump Level record not found in Catalog.\n"));
    goto bail_out;
  }

bail_out:
  DbUnlock(this);
  return dumplevel;
}

/**
 * CountingHandler() with a CountContext* can be used to count the number of
 * times that SqlQueryWithHandler() calls the handler.
 * This is not neccesarily the number of rows, because the ResultHandler can
 * stop processing of further rows by returning non-zero.
 */
struct CountContext {
  DB_RESULT_HANDLER* handler;
  void* ctx;
  int count;

  CountContext(DB_RESULT_HANDLER* t_handler, void* t_ctx)
      : handler(t_handler), ctx(t_ctx), count(0)
  {
  }
};

static int CountingHandler(void* counting_ctx, int num_fields, char** rows)
{
  auto* c = static_cast<struct CountContext*>(counting_ctx);
  c->count++;
  return c->handler(c->ctx, num_fields, rows);
}

/**
 * Fetch NDMP Job Environment based on raw SQL query.
 * Returns false: on sql failure or when 0 rows were returned
 *         true:  otherwise
 */
bool BareosDb::GetNdmpEnvironmentString(const std::string& query,
                                        DB_RESULT_HANDLER* ResultHandler,
                                        void* ctx)
{
  auto myctx = std::make_unique<CountContext>(ResultHandler, ctx);
  bool status =
      SqlQueryWithHandler(query.c_str(), CountingHandler, myctx.get());
  Dmsg3(150, "Got %d NDMP environment records\n", myctx->count);
  return status && myctx->count > 0;  // no rows means no environment was found
}

/**
 * Fetch the NDMP Job Environment Strings based on JobId only
 *
 * Returns false: on failure
 *         true: on success
 */
bool BareosDb::GetNdmpEnvironmentString(JobId_t JobId,
                                        DB_RESULT_HANDLER* ResultHandler,
                                        void* ctx)
{
  ASSERT(JobId > 0)
  std::string query{"SELECT EnvName, EnvValue FROM NDMPJobEnvironment"};
  query += " WHERE JobId=" + std::to_string(JobId);

  return GetNdmpEnvironmentString(query, ResultHandler, ctx);
}

/**
 * Fetch the NDMP Job Environment Strings based on JobId and FileIndex
 *
 * Returns false: on failure
 *         true: on success
 */
bool BareosDb::GetNdmpEnvironmentString(JobId_t JobId,
                                        int32_t FileIndex,
                                        DB_RESULT_HANDLER* ResultHandler,
                                        void* ctx)
{
  ASSERT(JobId > 0)
  std::string query{"SELECT EnvName, EnvValue FROM NDMPJobEnvironment"};
  query += " WHERE JobId=" + std::to_string(JobId);
  query += " AND FileIndex=" + std::to_string(FileIndex);

  return GetNdmpEnvironmentString(query, ResultHandler, ctx);
}


/**
 * Fetch the NDMP Job Environment Strings for NDMP_BAREOS Backups
 *
 * Returns false: on failure
 *         true: on success
 */
bool BareosDb::GetNdmpEnvironmentString(const VolumeSessionInfo& vsi,
                                        int32_t FileIndex,
                                        DB_RESULT_HANDLER* ResultHandler,
                                        void* ctx)
{
  db_int64_ctx lctx;
  std::string query{"SELECT JobId FROM Job"};
  query += " WHERE VolSessionId = " + std::to_string(vsi.id);
  query += " AND VolSessionTime = " + std::to_string(vsi.time);

  if (SqlQueryWithHandler(query.c_str(), db_int64_handler, &lctx)) {
    if (lctx.count == 1) {
      /* now lctx.value contains the jobid we restore */
      return GetNdmpEnvironmentString(lctx.value, FileIndex, ResultHandler,
                                      ctx);
    }
  }
  Dmsg3(
      100,
      "Got %d JobIds for VolSessionTime=%lld VolSessionId=%lld instead of 1\n",
      lctx.count, vsi.time, vsi.id);
  return false;
}

/**
 * This function creates a sql query string at cmd to return a list of all the
 * Media records for the current Pool, the correct Media Type, Recyle, Enabled,
 * StorageId, VolBytes and volumes or VolumeName if specified. Comma separated
 * list of volumes takes precedence over VolumeName. The caller must free ids if
 * non-NULL.
 */
bool BareosDb::PrepareMediaSqlQuery(JobControlRecord* jcr,
                                    MediaDbRecord* mr,
                                    PoolMem* querystring,
                                    PoolMem& volumes)
{
  bool ok = true;
  char ed1[50];
  char esc[MAX_NAME_LENGTH * 2 + 1];
  PoolMem buf(PM_MESSAGE);

  /*
   * columns we care of.
   * Reduced, to be better displayable.
   * Important:
   * column 2: pool.name, column 3: storage.name,
   * as this is used for ACL handling (counting starts at 0).
   */
  const char* columns =
      "Media.MediaId,"
      "Media.VolumeName,"
      "Pool.Name AS Pool,"
      "Storage.Name AS Storage,"
      "Media.MediaType,"
      /* "Media.DeviceId," */
      /* "Media.FirstWritten, "*/
      "Media.LastWritten,"
      "Media.VolFiles,"
      "Media.VolBytes,"
      "Media.VolStatus,"
      /* "Media.Recycle AS Recycle," */
      "Media.ActionOnPurge,"
      /* "Media.VolRetention," */
      "Media.Comment";

  Mmsg(querystring,
       "SELECT DISTINCT %s FROM Media "
       "LEFT JOIN Pool USING(PoolId) "
       "LEFT JOIN Storage USING(StorageId) "
       "WHERE Media.Recycle=%d AND Media.Enabled=%d ",
       columns, mr->Recycle, mr->Enabled);

  if (*mr->MediaType) {
    EscapeString(jcr, esc, mr->MediaType, strlen(mr->MediaType));
    Mmsg(buf, "AND Media.MediaType='%s' ", esc);
    PmStrcat(querystring, buf.c_str());
  }

  if (mr->StorageId) {
    Mmsg(buf, "AND Media.StorageId=%s ", edit_uint64(mr->StorageId, ed1));
    PmStrcat(querystring, buf.c_str());
  }

  if (mr->PoolId) {
    Mmsg(buf, "AND Media.PoolId=%s ", edit_uint64(mr->PoolId, ed1));
    PmStrcat(querystring, buf.c_str());
  }

  if (mr->VolBytes) {
    Mmsg(buf, "AND Media.VolBytes > %s ", edit_uint64(mr->VolBytes, ed1));
    PmStrcat(querystring, buf.c_str());
  }

  if (*mr->VolStatus) {
    EscapeString(jcr, esc, mr->VolStatus, strlen(mr->VolStatus));
    Mmsg(buf, "AND Media.VolStatus = '%s' ", esc);
    PmStrcat(querystring, buf.c_str());
  }

  if (volumes.strlen() > 0) {
    /* extra list of volumes given */
    Mmsg(buf, "AND Media.VolumeName IN (%s) ", volumes.c_str());
    PmStrcat(querystring, buf.c_str());
  } else if (*mr->VolumeName) {
    /* single volume given in media record */
    EscapeString(jcr, esc, mr->VolumeName, strlen(mr->VolumeName));
    Mmsg(buf, "AND Media.VolumeName = '%s' ", esc);
    PmStrcat(querystring, buf.c_str());
  }

  Dmsg1(100, "query=%s\n", querystring);

  return ok;
}

/**
 * verify that all media use the same storage.
 */
bool BareosDb::VerifyMediaIdsFromSingleStorage(JobControlRecord* jcr,
                                               dbid_list& mediaIds)
{
  MediaDbRecord mr;
  DBId_t storageId = 0;

  for (int i = 0; i < mediaIds.size(); i++) {
    mr.MediaId = mediaIds.get(i);
    if (!GetMediaRecord(jcr, &mr)) {
      Mmsg1(errmsg, _("Failed to find MediaId=%lld\n"), (uint64_t)mr.MediaId);
      Jmsg(jcr, M_ERROR, 0, "%s", errmsg);
      return false;
    } else if (i == 0) {
      storageId = mr.StorageId;
    } else if (storageId != mr.StorageId) {
      return false;
    }
  }
  return true;
}


#endif /* HAVE_SQLITE3 || HAVE_MYSQL || HAVE_POSTGRESQL || HAVE_INGRES || \
          HAVE_DBI */
