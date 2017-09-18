/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2009 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2016 Planets Communications B.V.
   Copyright (C) 2013-2017 Bareos GmbH & Co. KG

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
 * BAREOS Catalog Database List records interface routines
 */

#include "bareos.h"

#if HAVE_SQLITE3 || HAVE_MYSQL || HAVE_POSTGRESQL || HAVE_INGRES || HAVE_DBI

#include "cats.h"

/* -----------------------------------------------------------------------
 *
 *   Generic Routines (or almost generic)
 *
 * -----------------------------------------------------------------------
 */

/**
 * Submit general SQL query
 */
bool B_DB::list_sql_query(JCR *jcr, const char *query, OUTPUT_FORMATTER *sendit,
                          e_list_type type, bool verbose)
{
   return list_sql_query(jcr, query, sendit, type, "query", verbose);
}

bool B_DB::list_sql_query(JCR *jcr, B_DB::SQL_QUERY_ENUM query, OUTPUT_FORMATTER *sendit, e_list_type type, bool verbose)
{
   return list_sql_query(jcr, query, sendit, type, "query", verbose);
}

bool B_DB::list_sql_query(JCR *jcr, const char *query, OUTPUT_FORMATTER *sendit,
                          e_list_type type, const char *description, bool verbose)
{
   bool retval = false;

   db_lock(this);

   if (!sql_query(query, QF_STORE_RESULT)) {
      Mmsg(errmsg, _("Query failed: %s\n"), sql_strerror());
      if (verbose) {
         sendit->decoration(errmsg);
      }
      goto bail_out;
   }

   sendit->array_start(description);
   list_result(jcr, sendit, type);
   sendit->array_end(description);
   sql_free_result();
   retval = true;

bail_out:
   db_unlock(this);
   return retval;
}

bool B_DB::list_sql_query(JCR *jcr, B_DB::SQL_QUERY_ENUM query, OUTPUT_FORMATTER *sendit,
                          e_list_type type, const char *description, bool verbose)
{
   return list_sql_query(jcr, get_predefined_query(query), sendit, type, description, verbose);
}

void B_DB::list_pool_records(JCR *jcr, POOL_DBR *pdbr, OUTPUT_FORMATTER *sendit, e_list_type type)
{
   char esc[MAX_ESCAPE_NAME_LENGTH];

   db_lock(this);
   escape_string(jcr, esc, pdbr->Name, strlen(pdbr->Name));

   if (type == VERT_LIST) {
      if (pdbr->Name[0] != 0) {
         Mmsg(cmd, "SELECT PoolId,Name,NumVols,MaxVols,UseOnce,UseCatalog,"
              "AcceptAnyVolume,VolRetention,VolUseDuration,MaxVolJobs,MaxVolBytes,"
              "AutoPrune,Recycle,PoolType,LabelFormat,Enabled,ScratchPoolId,"
              "RecyclePoolId,LabelType "
              "FROM Pool WHERE Name='%s'", esc);
      } else {
         Mmsg(cmd, "SELECT PoolId,Name,NumVols,MaxVols,UseOnce,UseCatalog,"
              "AcceptAnyVolume,VolRetention,VolUseDuration,MaxVolJobs,MaxVolBytes,"
              "AutoPrune,Recycle,PoolType,LabelFormat,Enabled,ScratchPoolId,"
              "RecyclePoolId,LabelType "
              "FROM Pool ORDER BY PoolId");
      }
   } else {
      if (pdbr->Name[0] != 0) {
         Mmsg(cmd, "SELECT PoolId,Name,NumVols,MaxVols,PoolType,LabelFormat "
              "FROM Pool WHERE Name='%s'", esc);
      } else {
         Mmsg(cmd, "SELECT PoolId,Name,NumVols,MaxVols,PoolType,LabelFormat "
              "FROM Pool ORDER BY PoolId");
      }
   }

   if (!QUERY_DB(jcr, cmd)) {
      goto bail_out;
   }

   sendit->array_start("pools");
   list_result(jcr, sendit, type);
   sendit->array_end("pools");

   sql_free_result();

bail_out:
   db_unlock(this);
}

void B_DB::list_client_records(JCR *jcr, char *clientname, OUTPUT_FORMATTER *sendit, e_list_type type)
{
   db_lock(this);
   POOL_MEM clientfilter(PM_MESSAGE);

   if (clientname) {
      clientfilter.bsprintf("WHERE Name = '%s'", clientname);
   }
   if (type == VERT_LIST) {
      Mmsg(cmd, "SELECT ClientId,Name,Uname,AutoPrune,FileRetention,"
           "JobRetention "
           "FROM Client %s ORDER BY ClientId ", clientfilter.c_str());
   } else {
      Mmsg(cmd, "SELECT ClientId,Name,FileRetention,JobRetention "
           "FROM Client %s ORDER BY ClientId", clientfilter.c_str());
   }

   if (!QUERY_DB(jcr, cmd)) {
      goto bail_out;
   }

   sendit->array_start("clients");
   list_result(jcr, sendit, type);
   sendit->array_end("clients");

   sql_free_result();

bail_out:
   db_unlock(this);
}

void B_DB::list_storage_records(JCR *jcr, OUTPUT_FORMATTER *sendit, e_list_type type)
{
   db_lock(this);

   Mmsg(cmd, "SELECT StorageId,Name,AutoChanger FROM Storage");

   if (!QUERY_DB(jcr, cmd)) {
      goto bail_out;
   }

   sendit->array_start("storages");
   list_result(jcr, sendit, type);
   sendit->array_end("storages");

   sql_free_result();

bail_out:
   db_unlock(this);
}

/**
 * If VolumeName is non-zero, list the record for that Volume
 *   otherwise, list the Volumes in the Pool specified by PoolId
 */
void B_DB::list_media_records(JCR *jcr, MEDIA_DBR *mdbr, const char *range, bool count, OUTPUT_FORMATTER *sendit, e_list_type type)
{
   char ed1[50];
   char esc[MAX_ESCAPE_NAME_LENGTH];

   db_lock(this);
   escape_string(jcr, esc, mdbr->VolumeName, strlen(mdbr->VolumeName));

   if (type == VERT_LIST) {
      if (mdbr->VolumeName[0] != 0) {
         Mmsg(cmd, "SELECT MediaId,VolumeName,Slot,PoolId,"
                   "MediaType,FirstWritten,LastWritten,LabelDate,VolJobs,"
                   "VolFiles,VolBlocks,VolMounts,VolBytes,VolErrors,VolWrites,"
                   "VolCapacityBytes,VolStatus,Enabled,Recycle,VolRetention,"
                   "VolUseDuration,MaxVolJobs,MaxVolFiles,MaxVolBytes,InChanger,"
                   "EndFile,EndBlock,LabelType,StorageId,DeviceId,"
                   "LocationId,RecycleCount,InitialWrite,ScratchPoolId,RecyclePoolId, "
                   "Comment,Name AS Storage "
                   "FROM Media "
                   "LEFT JOIN Storage USING(StorageId) "
                   "WHERE Media.VolumeName='%s'", esc);
      } else if (mdbr->PoolId > 0) {
         Mmsg(cmd, "SELECT MediaId,VolumeName,Slot,PoolId,"
                   "MediaType,FirstWritten,LastWritten,LabelDate,VolJobs,"
                   "VolFiles,VolBlocks,VolMounts,VolBytes,VolErrors,VolWrites,"
                   "VolCapacityBytes,VolStatus,Enabled,Recycle,VolRetention,"
                   "VolUseDuration,MaxVolJobs,MaxVolFiles,MaxVolBytes,InChanger,"
                   "EndFile,EndBlock,LabelType,StorageId,DeviceId,"
                   "LocationId,RecycleCount,InitialWrite,ScratchPoolId,RecyclePoolId, "
                   "Comment,Name AS Storage "
                   "FROM Media "
                   "LEFT JOIN Storage USING(StorageId) "
                   "WHERE Media.PoolId=%s ORDER BY MediaId "
                   "%s",
              edit_int64(mdbr->PoolId, ed1), range);
      } else {
          Mmsg(cmd, "SELECT MediaId,VolumeName,Slot,PoolId,"
                    "MediaType,FirstWritten,LastWritten,LabelDate,VolJobs,"
                    "VolFiles,VolBlocks,VolMounts,VolBytes,VolErrors,VolWrites,"
                    "VolCapacityBytes,VolStatus,Enabled,Recycle,VolRetention,"
                    "VolUseDuration,MaxVolJobs,MaxVolFiles,MaxVolBytes,InChanger,"
                    "EndFile,EndBlock,LabelType,StorageId,DeviceId,"
                    "LocationId,RecycleCount,InitialWrite,ScratchPoolId,RecyclePoolId, "
                    "Comment,Name AS Storage "
                    "FROM Media "
                    "LEFT JOIN Storage USING(StorageId) "
                    "ORDER BY MediaId "
                    "%s", range);
      }
   } else {
      if (mdbr->VolumeName[0] != 0) {
         Mmsg(cmd, "SELECT MediaId,VolumeName,VolStatus,Enabled,"
                   "VolBytes,VolFiles,VolRetention,Recycle,Slot,InChanger,"
                   "MediaType,LastWritten,Name AS Storage "
                   "FROM Media "
                   "LEFT JOIN Storage USING(StorageId) "
                   "WHERE VolumeName='%s'", esc);
      } else if (mdbr->PoolId > 0) {
         Mmsg(cmd, "SELECT MediaId,VolumeName,VolStatus,Enabled,"
                   "VolBytes,VolFiles,VolRetention,Recycle,Slot,InChanger,"
                   "MediaType,LastWritten,Name AS Storage "
                   "FROM Media "
                   "LEFT JOIN Storage USING(StorageId) "
                   "WHERE PoolId=%s ORDER BY MediaId "
                   "%s",
              edit_int64(mdbr->PoolId, ed1), range);
      } else {
         Mmsg(cmd, "SELECT MediaId,VolumeName,VolStatus,Enabled,"
                   "VolBytes,VolFiles,VolRetention,Recycle,Slot,InChanger,"
                   "MediaType,LastWritten,Name AS Storage "
                   "FROM Media "
                   "LEFT JOIN Storage USING(StorageId) "
                   "ORDER BY MediaId "
                   "%s",
              range);
      }
   }

   if (count) {
      /* NOTE: ACLs are ignored. */
      if (mdbr->VolumeName[0] != 0) {
         fill_query(SQL_QUERY_list_volumes_by_name_count_1, esc);
      } else if (mdbr->PoolId > 0) {
         fill_query(SQL_QUERY_list_volumes_by_poolid_count_1, edit_int64(mdbr->PoolId, ed1));
      } else {
         fill_query(SQL_QUERY_list_volumes_count_0);
      }
   }

   if (!QUERY_DB(jcr, cmd)) {
      goto bail_out;
   }

   list_result(jcr, sendit, type);

   sql_free_result();

bail_out:
   db_unlock(this);
}

void B_DB::list_jobmedia_records(JCR *jcr, uint32_t JobId, OUTPUT_FORMATTER *sendit, e_list_type type)
{
   char ed1[50];

   db_lock(this);
   if (type == VERT_LIST) {
      if (JobId > 0) {                   /* do by JobId */
         Mmsg(cmd, "SELECT JobMediaId,JobId,Media.MediaId,Media.VolumeName,"
              "FirstIndex,LastIndex,StartFile,JobMedia.EndFile,StartBlock,"
              "JobMedia.EndBlock "
              "FROM JobMedia,Media WHERE Media.MediaId=JobMedia.MediaId "
              "AND JobMedia.JobId=%s", edit_int64(JobId, ed1));
      } else {
         Mmsg(cmd, "SELECT JobMediaId,JobId,Media.MediaId,Media.VolumeName,"
              "FirstIndex,LastIndex,StartFile,JobMedia.EndFile,StartBlock,"
              "JobMedia.EndBlock "
              "FROM JobMedia,Media WHERE Media.MediaId=JobMedia.MediaId");
      }

   } else {
      if (JobId > 0) {                   /* do by JobId */
         Mmsg(cmd, "SELECT JobId,Media.VolumeName,FirstIndex,LastIndex "
              "FROM JobMedia,Media WHERE Media.MediaId=JobMedia.MediaId "
              "AND JobMedia.JobId=%s", edit_int64(JobId, ed1));
      } else {
         Mmsg(cmd, "SELECT JobId,Media.VolumeName,FirstIndex,LastIndex "
              "FROM JobMedia,Media WHERE Media.MediaId=JobMedia.MediaId");
      }
   }
   if (!QUERY_DB(jcr, cmd)) {
      goto bail_out;
   }

   sendit->array_start("jobmedia");
   list_result(jcr, sendit, type);
   sendit->array_end("jobmedia");

   sql_free_result();

bail_out:
   db_unlock(this);
}

void B_DB::list_copies_records(JCR *jcr, const char *range, const char *JobIds,
                               OUTPUT_FORMATTER *send, e_list_type type)
{
   POOL_MEM str_jobids(PM_MESSAGE);

   if (JobIds && JobIds[0]) {
      Mmsg(str_jobids, " AND (Job.PriorJobId IN (%s) OR Job.JobId IN (%s)) ",
           JobIds, JobIds);
   }

   db_lock(this);
   Mmsg(cmd,
        "SELECT DISTINCT Job.PriorJobId AS JobId, Job.Job, "
                        "Job.JobId AS CopyJobId, Media.MediaType "
        "FROM Job "
        "JOIN JobMedia USING (JobId) "
        "JOIN Media USING (MediaId) "
        "WHERE Job.Type = '%c' %s ORDER BY Job.PriorJobId DESC %s",
        (char) JT_JOB_COPY, str_jobids.c_str(), range);

   if (!QUERY_DB(jcr, cmd)) {
      goto bail_out;
   }

   if (sql_num_rows()) {
      if (JobIds && JobIds[0]) {
         send->decoration(_("These JobIds have copies as follows:\n"));
      } else {
         send->decoration(_("The catalog contains copies as follows:\n"));
      }

      send->array_start("copies");
      list_result(jcr, send, type);
      send->array_end("copies");
   }

   sql_free_result();

bail_out:
   db_unlock(this);
}

void B_DB::list_log_records(JCR *jcr, const char *clientname, const char *range,
                            bool reverse, OUTPUT_FORMATTER *sendit, e_list_type type)
{
   POOL_MEM client_filter(PM_MESSAGE);

   if (clientname) {
      Mmsg(client_filter, "AND Client.Name = '%s' ", clientname);
   }

   if (reverse) {
     Mmsg(cmd, "SELECT LogId, Job.Name AS JobName, Client.Name AS ClientName, Time, LogText "
               "FROM Log "
               "JOIN Job USING (JobId) "
               "LEFT JOIN Client USING (ClientId) "
               "WHERE Job.Type != 'C' "
               "%s"
               "ORDER BY Log.LogId DESC %s",
          client_filter.c_str(), range);
   } else {
     Mmsg(cmd, "SELECT LogId, JobName, ClientName, Time, LogText FROM ("
                  "SELECT LogId, Job.Name AS JobName, Client.Name As ClientName, Time, LogText "
                  "FROM Log "
                  "JOIN Job USING (JobId) "
                  "LEFT JOIN Client USING (ClientId) "
                  "WHERE Job.Type != 'C' "
                  "%s"
                  "ORDER BY Log.LogId DESC %s"
               ") AS sub ORDER BY LogId ASC",
          client_filter.c_str(), range);
   }

   if (type != VERT_LIST) {
      /*
       * When something else then a vertical list is requested set the list type
       * to RAW_LIST e.g. non formated raw data as that makes the only sense for
       * the logtext output. The logtext already has things like \n etc in it
       * so we should just dump the raw content out for the best visible output.
       */
      type = RAW_LIST;
   }

   db_lock(this);

   if (!QUERY_DB(jcr, cmd)) {
      goto bail_out;
   }

   sendit->array_start("log");
   list_result(jcr, sendit, type);
   sendit->array_end("log");

   sql_free_result();

bail_out:
   db_unlock(this);
}

void B_DB::list_joblog_records(JCR *jcr, uint32_t JobId, const char *range, bool count, OUTPUT_FORMATTER *sendit, e_list_type type)
{
   char ed1[50];

   if (JobId <= 0) {
      return;
   }

   db_lock(this);
   if (count) {
      fill_query(SQL_QUERY_list_joblog_count_1, edit_int64(JobId, ed1));
   } else {
      fill_query(SQL_QUERY_list_joblog_2, edit_int64(JobId, ed1), range);
      if (type != VERT_LIST) {
         /*
          * When something else then a vertical list is requested set the list type
          * to RAW_LIST e.g. non formated raw data as that makes the only sense for
          * the logtext output. The logtext already has things like \n etc in it
          * so we should just dump the raw content out for the best visible output.
          */
         type = RAW_LIST;
      }
   }

   if (!QUERY_DB(jcr, cmd)) {
      goto bail_out;
   }

   sendit->array_start("joblog");
   list_result(jcr, sendit, type);
   sendit->array_end("joblog");

   sql_free_result();

bail_out:
   db_unlock(this);
}

/**
 * list job statistics records for certain jobid
 *
 */
void B_DB::list_jobstatistics_records(JCR *jcr, uint32_t JobId, OUTPUT_FORMATTER *sendit, e_list_type type)
{
   char ed1[50];

   if (JobId <= 0) {
      return;
   }
   db_lock(this);
      Mmsg(cmd, "SELECT DeviceId, SampleTime, JobId, JobFiles, JobBytes "
                "FROM JobStats "
                "WHERE JobStats.JobId=%s "
                "ORDER BY JobStats.SampleTime "
                ,edit_int64(JobId, ed1));
   if (!QUERY_DB(jcr, cmd)) {
      goto bail_out;
   }

   sendit->array_start("jobstats");
   list_result(jcr, sendit, type);
   sendit->array_end("jobstats");

   sql_free_result();

bail_out:
   db_unlock(this);
}

/**
 * List Job record(s) that match JOB_DBR
 */
void B_DB::list_job_records(JCR *jcr, JOB_DBR *jr, const char *range, const char *clientname, int jobstatus,
                            int joblevel, const char *volumename, utime_t since_time, bool last, bool count,
                            OUTPUT_FORMATTER *sendit, e_list_type type)
{
   char ed1[50];
   char dt[MAX_TIME_LENGTH];
   char esc[MAX_ESCAPE_NAME_LENGTH];
   POOL_MEM temp(PM_MESSAGE),
            selection(PM_MESSAGE),
            criteria(PM_MESSAGE);

   if (jr->JobId > 0) {
      temp.bsprintf("AND Job.JobId=%s", edit_int64(jr->JobId, ed1));
      pm_strcat(selection, temp.c_str());
   }

   if (jr->Name[0] != 0) {
      escape_string(jcr, esc, jr->Name, strlen(jr->Name));
      temp.bsprintf( "AND Job.Name = '%s' ", esc);
      pm_strcat(selection, temp.c_str());
   }

   if (clientname) {
      temp.bsprintf("AND Client.Name = '%s' ", clientname);
      pm_strcat(selection, temp.c_str());
   }

   if (jobstatus) {
      temp.bsprintf("AND Job.JobStatus = '%c' ", jobstatus);
      pm_strcat(selection, temp.c_str());
   }

   if (joblevel) {
      temp.bsprintf("AND Job.Level = '%c' ", joblevel);
      pm_strcat(selection, temp.c_str());
   }

   if (volumename) {
      temp.bsprintf("AND Media.Volumename = '%s' ", volumename);
      pm_strcat(selection, temp.c_str());
   }

   if (since_time) {
      bstrutime(dt, sizeof(dt), since_time);
      temp.bsprintf("AND Job.SchedTime > '%s' ", dt);
      pm_strcat(selection, temp.c_str());
   }

   db_lock(this);

   if (count) {
      fill_query(SQL_QUERY_list_jobs_count, selection.c_str(), range);
   } else if (last) {
      if (type == VERT_LIST) {
         fill_query(SQL_QUERY_list_jobs_long_last, selection.c_str(), range);
      } else {
         fill_query(SQL_QUERY_list_jobs_last, selection.c_str(), range);
      }
   } else {
      if (type == VERT_LIST) {
         fill_query(SQL_QUERY_list_jobs_long, selection.c_str(), range);
      } else {
         fill_query(SQL_QUERY_list_jobs, selection.c_str(), range);
      }
   }

   if (!QUERY_DB(jcr, cmd)) {
      goto bail_out;
   }

   sendit->array_start("jobs");
   list_result(jcr, sendit, type);
   sendit->array_end("jobs");

   sql_free_result();

bail_out:
   db_unlock(this);
}

/**
 * List Job totals
 */
void B_DB::list_job_totals(JCR *jcr, JOB_DBR *jr, OUTPUT_FORMATTER *sendit)
{
   db_lock(this);

   /*
    * List by Job
    */
   Mmsg(cmd, "SELECT count(*) AS Jobs,sum(JobFiles) "
             "AS Files,sum(JobBytes) AS Bytes,Name AS Job FROM Job GROUP BY Name");

   if (!QUERY_DB(jcr, cmd)) {
      goto bail_out;
   }

   sendit->array_start("jobs");
   list_result(jcr, sendit, HORZ_LIST);
   sendit->array_end("jobs");

   sql_free_result();

   /*
    * Do Grand Total
    */
   Mmsg(cmd, "SELECT COUNT(*) AS Jobs,sum(JobFiles) "
             "AS Files,sum(JobBytes) As Bytes FROM Job");

   if (!QUERY_DB(jcr, cmd)) {
      goto bail_out;
   }

   sendit->object_start("jobtotals");
   list_result(jcr, sendit, HORZ_LIST);
   sendit->object_end("jobtotals");

   sql_free_result();

bail_out:
   db_unlock(this);
}

void B_DB::list_files_for_job(JCR *jcr, JobId_t jobid, OUTPUT_FORMATTER *sendit)
{
   char ed1[50];
   LIST_CTX lctx(jcr, this, sendit, NF_LIST);

   db_lock(this);

   /*
    * Stupid MySQL is NON-STANDARD !
    */
   if (get_type_index() == SQL_TYPE_MYSQL) {
      Mmsg(cmd, "SELECT CONCAT(Path.Path,Name) AS Filename "
                 "FROM (SELECT PathId, Name FROM File WHERE JobId=%s "
                 "UNION ALL "
                 "SELECT PathId, Name "
                   "FROM BaseFiles JOIN File "
                         "ON (BaseFiles.FileId = File.FileId) "
                  "WHERE BaseFiles.JobId = %s"
           ") AS F, Path "
           "WHERE Path.PathId=F.PathId",
           edit_int64(jobid, ed1), ed1);
   } else {
      Mmsg(cmd, "SELECT Path.Path||Name AS Filename "
           "FROM (SELECT PathId, Name FROM File WHERE JobId=%s "
                  "UNION ALL "
                 "SELECT PathId, Name "
                   "FROM BaseFiles JOIN File "
                         "ON (BaseFiles.FileId = File.FileId) "
                  "WHERE BaseFiles.JobId = %s"
           ") AS F, Path "
           "WHERE Path.PathId=F.PathId",
           edit_int64(jobid, ed1), ed1);
   }

   sendit->array_start("filenames");
   if (!big_sql_query(cmd, ::list_result, &lctx)) {
       goto bail_out;
   }
   sendit->array_end("filenames");

   sql_free_result();

bail_out:
   db_unlock(this);
}

void B_DB::list_base_files_for_job(JCR *jcr, JobId_t jobid, OUTPUT_FORMATTER *sendit)
{
   char ed1[50];
   LIST_CTX lctx(jcr, this, sendit, NF_LIST);

   db_lock(this);

   /*
    * Stupid MySQL is NON-STANDARD !
    */
   if (get_type_index() == SQL_TYPE_MYSQL) {
      Mmsg(cmd, 
           "SELECT CONCAT(Path.Path,File.Name) AS Filename "
           "FROM BaseFiles, File, Path "
           "WHERE BaseFiles.JobId=%s AND BaseFiles.BaseJobId = File.JobId "
           "AND BaseFiles.FileId = File.FileId "
           "AND Path.PathId=File.PathId",
         edit_int64(jobid, ed1));
   } else {
      Mmsg(cmd, 
           "SELECT Path.Path||File.Name AS Filename "
           "FROM BaseFiles, File, Path "
           "WHERE BaseFiles.JobId=%s AND BaseFiles.BaseJobId = File.JobId "
           "AND BaseFiles.FileId = File.FileId "
           "AND Path.PathId=File.PathId",
           edit_int64(jobid, ed1));
   }

   sendit->array_start("files");
   if (!big_sql_query(cmd, ::list_result, &lctx)) {
       goto bail_out;
   }
   sendit->array_end("files");

   sql_free_result();

bail_out:
   db_unlock(this);
}

/**
 * List fileset
 */
void B_DB::list_filesets(JCR *jcr, JOB_DBR *jr, const char *range, OUTPUT_FORMATTER *sendit, e_list_type type)
{
   char esc[MAX_ESCAPE_NAME_LENGTH];

   db_lock(this);
   if (jr->Name[0] != 0) {
      escape_string(jcr, esc, jr->Name, strlen(jr->Name));
      Mmsg(cmd, "SELECT DISTINCT FileSet.FileSetId AS FileSetId, FileSet, MD5, CreateTime, FileSetText "
           "FROM Job, FileSet "
           "WHERE Job.FileSetId = FileSet.FileSetId "
           "AND Job.Name='%s'%s", esc, range);
   } else if (jr->Job[0] != 0) {
      escape_string(jcr, esc, jr->Job, strlen(jr->Job));
      Mmsg(cmd, "SELECT DISTINCT FileSet.FileSetId AS FileSetId, FileSet, MD5, CreateTime, FileSetText "
           "FROM Job, FileSet "
           "WHERE Job.FileSetId = FileSet.FileSetId "
           "AND Job.Name='%s'%s", esc, range);
   } else if (jr->JobId != 0) {
      Mmsg(cmd, "SELECT DISTINCT FileSet.FileSetId AS FileSetId, FileSet, MD5, CreateTime, FileSetText "
           "FROM Job, FileSet "
           "WHERE Job.FileSetId = FileSet.FileSetId "
           "AND Job.JobId='%s'%s", edit_int64(jr->JobId, esc), range);
   } else if (jr->FileSetId != 0) {
      Mmsg(cmd, "SELECT FileSetId, FileSet, MD5, CreateTime, FileSetText "
           "FROM FileSet "
           "WHERE  FileSetId=%s", edit_int64(jr->FileSetId, esc));
   } else {                           /* all records */
      Mmsg(cmd, "SELECT DISTINCT FileSet.FileSetId AS FileSetId, FileSet, MD5, CreateTime, FileSetText "
           "FROM FileSet ORDER BY FileSetId ASC%s", range);
   }

   if (!QUERY_DB(jcr, cmd)) {
      goto bail_out;
   }
   sendit->array_start("filesets");
   list_result(jcr, sendit, type);
   sendit->array_end("filesets");

   sql_free_result();

bail_out:
   db_unlock(this);
}
#endif /* HAVE_SQLITE3 || HAVE_MYSQL || HAVE_POSTGRESQL || HAVE_INGRES || HAVE_DBI */
