/* BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2001-2012 Free Software Foundation Europe e.V.
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
// Kern Sibbald, December MMI
/**
 * @file
 * BAREOS Director -- Run Command
 */
#include "dird/dird_globals.h"
#include "include/auth_protocol_types.h"
#include "include/bareos.h"
#include "include/job_types.h"
#include "dird.h"
#include "dird/backup.h"
#include "dird/director_jcr_impl.h"
#include "dird/fd_cmds.h"
#include "dird/job.h"
#include "dird/migration.h"
#include "dird/msgchan.h"
#include "dird/restore_plugin_hints.h"
#include "dird/storage.h"
#include "dird/ua_db.h"
#include "dird/ua_input.h"
#include "dird/ua_select.h"
#include "dird/ua_tree_browser.h"
#include "dird/ua_tree_browser_internal.h"
#include "dird/ua_run.h"
#include "lib/bnet.h"
#include "lib/bool_string.h"
#include "lib/breg.h"
#include "lib/berrno.h"
#include "lib/edit.h"
#include "lib/keyword_table_s.h"
#include "lib/util.h"
#include "dird/jcr_util.h"

#include <algorithm>
#include <string_view>
#include <vector>

namespace directordaemon {

using tree_browser_internal::FitText;
using tree_browser_internal::FrameBorderStyle;
using tree_browser_internal::RenderFrameBorder;
using tree_browser_internal::StyleFrameContent;

/* Forward referenced subroutines */
static void SelectJobLevel(UaContext* ua, JobControlRecord* jcr);
static bool DisplayJobParameters(UaContext* ua,
                                 JobControlRecord* jcr,
                                 RunContext& rc);
static int ModifyRestoreParameters(UaContext* ua,
                                   JobControlRecord* jcr,
                                   RunContext& rc);
static void SelectWhereRegexp(UaContext* ua, JobControlRecord* jcr);
static bool GetPluginOptions(UaContext* ua, JobControlRecord* jcr);
static bool ScanCommandLineArguments(UaContext* ua, RunContext& rc);
static bool ResetRestoreContext(UaContext* ua,
                                JobControlRecord* jcr,
                                RunContext& rc);
static int ModifyJobParameters(UaContext* ua,
                               JobControlRecord* jcr,
                               RunContext& rc);

/* Imported variables */
extern struct s_kw ReplaceOptions[];

/**
 * Rerun a job by jobid. Lookup the job data and rerun the job with that data.
 *
 * Returns: false on error
 *          true if OK
 */
static inline bool reRunJob(UaContext* ua, JobId_t JobId, bool yes, utime_t now)
{
  JobDbRecord jr;
  char dt[MAX_TIME_LENGTH];
  PoolMem cmdline(PM_MESSAGE);

  jr.JobId = JobId;
  if (DbLocker _{ua->db}; !ua->db->GetJobRecord(ua->jcr, &jr)) {
    Jmsg(ua->jcr, M_WARNING, 0,
         T_("Error getting Job record for Job rerun: ERR=%s\n"),
         ua->db->strerror());
    return false;
  }

  /* Only perform rerun on JobTypes where it makes sense. Skipping is not an
   * error, so that a rerun over a selection of failed jobs continues with the
   * remaining jobids. */
  if (!IsRerunableJobType(static_cast<JobTypes>(jr.JobType))) {
    ua->SendMsg(T_("Cannot rerun jobid %" PRIu32 ": %s jobs cannot be "
                   "rerun.\n"),
                jr.JobId, job_type_to_str(jr.JobType));
    return true;
  }

  ua->SendMsg("rerunning jobid %" PRIu32 "\n", jr.JobId);

  if (jr.JobLevel == L_NONE) {
    Mmsg(cmdline, "run job=\"%s\"", jr.Name);
  } else {
    Mmsg(cmdline, "run job=\"%s\" level=\"%s\"", jr.Name,
         JobLevelToString(jr.JobLevel));
  }
  PmStrcpy(ua->cmd, cmdline);

  if (jr.ClientId) {
    ClientDbRecord cr;

    cr.ClientId = jr.ClientId;
    if (DbLocker _{ua->db}; !ua->db->GetClientRecord(ua->jcr, &cr)) {
      Jmsg(ua->jcr, M_WARNING, 0,
           T_("Error getting Client record for Job rerun: ERR=%s\n"),
           ua->db->strerror());
      return false;
    }
    Mmsg(cmdline, " client=\"%s\"", cr.Name);
    PmStrcat(ua->cmd, cmdline);
  }

  if (jr.PoolId) {
    PoolDbRecord pr;

    pr.PoolId = jr.PoolId;
    if (DbLocker _{ua->db}; !ua->db->GetPoolRecord(ua->jcr, &pr)) {
      Jmsg(ua->jcr, M_WARNING, 0,
           T_("Error getting Pool record for Job rerun: ERR=%s\n"),
           ua->db->strerror());
      return false;
    }

    // Source pool.
    switch (jr.JobType) {
      case JT_COPY:
      case JT_MIGRATE: {
        JobResource* job = NULL;

        job = ua->GetJobResWithName(jr.Name, false);
        if (job) {
          Mmsg(cmdline, " pool=\"%s\"", job->pool->resource_name_);
          PmStrcat(ua->cmd, cmdline);
        }
        break;
      }
      case JT_BACKUP:
        switch (jr.JobLevel) {
          case L_VIRTUAL_FULL: {
            JobResource* job = NULL;

            job = ua->GetJobResWithName(jr.Name, false);
            if (job) {
              Mmsg(cmdline, " pool=\"%s\"", job->pool->resource_name_);
              PmStrcat(ua->cmd, cmdline);
            }
            break;
          }
          default:
            Mmsg(cmdline, " pool=\"%s\"", pr.Name);
            PmStrcat(ua->cmd, cmdline);
            break;
        }
    }

    // Next pool.
    switch (jr.JobType) {
      case JT_COPY:
      case JT_MIGRATE:
        Mmsg(cmdline, " nextpool=\"%s\"", pr.Name);
        PmStrcat(ua->cmd, cmdline);
        break;
      case JT_BACKUP:
        switch (jr.JobLevel) {
          case L_VIRTUAL_FULL:
            Mmsg(cmdline, " nextpool=\"%s\"", pr.Name);
            PmStrcat(ua->cmd, cmdline);

            break;
          default:
            break;
        }
        break;
    }
  }

  if (jr.FileSetId) {
    FileSetDbRecord fs;

    fs.FileSetId = jr.FileSetId;
    if (DbLocker _{ua->db}; !ua->db->GetFilesetRecord(ua->jcr, &fs)) {
      Jmsg(ua->jcr, M_WARNING, 0,
           T_("Error getting FileSet record for Job rerun: ERR=%s\n"),
           ua->db->strerror());
      return false;
    }
    Mmsg(cmdline, " fileset=\"%s\"", fs.FileSet);
    PmStrcat(ua->cmd, cmdline);
  }

  bstrutime(dt, sizeof(dt), now);
  Mmsg(cmdline, " when=\"%s\"", dt);
  PmStrcat(ua->cmd, cmdline);

  if (yes) { PmStrcat(ua->cmd, " yes"); }

  Dmsg1(100, "rerun cmdline=%s\n", ua->cmd);

  ParseUaArgs(ua);
  return RunCmd(ua, ua->cmd);
}

RerunArguments GetRerunCmdlineArguments(UaContext* ua)
{
  RerunArguments rerunarguments{};
  // Determine what cmdline arguments are given.

  int s = FindArgWithValue(ua, NT_("since_jobid"));
  if (s > 0) { rerunarguments.since_jobid = str_to_int64(ua->argv[s]); }

  int u = FindArgWithValue(ua, NT_("until_jobid"));
  if (u > 0) { rerunarguments.until_jobid = str_to_int64(ua->argv[u]); }

  int d = FindArgWithValue(ua, NT_("days"));
  if (d > 0) { rerunarguments.days = str_to_int64(ua->argv[d]); }

  int h = FindArgWithValue(ua, NT_("hours"));
  if (h > 0) { rerunarguments.hours = str_to_int64(ua->argv[h]); }

  if (FindArg(ua, NT_("yes")) > 0) { rerunarguments.yes = true; }

  int j = FindArgWithValue(ua, NT_("jobid"));
  if (j < 0 && !(rerunarguments.days || rerunarguments.hours)
      && !rerunarguments.since_jobid) {
    ua->SendMsg("Please specify jobid, since_jobid, hours or days\n");
    rerunarguments.parsingerror = true;
    return rerunarguments;
  }
  if (j >= 0) {
    char delimiter = ',';
    std::vector<std::string> parsed_jobids
        = split_string(ua->argv[j], delimiter);
    for (const auto& jobid : parsed_jobids) {
      if (Is_a_number(jobid.c_str())) {
        rerunarguments.JobIds.push_back(str_to_int64(jobid.c_str()));
      } else {
        ua->SendMsg("Specified jobid \"%s\" is not valid\n", jobid.c_str());
        rerunarguments.JobIds.clear();
        rerunarguments.parsingerror = true;
        return rerunarguments;
      }
    }
  }
  if (j >= 0 && rerunarguments.since_jobid) {
    ua->SendMsg(
        "Please specify either jobid or since_jobid (and optionally "
        "until_jobid)\n");
    rerunarguments.parsingerror = true;
    return rerunarguments;
  }

  if (j >= 0 && (rerunarguments.days || rerunarguments.hours)) {
    ua->SendMsg("Please specify either jobid or a timeframe\n");
    rerunarguments.parsingerror = true;
    return rerunarguments;
  }

  return rerunarguments;
}

static time_t ConvertDaysHoursToSecs(const RerunArguments& rerunargs,
                                     utime_t now)
{
  const int secs_in_day = 86400;
  const int secs_in_hour = 3600;
  time_t schedtime
      = now - (rerunargs.days * secs_in_day + rerunargs.hours * secs_in_hour);
  return schedtime;
}

static std::string PrepareRerunSqlQuery(UaContext*,
                                        const RerunArguments& rerunargs,
                                        utime_t now)
{
  char dt[MAX_TIME_LENGTH];
  time_t schedtime = ConvertDaysHoursToSecs(rerunargs, now);
  bstrutime(dt, sizeof(dt), schedtime);

  std::string select{"SELECT JobId FROM Job WHERE JobStatus = 'f'"};
  if (rerunargs.since_jobid) {
    if (rerunargs.until_jobid) {
      select += " AND JobId >= " + std::to_string(rerunargs.since_jobid)
                + " AND JobId <= " + std::to_string(rerunargs.until_jobid);
    } else {
      select += " AND JobId >= " + std::to_string(rerunargs.since_jobid);
    }

  } else {
    select += " AND SchedTime > '" + std::string(dt) + "'";
  }
  select += " ORDER BY JobId";

  return select;
}

/**
 * Rerun a job selection.
 *
 * Returns: 0 on error
 *          1 if OK
 */
bool reRunCmd(UaContext* ua, const char*)
{
  if (!OpenClientDb(ua)) { return true; }
  // Determine what cmdline arguments are given.

  RerunArguments rerunargs = GetRerunCmdlineArguments(ua);

  if (rerunargs.parsingerror) { return false; }

  utime_t now = (utime_t)time(NULL);
  if ((rerunargs.days || rerunargs.hours) || rerunargs.since_jobid) {
    std::string select = PrepareRerunSqlQuery(ua, rerunargs, now);

    dbid_list ids;
    PoolMem query(PM_MESSAGE);
    PmStrcpy(query, select.c_str());
    ua->db->GetQueryDbids(ua->jcr, query, ids);

    if (!ids.size()) {
      ua->SendMsg("No jobids with the specified options were found.\n");
    } else {
      ua->SendMsg("The following ids were selected for rerun:\n");
      for (int i = 0; i < ids.num_ids; i++) {
        if (i > 0) {
          ua->SendMsg(",%" PRIdbid, ids.DBId[i]);
        } else {
          ua->SendMsg("%" PRIdbid, ids.DBId[i]);
        }
      }
      ua->SendMsg("\n");

      if (!rerunargs.yes
          && (!GetYesno(ua, T_("rerun these jobids? (yes/no): "))
              || !ua->pint32_val)) {
        return false;
      }
      // Loop over all selected JobIds.
      for (int i = 0; i < ids.num_ids; i++) {
        rerunargs.JobIds.push_back(ids.DBId[i]);
      }
      for (const auto& jobid : rerunargs.JobIds) {
        if (!reRunJob(ua, jobid, rerunargs.yes, now)) { return false; }
      }
    }
  } else {
    for (const auto& jobid : rerunargs.JobIds) {
      if (!reRunJob(ua, jobid, rerunargs.yes, now)) { return false; }
    }
  }

  return true;
}

/**
 * For Backup and Verify Jobs
 *     run [job=]<job-name> level=<level-name>
 *
 *  Jobs
 *     run <job-name>
 *
 * Returns: 0 on error
 *          JobId if OK
 *
 */
int DoRunCmd(UaContext* ua, const char*)
{
  RunContext rc;
  int status, length;
  bool valid_response;
  bool do_pool_overrides = true;
  bool restore_options_editor_shown = false;

  auto used_config = my_config->GetCurrentConfiguration();

  if (!OpenClientDb(ua)) { return 0; }

  if (!ScanCommandLineArguments(ua, rc)) { return 0; }

  /* Create JobControlRecord to run job.  NOTE!!! after this point, FreeJcr()
   * before returning. */
  auto* jcr = NewDirectorJcr(used_config);
  SetJcrDefaults(jcr, rc.job);
  jcr->dir_impl->unlink_bsr
      = ua->jcr->dir_impl->unlink_bsr; /* copy unlink flag from caller */
  ua->jcr->dir_impl->unlink_bsr = false;

  // Transfer JobIds to new restore Job
  if (ua->jcr->JobIds) {
    jcr->JobIds = ua->jcr->JobIds;
    ua->jcr->JobIds = NULL;
  }

  // Transfer selected restore tree to new restore Job
  if (ua->jcr->dir_impl->restore_tree_root) {
    jcr->dir_impl->restore_tree_root = ua->jcr->dir_impl->restore_tree_root;
    ua->jcr->dir_impl->restore_tree_root = NULL;
  }

try_again:
  if (!ResetRestoreContext(ua, jcr, rc)) { goto bail_out; }

  // Run without prompting?
  if (ua->batch || FindArg(ua, NT_("yes")) > 0) { goto start_job; }

  /* When doing interactive runs perform the pool level overrides
   * early this way the user doesn't get nasty surprisses when
   * a level override changes the pool the data will be saved to
   * later. We only want to do these overrides once so we use
   * a tracking boolean do_pool_overrides to see if we still
   * need to do this (e.g. we pass by here multiple times when
   * the user interactively changes options. */
  if (do_pool_overrides) {
    switch (jcr->getJobType()) {
      case JT_BACKUP:
        if (!jcr->is_JobLevel(L_VIRTUAL_FULL)) { ApplyPoolOverrides(jcr); }
        break;
      default:
        break;
    }
    do_pool_overrides = false;
  }

  /* Prompt User to see if all run job parameters are correct, and
   * allow him to modify them. */
  if (!restore_options_editor_shown && jcr->is_JobType(JT_RESTORE)
      && TreeBrowserSupported(ua)) {
    restore_options_editor_shown = true;
    status = ModifyRestoreParameters(ua, jcr, rc);
    switch (status) {
      case 0:
        restore_options_editor_shown = false;
        goto try_again;
      case 1:
        break;
      case -1:
        goto bail_out;
    }
  }

  if (!DisplayJobParameters(ua, jcr, rc)) { goto bail_out; }

  // Prompt User until we have a valid response.
  do {
    if (!GetCmd(ua, T_("OK to run? (yes/mod/no): "))) { goto bail_out; }

    /* Empty line equals yes, anything other we compare
     * the cmdline for the length of the given input unless
     * its mod or .mod where we compare only the keyword
     * and a space as it can be followed by a full cmdline
     * with new cmdline arguments that need to be parsed. */
    valid_response = false;
    length = strlen(ua->cmd);
    if (ua->cmd[0] == 0 || bstrncasecmp(ua->cmd, ".mod ", MIN(length, 5))
        || bstrncasecmp(ua->cmd, "mod ", MIN(length, 4))
        || bstrncasecmp(ua->cmd, NT_("yes"), length)
        || bstrncasecmp(ua->cmd, T_("yes"), length)
        || bstrncasecmp(ua->cmd, NT_("no"), length)
        || bstrncasecmp(ua->cmd, T_("no"), length)) {
      valid_response = true;
    }

    if (!valid_response) {
      ua->WarningMsg(T_("Illegal response %s\n"), ua->cmd);
    }
  } while (!valid_response);

  // See if the .mod or mod has arguments.
  if (bstrncasecmp(ua->cmd, ".mod ", 5)
      || (bstrncasecmp(ua->cmd, "mod ", 4) && strlen(ua->cmd) > 6)) {
    ParseUaArgs(ua);
    rc.mod = true;
    if (!ScanCommandLineArguments(ua, rc)) { return 0; }
    goto try_again;
  }

  // Allow the user to modify the settings
  status = ModifyJobParameters(ua, jcr, rc);
  switch (status) {
    case 0:
      if (jcr->is_JobType(JT_RESTORE) && TreeBrowserSupported(ua)) {
        restore_options_editor_shown = false;
      }
      goto try_again;
    case 1:
      break;
    case -1:
      goto bail_out;
  }

  /* For interactive runs we set IgnoreLevelPoolOverrides as we already
   * performed the actual overrides. */
  jcr->dir_impl->IgnoreLevelPoolOverrides = true;

  if (ua->cmd[0] == 0 || bstrncasecmp(ua->cmd, NT_("yes"), strlen(ua->cmd))
      || bstrncasecmp(ua->cmd, T_("yes"), strlen(ua->cmd))) {
    JobId_t JobId;
    Dmsg1(800, "Calling RunJob job=%p\n", jcr->dir_impl->res.job);

  start_job:
    Dmsg3(100, "JobId=%" PRIu32 " using pool %s priority=%d\n", jcr->JobId,
          jcr->dir_impl->res.pool->resource_name_, jcr->JobPriority);
    Dmsg1(900, "Running a job; its spool_data = %d\n",
          jcr->dir_impl->spool_data);

    JobId = RunJob(jcr);

    Dmsg4(100,
          "JobId=%" PRIu32 " NewJobId=%" PRIu32 " using pool %s priority=%d\n",
          jcr->JobId, JobId, jcr->dir_impl->res.pool->resource_name_,
          jcr->JobPriority);

    jcr->dir_impl->job_trigger = JobTrigger::kUser;

    // For interactive runs we send a message to the audit log
    if (jcr->dir_impl->IgnoreLevelPoolOverrides) {
      char buf[50];
      ua->LogAuditEventInfoMsg(T_("Job queued. JobId=%s"),
                               edit_int64(jcr->JobId, buf));
    }

    FreeJcr(jcr); /* release jcr */

    if (JobId == 0) {
      ua->ErrorMsg(T_("Job failed.\n"));
    } else {
      char ed1[50];
      ua->send->ObjectStart("run");
      ua->send->ObjectKeyValue("jobid", edit_int64(JobId, ed1),
                               T_("Job queued. JobId=%s\n"));
      ua->send->ObjectEnd("run");
    }

    return JobId;
  }

bail_out:
  ua->SendMsg(T_("Job not run.\n"));
  FreeJcr(jcr);

  return 0; /* do not run */
}

bool RunCmd(UaContext* ua, const char*) { return (DoRunCmd(ua, ua->cmd) != 0); }

int ModifyJobParameters(UaContext* ua, JobControlRecord* jcr, RunContext& rc)
{
  int opt;

  // At user request modify parameters of job to be run.
  if (ua->cmd[0] != 0 && bstrncasecmp(ua->cmd, T_("mod"), strlen(ua->cmd))) {
    if (jcr->is_JobType(JT_RESTORE)) {
      return ModifyRestoreParameters(ua, jcr, rc);
    }

    StartPrompt(ua, T_("Parameters to modify:\n"));

    AddPrompt(ua, T_("Level"));   /* 0 */
    AddPrompt(ua, T_("Storage")); /* 1 */
    AddPrompt(ua, T_("Job"));     /* 2 */
    AddPrompt(ua, T_("FileSet")); /* 3 */
    if (jcr->is_JobType(JT_RESTORE)) {
      AddPrompt(ua, T_("Restore Client")); /* 4 */
    } else {
      AddPrompt(ua, T_("Client")); /* 4 */
    }
    AddPrompt(ua, T_("Backup Format")); /* 5 */
    AddPrompt(ua, T_("When"));          /* 6 */
    AddPrompt(ua, T_("Priority"));      /* 7 */
    if (jcr->is_JobType(JT_BACKUP) || jcr->is_JobType(JT_COPY)
        || jcr->is_JobType(JT_MIGRATE) || jcr->is_JobType(JT_VERIFY)) {
      AddPrompt(ua, T_("Pool")); /* 8 */
      if (jcr->is_JobType(JT_MIGRATE) || jcr->is_JobType(JT_COPY)
          || (jcr->is_JobType(JT_BACKUP)
              && jcr->is_JobLevel(L_VIRTUAL_FULL))) { /* NextPool */
        AddPrompt(ua, T_("NextPool"));                /* 9 */
        if (jcr->is_JobType(JT_BACKUP)) {
          AddPrompt(ua, T_("Plugin Options")); /* 10 */
        }
      } else if (jcr->is_JobType(JT_VERIFY)) {
        AddPrompt(ua, T_("Verify Job")); /* 9 */
      } else if (jcr->is_JobType(JT_BACKUP)) {
        AddPrompt(ua, T_("Plugin Options")); /* 9 */
      }
    } else if (jcr->is_JobType(JT_RESTORE)) {
      AddPrompt(ua, T_("Bootstrap"));       /* 8 */
      AddPrompt(ua, T_("Where"));           /* 9 */
      AddPrompt(ua, T_("File Relocation")); /* 10 */
      AddPrompt(ua, T_("Replace"));         /* 11 */
      AddPrompt(ua, T_("JobId"));           /* 12 */
      AddPrompt(ua, T_("Plugin Options"));  /* 13 */
    }

    switch (DoPrompt(ua, "", T_("Select parameter to modify"), NULL, 0)) {
      case 0:
        /* Level */
        SelectJobLevel(ua, jcr);
        switch (jcr->getJobType()) {
          case JT_BACKUP:
            if (!rc.pool_override && !jcr->is_JobLevel(L_VIRTUAL_FULL)) {
              ApplyPoolOverrides(jcr, true);
              rc.pool = jcr->dir_impl->res.pool;
              rc.level_override = true;
            }
            break;
          default:
            break;
        }
        goto try_again;
      case 1:
        /* Storage */
        rc.store->store = select_storage_resource(ua);
        if (rc.store->store) {
          PmStrcpy(rc.store->store_source, T_("user selection"));
          SetRwstorage(jcr, rc.store);
          goto try_again;
        }
        break;
      case 2:
        /* Job */
        rc.job = select_job_resource(ua);
        if (rc.job) {
          jcr->dir_impl->res.job = rc.job;
          SetJcrDefaults(jcr, rc.job);
          goto try_again;
        }
        break;
      case 3:
        /* FileSet */
        rc.fileset = select_fileset_resource(ua);
        if (rc.fileset) {
          jcr->dir_impl->res.fileset = rc.fileset;
          goto try_again;
        }
        break;
      case 4:
        /* Client */
        rc.client = select_client_resource(ua);
        if (rc.client) {
          jcr->dir_impl->res.client = rc.client;
          goto try_again;
        }
        break;
      case 5:
        /* Backup Format */
        if (GetCmd(ua, T_("Please enter Backup Format: "))) {
          if (jcr->dir_impl->backup_format) {
            free(jcr->dir_impl->backup_format);
            jcr->dir_impl->backup_format = NULL;
          }
          jcr->dir_impl->backup_format = strdup(ua->cmd);
          goto try_again;
        }
        break;
      case 6:
        /* When */
        for (;;) {
          if (!GetCmd(ua, T_("Please enter desired start time as YYYY-MM-DD "
                             "HH:MM:SS (return for now): "))) {
            break;
          }
          if (ua->cmd[0] == 0) {
            jcr->sched_time = time(NULL);
          } else {
            auto parsed = StrToUtime(ua->cmd);
            if (parsed == 0) {
              ua->SendMsg(T_("Invalid time specification.\n"));
              continue;
            } else {
              jcr->sched_time = parsed;
            }
          }
          goto try_again;
        }
        break;
      case 7:
        /* Priority */
        if (GetPint(ua, T_("Enter new Priority: "))) {
          if (!ua->pint32_val) {
            ua->SendMsg(T_("Priority must be a positive integer.\n"));
          } else {
            jcr->JobPriority = ua->pint32_val;
          }
          goto try_again;
        }
        break;
      case 8:
        /* Pool or Bootstrap depending on JobType */
        if (jcr->is_JobType(JT_BACKUP) || jcr->is_JobType(JT_COPY)
            || jcr->is_JobType(JT_MIGRATE)
            || jcr->is_JobType(JT_VERIFY)) { /* Pool */
          rc.pool = select_pool_resource(ua);
          if (rc.pool) {
            jcr->dir_impl->res.pool = rc.pool;
            rc.level_override = false;
            rc.pool_override = true;
            Dmsg1(100, "Set new pool=%s\n",
                  jcr->dir_impl->res.pool->resource_name_);
            goto try_again;
          }
        } else {
          /* Bootstrap */
          if (GetCmd(ua, T_("Please enter the Bootstrap file name: "))) {
            if (jcr->RestoreBootstrap) {
              free(jcr->RestoreBootstrap);
              jcr->RestoreBootstrap = NULL;
            }
            if (ua->cmd[0] != 0) {
              FILE* fd;

              jcr->RestoreBootstrap = strdup(ua->cmd);
              fd = fopen(jcr->RestoreBootstrap, "rb");
              if (!fd) {
                BErrNo be;
                ua->SendMsg(T_("Warning cannot open %s: ERR=%s\n"),
                            jcr->RestoreBootstrap, be.bstrerror());
                free(jcr->RestoreBootstrap);
                jcr->RestoreBootstrap = NULL;
              } else {
                fclose(fd);
              }
            }
            goto try_again;
          }
        }
        break;
      case 9:
        /* NextPool/Verify Job/Where/Plugin Options depending on JobType */
        if (jcr->is_JobType(JT_MIGRATE) || jcr->is_JobType(JT_COPY)
            || (jcr->is_JobType(JT_BACKUP)
                && jcr->is_JobLevel(L_VIRTUAL_FULL))) { /* NextPool */
          rc.next_pool = select_pool_resource(ua);
          if (rc.next_pool) {
            jcr->dir_impl->res.next_pool = rc.next_pool;
            Dmsg1(100, "Set new next_pool=%s\n",
                  jcr->dir_impl->res.next_pool->resource_name_);
            goto try_again;
          }
        } else if (jcr->is_JobType(JT_VERIFY)) { /* Verify Job */
          rc.verify_job = select_job_resource(ua);
          if (rc.verify_job) { jcr->dir_impl->res.verify_job = rc.verify_job; }
          goto try_again;
        } else if (jcr->is_JobType(JT_RESTORE)) { /* Where */
          if (GetCmd(ua, T_("Please enter the full path prefix for restore (/ "
                            "for none): "))) {
            if (!ua->AclAccessOk(Where_ACL, ua->cmd, true)) {
              ua->SendMsg(
                  T_("No authorization for \"where\" specification.\n"));
            } else {
              if (jcr->RegexWhere) { /* cannot use regexwhere and where */
                free(jcr->RegexWhere);
                jcr->RegexWhere = NULL;
              }
              if (jcr->where) {
                free(jcr->where);
                jcr->where = NULL;
              }
              // "/" is treated as no prefix.
              if (IsPathSeparator(ua->cmd[0]) && ua->cmd[1] == '\0') {
                ua->cmd[0] = 0;
              }
              jcr->where = strdup(ua->cmd);
            }
            goto try_again;
          }
        } else { /* Plugin Options */
          GetPluginOptions(ua, jcr);
          goto try_again;
        }
        break;
      case 10:
        /* File relocation/Plugin Options depending on JobType */
        if (jcr->is_JobType(JT_RESTORE)) {
          SelectWhereRegexp(ua, jcr);
          goto try_again;
        } else if (jcr->is_JobType(JT_BACKUP)) {
          GetPluginOptions(ua, jcr);
          goto try_again;
        }
        break;
      case 11:
        /* Replace */
        StartPrompt(ua, T_("Replace:\n"));
        for (int i = 0; ReplaceOptions[i].name; i++) {
          AddPrompt(ua, ReplaceOptions[i].name);
        }
        opt = DoPrompt(ua, "", T_("Select replace option"), NULL, 0);
        if (opt >= 0) {
          rc.replace = ReplaceOptions[opt].name;
          jcr->dir_impl->replace = ReplaceOptions[opt].token;
        }
        goto try_again;
      case 12:
        /* JobId */
        rc.jid = NULL; /* force reprompt */
        jcr->dir_impl->RestoreJobId = 0;
        if (jcr->RestoreBootstrap) {
          ua->SendMsg(T_(
              "You must set the bootstrap file to NULL to be able to specify "
              "a JobId.\n"));
        }
        goto try_again;
      case 13:
        /* Plugin Options */
        GetPluginOptions(ua, jcr);
        goto try_again;
        break;
      case -1: /* error or cancel */
        return -1;
      default:
        goto try_again;
    }
    return -1;
  }
  return 1;

try_again:
  return 0;
}

static const char* RestoreReplacePromptLabel(const char* replace)
{
  if (Bstrcasecmp(replace, "Always")) {
    return T_("Always - overwrite existing files (Bareos value: Always)");
  }
  if (Bstrcasecmp(replace, "IfNewer")) {
    return T_("Only overwrite older files (Bareos value: IfNewer)");
  }
  if (Bstrcasecmp(replace, "IfOlder")) {
    return T_("Only overwrite newer files (Bareos value: IfOlder)");
  }
  if (Bstrcasecmp(replace, "Never")) {
    return T_("Never overwrite existing files (Bareos value: Never)");
  }
  return replace;
}

enum class RestoreOptionAction
{
  kContinue = 0,
  kBack,
  kRestoreClient,
  kBrowseWhere,
  kWhere,
  kRelocation,
  kReplace,
  kPluginOptions,
  kAdvancedMenu,
  kWhen,
  kPriority,
  kBootstrap,
  kStorage,
  kJobId,
  kRestoreJob,
};

enum class DestinationBrowseAction
{
  kUseCurrentDirectory,
  kParentDirectory,
  kOpenDirectory,
  kFile,
};

enum class DestinationBrowseMode
{
  kBrowsing,
  kEnteringSearch,
};

struct RestoreOptionRow {
  RestoreOptionAction action;
  const char* label;
  std::string value;
  bool advanced = false;
};

struct DestinationBrowseEntry {
  DestinationBrowseAction action;
  std::string label;
  std::string path;
  bool directory = false;
};

static std::string FitRestoreOptionText(std::string_view text, size_t width)
{
  if (text.size() <= width) { return std::string(text); }
  if (width <= 3) { return std::string(text.substr(0, width)); }
  std::string result(text.substr(0, width - 3));
  result += "...";
  return result;
}

static std::string RestoreOptionsFrameBorder(size_t width,
                                             bool color,
                                             FrameBorderStyle style,
                                             std::string_view title = {});

static std::string RestoreOptionsFrameLine(size_t width,
                                           std::string_view text,
                                           bool color,
                                           bool highlighted = false);

static size_t RestoreOptionsScreenWidth(UaContext* ua)
{
  constexpr size_t kDefaultTerminalWidth = 80;
  constexpr size_t kMinTerminalWidth = 40;

  if (ua->terminal_width <= 1) { return kDefaultTerminalWidth; }
  return std::max(kMinTerminalWidth,
                  static_cast<size_t>(ua->terminal_width - 1));
}

static void RemoveFinalNewline(std::string* text)
{
  if (!text->empty() && text->back() == '\n') { text->pop_back(); }
}

static std::vector<RestoreOptionRow> BuildRestoreOptionRows(
    JobControlRecord* jcr,
    RunContext& rc)
{
  std::vector<RestoreOptionRow> rows;
  rows.push_back({RestoreOptionAction::kContinue,
                  T_("Continue to restore summary"),
                  T_("review and confirm the restore job")});
  rows.push_back({RestoreOptionAction::kRestoreClient, T_("Restore Client"),
                  jcr->dir_impl->res.client
                      ? jcr->dir_impl->res.client->resource_name_
                      : T_("*None*")});
  rows.push_back({RestoreOptionAction::kBrowseWhere,
                  T_("Browse destination client"),
                  T_("open target client filesystem browser"), false});
  rows.push_back({RestoreOptionAction::kWhere, T_("Where"),
                  jcr->where ? jcr->where : NPRT(rc.job->RestoreWhere)});
  rows.push_back({RestoreOptionAction::kRelocation, T_("File Relocation"),
                  jcr->RegexWhere ? jcr->RegexWhere : T_("not configured"),
                  false});
  rows.push_back(
      {RestoreOptionAction::kReplace, T_("Replace Policy"), rc.replace});
  rows.push_back({RestoreOptionAction::kPluginOptions, T_("Plugin Options"),
                  jcr->dir_impl->plugin_options ? jcr->dir_impl->plugin_options
                                                : T_("not configured")});
  rows.push_back({RestoreOptionAction::kAdvancedMenu, T_("Advanced Options"),
                  T_("press Enter to show advanced restore options"), false});
  return rows;
}

static bool IsListingTimestamp(std::string_view text, size_t pos)
{
  if (pos + 19 > text.size()) { return false; }
  const auto digit = [](char ch) { return ch >= '0' && ch <= '9'; };
  return digit(text[pos]) && digit(text[pos + 1]) && digit(text[pos + 2])
         && digit(text[pos + 3]) && text[pos + 4] == '-' && digit(text[pos + 5])
         && digit(text[pos + 6]) && text[pos + 7] == '-' && digit(text[pos + 8])
         && digit(text[pos + 9]) && text[pos + 10] == ' '
         && digit(text[pos + 11]) && digit(text[pos + 12])
         && text[pos + 13] == ':' && digit(text[pos + 14])
         && digit(text[pos + 15]) && text[pos + 16] == ':'
         && digit(text[pos + 17]) && digit(text[pos + 18]);
}

static bool ParseEstimateListingLine(std::string_view line,
                                     std::string* path,
                                     bool* directory)
{
  if (line.empty()) { return false; }
  switch (line.front()) {
    case 'd':
      *directory = true;
      break;
    case '-':
    case 'l':
    case 'b':
    case 'c':
    case 'f':
    case 's':
      *directory = false;
      break;
    default:
      return false;
  }

  size_t timestamp = std::string_view::npos;
  for (size_t i = 0; i + 19 <= line.size(); ++i) {
    if (IsListingTimestamp(line, i)) {
      timestamp = i;
      break;
    }
  }
  if (timestamp == std::string_view::npos) { return false; }

  size_t filename = timestamp + 19;
  while (filename < line.size() && line[filename] == ' ') { ++filename; }
  if (filename >= line.size()) { return false; }

  std::string parsed(line.substr(filename));
  size_t link_separator = parsed.find(" -> ");
  if (link_separator != std::string::npos) { parsed.resize(link_separator); }
  *path = std::move(parsed);
  return !path->empty();
}

static std::string NormalizeBrowsePath(std::string_view path)
{
  std::string normalized(path);
  if (normalized.empty()) { return "/"; }
  while (normalized.size() > 1 && IsPathSeparator(normalized.back())) {
    normalized.pop_back();
  }
  return normalized.empty() ? "/" : normalized;
}

static std::string ParentBrowsePath(std::string_view path)
{
  std::string normalized = NormalizeBrowsePath(path);
  if (normalized == "/") { return normalized; }
  size_t separator = normalized.find_last_of("/\\");
  if (separator == std::string::npos || separator == 0) { return "/"; }
  return normalized.substr(0, separator);
}

static std::string BrowsePathBasename(std::string_view path)
{
  std::string normalized = NormalizeBrowsePath(path);
  if (normalized == "/") { return normalized; }
  size_t separator = normalized.find_last_of("/\\");
  if (separator == std::string::npos) { return normalized; }
  return normalized.substr(separator + 1);
}

static bool IsImmediateBrowseChild(std::string_view parent,
                                   std::string_view child)
{
  std::string normalized_parent = NormalizeBrowsePath(parent);
  if (child == normalized_parent) { return false; }
  if (normalized_parent == "/") {
    if (child.empty() || child.front() != '/') { return false; }
    std::string_view remainder = child.substr(1);
    return !remainder.empty()
           && remainder.find_first_of("/\\") == std::string_view::npos;
  }

  if (child.size() <= normalized_parent.size()
      || child.substr(0, normalized_parent.size()) != normalized_parent
      || !IsPathSeparator(child[normalized_parent.size()])) {
    return false;
  }
  std::string_view remainder = child.substr(normalized_parent.size() + 1);
  return !remainder.empty()
         && remainder.find_first_of("/\\") == std::string_view::npos;
}

static void SortBrowseEntries(std::vector<DestinationBrowseEntry>* entries)
{
  std::stable_sort(
      entries->begin(), entries->end(),
      [](const DestinationBrowseEntry& lhs, const DestinationBrowseEntry& rhs) {
        if (lhs.directory != rhs.directory) {
          return lhs.directory && !rhs.directory;
        }
        return lhs.label < rhs.label;
      });
}

static void RemoveLastBrowseSearchCharacter(std::string* text)
{
  if (text->empty()) { return; }
  size_t pos = text->size() - 1;
  while (pos > 0 && (static_cast<unsigned char>((*text)[pos]) & 0xc0) == 0x80) {
    --pos;
  }
  text->resize(pos);
}

static bool ContainsBrowseSearchMatch(std::string_view text,
                                      std::string_view search)
{
  return search.empty() || text.find(search) != std::string_view::npos;
}

static std::vector<DestinationBrowseEntry> FilterDestinationBrowseEntries(
    const std::vector<DestinationBrowseEntry>& entries,
    std::string_view search)
{
  if (search.empty()) { return entries; }

  std::vector<DestinationBrowseEntry> filtered;
  for (const auto& entry : entries) {
    if (ContainsBrowseSearchMatch(entry.label, search)
        || ContainsBrowseSearchMatch(entry.path, search)) {
      filtered.push_back(entry);
    }
  }
  return filtered;
}

static bool SendBrowseJobInfoToFileDaemon(JobControlRecord* jcr)
{
  BareosSocket* fd = jcr->file_bsock;
  if (jcr->sd_auth_key == NULL) { jcr->sd_auth_key = strdup("dummy"); }

  char ed1[30];
  fd->fsend("JobId=%s Job=%s SDid=%u SDtime=%u Authorization=%s ssl=%u\n",
            edit_int64(jcr->JobId, ed1), jcr->Job, jcr->VolSessionId,
            jcr->VolSessionTime, jcr->sd_auth_key,
            static_cast<unsigned int>(TlsPolicy::kBnetTlsNone));

  if (!jcr->dir_impl->keep_sd_auth_key && !bstrcmp(jcr->sd_auth_key, "dummy")) {
    memset(jcr->sd_auth_key, 0, strlen(jcr->sd_auth_key));
  }

  Dmsg1(100, ">filed: %s", fd->msg);
  if (BgetDirmsg(fd) > 0) {
    Dmsg1(110, "<filed: %s", fd->msg);
    if (!bstrncmp(fd->msg, "2000 OK Job", strlen("2000 OK Job"))) {
      Jmsg(jcr, M_FATAL, 0, T_("File daemon \"%s\" rejected Job command: %s\n"),
           jcr->dir_impl->res.client->resource_name_, fd->msg);
      jcr->setJobStatusWithPriorityCheck(JS_ErrorTerminated);
      return false;
    }
  } else {
    Jmsg(jcr, M_FATAL, 0, T_("FD gave bad response to Job command: %s\n"),
         BnetStrerror(fd));
    jcr->setJobStatusWithPriorityCheck(JS_ErrorTerminated);
    return false;
  }

  return true;
}

static bool ListDestinationClientDirectory(
    UaContext* ua,
    JobControlRecord* jcr,
    ClientResource* client,
    const std::string& path,
    std::vector<DestinationBrowseEntry>* entries)
{
  if (!client) {
    ua->ErrorMsg(T_("No restore client selected.\n"));
    return false;
  }
  if (!ua->AclAccessOk(Where_ACL, path.c_str(), true)) {
    ua->ErrorMsg(T_("No authorization for \"where\" specification.\n"));
    return false;
  }
  if (client->Protocol != APT_NATIVE) {
    ua->ErrorMsg(T_("Browsing is only supported on native clients.\n"));
    return false;
  }
  if (jcr->file_bsock) {
    ua->ErrorMsg(T_("Client connection already active.\n"));
    return false;
  }

  FilesetResource browse_fileset;
  browse_fileset.new_include = true;
  IncludeExcludeItem include_item;
  FileOptions file_options;
  bstrncpy(file_options.opts, "h", sizeof(file_options.opts));
  include_item.file_options_list.push_back(&file_options);
  include_item.name_list.append(strdup(path.c_str()));
  browse_fileset.include_items.push_back(&include_item);

  FilesetResource* saved_fileset = jcr->dir_impl->res.fileset;
  ClientResource* saved_client = jcr->dir_impl->res.client;
  int32_t saved_job_type = jcr->getJobType();
  int32_t saved_job_level = jcr->getJobLevel();
  bool saved_accurate = jcr->accurate;

  auto cleanup = [&]() {
    if (jcr->file_bsock) {
      jcr->file_bsock->signal(BNET_TERMINATE);
      jcr->file_bsock->close();
      delete jcr->file_bsock;
      jcr->file_bsock = NULL;
    }
    jcr->dir_impl->res.fileset = saved_fileset;
    jcr->dir_impl->res.client = saved_client;
    jcr->setJobType(saved_job_type);
    jcr->setJobLevel(saved_job_level);
    jcr->accurate = saved_accurate;
  };

  jcr->dir_impl->res.client = client;
  jcr->dir_impl->res.fileset = &browse_fileset;
  jcr->setJobType(JT_BACKUP);
  jcr->setJobLevel(L_FULL);
  jcr->accurate = false;

  if (!ConnectToFileDaemon(jcr, client, 1, 15, false, nullptr)
      || !SendBrowseJobInfoToFileDaemon(jcr) || !SendLevelCommand(jcr)
      || !SendIncludeExcludeLists(jcr) || !SendAccurateCurrentFiles(jcr)) {
    cleanup();
    ua->ErrorMsg(T_("Unable to browse Client \"%s\".\n"),
                 client->resource_name_);
    return false;
  }

  jcr->file_bsock->fsend("estimate listing=1\n");
  while (jcr->file_bsock->recv() >= 0) {
    std::string_view output(jcr->file_bsock->msg,
                            jcr->file_bsock->message_length);
    size_t start = 0;
    while (start < output.size()) {
      size_t end = output.find('\n', start);
      if (end == std::string_view::npos) { end = output.size(); }
      std::string entry_path;
      bool directory = false;
      if (ParseEstimateListingLine(output.substr(start, end - start),
                                   &entry_path, &directory)
          && IsImmediateBrowseChild(path, entry_path)) {
        std::string label = BrowsePathBasename(entry_path);
        if (directory) { label += "/"; }
        entries->push_back({directory ? DestinationBrowseAction::kOpenDirectory
                                      : DestinationBrowseAction::kFile,
                            std::move(label), std::move(entry_path),
                            directory});
      }
      start = end + 1;
    }
  }

  cleanup();
  SortBrowseEntries(entries);
  return true;
}

static std::vector<DestinationBrowseEntry> BuildDestinationBrowseRows(
    const std::string& path,
    const std::vector<DestinationBrowseEntry>& listed_entries)
{
  std::vector<DestinationBrowseEntry> rows;
  rows.push_back({DestinationBrowseAction::kUseCurrentDirectory,
                  T_("Use this directory"), path, true});
  rows.push_back({DestinationBrowseAction::kParentDirectory, "../",
                  ParentBrowsePath(path), true});
  rows.insert(rows.end(), listed_entries.begin(), listed_entries.end());
  return rows;
}

static size_t DestinationBrowseVisibleRows(UaContext* ua, bool search_active)
{
  size_t chrome_lines = search_active ? 6 : 5;
  constexpr size_t kDefaultVisibleRows = 20;
  constexpr size_t kMinVisibleRows = 3;

  if (ua->terminal_height <= 0) { return kDefaultVisibleRows; }
  size_t available
      = static_cast<size_t>(ua->terminal_height) > chrome_lines
            ? static_cast<size_t>(ua->terminal_height) - chrome_lines
            : 0;
  return std::max(kMinVisibleRows, available);
}

static std::string RenderDestinationBrowseScreen(
    UaContext* ua,
    JobControlRecord* jcr,
    const std::string& path,
    const std::vector<DestinationBrowseEntry>& rows,
    size_t cursor,
    size_t first_row,
    std::string_view search)
{
  size_t width = RestoreOptionsScreenWidth(ua);
  std::string title = T_("Browse destination client");
  if (jcr->dir_impl->res.client) {
    title += ": ";
    title += jcr->dir_impl->res.client->resource_name_;
  }

  size_t visible_rows = std::min(
      DestinationBrowseVisibleRows(ua, !search.empty()), rows.size());
  first_row = std::min(first_row, rows.size() - visible_rows);

  std::string help = T_(
      "Enter: open/use  PgUp/PgDn or Ctrl-U/D: page  /: search  "
      "Esc/.: cancel");
  if (rows.size() > visible_rows && visible_rows > 0) {
    help += "  ";
    help += std::to_string(first_row + 1);
    help += "-";
    help += std::to_string(first_row + visible_rows);
    help += "/";
    help += std::to_string(rows.size());
  }

  std::string screen;
  screen.reserve((visible_rows + 5) * 80);
  screen += RestoreOptionsFrameBorder(width, ua->supports_color,
                                      FrameBorderStyle::kTop, title);
  screen += RestoreOptionsFrameLine(width, help, ua->supports_color);
  screen += RestoreOptionsFrameLine(width, "Path: " + path, ua->supports_color);
  if (!search.empty()) {
    screen += RestoreOptionsFrameLine(
        width, "Search: \"" + std::string(search) + "\"  (Esc clears search)",
        ua->supports_color);
  }
  screen += RestoreOptionsFrameBorder(width, ua->supports_color,
                                      FrameBorderStyle::kMiddle);

  if (visible_rows == 0) {
    screen
        += RestoreOptionsFrameLine(width, T_("No entries"), ua->supports_color);
  } else {
    for (size_t i = first_row; i < first_row + visible_rows; ++i) {
      std::string line = i == cursor ? "> " : "  ";
      line += rows[i].label;
      screen += RestoreOptionsFrameLine(width, line, ua->supports_color,
                                        i == cursor);
    }
  }

  screen += RestoreOptionsFrameBorder(width, ua->supports_color,
                                      FrameBorderStyle::kBottom);
  RemoveFinalNewline(&screen);
  return screen;
}

static std::string RenderDestinationBrowseSearchInput(
    UaContext* ua,
    JobControlRecord* jcr,
    const std::string& path,
    const std::vector<DestinationBrowseEntry>& rows,
    size_t cursor,
    size_t first_row,
    std::string_view search)
{
  size_t width = RestoreOptionsScreenWidth(ua);
  std::string title = T_("Search destination client");
  if (jcr->dir_impl->res.client) {
    title += ": ";
    title += jcr->dir_impl->res.client->resource_name_;
  }

  size_t visible_rows
      = std::min(DestinationBrowseVisibleRows(ua, true), rows.size());
  first_row = std::min(first_row, rows.size() - visible_rows);

  std::string help
      = T_("Type substring  Enter: accept  Backspace: delete  Esc: clear");
  if (rows.size() > visible_rows && visible_rows > 0) {
    help += "  ";
    help += std::to_string(first_row + 1);
    help += "-";
    help += std::to_string(first_row + visible_rows);
    help += "/";
    help += std::to_string(rows.size());
  }

  std::string screen;
  screen.reserve((visible_rows + 5) * 80);
  screen += RestoreOptionsFrameBorder(width, ua->supports_color,
                                      FrameBorderStyle::kTop, title);
  screen += RestoreOptionsFrameLine(width, help, ua->supports_color);
  screen += RestoreOptionsFrameLine(width, "Path: " + path, ua->supports_color);
  screen += RestoreOptionsFrameLine(width, "Search: " + std::string(search),
                                    ua->supports_color, true);
  screen += RestoreOptionsFrameBorder(width, ua->supports_color,
                                      FrameBorderStyle::kMiddle);
  if (visible_rows == 0) {
    screen
        += RestoreOptionsFrameLine(width, T_("No matches"), ua->supports_color);
  } else {
    for (size_t i = first_row; i < first_row + visible_rows; ++i) {
      std::string line = i == cursor ? "> " : "  ";
      line += rows[i].label;
      screen += RestoreOptionsFrameLine(width, line, ua->supports_color,
                                        i == cursor);
    }
  }
  screen += RestoreOptionsFrameBorder(width, ua->supports_color,
                                      FrameBorderStyle::kBottom);
  RemoveFinalNewline(&screen);
  return screen;
}

static void ClampDestinationBrowseWindow(size_t row_count,
                                         size_t visible_rows,
                                         size_t* cursor,
                                         size_t* first_row)
{
  if (row_count == 0) {
    *cursor = 0;
    *first_row = 0;
    return;
  }

  if (*cursor >= row_count) { *cursor = row_count - 1; }

  size_t max_first_row
      = row_count > visible_rows ? row_count - visible_rows : 0;
  if (*first_row > max_first_row) { *first_row = max_first_row; }

  if (*cursor < *first_row) {
    *first_row = *cursor;
  } else if (visible_rows > 0 && *cursor >= *first_row + visible_rows) {
    *first_row = *cursor - visible_rows + 1;
  }
}

static void MoveDestinationBrowsePage(size_t row_count,
                                      size_t visible_rows,
                                      int direction,
                                      size_t* cursor,
                                      size_t* first_row)
{
  if (row_count == 0) {
    *cursor = 0;
    *first_row = 0;
    return;
  }

  size_t max_first_row
      = row_count > visible_rows ? row_count - visible_rows : 0;
  if (direction > 0) {
    *first_row = std::min(*first_row + visible_rows, max_first_row);
  } else {
    *first_row = *first_row > visible_rows ? *first_row - visible_rows : 0;
  }
  *cursor = *first_row;
  ClampDestinationBrowseWindow(row_count, visible_rows, cursor, first_row);
}

static bool SetRestoreWhere(UaContext* ua,
                            JobControlRecord* jcr,
                            const std::string& where)
{
  if (!ua->AclAccessOk(Where_ACL, where.c_str(), true)) {
    ua->SendMsg(T_("No authorization for \"where\" specification.\n"));
    return false;
  }
  if (jcr->RegexWhere) {
    free(jcr->RegexWhere);
    jcr->RegexWhere = NULL;
  }
  if (jcr->where) {
    free(jcr->where);
    jcr->where = NULL;
  }
  jcr->where = strdup(where.c_str());
  return true;
}

static bool BrowseDestinationClientWhere(UaContext* ua,
                                         JobControlRecord* jcr,
                                         RunContext& rc)
{
  BareosSocket* user = ua->UA_sock;
  if (!user || !TreeBrowserSupported(ua)) { return false; }

  std::string current = NormalizeBrowsePath(
      jcr->where ? jcr->where
                 : (rc.job->RestoreWhere ? rc.job->RestoreWhere : "/"));
  std::string cached_path;
  std::vector<DestinationBrowseEntry> cached_entries;
  DestinationBrowseMode mode = DestinationBrowseMode::kBrowsing;
  std::string search;
  size_t cursor = 0;
  size_t first_row = 0;
  for (;;) {
    if (cached_path != current) {
      cached_entries.clear();
      if (!ListDestinationClientDirectory(ua, jcr, jcr->dir_impl->res.client,
                                          current, &cached_entries)) {
        return false;
      }
      cached_path = current;
      search.clear();
      mode = DestinationBrowseMode::kBrowsing;
      cursor = 0;
      first_row = 0;
    }
    std::vector<DestinationBrowseEntry> visible_entries
        = FilterDestinationBrowseEntries(cached_entries, search);
    std::vector<DestinationBrowseEntry> rows
        = BuildDestinationBrowseRows(current, visible_entries);
    size_t visible_rows = DestinationBrowseVisibleRows(
        ua, mode == DestinationBrowseMode::kEnteringSearch || !search.empty());
    ClampDestinationBrowseWindow(rows.size(), visible_rows, &cursor,
                                 &first_row);

    user->signal(BNET_START_SELECT);
    if (mode == DestinationBrowseMode::kEnteringSearch) {
      ua->SendMsg("%s", RenderDestinationBrowseSearchInput(
                            ua, jcr, current, rows, cursor, first_row, search)
                            .c_str());
    } else {
      ua->SendMsg("%s", RenderDestinationBrowseScreen(ua, jcr, current, rows,
                                                      cursor, first_row, search)
                            .c_str());
    }
    user->signal(BNET_END_SELECT);
    user->signal(BNET_SELECT_INPUT);

    int status = user->recv();
    if (status == BNET_SIGNAL || IsBnetStop(user)) { return false; }

    std::string_view input(user->msg, user->message_length);
    while (!input.empty()
           && (input.back() == '\r' || input.back() == '\n'
               || input.back() == ' ' || input.back() == '\t')) {
      input.remove_suffix(1);
    }

    if (input.starts_with("resize:")) {
      std::string_view size_view = input.substr(strlen("resize:"));
      size_t separator = size_view.find(':');
      std::string rows_text(size_view.substr(0, separator));
      int new_height = atoi(rows_text.c_str());
      if (new_height > 0) { ua->terminal_height = new_height; }
      if (separator != std::string_view::npos) {
        std::string cols_text(size_view.substr(separator + 1));
        int new_width = atoi(cols_text.c_str());
        if (new_width > 0) { ua->terminal_width = new_width; }
      }
      continue;
    }

    if (mode == DestinationBrowseMode::kEnteringSearch) {
      if (input == "key:enter") {
        mode = DestinationBrowseMode::kBrowsing;
        cursor = 0;
        first_row = 0;
      } else if (input == "key:cancel") {
        search.clear();
        mode = DestinationBrowseMode::kBrowsing;
        cursor = 0;
        first_row = 0;
      } else if (input == "key:down") {
        if (cursor + 1 < rows.size()) { cursor++; }
      } else if (input == "key:up") {
        if (cursor > 0) { cursor--; }
      } else if (input == "key:pagedown") {
        MoveDestinationBrowsePage(rows.size(), visible_rows, 1, &cursor,
                                  &first_row);
      } else if (input == "key:pageup") {
        MoveDestinationBrowsePage(rows.size(), visible_rows, -1, &cursor,
                                  &first_row);
      } else if (input == "key:backspace") {
        RemoveLastBrowseSearchCharacter(&search);
      } else if (input == "key:space") {
        search.push_back(' ');
      } else if (input.starts_with("key:text:")) {
        search.append(input.substr(strlen("key:text:")));
      }
      continue;
    }

    if (input == "key:cancel" || input == "." || input == "key:text:.") {
      if (!search.empty()) {
        search.clear();
        cursor = 0;
        first_row = 0;
        continue;
      }
      return false;
    }
    if (input == "key:text:/") {
      mode = DestinationBrowseMode::kEnteringSearch;
      search.clear();
      cursor = 0;
      first_row = 0;
      continue;
    }
    if (input == "key:tab" || input == "key:down" || input == "key:right") {
      cursor = rows.empty() ? 0 : (cursor + 1) % rows.size();
      continue;
    }
    if (input == "key:up" || input == "key:left") {
      cursor = cursor == 0 ? rows.size() - 1 : cursor - 1;
      continue;
    }
    if (input == "key:pagedown") {
      MoveDestinationBrowsePage(rows.size(), visible_rows, 1, &cursor,
                                &first_row);
      continue;
    }
    if (input == "key:pageup") {
      MoveDestinationBrowsePage(rows.size(), visible_rows, -1, &cursor,
                                &first_row);
      continue;
    }
    if (input == "key:backspace") {
      current = ParentBrowsePath(current);
      cursor = 0;
      first_row = 0;
      continue;
    }
    if (input == "key:home") {
      cursor = 0;
      first_row = 0;
      continue;
    }
    if (input == "key:end") {
      cursor = rows.empty() ? 0 : rows.size() - 1;
      ClampDestinationBrowseWindow(rows.size(), visible_rows, &cursor,
                                   &first_row);
      continue;
    }
    if (input != "key:enter" && !input.empty()) { continue; }

    const DestinationBrowseEntry& selected = rows[cursor];
    switch (selected.action) {
      case DestinationBrowseAction::kUseCurrentDirectory:
        return SetRestoreWhere(ua, jcr, current);
      case DestinationBrowseAction::kParentDirectory:
      case DestinationBrowseAction::kOpenDirectory:
        current = NormalizeBrowsePath(selected.path);
        cursor = 0;
        break;
      case DestinationBrowseAction::kFile:
        ua->InfoMsg(T_("Select a directory for the restore destination.\n"));
        break;
    }
  }
}

bool DotClientbrowseCmd(UaContext* ua, const char*)
{
  int client_arg = FindArgWithValue(ua, NT_("client"));
  int path_arg = FindArgWithValue(ua, NT_("path"));
  if (client_arg < 0 || path_arg < 0) {
    ua->ErrorMsg(T_("Required arguments: client=<client-name> path=<path>\n"));
    return false;
  }

  ClientResource* client = ua->GetClientResWithName(ua->argv[client_arg]);
  if (!client) {
    ua->ErrorMsg(T_("Client \"%s\" not found.\n"), ua->argv[client_arg]);
    return false;
  }

  std::string path = NormalizeBrowsePath(ua->argv[path_arg]);
  std::vector<DestinationBrowseEntry> entries;
  if (!ListDestinationClientDirectory(ua, ua->jcr, client, path, &entries)) {
    return false;
  }

  ua->send->ObjectStart(".clientbrowse");
  ua->send->ObjectKeyValue("client", client->resource_name_, "%s\n");
  ua->send->ObjectKeyValue("path", path.c_str(), "%s\n");
  ua->send->ArrayStart("entries");
  for (const auto& entry : entries) {
    ua->send->ObjectStart();
    ua->send->ObjectKeyValue("name", entry.label.c_str(), "%s\n");
    ua->send->ObjectKeyValue("path", entry.path.c_str(), "%s\n");
    ua->send->ObjectKeyValue("type", entry.directory ? "directory" : "file",
                             "%s\n");
    ua->send->ObjectEnd();
  }
  ua->send->ArrayEnd("entries");
  ua->send->ObjectEnd(".clientbrowse");
  return true;
}

static std::vector<RestoreOptionRow> BuildRestoreAdvancedOptionRows(
    JobControlRecord* jcr,
    RunContext& rc)
{
  char dt[MAX_TIME_LENGTH];
  std::vector<RestoreOptionRow> rows;
  rows.push_back({RestoreOptionAction::kBack, T_("Back"),
                  T_("return to restore options")});
  rows.push_back({RestoreOptionAction::kWhen, T_("When"),
                  bstrutime(dt, sizeof(dt), jcr->sched_time)});
  rows.push_back({RestoreOptionAction::kPriority, T_("Priority"),
                  std::to_string(jcr->JobPriority)});
  rows.push_back({RestoreOptionAction::kBootstrap, T_("Bootstrap"),
                  NPRT(jcr->RestoreBootstrap)});
  rows.push_back({RestoreOptionAction::kStorage, T_("Storage"),
                  jcr->dir_impl->res.read_storage
                      ? jcr->dir_impl->res.read_storage->resource_name_
                      : T_("*None*"),
                  false});
  rows.push_back({RestoreOptionAction::kJobId, T_("JobId"),
                  jcr->dir_impl->RestoreJobId == 0
                      ? T_("*None*")
                      : std::to_string(jcr->dir_impl->RestoreJobId),
                  false});
  rows.push_back({RestoreOptionAction::kRestoreJob, T_("Restore Job"),
                  rc.job ? rc.job->resource_name_ : T_("*None*")});
  return rows;
}

static std::string RestoreOptionsFrameBorder(size_t width,
                                             bool color,
                                             FrameBorderStyle style,
                                             std::string_view title)
{
  std::string line = RenderFrameBorder(width, style, title);
  if (!color) { return line + "\n"; }
  return "\033[34m" + line + "\033[0m\n";
}

static std::string RestoreOptionsFrameLine(size_t width,
                                           std::string_view text,
                                           bool color,
                                           bool highlighted)
{
  if (width < 2) { return FitText(text, width) + "\n"; }

  std::string content = FitText(text, width - 2);
  content = StyleFrameContent(std::move(content), ' ', highlighted, color);

  std::string border = color ? "\033[34m│\033[0m" : "│";
  return border + content + border + "\n";
}

static std::string RenderRestoreOptionsScreen(UaContext* ua,
                                              JobControlRecord* jcr,
                                              RunContext& rc,
                                              size_t cursor,
                                              bool advanced)
{
  std::vector<RestoreOptionRow> rows
      = advanced ? BuildRestoreAdvancedOptionRows(jcr, rc)
                 : BuildRestoreOptionRows(jcr, rc);
  size_t width = RestoreOptionsScreenWidth(ua);
  size_t inner_width = width > 2 ? width - 2 : width;
  size_t value_width = inner_width > 46 ? inner_width - 46 : 20;
  std::string screen;
  screen.reserve(rows.size() * 80);
  screen += RestoreOptionsFrameBorder(
      width, ua->supports_color, FrameBorderStyle::kTop,
      advanced ? T_("Advanced Restore Options") : T_("Restore Options"));
  screen += RestoreOptionsFrameLine(
      width, T_("Tab/Down: next  Up: previous  Enter: edit  Esc/.: cancel"),
      ua->supports_color);
  screen += RestoreOptionsFrameBorder(width, ua->supports_color,
                                      FrameBorderStyle::kMiddle);

  for (size_t i = 0; i < rows.size(); ++i) {
    const auto& row = rows[i];
    std::string line;
    line += i == cursor ? "> " : "  ";
    line += row.advanced ? "[Advanced] " : "           ";
    line += FitRestoreOptionText(row.label, 30);
    if (line.size() < 46) { line.append(46 - line.size(), ' '); }
    line += FitRestoreOptionText(row.value, value_width);
    screen += RestoreOptionsFrameLine(width, line, ua->supports_color,
                                      i == cursor);
  }
  screen += RestoreOptionsFrameBorder(width, ua->supports_color,
                                      FrameBorderStyle::kBottom);
  RemoveFinalNewline(&screen);
  return screen;
}

static int SelectRestoreOptionVisual(UaContext* ua,
                                     JobControlRecord* jcr,
                                     RunContext& rc,
                                     bool advanced)
{
  BareosSocket* user = ua->UA_sock;
  if (!user) { return -1; }

  size_t cursor = 0;
  std::vector<RestoreOptionRow> rows
      = advanced ? BuildRestoreAdvancedOptionRows(jcr, rc)
                 : BuildRestoreOptionRows(jcr, rc);
  size_t row_count = rows.size();
  for (;;) {
    user->signal(BNET_START_SELECT);
    ua->SendMsg(
        "%s",
        RenderRestoreOptionsScreen(ua, jcr, rc, cursor, advanced).c_str());
    user->signal(BNET_END_SELECT);
    user->signal(BNET_SELECT_INPUT);

    int status = user->recv();
    if (status == BNET_SIGNAL || IsBnetStop(user)) { return -1; }

    std::string_view input(user->msg, user->message_length);
    while (!input.empty()
           && (input.back() == '\r' || input.back() == '\n'
               || input.back() == ' ' || input.back() == '\t')) {
      input.remove_suffix(1);
    }

    if (input.starts_with("resize:")) {
      std::string_view size_view = input.substr(strlen("resize:"));
      size_t separator = size_view.find(':');
      std::string rows_text(size_view.substr(0, separator));
      int new_height = atoi(rows_text.c_str());
      if (new_height > 0) { ua->terminal_height = new_height; }
      if (separator != std::string_view::npos) {
        std::string cols_text(size_view.substr(separator + 1));
        int new_width = atoi(cols_text.c_str());
        if (new_width > 0) { ua->terminal_width = new_width; }
      }
      continue;
    }

    if (input == "key:cancel" || input == "." || input == "key:text:.") {
      ua->InfoMsg(T_("Selection aborted, nothing done.\n"));
      return -1;
    }
    if (input == "key:enter" || input.empty()) {
      return static_cast<int>(rows[cursor].action);
    }
    if (input == "key:tab" || input == "key:down" || input == "key:right") {
      cursor = (cursor + 1) % row_count;
    } else if (input == "key:up" || input == "key:left"
               || input == "key:backspace") {
      cursor = cursor == 0 ? row_count - 1 : cursor - 1;
    } else if (input == "key:home") {
      cursor = 0;
    } else if (input == "key:end") {
      cursor = row_count - 1;
    }
  }
}

static int SelectRestoreOptionFallback(UaContext* ua, bool advanced)
{
  StartPrompt(ua, advanced ? T_("Advanced restore options:\n")
                           : T_("Restore options to modify:\n"));
  if (advanced) {
    AddPrompt(ua, T_("Back"));        /* 0 */
    AddPrompt(ua, T_("When"));        /* 1 */
    AddPrompt(ua, T_("Priority"));    /* 2 */
    AddPrompt(ua, T_("Bootstrap"));   /* 3 */
    AddPrompt(ua, T_("Storage"));     /* 4 */
    AddPrompt(ua, T_("JobId"));       /* 5 */
    AddPrompt(ua, T_("Restore Job")); /* 6 */
  } else {
    AddPrompt(ua, T_("Continue to restore summary"));             /* 0 */
    AddPrompt(ua, T_("Restore Client"));                          /* 1 */
    AddPrompt(ua, T_("Browse destination client (unavailable)")); /* 2 */
    AddPrompt(ua, T_("Where"));                                   /* 3 */
    AddPrompt(ua, T_("File Relocation"));                         /* 4 */
    AddPrompt(ua, T_("Replace Policy"));                          /* 5 */
    AddPrompt(ua, T_("Plugin Options"));                          /* 6 */
    AddPrompt(ua, T_("Advanced Options"));                        /* 7 */
  }
  int selected = DoPrompt(ua, "",
                          advanced ? T_("Select advanced restore option")
                                   : T_("Select restore option to modify"),
                          NULL, 0);
  if (selected < 0) { return selected; }
  if (advanced) {
    static constexpr RestoreOptionAction kAdvancedActions[]
        = {RestoreOptionAction::kBack,      RestoreOptionAction::kWhen,
           RestoreOptionAction::kPriority,  RestoreOptionAction::kBootstrap,
           RestoreOptionAction::kStorage,   RestoreOptionAction::kJobId,
           RestoreOptionAction::kRestoreJob};
    return static_cast<int>(kAdvancedActions[selected]);
  }
  static constexpr RestoreOptionAction kMainActions[] = {
      RestoreOptionAction::kContinue,      RestoreOptionAction::kRestoreClient,
      RestoreOptionAction::kBrowseWhere,   RestoreOptionAction::kWhere,
      RestoreOptionAction::kRelocation,    RestoreOptionAction::kReplace,
      RestoreOptionAction::kPluginOptions, RestoreOptionAction::kAdvancedMenu};
  return static_cast<int>(kMainActions[selected]);
}

static int ModifyRestoreParameters(UaContext* ua,
                                   JobControlRecord* jcr,
                                   RunContext& rc)
{
  int opt;

  int selection = -1;
  for (;;) {
    selection = TreeBrowserSupported(ua)
                    ? SelectRestoreOptionVisual(ua, jcr, rc, false)
                    : SelectRestoreOptionFallback(ua, false);
    if (selection < 0) { return -1; }

    auto action = static_cast<RestoreOptionAction>(selection);
    if (action != RestoreOptionAction::kAdvancedMenu) { break; }

    selection = TreeBrowserSupported(ua)
                    ? SelectRestoreOptionVisual(ua, jcr, rc, true)
                    : SelectRestoreOptionFallback(ua, true);
    if (selection < 0) { return -1; }
    if (static_cast<RestoreOptionAction>(selection)
        != RestoreOptionAction::kBack) {
      break;
    }
  }

  switch (static_cast<RestoreOptionAction>(selection)) {
    case RestoreOptionAction::kContinue:
      return 1;
    case RestoreOptionAction::kBack:
    case RestoreOptionAction::kAdvancedMenu:
      goto try_again;
    case RestoreOptionAction::kRestoreClient:
      rc.client = select_client_resource(ua);
      if (rc.client) {
        jcr->dir_impl->res.client = rc.client;
        goto try_again;
      }
      break;
    case RestoreOptionAction::kBrowseWhere:
      if (BrowseDestinationClientWhere(ua, jcr, rc)) { goto try_again; }
      ua->SendMsg(T_("Browsing canceled. Enter Where manually if needed.\n"));
      goto try_again;
    case RestoreOptionAction::kWhere:
      if (GetCmd(ua, T_("Please enter the full path prefix for restore (/ "
                        "for none): "))) {
        if (!ua->AclAccessOk(Where_ACL, ua->cmd, true)) {
          ua->SendMsg(T_("No authorization for \"where\" specification.\n"));
        } else {
          if (jcr->RegexWhere) {
            free(jcr->RegexWhere);
            jcr->RegexWhere = NULL;
          }
          if (jcr->where) {
            free(jcr->where);
            jcr->where = NULL;
          }
          if (IsPathSeparator(ua->cmd[0]) && ua->cmd[1] == '\0') {
            ua->cmd[0] = 0;
          }
          jcr->where = strdup(ua->cmd);
        }
        goto try_again;
      }
      break;
    case RestoreOptionAction::kRelocation:
      SelectWhereRegexp(ua, jcr);
      goto try_again;
    case RestoreOptionAction::kReplace:
      StartPrompt(ua, T_("Replace policy:\n"));
      for (int i = 0; ReplaceOptions[i].name; i++) {
        AddPrompt(ua, RestoreReplacePromptLabel(ReplaceOptions[i].name));
      }
      opt = DoPrompt(ua, "", T_("Select replace policy"), NULL, 0);
      if (opt >= 0) {
        rc.replace = ReplaceOptions[opt].name;
        jcr->dir_impl->replace = ReplaceOptions[opt].token;
      }
      goto try_again;
    case RestoreOptionAction::kPluginOptions:
      GetPluginOptions(ua, jcr);
      goto try_again;
    case RestoreOptionAction::kWhen:
      for (;;) {
        if (!GetCmd(ua, T_("Please enter desired start time as YYYY-MM-DD "
                           "HH:MM:SS (return for now): "))) {
          break;
        }
        if (ua->cmd[0] == 0) {
          jcr->sched_time = time(NULL);
        } else {
          auto parsed = StrToUtime(ua->cmd);
          if (parsed == 0) {
            ua->SendMsg(T_("Invalid time specification.\n"));
            continue;
          }
          jcr->sched_time = parsed;
        }
        goto try_again;
      }
      break;
    case RestoreOptionAction::kPriority:
      if (GetPint(ua, T_("Enter new Priority: "))) {
        if (!ua->pint32_val) {
          ua->SendMsg(T_("Priority must be a positive integer.\n"));
        } else {
          jcr->JobPriority = ua->pint32_val;
        }
        goto try_again;
      }
      break;
    case RestoreOptionAction::kBootstrap:
      if (GetCmd(ua, T_("Please enter the Bootstrap file name: "))) {
        if (jcr->RestoreBootstrap) {
          free(jcr->RestoreBootstrap);
          jcr->RestoreBootstrap = NULL;
        }
        if (ua->cmd[0] != 0) {
          FILE* fd;

          jcr->RestoreBootstrap = strdup(ua->cmd);
          fd = fopen(jcr->RestoreBootstrap, "rb");
          if (!fd) {
            BErrNo be;
            ua->SendMsg(T_("Warning cannot open %s: ERR=%s\n"),
                        jcr->RestoreBootstrap, be.bstrerror());
            free(jcr->RestoreBootstrap);
            jcr->RestoreBootstrap = NULL;
          } else {
            fclose(fd);
          }
        }
        goto try_again;
      }
      break;
    case RestoreOptionAction::kStorage:
      rc.store->store = select_storage_resource(ua);
      if (rc.store->store) {
        PmStrcpy(rc.store->store_source, T_("user selection"));
        SetRwstorage(jcr, rc.store);
        goto try_again;
      }
      break;
    case RestoreOptionAction::kJobId:
      rc.jid = NULL;
      jcr->dir_impl->RestoreJobId = 0;
      if (jcr->RestoreBootstrap) {
        ua->SendMsg(
            T_("You must set the bootstrap file to NULL to be able to specify "
               "a JobId.\n"));
      }
      goto try_again;
    case RestoreOptionAction::kRestoreJob:
      rc.job = select_job_resource(ua);
      if (rc.job) {
        jcr->dir_impl->res.job = rc.job;
        SetJcrDefaults(jcr, rc.job);
        goto try_again;
      }
      break;
  }
  return -1;

try_again:
  return 0;
}

/**
 * Reset the restore context.
 * This subroutine can be called multiple times, so it must keep any prior
 * settings.
 */
static bool ResetRestoreContext(UaContext* ua,
                                JobControlRecord* jcr,
                                RunContext& rc)
{
  jcr->dir_impl->res.verify_job = rc.verify_job;
  jcr->dir_impl->res.previous_job = rc.previous_job;
  jcr->dir_impl->res.pool = rc.pool;
  jcr->dir_impl->res.next_pool = rc.next_pool;

  /* See if an explicit pool override was performed.
   * If so set the pool_source to command line and
   * set the IgnoreLevelPoolOverrides so any level Pool
   * overrides are ignored. */
  if (rc.pool_name) {
    PmStrcpy(jcr->dir_impl->res.pool_source, T_("command line"));
    jcr->dir_impl->IgnoreLevelPoolOverrides = true;
  } else if (!rc.level_override
             && jcr->dir_impl->res.pool != jcr->dir_impl->res.job->pool) {
    PmStrcpy(jcr->dir_impl->res.pool_source, T_("user input"));
  }
  SetRwstorage(jcr, rc.store);

  if (rc.next_pool_name) {
    PmStrcpy(jcr->dir_impl->res.npool_source, T_("command line"));
    jcr->dir_impl->res.run_next_pool_override = true;
  } else if (jcr->dir_impl->res.next_pool
             != jcr->dir_impl->res.pool->NextPool) {
    PmStrcpy(jcr->dir_impl->res.npool_source, T_("user input"));
    jcr->dir_impl->res.run_next_pool_override = true;
  }

  jcr->dir_impl->res.client = rc.client;
  if (jcr->dir_impl->res.client) {
    PmStrcpy(jcr->client_name, rc.client->resource_name_);
  }
  jcr->dir_impl->res.fileset = rc.fileset;
  jcr->dir_impl->ExpectedFiles = rc.files;

  if (rc.catalog) {
    jcr->dir_impl->res.catalog = rc.catalog;
    PmStrcpy(jcr->dir_impl->res.catalog_source, T_("user input"));
  }

  PmStrcpy(jcr->comment, rc.comment);

  if (rc.where) {
    if (jcr->where) { free(jcr->where); }
    jcr->where = strdup(rc.where);
    rc.where = NULL;
  }

  if (rc.regexwhere) {
    if (jcr->RegexWhere) { free(jcr->RegexWhere); }
    jcr->RegexWhere = strdup(rc.regexwhere);
    rc.regexwhere = NULL;
  }

  if (rc.when) {
    jcr->sched_time = StrToUtime(rc.when);
    if (jcr->sched_time == 0) {
      ua->SendMsg(
          T_("Invalid time specification in when; expected: \"YYYY-MM-DD "
             "HH:MM:SS\"\n"));
      return false;
    }
    rc.when = NULL;
  }

  if (rc.bootstrap) {
    if (jcr->RestoreBootstrap) { free(jcr->RestoreBootstrap); }
    jcr->RestoreBootstrap = strdup(rc.bootstrap);
    rc.bootstrap = NULL;
  }

  if (rc.plugin_options) {
    if (jcr->dir_impl->plugin_options) { free(jcr->dir_impl->plugin_options); }
    jcr->dir_impl->plugin_options = strdup(rc.plugin_options);
    rc.plugin_options = NULL;
  }

  if (rc.replace) {
    jcr->dir_impl->replace = 0;
    for (int i = 0; ReplaceOptions[i].name; i++) {
      if (Bstrcasecmp(rc.replace, ReplaceOptions[i].name)) {
        jcr->dir_impl->replace = ReplaceOptions[i].token;
      }
    }
    if (!jcr->dir_impl->replace) {
      ua->SendMsg(T_("Invalid replace option: %s\n"), rc.replace);
      return false;
    }
  } else if (rc.job->replace) {
    jcr->dir_impl->replace = rc.job->replace;
  } else {
    jcr->dir_impl->replace = REPLACE_ALWAYS;
  }
  rc.replace = NULL;

  if (rc.Priority) {
    jcr->JobPriority = rc.Priority;
    rc.Priority = 0;
  }

  if (rc.since) {
    if (StrToUtime(rc.since) != 0) {
      if (!jcr->starttime_string) {
        jcr->starttime_string = GetPoolMemory(PM_MESSAGE);
      }
      PmStrcpy(jcr->starttime_string, rc.since);
      rc.since = NULL;
    } else {
      ua->SendMsg(T_("Since option expects string in format \"yyyy-mm-dd "
                     "hh:mm:ss\", but is: %s\n"),
                  rc.since);
      return false;
    }
  }

  if (rc.cloned) {
    jcr->dir_impl->cloned = rc.cloned;
    rc.cloned = false;
  }

  // If pool changed, update migration write storage
  if (jcr->is_JobType(JT_MIGRATE) || jcr->is_JobType(JT_COPY)
      || (jcr->is_JobType(JT_BACKUP) && jcr->is_JobLevel(L_VIRTUAL_FULL))) {
    if (!SetMigrationWstorage(jcr, rc.pool, rc.next_pool,
                              T_("Storage from Run NextPool override"))) {
      return false;
    }
  }
  rc.replace = ReplaceOptions[0].name;
  for (int i = 0; ReplaceOptions[i].name; i++) {
    if (ReplaceOptions[i].token == jcr->dir_impl->replace) {
      rc.replace = ReplaceOptions[i].name;
    }
  }

  if (rc.level_name) {
    if (!GetLevelFromName(jcr, rc.level_name)) {
      ua->SendMsg(T_("Level \"%s\" not valid.\n"), rc.level_name);
      return false;
    }
    rc.level_name = NULL;
  }

  if (rc.jid) {
    if (jcr->is_JobType(JT_BACKUP) && jcr->is_JobLevel(L_VIRTUAL_FULL)) {
      if (!jcr->dir_impl->vf_jobids) {
        jcr->dir_impl->vf_jobids = GetPoolMemory(PM_MESSAGE);
      }
      PmStrcpy(jcr->dir_impl->vf_jobids, rc.jid);
    } else {
      // Note, this is also MigrateJobId and a VerifyJobId
      jcr->dir_impl->RestoreJobId = str_to_int64(rc.jid);
    }
    rc.jid = NULL;
  }

  if (rc.backup_format) {
    if (jcr->dir_impl->backup_format) { free(jcr->dir_impl->backup_format); }
    jcr->dir_impl->backup_format = strdup(rc.backup_format);
    rc.backup_format = NULL;
  }

  /* Some options are not available through the menu
   * TODO: Add an advanced menu? */
  if (rc.spool_data_set) { jcr->dir_impl->spool_data = rc.spool_data; }

  if (rc.accurate_set) { jcr->accurate = rc.accurate; }

  /* Used by migration jobs that can have the same name,
   * but can run at the same time */
  if (rc.ignoreduplicatecheck_set) {
    jcr->dir_impl->IgnoreDuplicateJobChecking = rc.ignoreduplicatecheck;
  }

  /* If this is a virtualfull spawned by a consolidate job
   * then the same rjs is used so that max concurrent jobs
   * of the consolidate job applies. AllowMixedPriority is
   * also inherited from consolidate job.
   */
  if (rc.consolidate_job) {
    if (!jcr->is_JobLevel(L_VIRTUAL_FULL)) {
      ua->SendMsg(
          T_("Consolidate Job option is only valid for virtual full jobs.\n"));
      return false;
    }
    jcr->dir_impl->res.rjs = rc.consolidate_job->rjs;
    jcr->dir_impl->max_concurrent_jobs = rc.consolidate_job->MaxConcurrentJobs;
    jcr->allow_mixed_priority = rc.consolidate_job->allow_mixed_priority;
  }

  return true;
}

static void SelectWhereRegexp(UaContext* ua, JobControlRecord* jcr)
{
  alist<BareosRegex*>* regs;
  char *strip_prefix, *add_prefix, *add_suffix, *rwhere;
  strip_prefix = add_suffix = rwhere = add_prefix = NULL;

try_again_reg:
  ua->SendMsg(T_("strip_prefix=%s add_prefix=%s add_suffix=%s\n"),
              NPRT(strip_prefix), NPRT(add_prefix), NPRT(add_suffix));

  StartPrompt(ua, T_("This will replace your current Where value\n"));
  AddPrompt(ua, T_("Strip prefix"));               /* 0 */
  AddPrompt(ua, T_("Add prefix"));                 /* 1 */
  AddPrompt(ua, T_("Add file suffix"));            /* 2 */
  AddPrompt(ua, T_("Enter a regexp"));             /* 3 */
  AddPrompt(ua, T_("Test filename manipulation")); /* 4 */
  AddPrompt(ua, T_("Use this ?"));                 /* 5 */

  switch (DoPrompt(ua, "", T_("Select parameter to modify"), NULL, 0)) {
    case 0:
      /* Strip prefix */
      if (GetCmd(ua, T_("Please enter the path prefix to strip: "))) {
        if (strip_prefix) free(strip_prefix);
        strip_prefix = strdup(ua->cmd);
      }
      goto try_again_reg;
    case 1:
      /* Add prefix */
      if (GetCmd(ua,
                 T_("Please enter the path prefix to add (/ for none): "))) {
        if (IsPathSeparator(ua->cmd[0]) && ua->cmd[1] == '\0') {
          ua->cmd[0] = 0;
        }

        if (add_prefix) free(add_prefix);
        add_prefix = strdup(ua->cmd);
      }
      goto try_again_reg;
    case 2:
      /* Add suffix */
      if (GetCmd(ua, T_("Please enter the file suffix to add: "))) {
        if (add_suffix) free(add_suffix);
        add_suffix = strdup(ua->cmd);
      }
      goto try_again_reg;
    case 3:
      /* Add rwhere */
      if (GetCmd(ua, T_("Please enter a valid regexp (!from!to!): "))) {
        if (!ua->AclAccessOk(Where_ACL, ua->cmd, true)) {
          ua->SendMsg(T_("Denied by \"WhereACL\" configuration.\n"));
          goto try_again_reg;
        }
        if (rwhere) free(rwhere);
        rwhere = strdup(ua->cmd);
      }
      goto try_again_reg;
    case 4:
      /* Test regexp */
      char* result;
      char* regexp;

      if (rwhere && rwhere[0] != '\0') {
        regs = get_bregexps(rwhere);
        ua->SendMsg(T_("regexwhere=%s\n"), NPRT(rwhere));
      } else {
        int len
            = BregexpGetBuildWhereSize(strip_prefix, add_prefix, add_suffix);
        regexp = (char*)malloc(len * sizeof(char));
        bregexp_build_where(regexp, len, strip_prefix, add_prefix, add_suffix);
        regs = get_bregexps(regexp);
        ua->SendMsg(
            T_("strip_prefix=%s add_prefix=%s add_suffix=%s result=%s\n"),
            NPRT(strip_prefix), NPRT(add_prefix), NPRT(add_suffix),
            NPRT(regexp));

        free(regexp);
      }

      if (!regs) {
        ua->SendMsg(T_("Cannot use your regexp\n"));
        goto try_again_reg;
      }
      ua->SendMsg(T_("Enter a period (.) to stop this test\n"));
      while (GetCmd(ua, T_("Please enter filename to test: "))) {
        ApplyBregexps(ua->cmd, regs, &result);
        ua->SendMsg(T_("%s -> %s\n"), ua->cmd, result);
      }
      FreeBregexps(regs);
      delete regs;
      goto try_again_reg;
    case 5:
      /* OK */
      break;
    case -1: /* error or cancel */
      goto bail_out_reg;
    default:
      goto try_again_reg;
  }

  /* replace the existing where */
  if (jcr->where) {
    free(jcr->where);
    jcr->where = NULL;
  }

  /* replace the existing regexwhere */
  if (jcr->RegexWhere) {
    free(jcr->RegexWhere);
    jcr->RegexWhere = NULL;
  }

  if (rwhere) {
    jcr->RegexWhere = strdup(rwhere);
  } else if (strip_prefix || add_prefix || add_suffix) {
    int len = BregexpGetBuildWhereSize(strip_prefix, add_prefix, add_suffix);
    jcr->RegexWhere = (char*)malloc(len * sizeof(char));
    bregexp_build_where(jcr->RegexWhere, len, strip_prefix, add_prefix,
                        add_suffix);
    if (!ua->AclAccessOk(Where_ACL, jcr->RegexWhere, true)) {
      ua->SendMsg(T_("Regex (%s) denied by \"WhereACL\" configuration.\n"),
                  jcr->RegexWhere);
      free(jcr->RegexWhere);
      jcr->RegexWhere = NULL;
      goto try_again_reg;
    }
  }

  regs = get_bregexps(jcr->RegexWhere);
  if (regs) {
    FreeBregexps(regs);
    delete regs;
  } else {
    if (jcr->RegexWhere) {
      free(jcr->RegexWhere);
      jcr->RegexWhere = NULL;
    }
    ua->SendMsg(T_("Cannot use your regexp.\n"));
  }

bail_out_reg:
  if (strip_prefix) { free(strip_prefix); }
  if (add_prefix) { free(add_prefix); }
  if (add_suffix) { free(add_suffix); }
  if (rwhere) { free(rwhere); }
}

static bool GetPluginOptions(UaContext* ua, JobControlRecord* jcr)
{
  if (GetCmd(ua, T_("Please enter Plugin Options string: "))) {
    // Authorize every "pluginname:key=value:..." block individually,
    // since SendPluginOptions() (dird/fd_cmds.cc) sends one
    // "pluginoptions" protocol command per newline-separated block --
    // checking only the whole string would let an allowed block
    // smuggle an otherwise-denied block past a single-block ACL
    // pattern.
    if (!restore_plugin_hints::AllPluginOptionsBlocksAuthorized(
            ua->cmd, [ua](const std::string& block) {
              return ua->AclAccessOk(PluginOptions_ACL, block.c_str(), true);
            })) {
      ua->SendMsg(
          T_("No authorization for \"PluginOptions\" specification.\n"));
      return false;
    }
    if (jcr->dir_impl->plugin_options) {
      free(jcr->dir_impl->plugin_options);
      jcr->dir_impl->plugin_options = NULL;
    }
    jcr->dir_impl->plugin_options = strdup(ua->cmd);
    return true;
  }
  return false;
}

static void SelectJobLevel(UaContext* ua, JobControlRecord* jcr)
{
  if (jcr->is_JobType(JT_BACKUP)) {
    StartPrompt(ua, T_("Levels:\n"));
    AddPrompt(ua, T_("Full"));
    AddPrompt(ua, T_("Incremental"));
    AddPrompt(ua, T_("Differential"));
    AddPrompt(ua, T_("Since"));
    AddPrompt(ua, T_("VirtualFull"));
    switch (DoPrompt(ua, "", T_("Select level"), NULL, 0)) {
      case 0:
        jcr->setJobLevel(L_FULL);
        break;
      case 1:
        jcr->setJobLevel(L_INCREMENTAL);
        break;
      case 2:
        jcr->setJobLevel(L_DIFFERENTIAL);
        break;
      case 3:
        jcr->setJobLevel(L_SINCE);
        break;
      case 4:
        jcr->setJobLevel(L_VIRTUAL_FULL);
        break;
      default:
        break;
    }
  } else if (jcr->is_JobType(JT_VERIFY)) {
    StartPrompt(ua, T_("Levels:\n"));
    AddPrompt(ua, T_("Initialize Catalog"));
    AddPrompt(ua, T_("Verify Catalog"));
    AddPrompt(ua, T_("Verify Volume to Catalog"));
    AddPrompt(ua, T_("Verify Disk to Catalog"));
    AddPrompt(ua, T_("Verify Volume Data (not yet implemented)"));
    switch (DoPrompt(ua, "", T_("Select level"), NULL, 0)) {
      case 0:
        jcr->setJobLevel(L_VERIFY_INIT);
        break;
      case 1:
        jcr->setJobLevel(L_VERIFY_CATALOG);
        break;
      case 2:
        jcr->setJobLevel(L_VERIFY_VOLUME_TO_CATALOG);
        break;
      case 3:
        jcr->setJobLevel(L_VERIFY_DISK_TO_CATALOG);
        break;
      case 4:
        jcr->setJobLevel(L_VERIFY_DATA);
        break;
      default:
        break;
    }
  } else {
    ua->WarningMsg(
        T_("Level not appropriate for this Job. Cannot be changed.\n"));
  }
  return;
}

static bool DisplayJobParameters(UaContext* ua,
                                 JobControlRecord* jcr,
                                 RunContext& rc)
{
  char ec1[30];
  JobResource* job = rc.job;
  char dt[MAX_TIME_LENGTH];

  Dmsg1(800, "JobType=%c\n", jcr->getJobType());
  switch (jcr->getJobType()) {
    case JT_ADMIN:
      if (ua->api) {
        ua->signal(BNET_RUN_CMD);
        ua->SendMsg(
            "Type: Admin\n"
            "Title: Run Admin Job\n"
            "JobName:  %s\n"
            "FileSet:  %s\n"
            "Client:   %s\n"
            "Storage:  %s\n"
            "When:     %s\n"
            "Priority: %d\n",
            job->resource_name_, jcr->dir_impl->res.fileset->resource_name_,
            NPRT(jcr->dir_impl->res.client->resource_name_),
            jcr->dir_impl->res.write_storage
                ? jcr->dir_impl->res.write_storage->resource_name_
                : T_("*None*"),
            bstrutime(dt, sizeof(dt), jcr->sched_time), jcr->JobPriority);
      } else {
        ua->SendMsg(
            T_("Run Admin Job\n"
               "JobName:  %s\n"
               "FileSet:  %s\n"
               "Client:   %s\n"
               "Storage:  %s\n"
               "When:     %s\n"
               "Priority: %d\n"),
            job->resource_name_, jcr->dir_impl->res.fileset->resource_name_,
            NPRT(jcr->dir_impl->res.client->resource_name_),
            jcr->dir_impl->res.write_storage
                ? jcr->dir_impl->res.write_storage->resource_name_
                : T_("*None*"),
            bstrutime(dt, sizeof(dt), jcr->sched_time), jcr->JobPriority);
      }
      jcr->setJobLevel(L_FULL);
      break;
    case JT_ARCHIVE:
      if (ua->api) {
        ua->signal(BNET_RUN_CMD);
        ua->SendMsg(
            "Type: Archive\n"
            "Title: Run Archive Job\n"
            "JobName:  %s\n"
            "FileSet:  %s\n"
            "Client:   %s\n"
            "Storage:  %s\n"
            "When:     %s\n"
            "Priority: %d\n",
            job->resource_name_, jcr->dir_impl->res.fileset->resource_name_,
            NPRT(jcr->dir_impl->res.client->resource_name_),
            jcr->dir_impl->res.write_storage
                ? jcr->dir_impl->res.write_storage->resource_name_
                : T_("*None*"),
            bstrutime(dt, sizeof(dt), jcr->sched_time), jcr->JobPriority);
      } else {
        ua->SendMsg(
            T_("Run Archive Job\n"
               "JobName:  %s\n"
               "FileSet:  %s\n"
               "Client:   %s\n"
               "Storage:  %s\n"
               "When:     %s\n"
               "Priority: %d\n"),
            job->resource_name_, jcr->dir_impl->res.fileset->resource_name_,
            NPRT(jcr->dir_impl->res.client->resource_name_),
            jcr->dir_impl->res.write_storage
                ? jcr->dir_impl->res.write_storage->resource_name_
                : T_("*None*"),
            bstrutime(dt, sizeof(dt), jcr->sched_time), jcr->JobPriority);
      }
      jcr->setJobLevel(L_FULL);
      break;
    case JT_CONSOLIDATE:
      if (ua->api) {
        ua->signal(BNET_RUN_CMD);
        ua->SendMsg(
            "Type: Consolidate\n"
            "Title: Run Consolidate Job\n"
            "JobName:  %s\n"
            "FileSet:  %s\n"
            "Client:   %s\n"
            "Storage:  %s\n"
            "When:     %s\n"
            "Priority: %d\n",
            job->resource_name_, jcr->dir_impl->res.fileset->resource_name_,
            NPRT(jcr->dir_impl->res.client->resource_name_),
            jcr->dir_impl->res.write_storage
                ? jcr->dir_impl->res.write_storage->resource_name_
                : T_("*None*"),
            bstrutime(dt, sizeof(dt), jcr->sched_time), jcr->JobPriority);
      } else {
        ua->SendMsg(
            T_("Run Consolidate Job\n"
               "JobName:  %s\n"
               "FileSet:  %s\n"
               "Client:   %s\n"
               "Storage:  %s\n"
               "When:     %s\n"
               "Priority: %d\n"),
            job->resource_name_, jcr->dir_impl->res.fileset->resource_name_,
            NPRT(jcr->dir_impl->res.client->resource_name_),
            jcr->dir_impl->res.write_storage
                ? jcr->dir_impl->res.write_storage->resource_name_
                : T_("*None*"),
            bstrutime(dt, sizeof(dt), jcr->sched_time), jcr->JobPriority);
      }
      jcr->setJobLevel(L_FULL);
      break;
    case JT_BACKUP:
    case JT_VERIFY:
      if (jcr->is_JobType(JT_BACKUP)) {
        bool is_virtual = jcr->is_JobLevel(L_VIRTUAL_FULL);

        if (ua->api) {
          ua->signal(BNET_RUN_CMD);
          ua->SendMsg(
              "Type: Backup\n"
              "Title: Run Backup Job\n"
              "JobName:  %s\n"
              "Level:    %s\n"
              "Client:   %s\n"
              "Format:   %s\n"
              "FileSet:  %s\n"
              "Pool:     %s\n"
              "%s%s%s"
              "Storage:  %s\n"
              "When:     %s\n"
              "Priority: %d\n"
              "%s%s%s",
              job->resource_name_, JobLevelToString(jcr->getJobLevel()),
              jcr->dir_impl->res.client->resource_name_,
              jcr->dir_impl->backup_format,
              jcr->dir_impl->res.fileset->resource_name_,
              NPRT(jcr->dir_impl->res.pool->resource_name_),
              is_virtual ? "NextPool: " : "",
              is_virtual ? (jcr->dir_impl->res.next_pool
                                ? jcr->dir_impl->res.next_pool->resource_name_
                                : T_("*None*"))
                         : "",
              is_virtual ? "\n" : "",
              jcr->dir_impl->res.write_storage
                  ? jcr->dir_impl->res.write_storage->resource_name_
                  : T_("*None*"),
              bstrutime(dt, sizeof(dt), jcr->sched_time), jcr->JobPriority,
              jcr->dir_impl->plugin_options ? "Plugin Options: " : "",
              jcr->dir_impl->plugin_options ? jcr->dir_impl->plugin_options
                                            : "",
              jcr->dir_impl->plugin_options ? "\n" : "");
        } else {
          ua->SendMsg(
              T_("Run Backup job\n"
                 "JobName:  %s\n"
                 "Level:    %s\n"
                 "Client:   %s\n"
                 "Format:   %s\n"
                 "FileSet:  %s\n"
                 "Pool:     %s (From %s)\n"
                 "%s%s%s%s%s"
                 "Storage:  %s (From %s)\n"
                 "When:     %s\n"
                 "Priority: %d\n"
                 "%s%s%s"),
              job->resource_name_, JobLevelToString(jcr->getJobLevel()),
              jcr->dir_impl->res.client->resource_name_,
              jcr->dir_impl->backup_format,
              jcr->dir_impl->res.fileset->resource_name_,
              NPRT(jcr->dir_impl->res.pool->resource_name_),
              jcr->dir_impl->res.pool_source, is_virtual ? "NextPool: " : "",
              is_virtual ? (jcr->dir_impl->res.next_pool
                                ? jcr->dir_impl->res.next_pool->resource_name_
                                : T_("*None*"))
                         : "",
              is_virtual ? " (From " : "",
              is_virtual ? jcr->dir_impl->res.npool_source : "",
              is_virtual ? ")\n" : "",
              jcr->dir_impl->res.write_storage
                  ? jcr->dir_impl->res.write_storage->resource_name_
                  : T_("*None*"),
              jcr->dir_impl->res.wstore_source,
              bstrutime(dt, sizeof(dt), jcr->sched_time), jcr->JobPriority,
              jcr->dir_impl->plugin_options ? "Plugin Options: " : "",
              jcr->dir_impl->plugin_options ? jcr->dir_impl->plugin_options
                                            : "",
              jcr->dir_impl->plugin_options ? "\n" : "");
        }
      } else { /* JT_VERIFY */
        JobDbRecord jr;
        const char* Name;
        if (jcr->dir_impl->res.verify_job) {
          Name = jcr->dir_impl->res.verify_job->resource_name_;
        } else if (jcr->dir_impl->RestoreJobId) { /* Display job name if jobid
                                                   * requested
                                                   */
          jr.JobId = jcr->dir_impl->RestoreJobId;
          if (DbLocker _{ua->db}; !ua->db->GetJobRecord(jcr, &jr)) {
            ua->ErrorMsg(
                T_("Could not get job record for selected JobId. ERR=%s"),
                ua->db->strerror());
            return false;
          }
          Name = jr.Job;
        } else {
          Name = "";
        }
        if (ua->api) {
          ua->signal(BNET_RUN_CMD);
          ua->SendMsg(
              "Type: Verify\n"
              "Title: Run Verify Job\n"
              "JobName:     %s\n"
              "Level:       %s\n"
              "Client:      %s\n"
              "FileSet:     %s\n"
              "Pool:        %s (From %s)\n"
              "Storage:     %s (From %s)\n"
              "Verify Job:  %s\n"
              "When:        %s\n"
              "Priority:    %d\n",
              job->resource_name_, JobLevelToString(jcr->getJobLevel()),
              jcr->dir_impl->res.client->resource_name_,
              jcr->dir_impl->res.fileset->resource_name_,
              NPRT(jcr->dir_impl->res.pool->resource_name_),
              jcr->dir_impl->res.pool_source,
              jcr->dir_impl->res.read_storage->resource_name_,
              jcr->dir_impl->res.rstore_source, Name,
              bstrutime(dt, sizeof(dt), jcr->sched_time), jcr->JobPriority);
        } else {
          ua->SendMsg(T_("Run Verify Job\n"
                         "JobName:     %s\n"
                         "Level:       %s\n"
                         "Client:      %s\n"
                         "FileSet:     %s\n"
                         "Pool:        %s (From %s)\n"
                         "Storage:     %s (From %s)\n"
                         "Verify Job:  %s\n"
                         "When:        %s\n"
                         "Priority:    %d\n"),
                      job->resource_name_, JobLevelToString(jcr->getJobLevel()),
                      jcr->dir_impl->res.client->resource_name_,
                      jcr->dir_impl->res.fileset->resource_name_,
                      NPRT(jcr->dir_impl->res.pool->resource_name_),
                      jcr->dir_impl->res.pool_source,
                      jcr->dir_impl->res.read_storage->resource_name_,
                      jcr->dir_impl->res.rstore_source, Name,
                      bstrutime(dt, sizeof(dt), jcr->sched_time),
                      jcr->JobPriority);
        }
      }
      break;
    case JT_RESTORE:
      if (jcr->dir_impl->RestoreJobId == 0 && !jcr->RestoreBootstrap) {
        if (rc.jid) {
          jcr->dir_impl->RestoreJobId = str_to_int64(rc.jid);
        } else {
          if (!GetPint(ua, T_("Please enter a JobId for restore: "))) {
            return false;
          }
          std::string jobIdInput = std::to_string(ua->int64_val);
          if (!ua->db->FindJobById(jcr, jobIdInput)) {
            ua->SendMsg("JobId %s not found in catalog. \n",
                        jobIdInput.c_str());
            return false;
          }

          jcr->dir_impl->RestoreJobId = ua->int64_val;
        }
      }
      jcr->setJobLevel(L_FULL); /* default level */
      Dmsg1(800, "JobId to restore=%" PRIu32 "\n", jcr->dir_impl->RestoreJobId);
      if (jcr->dir_impl->RestoreJobId == 0) {
        /* RegexWhere is take before RestoreWhere */
        if (jcr->RegexWhere || (job->RegexWhere && !jcr->where)) {
          if (ua->api) {
            ua->signal(BNET_RUN_CMD);
            ua->SendMsg(
                "Type: Restore\n"
                "Title: Run Restore Job\n"
                "JobName:         %s\n"
                "Bootstrap:       %s\n"
                "RegexWhere:      %s\n"
                "Replace:         %s\n"
                "FileSet:         %s\n"
                "Backup Client:   %s\n"
                "Restore Client:  %s\n"
                "Format:          %s\n"
                "Storage:         %s\n"
                "When:            %s\n"
                "Catalog:         %s\n"
                "Priority:        %d\n"
                "Plugin Options:  %s\n",
                job->resource_name_, NPRT(jcr->RestoreBootstrap),
                jcr->RegexWhere ? jcr->RegexWhere : job->RegexWhere, rc.replace,
                jcr->dir_impl->res.fileset->resource_name_, rc.client_name,
                jcr->dir_impl->res.client->resource_name_,
                jcr->dir_impl->backup_format,
                jcr->dir_impl->res.read_storage->resource_name_,
                bstrutime(dt, sizeof(dt), jcr->sched_time),
                jcr->dir_impl->res.catalog->resource_name_, jcr->JobPriority,
                NPRT(jcr->dir_impl->plugin_options));
          } else {
            ua->SendMsg(T_("Run Restore job\n"
                           "JobName:         %s\n"
                           "Bootstrap:       %s\n"
                           "RegexWhere:      %s\n"
                           "Replace:         %s\n"
                           "FileSet:         %s\n"
                           "Backup Client:   %s\n"
                           "Restore Client:  %s\n"
                           "Format:          %s\n"
                           "Storage:         %s\n"
                           "When:            %s\n"
                           "Catalog:         %s\n"
                           "Priority:        %d\n"
                           "Plugin Options:  %s\n"),
                        job->resource_name_, NPRT(jcr->RestoreBootstrap),
                        jcr->RegexWhere ? jcr->RegexWhere : job->RegexWhere,
                        rc.replace, jcr->dir_impl->res.fileset->resource_name_,
                        rc.client_name,
                        jcr->dir_impl->res.client->resource_name_,
                        jcr->dir_impl->backup_format,
                        jcr->dir_impl->res.read_storage->resource_name_,
                        bstrutime(dt, sizeof(dt), jcr->sched_time),
                        jcr->dir_impl->res.catalog->resource_name_,
                        jcr->JobPriority, NPRT(jcr->dir_impl->plugin_options));
          }
        } else {
          if (ua->api) {
            ua->signal(BNET_RUN_CMD);
            ua->SendMsg(
                "Type: Restore\n"
                "Title: Run Restore job\n"
                "JobName:         %s\n"
                "Bootstrap:       %s\n"
                "Where:           %s\n"
                "Replace:         %s\n"
                "FileSet:         %s\n"
                "Backup Client:   %s\n"
                "Restore Client:  %s\n"
                "Format:          %s\n"
                "Storage:         %s\n"
                "When:            %s\n"
                "Catalog:         %s\n"
                "Priority:        %d\n"
                "Plugin Options:  %s\n",
                job->resource_name_, NPRT(jcr->RestoreBootstrap),
                jcr->where ? jcr->where : NPRT(job->RestoreWhere), rc.replace,
                jcr->dir_impl->res.fileset->resource_name_, rc.client_name,
                jcr->dir_impl->res.client->resource_name_,
                jcr->dir_impl->backup_format,
                jcr->dir_impl->res.read_storage->resource_name_,
                bstrutime(dt, sizeof(dt), jcr->sched_time),
                jcr->dir_impl->res.catalog->resource_name_, jcr->JobPriority,
                NPRT(jcr->dir_impl->plugin_options));
          } else {
            ua->SendMsg(T_("Run Restore job\n"
                           "JobName:         %s\n"
                           "Bootstrap:       %s\n"
                           "Where:           %s\n"
                           "Replace:         %s\n"
                           "FileSet:         %s\n"
                           "Backup Client:   %s\n"
                           "Restore Client:  %s\n"
                           "Format:          %s\n"
                           "Storage:         %s\n"
                           "When:            %s\n"
                           "Catalog:         %s\n"
                           "Priority:        %d\n"
                           "Plugin Options:  %s\n"),
                        job->resource_name_, NPRT(jcr->RestoreBootstrap),
                        jcr->where ? jcr->where : NPRT(job->RestoreWhere),
                        rc.replace, jcr->dir_impl->res.fileset->resource_name_,
                        rc.client_name,
                        jcr->dir_impl->res.client->resource_name_,
                        jcr->dir_impl->backup_format,
                        jcr->dir_impl->res.read_storage->resource_name_,
                        bstrutime(dt, sizeof(dt), jcr->sched_time),
                        jcr->dir_impl->res.catalog->resource_name_,
                        jcr->JobPriority, NPRT(jcr->dir_impl->plugin_options));
          }
        }

      } else {
        if (ua->api) ua->signal(BNET_RUN_CMD);
        ua->SendMsg(T_("Run Restore job\n"
                       "JobName:         %s\n"
                       "Bootstrap:       %s\n"),
                    job->resource_name_, NPRT(jcr->RestoreBootstrap));

        /* RegexWhere is take before RestoreWhere */
        if (jcr->RegexWhere || (job->RegexWhere && !jcr->where)) {
          ua->SendMsg(T_("RegexWhere:      %s\n"),
                      jcr->RegexWhere ? jcr->RegexWhere : job->RegexWhere);
        } else {
          ua->SendMsg(T_("Where:           %s\n"),
                      jcr->where ? jcr->where : NPRT(job->RestoreWhere));
        }

        ua->SendMsg(T_("Replace:         %s\n"
                       "Client:          %s\n"
                       "Format:          %s\n"
                       "Storage:         %s\n"
                       "JobId:           %s\n"
                       "When:            %s\n"
                       "Catalog:         %s\n"
                       "Priority:        %d\n"
                       "Plugin Options:  %s\n"),
                    rc.replace, jcr->dir_impl->res.client->resource_name_,
                    jcr->dir_impl->backup_format,
                    jcr->dir_impl->res.read_storage->resource_name_,
                    (jcr->dir_impl->RestoreJobId == 0)
                        ? T_("*None*")
                        : edit_uint64(jcr->dir_impl->RestoreJobId, ec1),
                    bstrutime(dt, sizeof(dt), jcr->sched_time),
                    jcr->dir_impl->res.catalog->resource_name_,
                    jcr->JobPriority, NPRT(jcr->dir_impl->plugin_options));
      }
      break;
    case JT_COPY:
    case JT_MIGRATE:
      const char* prt_type;

      jcr->setJobLevel(L_FULL); /* default level */
      if (ua->api) {
        ua->signal(BNET_RUN_CMD);
        if (jcr->is_JobType(JT_COPY)) {
          prt_type = T_("Type: Copy\nTitle: Run Copy Job\n");
        } else {
          prt_type = T_("Type: Migration\nTitle: Run Migration Job\n");
        }
        ua->SendMsg(
            "%s"
            "JobName:       %s\n"
            "Bootstrap:     %s\n"
            "Read Storage:  %s\n"
            "Pool:          %s\n"
            "Write Storage: %s\n"
            "NextPool:      %s\n"
            "JobId:         %s\n"
            "When:          %s\n"
            "Catalog:       %s\n"
            "Priority:      %d\n",
            prt_type, job->resource_name_, NPRT(jcr->RestoreBootstrap),
            jcr->dir_impl->res.read_storage
                ? jcr->dir_impl->res.read_storage->resource_name_
                : T_("*None*"),
            NPRT(jcr->dir_impl->res.pool->resource_name_),
            jcr->dir_impl->res.next_pool
                ? jcr->dir_impl->res.next_pool->resource_name_
                : T_("*None*"),
            jcr->dir_impl->res.write_storage
                ? jcr->dir_impl->res.write_storage->resource_name_
                : T_("*None*"),
            (jcr->dir_impl->MigrateJobId == 0)
                ? T_("*None*")
                : edit_uint64(jcr->dir_impl->MigrateJobId, ec1),
            bstrutime(dt, sizeof(dt), jcr->sched_time),
            jcr->dir_impl->res.catalog->resource_name_, jcr->JobPriority);
      } else {
        if (jcr->is_JobType(JT_COPY)) {
          prt_type = T_("Run Copy job\n");
        } else {
          prt_type = T_("Run Migration job\n");
        }
        ua->SendMsg(T_("%s"
                       "JobName:       %s\n"
                       "Bootstrap:     %s\n"
                       "Read Storage:  %s (From %s)\n"
                       "Pool:          %s (From %s)\n"
                       "Write Storage: %s (From %s)\n"
                       "NextPool:      %s (From %s)\n"
                       "JobId:         %s\n"
                       "When:          %s\n"
                       "Catalog:       %s\n"
                       "Priority:      %d\n"),
                    prt_type, job->resource_name_, NPRT(jcr->RestoreBootstrap),
                    jcr->dir_impl->res.read_storage
                        ? jcr->dir_impl->res.read_storage->resource_name_
                        : T_("*None*"),
                    jcr->dir_impl->res.rstore_source,
                    NPRT(jcr->dir_impl->res.pool->resource_name_),
                    jcr->dir_impl->res.pool_source,
                    jcr->dir_impl->res.write_storage
                        ? jcr->dir_impl->res.write_storage->resource_name_
                        : T_("*None*"),
                    jcr->dir_impl->res.wstore_source,
                    jcr->dir_impl->res.next_pool
                        ? jcr->dir_impl->res.next_pool->resource_name_
                        : T_("*None*"),
                    NPRT(jcr->dir_impl->res.npool_source),
                    jcr->dir_impl->MigrateJobId == 0
                        ? T_("*None*")
                        : edit_uint64(jcr->dir_impl->MigrateJobId, ec1),
                    bstrutime(dt, sizeof(dt), jcr->sched_time),
                    jcr->dir_impl->res.catalog->resource_name_,
                    jcr->JobPriority);
      }
      break;
    default:
      ua->ErrorMsg(T_("Unknown Job Type=%d\n"), jcr->getJobType());
      return false;
  }
  return true;
}

static bool ScanCommandLineArguments(UaContext* ua, RunContext& rc)
{
  bool kw_ok;
  int i, j;
  static const char* kw[]
      = {                 /* command line arguments */
         "job",           /* 0 */
         "jobid",         /* 1 */
         "client",        /* 2 */
         "fd",            /* 3 */
         "fileset",       /* 4 */
         "level",         /* 5 */
         "storage",       /* 6 */
         "sd",            /* 7 */
         "regexwhere",    /* 8 - where string as a bregexp */
         "where",         /* 9 */
         "bootstrap",     /* 10 */
         "replace",       /* 11 */
         "when",          /* 12 */
         "priority",      /* 13 */
         "yes",           /* 14 -- if you change this change YES_POS too */
         "verifyjob",     /* 15 */
         "files",         /* 16 - number of files to restore */
         "catalog",       /* 17 - override catalog */
         "since",         /* 18 - since */
         "cloned",        /* 19 - cloned */
         "migrationjob",  /* 20 - migration job name */
         "pool",          /* 21 */
         "nextpool",      /* 22 */
         "backupclient",  /* 23 */
         "restoreclient", /* 24 */
         "pluginoptions", /* 25 */
         "spooldata",     /* 26 */
         "comment",       /* 27 */
         "ignoreduplicatecheck", /* 28 */
         "accurate",             /* 29 */
         "backupformat",         /* 30 */
         "consolidatejob",       /* 31 - parent consolidate job */
         NULL};

#define YES_POS 14

  rc.catalog_name = NULL;
  rc.job_name = NULL;
  rc.pool_name = NULL;
  rc.next_pool_name = NULL;
  rc.StoreName = NULL;
  rc.client_name = NULL;
  rc.restore_client_name = NULL;
  rc.fileset_name = NULL;
  rc.verify_job_name = NULL;
  rc.previous_job_name = NULL;
  rc.accurate_set = false;
  rc.spool_data_set = false;
  rc.ignoreduplicatecheck = false;
  rc.comment = NULL;
  rc.backup_format = NULL;

  char* consolidate_job_name = nullptr;

  for (i = 1; i < ua->argc; i++) {
    Dmsg2(800, "Doing arg %d = %s\n", i, ua->argk[i]);
    kw_ok = false;

    // Keep looking until we find a good keyword
    for (j = 0; !kw_ok && kw[j]; j++) {
      if (Bstrcasecmp(ua->argk[i], kw[j])) {
        // Note, yes and run have no value, so do not fail
        if (!ua->argv[i] && j != YES_POS /*yes*/) {
          ua->SendMsg(T_("Value missing for keyword %s\n"), ua->argk[i]);
          return false;
        }
        Dmsg1(800, "Got keyword=%s\n", NPRT(kw[j]));
        switch (j) {
          case 0: /* job */
            if (rc.job_name) {
              ua->SendMsg(T_("Job name specified twice.\n"));
              return false;
            }
            rc.job_name = ua->argv[i];
            kw_ok = true;
            break;
          case 1: /* JobId */ {
            if (rc.jid && !rc.mod) {
              ua->SendMsg(T_("JobId specified twice.\n"));
              return false;
            }
            rc.jid = ua->argv[i];
            std::vector jobIdList = split_string(rc.jid, ',');
            for (const auto& jobId : jobIdList) {
              if (!ua->db->FindJobById(ua->jcr, jobId)) {
                ua->SendMsg("JobId %s not found in catalog. \n", jobId.c_str());
                return false;
              }
            }

            kw_ok = true;
            break;
          }
          case 2: /* client */
          case 3: /* fd */
            if (rc.client_name) {
              ua->SendMsg(T_("Client specified twice.\n"));
              return false;
            }
            rc.client_name = ua->argv[i];
            kw_ok = true;
            break;
          case 4: /* fileset */
            if (rc.fileset_name) {
              ua->SendMsg(T_("FileSet specified twice.\n"));
              return false;
            }
            rc.fileset_name = ua->argv[i];
            kw_ok = true;
            break;
          case 5: /* level */
            if (rc.level_name) {
              ua->SendMsg(T_("Level specified twice.\n"));
              return false;
            }
            rc.level_name = ua->argv[i];
            kw_ok = true;
            break;
          case 6: /* storage */
          case 7: /* sd */
            if (rc.StoreName) {
              ua->SendMsg(T_("Storage specified twice.\n"));
              return false;
            }
            rc.StoreName = ua->argv[i];
            kw_ok = true;
            break;
          case 8: /* regexwhere */
            if ((rc.regexwhere || rc.where) && !rc.mod) {
              ua->SendMsg(T_("RegexWhere or Where specified twice.\n"));
              return false;
            }
            rc.regexwhere = ua->argv[i];
            if (!ua->AclAccessOk(Where_ACL, rc.regexwhere, true)) {
              ua->SendMsg(
                  T_("No authorization for \"regexwhere\" specification.\n"));
              return false;
            }
            kw_ok = true;
            break;
          case 9: /* where */
            if ((rc.where || rc.regexwhere) && !rc.mod) {
              ua->SendMsg(T_("Where or RegexWhere specified twice.\n"));
              return false;
            }
            rc.where = ua->argv[i];
            if (!ua->AclAccessOk(Where_ACL, rc.where, true)) {
              ua->SendMsg(
                  T_("No authorization for \"where\" specification.\n"));
              return false;
            }
            kw_ok = true;
            break;
          case 10: /* bootstrap */
            if (rc.bootstrap && !rc.mod) {
              ua->SendMsg(T_("Bootstrap specified twice.\n"));
              return false;
            }
            rc.bootstrap = ua->argv[i];
            kw_ok = true;
            break;
          case 11: /* replace */
            if (rc.replace && !rc.mod) {
              ua->SendMsg(T_("Replace specified twice.\n"));
              return false;
            }
            rc.replace = ua->argv[i];
            kw_ok = true;
            break;
          case 12: /* When */
            if (rc.when && !rc.mod) {
              ua->SendMsg(T_("When specified twice.\n"));
              return false;
            }
            rc.when = ua->argv[i];
            kw_ok = true;
            break;
          case 13: /* Priority */
            if (rc.Priority && !rc.mod) {
              ua->SendMsg(T_("Priority specified twice.\n"));
              return false;
            }
            rc.Priority = atoi(ua->argv[i]);
            if (rc.Priority <= 0) {
              ua->SendMsg(
                  T_("Priority must be positive nonzero setting it to 10.\n"));
              rc.Priority = 10;
            }
            kw_ok = true;
            break;
          case 14: /* yes */
            kw_ok = true;
            break;
          case 15: /* Verify Job */
            if (rc.verify_job_name) {
              ua->SendMsg(T_("Verify Job specified twice.\n"));
              return false;
            }
            rc.verify_job_name = ua->argv[i];
            kw_ok = true;
            break;
          case 16: /* files */
            rc.files = atoi(ua->argv[i]);
            kw_ok = true;
            break;
          case 17: /* catalog */
            rc.catalog_name = ua->argv[i];
            kw_ok = true;
            break;
          case 18: /* since */
            rc.since = ua->argv[i];
            kw_ok = true;
            break;
          case 19: /* cloned */
            rc.cloned = true;
            kw_ok = true;
            break;
          case 20: /* Migration Job */
            if (rc.previous_job_name) {
              ua->SendMsg(T_("Migration Job specified twice.\n"));
              return false;
            }
            rc.previous_job_name = ua->argv[i];
            kw_ok = true;
            break;
          case 21: /* pool */
            if (rc.pool_name) {
              ua->SendMsg(T_("Pool specified twice.\n"));
              return false;
            }
            rc.pool_name = ua->argv[i];
            kw_ok = true;
            break;
          case 22: /* nextpool */
            if (rc.next_pool_name) {
              ua->SendMsg(T_("NextPool specified twice.\n"));
              return false;
            }
            rc.next_pool_name = ua->argv[i];
            kw_ok = true;
            break;
          case 23: /* backupclient */
            if (rc.client_name) {
              ua->SendMsg(T_("Client specified twice.\n"));
              return 0;
            }
            rc.client_name = ua->argv[i];
            kw_ok = true;
            break;
          case 24: /* restoreclient */
            if (rc.restore_client_name && !rc.mod) {
              ua->SendMsg(T_("Restore Client specified twice.\n"));
              return false;
            }
            rc.restore_client_name = ua->argv[i];
            kw_ok = true;
            break;
          case 25: /* pluginoptions */
            if (rc.plugin_options) {
              ua->SendMsg(T_("Plugin Options specified twice.\n"));
              return false;
            }
            rc.plugin_options = ua->argv[i];
            // See GetPluginOptions() above: authorize every block
            // individually, not the whole (possibly multi-block)
            // string at once.
            if (!restore_plugin_hints::AllPluginOptionsBlocksAuthorized(
                    rc.plugin_options, [ua](const std::string& block) {
                      return ua->AclAccessOk(PluginOptions_ACL, block.c_str(),
                                             true);
                    })) {
              ua->SendMsg(T_(
                  "No authorization for \"PluginOptions\" specification.\n"));
              return false;
            }
            kw_ok = true;
            break;
          case 26: /* spooldata */
            if (rc.spool_data_set) {
              ua->SendMsg(T_("Spool flag specified twice.\n"));
              return false;
            }
            switch (parse_user_bool(ua->argv[i])) {
              case parse_bool_result::True: {
                rc.spool_data_set = true;
                rc.spool_data = true;
                kw_ok = true;
              } break;
              case parse_bool_result::False: {
                rc.spool_data_set = true;
                rc.spool_data = false;
                kw_ok = true;
              } break;
              case parse_bool_result::Error: {
                ua->SendMsg(T_("Invalid spooldata flag.\n"));
              } break;
            }
            break;
          case 27: /* comment */
            rc.comment = ua->argv[i];
            kw_ok = true;
            break;
          case 28: /* ignoreduplicatecheck */
            if (rc.ignoreduplicatecheck_set) {
              ua->SendMsg(T_("IgnoreDuplicateCheck flag specified twice.\n"));
              return false;
            }
            switch (parse_user_bool(ua->argv[i])) {
              case parse_bool_result::True: {
                rc.ignoreduplicatecheck_set = true;
                rc.ignoreduplicatecheck = true;
                kw_ok = true;
              } break;
              case parse_bool_result::False: {
                rc.ignoreduplicatecheck_set = true;
                rc.ignoreduplicatecheck = false;
                kw_ok = true;
              } break;
              case parse_bool_result::Error: {
                ua->SendMsg(T_("Invalid ignoreduplicatecheck flag.\n"));
              } break;
            }
            break;
          case 29: /* accurate */
            if (rc.accurate_set) {
              ua->SendMsg(T_("Accurate flag specified twice.\n"));
              return false;
            }
            switch (parse_user_bool(ua->argv[i])) {
              case parse_bool_result::True: {
                rc.accurate_set = true;
                kw_ok = true;
                rc.accurate = true;
              } break;
              case parse_bool_result::False: {
                rc.accurate_set = true;
                kw_ok = true;
                rc.accurate = false;
              } break;
              case parse_bool_result::Error: {
                ua->SendMsg(T_("Invalid accurate flag.\n"));
              } break;
            }
            break;
          case 30: /* backupformat */
            if (rc.backup_format && !rc.mod) {
              ua->SendMsg(T_("Backup Format specified twice.\n"));
              return false;
            }
            rc.backup_format = ua->argv[i];
            kw_ok = true;
            break;
          case 31: /* consolidatejob */
            if (consolidate_job_name) {
              ua->SendMsg(T_("Consolidate Job specified twice.\n"));
              return false;
            }
            consolidate_job_name = ua->argv[i];
            kw_ok = true;
            break;
          default:
            break;
        }
      } /* end strcase compare */
    } /* end keyword loop */

    // End of keyword for loop -- if not found, we got a bogus keyword
    if (!kw_ok) {
      Dmsg1(800, "%s not found\n", ua->argk[i]);
      /* Special case for Job Name, it can be the first
       * keyword that has no value. */
      if (!rc.job_name && !ua->argv[i]) {
        rc.job_name = ua->argk[i]; /* use keyword as job name */
        Dmsg1(800, "Set jobname=%s\n", rc.job_name);
      } else {
        ua->SendMsg(T_("Invalid keyword: %s\n"), ua->argk[i]);
        return false;
      }
    }
  } /* end argc loop */

  Dmsg0(800, "Done scan.\n");
  if (rc.comment) {
    if (!IsCommentLegal(ua, rc.comment)) { return false; }
  }
  if (rc.catalog_name) {
    rc.catalog = ua->GetCatalogResWithName(rc.catalog_name);
    if (rc.catalog == NULL) {
      ua->ErrorMsg(T_("Catalog \"%s\" not found\n"), rc.catalog_name);
      return false;
    }
  }
  Dmsg1(800, "Using catalog=%s\n", NPRT(rc.catalog_name));

  if (rc.job_name) {
    /* Find Job */
    rc.job = ua->GetJobResWithName(rc.job_name);
    if (!rc.job) {
      if (*rc.job_name != 0) {
        ua->SendMsg(T_("Job \"%s\" not found\n"), rc.job_name);
      }
      rc.job = select_job_resource(ua);
    } else {
      Dmsg1(800, "Found job=%s\n", rc.job_name);
    }
  } else if (!rc.job) {
    ua->SendMsg(T_("A job name must be specified.\n"));
    rc.job = select_job_resource(ua);
  }
  if (!rc.job) { return false; }

  if (rc.pool_name) {
    rc.pool = ua->GetPoolResWithName(rc.pool_name);
    if (!rc.pool) {
      if (*rc.pool_name != 0) {
        ua->WarningMsg(T_("Pool \"%s\" not found.\n"), rc.pool_name);
      }
      rc.pool = select_pool_resource(ua);
    }
  } else if (!rc.pool) {
    rc.pool = rc.job->pool; /* use default */
  }
  if (!rc.pool) { return false; }
  Dmsg1(100, "Using pool %s\n", rc.pool->resource_name_);

  if (rc.next_pool_name) {
    rc.next_pool = ua->GetPoolResWithName(rc.next_pool_name);
    if (!rc.next_pool) {
      if (*rc.next_pool_name != 0) {
        ua->WarningMsg(T_("Pool \"%s\" not found.\n"), rc.next_pool_name);
      }
      rc.next_pool = select_pool_resource(ua);
    }
  } else if (!rc.next_pool) {
    // use next pool from job
    if (rc.job->next_pool) {
      rc.next_pool = rc.job->next_pool;
    }
    // use next pool from pool
    else {
      rc.next_pool = rc.pool->NextPool;
    }
  }
  if (rc.next_pool) {
    Dmsg1(100, "Using next pool %s\n", rc.next_pool->resource_name_);
  }

  if (rc.StoreName) {
    rc.store->store = ua->GetStoreResWithName(rc.StoreName);
    PmStrcpy(rc.store->store_source, T_("command line"));
    if (!rc.store->store) {
      if (*rc.StoreName != 0) {
        ua->WarningMsg(T_("Storage \"%s\" not found.\n"), rc.StoreName);
      }
      rc.store->store = select_storage_resource(ua);
      PmStrcpy(rc.store->store_source, T_("user selection"));
    }
  } else if (!rc.store->store) {
    GetJobStorage(rc.store, rc.job, NULL); /* use default */
  }

  /* For certain Jobs an explicit setting of the read storage is not
   * required as its determined when the Job is executed automatically. */
  switch (rc.job->JobType) {
    case JT_COPY:
    case JT_MIGRATE:
      break;
    default:
      if (!rc.store->store) {
        ua->ErrorMsg(T_("No storage specified.\n"));
        return false;
      } else if (!ua->AclAccessOk(Storage_ACL, rc.store->store->resource_name_,
                                  true)) {
        ua->ErrorMsg(T_("No authorization. Storage \"%s\".\n"),
                     rc.store->store->resource_name_);
        return false;
      }
      Dmsg1(800, "Using storage=%s\n", rc.store->store->resource_name_);
      break;
  }

  if (rc.client_name) {
    rc.client = ua->GetClientResWithName(rc.client_name);
    if (!rc.client) {
      // Running backup jobs can only be performed on clients with a
      // configuration resource. However, a restore job should also work from
      // clients without configuration resource (client has been deleted from
      // the configuration, but can still be found in the catalog database).
      if (rc.job->JobType == JT_RESTORE) {
        ClientDbRecord cr;
        bstrncpy(cr.Name, rc.client_name, sizeof(cr.Name));
        if ((!ua->db->GetClientRecord(ua->jcr, &cr))
            || (!ua->AclAccessOk(Client_ACL, cr.Name, true))) {
          ua->ErrorMsg(T_("Client \"%s\" not found.\n"), rc.client_name);
          return false;
        }
      } else {
        if (*rc.client_name != 0) {
          ua->WarningMsg(T_("Client \"%s\" not found.\n"), rc.client_name);
        }
        rc.client = select_client_resource(ua);
        if (!rc.client) { return false; }
      }
    }
  } else if (!rc.client) {
    rc.client = rc.job->client; /* use default */
  }

  if (rc.client) {
    if (!ua->AclAccessOk(Client_ACL, rc.client->resource_name_, true)) {
      ua->ErrorMsg(T_("No authorization. Client \"%s\".\n"),
                   rc.client->resource_name_);
      return false;
    } else {
      Dmsg1(800, "Using client=%s\n", rc.client->resource_name_);
    }
  }

  if (rc.restore_client_name) {
    rc.client = ua->GetClientResWithName(rc.restore_client_name);
    if (!rc.client) {
      if (*rc.restore_client_name != 0) {
        ua->WarningMsg(T_("Restore Client \"%s\" not found.\n"),
                       rc.restore_client_name);
      }
      rc.client = select_client_resource(ua);
      if (!rc.client) { return false; }
    }
  } else if (!rc.client) {
    rc.client = rc.job->client; /* use default */
  }

  if (rc.client) {
    if (!ua->AclAccessOk(Client_ACL, rc.client->resource_name_, true)) {
      ua->ErrorMsg(T_("No authorization. Client \"%s\".\n"),
                   rc.client->resource_name_);
      return false;
    } else {
      Dmsg1(800, "Using restore client=%s\n", rc.client->resource_name_);
    }
  }

  if (rc.fileset_name) {
    rc.fileset = ua->GetFileSetResWithName(rc.fileset_name);
    if (!rc.fileset) {
      ua->SendMsg(T_("FileSet \"%s\" not found.\n"), rc.fileset_name);
      rc.fileset = select_fileset_resource(ua);
      if (!rc.fileset) { return false; }
    }
  } else if (!rc.fileset) {
    rc.fileset = rc.job->fileset; /* use default */
  }

  if (rc.verify_job_name) {
    rc.verify_job = ua->GetJobResWithName(rc.verify_job_name);
    if (!rc.verify_job) {
      ua->SendMsg(T_("Verify Job \"%s\" not found.\n"), rc.verify_job_name);
      rc.verify_job = select_job_resource(ua);
    }
  } else if (!rc.verify_job) {
    rc.verify_job = rc.job->verify_job;
  }

  if (rc.previous_job_name) {
    rc.previous_job = ua->GetJobResWithName(rc.previous_job_name);
    if (!rc.previous_job) {
      ua->SendMsg(T_("Migration Job \"%s\" not found.\n"),
                  rc.previous_job_name);
      rc.previous_job = select_job_resource(ua);
    }
  } else {
    rc.previous_job = rc.job->verify_job;
  }

  if (consolidate_job_name) {
    rc.consolidate_job = ua->GetJobResWithName(consolidate_job_name);
    if (!rc.consolidate_job) {
      ua->SendMsg(T_("Consolidate Job \"%s\" not found.\n"),
                  consolidate_job_name);
      rc.consolidate_job = select_job_resource(ua);
    }
    if (rc.consolidate_job && rc.consolidate_job->JobType != JT_CONSOLIDATE) {
      ua->ErrorMsg(T_("Invalid Consolidate Job \"%s\". Job type is \"%c\" but "
                      "expected \"%c\".\n"),
                   rc.consolidate_job->resource_name_,
                   static_cast<int>(rc.consolidate_job->JobType),
                   static_cast<int>(JT_CONSOLIDATE));
      return false;
    }
  }

  return true;
}
} /* namespace directordaemon */
