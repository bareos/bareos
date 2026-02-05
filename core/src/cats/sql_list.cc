/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2009 Free Software Foundation Europe e.V.
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
 * BAREOS Catalog Database List records interface routines
 */

#include "include/bareos.h"

#if HAVE_POSTGRESQL

#  include "cats.h"
#  include "lib/edit.h"
#  include "lib/util.h"

/* -----------------------------------------------------------------------
 *
 *   Generic Routines (or almost generic)
 *
 * -----------------------------------------------------------------------
 */

// Submit general SQL query
bool BareosDb::ListSqlQuery(JobControlRecord* jcr,
                            const char* query,
                            OutputFormatter* sendit,
                            e_list_type type,
                            const bool verbose)
{
  return ListSqlQuery(jcr, query, sendit, type, "query", verbose);
}

bool BareosDb::ListSqlQuery(JobControlRecord* jcr,
                            SQL_QUERY query,
                            OutputFormatter* sendit,
                            e_list_type type,
                            const bool verbose)
{
  return ListSqlQuery(jcr, query, sendit, type, "query", verbose);
}

bool BareosDb::ListSqlQuery(JobControlRecord* jcr,
                            const char* query,
                            OutputFormatter* sendit,
                            e_list_type type,
                            const char* description,
                            const bool verbose,
                            const CollapseMode collapse)
{
  DbLocker _{this};

  if (!SqlQuery(query)) {
    Mmsg(errmsg, T_("Query failed: %s\n"), sql_strerror());
    if (verbose) { sendit->Decoration("%s", errmsg); }
    return false;
  }

  if ((collapse == CollapseMode::Collapse) && (SqlNumRows() > 1)) {
    Mmsg(errmsg,
         T_("Query returned %d rows. In collapsed mode, only one row is "
            "accepted.\n"),
         SqlNumRows());
    if (verbose) { sendit->Decoration("%s", errmsg); }
    return false;
  }

  if (collapse == CollapseMode::Collapse) {
    sendit->ObjectStart(description);
    ListResult(jcr, sendit, type);
    sendit->ObjectEnd(description);
  } else {
    sendit->ArrayStart(description);
    ListResult(jcr, sendit, type);
    sendit->ArrayEnd(description);
  }
  SqlFreeResult();

  return true;
}

bool BareosDb::ListSqlQuery(JobControlRecord* jcr,
                            SQL_QUERY query,
                            OutputFormatter* sendit,
                            e_list_type type,
                            const char* description,
                            const bool verbose,
                            const CollapseMode collapse)
{
  return ListSqlQuery(jcr, get_predefined_query(query), sendit, type,
                      description, verbose, collapse);
}

void BareosDb::ListPoolRecords(JobControlRecord* jcr,
                               PoolDbRecord* pdbr,
                               OutputFormatter* sendit,
                               e_list_type type)
{
  char escaped_pool_name[MAX_ESCAPE_NAME_LENGTH];

  PoolMem query(PM_MESSAGE);
  PoolMem select(PM_MESSAGE);

  DbLocker _{this};
  EscapeString(jcr, escaped_pool_name, pdbr->Name, strlen(pdbr->Name));

  if (type == VERT_LIST) {
    Mmsg(select,
         "SELECT PoolId,Name,NumVols,MaxVols,UseOnce,UseCatalog,"
         "AcceptAnyVolume,VolRetention,VolUseDuration,MaxVolJobs,MaxVolBytes,"
         "AutoPrune,Recycle,PoolType,LabelFormat,Enabled,ScratchPoolId,"
         "RecyclePoolId,LabelType ");
    if (pdbr->Name[0] != 0) {
      query.bsprintf("%s FROM Pool WHERE Name='%s'", select.c_str(),
                     escaped_pool_name);
    } else if (pdbr->PoolId > 0) {
      query.bsprintf("%s FROM Pool WHERE poolid=%d", select.c_str(),
                     pdbr->PoolId);
    } else {
      query.bsprintf("%s FROM Pool ORDER BY PoolId", select.c_str());
    }
  } else {
    Mmsg(select, "SELECT PoolId,Name,NumVols,MaxVols,PoolType,LabelFormat ");
    if (pdbr->Name[0] != 0) {
      query.bsprintf("%s FROM Pool WHERE Name='%s'", select.c_str(),
                     escaped_pool_name);
    } else if (pdbr->PoolId > 0) {
      query.bsprintf("%s FROM Pool WHERE poolid=%d", select.c_str(),
                     pdbr->PoolId);
    } else {
      query.bsprintf("%s FROM Pool ORDER BY PoolId", select.c_str());
    }
  }

  if (!QueryDb(jcr, query.c_str())) { return; }

  sendit->ArrayStart("pools");
  ListResult(jcr, sendit, type);
  sendit->ArrayEnd("pools");

  SqlFreeResult();
}

void BareosDb::ListClientRecords(JobControlRecord* jcr,
                                 char* clientname,
                                 OutputFormatter* sendit,
                                 e_list_type type)
{
  DbLocker _{this};
  PoolMem clientfilter(PM_MESSAGE);

  if (clientname) { clientfilter.bsprintf("WHERE Name = '%s'", clientname); }
  if (type == VERT_LIST) {
    Mmsg(cmd,
         "SELECT ClientId,Name,Uname,AutoPrune,FileRetention,"
         "JobRetention "
         "FROM Client %s ORDER BY ClientId ",
         clientfilter.c_str());
  } else {
    Mmsg(cmd,
         "SELECT ClientId,Name,FileRetention,JobRetention "
         "FROM Client %s ORDER BY ClientId ",
         clientfilter.c_str());
  }

  if (!QueryDb(jcr, cmd)) { return; }

  sendit->ArrayStart("clients");
  ListResult(jcr, sendit, type);
  sendit->ArrayEnd("clients");

  SqlFreeResult();
}

/**
 * If VolumeName is non-zero, list the record for that Volume
 *   otherwise, list the Volumes in the Pool specified by PoolId
 */
void BareosDb::ListMediaRecords(JobControlRecord* jcr,
                                MediaDbRecord* mdbr,
                                const char* range,
                                bool count,
                                OutputFormatter* sendit,
                                e_list_type type)
{
  char ed1[50];
  char esc[MAX_ESCAPE_NAME_LENGTH];
  PoolMem select(PM_MESSAGE);
  PoolMem query(PM_MESSAGE);

  EscapeString(jcr, esc, mdbr->VolumeName, strlen(mdbr->VolumeName));

  /* There is one case where ListMediaRecords() is called from SelectMediaDbr()
   * with the range argument set to NULL. To avoid problems, we set the range to
   * an empty string if range is set to NULL. Otherwise it would result in
   * malformed SQL queries. */
  if (range == NULL) { range = ""; }

  if (count) {
    /* NOTE: ACLs are ignored. */
    if (mdbr->VolumeName[0] != 0) {
      FillQuery(query, SQL_QUERY::list_volumes_by_name_count_1, esc);
    } else if (mdbr->PoolId > 0) {
      FillQuery(query, SQL_QUERY::list_volumes_by_poolid_count_1,
                edit_int64(mdbr->PoolId, ed1));
    } else {
      FillQuery(query, SQL_QUERY::list_volumes_count_0);
    }
  } else {
    if (type == VERT_LIST) {
      FillQuery(select, SQL_QUERY::list_volumes_select_long_0);
    } else {
      FillQuery(select, SQL_QUERY::list_volumes_select_0);
    }

    if (mdbr->VolumeName[0] != 0) {
      query.bsprintf("%s WHERE VolumeName='%s'", select.c_str(), esc);
    } else if (mdbr->PoolId > 0) {
      query.bsprintf("%s WHERE PoolId=%s ORDER BY MediaId %s", select.c_str(),
                     edit_int64(mdbr->PoolId, ed1), range);
    } else if (mdbr->MediaId > 0) {
      query.bsprintf("%s WHERE MediaId=%s ORDER BY MediaId %s", select.c_str(),
                     edit_int64(mdbr->MediaId, ed1), range);
    } else {
      query.bsprintf("%s ORDER BY MediaId %s", select.c_str(), range);
    }
  }

  DbLocker _{this};

  if (!QueryDb(jcr, query.c_str())) { return; }

  ListResult(jcr, sendit, type);

  SqlFreeResult();
}


void BareosDb::ListJobmediaRecords(JobControlRecord* jcr,
                                   uint32_t JobId,
                                   OutputFormatter* sendit,
                                   e_list_type type)
{
  char ed1[50];

  DbLocker _{this};
  if (type == VERT_LIST) {
    if (JobId > 0) { /* do by JobId */
      Mmsg(cmd,
           "SELECT JobMediaId,JobId,Media.MediaId,Media.VolumeName,"
           "FirstIndex,LastIndex,StartFile,JobMedia.EndFile,StartBlock,"
           "JobMedia.EndBlock "
           "FROM JobMedia,Media WHERE Media.MediaId=JobMedia.MediaId "
           "AND JobMedia.JobId=%s ",
           edit_int64(JobId, ed1));
    } else {
      Mmsg(cmd,
           "SELECT JobMediaId,JobId,Media.MediaId,Media.VolumeName,"
           "FirstIndex,LastIndex,StartFile,JobMedia.EndFile,StartBlock,"
           "JobMedia.EndBlock "
           "FROM JobMedia,Media WHERE Media.MediaId=JobMedia.MediaId ");
    }

  } else {
    if (JobId > 0) { /* do by JobId */
      Mmsg(cmd,
           "SELECT JobId,Media.VolumeName,FirstIndex,LastIndex "
           "FROM JobMedia,Media WHERE Media.MediaId=JobMedia.MediaId "
           "AND JobMedia.JobId=%s ",
           edit_int64(JobId, ed1));
    } else {
      Mmsg(cmd,
           "SELECT JobId,Media.VolumeName,FirstIndex,LastIndex "
           "FROM JobMedia,Media WHERE Media.MediaId=JobMedia.MediaId ");
    }
  }
  if (!QueryDb(jcr, cmd)) { return; }

  sendit->ArrayStart("jobmedia");
  ListResult(jcr, sendit, type);
  sendit->ArrayEnd("jobmedia");

  SqlFreeResult();
}


void BareosDb::ListVolumesOfJobid(JobControlRecord* jcr,
                                  uint32_t JobId,
                                  OutputFormatter* sendit,
                                  e_list_type type)
{
  char ed1[50];

  if (JobId <= 0) { return; }

  DbLocker _{this};
  if (type == VERT_LIST) {
    Mmsg(cmd,
         "SELECT JobMediaId,JobId,Media.MediaId,Media.VolumeName "
         "FROM JobMedia,Media WHERE Media.MediaId=JobMedia.MediaId "
         "AND JobMedia.JobId=%s ",
         edit_int64(JobId, ed1));
  } else {
    Mmsg(cmd,
         "SELECT DISTINCT Media.VolumeName "
         "FROM JobMedia,Media WHERE Media.MediaId=JobMedia.MediaId "
         "AND JobMedia.JobId=%s ",
         edit_int64(JobId, ed1));
  }
  if (!QueryDb(jcr, cmd)) { return; }

  sendit->ArrayStart("volumes");
  ListResult(jcr, sendit, type);
  sendit->ArrayEnd("volumes");

  SqlFreeResult();
}


void BareosDb::ListCopiesRecords(JobControlRecord* jcr,
                                 const char* range,
                                 const char* JobIds,
                                 OutputFormatter* send,
                                 e_list_type type)
{
  PoolMem str_jobids(PM_MESSAGE);

  if (JobIds && JobIds[0]) {
    Mmsg(str_jobids, " AND (Job.PriorJobId IN (%s) OR Job.JobId IN (%s)) ",
         JobIds, JobIds);
  }

  DbLocker _{this};
  Mmsg(cmd,
       "SELECT DISTINCT Job.PriorJobId AS JobId, Job.Job, "
       "Job.JobId AS CopyJobId, Media.MediaType "
       "FROM Job "
       "JOIN JobMedia USING (JobId) "
       "JOIN Media USING (MediaId) "
       "WHERE Job.Type = '%c' %s ORDER BY Job.PriorJobId DESC %s ",
       (char)JT_JOB_COPY, str_jobids.c_str(), range);

  if (!QueryDb(jcr, cmd)) { return; }

  if (SqlNumRows()) {
    if (JobIds && JobIds[0]) {
      send->Decoration(T_("These JobIds have copies as follows:\n"));
    } else {
      send->Decoration(T_("The catalog contains copies as follows:\n"));
    }

    send->ArrayStart("copies");
    ListResult(jcr, send, type);
    send->ArrayEnd("copies");
  }

  SqlFreeResult();
}

void BareosDb::ListLogRecords(JobControlRecord* jcr,
                              const char* clientname,
                              const char* range,
                              bool reverse,
                              OutputFormatter* sendit,
                              e_list_type type)
{
  PoolMem client_filter(PM_MESSAGE);

  if (clientname) {
    Mmsg(client_filter, "AND Client.Name = '%s' ", clientname);
  }

  if (reverse) {
    Mmsg(cmd,
         "SELECT LogId, Job.Name AS JobName, Client.Name AS ClientName, Time, "
         "LogText "
         "FROM Log "
         "JOIN Job USING (JobId) "
         "LEFT JOIN Client USING (ClientId) "
         "WHERE Job.Type != 'C' "
         "%s"
         "ORDER BY Log.LogId DESC %s ",
         client_filter.c_str(), range);
  } else {
    Mmsg(cmd,
         "SELECT LogId, JobName, ClientName, Time, LogText FROM ("
         "SELECT LogId, Job.Name AS JobName, Client.Name As ClientName, Time, "
         "LogText "
         "FROM Log "
         "JOIN Job USING (JobId) "
         "LEFT JOIN Client USING (ClientId) "
         "WHERE Job.Type != 'C' "
         "%s"
         "ORDER BY Log.LogId DESC %s "
         ") AS sub ORDER BY LogId ASC ",
         client_filter.c_str(), range);
  }

  if (type != VERT_LIST) {
    /* When something else then a vertical list is requested set the list type
     * to RAW_LIST e.g. non formated raw data as that makes the only sense for
     * the logtext output. The logtext already has things like \n etc in it
     * so we should just dump the raw content out for the best visible output.
     */
    type = RAW_LIST;
  }

  DbLocker _{this};

  if (!QueryDb(jcr, cmd)) { return; }

  sendit->ArrayStart("log");
  ListResult(jcr, sendit, type);
  sendit->ArrayEnd("log");

  SqlFreeResult();
}

void BareosDb::ListJoblogRecords(JobControlRecord* jcr,
                                 uint32_t JobId,
                                 const char* range,
                                 bool count,
                                 OutputFormatter* sendit,
                                 e_list_type type)
{
  char ed1[50];

  if (JobId <= 0) { return; }

  DbLocker _{this};
  if (count) {
    FillQuery(SQL_QUERY::list_joblog_count_1, edit_int64(JobId, ed1));
  } else {
    FillQuery(SQL_QUERY::list_joblog_2, edit_int64(JobId, ed1), range);
    if (type != VERT_LIST) {
      /* When something else then a vertical list is requested set the list type
       * to RAW_LIST e.g. non formated raw data as that makes the only sense for
       * the logtext output. The logtext already has things like \n etc in it
       * so we should just dump the raw content out for the best visible output.
       */
      type = RAW_LIST;
    }
  }

  if (!QueryDb(jcr, cmd)) { return; }

  sendit->ArrayStart("joblog");
  ListResult(jcr, sendit, type);
  sendit->ArrayEnd("joblog");

  SqlFreeResult();
}

/**
 * list job statistics records for certain jobid
 *
 */
void BareosDb::ListJobstatisticsRecords(JobControlRecord* jcr,
                                        uint32_t JobId,
                                        OutputFormatter* sendit,
                                        e_list_type type)
{
  char ed1[50];

  if (JobId <= 0) { return; }
  DbLocker _{this};
  Mmsg(cmd,
       "SELECT DeviceId, SampleTime, JobId, JobFiles, JobBytes "
       "FROM JobStats "
       "WHERE JobStats.JobId=%s "
       "ORDER BY JobStats.SampleTime ",
       edit_int64(JobId, ed1));
  if (!QueryDb(jcr, cmd)) { return; }

  sendit->ArrayStart("jobstats");
  ListResult(jcr, sendit, type);
  sendit->ArrayEnd("jobstats");

  SqlFreeResult();
}

void BareosDb::ListJobRecords(JobControlRecord* jcr,
                              JobDbRecord* jr,
                              const char* range,
                              const char* clientname,
                              std::vector<char> jobstatuslist,
                              std::vector<char> joblevels,
                              std::vector<char> jobtypes,
                              const char* volumename,
                              const char* poolname,
                              utime_t since_time,
                              bool last,
                              bool count,
                              OutputFormatter* sendit,
                              e_list_type type)
{
  char ed1[50];
  char dt[MAX_TIME_LENGTH];
  char esc[MAX_ESCAPE_NAME_LENGTH];
  PoolMem temp(PM_MESSAGE), selection(PM_MESSAGE), criteria(PM_MESSAGE);

  if (jr->JobId > 0) {
    temp.bsprintf("AND Job.JobId=%s ", edit_int64(jr->JobId, ed1));
    PmStrcat(selection, temp.c_str());
  }

  if (jr->Name[0] != 0) {
    EscapeString(jcr, esc, jr->Name, strlen(jr->Name));
    temp.bsprintf("AND Job.Name = '%s' ", esc);
    PmStrcat(selection, temp.c_str());
  }

  if (clientname) {
    temp.bsprintf("AND Client.Name = '%s' ", clientname);
    PmStrcat(selection, temp.c_str());
  }

  if (!jobstatuslist.empty()) {
    std::string jobStatuses
        = CreateDelimitedStringForSqlQueries(jobstatuslist, ',');
    temp.bsprintf("AND Job.JobStatus in (%s) ", jobStatuses.c_str());
    PmStrcat(selection, temp.c_str());
  }

  if (!joblevels.empty()) {
    std::string levels = CreateDelimitedStringForSqlQueries(joblevels, ',');
    temp.bsprintf("AND Job.Level in (%s) ", levels.c_str());
    PmStrcat(selection, temp.c_str());
  }

  if (!jobtypes.empty()) {
    std::string types = CreateDelimitedStringForSqlQueries(jobtypes, ',');
    temp.bsprintf("AND Job.Type in (%s) ", types.c_str());
    PmStrcat(selection, temp.c_str());
  }

  if (volumename) {
    temp.bsprintf("AND Media.Volumename = '%s' ", volumename);
    PmStrcat(selection, temp.c_str());
  }

  if (poolname) {
    temp.bsprintf(
        "AND Job.poolid = (SELECT poolid FROM pool WHERE name = '%s' LIMIT 1) ",
        poolname);
    PmStrcat(selection, temp.c_str());
  }

  if (since_time) {
    bstrutime(dt, sizeof(dt), since_time);
    temp.bsprintf("AND Job.SchedTime > '%s' ", dt);
    PmStrcat(selection, temp.c_str());
  }

  DbLocker _{this};

  if (count) {
    FillQuery(SQL_QUERY::list_jobs_count, selection.c_str(), range);
  } else if (last) {
    if (type == VERT_LIST) {
      FillQuery(SQL_QUERY::list_jobs_long_last, selection.c_str(), range);
    } else {
      FillQuery(SQL_QUERY::list_jobs_last, selection.c_str(), range);
    }
  } else {
    if (type == VERT_LIST) {
      FillQuery(SQL_QUERY::list_jobs_long, selection.c_str(), range);
    } else {
      FillQuery(SQL_QUERY::list_jobs, selection.c_str(), range);
    }
  }

  if (!QueryDb(jcr, cmd)) { return; }

  sendit->ArrayStart("jobs");
  ListResult(jcr, sendit, type);
  sendit->ArrayEnd("jobs");

  SqlFreeResult();
}

void BareosDb::ListJobTotals(JobControlRecord* jcr,
                             JobDbRecord*,
                             OutputFormatter* sendit)
{
  DbLocker _{this};

  // List by Job
  // JobTypes BACKUP(B),ARCHIVE(A,a), JOB_COPY(C)
  Mmsg(cmd,
       "SELECT COUNT(*) AS Jobs, SUM(JobFiles) "
       "AS Files, SUM(JobBytes) AS Bytes, Name AS Job FROM Job WHERE Type IN "
       "('B','A','a','C') GROUP BY Name");

  if (!QueryDb(jcr, cmd)) { return; }

  sendit->ArrayStart("jobs");
  ListResult(jcr, sendit, HORZ_LIST);
  sendit->ArrayEnd("jobs");

  SqlFreeResult();

  // Do Grand Total
  // JobTypes BACKUP(B),ARCHIVE(A,a), JOB_COPY(C)
  Mmsg(cmd,
       "SELECT COUNT(*) AS Jobs, SUM(JobFiles) "
       "AS Files, SUM(JobBytes) AS Bytes FROM Job WHERE Type IN "
       "('B','A','a','C')");

  if (!QueryDb(jcr, cmd)) { return; }

  sendit->ObjectStart("jobtotals");
  ListResult(jcr, sendit, HORZ_LIST);
  sendit->ObjectEnd("jobtotals");

  SqlFreeResult();
}

void BareosDb::ListFilesForJob(JobControlRecord* jcr,
                               JobId_t jobid,
                               OutputFormatter* sendit)
{
  char ed1[50];
  ListContext lctx(jcr, this, sendit, NF_LIST);

  DbLocker _{this};

  Mmsg(cmd,
       "SELECT Path.Path||Name AS Filename "
       "FROM (SELECT PathId, Name FROM File WHERE JobId=%s "
       ") AS F, Path "
       "WHERE Path.PathId=F.PathId ",
       edit_int64(jobid, ed1));

  sendit->ArrayStart("filenames");
  if (!BigSqlQuery(cmd, ::ListResult, &lctx)) { return; }
  sendit->ArrayEnd("filenames");

  SqlFreeResult();
}

void BareosDb::ListFilesets(JobControlRecord* jcr,
                            JobDbRecord* jr,
                            const char* range,
                            OutputFormatter* sendit,
                            e_list_type type)
{
  char esc[MAX_ESCAPE_NAME_LENGTH];

  DbLocker _{this};
  if (jr->Name[0] != 0) {
    EscapeString(jcr, esc, jr->Name, strlen(jr->Name));
    Mmsg(cmd,
         "SELECT DISTINCT FileSet.FileSetId AS FileSetId, FileSet, MD5, "
         "CreateTime, FileSetText "
         "FROM Job, FileSet "
         "WHERE Job.FileSetId = FileSet.FileSetId "
         "AND Job.Name='%s' %s",
         esc, range);
  } else if (jr->Job[0] != 0) {
    EscapeString(jcr, esc, jr->Job, strlen(jr->Job));
    Mmsg(cmd,
         "SELECT DISTINCT FileSet.FileSetId AS FileSetId, FileSet, MD5, "
         "CreateTime, FileSetText "
         "FROM Job, FileSet "
         "WHERE Job.FileSetId = FileSet.FileSetId "
         "AND Job.Name='%s' %s",
         esc, range);
  } else if (jr->JobId != 0) {
    Mmsg(cmd,
         "SELECT DISTINCT FileSet.FileSetId AS FileSetId, FileSet, MD5, "
         "CreateTime, FileSetText "
         "FROM Job, FileSet "
         "WHERE Job.FileSetId = FileSet.FileSetId "
         "AND Job.JobId='%s' %s",
         edit_int64(jr->JobId, esc), range);
  } else if (jr->FileSetId != 0) {
    Mmsg(cmd,
         "SELECT FileSetId, FileSet, MD5, CreateTime, FileSetText "
         "FROM FileSet "
         "WHERE FileSetId=%s ",
         edit_int64(jr->FileSetId, esc));
  } else { /* all records */
    Mmsg(cmd,
         "SELECT DISTINCT FileSet.FileSetId AS FileSetId, FileSet, MD5, "
         "CreateTime, FileSetText "
         "FROM FileSet ORDER BY FileSetId ASC %s",
         range);
  }

  if (!QueryDb(jcr, cmd)) { return; }
  sendit->ArrayStart("filesets");
  ListResult(jcr, sendit, type);
  sendit->ArrayEnd("filesets");

  SqlFreeResult();
}
#endif /* HAVE_POSTGRESQL */
