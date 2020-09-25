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
 * Kern Sibbald, September MM
 */
/**
 * @file
 * User Agent Output Commands
 *
 * I.e. messages, listing database, showing resources, ...
 */

#include "include/bareos.h"
#include "dird.h"
#include "dird/dird_globals.h"
#include "dird/get_database_connection.h"
#include "dird/jcr_private.h"
#include "dird/job.h"
#include "dird/ua_cmdstruct.h"
#include "cats/sql_pooling.h"
#include "dird/next_vol.h"
#include "dird/ua_db.h"
#include "dird/ua_output.h"
#include "dird/ua_select.h"
#include "dird/show_cmd_available_resources.h"
#include "lib/edit.h"
#include "lib/parse_conf.h"

namespace directordaemon {

/* Imported subroutines */

/* Imported variables */
extern struct s_jl joblevels[];

/* Imported functions */

/* Forward referenced functions */
static bool DoListCmd(UaContext* ua, const char* cmd, e_list_type llist);
static bool ListNextvol(UaContext* ua, int ndays);
static bool ParseListBackupsCmd(UaContext* ua,
                                const char* range,
                                e_list_type llist);

/**
 * Some defaults.
 */
#define DEFAULT_LOG_LINES 5
#define DEFAULT_NR_DAYS 50

/**
 * Turn auto display of console messages on/off
 */
bool AutodisplayCmd(UaContext* ua, const char* cmd)
{
  static const char* kw[] = {NT_("on"), NT_("off"), NULL};

  switch (FindArgKeyword(ua, kw)) {
    case 0:
      ua->auto_display_messages = true;
      break;
    case 1:
      ua->auto_display_messages = false;
      break;
    default:
      ua->ErrorMsg(_("ON or OFF keyword missing.\n"));
      break;
  }
  return true;
}

/**
 * Turn GUI mode on/off
 */
bool gui_cmd(UaContext* ua, const char* cmd)
{
  static const char* kw[] = {NT_("on"), NT_("off"), NULL};

  switch (FindArgKeyword(ua, kw)) {
    case 0:
      ua->jcr->gui = ua->gui = true;
      break;
    case 1:
      ua->jcr->gui = ua->gui = false;
      break;
    default:
      ua->ErrorMsg(_("ON or OFF keyword missing.\n"));
      break;
  }
  return true;
}

/**
 * Enter with Resources locked
 */
static void ShowDisabledJobs(UaContext* ua)
{
  JobResource* job;
  bool first = true;

  ua->send->ArrayStart("jobs");
  foreach_res (job, R_JOB) {
    if (!ua->AclAccessOk(Job_ACL, job->resource_name_, false)) { continue; }

    if (!job->enabled) {
      if (first) {
        first = false;
        ua->send->Decoration(_("Disabled Jobs:\n"));
      }
      ua->send->ArrayItem(job->resource_name_, "   %s\n");
    }
  }

  if (first) { ua->send->Decoration(_("No disabled Jobs.\n")); }
  ua->send->ArrayEnd("jobs");
}

/**
 * Enter with Resources locked
 */
static void ShowDisabledClients(UaContext* ua)
{
  ClientResource* client;
  bool first = true;

  ua->send->ArrayStart("clients");
  foreach_res (client, R_CLIENT) {
    if (!ua->AclAccessOk(Client_ACL, client->resource_name_, false)) {
      continue;
    }

    if (!client->enabled) {
      if (first) {
        first = false;
        ua->send->Decoration(_("Disabled Clients:\n"));
      }
      ua->send->ArrayItem(client->resource_name_, "   %s\n");
    }
  }

  if (first) { ua->send->Decoration(_("No disabled Clients.\n")); }
  ua->send->ArrayEnd("clients");
}

/**
 * Enter with Resources locked
 */
static void ShowDisabledSchedules(UaContext* ua)
{
  ScheduleResource* sched;
  bool first = true;

  ua->send->ArrayStart("schedules");
  foreach_res (sched, R_SCHEDULE) {
    if (!ua->AclAccessOk(Schedule_ACL, sched->resource_name_, false)) {
      continue;
    }

    if (!sched->enabled) {
      if (first) {
        first = false;
        ua->send->Decoration(_("Disabled Schedules:\n"));
      }
      ua->send->ArrayItem(sched->resource_name_, "   %s\n");
    }
  }

  if (first) { ua->send->Decoration(_("No disabled Schedules.\n")); }
  ua->send->ArrayEnd("schedules");
}

/**
 * Enter with Resources locked
 */
static void ShowAll(UaContext* ua, bool hide_sensitive_data, bool verbose)
{
  for (int j = my_config->r_first_; j <= my_config->r_last_; j++) {
    switch (j) {
      case R_DEVICE:
        /*
         * Skip R_DEVICE since it is really not used or updated
         */
        continue;
      default:
        if (my_config->res_head_[j - my_config->r_first_]) {
          my_config->DumpResourceCb_(
              j, my_config->res_head_[j - my_config->r_first_], bsendmsg, ua,
              hide_sensitive_data, verbose);
        }
        break;
    }
  }
}

/**
 *  Displays Resources
 *
 *  show
 *  show all
 *  show <resource-keyword-name> - e.g. show directors
 *  show <resource-keyword-name>=<name> - e.g. show director=HeadMan
 *  show disabled - shows disabled jobs, clients and schedules
 *  show disabled jobs - shows disabled jobs
 *  show disabled clients - shows disabled clients
 *  show disabled schedules - shows disabled schedules
 */
bool show_cmd(UaContext* ua, const char* cmd)
{
  int i, j, type, len;
  int recurse;
  char* res_name;
  BareosResource* res = NULL;
  bool verbose = false;
  bool hide_sensitive_data;

  Dmsg1(20, "show: %s\n", ua->UA_sock->msg);

  /*
   * When the console has no access to the configure cmd then any show cmd
   * will suppress all sensitive information like for instance passwords.
   */
  hide_sensitive_data = !ua->AclAccessOk(Command_ACL, "configure", false);

  if (FindArg(ua, NT_("verbose")) > 0) { verbose = true; }

  LockRes(my_config);

  /*
   * Without parameter, show all ressources.
   */
  if (ua->argc == 1) { ShowAll(ua, hide_sensitive_data, verbose); }

  for (i = 1; i < ua->argc; i++) {
    /*
     * skip verbose keyword, already handled earlier.
     */
    if (Bstrcasecmp(ua->argk[i], NT_("verbose"))) { continue; }

    if (Bstrcasecmp(ua->argk[i], NT_("disabled"))) {
      ua->send->ObjectStart("disabled");
      if (((i + 1) < ua->argc) && Bstrcasecmp(ua->argk[i + 1], NT_("jobs"))) {
        ShowDisabledJobs(ua);
      } else if (((i + 1) < ua->argc) &&
                 Bstrcasecmp(ua->argk[i + 1], NT_("clients"))) {
        ShowDisabledClients(ua);
      } else if (((i + 1) < ua->argc) &&
                 Bstrcasecmp(ua->argk[i + 1], NT_("schedules"))) {
        ShowDisabledSchedules(ua);
      } else {
        ShowDisabledJobs(ua);
        ShowDisabledClients(ua);
        ShowDisabledSchedules(ua);
      }
      ua->send->ObjectEnd("disabled");
      goto bail_out;
    }

    type = 0;
    res_name = ua->argk[i];
    if (!ua->argv[i]) { /* was a name given? */
      /*
       * No name, dump all resources of specified type
       */
      recurse = 1;
      len = strlen(res_name);
      for (j = 0; show_cmd_available_resources[j].res_name; j++) {
        if (bstrncasecmp(res_name, _(show_cmd_available_resources[j].res_name),
                         len)) {
          type = show_cmd_available_resources[j].type;
          if (type > 0) {
            res = my_config->res_head_[type - my_config->r_first_];
          } else {
            res = NULL;
          }
          break;
        }
      }
    } else {
      /*
       * Dump a single resource with specified name
       */
      recurse = 0;
      len = strlen(res_name);
      for (j = 0; show_cmd_available_resources[j].res_name; j++) {
        if (bstrncasecmp(res_name, _(show_cmd_available_resources[j].res_name),
                         len)) {
          type = show_cmd_available_resources[j].type;
          res = (BareosResource*)ua->GetResWithName(type, ua->argv[i], true);
          if (!res) { type = -3; }
          break;
        }
      }
    }

    switch (type) {
      case -1: /* all */
        ShowAll(ua, hide_sensitive_data, verbose);
        break;
      case -2:
        ua->InfoMsg(_("Keywords for the show command are:\n"));
        for (j = 0; show_cmd_available_resources[j].res_name; j++) {
          ua->InfoMsg("%s\n", _(show_cmd_available_resources[j].res_name));
        }
        goto bail_out;
      case -3:
        ua->ErrorMsg(_("%s resource %s not found.\n"), res_name, ua->argv[i]);
        goto bail_out;
      case 0:
        ua->ErrorMsg(_("Resource %s not found\n"), res_name);
        goto bail_out;
      default:
        my_config->DumpResourceCb_(recurse ? type : -type, res, bsendmsg, ua,
                                   hide_sensitive_data, verbose);
        break;
    }
  }

bail_out:
  UnlockRes(my_config);
  return true;
}

/**
 *  List contents of database
 *
 *  list jobs                   - lists all jobs run
 *  list jobid=nnn              - list job data for jobid
 *  list ujobid=uname           - list job data for unique jobid
 *  list job=name               - list all jobs with "name"
 *  list jobname=name           - same as above
 *  list jobmedia jobid=nnn
 *  list jobmedia ujobid=uname
 *  list joblog jobid=<nn>
 *  list joblog job=name
 *  list log [ limit=<number> [ offset=<number> ] ]
 *  list basefiles jobid=nnn    - list files saved for job nn
 *  list basefiles ujobid=uname
 *  list files jobid=<nn>       - list files saved for job nn
 *  list files ujobid=name
 *  list pools                  - list pool records
 *  list jobtotals              - list totals for all jobs
 *  list media                  - list media for given pool (deprecated)
 *  list volumes                - list Volumes
 *  list clients                - list clients
 *  list nextvol job=xx         - list the next vol to be used by job
 *  list nextvolume job=xx      - same as above.
 *  list copies jobid=x,y,z
 */

/* Do long or full listing */
bool LlistCmd(UaContext* ua, const char* cmd)
{
  return DoListCmd(ua, cmd, VERT_LIST);
}

/* Do short or summary listing */
bool list_cmd(UaContext* ua, const char* cmd)
{
  return DoListCmd(ua, cmd, HORZ_LIST);
}

static int GetJobidFromCmdline(UaContext* ua)
{
  int i, jobid;
  JobDbRecord jr;
  ClientDbRecord cr;

  i = FindArgWithValue(ua, NT_("ujobid"));
  if (i >= 0) {
    bstrncpy(jr.Job, ua->argv[i], MAX_NAME_LENGTH);
    jr.JobId = 0;
    if (ua->db->GetJobRecord(ua->jcr, &jr)) {
      jobid = jr.JobId;
    } else {
      return -1;
    }
  } else {
    i = FindArgWithValue(ua, NT_("jobid"));
    if (i >= 0) {
      jr.JobId = str_to_int64(ua->argv[i]);
    } else {
      jobid = 0;
      goto bail_out;
    }
  }

  if (ua->db->GetJobRecord(ua->jcr, &jr)) {
    jobid = jr.JobId;
  } else {
    Dmsg1(200, "GetJobidFromCmdline: Failed to get job record for JobId %d\n",
          jr.JobId);
    jobid = -1;
    goto bail_out;
  }

  if (!ua->AclAccessOk(Job_ACL, jr.Name, true)) {
    Dmsg1(200, "GetJobidFromCmdline: No access to Job %s\n", jr.Name);
    jobid = -1;
    goto bail_out;
  }

  if (jr.ClientId) {
    cr.ClientId = jr.ClientId;
    if (ua->db->GetClientRecord(ua->jcr, &cr)) {
      if (!ua->AclAccessOk(Client_ACL, cr.Name, true)) {
        Dmsg1(200, "GetJobidFromCmdline: No access to Client %s\n", cr.Name);
        jobid = -1;
        goto bail_out;
      }
    } else {
      Dmsg1(
          200,
          "GetJobidFromCmdline: Failed to get client record for ClientId %d\n",
          jr.ClientId);
      jobid = -1;
      goto bail_out;
    }
  }

bail_out:
  return jobid;
}

/**
 * Filter convenience functions that abstract the actions needed to
 * perform a certain type of acl or resource filtering.
 */
static inline void SetAclFilter(UaContext* ua, int column, int acltype)
{
  if (ua->AclHasRestrictions(acltype)) {
    ua->send->AddAclFilterTuple(column, acltype);
  }
}

static inline void SetResFilter(UaContext* ua, int column, int restype)
{
  ua->send->AddResFilterTuple(column, restype);
}

static inline void SetEnabledFilter(UaContext* ua, int column, int restype)
{
  ua->send->AddEnabledFilterTuple(column, restype);
}

static inline void SetDisabledFilter(UaContext* ua, int column, int restype)
{
  ua->send->AddDisabledFilterTuple(column, restype);
}

static inline void SetHiddenColumnAclFilter(UaContext* ua,
                                            int column,
                                            int acltype)
{
  ua->send->AddHiddenColumn(column);
  if (ua->AclHasRestrictions(acltype)) {
    ua->send->AddAclFilterTuple(column, acltype);
  }
}

static inline void SetHiddenColumn(UaContext* ua, int column)
{
  ua->send->AddHiddenColumn(column);
}

static void SetQueryRange(PoolMem& query_range, UaContext* ua, JobDbRecord* jr)
{
  int i;

  /*
   * Ensure query range is an empty string instead of NULL
   * to avoid any issues.
   */
  if (query_range.c_str() == NULL) { PmStrcpy(query_range, ""); }

  /*
   * See if this is a second call to SetQueryRange() if so and any acl
   * filters have been set we setup a new query_range filter including a
   * limit filter.
   */
  if (query_range.strlen()) {
    if (!ua->send->has_acl_filters()) { return; }
    PmStrcpy(query_range, "");
  }

  /*
   * Apply any limit
   */
  i = FindArgWithValue(ua, NT_("limit"));
  if (i >= 0) {
    PoolMem temp(PM_MESSAGE);

    jr->limit = atoi(ua->argv[i]);
    ua->send->AddLimitFilterTuple(jr->limit);

    temp.bsprintf(" LIMIT %d", jr->limit);
    PmStrcat(query_range, temp.c_str());

    /*
     * offset is only valid, if limit is given
     */
    i = FindArgWithValue(ua, NT_("offset"));
    if (i >= 0) {
      jr->offset = atoi(ua->argv[i]);
      ua->send->AddOffsetFilterTuple(jr->offset);
      temp.bsprintf(" OFFSET %d", atoi(ua->argv[i]));
      PmStrcat(query_range, temp.c_str());
    }
  }
}

static bool DoListCmd(UaContext* ua, const char* cmd, e_list_type llist)
{
  JobDbRecord jr;
  PoolDbRecord pr;
  utime_t now;
  MediaDbRecord mr;
  int days = 0, hours = 0, jobstatus = 0, joblevel = 0;
  bool count, last, current, enabled, disabled;
  int i, d, h, jobid;
  time_t schedtime = 0;
  char* clientname = NULL;
  char* volumename = NULL;
  char* poolname = NULL;
  const int secs_in_day = 86400;
  const int secs_in_hour = 3600;
  PoolMem query_range(PM_MESSAGE);

  if (!OpenClientDb(ua, true)) { return true; }

  Dmsg1(20, "list: %s\n", cmd);

  if (ua->argc <= 1) {
    ua->ErrorMsg(_("%s command requires a keyword\n"), NPRT(ua->argk[0]));
    return false;
  }

  /*
   * days or hours given?
   */
  d = FindArgWithValue(ua, NT_("days"));
  h = FindArgWithValue(ua, NT_("hours"));

  now = (utime_t)time(NULL);
  if (d > 0) {
    days = str_to_int64(ua->argv[d]);
    schedtime = now - secs_in_day * days; /* Days in the past */
  }

  if (h > 0) {
    hours = str_to_int64(ua->argv[h]);
    schedtime = now - secs_in_hour * hours; /* Hours in the past */
  }

  current = FindArg(ua, NT_("current")) >= 0;
  enabled = FindArg(ua, NT_("enabled")) >= 0;
  disabled = FindArg(ua, NT_("disabled")) >= 0;
  count = FindArg(ua, NT_("count")) >= 0;
  last = FindArg(ua, NT_("last")) >= 0;

  PmStrcpy(query_range, "");
  SetQueryRange(query_range, ua, &jr);

  i = FindArgWithValue(ua, NT_("client"));
  if (i >= 0) {
    if (ua->GetClientResWithName(ua->argv[i])) {
      clientname = ua->argv[i];
    } else {
      ua->ErrorMsg(_("invalid client parameter\n"));
      return false;
    }
  }

  /*
   * jobstatus=X
   */
  if (!GetUserJobStatusSelection(ua, &jobstatus)) {
    ua->ErrorMsg(_("invalid jobstatus parameter\n"));
    return false;
  }

  /*
   * joblevel=X
   */
  if (!GetUserJobLevelSelection(ua, &joblevel)) {
    ua->ErrorMsg(_("invalid joblevel parameter\n"));
    return false;
  }

  /*
   * Select what to do based on the first argument.
   */
  if ((Bstrcasecmp(ua->argk[1], NT_("jobs")) && (ua->argv[1] == NULL)) ||
      ((Bstrcasecmp(ua->argk[1], NT_("job")) ||
        Bstrcasecmp(ua->argk[1], NT_("jobname"))) &&
       ua->argv[1])) {
    /*
     * List jobs or List job=xxx
     */
    i = FindArgWithValue(ua, NT_("jobname"));
    if (i < 0) { i = FindArgWithValue(ua, NT_("job")); }
    if (i >= 0) {
      jr.JobId = 0;
      bstrncpy(jr.Name, ua->argv[i], MAX_NAME_LENGTH);
    }

    i = FindArgWithValue(ua, NT_("volume"));
    if (i >= 0) { volumename = ua->argv[i]; }

    i = FindArgWithValue(ua, NT_("pool"));
    if (i >= 0) { poolname = ua->argv[i]; }

    switch (llist) {
      case VERT_LIST:
        if (!count) {
          SetAclFilter(ua, 2, Job_ACL);      /* JobName */
          SetAclFilter(ua, 7, Client_ACL);   /* ClientName */
          SetAclFilter(ua, 21, Pool_ACL);    /* PoolName */
          SetAclFilter(ua, 24, FileSet_ACL); /* FilesetName */
        }
        if (current) {
          SetResFilter(ua, 2, R_JOB);      /* JobName */
          SetResFilter(ua, 7, R_CLIENT);   /* ClientName */
          SetResFilter(ua, 21, R_POOL);    /* PoolName */
          SetResFilter(ua, 24, R_FILESET); /* FilesetName */
        }
        if (enabled) { SetEnabledFilter(ua, 2, R_JOB); /* JobName */ }
        if (disabled) { SetDisabledFilter(ua, 2, R_JOB); /* JobName */ }
        break;
      default:
        if (!count) {
          SetAclFilter(ua, 1, Job_ACL);    /* JobName */
          SetAclFilter(ua, 2, Client_ACL); /* ClientName */
        }
        if (current) {
          SetResFilter(ua, 1, R_JOB);    /* JobName */
          SetResFilter(ua, 2, R_CLIENT); /* ClientName */
        }
        if (enabled) { SetEnabledFilter(ua, 1, R_JOB); /* JobName */ }
        if (disabled) { SetDisabledFilter(ua, 1, R_JOB); /* JobName */ }
        break;
    }

    SetQueryRange(query_range, ua, &jr);

    ua->db->ListJobRecords(ua->jcr, &jr, query_range.c_str(), clientname,
                           jobstatus, joblevel, volumename, poolname, schedtime,
                           last, count, ua->send, llist);
  } else if (Bstrcasecmp(ua->argk[1], NT_("jobtotals"))) {
    /*
     * List JOBTOTALS
     */
    ua->db->ListJobTotals(ua->jcr, &jr, ua->send);
  } else if ((Bstrcasecmp(ua->argk[1], NT_("jobid")) ||
              Bstrcasecmp(ua->argk[1], NT_("ujobid"))) &&
             ua->argv[1]) {
    /*
     * List JOBID=nn
     * List UJOBID=xxx
     */
    if (ua->argv[1]) {
      jobid = GetJobidFromCmdline(ua);
      if (jobid > 0) {
        jr.JobId = jobid;

        i = FindArgWithValue(ua, NT_("pool"));
        if (i >= 0) { poolname = ua->argv[i]; }

        switch (llist) {
          case VERT_LIST:
            SetAclFilter(ua, 2, Job_ACL);      /* JobName */
            SetAclFilter(ua, 7, Client_ACL);   /* ClientName */
            SetAclFilter(ua, 21, Pool_ACL);    /* PoolName */
            SetAclFilter(ua, 24, FileSet_ACL); /* FilesetName */
            if (current) {
              SetResFilter(ua, 2, R_JOB);      /* JobName */
              SetResFilter(ua, 7, R_CLIENT);   /* ClientName */
              SetResFilter(ua, 21, R_POOL);    /* PoolName */
              SetResFilter(ua, 24, R_FILESET); /* FilesetName */
            }
            if (enabled) { SetEnabledFilter(ua, 2, R_JOB); /* JobName */ }
            if (disabled) { SetDisabledFilter(ua, 2, R_JOB); /* JobName */ }
            break;
          default:
            SetAclFilter(ua, 1, Job_ACL);    /* JobName */
            SetAclFilter(ua, 2, Client_ACL); /* ClientName */
            if (current) {
              SetResFilter(ua, 1, R_JOB);    /* JobName */
              SetResFilter(ua, 2, R_CLIENT); /* ClientName */
            }
            if (enabled) { SetEnabledFilter(ua, 1, R_JOB); /* JobName */ }
            if (disabled) { SetDisabledFilter(ua, 1, R_JOB); /* JobName */ }
            break;
        }

        SetQueryRange(query_range, ua, &jr);

        ua->db->ListJobRecords(ua->jcr, &jr, query_range.c_str(), clientname,
                               jobstatus, joblevel, volumename, poolname,
                               schedtime, last, count, ua->send, llist);
      }
    }
  } else if (Bstrcasecmp(ua->argk[1], NT_("basefiles"))) {
    /*
     * List BASEFILES
     */
    jobid = GetJobidFromCmdline(ua);
    if (jobid > 0) {
      ua->db->ListBaseFilesForJob(ua->jcr, jobid, ua->send);
    } else {
      ua->ErrorMsg(
          _("jobid not found in db, access to job or client denied by ACL, or "
            "client not found in db\n"));
    }
  } else if (Bstrcasecmp(ua->argk[1], NT_("files"))) {
    /*
     * List FILES
     */
    jobid = GetJobidFromCmdline(ua);
    if (jobid > 0) {
      ua->db->ListFilesForJob(ua->jcr, jobid, ua->send);
    } else {
      ua->ErrorMsg(
          _("jobid not found in db, access to job or client denied by ACL, or "
            "client not found in db\n"));
    }
  } else if (Bstrcasecmp(ua->argk[1], NT_("fileset"))) {
    int filesetid = 0;

    /*
     * List FileSet
     */
    i = FindArgWithValue(ua, NT_("filesetid"));
    if (i > 0) { filesetid = str_to_int64(ua->argv[i]); }

    jobid = GetJobidFromCmdline(ua);
    if (jobid > 0 || filesetid > 0) {
      jr.JobId = jobid;
      jr.FileSetId = filesetid;

      SetAclFilter(ua, 1, FileSet_ACL); /* FilesetName */
      if (current) { SetResFilter(ua, 1, R_FILESET); /* FilesetName */ }

      SetQueryRange(query_range, ua, &jr);

      ua->db->ListFilesets(ua->jcr, &jr, query_range.c_str(), ua->send, llist);
    } else {
      ua->ErrorMsg(
          _("jobid not found in db, access to job or client denied by ACL, or "
            "client not found in db or missing filesetid\n"));
    }
  } else if (Bstrcasecmp(ua->argk[1], NT_("filesets"))) {
    /*
     * List FILESETs
     */
    SetAclFilter(ua, 1, FileSet_ACL); /* FilesetName */
    if (current) { SetResFilter(ua, 1, R_FILESET); /* FilesetName */ }

    SetQueryRange(query_range, ua, &jr);

    ua->db->ListFilesets(ua->jcr, &jr, query_range.c_str(), ua->send, llist);
  } else if (Bstrcasecmp(ua->argk[1], NT_("jobmedia"))) {
    /*
     * List JOBMEDIA
     */
    jobid = GetJobidFromCmdline(ua);
    if (jobid >= 0) {
      ua->db->ListJobmediaRecords(ua->jcr, jobid, ua->send, llist);
    } else {
      ua->ErrorMsg(
          _("jobid not found in db, access to job or client denied by ACL, or "
            "client not found in db\n"));
    }
  } else if (Bstrcasecmp(ua->argk[1], NT_("joblog"))) {
    /*
     * List JOBLOG
     */
    jobid = GetJobidFromCmdline(ua);
    if (jobid >= 0) {
      ua->db->ListJoblogRecords(ua->jcr, jobid, query_range.c_str(), count,
                                ua->send, llist);
    } else {
      ua->ErrorMsg(
          _("jobid not found in db, access to job or client denied by ACL, or "
            "client not found in db\n"));
    }
  } else if (Bstrcasecmp(ua->argk[1], NT_("log"))) {
    bool reverse;

    /*
     * List last <limit> LOG entries
     * default is DEFAULT_LOG_LINES entries
     */
    reverse = FindArg(ua, NT_("reverse")) >= 0;

    if (strlen(query_range.c_str()) == 0) {
      Mmsg(query_range, " LIMIT %d", DEFAULT_LOG_LINES);
    }

    if (ua->api != API_MODE_JSON) {
      SetHiddenColumn(ua, 0);                      /* LogId */
      SetHiddenColumnAclFilter(ua, 1, Job_ACL);    /* JobName */
      SetHiddenColumnAclFilter(ua, 2, Client_ACL); /* ClientName */
      SetHiddenColumn(ua, 3);                      /* LogTime */
    } else {
      SetAclFilter(ua, 1, Job_ACL);    /* JobName */
      SetAclFilter(ua, 2, Client_ACL); /* ClientName */
    }

    SetQueryRange(query_range, ua, &jr);

    ua->db->ListLogRecords(ua->jcr, clientname, query_range.c_str(), reverse,
                           ua->send, llist);
  } else if (Bstrcasecmp(ua->argk[1], NT_("pool")) ||
             Bstrcasecmp(ua->argk[1], NT_("pools"))) {
    PoolDbRecord pr;

    /*
     * List POOLS
     */
    if (ua->argv[1]) { bstrncpy(pr.Name, ua->argv[1], sizeof(pr.Name)); }

    SetAclFilter(ua, 1, Pool_ACL); /* PoolName */
    if (current) { SetResFilter(ua, 1, R_POOL); /* PoolName */ }

    SetQueryRange(query_range, ua, &jr);

    ua->db->ListPoolRecords(ua->jcr, &pr, ua->send, llist);
  } else if (Bstrcasecmp(ua->argk[1], NT_("clients"))) {
    /*
     * List CLIENTS
     */
    SetAclFilter(ua, 1, Client_ACL); /* ClientName */
    if (current) { SetResFilter(ua, 1, R_CLIENT); /* ClientName */ }
    if (enabled) { SetEnabledFilter(ua, 1, R_CLIENT); /* ClientName */ }
    if (disabled) { SetDisabledFilter(ua, 1, R_CLIENT); /* ClientName */ }

    SetQueryRange(query_range, ua, &jr);

    ua->db->ListClientRecords(ua->jcr, NULL, ua->send, llist);
  } else if (Bstrcasecmp(ua->argk[1], NT_("client")) && ua->argv[1]) {
    /*
     * List CLIENT=xxx
     */
    SetAclFilter(ua, 1, Client_ACL); /* ClientName */
    if (current) { SetResFilter(ua, 1, R_CLIENT); /* ClientName */ }
    if (enabled) { SetEnabledFilter(ua, 1, R_CLIENT); /* ClientName */ }
    if (disabled) { SetDisabledFilter(ua, 1, R_CLIENT); /* ClientName */ }

    SetQueryRange(query_range, ua, &jr);

    ua->db->ListClientRecords(ua->jcr, ua->argv[1], ua->send, llist);
  } else if (Bstrcasecmp(ua->argk[1], NT_("storages"))) {
    /*
     * List STORAGES
     */
    SetAclFilter(ua, 1, Storage_ACL); /* StorageName */
    if (current) { SetResFilter(ua, 1, R_STORAGE); /* StorageName */ }
    if (enabled) { SetEnabledFilter(ua, 1, R_STORAGE); /* StorageName */ }
    if (disabled) { SetDisabledFilter(ua, 1, R_STORAGE); /* StorageName */ }

    SetQueryRange(query_range, ua, &jr);

    ua->db->ListSqlQuery(ua->jcr, "SELECT * FROM Storage", ua->send, llist,
                         "storages");
  } else if (Bstrcasecmp(ua->argk[1], NT_("media")) ||
             Bstrcasecmp(ua->argk[1], NT_("volume")) ||
             Bstrcasecmp(ua->argk[1], NT_("volumes"))) {
    /*
     * List MEDIA or VOLUMES
     */
    jobid = GetJobidFromCmdline(ua);
    if (jobid > 0) {
      ua->db->ListVolumesOfJobid(ua->jcr, jobid, ua->send, llist);
    } else if (jobid == 0) {
      /*
       * List a specific volume?
       */
      if (ua->argv[1]) {
        bstrncpy(mr.VolumeName, ua->argv[1], sizeof(mr.VolumeName));
        ua->send->ObjectStart("volume");
        ua->db->ListMediaRecords(ua->jcr, &mr, query_range.c_str(), count,
                                 ua->send, llist);
        ua->send->ObjectEnd("volume");
      } else {
        /*
         * If no job or jobid keyword found, then we list all media
         * Is a specific pool wanted?
         */
        i = FindArgWithValue(ua, NT_("pool"));
        if (i >= 0) {
          bstrncpy(pr.Name, ua->argv[i], sizeof(pr.Name));

          if (!GetPoolDbr(ua, &pr)) {
            ua->ErrorMsg(_("Pool %s doesn't exist.\n"), ua->argv[i]);
            return true;
          }

          SetQueryRange(query_range, ua, &jr);

          mr.PoolId = pr.PoolId;
          ua->send->ArrayStart("volumes");
          ua->db->ListMediaRecords(ua->jcr, &mr, query_range.c_str(), count,
                                   ua->send, llist);
          ua->send->ArrayEnd("volumes");
          return true;
        } else {
          int num_pools;
          uint32_t* ids = nullptr;

          /*
           * List all volumes, flat
           */
          if (FindArg(ua, NT_("all")) > 0) {
            /*
             * The result of "list media all"
             * does not contain the Pool information,
             * therefore checking the Pool_ACL is not possible.
             * For this reason, we prevent this command.
             */
            if (ua->AclHasRestrictions(Pool_ACL) && (llist != VERT_LIST)) {
              ua->ErrorMsg(
                  _("Restricted permission. Use the commands 'list media' or "
                    "'llist media all' instead\n"));
              return false;
            }
            ua->send->ArrayStart("volumes");
            SetAclFilter(ua, 4, Pool_ACL); /* PoolName */
            if (current) { SetResFilter(ua, 4, R_POOL); /* PoolName */ }
            ua->db->ListMediaRecords(ua->jcr, &mr, query_range.c_str(), count,
                                     ua->send, llist);
            ua->send->ArrayEnd("volumes");
          } else {
            /*
             * List Volumes in all pools
             */
            if (!ua->db->GetPoolIds(ua->jcr, &num_pools, &ids)) {
              ua->ErrorMsg(_("Error obtaining pool ids. ERR=%s\n"),
                           ua->db->strerror());
              return true;
            }

            if (num_pools <= 0) {
              if (ids) { free(ids); }
              return true;
            }

            ua->send->ObjectStart("volumes");
            for (i = 0; i < num_pools; i++) {
              pr.PoolId = ids[i];
              if (ua->db->GetPoolRecord(ua->jcr, &pr)) {
                if (ua->AclAccessOk(Pool_ACL, pr.Name, false)) {
                  ua->send->Decoration("Pool: %s\n", pr.Name);
                  ua->send->ArrayStart(pr.Name);
                  mr.PoolId = ids[i];
                  ua->db->ListMediaRecords(ua->jcr, &mr, query_range.c_str(),
                                           count, ua->send, llist);
                  ua->send->ArrayEnd(pr.Name);
                }
              }
            }
            ua->send->ObjectEnd("volumes");
            free(ids);
          }
        }
      }
    }

    return true;
  } else if (Bstrcasecmp(ua->argk[1], NT_("nextvol")) ||
             Bstrcasecmp(ua->argk[1], NT_("nextvolume"))) {
    int days;

    /*
     * List next volume
     */
    days = 1;

    i = FindArgWithValue(ua, NT_("days"));
    if (i >= 0) {
      days = atoi(ua->argv[i]);
      if ((days < 0) || (days > DEFAULT_NR_DAYS)) {
        ua->WarningMsg(_("Ignoring invalid value for days. Max is %d.\n"),
                       DEFAULT_NR_DAYS);
        days = 1;
      }
    }
    ListNextvol(ua, days);
  } else if (Bstrcasecmp(ua->argk[1], NT_("copies"))) {
    /*
     * List copies
     */
    i = FindArgWithValue(ua, NT_("jobid"));
    if (i >= 0) {
      if (Is_a_number_list(ua->argv[i])) {
        ua->db->ListCopiesRecords(ua->jcr, query_range.c_str(), ua->argv[i],
                                  ua->send, llist);
      }
    } else {
      ua->db->ListCopiesRecords(ua->jcr, query_range.c_str(), NULL, ua->send,
                                llist);
    }
  } else if (Bstrcasecmp(ua->argk[1], NT_("backups"))) {
    if (ParseListBackupsCmd(ua, query_range.c_str(), llist)) {
      switch (llist) {
        case VERT_LIST:
          SetAclFilter(ua, 2, Job_ACL);   /* JobName */
          SetAclFilter(ua, 21, Pool_ACL); /* PoolName */
          if (current) {
            SetResFilter(ua, 2, R_JOB);   /* JobName */
            SetResFilter(ua, 21, R_POOL); /* PoolName */
          }
          if (enabled) { SetEnabledFilter(ua, 2, R_JOB); /* JobName */ }
          if (disabled) { SetDisabledFilter(ua, 2, R_JOB); /* JobName */ }
          break;
        default:
          SetAclFilter(ua, 1, Job_ACL); /* JobName */
          if (current) { SetResFilter(ua, 1, R_JOB); /* JobName */ }
          if (enabled) { SetEnabledFilter(ua, 1, R_JOB); /* JobName */ }
          if (disabled) { SetDisabledFilter(ua, 1, R_JOB); /* JobName */ }
          break;
      }

      ua->db->ListSqlQuery(ua->jcr, ua->cmd, ua->send, llist, "backups");
    }
  } else if (Bstrcasecmp(ua->argk[1], NT_("jobstatistics")) ||
             Bstrcasecmp(ua->argk[1], NT_("jobstats"))) {
    jobid = GetJobidFromCmdline(ua);
    if (jobid > 0) {
      ua->db->ListJobstatisticsRecords(ua->jcr, jobid, ua->send, llist);
    } else {
      ua->ErrorMsg(_("no jobid given\n"));
      return false;
    }
  } else {
    ua->ErrorMsg(_("Unknown list keyword: %s\n"), NPRT(ua->argk[1]));
    return false;
  }

  return true;
}

static inline bool parse_jobstatus_selection_param(
    PoolMem& selection,
    UaContext* ua,
    const char* default_selection)
{
  int pos;

  selection.strcpy("");
  if ((pos = FindArgWithValue(ua, "jobstatus")) >= 0) {
    int cnt = 0;
    int jobstatus;
    PoolMem temp;
    char *cur_stat, *bp;

    cur_stat = ua->argv[pos];
    while (cur_stat) {
      bp = strchr(cur_stat, ',');
      if (bp) { *bp++ = '\0'; }

      /*
       * Try matching the status to an internal Job Termination code.
       */
      if (strlen(cur_stat) == 1 && cur_stat[0] >= 'A' && cur_stat[0] <= 'z') {
        jobstatus = cur_stat[0];
      } else if (Bstrcasecmp(cur_stat, "terminated")) {
        jobstatus = JS_Terminated;
      } else if (Bstrcasecmp(cur_stat, "warnings")) {
        jobstatus = JS_Warnings;
      } else if (Bstrcasecmp(cur_stat, "canceled")) {
        jobstatus = JS_Canceled;
      } else if (Bstrcasecmp(cur_stat, "running")) {
        jobstatus = JS_Running;
      } else if (Bstrcasecmp(cur_stat, "error")) {
        jobstatus = JS_Error;
      } else if (Bstrcasecmp(cur_stat, "fatal")) {
        jobstatus = JS_FatalError;
      } else {
        cur_stat = bp;
        continue;
      }

      if (cnt == 0) {
        Mmsg(temp, " AND JobStatus IN ('%c'", jobstatus);
        PmStrcat(selection, temp.c_str());
      } else {
        Mmsg(temp, ",'%c'", jobstatus);
        PmStrcat(selection, temp.c_str());
      }
      cur_stat = bp;
      cnt++;
    }

    /*
     * Close set if we opened one.
     */
    if (cnt > 0) { PmStrcat(selection, ")"); }
  }

  if (selection.strlen() == 0) {
    /*
     * When no explicit Job Termination code specified use default
     */
    selection.strcpy(default_selection);
  }

  return true;
}

static inline bool parse_level_selection_param(PoolMem& selection,
                                               UaContext* ua,
                                               const char* default_selection)
{
  int pos;

  selection.strcpy("");
  if ((pos = FindArgWithValue(ua, "level")) >= 0) {
    int cnt = 0;
    PoolMem temp;
    char *cur_level, *bp;

    cur_level = ua->argv[pos];
    while (cur_level) {
      bp = strchr(cur_level, ',');
      if (bp) { *bp++ = '\0'; }

      /*
       * Try mapping from text level to internal level.
       */
      for (int i = 0; joblevels[i].level_name; i++) {
        if (joblevels[i].job_type == JT_BACKUP &&
            bstrncasecmp(joblevels[i].level_name, cur_level,
                         strlen(cur_level))) {
          if (cnt == 0) {
            Mmsg(temp, " AND Level IN ('%c'", joblevels[i].level);
            PmStrcat(selection, temp.c_str());
          } else {
            Mmsg(temp, ",'%c'", joblevels[i].level);
            PmStrcat(selection, temp.c_str());
          }
        }
      }
      cur_level = bp;
      cnt++;
    }

    /*
     * Close set if we opened one.
     */
    if (cnt > 0) { PmStrcat(selection, ")"); }
  }
  if (selection.strlen() == 0) { selection.strcpy(default_selection); }

  return true;
}

static inline bool parse_fileset_selection_param(PoolMem& selection,
                                                 UaContext* ua,
                                                 bool listall)
{
  int fileset;

  PmStrcpy(selection, "");
  fileset = FindArgWithValue(ua, "fileset");
  if ((fileset >= 0 && Bstrcasecmp(ua->argv[fileset], "any")) ||
      (listall && fileset < 0)) {
    FilesetResource* fs;
    PoolMem temp(PM_MESSAGE);

    LockRes(my_config);
    foreach_res (fs, R_FILESET) {
      if (!ua->AclAccessOk(FileSet_ACL, fs->resource_name_, false)) {
        continue;
      }
      if (selection.strlen() == 0) {
        temp.bsprintf("AND (FileSet='%s'", fs->resource_name_);
      } else {
        temp.bsprintf(" OR FileSet='%s'", fs->resource_name_);
      }
      PmStrcat(selection, temp.c_str());
    }
    PmStrcat(selection, ") ");
    UnlockRes(my_config);
  } else if (fileset >= 0) {
    if (!ua->AclAccessOk(FileSet_ACL, ua->argv[fileset], true)) {
      ua->ErrorMsg(_("Access to specified FileSet not allowed.\n"));
      return false;
    } else {
      selection.bsprintf("AND FileSet='%s' ", ua->argv[fileset]);
    }
  }

  return true;
}

static bool ParseListBackupsCmd(UaContext* ua,
                                const char* range,
                                e_list_type llist)
{
  int pos, client;
  PoolMem temp(PM_MESSAGE), selection(PM_MESSAGE), criteria(PM_MESSAGE);

  client = FindArgWithValue(ua, "client");
  if (client < 0) {
    ua->ErrorMsg(_("missing parameter: client\n"));
    return false;
  }

  if (!ua->AclAccessOk(Client_ACL, ua->argv[client], true)) {
    ua->ErrorMsg(_("Access to specified Client not allowed.\n"));
    return false;
  }

  /* allow jobtypes 'B' for Backup and 'A' or 'a' for archive (update job
   * doesn't enforce a valid jobtype, so people have 'a' in their catalogs */
  selection.bsprintf("AND Job.Type IN('B', 'A', 'a') AND Client.Name='%s' ",
                     ua->argv[client]);

  /*
   * Build a selection pattern based on the jobstatus and level arguments.
   */
  parse_jobstatus_selection_param(temp, ua, "AND JobStatus IN ('T','W') ");
  PmStrcat(selection, temp.c_str());

  parse_level_selection_param(temp, ua, "");
  PmStrcat(selection, temp.c_str());

  if (!parse_fileset_selection_param(temp, ua, true)) { return false; }
  PmStrcat(selection, temp.c_str());

  /*
   * Build a criteria pattern if the order and/or limit argument are given.
   */
  PmStrcpy(criteria, "");
  if ((pos = FindArgWithValue(ua, "order")) >= 0) {
    if (bstrncasecmp(ua->argv[pos], "ascending", strlen(ua->argv[pos]))) {
      PmStrcat(criteria, " ASC");
    } else if (bstrncasecmp(ua->argv[pos], "descending",
                            strlen(ua->argv[pos]))) {
      PmStrcat(criteria, " DESC");
    } else {
      return false;
    }
  }

  /*
   * add range settings
   */
  PmStrcat(criteria, range);

  if (llist == VERT_LIST) {
    ua->db->FillQuery(ua->cmd, BareosDb::SQL_QUERY::list_jobs_long,
                      selection.c_str(), criteria.c_str());
  } else {
    ua->db->FillQuery(ua->cmd, BareosDb::SQL_QUERY::list_jobs,
                      selection.c_str(), criteria.c_str());
  }

  return true;
}

static bool ListNextvol(UaContext* ua, int ndays)
{
  int i;
  JobResource* job;
  JobControlRecord* jcr;
  UnifiedStorageResource store;
  RunResource* run;
  utime_t runtime;
  bool found = false;

  i = FindArgWithValue(ua, "job");
  if (i <= 0) {
    if ((job = select_job_resource(ua)) == NULL) { return false; }
  } else {
    job = ua->GetJobResWithName(ua->argv[i]);
    if (!job) {
      Jmsg(ua->jcr, M_ERROR, 0, _("%s is not a job name.\n"), ua->argv[i]);
      if ((job = select_job_resource(ua)) == NULL) { return false; }
    }
  }

  jcr = NewDirectorJcr();
  for (run = NULL; (run = find_next_run(run, job, runtime, ndays));) {
    if (!CompleteJcrForJob(jcr, job, run->pool)) {
      found = false;
      goto get_out;
    }
    if (!jcr->impl->jr.PoolId) {
      ua->ErrorMsg(_("Could not find Pool for Job %s\n"), job->resource_name_);
      continue;
    }
    PoolDbRecord pr;
    pr.PoolId = jcr->impl->jr.PoolId;
    if (!ua->db->GetPoolRecord(jcr, &pr)) {
      bstrncpy(pr.Name, "*UnknownPool*", sizeof(pr.Name));
    }
    MediaDbRecord mr;
    mr.PoolId = jcr->impl->jr.PoolId;
    GetJobStorage(&store, job, run);
    SetStorageidInMr(store.store, &mr);
    /* no need to set ScratchPoolId, since we use fnv_no_create_vol */
    if (!FindNextVolumeForAppend(jcr, &mr, 1, NULL, fnv_no_create_vol,
                                 fnv_prune)) {
      ua->ErrorMsg(
          _("Could not find next Volume for Job %s (Pool=%s, Level=%s).\n"),
          job->resource_name_, pr.Name, JobLevelToString(run->level));
    } else {
      ua->SendMsg(_("The next Volume to be used by Job \"%s\" (Pool=%s, "
                    "Level=%s) will be %s\n"),
                  job->resource_name_, pr.Name, JobLevelToString(run->level),
                  mr.VolumeName);
      found = true;
    }
  }

get_out:
  if (jcr->db) {
    DbSqlClosePooledConnection(jcr, jcr->db);
    jcr->db = NULL;
  }
  FreeJcr(jcr);
  if (!found) {
    ua->ErrorMsg(_("Could not find next Volume for Job %s.\n"),
                 job->resource_name_);
    return false;
  }
  return true;
}

/**
 * For a given job, we examine all his run records
 *  to see if it is scheduled today or tomorrow.
 */
RunResource* find_next_run(RunResource* run,
                           JobResource* job,
                           utime_t& runtime,
                           int ndays)
{
  time_t now, future, endtime;
  ScheduleResource* sched;
  struct tm tm, runtm;
  int mday, wday, month, wom, i;
  int woy;
  int day;

  sched = job->schedule;
  if (sched == NULL) { /* scheduled? */
    return NULL;       /* no nothing to report */
  }

  /* Break down the time into components */
  now = time(NULL);
  endtime = now + (ndays * 60 * 60 * 24);

  if (run == NULL) {
    run = sched->run;
  } else {
    run = run->next;
  }
  for (; run; run = run->next) {
    /*
     * Find runs in next 24 hours.  Day 0 is today, so if
     *   ndays=1, look at today and tomorrow.
     */
    for (day = 0; day <= ndays; day++) {
      future = now + (day * 60 * 60 * 24);

      /* Break down the time into components */
      Blocaltime(&future, &tm);
      mday = tm.tm_mday - 1;
      wday = tm.tm_wday;
      month = tm.tm_mon;
      wom = mday / 7;
      woy = TmWoy(future);

      bool is_scheduled = BitIsSet(mday, run->date_time_bitfield.mday) &&
                          BitIsSet(wday, run->date_time_bitfield.wday) &&
                          BitIsSet(month, run->date_time_bitfield.month) &&
                          BitIsSet(wom, run->date_time_bitfield.wom) &&
                          BitIsSet(woy, run->date_time_bitfield.woy);

      if (is_scheduled) { /* Jobs scheduled on that day */
        /* find time (time_t) job is to be run */
        Blocaltime(&future, &runtm);
        for (i = 0; i < 24; i++) {
          if (BitIsSet(i, run->date_time_bitfield.hour)) {
            runtm.tm_hour = i;
            runtm.tm_min = run->minute;
            runtm.tm_sec = 0;
            runtime = mktime(&runtm);
            Dmsg2(200, "now=%d runtime=%lld\n", now, runtime);
            if ((runtime > now) && (runtime < endtime)) {
              Dmsg2(200, "Found it level=%d %c\n", run->level, run->level);
              return run; /* found it, return run resource */
            }
          }
        }
      }
    }
  } /* end for loop over runs */
  /* Nothing found */
  return NULL;
}

/**
 * Fill in the remaining fields of the jcr as if it is going to run the job.
 */
bool CompleteJcrForJob(JobControlRecord* jcr,
                       JobResource* job,
                       PoolResource* pool)
{
  SetJcrDefaults(jcr, job);
  if (pool) { jcr->impl->res.pool = pool; /* override */ }
  if (jcr->db) {
    Dmsg0(100, "complete_jcr close db\n");
    DbSqlClosePooledConnection(jcr, jcr->db);
    jcr->db = NULL;
  }

  Dmsg0(100, "complete_jcr open db\n");
  jcr->db = GetDatabaseConnection(jcr);
  if (jcr->db == NULL) {
    Jmsg(jcr, M_FATAL, 0, _("Could not open database \"%s\".\n"),
         jcr->impl->res.catalog->db_name);
    return false;
  }
  PoolDbRecord pr;
  bstrncpy(pr.Name, jcr->impl->res.pool->resource_name_, sizeof(pr.Name));
  while (!jcr->db->GetPoolRecord(jcr, &pr)) { /* get by Name */
    /* Try to create the pool */
    if (CreatePool(jcr, jcr->db, jcr->impl->res.pool, POOL_OP_CREATE) < 0) {
      Jmsg(jcr, M_FATAL, 0, _("Pool %s not in database. %s"), pr.Name,
           jcr->db->strerror());
      if (jcr->db) {
        DbSqlClosePooledConnection(jcr, jcr->db);
        jcr->db = NULL;
      }
      return false;
    } else {
      Jmsg(jcr, M_INFO, 0, _("Pool %s created in database.\n"), pr.Name);
    }
  }
  jcr->impl->jr.PoolId = pr.PoolId;
  return true;
}

static void ConLockRelease(void* arg) { Vw(con_lock); }

void DoMessages(UaContext* ua, const char* cmd)
{
  char msg[2000];
  int mlen;
  bool DoTruncate = false;

  /*
   * Flush any queued messages.
   */
  if (ua->jcr) { DequeueMessages(ua->jcr); }

  Pw(con_lock);
  pthread_cleanup_push(ConLockRelease, (void*)NULL);
  rewind(con_fd);
  while (fgets(msg, sizeof(msg), con_fd)) {
    mlen = strlen(msg);
    ua->UA_sock->msg = CheckPoolMemorySize(ua->UA_sock->msg, mlen + 1);
    strcpy(ua->UA_sock->msg, msg);
    ua->UA_sock->message_length = mlen;
    ua->UA_sock->send();
    DoTruncate = true;
  }
  if (DoTruncate) { (void)!ftruncate(fileno(con_fd), 0L); }
  console_msg_pending = FALSE;
  ua->user_notified_msg_pending = FALSE;
  pthread_cleanup_pop(0);
  Vw(con_lock);
}

bool DotMessagesCmd(UaContext* ua, const char* cmd)
{
  if (console_msg_pending && ua->AclNoRestrictions(Command_ACL) &&
      ua->auto_display_messages) {
    DoMessages(ua, cmd);
  }
  return true;
}

bool MessagesCmd(UaContext* ua, const char* cmd)
{
  if (console_msg_pending && ua->AclNoRestrictions(Command_ACL)) {
    DoMessages(ua, cmd);
  } else {
    ua->send->Decoration(_("You have no messages.\n"));
  }
  return true;
}

/**
 * Callback routine for "filtering" database listing.
 */
of_filter_state filterit(void* ctx, void* data, of_filter_tuple* tuple)
{
  char** row = (char**)data;
  UaContext* ua = (UaContext*)ctx;
  of_filter_state retval = OF_FILTER_STATE_SHOW;

  switch (tuple->type) {
    case OF_FILTER_LIMIT:
      break;
    case OF_FILTER_OFFSET:
      break;
    case OF_FILTER_ACL:
      if (!row[tuple->u.acl_filter.column] ||
          strlen(row[tuple->u.acl_filter.column]) == 0) {
        retval = OF_FILTER_STATE_UNKNOWN;
      } else {
        if (!ua->AclAccessOk(tuple->u.acl_filter.acltype,
                             row[tuple->u.acl_filter.column], false)) {
          Dmsg2(200,
                "filterit: Filter on acl_type %d value %s, suppress output\n",
                tuple->u.acl_filter.acltype, row[tuple->u.acl_filter.column]);
          retval = OF_FILTER_STATE_SUPPRESS;
        }
      }
      goto bail_out;
    case OF_FILTER_RESOURCE:
      if (!row[tuple->u.res_filter.column] ||
          strlen(row[tuple->u.res_filter.column]) == 0) {
        retval = OF_FILTER_STATE_UNKNOWN;
      } else {
        if (!my_config->GetResWithName(tuple->u.res_filter.restype,
                                       row[tuple->u.res_filter.column],
                                       false)) {
          Dmsg2(200,
                "filterit: Filter on resource_type %d value %s, suppress "
                "output\n",
                tuple->u.res_filter.restype, row[tuple->u.res_filter.column]);
          retval = OF_FILTER_STATE_SUPPRESS;
        }
      }
      goto bail_out;
    case OF_FILTER_ENABLED:
    case OF_FILTER_DISABLED: {
      bool enabled = true;

      if (!row[tuple->u.res_filter.column] ||
          strlen(row[tuple->u.res_filter.column]) == 0) {
        retval = OF_FILTER_STATE_UNKNOWN;
        goto bail_out;
      }

      if (tuple->type == OF_FILTER_DISABLED) { enabled = false; }

      switch (tuple->u.res_filter.restype) {
        case R_CLIENT: {
          ClientResource* client;

          client = ua->GetClientResWithName(row[tuple->u.res_filter.column],
                                            false, false);
          if (!client || client->enabled != enabled) {
            Dmsg2(200, "filterit: Filter on Client, %s is not %sabled\n",
                  row[tuple->u.res_filter.column], (enabled) ? "En" : "Dis");
            retval = OF_FILTER_STATE_SUPPRESS;
          }
          goto bail_out;
        }
        case R_JOB: {
          JobResource* job;

          job = ua->GetJobResWithName(row[tuple->u.res_filter.column], false,
                                      false);
          if (!job || job->enabled != enabled) {
            Dmsg2(200, "filterit: Filter on Job, %s is not %sabled\n",
                  row[tuple->u.res_filter.column], (enabled) ? "En" : "Dis");
            retval = OF_FILTER_STATE_SUPPRESS;
          }
          goto bail_out;
        }
        case R_STORAGE: {
          StorageResource* store;

          store = ua->GetStoreResWithName(row[tuple->u.res_filter.column],
                                          false, false);
          if (!store || store->enabled != enabled) {
            Dmsg2(200, "filterit: Filter on Storage, %s is not %sabled\n",
                  row[tuple->u.res_filter.column], (enabled) ? "En" : "Dis");
            retval = OF_FILTER_STATE_SUPPRESS;
          }
          goto bail_out;
        }
        case R_SCHEDULE: {
          ScheduleResource* schedule;

          schedule = ua->GetScheduleResWithName(row[tuple->u.res_filter.column],
                                                false, false);
          if (!schedule || schedule->enabled != enabled) {
            Dmsg2(200, "filterit: Filter on Schedule, %s is not %sabled\n",
                  row[tuple->u.res_filter.column], (enabled) ? "En" : "Dis");
            retval = OF_FILTER_STATE_SUPPRESS;
          }
          goto bail_out;
        }
        default:
          goto bail_out;
      }
      break;
    }
    default:
      retval = OF_FILTER_STATE_SUPPRESS;
  }

bail_out:
  return retval;
}

/**
 * Callback routine for "printing" database listing
 */
bool printit(void* ctx, const char* msg)
{
  bool retval = false;
  UaContext* ua = (UaContext*)ctx;

  if (ua->UA_sock) {
    retval = ua->UA_sock->fsend("%s", msg);
  } else { /* No UA, send to Job */
    Jmsg(ua->jcr, M_INFO, 0, "%s", msg);
    retval = true;
  }

  return retval;
}

bool sprintit(void* ctx, const char* fmt, ...)
{
  va_list arg_ptr;
  PoolMem msg;

  va_start(arg_ptr, fmt);
  msg.Bvsprintf(fmt, arg_ptr);
  va_end(arg_ptr);

  return printit(ctx, msg.c_str());
}


/**
 * Format message and send to other end.

 * If the UA_sock is NULL, it means that there is no user
 * agent, so we are being called from BAREOS core. In
 * that case direct the messages to the Job.
 */
void bmsg(UaContext* ua, const char* fmt, va_list arg_ptr)
{
  BareosSocket* bs = ua->UA_sock;
  int maxlen, len;
  POOLMEM* msg = NULL;
  va_list ap;

  if (bs) { msg = bs->msg; }
  if (!msg) { msg = GetPoolMemory(PM_EMSG); }

again:
  maxlen = SizeofPoolMemory(msg) - 1;
  va_copy(ap, arg_ptr);
  len = Bvsnprintf(msg, maxlen, fmt, ap);
  va_end(ap);
  if (len < 0 || len >= maxlen) {
    msg = ReallocPoolMemory(msg, maxlen + maxlen / 2);
    goto again;
  }

  if (bs) {
    bs->msg = msg;
    bs->message_length = len;
    bs->send();
  } else { /* No UA, send to Job */
    Jmsg(ua->jcr, M_INFO, 0, "%s", msg);
    FreePoolMemory(msg);
  }
}

bool bsendmsg(void* ctx, const char* fmt, ...)
{
  va_list arg_ptr;
  va_start(arg_ptr, fmt);
  bmsg((UaContext*)ctx, fmt, arg_ptr);
  va_end(arg_ptr);

  return true;
}

/*
 * The following UA methods are mainly intended for GUI
 * programs
 */
/**
 * This is a message that should be displayed on the user's
 *  console.
 */
void UaContext::SendMsg(const char* fmt, ...)
{
  va_list arg_ptr;
  PoolMem message;

  /* send current buffer */
  send->SendBuffer();

  va_start(arg_ptr, fmt);
  message.Bvsprintf(fmt, arg_ptr);
  va_end(arg_ptr);
  send->message(NULL, message);
}

void UaContext::SendRawMsg(const char* msg) { SendMsg(msg); }


/**
 * This is an error condition with a command. The gui should put
 *  up an error or critical dialog box.  The command is aborted.
 */
void UaContext::ErrorMsg(const char* fmt, ...)
{
  va_list arg_ptr;
  BareosSocket* bs = UA_sock;
  PoolMem message;

  /* send current buffer */
  send->SendBuffer();

  if (bs && api) bs->signal(BNET_ERROR_MSG);
  va_start(arg_ptr, fmt);
  message.Bvsprintf(fmt, arg_ptr);
  va_end(arg_ptr);
  send->message(MSG_TYPE_ERROR, message);
}

/**
 * This is a warning message, that should bring up a warning
 *  dialog box on the GUI. The command is not aborted, but something
 *  went wrong.
 */
void UaContext::WarningMsg(const char* fmt, ...)
{
  va_list arg_ptr;
  BareosSocket* bs = UA_sock;
  PoolMem message;

  /* send current buffer */
  send->SendBuffer();

  if (bs && api) bs->signal(BNET_WARNING_MSG);
  va_start(arg_ptr, fmt);
  message.Bvsprintf(fmt, arg_ptr);
  va_end(arg_ptr);
  send->message(MSG_TYPE_WARNING, message);
}

/**
 * This is an information message that should probably be put
 *  into the status line of a GUI program.
 */
void UaContext::InfoMsg(const char* fmt, ...)
{
  va_list arg_ptr;
  BareosSocket* bs = UA_sock;
  PoolMem message;

  /* send current buffer */
  send->SendBuffer();

  if (bs && api) bs->signal(BNET_INFO_MSG);
  va_start(arg_ptr, fmt);
  message.Bvsprintf(fmt, arg_ptr);
  va_end(arg_ptr);
  send->message(MSG_TYPE_INFO, message);
}


void UaContext::SendCmdUsage(const char* fmt, ...)
{
  va_list arg_ptr;
  PoolMem message;
  PoolMem usage;

  /* send current buffer */
  send->SendBuffer();

  va_start(arg_ptr, fmt);
  message.Bvsprintf(fmt, arg_ptr);
  va_end(arg_ptr);

  if (cmddef) {
    if (cmddef->key && cmddef->usage) {
      usage.bsprintf("\nUSAGE: %s %s\n", cmddef->key, cmddef->usage);
      message.strcat(usage);
    }
  }

  send->message(NULL, message);
}
} /* namespace directordaemon */
