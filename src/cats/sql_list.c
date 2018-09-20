/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2009 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2016 Planets Communications B.V.
   Copyright (C) 2013-2016 Bareos GmbH & Co. KG

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
 * BAREOS Catalog Database List records interface routines
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

/*
 * Submit general SQL query
 */
bool db_list_sql_query(JCR *jcr, B_DB *mdb, const char *query,
                       OUTPUT_FORMATTER *sendit, e_list_type type,
                       bool verbose)
{
   return db_list_sql_query(jcr, mdb, query, sendit, type, "query", verbose);
}

bool db_list_sql_query(JCR *jcr, B_DB *mdb, const char *query,
                       OUTPUT_FORMATTER *sendit, e_list_type type,
                       const char *description, bool verbose)
{
   bool retval = false;

   if (db_list_sql_query_start(jcr, mdb, query, sendit, type, description, verbose)) {
      retval = true;
      db_list_sql_query_end(jcr, mdb);
   }

   return retval;
}

bool db_list_sql_query_start(JCR *jcr, B_DB *mdb, const char *query,
                             OUTPUT_FORMATTER *sendit, e_list_type type,
                             const char *description, bool verbose)
{
   db_lock(mdb);
   if (!sql_query(mdb, query, QF_STORE_RESULT)) {
      Mmsg(mdb->errmsg, _("Query failed: %s\n"), sql_strerror(mdb));
      if (verbose) {
         sendit->decoration(mdb->errmsg);
      }
      db_unlock(mdb);
      return false;
   }

   sendit->array_start(description);
   list_result(jcr, mdb, sendit, type);
   sendit->array_end(description);

   return true;
}


void db_list_sql_query_end(JCR *jcr, B_DB *mdb)
{
   sql_free_result(mdb);
   db_unlock(mdb);
}

void db_list_pool_records(JCR *jcr, B_DB *mdb, POOL_DBR *pdbr,
                          OUTPUT_FORMATTER *sendit, e_list_type type)
{
   char esc[MAX_ESCAPE_NAME_LENGTH];

   db_lock(mdb);
   mdb->db_escape_string(jcr, esc, pdbr->Name, strlen(pdbr->Name));

   if (type == VERT_LIST) {
      if (pdbr->Name[0] != 0) {
         Mmsg(mdb->cmd, "SELECT PoolId,Name,NumVols,MaxVols,UseOnce,UseCatalog,"
              "AcceptAnyVolume,VolRetention,VolUseDuration,MaxVolJobs,MaxVolBytes,"
              "AutoPrune,Recycle,PoolType,LabelFormat,Enabled,ScratchPoolId,"
              "RecyclePoolId,LabelType "
              " FROM Pool WHERE Name='%s'", esc);
      } else {
         Mmsg(mdb->cmd, "SELECT PoolId,Name,NumVols,MaxVols,UseOnce,UseCatalog,"
              "AcceptAnyVolume,VolRetention,VolUseDuration,MaxVolJobs,MaxVolBytes,"
              "AutoPrune,Recycle,PoolType,LabelFormat,Enabled,ScratchPoolId,"
              "RecyclePoolId,LabelType "
              " FROM Pool ORDER BY PoolId");
      }
   } else {
      if (pdbr->Name[0] != 0) {
         Mmsg(mdb->cmd, "SELECT PoolId,Name,NumVols,MaxVols,PoolType,LabelFormat "
              "FROM Pool WHERE Name='%s'", esc);
      } else {
         Mmsg(mdb->cmd, "SELECT PoolId,Name,NumVols,MaxVols,PoolType,LabelFormat "
              "FROM Pool ORDER BY PoolId");
      }
   }

   if (!QUERY_DB(jcr, mdb, mdb->cmd)) {
      goto bail_out;
   }

   sendit->array_start("pools");
   list_result(jcr, mdb, sendit, type);
   sendit->array_end("pools");

   sql_free_result(mdb);

bail_out:
   db_unlock(mdb);
}

void db_list_client_records(JCR *jcr, B_DB *mdb, char *clientname,
                            OUTPUT_FORMATTER *sendit, e_list_type type)
{
   db_lock(mdb);
   POOL_MEM clientfilter(PM_MESSAGE);

   if (clientname) {
      clientfilter.bsprintf("WHERE Name = '%s'", clientname);
   }
   if (type == VERT_LIST) {
      Mmsg(mdb->cmd, "SELECT ClientId,Name,Uname,AutoPrune,FileRetention,"
           "JobRetention "
           "FROM Client %s ORDER BY ClientId ", clientfilter.c_str());
   } else {
      Mmsg(mdb->cmd, "SELECT ClientId,Name,FileRetention,JobRetention "
           "FROM Client %s ORDER BY ClientId", clientfilter.c_str());
   }

   if (!QUERY_DB(jcr, mdb, mdb->cmd)) {
      goto bail_out;
   }

   sendit->array_start("clients");
   list_result(jcr, mdb, sendit, type);
   sendit->array_end("clients");

   sql_free_result(mdb);

bail_out:
   db_unlock(mdb);
}

void db_list_storage_records(JCR *jcr, B_DB *mdb,
                             OUTPUT_FORMATTER *sendit, e_list_type type)
{
   db_lock(mdb);


   Mmsg(mdb->cmd, "SELECT StorageId,Name,AutoChanger FROM Storage");

   if (!QUERY_DB(jcr, mdb, mdb->cmd)) {
      goto bail_out;
   }

   sendit->array_start("storages");
   list_result(jcr, mdb, sendit, type);
   sendit->array_end("storages");

   sql_free_result(mdb);

bail_out:
   db_unlock(mdb);
}

/*
 * If VolumeName is non-zero, list the record for that Volume
 *   otherwise, list the Volumes in the Pool specified by PoolId
 */
void db_list_media_records(JCR *jcr, B_DB *mdb, MEDIA_DBR *mdbr,
                           OUTPUT_FORMATTER *sendit, e_list_type type)
{
   char ed1[50];
   char esc[MAX_ESCAPE_NAME_LENGTH];

   db_lock(mdb);
   mdb->db_escape_string(jcr, esc, mdbr->VolumeName, strlen(mdbr->VolumeName));

   if (type == VERT_LIST) {
      if (mdbr->VolumeName[0] != 0) {
         Mmsg(mdb->cmd, "SELECT MediaId,VolumeName,Slot,PoolId,"
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
         Mmsg(mdb->cmd, "SELECT MediaId,VolumeName,Slot,PoolId,"
                        "MediaType,FirstWritten,LastWritten,LabelDate,VolJobs,"
                        "VolFiles,VolBlocks,VolMounts,VolBytes,VolErrors,VolWrites,"
                        "VolCapacityBytes,VolStatus,Enabled,Recycle,VolRetention,"
                        "VolUseDuration,MaxVolJobs,MaxVolFiles,MaxVolBytes,InChanger,"
                        "EndFile,EndBlock,LabelType,StorageId,DeviceId,"
                        "LocationId,RecycleCount,InitialWrite,ScratchPoolId,RecyclePoolId, "
                        "Comment,Name AS Storage "
                        "FROM Media "
                        "LEFT JOIN Storage USING(StorageId) "
                        "WHERE Media.PoolId=%s ORDER BY MediaId",
              edit_int64(mdbr->PoolId, ed1));
      } else {
          Mmsg(mdb->cmd, "SELECT MediaId,VolumeName,Slot,PoolId,"
                        "MediaType,FirstWritten,LastWritten,LabelDate,VolJobs,"
                        "VolFiles,VolBlocks,VolMounts,VolBytes,VolErrors,VolWrites,"
                        "VolCapacityBytes,VolStatus,Enabled,Recycle,VolRetention,"
                        "VolUseDuration,MaxVolJobs,MaxVolFiles,MaxVolBytes,InChanger,"
                        "EndFile,EndBlock,LabelType,StorageId,DeviceId,"
                        "LocationId,RecycleCount,InitialWrite,ScratchPoolId,RecyclePoolId, "
                        "Comment,Name AS Storage "
                        "FROM Media "
                        "LEFT JOIN Storage USING(StorageId) "
                        "ORDER BY MediaId");
      }
   } else {
      if (mdbr->VolumeName[0] != 0) {
         Mmsg(mdb->cmd, "SELECT MediaId,VolumeName,VolStatus,Enabled,"
                        "VolBytes,VolFiles,VolRetention,Recycle,Slot,InChanger,"
                        "MediaType,LastWritten,Name AS Storage "
                        "FROM Media "
                        "LEFT JOIN Storage USING(StorageId) "
                        "WHERE VolumeName='%s'", esc);
      } else if (mdbr->PoolId > 0) {
         Mmsg(mdb->cmd, "SELECT MediaId,VolumeName,VolStatus,Enabled,"
                        "VolBytes,VolFiles,VolRetention,Recycle,Slot,InChanger,"
                        "MediaType,LastWritten,Name AS Storage "
                        "FROM Media "
                        "LEFT JOIN Storage USING(StorageId) "
                        "WHERE PoolId=%s ORDER BY MediaId",
              edit_int64(mdbr->PoolId, ed1));
      } else {
         Mmsg(mdb->cmd, "SELECT MediaId,VolumeName,VolStatus,Enabled,"
                        "VolBytes,VolFiles,VolRetention,Recycle,Slot,InChanger,"
                        "MediaType,LastWritten,Name AS Storage "
                        "FROM Media "
                        "LEFT JOIN Storage USING(StorageId) "
                        "ORDER BY MediaId");
      }
   }

   if (!QUERY_DB(jcr, mdb, mdb->cmd)) {
      goto bail_out;
   }

   list_result(jcr, mdb, sendit, type);

   sql_free_result(mdb);

bail_out:
   db_unlock(mdb);
}

void db_list_jobmedia_records(JCR *jcr, B_DB *mdb, uint32_t JobId,
                              OUTPUT_FORMATTER *sendit, e_list_type type)
{
   char ed1[50];

   db_lock(mdb);
   if (type == VERT_LIST) {
      if (JobId > 0) {                   /* do by JobId */
         Mmsg(mdb->cmd, "SELECT JobMediaId,JobId,Media.MediaId,Media.VolumeName,"
              "FirstIndex,LastIndex,StartFile,JobMedia.EndFile,StartBlock,"
              "JobMedia.EndBlock "
              "FROM JobMedia,Media WHERE Media.MediaId=JobMedia.MediaId "
              "AND JobMedia.JobId=%s", edit_int64(JobId, ed1));
      } else {
         Mmsg(mdb->cmd, "SELECT JobMediaId,JobId,Media.MediaId,Media.VolumeName,"
              "FirstIndex,LastIndex,StartFile,JobMedia.EndFile,StartBlock,"
              "JobMedia.EndBlock "
              "FROM JobMedia,Media WHERE Media.MediaId=JobMedia.MediaId");
      }

   } else {
      if (JobId > 0) {                   /* do by JobId */
         Mmsg(mdb->cmd, "SELECT JobId,Media.VolumeName,FirstIndex,LastIndex "
              "FROM JobMedia,Media WHERE Media.MediaId=JobMedia.MediaId "
              "AND JobMedia.JobId=%s", edit_int64(JobId, ed1));
      } else {
         Mmsg(mdb->cmd, "SELECT JobId,Media.VolumeName,FirstIndex,LastIndex "
              "FROM JobMedia,Media WHERE Media.MediaId=JobMedia.MediaId");
      }
   }
   if (!QUERY_DB(jcr, mdb, mdb->cmd)) {
      goto bail_out;
   }

   sendit->array_start("jobmedia");
   list_result(jcr, mdb, sendit, type);
   sendit->array_end("jobmedia");

   sql_free_result(mdb);

bail_out:
   db_unlock(mdb);
}

void db_list_copies_records(JCR *jcr, B_DB *mdb, const char *range, const char *JobIds,
                            OUTPUT_FORMATTER *send, e_list_type type)
{
   POOL_MEM str_jobids(PM_MESSAGE);

   if (JobIds && JobIds[0]) {
      Mmsg(str_jobids, " AND (Job.PriorJobId IN (%s) OR Job.JobId IN (%s)) ",
           JobIds, JobIds);
   }

   db_lock(mdb);
   Mmsg(mdb->cmd,
        "SELECT DISTINCT Job.PriorJobId AS JobId, Job.Job, "
                        "Job.JobId AS CopyJobId, Media.MediaType "
        "FROM Job "
        "JOIN JobMedia USING (JobId) "
        "JOIN Media USING (MediaId) "
        "WHERE Job.Type = '%c' %s ORDER BY Job.PriorJobId DESC %s",
        (char) JT_JOB_COPY, str_jobids.c_str(), range);

   if (!QUERY_DB(jcr, mdb, mdb->cmd)) {
      goto bail_out;
   }

   if (sql_num_rows(mdb)) {
      if (JobIds && JobIds[0]) {
         send->decoration(_("These JobIds have copies as follows:\n"));
      } else {
         send->decoration(_("The catalog contains copies as follows:\n"));
      }

      send->array_start("copies");
      list_result(jcr, mdb, send, type);
      send->array_end("copies");
   }

   sql_free_result(mdb);

bail_out:
   db_unlock(mdb);
}

void db_list_log_records(JCR *jcr, B_DB *mdb, const char *clientname, const char *range,
                         bool reverse, OUTPUT_FORMATTER *sendit, e_list_type type)
{
   POOL_MEM client_filter(PM_MESSAGE);

   if (clientname) {
      Mmsg(client_filter, "AND Client.Name = '%s' ", clientname);
   }

   if (reverse) {
     Mmsg(mdb->cmd, "SELECT LogId, Job.Name AS JobName, Client.Name AS ClientName, Time, LogText "
                    "FROM Log "
                    "JOIN Job USING (JobId) "
                    "LEFT JOIN Client USING (ClientId) "
                    "WHERE Job.Type != 'C' "
                    "%s"
                    "ORDER BY Log.LogId DESC %s",
          client_filter.c_str(), range);
   } else {
     Mmsg(mdb->cmd, "SELECT LogId, JobName, ClientName, Time, LogText FROM ("
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

   db_lock(mdb);

   if (!QUERY_DB(jcr, mdb, mdb->cmd)) {
      goto bail_out;
   }

   sendit->array_start("log");
   list_result(jcr, mdb, sendit, type);
   sendit->array_end("log");

   sql_free_result(mdb);

bail_out:
   db_unlock(mdb);
}

void db_list_joblog_records(JCR *jcr, B_DB *mdb, uint32_t JobId,
                            OUTPUT_FORMATTER *sendit, e_list_type type)
{
   char ed1[50];

   if (JobId <= 0) {
      return;
   }
   db_lock(mdb);
   if (type == VERT_LIST) {
      Mmsg(mdb->cmd, "SELECT Time, LogText FROM Log "
                     "WHERE Log.JobId=%s ORDER BY Log.LogId", edit_int64(JobId, ed1));
   } else {
      Mmsg(mdb->cmd, "SELECT Time, LogText FROM Log "
                     "WHERE Log.JobId=%s ORDER BY Log.LogId", edit_int64(JobId, ed1));
      /*
       * When something else then a vertical list is requested set the list type
       * to RAW_LIST e.g. non formated raw data as that makes the only sense for
       * the logtext output. The logtext already has things like \n etc in it
       * so we should just dump the raw content out for the best visible output.
       */
      type = RAW_LIST;
   }
   if (!QUERY_DB(jcr, mdb, mdb->cmd)) {
      goto bail_out;
   }

   sendit->array_start("joblog");
   list_result(jcr, mdb, sendit, type);
   sendit->array_end("joblog");

   sql_free_result(mdb);

bail_out:
   db_unlock(mdb);
}

/*
 * List Job record(s) that match JOB_DBR
 */
void db_list_job_records(JCR *jcr, B_DB *mdb, JOB_DBR *jr, const char *range,
                         const char *clientname, int jobstatus, const char *volumename, const char *poolname,
                         utime_t since_time, bool last, bool count,
                         OUTPUT_FORMATTER *sendit, e_list_type type)
{
   char ed1[50];
   char esc[MAX_ESCAPE_NAME_LENGTH];
   POOL_MEM temp(PM_MESSAGE),
            selection(PM_MESSAGE),
            criteria(PM_MESSAGE);
   char dt[MAX_TIME_LENGTH];

   if (jr->JobId > 0) {
      temp.bsprintf("AND Job.JobId=%s", edit_int64(jr->JobId, ed1));
      pm_strcat(selection, temp.c_str());
   }

   if (jr->Name[0] != 0) {
      mdb->db_escape_string(jcr, esc, jr->Name, strlen(jr->Name));
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

   if (volumename) {
      temp.bsprintf("AND Media.Volumename = '%s' ", volumename);
      pm_strcat(selection, temp.c_str());
   }

   if (poolname) {
      temp.bsprintf("AND Job.poolid = (SELECT poolid FROM pool WHERE name = '%s' LIMIT 1) ", poolname);
      pm_strcat(selection, temp.c_str());
   }

   if (since_time) {
      bstrutime(dt, sizeof(dt), since_time);
      temp.bsprintf("AND Job.SchedTime > '%s' ", dt);
      pm_strcat(selection, temp.c_str());
   }

   db_lock(mdb);
   if (count) {
      Mmsg(mdb->cmd, list_jobs_count, selection.c_str(), range);
   } else if (last) {
      if (type == VERT_LIST) {
         Mmsg(mdb->cmd, list_jobs_long_last, selection.c_str(), range);
      } else {
         Mmsg(mdb->cmd, list_jobs_last, selection.c_str(), range);
      }
   } else {
      if (type == VERT_LIST) {
         Mmsg(mdb->cmd, list_jobs_long, selection.c_str(), range);
      } else {
         Mmsg(mdb->cmd, list_jobs, selection.c_str(), range);
      }
   }

   if (!QUERY_DB(jcr, mdb, mdb->cmd)) {
      goto bail_out;
   }

   sendit->array_start("jobs");
   list_result(jcr, mdb, sendit, type);
   sendit->array_end("jobs");

   sql_free_result(mdb);

bail_out:
   db_unlock(mdb);
}

/*
 * List Job totals
 */
void db_list_job_totals(JCR *jcr, B_DB *mdb, JOB_DBR *jr, OUTPUT_FORMATTER *sendit)
{
   db_lock(mdb);

   /*
    * List by Job
    */
   Mmsg(mdb->cmd, "SELECT COUNT(*) AS Jobs,sum(JobFiles) "
                  "AS Files,sum(JobBytes) AS Bytes,Name AS Job FROM Job GROUP BY Name");

   if (!QUERY_DB(jcr, mdb, mdb->cmd)) {
      goto bail_out;
   }

   sendit->array_start("jobs");
   list_result(jcr, mdb, sendit, HORZ_LIST);
   sendit->array_end("jobs");

   sql_free_result(mdb);

   /*
    * Do Grand Total
    */
   Mmsg(mdb->cmd, "SELECT COUNT(*) AS Jobs,sum(JobFiles) "
                  "AS Files,sum(JobBytes) As Bytes FROM Job");

   if (!QUERY_DB(jcr, mdb, mdb->cmd)) {
      goto bail_out;
   }

   sendit->object_start("jobtotals");
   list_result(jcr, mdb, sendit, HORZ_LIST);
   sendit->object_end("jobtotals");

   sql_free_result(mdb);

bail_out:
   db_unlock(mdb);
}

void db_list_files_for_job(JCR *jcr, B_DB *mdb, JobId_t jobid, OUTPUT_FORMATTER *sendit)
{
   char ed1[50];
   LIST_CTX lctx(jcr, mdb, sendit, NF_LIST);

   db_lock(mdb);

   /*
    * Stupid MySQL is NON-STANDARD !
    */
   if (db_get_type_index(mdb) == SQL_TYPE_MYSQL) {
      Mmsg(mdb->cmd, "SELECT CONCAT(Path.Path,Filename.Name) AS Filename "
           "FROM (SELECT PathId, FilenameId FROM File WHERE JobId=%s "
                  "UNION ALL "
                 "SELECT PathId, FilenameId "
                   "FROM BaseFiles JOIN File "
                         "ON (BaseFiles.FileId = File.FileId) "
                  "WHERE BaseFiles.JobId = %s"
           ") AS F, Filename,Path "
           "WHERE Filename.FilenameId=F.FilenameId "
           "AND Path.PathId=F.PathId",
           edit_int64(jobid, ed1), ed1);
   } else {
      Mmsg(mdb->cmd, "SELECT Path.Path||Filename.Name AS Filename "
           "FROM (SELECT PathId, FilenameId FROM File WHERE JobId=%s "
                  "UNION ALL "
                 "SELECT PathId, FilenameId "
                   "FROM BaseFiles JOIN File "
                         "ON (BaseFiles.FileId = File.FileId) "
                  "WHERE BaseFiles.JobId = %s"
           ") AS F, Filename,Path "
           "WHERE Filename.FilenameId=F.FilenameId "
           "AND Path.PathId=F.PathId",
           edit_int64(jobid, ed1), ed1);
   }

   sendit->array_start("filenames");
   if (!db_big_sql_query(mdb, mdb->cmd, list_result, &lctx)) {
       goto bail_out;
   }
   sendit->array_end("filenames");

   sql_free_result(mdb);

bail_out:
   db_unlock(mdb);
}

void db_list_base_files_for_job(JCR *jcr, B_DB *mdb, JobId_t jobid, OUTPUT_FORMATTER *sendit)
{
   char ed1[50];
   LIST_CTX lctx(jcr, mdb, sendit, NF_LIST);

   db_lock(mdb);

   /*
    * Stupid MySQL is NON-STANDARD !
    */
   if (db_get_type_index(mdb) == SQL_TYPE_MYSQL) {
      Mmsg(mdb->cmd, "SELECT CONCAT(Path.Path,Filename.Name) AS Filename "
           "FROM BaseFiles, File, Filename, Path "
           "WHERE BaseFiles.JobId=%s AND BaseFiles.BaseJobId = File.JobId "
           "AND BaseFiles.FileId = File.FileId "
           "AND Filename.FilenameId=File.FilenameId "
           "AND Path.PathId=File.PathId",
         edit_int64(jobid, ed1));
   } else {
      Mmsg(mdb->cmd, "SELECT Path.Path||Filename.Name AS Filename "
           "FROM BaseFiles, File, Filename, Path "
           "WHERE BaseFiles.JobId=%s AND BaseFiles.BaseJobId = File.JobId "
           "AND BaseFiles.FileId = File.FileId "
           "AND Filename.FilenameId=File.FilenameId "
           "AND Path.PathId=File.PathId",
           edit_int64(jobid, ed1));
   }

   sendit->array_start("files");
   if (!db_big_sql_query(mdb, mdb->cmd, list_result, &lctx)) {
       goto bail_out;
   }
   sendit->array_end("files");

   sql_free_result(mdb);

bail_out:
   db_unlock(mdb);
}

/*
 * List fileset
 */
void db_list_filesets(JCR *jcr, B_DB *mdb, JOB_DBR *jr, const char *range,
                      OUTPUT_FORMATTER *sendit, e_list_type type)
{
   char esc[MAX_ESCAPE_NAME_LENGTH];

   db_lock(mdb);
   if (jr->Name[0] != 0) {
      mdb->db_escape_string(jcr, esc, jr->Name, strlen(jr->Name));
      Mmsg(mdb->cmd, "SELECT DISTINCT FileSet.FileSetId AS FileSetId, FileSet, MD5, CreateTime, FileSetText "
           "FROM Job, FileSet "
           "WHERE Job.FileSetId = FileSet.FileSetId "
           "AND Job.Name='%s'%s", esc, range);
   } else if (jr->Job[0] != 0) {
      mdb->db_escape_string(jcr, esc, jr->Job, strlen(jr->Job));
      Mmsg(mdb->cmd, "SELECT DISTINCT FileSet.FileSetId AS FileSetId, FileSet, MD5, CreateTime, FileSetText "
           "FROM Job, FileSet "
           "WHERE Job.FileSetId = FileSet.FileSetId "
           "AND Job.Name='%s'%s", esc, range);
   } else if (jr->JobId != 0) {
      Mmsg(mdb->cmd, "SELECT DISTINCT FileSet.FileSetId AS FileSetId, FileSet, MD5, CreateTime, FileSetText "
           "FROM Job, FileSet "
           "WHERE Job.FileSetId = FileSet.FileSetId "
           "AND Job.JobId='%s'%s", edit_int64(jr->JobId, esc), range);
   } else if (jr->FileSetId != 0) {
      Mmsg(mdb->cmd, "SELECT FileSetId, FileSet, MD5, CreateTime, FileSetText "
           "FROM FileSet "
           "WHERE  FileSetId=%s", edit_int64(jr->FileSetId, esc));
   } else {                           /* all records */
      Mmsg(mdb->cmd, "SELECT DISTINCT FileSet.FileSetId AS FileSetId, FileSet, MD5, CreateTime, FileSetText "
           "FROM FileSet ORDER BY FileSetId ASC%s", range);
   }

   if (!QUERY_DB(jcr, mdb, mdb->cmd)) {
      goto bail_out;
   }
   sendit->array_start("filesets");
   list_result(jcr, mdb, sendit, type);
   sendit->array_end("filesets");

   sql_free_result(mdb);

bail_out:
   db_unlock(mdb);
}

#endif /* HAVE_SQLITE3 || HAVE_MYSQL || HAVE_POSTGRESQL || HAVE_INGRES || HAVE_DBI */
