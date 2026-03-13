/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2012 Free Software Foundation Europe e.V.
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
// Kern Sibbald, September MM
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
#include "dird/director_jcr_impl.h"
#include "dird/job.h"
#include "dird/ua_cmdstruct.h"
#include "dird/date_time.h"
#include "cats/sql_pooling.h"
#include "dird/next_vol.h"
#include "dird/ua_db.h"
#include "dird/ua_output.h"
#include "dird/ua_select.h"
#include "lib/edit.h"
#include "lib/parse_conf.h"
#include "dird/jcr_util.h"

#include "job_levels.h"

namespace directordaemon {

/* Imported functions */

/* Forward referenced functions */
static bool DoListCmd(UaContext* ua, const char* cmd, e_list_type llist);
static bool ListNextvol(UaContext* ua, int ndays);
static bool ParseListBackupsCmd(UaContext* ua,
                                const char* range,
                                e_list_type llist);

// Some defaults.
const int kDefaultLogLines = 5;
const int kDefaultNumberOfDays = 50;

// Turn auto display of console messages on/off
bool AutodisplayCmd(UaContext* ua, const char*)
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
      ua->ErrorMsg(T_("ON or OFF keyword missing.\n"));
      break;
  }
  return true;
}

// Turn GUI mode on/off
bool gui_cmd(UaContext* ua, const char*)
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
      ua->ErrorMsg(T_("ON or OFF keyword missing.\n"));
      break;
  }
  return true;
}

// Enter with Resources locked
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
        ua->send->Decoration(T_("Disabled Jobs:\n"));
      }
      ua->send->ArrayItem(job->resource_name_, "   %s\n");
    }
  }

  if (first) { ua->send->Decoration(T_("No disabled Jobs.\n")); }
  ua->send->ArrayEnd("jobs");
}

// Enter with Resources locked
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
        ua->send->Decoration(T_("Disabled Clients:\n"));
      }
      ua->send->ArrayItem(client->resource_name_, "   %s\n");
    }
  }

  if (first) { ua->send->Decoration(T_("No disabled Clients.\n")); }
  ua->send->ArrayEnd("clients");
}

// Enter with Resources locked
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
        ua->send->Decoration(T_("Disabled Schedules:\n"));
      }
      ua->send->ArrayItem(sched->resource_name_, "   %s\n");
    }
  }

  if (first) { ua->send->Decoration(T_("No disabled Schedules.\n")); }
  ua->send->ArrayEnd("schedules");
}

// Enter with Resources locked
static void ShowAll(UaContext* ua, bool hide_sensitive_data, bool verbose)
{
  for (int j = 0; j <= my_config->r_num_ - 1; j++) {
    if (my_config->config_resources_container_->configuration_resources_[j]) {
      my_config->DumpResourceCb_(
          j,
          my_config->config_resources_container_->configuration_resources_[j],
          bsendmsg, ua, hide_sensitive_data, verbose);
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
bool show_cmd(UaContext* ua, const char*)
{
  Dmsg1(20, "show: %s\n", ua->UA_sock->msg);

  /* When the console has no access to the configure cmd then any show cmd
   * will suppress all sensitive information like for instance passwords. */

  bool hide_sensitive_data = !ua->AclAccessOk(Command_ACL, "configure", false);

  bool show_verbose = false;
  if (FindArg(ua, NT_("verbose")) > 0) { show_verbose = true; }

  if (FindArg(ua, "help") > 0) {
    ua->InfoMsg(T_("Keywords for the show command are:\n"));
    for (const auto& command : show_cmd_available_resources) {
      ua->InfoMsg("%s\n", command.first);
    }
    return true;
  }

  ResLocker _{my_config};

  // Without parameter, show all ressources.
  if (ua->argc == 1 || FindArg(ua, "all") > 0) {
    ShowAll(ua, hide_sensitive_data, show_verbose);
    return true;
  }

  for (int i = 1; i < ua->argc; i++) {
    // skip verbose keyword, already handled earlier.
    if (Bstrcasecmp(ua->argk[i], NT_("verbose"))) { continue; }

    if (Bstrcasecmp(ua->argk[i], NT_("disabled"))) {
      ua->send->ObjectStart("disabled");
      if (((i + 1) < ua->argc) && Bstrcasecmp(ua->argk[i + 1], NT_("jobs"))) {
        ShowDisabledJobs(ua);
      } else if (((i + 1) < ua->argc)
                 && Bstrcasecmp(ua->argk[i + 1], NT_("clients"))) {
        ShowDisabledClients(ua);
      } else if (((i + 1) < ua->argc)
                 && Bstrcasecmp(ua->argk[i + 1], NT_("schedules"))) {
        ShowDisabledSchedules(ua);
      } else {
        ShowDisabledJobs(ua);
        ShowDisabledClients(ua);
        ShowDisabledSchedules(ua);
      }
      ua->send->ObjectEnd("disabled");
      return true;
    }

    int type = -1;
    int recurse = 0;
    char* res_name = ua->argk[i];
    int len = strlen(res_name);
    BareosResource* res = nullptr;
    if (!ua->argv[i]) { /* was a name given? */
      // No name, dump all resources of specified type
      recurse = 1;
      for (const auto& command : show_cmd_available_resources) {
        if (bstrncasecmp(res_name, command.first, len)) {
          type = command.second;
          res = my_config->config_resources_container_
                    ->configuration_resources_[type];
          break;
        }
      }
    } else {
      // Dump a single resource with specified name
      recurse = 0;
      for (const auto& command : show_cmd_available_resources) {
        if (bstrncasecmp(res_name, command.first, len)) {
          type = command.second;
          res = (BareosResource*)ua->GetResWithName(type, ua->argv[i], true);
          if (!res) {
            ua->ErrorMsg(T_("%s resource %s not found.\n"), command.first,
                         ua->argv[i]);
            return true;
          }
          break;
        }
      }
    }

    if (type >= 0) {
      my_config->DumpResourceCb_(recurse ? type : -type, res, bsendmsg, ua,
                                 hide_sensitive_data, show_verbose);

    } else {
      ua->ErrorMsg(T_("Resource %s not found\n"), res_name);
    }
  }

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

// @return >0: jobid
// @return 0: neither "jobid" nor "ujobid" parameter is provided.
// @return -1: can't access jobid (because it doesn't exist or no permission to
// access it).
static int GetJobidFromCmdline(UaContext* ua)
{
  JobDbRecord jr{};

  if (const char* jobname = GetArgValue(ua, NT_("ujobid"))) {
    bstrncpy(jr.Job, jobname, MAX_NAME_LENGTH);
    Dmsg1(200, "GetJobidFromCmdline: Selecting ujobid %s from cmdline.\n",
          jr.Job);
  } else if (const char* jobid = GetArgValue(ua, NT_("jobid"))) {
    jr.JobId = str_to_int64(jobid);
    Dmsg1(200, "GetJobidFromCmdline: Selecting jobid %d from cmdline.\n",
          jr.JobId);
  } else {
    Dmsg0(200, "GetJobidFromCmdline: No jobid specified on cmdline.\n");
    return 0;
  }

  if (!ua->db->GetJobRecord(ua->jcr, &jr)) {
    Dmsg0(200, "GetJobidFromCmdline: Failed to get job record.\n");
    return -1;
  }

  Dmsg1(200, "GetJobidFromCmdline: Found job record with jobid %d.\n",
        jr.JobId);

  if (!ua->AclAccessOk(Job_ACL, jr.Name, true)) {
    Dmsg1(200, "GetJobidFromCmdline: No access to Job %s\n", jr.Name);
    return -1;
  }

  if (jr.ClientId) {
    ClientDbRecord cr{};
    cr.ClientId = jr.ClientId;
    if (!ua->db->GetClientRecord(ua->jcr, &cr)) {
      Dmsg1(
          200,
          "GetJobidFromCmdline: Failed to get client record for ClientId %d\n",
          jr.ClientId);
      return -1;
    }

    if (!ua->AclAccessOk(Client_ACL, cr.Name, true)) {
      Dmsg1(200, "GetJobidFromCmdline: No access to Client %s\n", cr.Name);
      return -1;
    }
  }

  return jr.JobId;
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

static void SetQueryRange(std::string& query_range,
                          UaContext* ua,
                          JobDbRecord* jr)
{
  /* See if this is a second call to SetQueryRange() if so and any acl
   * filters have been set we setup a new query_range filter including a
   * limit filter. */
  if (!query_range.empty()) {
    if (!ua->send->has_acl_filters()) { return; }
    query_range.clear();
  }

  // Apply any limit
  if (const char* limit = GetArgValue(ua, NT_("limit"))) {
    try {
      jr->limit = std::stoull(limit);
    } catch (...) {
      Dmsg1(50, "Could not convert %s to limit value.\n", limit);
      jr->limit = 0;
    }
    ua->send->AddLimitFilterTuple(jr->limit);
    query_range.append(" LIMIT " + std::to_string(jr->limit));

    // offset is only valid, if limit is given
    if (const char* offset = GetArgValue(ua, NT_("offset"))) {
      try {
        jr->offset = std::stoull(offset);
      } catch (...) {
        Dmsg1(50, "Could not convert %s to offset value.\n", offset);
        jr->offset = 0;
      }
      ua->send->AddOffsetFilterTuple(jr->offset);
      query_range.append(" OFFSET " + std::to_string(jr->offset));
    }
  }
}

struct ListCmdOptions {
  bool count{};
  bool last{};
  bool current{};
  bool enabled{};
  bool disabled{};
  // jobstatus=X,Y,Z....
  std::vector<char> jobstatuslist{};
  // joblevel=X
  std::vector<char> joblevel_list{};
  // jobtype=X
  std::vector<char> jobtypes{};

  bool parse(UaContext* ua)
  {
    current = FindArg(ua, NT_("current")) >= 0;
    enabled = FindArg(ua, NT_("enabled")) >= 0;
    disabled = FindArg(ua, NT_("disabled")) >= 0;
    count = FindArg(ua, NT_("count")) >= 0;
    last = FindArg(ua, NT_("last")) >= 0;
    if (!GetUserJobStatusSelection(ua, jobstatuslist)) {
      ua->ErrorMsg(T_("invalid jobstatus parameter\n"));
      return false;
    }
    if (!GetUserJobLevelSelection(ua, joblevel_list)) {
      ua->ErrorMsg(T_("invalid joblevel parameter\n"));
      return false;
    }
    if (!GetUserJobTypeListSelection(ua, jobtypes, false)) {
      ua->ErrorMsg(T_("invalid jobtype parameter\n"));
      return false;
    }
    return true;
  }
};

static bool ListMedia(UaContext* ua,
                      e_list_type llist,
                      const ListCmdOptions& optionslist)
{
  JobDbRecord jr;
  std::string query_range;
  SetQueryRange(query_range, ua, &jr);

  // List MEDIA or VOLUMES
  int jobid = GetJobidFromCmdline(ua);
  if (jobid > 0) {
    ua->db->ListVolumesOfJobid(ua->jcr, jobid, ua->send, llist);
  } else if (jobid == 0) {
    MediaDbRecord mr;
    // List a specific volume?
    if (ua->argv[1]) {
      bstrncpy(mr.VolumeName, ua->argv[1], sizeof(mr.VolumeName));
      ua->send->ObjectStart("volume");
      ua->db->ListMediaRecords(ua->jcr, &mr, query_range.c_str(),
                               optionslist.count, ua->send, llist);
      ua->send->ObjectEnd("volume");
    } else {
      /* If no job or jobid keyword found, then we list all media
       * Is a specific pool wanted? */

      PoolDbRecord pr;
      if (const char* pool = GetArgValue(ua, NT_("pool"))) {
        bstrncpy(pr.Name, pool, sizeof(pr.Name));

        if (!GetPoolDbr(ua, &pr)) {
          ua->ErrorMsg(T_("Pool %s doesn't exist.\n"), pool);
          return true;
        }

        SetQueryRange(query_range, ua, &jr);

        mr.PoolId = pr.PoolId;
        ua->send->ArrayStart("volumes");
        ua->db->ListMediaRecords(ua->jcr, &mr, query_range.c_str(),
                                 optionslist.count, ua->send, llist);
        ua->send->ArrayEnd("volumes");
        return true;
      } else {
        int num_pools;
        uint32_t* ids = nullptr;

        // List all volumes, flat
        if (FindArg(ua, NT_("all")) > 0) {
          /* The result of "list media all"
           * does not contain the Pool information,
           * therefore checking the Pool_ACL is not possible.
           * For this reason, we prevent this command. */
          if (ua->AclHasRestrictions(Pool_ACL) && (llist != VERT_LIST)) {
            ua->ErrorMsg(
                T_("Restricted permission. Use the commands 'list media' or "
                   "'llist media all' instead\n"));
            return false;
          }
          ua->send->ArrayStart("volumes");
          SetAclFilter(ua, 4, Pool_ACL); /* PoolName */
          if (optionslist.current) {
            SetResFilter(ua, 4, R_POOL); /* PoolName */
          }
          ua->db->ListMediaRecords(ua->jcr, &mr, query_range.c_str(),
                                   optionslist.count, ua->send, llist);
          ua->send->ArrayEnd("volumes");
        } else {
          // List Volumes in all pools
          if (!ua->db->GetPoolIds(ua->jcr, &num_pools, &ids)) {
            ua->ErrorMsg(T_("Error obtaining pool ids. ERR=%s\n"),
                         ua->db->strerror());
            return true;
          }

          if (num_pools <= 0) {
            if (ids) { free(ids); }
            return true;
          }

          ua->send->ObjectStart("volumes");
          for (int i = 0; i < num_pools; i++) {
            pr.PoolId = ids[i];
            if (ua->db->GetPoolRecord(ua->jcr, &pr)) {
              if (ua->AclAccessOk(Pool_ACL, pr.Name, false)) {
                ua->send->Decoration("Pool: %s\n", pr.Name);
                ua->send->ArrayStart(pr.Name);
                mr.PoolId = ids[i];
                ua->db->ListMediaRecords(ua->jcr, &mr, query_range.c_str(),
                                         optionslist.count, ua->send, llist);
                ua->send->ArrayEnd(pr.Name);
              }
            }
          }
          ua->send->ObjectEnd("volumes");
          if (ids) { free(ids); }
        }
      }
    }
  }

  return true;
}


static bool ListJobs(UaContext* ua,
                     e_list_type llist,
                     ListCmdOptions& optionslist)
{
  // list jobs [...]
  // list job=xxx [...]
  // list jobid=nnn [...]
  // list ujobid=xxx [...]

  JobDbRecord jr;

  if (Bstrcasecmp(ua->argk[1], NT_("jobs")) && ua->argv[1]) {
    ua->ErrorMsg(T_("'list jobs' does not allow an jobs parameter\n"));
    return false;
  }

  if (const char* value; (value = GetArgValue(ua, NT_("jobname")))
                         || (value = GetArgValue(ua, NT_("job")))) {
    jr.JobId = 0;
    bstrncpy(jr.Name, value, MAX_NAME_LENGTH);
  } else if (Bstrcasecmp(ua->argk[1], NT_("job"))
             || Bstrcasecmp(ua->argk[1], NT_("jobname"))) {
    ua->ErrorMsg(T_("Missing %s parameter\n"), NPRT(ua->argk[1]));
    return false;
  }

  int jobid = GetJobidFromCmdline(ua);
  if (jobid > 0) {
    jr.JobId = jobid;
  } else if (jobid < 0) {
    // jobid < 0: jobid does not exist or no permission to access it.
    // This is not treated as error (to keep behavior of prior versions).
    ua->send->ObjectStart("jobs");
    ua->send->ObjectEnd("jobs");
    return true;
  } else /* if (jobid == 0) */ {
    if (Bstrcasecmp(ua->argk[1], NT_("jobid"))
        || Bstrcasecmp(ua->argk[1], NT_("ujobid"))) {
      ua->ErrorMsg(T_("Missing %s parameter\n"), NPRT(ua->argk[1]));
      return false;
    }
  }

  // days or hours given?
  const int secs_in_day = 86400;
  const int secs_in_hour = 3600;

  utime_t now = (utime_t)time(NULL);
  time_t schedtime = 0;
  if (const char* value = GetArgValue(ua, NT_("days"))) {
    int days = str_to_int64(value);
    schedtime = now - secs_in_day * days; /* Days in the past */
  }
  if (const char* value = GetArgValue(ua, NT_("hours"))) {
    int hours = str_to_int64(value);
    schedtime = now - secs_in_hour * hours; /* Hours in the past */
  }

  const char* clientname = nullptr;
  int client_arg = FindClientArgFromDatabase(ua);
  if (client_arg < 0) {
    return false;
  } else if (client_arg > 0) {
    clientname = ua->argv[client_arg];
  }

  const char* volumename = GetArgValue(ua, NT_("volume"));
  const char* poolname = GetArgValue(ua, NT_("pool"));

  switch (llist) {
    case VERT_LIST:
      if (!optionslist.count) {  // count result is one column, no filtering
        SetAclFilter(ua, 2, Job_ACL);
        SetAclFilter(ua, 7, Client_ACL);
        SetAclFilter(ua, 22, Pool_ACL);
        SetAclFilter(ua, 25, FileSet_ACL);
        if (optionslist.current) {
          SetResFilter(ua, 2, R_JOB);
          SetResFilter(ua, 7, R_CLIENT);
          SetResFilter(ua, 22, R_POOL);
          SetResFilter(ua, 25, R_FILESET);
        }
      }
      if (optionslist.enabled) { SetEnabledFilter(ua, 2, R_JOB); }
      if (optionslist.disabled) { SetDisabledFilter(ua, 2, R_JOB); }
      break;
    default:
      if (!optionslist.count) {  // count result is one column, no filtering
        SetAclFilter(ua, 1, Job_ACL);
        SetAclFilter(ua, 2, Client_ACL);
        if (optionslist.current) {
          SetResFilter(ua, 1, R_JOB);
          SetResFilter(ua, 2, R_CLIENT);
        }
      }
      if (optionslist.enabled) { SetEnabledFilter(ua, 1, R_JOB); }
      if (optionslist.disabled) { SetDisabledFilter(ua, 1, R_JOB); }
      break;
  }

  std::string query_range;
  SetQueryRange(query_range, ua, &jr);

  ua->db->ListJobRecords(ua->jcr, &jr, query_range.c_str(), clientname,
                         optionslist.jobstatuslist, optionslist.joblevel_list,
                         optionslist.jobtypes, volumename, poolname, schedtime,
                         optionslist.last, optionslist.count, ua->send, llist);

  return true;
}

static bool DoListCmd(UaContext* ua, const char* cmd, e_list_type llist)
{
  if (!OpenClientDb(ua, true)) { return true; }

  Dmsg1(20, "list: %s\n", cmd);

  if (ua->argc <= 1) {
    ua->ErrorMsg(T_("%s command requires a keyword\n"), NPRT(ua->argk[0]));
    return false;
  }

  ListCmdOptions optionslist{};
  if (!optionslist.parse(ua)) { return false; }

  // Select what to do based on the first argument.
  if (Bstrcasecmp(ua->argk[1], NT_("jobs"))
      || Bstrcasecmp(ua->argk[1], NT_("job"))
      || Bstrcasecmp(ua->argk[1], NT_("jobname"))
      || Bstrcasecmp(ua->argk[1], NT_("jobid"))
      || Bstrcasecmp(ua->argk[1], NT_("ujobid"))) {
    return ListJobs(ua, llist, optionslist);
  }

  JobDbRecord jr;
  std::string query_range;
  SetQueryRange(query_range, ua, &jr);

  if (Bstrcasecmp(ua->argk[1], NT_("jobtotals"))) {
    // List JOBTOTALS
    ua->db->ListJobTotals(ua->jcr, &jr, ua->send);
    return true;
  }

  if (Bstrcasecmp(ua->argk[1], NT_("basefiles"))) {
    // List BASEFILES
    if (int jobid = GetJobidFromCmdline(ua); jobid > 0) {
      ua->db->ListBaseFilesForJob(ua->jcr, jobid, ua->send);
      return true;
    } else {
      ua->ErrorMsg(
          T_("jobid not found in db, access to job or client denied by ACL, or "
             "client not found in db\n"));
      return false;
    }
  }

  if (Bstrcasecmp(ua->argk[1], NT_("files"))) {
    // List FILES
    if (int jobid = GetJobidFromCmdline(ua); jobid > 0) {
      ua->db->ListFilesForJob(ua->jcr, jobid, ua->send);
      return true;
    } else {
      ua->ErrorMsg(
          T_("jobid not found in db, access to job or client denied by ACL, or "
             "client not found in db\n"));
      return false;
    }
  }

  if (Bstrcasecmp(ua->argk[1], NT_("fileset"))) {
    int filesetid = 0;

    // List FileSet
    if (const char* value = GetArgValue(ua, NT_("filesetid"))) {
      filesetid = str_to_int64(value);
    }

    int jobid = GetJobidFromCmdline(ua);
    if (jobid > 0 || filesetid > 0) {
      jr.JobId = jobid;
      jr.FileSetId = filesetid;

      SetAclFilter(ua, 1, FileSet_ACL);
      if (optionslist.current) { SetResFilter(ua, 1, R_FILESET); }

      ua->db->ListFilesets(ua->jcr, &jr, query_range.c_str(), ua->send, llist);
      return true;
    } else {
      ua->ErrorMsg(
          T_("jobid not found in db, access to job or client denied by ACL, or "
             "client not found in db or missing filesetid\n"));
      return false;
    }
  }

  if (Bstrcasecmp(ua->argk[1], NT_("filesets"))) {
    // List FILESETs
    SetAclFilter(ua, 1, FileSet_ACL);
    if (optionslist.current) { SetResFilter(ua, 1, R_FILESET); }

    ua->db->ListFilesets(ua->jcr, &jr, query_range.c_str(), ua->send, llist);
    return true;
  }

  if (Bstrcasecmp(ua->argk[1], NT_("jobmedia"))) {
    // List JOBMEDIA
    if (int jobid = GetJobidFromCmdline(ua); jobid >= 0) {
      ua->db->ListJobmediaRecords(ua->jcr, jobid, ua->send, llist);
      return true;
    } else {
      ua->ErrorMsg(
          T_("jobid not found in db, access to job or client denied by ACL, or "
             "client not found in db\n"));
      return false;
    }
  }

  if (Bstrcasecmp(ua->argk[1], NT_("joblog"))) {
    // List JOBLOG
    if (int jobid = GetJobidFromCmdline(ua); jobid >= 0) {
      ua->db->ListJoblogRecords(ua->jcr, jobid, query_range.c_str(),
                                optionslist.count, ua->send, llist);
      return true;
    } else {
      ua->ErrorMsg(
          T_("jobid not found in db, access to job or client denied by ACL, or "
             "client not found in db\n"));
      return false;
    }
  }

  if (Bstrcasecmp(ua->argk[1], NT_("log"))) {
    /* List last <limit> LOG entries
     * default is DEFAULT_LOG_LINES entries */

    const char* clientname = nullptr;
    int client_arg = FindClientArgFromDatabase(ua);
    if (client_arg < 0) {
      return false;
    } else if (client_arg > 0) {
      clientname = ua->argv[client_arg];
    }

    bool reverse = FindArg(ua, NT_("reverse")) >= 0;

    if (query_range.empty()) {
      query_range = " LIMIT " + std::to_string(kDefaultLogLines);
    }

    if (ua->api != API_MODE_JSON) {
      SetHiddenColumn(ua, 0);                      /* LogId */
      SetHiddenColumnAclFilter(ua, 1, Job_ACL);    /* JobName */
      SetHiddenColumnAclFilter(ua, 2, Client_ACL); /* ClientName */
      SetHiddenColumn(ua, 3);                      /* LogTime */
    } else {
      SetAclFilter(ua, 1, Job_ACL);
      SetAclFilter(ua, 2, Client_ACL);
    }

    ua->db->ListLogRecords(ua->jcr, clientname, query_range.c_str(), reverse,
                           ua->send, llist);
    return true;
  }

  if (Bstrcasecmp(ua->argk[1], NT_("pool"))
      || Bstrcasecmp(ua->argk[1], NT_("pools"))) {
    PoolDbRecord pr;

    // List POOLS
    if (ua->argv[1]) { bstrncpy(pr.Name, ua->argv[1], sizeof(pr.Name)); }

    SetAclFilter(ua, 1, Pool_ACL); /* PoolName */
    if (optionslist.current) { SetResFilter(ua, 1, R_POOL); }

    ua->db->ListPoolRecords(ua->jcr, &pr, ua->send, llist);
    return true;
  }

  if (Bstrcasecmp(ua->argk[1], NT_("poolid")) && ua->argv[1]) {
    PoolDbRecord pr;

    // List POOLS
    if (ua->argv[1]) { pr.PoolId = str_to_int64(ua->argv[1]); }

    SetAclFilter(ua, 1, Pool_ACL);
    if (optionslist.current) { SetResFilter(ua, 1, R_POOL); }

    ua->db->ListPoolRecords(ua->jcr, &pr, ua->send, llist);
    return true;
  }

  if (Bstrcasecmp(ua->argk[1], NT_("clients"))) {
    // List CLIENTS
    SetAclFilter(ua, 1, Client_ACL);
    if (optionslist.current) { SetResFilter(ua, 1, R_CLIENT); }
    if (optionslist.enabled) { SetEnabledFilter(ua, 1, R_CLIENT); }
    if (optionslist.disabled) { SetDisabledFilter(ua, 1, R_CLIENT); }

    ua->db->ListClientRecords(ua->jcr, NULL, ua->send, llist);
    return true;
  }

  if (Bstrcasecmp(ua->argk[1], NT_("client")) && ua->argv[1]) {
    // List CLIENT=xxx
    SetAclFilter(ua, 1, Client_ACL);
    if (optionslist.current) { SetResFilter(ua, 1, R_CLIENT); }
    if (optionslist.enabled) { SetEnabledFilter(ua, 1, R_CLIENT); }
    if (optionslist.disabled) { SetDisabledFilter(ua, 1, R_CLIENT); }

    ua->db->ListClientRecords(ua->jcr, ua->argv[1], ua->send, llist);
    return true;
  }

  if (Bstrcasecmp(ua->argk[1], NT_("storages"))) {
    // List STORAGES
    SetAclFilter(ua, 1, Storage_ACL);
    if (optionslist.current) { SetResFilter(ua, 1, R_STORAGE); }
    if (optionslist.enabled) { SetEnabledFilter(ua, 1, R_STORAGE); }
    if (optionslist.disabled) { SetDisabledFilter(ua, 1, R_STORAGE); }

    ua->db->ListSqlQuery(ua->jcr, "SELECT * FROM Storage", ua->send, llist,
                         "storages");
    return true;
  }

  if (Bstrcasecmp(ua->argk[1], NT_("media"))
      || Bstrcasecmp(ua->argk[1], NT_("volume"))
      || Bstrcasecmp(ua->argk[1], NT_("volumes"))) {
    return ListMedia(ua, llist, optionslist);
  }

  if ((Bstrcasecmp(ua->argk[1], NT_("mediaid"))
       || Bstrcasecmp(ua->argk[1], NT_("volumeid")))
      && ua->argv[1]) {
    MediaDbRecord mr;
    mr.MediaId = str_to_int64(ua->argv[1]);
    ua->send->ObjectStart("volume");
    ua->db->ListMediaRecords(ua->jcr, &mr, query_range.c_str(),
                             optionslist.count, ua->send, llist);
    ua->send->ObjectEnd("volume");
    return true;
  }

  if (Bstrcasecmp(ua->argk[1], NT_("nextvol"))
      || Bstrcasecmp(ua->argk[1], NT_("nextvolume"))) {
    // List next volume
    int days = 1;
    if (const char* value = GetArgValue(ua, NT_("days"))) {
      days = atoi(value);
      if ((days < 0) || (days > kDefaultNumberOfDays)) {
        ua->WarningMsg(T_("Ignoring invalid value for days. Max is %d.\n"),
                       kDefaultNumberOfDays);
        days = 1;
      }
    }
    return ListNextvol(ua, days);
  }

  if (Bstrcasecmp(ua->argk[1], NT_("copies"))) {
    // List copies
    if (const char* value = GetArgValue(ua, NT_("jobid"))) {
      if (Is_a_number_list(value)) {
        ua->db->ListCopiesRecords(ua->jcr, query_range.c_str(), value, ua->send,
                                  llist);
        return true;
      }
    } else {
      ua->db->ListCopiesRecords(ua->jcr, query_range.c_str(), NULL, ua->send,
                                llist);
      return true;
    }
  }

  if (Bstrcasecmp(ua->argk[1], NT_("backups"))) {
    if (ParseListBackupsCmd(ua, query_range.c_str(), llist)) {
      switch (llist) {
        case VERT_LIST:
          SetAclFilter(ua, 2, Job_ACL);
          SetAclFilter(ua, 22, Pool_ACL);
          if (optionslist.current) {
            SetResFilter(ua, 2, R_JOB);
            SetResFilter(ua, 22, R_POOL);
          }
          if (optionslist.enabled) { SetEnabledFilter(ua, 2, R_JOB); }
          if (optionslist.disabled) { SetDisabledFilter(ua, 2, R_JOB); }
          break;
        default:
          SetAclFilter(ua, 1, Job_ACL);
          if (optionslist.current) { SetResFilter(ua, 1, R_JOB); }
          if (optionslist.enabled) { SetEnabledFilter(ua, 1, R_JOB); }
          if (optionslist.disabled) { SetDisabledFilter(ua, 1, R_JOB); }
          break;
      }

      return ua->db->ListSqlQuery(ua->jcr, ua->cmd, ua->send, llist, "backups");
    }
  }

  if (Bstrcasecmp(ua->argk[1], NT_("jobstatistics"))
      || Bstrcasecmp(ua->argk[1], NT_("jobstats"))) {
    if (int jobid = GetJobidFromCmdline(ua); jobid > 0) {
      ua->db->ListJobstatisticsRecords(ua->jcr, jobid, ua->send, llist);
      return true;
    } else {
      ua->ErrorMsg(T_("no jobid given\n"));
      return false;
    }
  }

  ua->ErrorMsg(T_("Unknown list keyword: %s\n"), NPRT(ua->argk[1]));
  return false;
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

      // Try matching the status to an internal Job Termination code.
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

    // Close set if we opened one.
    if (cnt > 0) { PmStrcat(selection, ")"); }
  }

  if (selection.strlen() == 0) {
    // When no explicit Job Termination code specified use default
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

      // Try mapping from text level to internal level.
      for (int i = 0; joblevels[i].name; i++) {
        if (joblevels[i].job_type == JT_BACKUP
            && bstrncasecmp(joblevels[i].name, cur_level, strlen(cur_level))) {
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

    // Close set if we opened one.
    if (cnt > 0) { PmStrcat(selection, ")"); }
  }
  if (selection.strlen() == 0) { selection.strcpy(default_selection); }

  return true;
}

static inline bool parse_fileset_selection_param(PoolMem& selection,
                                                 UaContext* ua,
                                                 bool listall)
{
  PmStrcpy(selection, "");
  if (const char* fileset = GetArgValue(ua, "fileset");
      (fileset != nullptr && Bstrcasecmp(fileset, "any"))
      || (fileset == nullptr && listall)) {
    FilesetResource* fs;
    PoolMem temp(PM_MESSAGE);

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
  } else if (fileset != nullptr) {
    if (!ua->AclAccessOk(FileSet_ACL, fileset, true)) {
      ua->ErrorMsg(T_("Access to specified FileSet not allowed.\n"));
      return false;
    } else {
      selection.bsprintf("AND FileSet='%s' ", fileset);
    }
  }
  return true;
}

static bool ParseListBackupsCmd(UaContext* ua,
                                const char* range,
                                e_list_type llist)
{
  PoolMem temp(PM_MESSAGE), selection(PM_MESSAGE), criteria(PM_MESSAGE);

  const char* clientname = nullptr;
  int client_arg = FindClientArgFromDatabase(ua);
  if (client_arg < 0) {
    return false;
  } else if (client_arg == 0) {
    ua->ErrorMsg(T_("missing parameter: client\n"));
    return false;
  } else if (client_arg > 0) {
    clientname = ua->argv[client_arg];
  }

  /* allow jobtypes 'B' for Backup and 'A' or 'a' for archive (update job
   * doesn't enforce a valid jobtype, so people have 'a' in their catalogs */
  selection.bsprintf("AND Job.Type IN('B', 'A', 'a') AND Client.Name='%s' ",
                     clientname);

  // Build a selection pattern based on the jobstatus and level arguments.
  parse_jobstatus_selection_param(temp, ua, "AND JobStatus IN ('T','W') ");
  PmStrcat(selection, temp.c_str());

  parse_level_selection_param(temp, ua, "");
  PmStrcat(selection, temp.c_str());

  if (!parse_fileset_selection_param(temp, ua, true)) { return false; }
  PmStrcat(selection, temp.c_str());

  // Build a criteria pattern if the order and/or limit argument are given.
  PmStrcpy(criteria, "");
  if (const char* order = GetArgValue(ua, "order")) {
    if (bstrncasecmp(order, "ascending", strlen(order))) {
      PmStrcat(criteria, " ASC");
    } else if (bstrncasecmp(order, "descending", strlen(order))) {
      PmStrcat(criteria, " DESC");
    } else {
      return false;
    }
  }

  // add range settings
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
  JobResource* job{nullptr};
  JobControlRecord* jcr;
  UnifiedStorageResource store;
  if (const char* job_name = GetArgValue(ua, "job")) {
    job = ua->GetJobResWithName(job_name);
    if (!job) {
      Jmsg(ua->jcr, M_ERROR, 0, T_("%s is not a job name.\n"), job_name);
    }
  }
  if (!job) {
    if ((job = select_job_resource(ua)) == NULL) { return false; }
  }

  jcr = NewDirectorJcr(DirdFreeJcr);
  time_t now = time(NULL);
  bool found = false;
  if (job->schedule) {
    for (RunResource* run = job->schedule->run; run; run = run->next) {
      std::optional<time_t> next_scheduled = run->NextScheduleTime(now, ndays);
      if (!next_scheduled.has_value()) { continue; }

      if (!CompleteJcrForJob(jcr, job, run->pool)) {
        found = false;
        break;
      }
      if (!jcr->dir_impl->jr.PoolId) {
        ua->ErrorMsg(T_("Could not find Pool for Job %s\n"),
                     job->resource_name_);
        continue;
      }
      PoolDbRecord pr;
      pr.PoolId = jcr->dir_impl->jr.PoolId;
      if (!ua->db->GetPoolRecord(jcr, &pr)) {
        bstrncpy(pr.Name, "*UnknownPool*", sizeof(pr.Name));
      }
      MediaDbRecord mr;
      mr.PoolId = jcr->dir_impl->jr.PoolId;
      GetJobStorage(&store, job, run);
      SetStorageidInMr(store.store, &mr);
      /* no need to set ScratchPoolId, since we use fnv_no_create_vol */
      if (!FindNextVolumeForAppend(jcr, &mr, 1, NULL, fnv_no_create_vol,
                                   fnv_prune)) {
        ua->ErrorMsg(T_("Could not find next Volume for Job %s (Pool=%s, "
                        "Level=%s).\n"),
                     job->resource_name_, pr.Name,
                     JobLevelToString(run->level));
      } else {
        ua->SendMsg(T_("The next Volume to be used by Job \"%s\" (Pool=%s, "
                       "Level=%s) will be %s\n"),
                    job->resource_name_, pr.Name, JobLevelToString(run->level),
                    mr.VolumeName);
        found = true;
      }
    }
  }
  if (jcr->db) {
    DbSqlClosePooledConnection(jcr, jcr->db);
    jcr->db = NULL;
  }
  FreeJcr(jcr);
  if (!found) {
    ua->ErrorMsg(T_("Could not find next Volume for Job %s.\n"),
                 job->resource_name_);
    return false;
  }
  return true;
}

// Fill in the remaining fields of the jcr as if it is going to run the job.
bool CompleteJcrForJob(JobControlRecord* jcr,
                       JobResource* job,
                       PoolResource* pool)
{
  SetJcrDefaults(jcr, job);
  if (pool) { jcr->dir_impl->res.pool = pool; /* override */ }
  if (jcr->db) {
    Dmsg0(100, "complete_jcr close db\n");
    DbSqlClosePooledConnection(jcr, jcr->db);
    jcr->db = NULL;
  }

  Dmsg0(100, "complete_jcr open db\n");
  jcr->db = GetDatabaseConnection(jcr);
  if (jcr->db == NULL) {
    Jmsg(jcr, M_FATAL, 0, T_("Could not open database \"%s\".\n"),
         jcr->dir_impl->res.catalog->db_name);
    return false;
  }
  PoolDbRecord pr;
  bstrncpy(pr.Name, jcr->dir_impl->res.pool->resource_name_, sizeof(pr.Name));
  DbLocker _{jcr->db};
  while (!jcr->db->GetPoolRecord(jcr, &pr)) { /* get by Name */
    /* Try to create the pool */
    if (CreatePool(jcr, jcr->db, jcr->dir_impl->res.pool, POOL_OP_CREATE) < 0) {
      Jmsg(jcr, M_FATAL, 0, T_("Pool %s not in database. %s\n"), pr.Name,
           jcr->db->strerror());
      if (jcr->db) {
        DbSqlClosePooledConnection(jcr, jcr->db);
        jcr->db = NULL;
      }
      return false;
    } else {
      Jmsg(jcr, M_INFO, 0, T_("Pool %s created in database.\n"), pr.Name);
    }
  }
  jcr->dir_impl->jr.PoolId = pr.PoolId;
  return true;
}

static void ConLockRelease(void*) { Vw(con_lock); }

void DoMessages(UaContext* ua, const char*)
{
  char msg[2000];
  int mlen;
  bool DoTruncate = false;

  // Flush any queued messages.
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
  console_msg_pending = false;
  ua->user_notified_msg_pending = false;
  pthread_cleanup_pop(0);
  Vw(con_lock);
}

bool DotMessagesCmd(UaContext* ua, const char* cmd)
{
  if (console_msg_pending && ua->AclAccessOk(Command_ACL, cmd)
      && ua->auto_display_messages) {
    DoMessages(ua, cmd);
  }
  return true;
}

bool MessagesCmd(UaContext* ua, const char* cmd)
{
  if (console_msg_pending && ua->AclAccessOk(Command_ACL, cmd)) {
    DoMessages(ua, cmd);
  } else {
    ua->send->Decoration(T_("You have no messages.\n"));
  }
  return true;
}

// Callback routine for "filtering" database listing.
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
      if (!row[tuple->u.acl_filter.column]
          || strlen(row[tuple->u.acl_filter.column]) == 0) {
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
      if (!row[tuple->u.res_filter.column]
          || strlen(row[tuple->u.res_filter.column]) == 0) {
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

      if (!row[tuple->u.res_filter.column]
          || strlen(row[tuple->u.res_filter.column]) == 0) {
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

// Callback routine for "printing" database listing
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


/* Format message and send to other end.

 * If the UA_sock is NULL, it means that there is no user
 * agent, so we are being called from BAREOS core. In
 * that case direct the messages to the Job. */
void bmsg(UaContext* ua, const char* fmt, va_list arg_ptr)
{
  BareosSocket* bs = ua->UA_sock;
  POOLMEM* msg = NULL;

  if (bs) { msg = bs->msg; }
  if (!msg) { msg = GetPoolMemory(PM_EMSG); }

  int len = PmVFormat(msg, fmt, arg_ptr);

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

/* The following UA methods are mainly intended for GUI
 * programs */

void UaContext::vSendMsg(int signal,
                         const char* messagetype,
                         const char* fmt,
                         va_list arg_ptr)
{
  /* send current buffer */
  PoolMem message;
  send->SendBuffer();
  if (signal) {
    if (UA_sock && api) UA_sock->signal(signal);
  }
  message.Bvsprintf(fmt, arg_ptr);
  if (console_is_connected) {
    send->message(messagetype, message);
  } else { /* No UA, send to Job */
    Jmsg(jcr, M_INFO, 0, "%s", message.c_str());
  }
}

/* This is a message that should be displayed on the user's
 *  console. */
void UaContext::SendMsg(const char* fmt, ...)
{
  va_list arg_ptr;
  va_start(arg_ptr, fmt);
  vSendMsg(0, NULL, fmt, arg_ptr);
  va_end(arg_ptr);
}

void UaContext::SendRawMsg(const char* msg) { SendMsg("%s", msg); }


/* This is an error condition with a command. The gui should put
 *  up an error or critical dialog box.  The command is aborted. */
void UaContext::ErrorMsg(const char* fmt, ...)
{
  va_list arg_ptr;
  va_start(arg_ptr, fmt);
  vSendMsg(BNET_ERROR_MSG, MSG_TYPE_ERROR, fmt, arg_ptr);
  va_end(arg_ptr);
}

/* This is a warning message, that should bring up a warning
 *  dialog box on the GUI. The command is not aborted, but something
 *  went wrong. */
void UaContext::WarningMsg(const char* fmt, ...)
{
  va_list arg_ptr;
  va_start(arg_ptr, fmt);
  vSendMsg(BNET_WARNING_MSG, MSG_TYPE_WARNING, fmt, arg_ptr);
  va_end(arg_ptr);
}

/* This is an information message that should probably be put
 *  into the status line of a GUI program. */
void UaContext::InfoMsg(const char* fmt, ...)
{
  va_list arg_ptr;
  va_start(arg_ptr, fmt);
  vSendMsg(BNET_INFO_MSG, MSG_TYPE_INFO, fmt, arg_ptr);
  va_end(arg_ptr);
}


void UaContext::SendCmdUsage(const char* message)
{
  PoolMem usage;

  /* send current buffer */
  send->SendBuffer();

  if (cmddef && cmddef->key && cmddef->usage) {
    usage.bsprintf("%s\nUSAGE: %s %s\n", message, cmddef->key, cmddef->usage);
  } else {
    usage.strcpy(message);
  }

  send->message(NULL, usage);
}
} /* namespace directordaemon */
