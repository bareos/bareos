/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2012 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2013 Bareos GmbH & Co. KG

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
 * BAREOS Catalog Database Update record interface routines
 *
 * Kern Sibbald, March 2000
 */

#include "bareos.h"

#if HAVE_SQLITE3 || HAVE_MYSQL || HAVE_POSTGRESQL || HAVE_INGRES || HAVE_DBI

#include "cats.h"
#include "bdb_priv.h"
#include "sql_glue.h"

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
bool db_add_digest_to_file_record(JCR *jcr, B_DB *mdb,
                                  FileId_t FileId,
                                  char *digest, int type)
{
   bool retval;
   char ed1[50];
   int len = strlen(digest);

   db_lock(mdb);
   mdb->esc_name = check_pool_memory_size(mdb->esc_name, len*2+1);
   mdb->db_escape_string(jcr, mdb->esc_name, digest, len);
   Mmsg(mdb->cmd, "UPDATE File SET MD5='%s' WHERE FileId=%s", mdb->esc_name,
        edit_int64(FileId, ed1));
   retval = UPDATE_DB(jcr, mdb, mdb->cmd);
   db_unlock(mdb);
   return retval;
}

/* Mark the file record as being visited during database
 * verify compare. Stuff JobId into the MarkId field
 */
bool db_mark_file_record(JCR *jcr, B_DB *mdb, FileId_t FileId, JobId_t JobId)
{
   bool retval;
   char ed1[50], ed2[50];

   db_lock(mdb);
   Mmsg(mdb->cmd, "UPDATE File SET MarkId=%s WHERE FileId=%s",
      edit_int64(JobId, ed1), edit_int64(FileId, ed2));
   retval = UPDATE_DB(jcr, mdb, mdb->cmd);
   db_unlock(mdb);
   return retval;
}

/*
 * Update the Job record at start of Job
 *
 * Returns: false on failure
 *          true  on success
 */
bool db_update_job_start_record(JCR *jcr, B_DB *mdb, JOB_DBR *jr)
{
   char dt[MAX_TIME_LENGTH];
   time_t stime;
   struct tm tm;
   btime_t JobTDate;
   bool retval;
   char ed1[50], ed2[50], ed3[50], ed4[50], ed5[50];

   stime = jr->StartTime;
   (void)localtime_r(&stime, &tm);
   strftime(dt, sizeof(dt), "%Y-%m-%d %H:%M:%S", &tm);
   JobTDate = (btime_t)stime;

   db_lock(mdb);
   Mmsg(mdb->cmd, "UPDATE Job SET JobStatus='%c',Level='%c',StartTime='%s',"
"ClientId=%s,JobTDate=%s,PoolId=%s,FileSetId=%s WHERE JobId=%s",
      (char)(jcr->JobStatus),
      (char)(jr->JobLevel), dt,
      edit_int64(jr->ClientId, ed1),
      edit_uint64(JobTDate, ed2),
      edit_int64(jr->PoolId, ed3),
      edit_int64(jr->FileSetId, ed4),
      edit_int64(jr->JobId, ed5));

   retval = UPDATE_DB(jcr, mdb, mdb->cmd);
   mdb->changes = 0;
   db_unlock(mdb);
   return retval;
}

/*
 * Update Long term statistics with all jobs that were run before
 * age seconds
 */
int db_update_stats(JCR *jcr, B_DB *mdb, utime_t age)
{
   char ed1[30];
   int rows;
   utime_t now = (utime_t)time(NULL);
   edit_uint64(now - age, ed1);

   db_lock(mdb);

   Mmsg(mdb->cmd, fill_jobhisto, ed1);
   QUERY_DB(jcr, mdb, mdb->cmd); /* TODO: get a message ? */
   rows = sql_affected_rows(mdb);

   db_unlock(mdb);
   return rows;
}

/*
 * Update the Job record at end of Job
 *
 * Returns: false on failure
 *          true on success
 */
bool db_update_job_end_record(JCR *jcr, B_DB *mdb, JOB_DBR *jr)
{
   bool retval;
   char dt[MAX_TIME_LENGTH];
   char rdt[MAX_TIME_LENGTH];
   time_t ttime;
   struct tm tm;
   char ed1[30], ed2[30], ed3[50], ed4[50];
   btime_t JobTDate;
   char PriorJobId[50];

   if (jr->PriorJobId) {
      bstrncpy(PriorJobId, edit_int64(jr->PriorJobId, ed1), sizeof(PriorJobId));
   } else {
      bstrncpy(PriorJobId, "0", sizeof(PriorJobId));
   }

   ttime = jr->EndTime;
   (void)localtime_r(&ttime, &tm);
   strftime(dt, sizeof(dt), "%Y-%m-%d %H:%M:%S", &tm);

   if (jr->RealEndTime < jr->EndTime) {
      jr->RealEndTime = jr->EndTime;
   }
   ttime = jr->RealEndTime;
   (void)localtime_r(&ttime, &tm);
   strftime(rdt, sizeof(rdt), "%Y-%m-%d %H:%M:%S", &tm);

   JobTDate = ttime;

   db_lock(mdb);
   Mmsg(mdb->cmd,
      "UPDATE Job SET JobStatus='%c',EndTime='%s',"
"ClientId=%u,JobBytes=%s,ReadBytes=%s,JobFiles=%u,JobErrors=%u,VolSessionId=%u,"
"VolSessionTime=%u,PoolId=%u,FileSetId=%u,JobTDate=%s,"
"RealEndTime='%s',PriorJobId=%s,HasBase=%u,PurgedFiles=%u WHERE JobId=%s",
      (char)(jr->JobStatus), dt, jr->ClientId, edit_uint64(jr->JobBytes, ed1),
      edit_uint64(jr->ReadBytes, ed4),
      jr->JobFiles, jr->JobErrors, jr->VolSessionId, jr->VolSessionTime,
      jr->PoolId, jr->FileSetId, edit_uint64(JobTDate, ed2),
      rdt, PriorJobId, jr->HasBase, jr->PurgedFiles,
      edit_int64(jr->JobId, ed3));

   retval = UPDATE_DB(jcr, mdb, mdb->cmd);

   db_unlock(mdb);
   return retval;
}

/*
 * Update Client record
 * Returns: false on failure
 *          true on success
 */
bool db_update_client_record(JCR *jcr, B_DB *mdb, CLIENT_DBR *cr)
{
   bool retval = false;
   char ed1[50], ed2[50];
   char esc_name[MAX_ESCAPE_NAME_LENGTH];
   char esc_uname[MAX_ESCAPE_NAME_LENGTH];
   CLIENT_DBR tcr;

   db_lock(mdb);
   memcpy(&tcr, cr, sizeof(tcr));
   if (!db_create_client_record(jcr, mdb, &tcr)) {
      goto bail_out;
   }

   mdb->db_escape_string(jcr, esc_name, cr->Name, strlen(cr->Name));
   mdb->db_escape_string(jcr, esc_uname, cr->Uname, strlen(cr->Uname));
   Mmsg(mdb->cmd,
"UPDATE Client SET AutoPrune=%d,FileRetention=%s,JobRetention=%s,"
"Uname='%s' WHERE Name='%s'",
      cr->AutoPrune,
      edit_uint64(cr->FileRetention, ed1),
      edit_uint64(cr->JobRetention, ed2),
      esc_uname, esc_name);

   retval = UPDATE_DB(jcr, mdb, mdb->cmd);

bail_out:
   db_unlock(mdb);
   return retval;
}

/*
 * Update Counters record
 * Returns: false on failure
 *          true on success
 */
bool db_update_counter_record(JCR *jcr, B_DB *mdb, COUNTER_DBR *cr)
{
   bool retval;
   char esc[MAX_ESCAPE_NAME_LENGTH];

   db_lock(mdb);
   mdb->db_escape_string(jcr, esc, cr->Counter, strlen(cr->Counter));
   Mmsg(mdb->cmd,
        update_counter_values[mdb->db_get_type_index()],
        cr->MinValue, cr->MaxValue, cr->CurrentValue,
        cr->WrapCounter, esc);

   retval = UPDATE_DB(jcr, mdb, mdb->cmd);
   db_unlock(mdb);
   return retval;
}

bool db_update_pool_record(JCR *jcr, B_DB *mdb, POOL_DBR *pr)
{
   bool retval;
   char ed1[50], ed2[50], ed3[50], ed4[50], ed5[50], ed6[50];
   char esc[MAX_ESCAPE_NAME_LENGTH];

   db_lock(mdb);
   mdb->db_escape_string(jcr, esc, pr->LabelFormat, strlen(pr->LabelFormat));

   Mmsg(mdb->cmd, "SELECT count(*) from Media WHERE PoolId=%s",
      edit_int64(pr->PoolId, ed4));
   pr->NumVols = get_sql_record_max(jcr, mdb);
   Dmsg1(400, "NumVols=%d\n", pr->NumVols);

   Mmsg(mdb->cmd,
"UPDATE Pool SET NumVols=%u,MaxVols=%u,UseOnce=%d,UseCatalog=%d,"
"AcceptAnyVolume=%d,VolRetention='%s',VolUseDuration='%s',"
"MaxVolJobs=%u,MaxVolFiles=%u,MaxVolBytes=%s,Recycle=%d,"
"AutoPrune=%d,LabelType=%d,LabelFormat='%s',RecyclePoolId=%s,"
"ScratchPoolId=%s,ActionOnPurge=%d,MinBlockSize=%d,MaxBlockSize=%d WHERE PoolId=%s",
      pr->NumVols, pr->MaxVols, pr->UseOnce, pr->UseCatalog,
      pr->AcceptAnyVolume, edit_uint64(pr->VolRetention, ed1),
      edit_uint64(pr->VolUseDuration, ed2),
      pr->MaxVolJobs, pr->MaxVolFiles,
      edit_uint64(pr->MaxVolBytes, ed3),
      pr->Recycle, pr->AutoPrune, pr->LabelType,
      esc, edit_int64(pr->RecyclePoolId,ed5),
      edit_int64(pr->ScratchPoolId,ed6),
      pr->ActionOnPurge,
      pr->MinBlocksize,
      pr->MaxBlocksize,
      ed4);
   retval = UPDATE_DB(jcr, mdb, mdb->cmd);
   db_unlock(mdb);
   return retval;
}

bool db_update_storage_record(JCR *jcr, B_DB *mdb, STORAGE_DBR *sr)
{
   bool retval;
   char ed1[50];

   db_lock(mdb);
   Mmsg(mdb->cmd, "UPDATE Storage SET AutoChanger=%d WHERE StorageId=%s",
      sr->AutoChanger, edit_int64(sr->StorageId, ed1));

   retval = UPDATE_DB(jcr, mdb, mdb->cmd);
   db_unlock(mdb);
   return retval;
}


/*
 * Update the Media Record at end of Session
 *
 * Returns: false on failure
 *          true on success
 */
bool db_update_media_record(JCR *jcr, B_DB *mdb, MEDIA_DBR *mr)
{
   bool retval;
   char dt[MAX_TIME_LENGTH];
   time_t ttime;
   struct tm tm;
   char ed1[50], ed2[50],  ed3[50],  ed4[50];
   char ed5[50], ed6[50],  ed7[50],  ed8[50];
   char ed9[50], ed10[50], ed11[50];
   char esc_name[MAX_ESCAPE_NAME_LENGTH];
   char esc_status[MAX_ESCAPE_NAME_LENGTH];

   Dmsg1(100, "update_media: FirstWritten=%d\n", mr->FirstWritten);
   db_lock(mdb);
   mdb->db_escape_string(jcr, esc_name, mr->VolumeName, strlen(mr->VolumeName));
   mdb->db_escape_string(jcr, esc_status, mr->VolStatus, strlen(mr->VolStatus));

   if (mr->set_first_written) {
      Dmsg1(400, "Set FirstWritten Vol=%s\n", mr->VolumeName);
      ttime = mr->FirstWritten;
      (void)localtime_r(&ttime, &tm);
      strftime(dt, sizeof(dt), "%Y-%m-%d %H:%M:%S", &tm);
      Mmsg(mdb->cmd, "UPDATE Media SET FirstWritten='%s'"
           " WHERE VolumeName='%s'", dt, esc_name);
      retval = UPDATE_DB(jcr, mdb, mdb->cmd);
      Dmsg1(400, "Firstwritten=%d\n", mr->FirstWritten);
   }

   /* Label just done? */
   if (mr->set_label_date) {
      ttime = mr->LabelDate;
      if (ttime == 0) {
         ttime = time(NULL);
      }
      (void)localtime_r(&ttime, &tm);
      strftime(dt, sizeof(dt), "%Y-%m-%d %H:%M:%S", &tm);
      Mmsg(mdb->cmd, "UPDATE Media SET LabelDate='%s' "
           "WHERE VolumeName='%s'", dt, esc_name);
      UPDATE_DB(jcr, mdb, mdb->cmd);
   }

   if (mr->LastWritten != 0) {
      ttime = mr->LastWritten;
      (void)localtime_r(&ttime, &tm);
      strftime(dt, sizeof(dt), "%Y-%m-%d %H:%M:%S", &tm);
      Mmsg(mdb->cmd, "UPDATE Media Set LastWritten='%s' "
           "WHERE VolumeName='%s'", dt, esc_name);
      UPDATE_DB(jcr, mdb, mdb->cmd);
   }

   Mmsg(mdb->cmd, "UPDATE Media SET VolJobs=%u,"
        "VolFiles=%u,VolBlocks=%u,VolBytes=%s,VolMounts=%u,VolErrors=%u,"
        "VolWrites=%u,MaxVolBytes=%s,VolStatus='%s',"
        "Slot=%d,InChanger=%d,VolReadTime=%s,VolWriteTime=%s,"
        "LabelType=%d,StorageId=%s,PoolId=%s,VolRetention=%s,VolUseDuration=%s,"
        "MaxVolJobs=%d,MaxVolFiles=%d,Enabled=%d,LocationId=%s,"
        "ScratchPoolId=%s,RecyclePoolId=%s,RecycleCount=%d,Recycle=%d,ActionOnPurge=%d,"
        "MinBlocksize=%u,MaxBlocksize=%u"
        " WHERE VolumeName='%s'",
        mr->VolJobs, mr->VolFiles, mr->VolBlocks, edit_uint64(mr->VolBytes, ed1),
        mr->VolMounts, mr->VolErrors, mr->VolWrites,
        edit_uint64(mr->MaxVolBytes, ed2),
        esc_status, mr->Slot, mr->InChanger,
        edit_int64(mr->VolReadTime, ed3),
        edit_int64(mr->VolWriteTime, ed4),
        mr->LabelType,
        edit_int64(mr->StorageId, ed5),
        edit_int64(mr->PoolId, ed6),
        edit_uint64(mr->VolRetention, ed7),
        edit_uint64(mr->VolUseDuration, ed8),
        mr->MaxVolJobs, mr->MaxVolFiles,
        mr->Enabled, edit_uint64(mr->LocationId, ed9),
        edit_uint64(mr->ScratchPoolId, ed10),
        edit_uint64(mr->RecyclePoolId, ed11),
        mr->RecycleCount, mr->Recycle, mr->ActionOnPurge,
        mr->MinBlocksize, mr->MaxBlocksize,
        esc_name);

   Dmsg1(400, "%s\n", mdb->cmd);

   retval = UPDATE_DB(jcr, mdb, mdb->cmd);

   /* Make sure InChanger is 0 for any record having the same Slot */
   db_make_inchanger_unique(jcr, mdb, mr);

   db_unlock(mdb);
   return retval;
}

/*
 * Update the Media Record Default values from Pool
 *
 * Returns: false on failure
 *          true on success
 */
bool db_update_media_defaults(JCR *jcr, B_DB *mdb, MEDIA_DBR *mr)
{
   bool retval;
   char ed1[50], ed2[50], ed3[50], ed4[50], ed5[50];
   char esc[MAX_ESCAPE_NAME_LENGTH];

   db_lock(mdb);
   if (mr->VolumeName[0]) {
      mdb->db_escape_string(jcr, esc, mr->VolumeName, strlen(mr->VolumeName));
      Mmsg(mdb->cmd, "UPDATE Media SET "
           "ActionOnPurge=%d,Recycle=%d,VolRetention=%s,VolUseDuration=%s,"
           "MaxVolJobs=%u,MaxVolFiles=%u,MaxVolBytes=%s,RecyclePoolId=%s,"
           "MinBlocksize=%d,MaxBlocksize=%d"
           " WHERE VolumeName='%s'",
           mr->ActionOnPurge, mr->Recycle,edit_uint64(mr->VolRetention, ed1),
           edit_uint64(mr->VolUseDuration, ed2),
           mr->MaxVolJobs, mr->MaxVolFiles,
           edit_uint64(mr->MaxVolBytes, ed3),
           edit_uint64(mr->RecyclePoolId, ed4),
           mr->MinBlocksize,
           mr->MaxBlocksize,
           esc);
   } else {
      Mmsg(mdb->cmd, "UPDATE Media SET "
           "ActionOnPurge=%d,Recycle=%d,VolRetention=%s,VolUseDuration=%s,"
           "MaxVolJobs=%u,MaxVolFiles=%u,MaxVolBytes=%s,RecyclePoolId=%s,"
           "MinBlocksize=%d,MaxBlocksize=%d"
           " WHERE PoolId=%s",
           mr->ActionOnPurge, mr->Recycle,edit_uint64(mr->VolRetention, ed1),
           edit_uint64(mr->VolUseDuration, ed2),
           mr->MaxVolJobs, mr->MaxVolFiles,
           edit_uint64(mr->MaxVolBytes, ed3),
           edit_int64(mr->RecyclePoolId, ed4),
           mr->MinBlocksize,
           mr->MaxBlocksize,
           edit_int64(mr->PoolId, ed5));
   }

   Dmsg1(400, "%s\n", mdb->cmd);

   retval = UPDATE_DB(jcr, mdb, mdb->cmd);

   db_unlock(mdb);
   return retval;
}


/*
 * If we have a non-zero InChanger, ensure that no other Media
 *  record has InChanger set on the same Slot.
 *
 * This routine assumes the database is already locked.
 */
void
db_make_inchanger_unique(JCR *jcr, B_DB *mdb, MEDIA_DBR *mr)
{
   char ed1[50], ed2[50];
   char esc[MAX_ESCAPE_NAME_LENGTH];
   if (mr->InChanger != 0 && mr->Slot != 0 && mr->StorageId != 0) {

       if (mr->MediaId != 0) {
          Mmsg(mdb->cmd, "UPDATE Media SET InChanger=0, Slot=0 WHERE "
               "Slot=%d AND StorageId=%s AND MediaId!=%s",
               mr->Slot,
               edit_int64(mr->StorageId, ed1), edit_int64(mr->MediaId, ed2));

       } else if (*mr->VolumeName) {
          mdb->db_escape_string(jcr, esc,mr->VolumeName,strlen(mr->VolumeName));
          Mmsg(mdb->cmd, "UPDATE Media SET InChanger=0, Slot=0 WHERE "
               "Slot=%d AND StorageId=%s AND VolumeName!='%s'",
               mr->Slot,
               edit_int64(mr->StorageId, ed1), esc);

       } else {  /* used by ua_label to reset all volume with this slot */
          Mmsg(mdb->cmd, "UPDATE Media SET InChanger=0, Slot=0 WHERE "
               "Slot=%d AND StorageId=%s",
               mr->Slot,
               edit_int64(mr->StorageId, ed1), mr->VolumeName);
       }
       Dmsg1(100, "%s\n", mdb->cmd);
       UPDATE_DB_NO_AFR(jcr, mdb, mdb->cmd);
   }
}

/*
 * Update Quota record
 *
 * Returns: false on failure
 *          true on success
 */
bool db_update_quota_gracetime(JCR *jcr, B_DB *mdb, JOB_DBR *jr)
{
   bool retval;
   char ed1[50], ed2[50];
   time_t now = time(NULL);

   db_lock(mdb);

   Mmsg(mdb->cmd,
 "UPDATE Quota SET GraceTime=%s WHERE ClientId='%s'",
      edit_uint64(now, ed1),
      edit_uint64(jr->ClientId, ed2));

   retval = UPDATE_DB(jcr, mdb, mdb->cmd);

   db_unlock(mdb);
   return retval;
}

/*
 * Update Quota Softlimit
 *
 * Returns: false on failure
 *          true on success
 */
bool db_update_quota_softlimit(JCR *jcr, B_DB *mdb, JOB_DBR *jr)
{
   bool retval;
   char ed1[50], ed2[50];

   db_lock(mdb);

   Mmsg(mdb->cmd,
 "UPDATE Quota SET QuotaLimit=%s WHERE ClientId='%s'",
      edit_uint64((jr->JobSumTotalBytes + jr->JobBytes), ed1),
      edit_uint64(jr->ClientId, ed2));

   retval = UPDATE_DB(jcr, mdb, mdb->cmd);

   db_unlock(mdb);
   return retval;
}

/*
 * Reset Quota Gracetime
 *
 * Returns: false on failure
 *          true on success
 */
bool db_reset_quota_record(JCR *jcr, B_DB *mdb, CLIENT_DBR *cr)
{
   bool retval;
   char ed1[50];

   db_lock(mdb);

   Mmsg(mdb->cmd,
 "UPDATE Quota SET GraceTime='0', QuotaLimit='0' WHERE ClientId='%s'",
      edit_uint64(cr->ClientId, ed1));

   retval = UPDATE_DB(jcr, mdb, mdb->cmd);

   db_unlock(mdb);
   return retval;
}

/**
 * Update NDMP level mapping
 *
 * Returns: false on failure
 *          true on success
 */
bool db_update_ndmp_level_mapping(JCR *jcr, B_DB *mdb, JOB_DBR *jr, char *filesystem, int level)
{
   bool retval;
   char ed1[50], ed2[50], ed3[50];

   db_lock(mdb);

   mdb->esc_name = check_pool_memory_size(mdb->esc_name, strlen(filesystem) * 2 + 1);
   db_escape_string(jcr, mdb, mdb->esc_name, filesystem, strlen(filesystem));

   Mmsg(mdb->cmd,
 "UPDATE NDMPLevelMap SET DumpLevel='%s' WHERE "
 "ClientId='%s' AND FileSetId='%s' AND FileSystem='%s'",
        edit_uint64(level, ed1), edit_uint64(jr->ClientId, ed2),
        edit_uint64(jr->FileSetId, ed3), mdb->esc_name);

   retval = UPDATE_DB(jcr, mdb, mdb->cmd);

   db_unlock(mdb);
   return retval;
}
#endif /* HAVE_SQLITE3 || HAVE_MYSQL || HAVE_POSTGRESQL || HAVE_INGRES || HAVE_DBI */
