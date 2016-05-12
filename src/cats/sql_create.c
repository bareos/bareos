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
 * BAREOS Catalog Database Create record interface routines
 *
 * Kern Sibbald, March 2000
 */

#include "bareos.h"

static const int dbglevel = 100;

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

/*
 * Forward referenced subroutines
 */
static bool db_create_file_record(JCR *jcr, B_DB *mdb, ATTR_DBR *ar);

/**
 * Create a new record for the Job
 * Returns: false on failure
 *          true  on success
 */
bool db_create_job_record(JCR *jcr, B_DB *mdb, JOB_DBR *jr)
{
   bool retval = false;;
   POOL_MEM buf;
   char dt[MAX_TIME_LENGTH];
   time_t stime;
   int len;
   utime_t JobTDate;
   char ed1[30], ed2[30];
   char esc_job[MAX_ESCAPE_NAME_LENGTH];
   char esc_name[MAX_ESCAPE_NAME_LENGTH];

   db_lock(mdb);

   stime = jr->SchedTime;
   ASSERT(stime != 0);

   bstrutime(dt, sizeof(dt), stime);
   JobTDate = (utime_t)stime;

   len = strlen(jcr->comment);  /* TODO: use jr instead of jcr to get comment */
   buf.check_size(len*2+1);
   mdb->db_escape_string(jcr, buf.c_str(), jcr->comment, len);

   mdb->db_escape_string(jcr, esc_job, jr->Job, strlen(jr->Job));
   mdb->db_escape_string(jcr, esc_name, jr->Name, strlen(jr->Name));

   /*
    * Must create it
    */
   Mmsg(mdb->cmd,
        "INSERT INTO Job (Job,Name,Type,Level,JobStatus,SchedTime,JobTDate,"
        "ClientId,Comment) "
        "VALUES ('%s','%s','%c','%c','%c','%s',%s,%s,'%s')",
        esc_job, esc_name, (char)(jr->JobType), (char)(jr->JobLevel),
        (char)(jr->JobStatus), dt, edit_uint64(JobTDate, ed1),
        edit_int64(jr->ClientId, ed2), buf.c_str());

   jr->JobId = sql_insert_autokey_record(mdb, mdb->cmd, NT_("Job"));
   if (jr->JobId == 0) {
      Mmsg2(mdb->errmsg, _("Create DB Job record %s failed. ERR=%s\n"), mdb->cmd, sql_strerror(mdb));
   } else {
      retval = true;
   }
   db_unlock(mdb);
   return retval;
}

/**
 * Create a JobMedia record for medium used this job
 * Returns: false on failure
 *          true  on success
 */
bool db_create_jobmedia_record(JCR *jcr, B_DB *mdb, JOBMEDIA_DBR *jm)
{
   bool retval = false;
   int count;
   char ed1[50], ed2[50];

   db_lock(mdb);

   /*
    * Now get count for VolIndex
    */
   Mmsg(mdb->cmd,
        "SELECT count(*) from JobMedia WHERE JobId=%s",
        edit_int64(jm->JobId, ed1));
   count = get_sql_record_max(jcr, mdb);
   if (count < 0) {
      count = 0;
   }
   count++;

   Mmsg(mdb->cmd,
        "INSERT INTO JobMedia (JobId,MediaId,FirstIndex,LastIndex,"
        "StartFile,EndFile,StartBlock,EndBlock,VolIndex) "
        "VALUES (%s,%s,%u,%u,%u,%u,%u,%u,%u)",
        edit_int64(jm->JobId, ed1),
        edit_int64(jm->MediaId, ed2),
        jm->FirstIndex, jm->LastIndex,
        jm->StartFile, jm->EndFile, jm->StartBlock, jm->EndBlock,count);

   Dmsg0(300, mdb->cmd);
   if (!INSERT_DB(jcr, mdb, mdb->cmd)) {
      Mmsg2(mdb->errmsg, _("Create JobMedia record %s failed: ERR=%s\n"), mdb->cmd, sql_strerror(mdb));
   } else {
      /*
       * Worked, now update the Media record with the EndFile and EndBlock
       */
      Mmsg(mdb->cmd,
           "UPDATE Media SET EndFile=%u, EndBlock=%u WHERE MediaId=%u",
           jm->EndFile, jm->EndBlock, jm->MediaId);
      if (!UPDATE_DB(jcr, mdb, mdb->cmd)) {
         Mmsg2(mdb->errmsg, _("Update Media record %s failed: ERR=%s\n"), mdb->cmd, sql_strerror(mdb));
      } else {
         retval = true;
      }
   }
   db_unlock(mdb);
   Dmsg0(300, "Return from JobMedia\n");
   return retval;
}

/**
 * Create Unique Pool record
 * Returns: false on failure
 *          true  on success
 */
bool db_create_pool_record(JCR *jcr, B_DB *mdb, POOL_DBR *pr)
{
   bool retval = false;
   char ed1[30], ed2[30], ed3[50], ed4[50], ed5[50];
   char esc_name[MAX_ESCAPE_NAME_LENGTH];
   char esc_lf[MAX_ESCAPE_NAME_LENGTH];
   int num_rows;

   Dmsg0(200, "In create pool\n");
   db_lock(mdb);
   mdb->db_escape_string(jcr, esc_name, pr->Name, strlen(pr->Name));
   mdb->db_escape_string(jcr, esc_lf, pr->LabelFormat, strlen(pr->LabelFormat));
   Mmsg(mdb->cmd, "SELECT PoolId,Name FROM Pool WHERE Name='%s'", esc_name);
   Dmsg1(200, "selectpool: %s\n", mdb->cmd);

   if (QUERY_DB(jcr, mdb, mdb->cmd)) {
      num_rows = sql_num_rows(mdb);
      if (num_rows > 0) {
         Mmsg1(mdb->errmsg, _("pool record %s already exists\n"), pr->Name);
         sql_free_result(mdb);
         goto bail_out;
      }
      sql_free_result(mdb);
   }

   /*
    * Must create it
    */
   Mmsg(mdb->cmd,
        "INSERT INTO Pool (Name,NumVols,MaxVols,UseOnce,UseCatalog,"
        "AcceptAnyVolume,AutoPrune,Recycle,VolRetention,VolUseDuration,"
        "MaxVolJobs,MaxVolFiles,MaxVolBytes,PoolType,LabelType,LabelFormat,"
        "RecyclePoolId,ScratchPoolId,ActionOnPurge,MinBlocksize,MaxBlocksize) "
        "VALUES ('%s',%u,%u,%d,%d,%d,%d,%d,%s,%s,%u,%u,%s,'%s',%d,'%s',%s,%s,%d,%d,%d)",
        esc_name,
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
   Dmsg1(200, "Create Pool: %s\n", mdb->cmd);
   pr->PoolId = sql_insert_autokey_record(mdb, mdb->cmd, NT_("Pool"));
   if (pr->PoolId == 0) {
      Mmsg2(mdb->errmsg, _("Create db Pool record %s failed: ERR=%s\n"), mdb->cmd, sql_strerror(mdb));
   } else {
      retval = true;
   }

bail_out:
   db_unlock(mdb);
   Dmsg0(500, "Create Pool: done\n");
   return retval;
}

/**
 * Create Unique Device record
 * Returns: false on failure
 *          true  on success
 */
bool db_create_device_record(JCR *jcr, B_DB *mdb, DEVICE_DBR *dr)
{
   bool retval = false;
   SQL_ROW row;
   char ed1[30], ed2[30];
   char esc[MAX_ESCAPE_NAME_LENGTH];
   int num_rows;

   Dmsg0(200, "In create Device\n");
   db_lock(mdb);
   mdb->db_escape_string(jcr, esc, dr->Name, strlen(dr->Name));
   Mmsg(mdb->cmd,
        "SELECT DeviceId,Name FROM Device WHERE Name='%s' AND StorageId = %s",
        esc, edit_int64(dr->StorageId, ed1));
   Dmsg1(200, "selectdevice: %s\n", mdb->cmd);

   if (QUERY_DB(jcr, mdb, mdb->cmd)) {
      num_rows = sql_num_rows(mdb);

      /*
       * If more than one, report error, but return first row
       */
      if (num_rows > 1) {
         Mmsg1(mdb->errmsg, _("More than one Device!: %d\n"), num_rows);
         Jmsg(jcr, M_ERROR, 0, "%s", mdb->errmsg);
      }
      if (num_rows >= 1) {
         if ((row = sql_fetch_row(mdb)) == NULL) {
            Mmsg1(mdb->errmsg, _("error fetching Device row: %s\n"), sql_strerror(mdb));
            Jmsg(jcr, M_ERROR, 0, "%s", mdb->errmsg);
            sql_free_result(mdb);
            goto bail_out;
         }
         dr->DeviceId = str_to_int64(row[0]);
         if (row[1]) {
            bstrncpy(dr->Name, row[1], sizeof(dr->Name));
         } else {
            dr->Name[0] = 0;         /* no name */
         }
         sql_free_result(mdb);
         retval = true;
         goto bail_out;
      }
      sql_free_result(mdb);
   }

   /*
    * Must create it
    */
   Mmsg(mdb->cmd,
        "INSERT INTO Device (Name,MediaTypeId,StorageId) VALUES ('%s',%s,%s)",
        esc,
        edit_uint64(dr->MediaTypeId, ed1),
        edit_int64(dr->StorageId, ed2));
   Dmsg1(200, "Create Device: %s\n", mdb->cmd);
   dr->DeviceId = sql_insert_autokey_record(mdb, mdb->cmd, NT_("Device"));
   if (dr->DeviceId == 0) {
      Mmsg2(mdb->errmsg, _("Create db Device record %s failed: ERR=%s\n"), mdb->cmd, sql_strerror(mdb));
   } else {
      retval = true;
   }

bail_out:
   db_unlock(mdb);
   return retval;
}

/**
 * Create a Unique record for Storage -- no duplicates
 * Returns: false on failure
 *          true  on success with id in sr->StorageId
 */
bool db_create_storage_record(JCR *jcr, B_DB *mdb, STORAGE_DBR *sr)
{
   SQL_ROW row;
   bool retval = false;
   int num_rows;
   char esc[MAX_ESCAPE_NAME_LENGTH];

   db_lock(mdb);
   mdb->db_escape_string(jcr, esc, sr->Name, strlen(sr->Name));
   Mmsg(mdb->cmd, "SELECT StorageId,AutoChanger FROM Storage WHERE Name='%s'", esc);

   sr->StorageId = 0;
   sr->created = false;
   /*
    * Check if it already exists
    */
   if (QUERY_DB(jcr, mdb, mdb->cmd)) {
      num_rows = sql_num_rows(mdb);
      /*
       * If more than one, report error, but return first row
       */
      if (num_rows > 1) {
         Mmsg1(mdb->errmsg, _("More than one Storage record!: %d\n"), num_rows);
         Jmsg(jcr, M_ERROR, 0, "%s", mdb->errmsg);
      }
      if (num_rows >= 1) {
         if ((row = sql_fetch_row(mdb)) == NULL) {
            Mmsg1(mdb->errmsg, _("error fetching Storage row: %s\n"), sql_strerror(mdb));
            Jmsg(jcr, M_ERROR, 0, "%s", mdb->errmsg);
            sql_free_result(mdb);
            goto bail_out;
         }
         sr->StorageId = str_to_int64(row[0]);
         sr->AutoChanger = atoi(row[1]);   /* bool */
         sql_free_result(mdb);
         retval = true;
         goto bail_out;
      }
      sql_free_result(mdb);
   }

   /*
    * Must create it
    */
   Mmsg(mdb->cmd,
        "INSERT INTO Storage (Name,AutoChanger)"
        " VALUES ('%s',%d)",
        esc, sr->AutoChanger);

   sr->StorageId = sql_insert_autokey_record(mdb, mdb->cmd, NT_("Storage"));
   if (sr->StorageId == 0) {
      Mmsg2(mdb->errmsg, _("Create DB Storage record %s failed. ERR=%s\n"), mdb->cmd, sql_strerror(mdb));
      Jmsg(jcr, M_ERROR, 0, "%s", mdb->errmsg);
   } else {
      sr->created = true;
      retval = true;
   }

bail_out:
   db_unlock(mdb);
   return retval;
}

/**
 * Create Unique MediaType record
 * Returns: false on failure
 *          true  on success
 */
bool db_create_mediatype_record(JCR *jcr, B_DB *mdb, MEDIATYPE_DBR *mr)
{
   bool retval = false;
   int num_rows;
   char esc[MAX_ESCAPE_NAME_LENGTH];

   Dmsg0(200, "In create mediatype\n");
   db_lock(mdb);
   mdb->db_escape_string(jcr, esc, mr->MediaType, strlen(mr->MediaType));
   Mmsg(mdb->cmd, "SELECT MediaTypeId,MediaType FROM MediaType WHERE MediaType='%s'", esc);
   Dmsg1(200, "selectmediatype: %s\n", mdb->cmd);

   if (QUERY_DB(jcr, mdb, mdb->cmd)) {
      num_rows = sql_num_rows(mdb);
      if (num_rows > 0) {
         Mmsg1(mdb->errmsg, _("mediatype record %s already exists\n"), mr->MediaType);
         sql_free_result(mdb);
         goto bail_out;
      }
      sql_free_result(mdb);
   }

   /*
    * Must create it
    */
   Mmsg(mdb->cmd,
        "INSERT INTO MediaType (MediaType,ReadOnly) "
        "VALUES ('%s',%d)",
        mr->MediaType,
        mr->ReadOnly);
   Dmsg1(200, "Create mediatype: %s\n", mdb->cmd);
   mr->MediaTypeId = sql_insert_autokey_record(mdb, mdb->cmd, NT_("MediaType"));
   if (mr->MediaTypeId == 0) {
      Mmsg2(mdb->errmsg, _("Create db mediatype record %s failed: ERR=%s\n"), mdb->cmd, sql_strerror(mdb));
      goto bail_out;
   } else {
      retval = true;
   }

bail_out:
   db_unlock(mdb);
   return retval;
}

/**
 * Create Media record. VolumeName and non-zero Slot must be unique
 * Returns: false on failure
 *          true on success with id in mr->MediaId
 */
bool db_create_media_record(JCR *jcr, B_DB *mdb, MEDIA_DBR *mr)
{
   bool retval = false;
   char ed1[50], ed2[50], ed3[50], ed4[50], ed5[50], ed6[50], ed7[50], ed8[50];
   char ed9[50], ed10[50], ed11[50], ed12[50];
   int num_rows;
   char esc_name[MAX_ESCAPE_NAME_LENGTH];
   char esc_mtype[MAX_ESCAPE_NAME_LENGTH];
   char esc_status[MAX_ESCAPE_NAME_LENGTH];

   db_lock(mdb);
   mdb->db_escape_string(jcr, esc_name, mr->VolumeName, strlen(mr->VolumeName));
   mdb->db_escape_string(jcr, esc_mtype, mr->MediaType, strlen(mr->MediaType));
   mdb->db_escape_string(jcr, esc_status, mr->VolStatus, strlen(mr->VolStatus));

   Mmsg(mdb->cmd, "SELECT MediaId FROM Media WHERE VolumeName='%s'", esc_name);
   Dmsg1(500, "selectpool: %s\n", mdb->cmd);

   if (QUERY_DB(jcr, mdb, mdb->cmd)) {
      num_rows = sql_num_rows(mdb);
      if (num_rows > 0) {
         Mmsg1(mdb->errmsg, _("Volume \"%s\" already exists.\n"), mr->VolumeName);
         sql_free_result(mdb);
         goto bail_out;
      }
      sql_free_result(mdb);
   }

   /*
    * Must create it
    */
   Mmsg(mdb->cmd,
        "INSERT INTO Media (VolumeName,MediaType,MediaTypeId,PoolId,MaxVolBytes,"
        "VolCapacityBytes,Recycle,VolRetention,VolUseDuration,MaxVolJobs,MaxVolFiles,"
        "VolStatus,Slot,VolBytes,InChanger,VolReadTime,VolWriteTime,"
        "EndFile,EndBlock,LabelType,StorageId,DeviceId,LocationId,"
        "ScratchPoolId,RecyclePoolId,Enabled,ActionOnPurge,EncryptionKey,"
        "MinBlocksize,MaxBlocksize) "
        "VALUES ('%s','%s',0,%u,%s,%s,%d,%s,%s,%u,%u,'%s',%d,%s,%d,%s,%s,0,0,%d,%s,"
        "%s,%s,%s,%s,%d,%d,'%s',%d,%d)",
        esc_name,
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
        mr->MaxBlocksize);

   Dmsg1(500, "Create Volume: %s\n", mdb->cmd);
   mr->MediaId = sql_insert_autokey_record(mdb, mdb->cmd, NT_("Media"));
   if (mr->MediaId == 0) {
      Mmsg2(mdb->errmsg, _("Create DB Media record %s failed. ERR=%s\n"), mdb->cmd, sql_strerror(mdb));
   } else {
      retval = true;
      if (mr->set_label_date) {
         char dt[MAX_TIME_LENGTH];
         if (mr->LabelDate == 0) {
            mr->LabelDate = time(NULL);
         }

         bstrutime(dt, sizeof(dt), mr->LabelDate);
         Mmsg(mdb->cmd, "UPDATE Media SET LabelDate='%s' "
              "WHERE MediaId=%d", dt, mr->MediaId);
         retval = UPDATE_DB(jcr, mdb, mdb->cmd);
      }
      /*
       * Make sure that if InChanger is non-zero any other identical slot
       * has InChanger zero.
       */
      db_make_inchanger_unique(jcr, mdb, mr);
   }

bail_out:
   db_unlock(mdb);
   return retval;
}

/**
 * Create a Unique record for the client -- no duplicates
 * Returns: false on failure
 *          true on success with id in cr->ClientId
 */
bool db_create_client_record(JCR *jcr, B_DB *mdb, CLIENT_DBR *cr)
{
   bool retval = false;
   SQL_ROW row;
   char ed1[50], ed2[50];
   int num_rows;
   char esc_name[MAX_ESCAPE_NAME_LENGTH];
   char esc_uname[MAX_ESCAPE_NAME_LENGTH];

   db_lock(mdb);
   mdb->db_escape_string(jcr, esc_name, cr->Name, strlen(cr->Name));
   mdb->db_escape_string(jcr, esc_uname, cr->Uname, strlen(cr->Uname));
   Mmsg(mdb->cmd, "SELECT ClientId,Uname FROM Client WHERE Name='%s'", esc_name);

   cr->ClientId = 0;
   if (QUERY_DB(jcr, mdb, mdb->cmd)) {
      num_rows = sql_num_rows(mdb);
      /*
       * If more than one, report error, but return first row
       */
      if (num_rows > 1) {
         Mmsg1(mdb->errmsg, _("More than one Client!: %d\n"), num_rows);
         Jmsg(jcr, M_ERROR, 0, "%s", mdb->errmsg);
      }
      if (num_rows >= 1) {
         if ((row = sql_fetch_row(mdb)) == NULL) {
            Mmsg1(mdb->errmsg, _("error fetching Client row: %s\n"), sql_strerror(mdb));
            Jmsg(jcr, M_ERROR, 0, "%s", mdb->errmsg);
            sql_free_result(mdb);
            goto bail_out;
         }
         cr->ClientId = str_to_int64(row[0]);
         if (row[1]) {
            bstrncpy(cr->Uname, row[1], sizeof(cr->Uname));
         } else {
            cr->Uname[0] = 0;         /* no name */
         }
         sql_free_result(mdb);
         retval = true;
         goto bail_out;
      }
      sql_free_result(mdb);
   }

   /*
    * Must create it
    */
   Mmsg(mdb->cmd,
        "INSERT INTO Client (Name,Uname,AutoPrune,"
        "FileRetention,JobRetention) VALUES "
        "('%s','%s',%d,%s,%s)",
        esc_name, esc_uname, cr->AutoPrune,
        edit_uint64(cr->FileRetention, ed1),
        edit_uint64(cr->JobRetention, ed2));

   cr->ClientId = sql_insert_autokey_record(mdb, mdb->cmd, NT_("Client"));
   if (cr->ClientId == 0) {
      Mmsg2(mdb->errmsg, _("Create DB Client record %s failed. ERR=%s\n"), mdb->cmd, sql_strerror(mdb));
      Jmsg(jcr, M_ERROR, 0, "%s", mdb->errmsg);
   } else {
      retval = true;
   }

bail_out:
   db_unlock(mdb);
   return retval;
}

/**
 * Create a Unique record for the Path -- no duplicates
 * Returns: false on failure
 *          true on success with id in cr->ClientId
 */
bool db_create_path_record(JCR *jcr, B_DB *mdb, ATTR_DBR *ar)
{
   bool retval = false;
   SQL_ROW row;
   int num_rows;

   mdb->errmsg[0] = 0;
   mdb->esc_name = check_pool_memory_size(mdb->esc_name, 2*mdb->pnl+2);
   db_escape_string(jcr, mdb, mdb->esc_name, mdb->path, mdb->pnl);

   if (mdb->cached_path_id != 0 &&
       mdb->cached_path_len == mdb->pnl &&
       bstrcmp(mdb->cached_path, mdb->path)) {
      ar->PathId = mdb->cached_path_id;
      return true;
   }

   Mmsg(mdb->cmd, "SELECT PathId FROM Path WHERE Path='%s'", mdb->esc_name);

   if (QUERY_DB(jcr, mdb, mdb->cmd)) {
      num_rows = sql_num_rows(mdb);
      if (num_rows > 1) {
         char ed1[30];
         Mmsg2(mdb->errmsg, _("More than one Path!: %s for path: %s\n"), edit_uint64(num_rows, ed1), mdb->path);
         Jmsg(jcr, M_WARNING, 0, "%s", mdb->errmsg);
      }
      /*
       * Even if there are multiple paths, take the first one
       */
      if (num_rows >= 1) {
         if ((row = sql_fetch_row(mdb)) == NULL) {
            Mmsg1(mdb->errmsg, _("error fetching row: %s\n"), sql_strerror(mdb));
            Jmsg(jcr, M_ERROR, 0, "%s", mdb->errmsg);
            sql_free_result(mdb);
            ar->PathId = 0;
            ASSERT(ar->PathId);
            goto bail_out;
         }
         ar->PathId = str_to_int64(row[0]);
         sql_free_result(mdb);
         /*
          * Cache path
          */
         if (ar->PathId != mdb->cached_path_id) {
            mdb->cached_path_id = ar->PathId;
            mdb->cached_path_len = mdb->pnl;
            pm_strcpy(mdb->cached_path, mdb->path);
         }
         ASSERT(ar->PathId);
         retval = true;
         goto bail_out;
      }
      sql_free_result(mdb);
   }

   Mmsg(mdb->cmd, "INSERT INTO Path (Path) VALUES ('%s')", mdb->esc_name);

   ar->PathId = sql_insert_autokey_record(mdb, mdb->cmd, NT_("Path"));
   if (ar->PathId == 0) {
      Mmsg2(mdb->errmsg, _("Create db Path record %s failed. ERR=%s\n"), mdb->cmd, sql_strerror(mdb));
      Jmsg(jcr, M_FATAL, 0, "%s", mdb->errmsg);
      ar->PathId = 0;
      goto bail_out;
   }

   /*
    * Cache path
    */
   if (ar->PathId != mdb->cached_path_id) {
      mdb->cached_path_id = ar->PathId;
      mdb->cached_path_len = mdb->pnl;
      pm_strcpy(mdb->cached_path, mdb->path);
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
bool db_create_counter_record(JCR *jcr, B_DB *mdb, COUNTER_DBR *cr)
{
   bool retval = false;
   char esc[MAX_ESCAPE_NAME_LENGTH];
   COUNTER_DBR mcr;

   db_lock(mdb);
   memset(&mcr, 0, sizeof(mcr));
   bstrncpy(mcr.Counter, cr->Counter, sizeof(mcr.Counter));
   if (db_get_counter_record(jcr, mdb, &mcr)) {
      memcpy(cr, &mcr, sizeof(COUNTER_DBR));
      retval = true;
      goto bail_out;
   }
   mdb->db_escape_string(jcr, esc, cr->Counter, strlen(cr->Counter));

   /*
    * Must create it
    */
   Mmsg(mdb->cmd, insert_counter_values[db_get_type_index(mdb)],
        esc, cr->MinValue, cr->MaxValue, cr->CurrentValue,
        cr->WrapCounter);

   if (!INSERT_DB(jcr, mdb, mdb->cmd)) {
      Mmsg2(mdb->errmsg, _("Create DB Counters record %s failed. ERR=%s\n"), mdb->cmd, sql_strerror(mdb));
      Jmsg(jcr, M_ERROR, 0, "%s", mdb->errmsg);
   } else {
      retval = true;
   }

bail_out:
   db_unlock(mdb);
   return retval;
}

/**
 * Create a FileSet record. This record is unique in the
 * name and the MD5 signature of the include/exclude sets.
 * Returns: false on failure
 *          true on success with FileSetId in record
 */
bool db_create_fileset_record(JCR *jcr, B_DB *mdb, FILESET_DBR *fsr)
{
   bool retval = false;
   SQL_ROW row;
   int num_rows, len;
   char esc_fs[MAX_ESCAPE_NAME_LENGTH];
   char esc_md5[MAX_ESCAPE_NAME_LENGTH];

   db_lock(mdb);
   fsr->created = false;
   mdb->db_escape_string(jcr, esc_fs, fsr->FileSet, strlen(fsr->FileSet));
   mdb->db_escape_string(jcr, esc_md5, fsr->MD5, strlen(fsr->MD5));
   Mmsg(mdb->cmd,
        "SELECT FileSetId,CreateTime FROM FileSet WHERE "
        "FileSet='%s' AND MD5='%s'",
        esc_fs, esc_md5);

   fsr->FileSetId = 0;
   if (QUERY_DB(jcr, mdb, mdb->cmd)) {
      num_rows = sql_num_rows(mdb);
      if (num_rows > 1) {
         Mmsg1(mdb->errmsg, _("More than one FileSet!: %d\n"), num_rows);
         Jmsg(jcr, M_ERROR, 0, "%s", mdb->errmsg);
      }
      if (num_rows >= 1) {
         if ((row = sql_fetch_row(mdb)) == NULL) {
            Mmsg1(mdb->errmsg, _("error fetching FileSet row: ERR=%s\n"), sql_strerror(mdb));
            Jmsg(jcr, M_ERROR, 0, "%s", mdb->errmsg);
            sql_free_result(mdb);
            goto bail_out;
         }
         fsr->FileSetId = str_to_int64(row[0]);
         if (row[1] == NULL) {
            fsr->cCreateTime[0] = 0;
         } else {
            bstrncpy(fsr->cCreateTime, row[1], sizeof(fsr->cCreateTime));
         }
         sql_free_result(mdb);
         retval = true;
         goto bail_out;
      }
      sql_free_result(mdb);
   }

   /*
    * Must create it
    */
   if (fsr->CreateTime == 0 && fsr->cCreateTime[0] == 0) {
      fsr->CreateTime = time(NULL);
   }

   bstrutime(fsr->cCreateTime, sizeof(fsr->cCreateTime), fsr->CreateTime);
   if (fsr->FileSetText) {
      POOL_MEM esc_filesettext(PM_MESSAGE);

      len = strlen(fsr->FileSetText);
      esc_filesettext.check_size(len * 2 + 1);
      mdb->db_escape_string(jcr, esc_filesettext.c_str(), fsr->FileSetText, len);

      Mmsg(mdb->cmd,
           "INSERT INTO FileSet (FileSet,MD5,CreateTime,FileSetText) "
           "VALUES ('%s','%s','%s','%s')",
           esc_fs, esc_md5, fsr->cCreateTime, esc_filesettext.c_str());
   } else {
      Mmsg(mdb->cmd,
           "INSERT INTO FileSet (FileSet,MD5,CreateTime) "
           "VALUES ('%s','%s','%s')",
           esc_fs, esc_md5, fsr->cCreateTime);
   }

   fsr->FileSetId = sql_insert_autokey_record(mdb, mdb->cmd, NT_("FileSet"));
   if (fsr->FileSetId == 0) {
      Mmsg2(mdb->errmsg, _("Create DB FileSet record %s failed. ERR=%s\n"), mdb->cmd, sql_strerror(mdb));
      Jmsg(jcr, M_ERROR, 0, "%s", mdb->errmsg);
      goto bail_out;
   } else {
      fsr->created = true;
      retval = true;
   }

bail_out:
   db_unlock(mdb);
   return retval;
}

/**
 * All sql_batch_* functions are used to do bulk batch insert in File/Filename/Path
 * tables.
 *
 * To sum up :
 *  - bulk load a temp table
 *  - insert missing filenames into filename with a single query (lock filenames
 *  - table before that to avoid possible duplicate inserts with concurrent update)
 *  - insert missing paths into path with another single query
 *  - then insert the join between the temp, filename and path tables into file.
 *
 * Returns: false on failure
 *          true on success
 */
bool db_write_batch_file_records(JCR *jcr)
{
   bool retval = false;
   int JobStatus = jcr->JobStatus;

   if (!jcr->batch_started) {         /* no files to backup ? */
      Dmsg0(50,"db_create_file_record : no files\n");
      return true;
   }
   if (job_canceled(jcr)) {
      goto bail_out;
   }

   Dmsg1(50,"db_create_file_record changes=%u\n",jcr->db_batch->changes);

   jcr->JobStatus = JS_AttrInserting;
   if (!sql_batch_end(jcr, jcr->db_batch, NULL)) {
      Jmsg1(jcr, M_FATAL, 0, "Batch end %s\n", jcr->db_batch->errmsg);
      goto bail_out;
   }
   if (job_canceled(jcr)) {
      goto bail_out;
   }

   /*
    * We have to lock tables
    */
   if (!db_sql_query(jcr->db_batch, batch_lock_path_query[db_get_type_index(jcr->db_batch)])) {
      Jmsg1(jcr, M_FATAL, 0, "Lock Path table %s\n", jcr->db_batch->errmsg);
      goto bail_out;
   }

   if (!db_sql_query(jcr->db_batch, batch_fill_path_query[db_get_type_index(jcr->db_batch)])) {
      Jmsg1(jcr, M_FATAL, 0, "Fill Path table %s\n",jcr->db_batch->errmsg);
      db_sql_query(jcr->db_batch, batch_unlock_tables_query[db_get_type_index(jcr->db_batch)]);
      goto bail_out;
   }

   if (!db_sql_query(jcr->db_batch, batch_unlock_tables_query[db_get_type_index(jcr->db_batch)])) {
      Jmsg1(jcr, M_FATAL, 0, "Unlock Path table %s\n", jcr->db_batch->errmsg);
      goto bail_out;
   }

   /*
    * We have to lock tables
    */
   /*
   if (!db_sql_query(jcr->db_batch, batch_lock_filename_query[db_get_type_index(jcr->db_batch)])) {
      Jmsg1(jcr, M_FATAL, 0, "Lock Filename table %s\n", jcr->db_batch->errmsg);
      goto bail_out;
   }

   if (!db_sql_query(jcr->db_batch, batch_fill_filename_query[db_get_type_index(jcr->db_batch)])) {
      Jmsg1(jcr,M_FATAL,0,"Fill Filename table %s\n",jcr->db_batch->errmsg);
      db_sql_query(jcr->db_batch, batch_unlock_tables_query[db_get_type_index(jcr->db_batch)]);
      goto bail_out;
   }

   if (!db_sql_query(jcr->db_batch, batch_unlock_tables_query[db_get_type_index(jcr->db_batch)])) {
      Jmsg1(jcr, M_FATAL, 0, "Unlock Filename table %s\n", jcr->db_batch->errmsg);
      goto bail_out;
   }
   */

   if (!db_sql_query(jcr->db_batch,
                     "INSERT INTO File (FileIndex, JobId, PathId, Name, LStat, MD5, DeltaSeq) "
                     "SELECT batch.FileIndex, batch.JobId, Path.PathId, "
                     "batch.Name,batch.LStat, batch.MD5, batch.DeltaSeq "
                     "FROM batch "
                     "JOIN Path ON (batch.Path = Path.Path) ")) {
      Jmsg1(jcr, M_FATAL, 0, "Fill File table %s\n", jcr->db_batch->errmsg);
      goto bail_out;
   }

   jcr->JobStatus = JobStatus;         /* reset entry status */
   retval = true;

bail_out:
   db_sql_query(jcr->db_batch, "DROP TABLE batch");
   jcr->batch_started = false;

   return retval;
}

/**
 * Create File record in B_DB
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
bool db_create_batch_file_attributes_record(JCR *jcr, B_DB *mdb, ATTR_DBR *ar)
{
   ASSERT(ar->FileType != FT_BASE);

   Dmsg1(dbglevel, "Fname=%s\n", ar->fname);
   Dmsg0(dbglevel, "put_file_into_catalog\n");

   if (jcr->batch_started && jcr->db_batch->changes > BATCH_FLUSH) {
      db_write_batch_file_records(jcr);
      jcr->db_batch->changes = 0;
   }

   /*
    * Open the dedicated connection
    */
   if (!jcr->batch_started) {
      if (!db_open_batch_connection(jcr, mdb)) {
         return false;     /* error already printed */
      }
      if (!sql_batch_start(jcr, jcr->db_batch)) {
         Mmsg1(mdb->errmsg, "Can't start batch mode: ERR=%s", db_strerror(jcr->db_batch));
         Jmsg(jcr, M_FATAL, 0, "%s", mdb->errmsg);
         return false;
      }
      jcr->batch_started = true;
   }

   split_path_and_file(jcr, jcr->db_batch, ar->fname);

   return sql_batch_insert(jcr, jcr->db_batch, ar);
}

/**
 * Create File record in B_DB
 *
 * In order to reduce database size, we store the File attributes,
 * the FileName, and the Path separately.  In principle, there
 * is a single FileName record and a single Path record, no matter
 * how many times it occurs.  This is this subroutine, we separate
 * the file and the path and create three database records.
 * Returns: false on failure
 *          true on success
 */
bool db_create_file_attributes_record(JCR *jcr, B_DB *mdb, ATTR_DBR *ar)
{
   bool retval = false;

   db_lock(mdb);
   Dmsg1(dbglevel, "Fname=%s\n", ar->fname);
   Dmsg0(dbglevel, "put_file_into_catalog\n");

   split_path_and_file(jcr, mdb, ar->fname);

   if (!db_create_path_record(jcr, mdb, ar)) {
      goto bail_out;
   }
   Dmsg1(dbglevel, "db_create_path_record: %s\n", mdb->esc_name);

   /* Now create master File record */
   if (!db_create_file_record(jcr, mdb, ar)) {
      goto bail_out;
   }
   Dmsg0(dbglevel, "db_create_file_record OK\n");

   Dmsg2(dbglevel, "CreateAttributes Path=%s File=%s\n", mdb->path, mdb->fname);
   retval = true;

bail_out:
   db_unlock(mdb);
   return retval;
}

/**
 * This is the master File entry containing the attributes.
 * The filename and path records have already been created.
 * Returns: false on failure
 *          true on success with fileid filled in
 */
static bool db_create_file_record(JCR *jcr, B_DB *mdb, ATTR_DBR *ar)
{
   bool retval = false;
   static const char *no_digest = "0";
   const char *digest;

   ASSERT(ar->JobId);
   ASSERT(ar->PathId);

   mdb->esc_name = check_pool_memory_size(mdb->esc_name, 2*mdb->fnl+2);
   db_escape_string(jcr, mdb, mdb->esc_name, mdb->fname, mdb->fnl);

   if (ar->Digest == NULL || ar->Digest[0] == 0) {
      digest = no_digest;
   } else {
      digest = ar->Digest;
   }

   /* Must create it */
   Mmsg(mdb->cmd,
        "INSERT INTO File (FileIndex,JobId,PathId,Name,"
        "LStat,MD5,DeltaSeq) VALUES (%u,%u,%u,'%s','%s','%s',%u)",
        ar->FileIndex, ar->JobId, ar->PathId, mdb->esc_name,
        ar->attr, digest, ar->DeltaSeq);

   ar->FileId = sql_insert_autokey_record(mdb, mdb->cmd, NT_("File"));
   if (ar->FileId == 0) {
      Mmsg2(mdb->errmsg, _("Create db File record %s failed. ERR=%s"), mdb->cmd, sql_strerror(mdb));
      Jmsg(jcr, M_FATAL, 0, "%s", mdb->errmsg);
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
bool db_create_attributes_record(JCR *jcr, B_DB *mdb, ATTR_DBR *ar)
{
   bool retval;

   mdb->errmsg[0] = 0;
   /*
    * Make sure we have an acceptable attributes record.
    */
   if (!(ar->Stream == STREAM_UNIX_ATTRIBUTES ||
         ar->Stream == STREAM_UNIX_ATTRIBUTES_EX)) {
      Mmsg1(mdb->errmsg, _("Attempt to put non-attributes into catalog. Stream=%d\n"), ar->Stream);
      Jmsg(jcr, M_FATAL, 0, "%s", mdb->errmsg);
      return false;
   }

   if (ar->FileType != FT_BASE) {
      if (mdb->batch_insert_available()) {
         retval = db_create_batch_file_attributes_record(jcr, mdb, ar);
         /* Error message already printed */
      } else {
         retval = db_create_file_attributes_record(jcr, mdb, ar);
      }
   } else if (jcr->HasBase) {
      retval = db_create_base_file_attributes_record(jcr, mdb, ar);
   } else {
      Mmsg0(mdb->errmsg, _("Cannot Copy/Migrate job using BaseJob.\n"));
      Jmsg(jcr, M_FATAL, 0, "%s", mdb->errmsg);
      retval = true;               /* in copy/migration what do we do ? */
   }

   return retval;
}

/**
 * Create Base File record in B_DB
 * Returns: false on failure
 *          true on success
 */
bool db_create_base_file_attributes_record(JCR *jcr, B_DB *mdb, ATTR_DBR *ar)
{
   bool retval;
   Dmsg1(dbglevel, "create_base_file Fname=%s\n", ar->fname);
   Dmsg0(dbglevel, "put_base_file_into_catalog\n");

   db_lock(mdb);
   split_path_and_file(jcr, mdb, ar->fname);

   mdb->esc_name = check_pool_memory_size(mdb->esc_name, mdb->fnl*2+1);
   db_escape_string(jcr, mdb, mdb->esc_name, mdb->fname, mdb->fnl);

   mdb->esc_path = check_pool_memory_size(mdb->esc_path, mdb->pnl*2+1);
   db_escape_string(jcr, mdb, mdb->esc_path, mdb->path, mdb->pnl);

   Mmsg(mdb->cmd,
        "INSERT INTO basefile%lld (Path, Name) VALUES ('%s','%s')",
        (uint64_t)jcr->JobId, mdb->esc_path, mdb->esc_name);

   retval = INSERT_DB(jcr, mdb, mdb->cmd);
   db_unlock(mdb);

   return retval;
}

/**
 * Cleanup the base file temporary tables
 */
static void db_cleanup_base_file(JCR *jcr, B_DB *mdb)
{
   POOL_MEM buf(PM_MESSAGE);
   Mmsg(buf, "DROP TABLE new_basefile%lld", (uint64_t) jcr->JobId);
   db_sql_query(mdb, buf.c_str());

   Mmsg(buf, "DROP TABLE basefile%lld", (uint64_t) jcr->JobId);
   db_sql_query(mdb, buf.c_str());
}

/**
 * Put all base file seen in the backup to the BaseFile table
 * and cleanup temporary tables
 * Returns: false on failure
 *          true on success
 */
bool db_commit_base_file_attributes_record(JCR *jcr, B_DB *mdb)
{
   bool retval;
   char ed1[50];

   db_lock(mdb);

   Mmsg(mdb->cmd,
        "INSERT INTO BaseFiles (BaseJobId, JobId, FileId, FileIndex) "
        "SELECT B.JobId AS BaseJobId, %s AS JobId, "
        "B.FileId, B.FileIndex "
        "FROM basefile%s AS A, new_basefile%s AS B "
        "WHERE A.Path = B.Path "
        "AND A.Name = B.Name "
        "ORDER BY B.FileId",
        edit_uint64(jcr->JobId, ed1), ed1, ed1);
   retval = db_sql_query(mdb, mdb->cmd);
   jcr->nb_base_files_used = sql_affected_rows(mdb);
   db_cleanup_base_file(jcr, mdb);

   db_unlock(mdb);
   return retval;
}

/**
 * Find the last "accurate" backup state with Base jobs
 * 1) Get all files with jobid in list (F subquery)
 * 2) Take only the last version of each file (Temp subquery) => accurate list is ok
 * 3) Put the result in a temporary table for the end of job
 * Returns: false on failure
 *          true on success
 */
bool db_create_base_file_list(JCR *jcr, B_DB *mdb, char *jobids)
{
   bool retval = false;
   POOL_MEM buf;

   db_lock(mdb);

   if (!*jobids) {
      Mmsg(mdb->errmsg, _("ERR=JobIds are empty\n"));
      goto bail_out;
   }

   Mmsg(mdb->cmd, create_temp_basefile[db_get_type_index(mdb)], (uint64_t) jcr->JobId);
   if (!db_sql_query(mdb, mdb->cmd)) {
      goto bail_out;
   }
   Mmsg(buf, select_recent_version[db_get_type_index(mdb)], jobids, jobids);
   Mmsg(mdb->cmd, create_temp_new_basefile[db_get_type_index(mdb)], (uint64_t)jcr->JobId, buf.c_str());

   retval = db_sql_query(mdb, mdb->cmd);

bail_out:
   db_unlock(mdb);
   return retval;
}

/**
 * Create Restore Object record in B_DB
 * Returns: false on failure
 *          true on success
 */
bool db_create_restore_object_record(JCR *jcr, B_DB *mdb, ROBJECT_DBR *ro)
{
   bool retval = false;
   int plug_name_len;
   POOLMEM *esc_plug_name = get_pool_memory(PM_MESSAGE);

   db_lock(mdb);

   Dmsg1(dbglevel, "Oname=%s\n", ro->object_name);
   Dmsg0(dbglevel, "put_object_into_catalog\n");

   mdb->fnl = strlen(ro->object_name);
   mdb->esc_name = check_pool_memory_size(mdb->esc_name, mdb->fnl*2+1);
   db_escape_string(jcr, mdb, mdb->esc_name, ro->object_name, mdb->fnl);

   db_escape_object(jcr, mdb, ro->object, ro->object_len);

   plug_name_len = strlen(ro->plugin_name);
   esc_plug_name = check_pool_memory_size(esc_plug_name, plug_name_len*2+1);
   db_escape_string(jcr, mdb, esc_plug_name, ro->plugin_name, plug_name_len);

   Mmsg(mdb->cmd,
        "INSERT INTO RestoreObject (ObjectName,PluginName,RestoreObject,"
        "ObjectLength,ObjectFullLength,ObjectIndex,ObjectType,"
        "ObjectCompression,FileIndex,JobId) "
        "VALUES ('%s','%s','%s',%d,%d,%d,%d,%d,%d,%u)",
        mdb->esc_name, esc_plug_name, mdb->esc_obj,
        ro->object_len, ro->object_full_len, ro->object_index,
        ro->FileType, ro->object_compression, ro->FileIndex, ro->JobId);

   ro->RestoreObjectId = sql_insert_autokey_record(mdb, mdb->cmd, NT_("RestoreObject"));
   if (ro->RestoreObjectId == 0) {
      Mmsg2(mdb->errmsg, _("Create db Object record %s failed. ERR=%s"), mdb->cmd, sql_strerror(mdb));
      Jmsg(jcr, M_FATAL, 0, "%s", mdb->errmsg);
   } else {
      retval = true;
   }
   db_unlock(mdb);
   free_pool_memory(esc_plug_name);
   return retval;
}

/**
 * Create a quota record if it does not exist.
 * Returns: false on failure
 *          true on success
 */
bool db_create_quota_record(JCR *jcr, B_DB *mdb, CLIENT_DBR *cr)
{
   bool retval = false;
   char ed1[50];
   int num_rows;

   db_lock(mdb);
   Mmsg(mdb->cmd,
        "SELECT ClientId FROM Quota WHERE ClientId='%s'",
        edit_uint64(cr->ClientId,ed1));

   if (QUERY_DB(jcr, mdb, mdb->cmd)) {
      num_rows = sql_num_rows(mdb);
      if (num_rows == 1) {
         sql_free_result(mdb);
         retval = true;
         goto bail_out;
      }
      sql_free_result(mdb);
   }

   /*
    * Must create it
    */
   Mmsg(mdb->cmd,
        "INSERT INTO Quota (ClientId, GraceTime, QuotaLimit)"
        " VALUES ('%s', '%s', %s)",
        edit_uint64(cr->ClientId, ed1), "0", "0");

   if (!INSERT_DB(jcr, mdb, mdb->cmd)) {
      Mmsg2(mdb->errmsg, _("Create DB Quota record %s failed. ERR=%s\n"), mdb->cmd, sql_strerror(mdb));
      Jmsg(jcr, M_ERROR, 0, "%s", mdb->errmsg);
   } else {
      retval = true;
   }

bail_out:
   db_unlock(mdb);
   return retval;
}

/**
 * Create a NDMP level mapping if it does not exist.
 * Returns: false on failure
 *          true on success
 */
bool db_create_ndmp_level_mapping(JCR *jcr, B_DB *mdb, JOB_DBR *jr, char *filesystem)
{
   bool retval = false;
   char ed1[50], ed2[50];
   int num_rows;

   db_lock(mdb);

   mdb->esc_name = check_pool_memory_size(mdb->esc_name, strlen(filesystem) * 2 + 1);
   db_escape_string(jcr, mdb, mdb->esc_name, filesystem, strlen(filesystem));

   Mmsg(mdb->cmd,
         "SELECT ClientId FROM NDMPLevelMap WHERE "
        "ClientId='%s' AND FileSetId='%s' AND FileSystem='%s'",
        edit_uint64(jr->ClientId, ed1), edit_uint64(jr->FileSetId, ed2), mdb->esc_name);

   if (QUERY_DB(jcr, mdb, mdb->cmd)) {
      num_rows = sql_num_rows(mdb);
      if (num_rows == 1) {
         sql_free_result(mdb);
         retval = true;
         goto bail_out;
      }
      sql_free_result(mdb);
   }

   /*
    * Must create it
    */
   Mmsg(mdb->cmd,
        "INSERT INTO NDMPLevelMap (ClientId, FilesetId, FileSystem, DumpLevel)"
        " VALUES ('%s', '%s', '%s', %s)",
        edit_uint64(jr->ClientId, ed1), edit_uint64(jr->FileSetId, ed2), mdb->esc_name, "0");
   if (!INSERT_DB(jcr, mdb, mdb->cmd)) {
      Mmsg2(mdb->errmsg, _("Create DB NDMP Level Map record %s failed. ERR=%s\n"), mdb->cmd, sql_strerror(mdb));
      Jmsg(jcr, M_ERROR, 0, "%s", mdb->errmsg);
   } else {
      retval = true;
   }

bail_out:
   db_unlock(mdb);
   return retval;
}

/**
 * Create a NDMP Job Environment String
 * Returns: false on failure
 *          true on success
 */
bool db_create_ndmp_environment_string(JCR *jcr, B_DB *mdb, JOB_DBR *jr, char *name, char *value)
{
   bool retval = false;
   char ed1[50], ed2[50];
   char esc_name[MAX_ESCAPE_NAME_LENGTH];
   char esc_value[MAX_ESCAPE_NAME_LENGTH];

   db_lock(mdb);

   mdb->db_escape_string(jcr, esc_name, name, strlen(name));
   mdb->db_escape_string(jcr, esc_value, value, strlen(value));
   Mmsg(mdb->cmd, "INSERT INTO NDMPJobEnvironment (JobId, FileIndex, EnvName, EnvValue)"
                  " VALUES ('%s', '%s', '%s', '%s')",
        edit_int64(jr->JobId, ed1), edit_uint64(jr->FileIndex, ed2), esc_name, esc_value);
   if (!INSERT_DB(jcr, mdb, mdb->cmd)) {
      Mmsg2(mdb->errmsg, _("Create DB NDMP Job Environment record %s failed. ERR=%s\n"), mdb->cmd, sql_strerror(mdb));
      Jmsg(jcr, M_ERROR, 0, "%s", mdb->errmsg);
   } else {
      retval = true;
   }

   db_unlock(mdb);
   return retval;
}

/**
 * Create a Job Statistics record.
 * Returns: false on failure
 *          true on success
 */
bool db_create_job_statistics(JCR *jcr, B_DB *mdb, JOB_STATS_DBR *jsr)
{
   time_t stime;
   bool retval = false;
   char dt[MAX_TIME_LENGTH];
   char ed1[50], ed2[50], ed3[50], ed4[50];

   db_lock(mdb);

   stime = jsr->SampleTime;
   ASSERT(stime != 0);

   bstrutime(dt, sizeof(dt), stime);

   /*
    * Create job statistics record
    */
   Mmsg(mdb->cmd, "INSERT INTO JobStats (SampleTime, JobId, JobFiles, JobBytes, DeviceId)"
                  " VALUES ('%s', %s, %s, %s, %s)",
                  dt,
                  edit_int64(jsr->JobId, ed1),
                  edit_uint64(jsr->JobFiles, ed2),
                  edit_uint64(jsr->JobBytes, ed3),
                  edit_int64(jsr->DeviceId, ed4));
   Dmsg1(200, "Create job stats: %s\n", mdb->cmd);

   if (!INSERT_DB(jcr, mdb, mdb->cmd)) {
      Mmsg2(mdb->errmsg, _("Create DB JobStats record %s failed. ERR=%s\n"), mdb->cmd, sql_strerror(mdb));
      Jmsg(jcr, M_ERROR, 0, "%s", mdb->errmsg);
      goto bail_out;
   } else {
      retval = true;
   }

bail_out:
   db_unlock(mdb);
   return retval;
}

/**
 * Create a Device Statistics record.
 * Returns: false on failure
 *          true on success
 */
bool db_create_device_statistics(JCR *jcr, B_DB *mdb, DEVICE_STATS_DBR *dsr)
{
   time_t stime;
   bool retval = false;
   char dt[MAX_TIME_LENGTH];
   char ed1[50], ed2[50], ed3[50], ed4[50], ed5[50], ed6[50];
   char ed7[50], ed8[50], ed9[50], ed10[50], ed11[50], ed12[50];

   db_lock(mdb);

   stime = dsr->SampleTime;
   ASSERT(stime != 0);

   bstrutime(dt, sizeof(dt), stime);

   /*
    * Create device statistics record
    */
   Mmsg(mdb->cmd,
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
   Dmsg1(200, "Create device stats: %s\n", mdb->cmd);

   if (!INSERT_DB(jcr, mdb, mdb->cmd)) {
      Mmsg2(mdb->errmsg, _("Create DB DeviceStats record %s failed. ERR=%s\n"), mdb->cmd, sql_strerror(mdb));
      Jmsg(jcr, M_ERROR, 0, "%s", mdb->errmsg);
      goto bail_out;
   } else {
      retval = true;
   }

bail_out:
   db_unlock(mdb);
   return retval;
}

/**
 * Create a tapealert record.
 * Returns: false on failure
 *          true on success
 */
bool db_create_tapealert_statistics(JCR *jcr, B_DB *mdb, TAPEALERT_STATS_DBR *tsr)
{
   time_t stime;
   bool retval = false;
   char dt[MAX_TIME_LENGTH];
   char ed1[50], ed2[50];

   db_lock(mdb);

   stime = tsr->SampleTime;
   ASSERT(stime != 0);

   bstrutime(dt, sizeof(dt), stime);

   /*
    * Create device statistics record
    */
   Mmsg(mdb->cmd,
        "INSERT INTO TapeAlerts (DeviceId, SampleTime, AlertFlags)"
        " VALUES (%s, '%s', %s)",
        edit_int64(tsr->DeviceId, ed1),
        dt,
        edit_uint64(tsr->AlertFlags, ed2));
   Dmsg1(200, "Create tapealert: %s\n", mdb->cmd);

   if (!INSERT_DB(jcr, mdb, mdb->cmd)) {
      Mmsg2(mdb->errmsg, _("Create DB TapeAlerts record %s failed. ERR=%s\n"), mdb->cmd, sql_strerror(mdb));
      Jmsg(jcr, M_ERROR, 0, "%s", mdb->errmsg);
      goto bail_out;
   } else {
      retval = true;
   }

bail_out:
   db_unlock(mdb);
   return retval;
}
#endif /* HAVE_SQLITE3 || HAVE_MYSQL || HAVE_POSTGRESQL || HAVE_INGRES || HAVE_DBI */
