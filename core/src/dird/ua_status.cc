/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2001-2012 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2016 Planets Communications B.V.
   Copyright (C) 2013-2021 Bareos GmbH & Co. KG

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
// Kern Sibbald, August MMI
/**
 * @file
 * User Agent Status Command
 */

#include "include/bareos.h"
#include "dird.h"
#include "dird/jcr_private.h"
#include "dird/run_hour_validator.h"
#include "dird/dird_globals.h"
#include "dird/fd_cmds.h"
#include "dird/job.h"
#include "dird/ndmp_dma_generic.h"
#include "dird/next_vol.h"
#include "dird/sd_cmds.h"
#include "dird/scheduler.h"
#include "dird/storage.h"

#include "cats/sql_pooling.h"
#include "dird/ua_db.h"
#include "dird/ua_output.h"
#include "dird/ua_select.h"
#include "dird/ua_status.h"
#include "include/auth_protocol_types.h"
#include "lib/edit.h"
#include "lib/recent_job_results_list.h"
#include "lib/parse_conf.h"
#include "lib/util.h"

#include "include/make_unique.h"

#include <memory>

#define DEFAULT_STATUS_SCHED_DAYS 7

namespace directordaemon {

static void ListScheduledJobs(UaContext* ua);
static void ListRunningJobs(UaContext* ua);
static void ListTerminatedJobs(UaContext* ua);
static void ListConnectedClients(UaContext* ua);
static void DoDirectorStatus(UaContext* ua);
static void DoSchedulerStatus(UaContext* ua);
static bool DoSubscriptionStatus(UaContext* ua);
static void DoConfigurationStatus(UaContext* ua);
static void DoAllStatus(UaContext* ua);
static void StatusSlots(UaContext* ua, StorageResource* store);
static void StatusContentApi(UaContext* ua, StorageResource* store);
static void StatusContentJson(UaContext* ua, StorageResource* store);

static char OKdotstatus[] = "1000 OK .status\n";
static char DotStatusJob[] = "JobId=%s JobStatus=%c JobErrors=%d\n";

static void ClientStatus(UaContext* ua, ClientResource* client, char* cmd)
{
  switch (client->Protocol) {
    case APT_NATIVE:
      DoNativeClientStatus(ua, client, cmd);
      break;
    case APT_NDMPV2:
    case APT_NDMPV3:
    case APT_NDMPV4:
      DoNdmpClientStatus(ua, client, cmd);
      break;
    default:
      break;
  }
}

// .status command
bool DotStatusCmd(UaContext* ua, const char* cmd)
{
  StorageResource* store;
  ClientResource* client;
  JobControlRecord* njcr = NULL;
  char ed1[50];
  char* statuscmd = NULL;

  Dmsg2(20, "status=\"%s\" argc=%d\n", cmd, ua->argc);

  if (ua->argc < 2) {
    ua->SendMsg("1900 Bad .status command, missing arguments.\n");
    return false;
  }

  if (Bstrcasecmp(ua->argk[1], "dir")) {
    if (Bstrcasecmp(ua->argk[2], "current")) {
      ua->SendMsg(OKdotstatus, ua->argk[2]);
      foreach_jcr (njcr) {
        if (njcr->JobId != 0
            && ua->AclAccessOk(Job_ACL, njcr->impl->res.job->resource_name_)) {
          ua->SendMsg(DotStatusJob, edit_int64(njcr->JobId, ed1),
                      njcr->JobStatus, njcr->JobErrors);
        }
      }
      endeach_jcr(njcr);
    } else if (Bstrcasecmp(ua->argk[2], "last")) {
      ua->SendMsg(OKdotstatus, ua->argk[2]);
      if (RecentJobResultsList::Count() > 0) {
        RecentJobResultsList::JobResult job
            = RecentJobResultsList::GetMostRecentJobResult();
        if (ua->AclAccessOk(Job_ACL, job.Job)) {
          ua->SendMsg(DotStatusJob, edit_int64(job.JobId, ed1), job.JobStatus,
                      job.Errors);
        }
      }
    } else if (Bstrcasecmp(ua->argk[2], "header")) {
      ListDirStatusHeader(ua);
    } else if (Bstrcasecmp(ua->argk[2], "scheduled")) {
      ListScheduledJobs(ua);
    } else if (Bstrcasecmp(ua->argk[2], "running")) {
      ListRunningJobs(ua);
    } else if (Bstrcasecmp(ua->argk[2], "terminated")) {
      ListTerminatedJobs(ua);
    } else {
      ua->SendMsg("1900 Bad .status command, wrong argument.\n");
      return false;
    }
  } else if (Bstrcasecmp(ua->argk[1], "client")) {
    client = get_client_resource(ua);
    if (client) {
      if (ua->argc == 3) { statuscmd = ua->argk[2]; }
      Dmsg2(200, "Client=%s arg=%s\n", client->resource_name_, NPRT(statuscmd));
      ClientStatus(ua, client, statuscmd);
    }
  } else if (Bstrcasecmp(ua->argk[1], "storage")) {
    store = get_storage_resource(ua);
    if (store) {
      if (ua->argc == 3) { statuscmd = ua->argk[2]; }
      StorageStatus(ua, store, statuscmd);
    }
  } else {
    ua->SendMsg("1900 Bad .status command, wrong argument.\n");
    return false;
  }

  return true;
}

// status command
bool StatusCmd(UaContext* ua, const char* cmd)
{
  StorageResource* store;
  ClientResource* client;
  int item, i;
  bool autochangers_only;

  Dmsg1(20, "status:%s:\n", cmd);

  for (i = 1; i < ua->argc; i++) {
    if (Bstrcasecmp(ua->argk[i], NT_("all"))) {
      DoAllStatus(ua);
      return true;
    } else if (bstrncasecmp(ua->argk[i], NT_("dir"), 3)) {
      DoDirectorStatus(ua);
      return true;
    } else if (Bstrcasecmp(ua->argk[i], NT_("client"))) {
      client = get_client_resource(ua);
      if (client) { ClientStatus(ua, client, NULL); }
      return true;
    } else if (bstrncasecmp(ua->argk[i], NT_("sched"), 5)) {
      DoSchedulerStatus(ua);
      return true;
    } else if (bstrncasecmp(ua->argk[i], NT_("sub"), 3)) {
      if (DoSubscriptionStatus(ua)) {
        return true;
      } else {
        return false;
      }
    } else if (bstrncasecmp(ua->argk[i], NT_("conf"), 4)) {
      DoConfigurationStatus(ua);
      return true;
    } else {
      // limit storages to autochangers if slots is given
      autochangers_only = (FindArg(ua, NT_("slots")) > 0);
      store = get_storage_resource(ua, false, autochangers_only);

      if (store) {
        if (FindArg(ua, NT_("slots")) > 0) {
          switch (ua->api) {
            case API_MODE_OFF:
              StatusSlots(ua, store);
              break;
            case API_MODE_ON:
              StatusContentApi(ua, store);
              break;
            case API_MODE_JSON:
              StatusContentJson(ua, store);
              break;
          }
        } else {
          StorageStatus(ua, store, NULL);
        }
      }

      return true;
    }
  }
  /* If no args, ask for status type */
  if (ua->argc == 1) {
    char prmt[MAX_NAME_LENGTH];

    StartPrompt(ua, _("Status available for:\n"));
    AddPrompt(ua, NT_("Director"));
    AddPrompt(ua, NT_("Storage"));
    AddPrompt(ua, NT_("Client"));
    AddPrompt(ua, NT_("Scheduler"));
    AddPrompt(ua, NT_("All"));
    Dmsg0(20, "DoPrompt: select daemon\n");
    if ((item = DoPrompt(ua, "", _("Select daemon type for status"), prmt,
                         sizeof(prmt)))
        < 0) {
      return true;
    }
    Dmsg1(20, "item=%d\n", item);
    switch (item) {
      case 0: /* Director */
        DoDirectorStatus(ua);
        break;
      case 1:
        store = select_storage_resource(ua);
        if (store) { StorageStatus(ua, store, NULL); }
        break;
      case 2:
        client = select_client_resource(ua);
        if (client) { ClientStatus(ua, client, NULL); }
        break;
      case 3:
        DoSchedulerStatus(ua);
        break;
      case 4:
        DoAllStatus(ua);
        break;
      default:
        break;
    }
  }
  return true;
}

static void DoAllStatus(UaContext* ua)
{
  StorageResource *store, **unique_store;
  ClientResource *client, **unique_client;
  int i, j;
  bool found;
  int32_t previous_JobStatus = 0;

  DoDirectorStatus(ua);

  /* Count Storage items */
  LockRes(my_config);
  i = 0;
  foreach_res (store, R_STORAGE) {
    i++;
  }
  unique_store = (StorageResource**)malloc(i * sizeof(StorageResource));
  /* Find Unique Storage address/port */
  i = 0;
  foreach_res (store, R_STORAGE) {
    found = false;
    if (!ua->AclAccessOk(Storage_ACL, store->resource_name_)) { continue; }
    for (j = 0; j < i; j++) {
      if (bstrcmp(unique_store[j]->address, store->address)
          && unique_store[j]->SDport == store->SDport) {
        found = true;
        break;
      }
    }
    if (!found) {
      unique_store[i++] = store;
      Dmsg2(40, "Stuffing: %s:%d\n", store->address, store->SDport);
    }
  }
  UnlockRes(my_config);

  previous_JobStatus = ua->jcr->JobStatus;

  /* Call each unique Storage daemon */
  for (j = 0; j < i; j++) {
    StorageStatus(ua, unique_store[j], NULL);
    ua->jcr->JobStatus = previous_JobStatus;
  }
  free(unique_store);

  /* Count Client items */
  LockRes(my_config);
  i = 0;
  foreach_res (client, R_CLIENT) {
    i++;
  }
  unique_client = (ClientResource**)malloc(i * sizeof(ClientResource));
  /* Find Unique Client address/port */
  i = 0;
  foreach_res (client, R_CLIENT) {
    found = false;
    if (!ua->AclAccessOk(Client_ACL, client->resource_name_)) { continue; }
    for (j = 0; j < i; j++) {
      if (bstrcmp(unique_client[j]->address, client->address)
          && unique_client[j]->FDport == client->FDport) {
        found = true;
        break;
      }
    }
    if (!found) {
      unique_client[i++] = client;
      Dmsg2(40, "Stuffing: %s:%d\n", client->address, client->FDport);
    }
  }
  UnlockRes(my_config);

  previous_JobStatus = ua->jcr->JobStatus;

  /* Call each unique File daemon */
  for (j = 0; j < i; j++) {
    ClientStatus(ua, unique_client[j], NULL);
    ua->jcr->JobStatus = previous_JobStatus;
  }
  free(unique_client);
}

void ListDirStatusHeader(UaContext* ua)
{
  int len, cnt;
  CatalogResource* catalog;
  char dt[MAX_TIME_LENGTH];
  PoolMem msg(PM_FNAME), dbdrivers(PM_FNAME);

  cnt = 0;
  foreach_res (catalog, R_CATALOG) {
    if (cnt) { dbdrivers.strcat(" "); }
    dbdrivers.strcat(catalog->db_driver);
    cnt++;
  }
  ua->SendMsg(_("%s Version: %s (%s) %s\n"), my_name,
              kBareosVersionStrings.Full, kBareosVersionStrings.Date,
              kBareosVersionStrings.GetOsInfo());
  bstrftime_nc(dt, sizeof(dt), daemon_start_time);
  ua->SendMsg(
      _("Daemon started %s. Jobs: run=%d, running=%d db:%s, %s binary\n"), dt,
      num_jobs_run, JobCount(), dbdrivers.c_str(),
      kBareosVersionStrings.BinaryInfo);

  if (me->secure_erase_cmdline) {
    ua->SendMsg(_(" secure erase command='%s'\n"), me->secure_erase_cmdline);
  }

  len = ListDirPlugins(msg);
  if (len > 0) { ua->SendMsg("%s\n", msg.c_str()); }

  if (my_config->HasWarnings()) {
    ua->SendMsg(
        _("\n"
          "There are WARNINGS for the director configuration!\n"
          "See 'status configuration' for details.\n"
          "\n"));
  }
}

static bool show_scheduled_preview(UaContext* ua,
                                   ScheduleResource* sched,
                                   PoolMem& overview,
                                   int* max_date_len,
                                   time_t time_to_check)
{
  int date_len;
  char dt[MAX_TIME_LENGTH];
  time_t runtime;
  RunResource* run;
  PoolMem temp(PM_NAME);

  RunHourValidator run_hour_validator(time_to_check);

  for (run = sched->run; run; run = run->next) {
    bool run_now;
    int cnt = 0;

    run_now = run_hour_validator.TriggersOn(run->date_time_bitfield);

    if (run_now) {
      // Find time (time_t) job is to be run
      struct tm tm;
      Blocaltime(&time_to_check, &tm); /* Reset tm structure */
      tm.tm_min = run->minute;         /* Set run minute */
      tm.tm_sec = 0;                   /* Zero secs */

      /*
       * Convert the time into a user parsable string.
       * As we use locale specific strings for weekday and month we
       * need to keep track of the longest data string used.
       */
      runtime = mktime(&tm);
      bstrftime_wd(dt, sizeof(dt), runtime);
      date_len = strlen(dt);
      if (date_len > *max_date_len) {
        if (*max_date_len == 0) {
          /*
           * When the datelen changes during the loop the locale generates a
           * date string that is variable. Only thing we can do about that is
           * start from scratch again. We invoke this by return false from this
           * function.
           */
          *max_date_len = date_len;
          PmStrcpy(overview, "");
          return false;
        } else {
          /*
           * This is the first determined length we use this until we are proven
           * wrong.
           */
          *max_date_len = date_len;
        }
      }

      Mmsg(temp, "%-*s  %-22.22s  ", *max_date_len, dt, sched->resource_name_);
      PmStrcat(overview, temp.c_str());

      if (run->level) {
        if (cnt++ > 0) { PmStrcat(overview, " "); }
        Mmsg(temp, "Level=%s", JobLevelToString(run->level));
        PmStrcat(overview, temp.c_str());
      }

      if (run->Priority) {
        if (cnt++ > 0) { PmStrcat(overview, " "); }
        Mmsg(temp, "Priority=%d", run->Priority);
        PmStrcat(overview, temp.c_str());
      }

      if (run->spool_data_set) {
        if (cnt++ > 0) { PmStrcat(overview, " "); }
        Mmsg(temp, "Spool Data=%d", run->spool_data);
        PmStrcat(overview, temp.c_str());
      }

      if (run->accurate_set) {
        if (cnt++ > 0) { PmStrcat(overview, " "); }
        Mmsg(temp, "Accurate=%d", run->accurate);
        PmStrcat(overview, temp.c_str());
      }

      if (run->pool) {
        if (cnt++ > 0) { PmStrcat(overview, " "); }
        Mmsg(temp, "Pool=%s", run->pool->resource_name_);
        PmStrcat(overview, temp.c_str());
      }

      if (run->storage) {
        if (cnt++ > 0) { PmStrcat(overview, " "); }
        Mmsg(temp, "Storage=%s", run->storage->resource_name_);
        PmStrcat(overview, temp.c_str());
      }

      if (run->msgs) {
        if (cnt++ > 0) { PmStrcat(overview, " "); }
        Mmsg(temp, "Messages=%s", run->msgs->resource_name_);
        PmStrcat(overview, temp.c_str());
      }

      PmStrcat(overview, "\n");
    }
  }

  /*
   * If we make it till here the length of the datefield is constant or didn't
   * change.
   */
  return true;
}

/**
 * Check the number of clients in the DB against the configured number of
 * subscriptions
 *
 * Return true if (number of clients < number of subscriptions), else
 * return false
 */
static bool DoSubscriptionStatus(UaContext* ua)
{
  int available;
  bool retval = false;

  // See if we need to check.
  if (me->subscriptions == 0) {
    ua->SendMsg(_("No subscriptions configured in director.\n"));
    retval = true;
    goto bail_out;
  }

  if (me->subscriptions_used <= 0) {
    ua->ErrorMsg(_("No clients defined.\n"));
    goto bail_out;
  } else {
    available = me->subscriptions - me->subscriptions_used;
    if (available < 0) {
      ua->SendMsg(
          _("Warning! No available subscriptions: %d (%d/%d) (used/total)\n"),
          available, me->subscriptions_used, me->subscriptions);
    } else {
      ua->SendMsg(_("Ok: available subscriptions: %d (%d/%d) (used/total)\n"),
                  available, me->subscriptions_used, me->subscriptions);
      retval = true;
    }
  }

bail_out:
  return retval;
}

static void DoConfigurationStatus(UaContext* ua)
{
  if (!ua->AclAccessOk(Command_ACL, "configure")) {
    ua->ErrorMsg(_("%s %s: is an invalid command or needs access right to the"
                   " \"configure\" command.\n"),
                 ua->argk[0], ua->argk[1]);
  } else {
    if (my_config->HasWarnings()) {
      ua->SendMsg(_("Deprecated configuration settings detected:\n"));
      for (auto& warning : my_config->GetWarnings()) {
        ua->SendMsg(" * %s\n", warning.c_str());
      }
    } else {
      ua->SendMsg(_("No deprecated configuration settings detected.\n"));
    }
  }
}

static void DoSchedulerStatus(UaContext* ua)
{
  int i;
  int max_date_len = 0;
  int days = DEFAULT_STATUS_SCHED_DAYS; /* Default days for preview */
  bool schedule_given_by_cmdline_args = false;
  time_t time_to_check, now, start, stop;
  char schedulename[MAX_NAME_LENGTH];
  const int seconds_per_day = 86400; /* Number of seconds in one day */
  const int seconds_per_hour = 3600; /* Number of seconds in one hour */
  ClientResource* client = NULL;
  JobResource* job = NULL;
  ScheduleResource* sched;
  PoolMem overview(PM_MESSAGE);

  now = time(NULL); /* Initialize to now */
  time_to_check = now;

  i = FindArgWithValue(ua, NT_("days"));
  if (i >= 0) {
    days = atoi(ua->argv[i]);
    if (((days < -366) || (days > 366)) && !ua->api) {
      ua->SendMsg(_(
          "Ignoring invalid value for days. Allowed is -366 < days < 366.\n"));
      days = DEFAULT_STATUS_SCHED_DAYS;
    }
  }

  // Schedule given ?
  i = FindArgWithValue(ua, NT_("schedule"));
  if (i >= 0) {
    bstrncpy(schedulename, ua->argv[i], sizeof(schedulename));
    schedule_given_by_cmdline_args = true;
  }

  // Client given ?
  i = FindArgWithValue(ua, NT_("client"));
  if (i >= 0) { client = get_client_resource(ua); }

  // Jobname given ?
  i = FindArgWithValue(ua, NT_("job"));
  if (i >= 0) {
    job = ua->GetJobResWithName(ua->argv[i]);

    // If a bogus jobname was given ask for it interactively.
    if (!job) { job = select_job_resource(ua); }
  }

  ua->SendMsg("Scheduler Jobs:\n\n");
  ua->SendMsg("Schedule               Jobs Triggered\n");
  ua->SendMsg("===========================================================\n");

  LockRes(my_config);
  foreach_res (sched, R_SCHEDULE) {
    int cnt = 0;

    if (!sched->enabled) { continue; }

    if (!ua->AclAccessOk(Schedule_ACL, sched->resource_name_)) { continue; }

    if (schedule_given_by_cmdline_args) {
      if (!bstrcmp(sched->resource_name_, schedulename)) { continue; }
    }

    if (job) {
      if (job->schedule
          && bstrcmp(sched->resource_name_, job->schedule->resource_name_)) {
        if (cnt++ == 0) { ua->SendMsg("%s\n", sched->resource_name_); }
        if (!job->enabled || (job->client && !job->client->enabled)) {
          ua->SendMsg("                       %s (disabled)\n",
                      job->resource_name_);
        } else {
          ua->SendMsg("                       %s\n", job->resource_name_);
        }
      }
    } else {
      foreach_res (job, R_JOB) {
        if (!ua->AclAccessOk(Job_ACL, job->resource_name_)) { continue; }

        if (client && job->client != client) { continue; }

        if (job->schedule
            && bstrcmp(sched->resource_name_, job->schedule->resource_name_)) {
          if (cnt++ == 0) { ua->SendMsg("%s\n", sched->resource_name_); }
          if (!job->enabled || (job->client && !job->client->enabled)) {
            ua->SendMsg("                       %s (disabled)\n",
                        job->resource_name_);
          } else {
            ua->SendMsg("                       %s\n", job->resource_name_);
          }
        }
      }
    }

    if (cnt > 0) { ua->SendMsg("\n"); }
  }
  UnlockRes(my_config);

  // Build an overview.
  if (days > 0) { /* future */
    start = now;
    stop = now + (days * seconds_per_day);
  } else { /* past */
    start = now + (days * seconds_per_day);
    stop = now;
  }

start_again:
  time_to_check = start;
  while (time_to_check < stop) {
    if (client || job) {
      // List specific schedule.
      if (job) {
        if (job->schedule && job->schedule->enabled && job->enabled
            && job->client->enabled) {
          if (!show_scheduled_preview(ua, job->schedule, overview,
                                      &max_date_len, time_to_check)) {
            goto start_again;
          }
        }
      } else {
        LockRes(my_config);
        foreach_res (job, R_JOB) {
          if (!ua->AclAccessOk(Job_ACL, job->resource_name_)) { continue; }

          if (job->schedule && job->schedule->enabled && job->enabled
              && job->client == client && job->client->enabled) {
            if (!show_scheduled_preview(ua, job->schedule, overview,
                                        &max_date_len, time_to_check)) {
              job = NULL;
              UnlockRes(my_config);
              goto start_again;
            }
          }
        }
        UnlockRes(my_config);
        job = NULL;
      }
    } else {
      // List all schedules.
      LockRes(my_config);
      foreach_res (sched, R_SCHEDULE) {
        if (!sched->enabled) { continue; }

        if (!ua->AclAccessOk(Schedule_ACL, sched->resource_name_)) { continue; }

        if (schedule_given_by_cmdline_args) {
          if (!bstrcmp(sched->resource_name_, schedulename)) { continue; }
        }

        if (!show_scheduled_preview(ua, sched, overview, &max_date_len,
                                    time_to_check)) {
          UnlockRes(my_config);
          goto start_again;
        }
      }
      UnlockRes(my_config);
    }

    time_to_check += seconds_per_hour; /* next hour */
  }

  ua->SendMsg("====\n\n");
  ua->SendMsg("Scheduler Preview for %d days:\n\n", days);
  ua->SendMsg("%-*s  %-22s  %s\n", max_date_len, _("Date"), _("Schedule"),
              _("Overrides"));
  ua->SendMsg(
      "==============================================================\n");
  ua->SendMsg(overview.c_str());
  ua->SendMsg("====\n");
}

static void DoDirectorStatus(UaContext* ua)
{
  ListDirStatusHeader(ua);

  // List scheduled Jobs
  ListScheduledJobs(ua);

  // List running jobs
  ListRunningJobs(ua);

  // List terminated jobs
  ListTerminatedJobs(ua);

  ListConnectedClients(ua);

  ua->SendMsg("====\n");
}

static void PrtRunhdr(UaContext* ua)
{
  if (!ua->api) {
    ua->SendMsg(_("\nScheduled Jobs:\n"));
    ua->SendMsg(
        _("Level          Type     Pri  Scheduled          Name               "
          "Volume\n"));
    ua->SendMsg(
        _("===================================================================="
          "===============\n"));
  }
}

/* Scheduling packet */
struct sched_pkt {
  dlink<sched_pkt> link; /* keep this as first item!!! */
  JobResource* job;
  int level;
  int priority;
  utime_t runtime;
  PoolResource* pool;
  StorageResource* store;
};

static void PrtRuntime(UaContext* ua, sched_pkt* sp)
{
  char dt[MAX_TIME_LENGTH];
  const char* level_ptr;
  bool ok = false;
  bool CloseDb = false;
  JobControlRecord* jcr = ua->jcr;
  MediaDbRecord mr;
  int orig_jobtype;

  orig_jobtype = jcr->getJobType();
  if (sp->job->JobType == JT_BACKUP) {
    jcr->db = NULL;
    ok = CompleteJcrForJob(jcr, sp->job, sp->pool);
    Dmsg1(250, "Using pool=%s\n", jcr->impl->res.pool->resource_name_);
    if (jcr->db) { CloseDb = true; /* new db opened, remember to close it */ }
    if (ok) {
      mr.PoolId = jcr->impl->jr.PoolId;
      jcr->impl->res.write_storage = sp->store;
      SetStorageidInMr(jcr->impl->res.write_storage, &mr);
      Dmsg0(250, "call FindNextVolumeForAppend\n");
      /* no need to set ScratchPoolId, since we use fnv_no_create_vol */
      ok = FindNextVolumeForAppend(jcr, &mr, 1, NULL, fnv_no_create_vol,
                                   fnv_no_prune);
    }
    if (!ok) { bstrncpy(mr.VolumeName, "*unknown*", sizeof(mr.VolumeName)); }
  }
  bstrftime_nc(dt, sizeof(dt), sp->runtime);
  switch (sp->job->JobType) {
    case JT_ADMIN:
    case JT_ARCHIVE:
    case JT_RESTORE:
      level_ptr = " ";
      break;
    default:
      level_ptr = JobLevelToString(sp->level);
      break;
  }
  if (ua->api) {
    ua->SendMsg(_("%-14s\t%-8s\t%3d\t%-18s\t%-18s\t%s\n"), level_ptr,
                job_type_to_str(sp->job->JobType), sp->priority, dt,
                sp->job->resource_name_, mr.VolumeName);
  } else {
    ua->SendMsg(_("%-14s %-8s %3d  %-18s %-18s %s\n"), level_ptr,
                job_type_to_str(sp->job->JobType), sp->priority, dt,
                sp->job->resource_name_, mr.VolumeName);
  }
  if (CloseDb) { DbSqlClosePooledConnection(jcr, jcr->db); }
  jcr->db = ua->db; /* restore ua db to jcr */
  jcr->setJobType(orig_jobtype);
}

// Sort items by runtime, priority
static int CompareByRuntimePriority(sched_pkt* p1, sched_pkt* p2)
{
  if (p1->runtime < p2->runtime) {
    return -1;
  } else if (p1->runtime > p2->runtime) {
    return 1;
  }

  if (p1->priority < p2->priority) {
    return -1;
  } else if (p1->priority > p2->priority) {
    return 1;
  }

  return 0;
}

// Find all jobs to be run in roughly the next 24 hours.
static void ListScheduledJobs(UaContext* ua)
{
  utime_t runtime;
  RunResource* run;
  JobResource* job;
  int level, num_jobs = 0;
  int priority;
  bool hdr_printed = false;
  auto sched = std::make_unique<dlist<sched_pkt>>();
  sched_pkt* sp;
  int days, i;

  Dmsg0(200, "enter list_sched_jobs()\n");

  days = 1;
  i = FindArgWithValue(ua, NT_("days"));
  if (i >= 0) {
    days = atoi(ua->argv[i]);
    if (((days < 0) || (days > 500)) && !ua->api) {
      ua->SendMsg(_("Ignoring invalid value for days. Max is 500.\n"));
      days = 1;
    }
  }

  // Loop through all jobs
  LockRes(my_config);
  foreach_res (job, R_JOB) {
    if (!ua->AclAccessOk(Job_ACL, job->resource_name_) || !job->enabled
        || (job->client && !job->client->enabled)) {
      continue;
    }
    for (run = NULL; (run = find_next_run(run, job, runtime, days));) {
      UnifiedStorageResource store;
      level = job->JobLevel;
      if (run->level) { level = run->level; }
      priority = job->Priority;
      if (run->Priority) { priority = run->Priority; }
      if (!hdr_printed) {
        PrtRunhdr(ua);
        hdr_printed = true;
      }
      sp = (sched_pkt*)malloc(sizeof(sched_pkt));
      sp->job = job;
      sp->level = level;
      sp->priority = priority;
      sp->runtime = runtime;
      sp->pool = run->pool;
      GetJobStorage(&store, job, run);
      sp->store = store.store;
      if (sp->store) {
        Dmsg3(250, "job=%s storage=%s MediaType=%s\n", job->resource_name_,
              sp->store->resource_name_, sp->store->media_type);
      } else {
        Dmsg1(250, "job=%s could not get job storage\n", job->resource_name_);
      }
      sched->BinaryInsertMultiple(sp, CompareByRuntimePriority);
      num_jobs++;
    }
  } /* end for loop over resources */
  UnlockRes(my_config);
  foreach_dlist (sp, sched) {
    PrtRuntime(ua, sp);
  }
  if (num_jobs == 0 && !ua->api) { ua->SendMsg(_("No Scheduled Jobs.\n")); }
  if (!ua->api) ua->SendMsg("====\n");
  Dmsg0(200, "Leave list_sched_jobs_runs()\n");
}

static void ListRunningJobs(UaContext* ua)
{
  JobControlRecord* jcr;
  int njobs = 0;
  const char* msg;
  char* emsg; /* edited message */
  char dt[MAX_TIME_LENGTH];
  char level[10];
  bool pool_mem = false;

  Dmsg0(200, "enter list_run_jobs()\n");
  if (!ua->api) ua->SendMsg(_("\nRunning Jobs:\n"));
  foreach_jcr (jcr) {
    if (jcr->JobId == 0) { /* this is us */
      /* this is a console or other control job. We only show console
       * jobs in the status output.
       */
      if (jcr->is_JobType(JT_CONSOLE) && !ua->api) {
        bstrftime_nc(dt, sizeof(dt), jcr->start_time);
        ua->SendMsg(_("Console connected at %s\n"), dt);
      }
      continue;
    }
    njobs++;
  }
  endeach_jcr(jcr);

  if (njobs == 0) {
    // Note the following message is used by external programs -- don't change
    if (!ua->api) ua->SendMsg(_("No Jobs running.\n====\n"));
    Dmsg0(200, "leave list_run_jobs()\n");
    return;
  }
  njobs = 0;
  if (!ua->api) {
    ua->SendMsg(_(" JobId Level   Name                       Status\n"));
    ua->SendMsg(
        _("===================================================================="
          "==\n"));
  }
  foreach_jcr (jcr) {
    if (jcr->JobId == 0
        || !ua->AclAccessOk(Job_ACL, jcr->impl->res.job->resource_name_)) {
      continue;
    }
    njobs++;
    switch (jcr->JobStatus) {
      case JS_Created:
        msg = _("is waiting execution");
        break;
      case JS_Running:
        msg = _("is running");
        break;
      case JS_Blocked:
        msg = _("is blocked");
        break;
      case JS_Terminated:
        msg = _("has terminated");
        break;
      case JS_Warnings:
        msg = _("has terminated with warnings");
        break;
      case JS_ErrorTerminated:
        msg = _("has erred");
        break;
      case JS_Error:
        msg = _("has errors");
        break;
      case JS_FatalError:
        msg = _("has a fatal error");
        break;
      case JS_Differences:
        msg = _("has verify differences");
        break;
      case JS_Canceled:
        msg = _("has been canceled");
        break;
      case JS_WaitFD:
        emsg = (char*)GetPoolMemory(PM_FNAME);
        if (!jcr->impl->res.client) {
          Mmsg(emsg, _("is waiting on Client"));
        } else {
          Mmsg(emsg, _("is waiting on Client %s"),
               jcr->impl->res.client->resource_name_);
        }
        pool_mem = true;
        msg = emsg;
        break;
      case JS_WaitSD:
        emsg = (char*)GetPoolMemory(PM_FNAME);
        if (jcr->impl->res.write_storage) {
          Mmsg(emsg, _("is waiting on Storage \"%s\""),
               jcr->impl->res.write_storage->resource_name_);
        } else if (jcr->impl->res.read_storage) {
          Mmsg(emsg, _("is waiting on Storage \"%s\""),
               jcr->impl->res.read_storage->resource_name_);
        } else {
          Mmsg(emsg, _("is waiting on Storage"));
        }
        pool_mem = true;
        msg = emsg;
        break;
      case JS_WaitStoreRes:
        msg = _("is waiting on max Storage jobs");
        break;
      case JS_WaitClientRes:
        msg = _("is waiting on max Client jobs");
        break;
      case JS_WaitJobRes:
        msg = _("is waiting on max Job jobs");
        break;
      case JS_WaitMaxJobs:
        msg = _("is waiting on max total jobs");
        break;
      case JS_WaitStartTime:
        emsg = (char*)GetPoolMemory(PM_FNAME);
        if (jcr->sched_time) {
          char dt[MAX_TIME_LENGTH];
          bstrftime_nc(dt, sizeof(dt), jcr->sched_time);
          Mmsg(emsg, _("is waiting for its start time at %s"), dt);
        } else {
          Mmsg(emsg, _("is waiting for its start time"));
        }
        pool_mem = true;
        msg = emsg;
        break;
      case JS_WaitPriority:
        msg = _("is waiting for higher priority jobs to finish");
        break;
      case JS_DataCommitting:
        msg = _("SD committing Data");
        break;
      case JS_DataDespooling:
        msg = _("SD despooling Data");
        break;
      case JS_AttrDespooling:
        msg = _("SD despooling Attributes");
        break;
      case JS_AttrInserting:
        msg = _("Dir inserting Attributes");
        break;

      default:
        emsg = (char*)GetPoolMemory(PM_FNAME);
        Mmsg(emsg, _("is in unknown state %c"), jcr->JobStatus);
        pool_mem = true;
        msg = emsg;
        break;
    }
    // Now report Storage daemon status code
    switch (jcr->impl->SDJobStatus) {
      case JS_WaitMount:
        if (pool_mem) {
          FreePoolMemory(emsg);
          pool_mem = false;
        }
        msg = _("is waiting for a mount request");
        break;
      case JS_WaitMedia:
        if (pool_mem) {
          FreePoolMemory(emsg);
          pool_mem = false;
        }
        msg = _("is waiting for an appendable Volume");
        break;
      case JS_WaitFD:
        if (!pool_mem) {
          emsg = (char*)GetPoolMemory(PM_FNAME);
          pool_mem = true;
        }
        if (!jcr->file_bsock) {
          // client initiated connection
          Mmsg(emsg, _("is waiting for Client to connect (Client Initiated "
                       "Connection)"));
        } else if (!jcr->impl->res.client || !jcr->impl->res.write_storage) {
          Mmsg(emsg, _("is waiting for Client to connect to Storage daemon"));
        } else {
          Mmsg(emsg, _("is waiting for Client %s to connect to Storage %s"),
               jcr->impl->res.client->resource_name_,
               jcr->impl->res.write_storage->resource_name_);
        }
        msg = emsg;
        break;
      case JS_DataCommitting:
        msg = _("SD committing Data");
        break;
      case JS_DataDespooling:
        msg = _("SD despooling Data");
        break;
      case JS_AttrDespooling:
        msg = _("SD despooling Attributes");
        break;
      case JS_AttrInserting:
        msg = _("Dir inserting Attributes");
        break;
    }
    switch (jcr->getJobType()) {
      case JT_ADMIN:
      case JT_ARCHIVE:
      case JT_RESTORE:
        bstrncpy(level, "      ", sizeof(level));
        break;
      default:
        bstrncpy(level, JobLevelToString(jcr->getJobLevel()), sizeof(level));
        level[7] = 0;
        break;
    }

    if (ua->api) {
      BashSpaces(jcr->comment);
      ua->SendMsg(_("%6d\t%-6s\t%-20s\t%s\t%s\n"), jcr->JobId, level, jcr->Job,
                  msg, jcr->comment);
      UnbashSpaces(jcr->comment);
    } else {
      ua->SendMsg(_("%6d %-6s  %-20s %s\n"), jcr->JobId, level, jcr->Job, msg);
      /* Display comments if any */
      if (*jcr->comment) {
        ua->SendMsg(_("               %-30s\n"), jcr->comment);
      }
    }

    if (pool_mem) {
      FreePoolMemory(emsg);
      pool_mem = false;
    }
  }
  endeach_jcr(jcr);
  if (!ua->api) ua->SendMsg("====\n");
  Dmsg0(200, "leave list_run_jobs()\n");
}

static void ListTerminatedJobs(UaContext* ua)
{
  char dt[MAX_TIME_LENGTH], b1[30], b2[30];
  char level[10];

  if (RecentJobResultsList::IsEmpty()) {
    if (!ua->api) ua->SendMsg(_("No Terminated Jobs.\n"));
    return;
  }
  if (!ua->api) {
    ua->SendMsg(_("\nTerminated Jobs:\n"));
    ua->SendMsg(
        _(" JobId  Level    Files      Bytes   Status   Finished        Name "
          "\n"));
    ua->SendMsg(
        _("===================================================================="
          "\n"));
  }

  for (const RecentJobResultsList::JobResult& je :
       RecentJobResultsList::Get()) {
    char JobName[MAX_NAME_LENGTH];
    const char* termstat;

    bstrncpy(JobName, je.Job, sizeof(JobName));
    /* There are three periods after the Job name */
    char* p;
    for (int i = 0; i < 3; i++) {
      if ((p = strrchr(JobName, '.')) != NULL) { *p = 0; }
    }

    if (!ua->AclAccessOk(Job_ACL, JobName)) { continue; }

    bstrftime_nc(dt, sizeof(dt), je.end_time);
    switch (je.JobType) {
      case JT_ADMIN:
      case JT_ARCHIVE:
      case JT_RESTORE:
        bstrncpy(level, "    ", sizeof(level));
        break;
      default:
        bstrncpy(level, JobLevelToString(je.JobLevel), sizeof(level));
        level[4] = 0;
        break;
    }
    switch (je.JobStatus) {
      case JS_Created:
        termstat = _("Created");
        break;
      case JS_FatalError:
      case JS_ErrorTerminated:
        termstat = _("Error");
        break;
      case JS_Differences:
        termstat = _("Diffs");
        break;
      case JS_Canceled:
        termstat = _("Cancel");
        break;
      case JS_Terminated:
        termstat = _("OK");
        break;
      case JS_Warnings:
        termstat = _("OK -- with warnings");
        break;
      default:
        termstat = _("Other");
        break;
    }
    if (ua->api) {
      ua->SendMsg(_("%6d\t%-6s\t%8s\t%10s\t%-7s\t%-8s\t%s\n"), je.JobId, level,
                  edit_uint64_with_commas(je.JobFiles, b1),
                  edit_uint64_with_suffix(je.JobBytes, b2), termstat, dt,
                  JobName);
    } else {
      ua->SendMsg(_("%6d  %-6s %8s %10s  %-7s  %-8s %s\n"), je.JobId, level,
                  edit_uint64_with_commas(je.JobFiles, b1),
                  edit_uint64_with_suffix(je.JobBytes, b2), termstat, dt,
                  JobName);
    }
  }
  if (!ua->api) ua->SendMsg(_("\n"));
}


static void ListConnectedClients(UaContext* ua)
{
  Connection* connection = NULL;
  alist<Connection*>* connections = NULL;
  const char* separator = "====================";
  char dt[MAX_TIME_LENGTH];

  ua->send->Decoration("\n");
  ua->send->Decoration("Client Initiated Connections (waiting for jobs):\n");
  connections = get_client_connections()->get_as_alist();
  ua->send->Decoration("%-20s%-20s%-20s%-40s\n", "Connect time", "Protocol",
                       "Authenticated", "Name");
  ua->send->Decoration("%-20s%-20s%-20s%-20s%-20s\n", separator, separator,
                       separator, separator, separator);
  ua->send->ArrayStart("client-connection");
  foreach_alist (connection, connections) {
    ua->send->ObjectStart();
    bstrftime_nc(dt, sizeof(dt), connection->ConnectTime());
    ua->send->ObjectKeyValue("ConnectTime", dt, "%-20s");
    ua->send->ObjectKeyValue("protocol_version", connection->protocol_version(),
                             "%-20d");
    ua->send->ObjectKeyValue("authenticated", connection->authenticated(),
                             "%-20d");
    ua->send->ObjectKeyValue("name", connection->name(), "%-40s");
    ua->send->ObjectEnd();
    ua->send->Decoration("\n");
  }
  ua->send->ArrayEnd("client-connection");
}

static void ContentSendInfoApi(UaContext* ua,
                               char type,
                               int Slot,
                               char* vol_name)
{
  char ed1[50], ed2[50], ed3[50];
  PoolDbRecord pr;
  MediaDbRecord mr;
  /* Type|Slot|RealSlot|Volume|Bytes|Status|MediaType|Pool|LastW|Expire */
  const char* slot_api_full_format = "%c|%hd|%hd|%s|%s|%s|%s|%s|%s|%s\n";

  bstrncpy(mr.VolumeName, vol_name, sizeof(mr.VolumeName));
  if (ua->db->GetMediaRecord(ua->jcr, &mr)) {
    pr.PoolId = mr.PoolId;
    if (!ua->db->GetPoolRecord(ua->jcr, &pr)) { strcpy(pr.Name, "?"); }
    ua->SendMsg(slot_api_full_format, type, Slot, mr.Slot, mr.VolumeName,
                edit_uint64(mr.VolBytes, ed1), mr.VolStatus, mr.MediaType,
                pr.Name, edit_uint64(mr.LastWritten, ed2),
                edit_uint64(mr.LastWritten + mr.VolRetention, ed3));
  } else { /* Media unknown */
    ua->SendMsg(slot_api_full_format, type, Slot, 0, mr.VolumeName, "?", "?",
                "?", "?", "0", "0");
  }
}

static void ContentSendInfoJson(UaContext* ua,
                                const char* type,
                                int Slot,
                                char* vol_name)
{
  PoolDbRecord pr;
  MediaDbRecord mr;

  bstrncpy(mr.VolumeName, vol_name, sizeof(mr.VolumeName));
  if (ua->db->GetMediaRecord(ua->jcr, &mr)) {
    pr.PoolId = mr.PoolId;
    if (!ua->db->GetPoolRecord(ua->jcr, &pr)) { strcpy(pr.Name, "?"); }

    ua->send->ObjectStart();
    ua->send->ObjectKeyValue("type", type, "%s\n");
    ua->send->ObjectKeyValue("slotnr", Slot, "%hd\n");
    ua->send->ObjectKeyValue("content", "full", "%s\n");
    ua->send->ObjectKeyValue("mr_slotnr", mr.Slot, "%lld\n");
    ua->send->ObjectKeyValue("mr_volname", mr.VolumeName, "%s\n");
    ua->send->ObjectKeyValue("mr_volbytes", mr.VolBytes, "%lld\n");
    ua->send->ObjectKeyValue("mr_volstatus", mr.VolStatus, "%s\n");
    ua->send->ObjectKeyValue("mr_mediatype", mr.MediaType, "%s\n");
    ua->send->ObjectKeyValue("pr_name", pr.Name, "%s\n");
    ua->send->ObjectKeyValue("mr_lastwritten", mr.LastWritten, "%lld\n");
    ua->send->ObjectKeyValue("mr_expire", mr.LastWritten + mr.VolRetention,
                             "%lld\n");
    ua->send->ObjectEnd();
  } else { /* Media unknown */
    ua->send->ObjectStart();
    ua->send->ObjectKeyValue("type", type, "%s\n");
    ua->send->ObjectKeyValue("slotnr", Slot, "%hd\n");
    ua->send->ObjectKeyValue("content", "full", "%s\n");
    ua->send->ObjectKeyValue("mr_slotnr", (uint64_t)0, "%lld\n");
    ua->send->ObjectKeyValue("mr_volname", mr.VolumeName, "%s\n");
    ua->send->ObjectKeyValue("mr_volbytes", "?", "%s\n");
    ua->send->ObjectKeyValue("mr_volstatus", "?", "%s\n");
    ua->send->ObjectKeyValue("mr_mediatype", "?", "%s\n");
    ua->send->ObjectKeyValue("pr_name", "?", "%s\n");
    ua->send->ObjectKeyValue("mr_lastwritten", (uint64_t)0, "%lld\n");
    ua->send->ObjectKeyValue("mr_expire", (uint64_t)0, "%lld\n");
    ua->send->ObjectEnd();
  }
}

/**
 * Input (output of mxt-changer listall):
 *
 * Drive content:         D:Drive num:F:Slot loaded:Volume Name
 * D:0:F:2:vol2        or D:Drive num:E
 * D:1:F:42:vol42
 * D:3:E
 *
 * Slot content:
 * S:1:F:vol1             S:Slot num:F:Volume Name
 * S:2:E               or S:Slot num:E
 * S:3:F:vol4
 *
 * Import/Export tray slots:
 * I:10:F:vol10           I:Slot num:F:Volume Name
 * I:11:E              or I:Slot num:E
 * I:12:F:vol40
 *
 * If a drive is loaded, the slot *should* be empty
 *
 * Output:
 *
 * Drive list:       D|Drive num|Slot loaded|Volume Name
 * D|0|45|vol45
 * D|1|42|vol42
 * D|3||
 *
 * Slot list: Type|Slot|RealSlot|Volume|Bytes|Status|MediaType|Pool|LastW|Expire
 *
 * S|1|1|vol1|31417344|Full|LTO1-ANSI|Inc|1250858902|1282394902
 * S|2||||||||
 * S|3|3|vol4|15869952|Append|LTO1-ANSI|Inc|1250858907|1282394907
 */
static void StatusContentApi(UaContext* ua, StorageResource* store)
{
  vol_list_t *vl1, *vl2;
  changer_vol_list_t* vol_list = NULL;
  const char* slot_api_drive_full_format = "%c|%hd|%hd|%s\n";
  const char* slot_api_drive_empty_format = "%c|%hd||\n";
  const char* slot_api_slot_empty_format = "%c|%hd||||||||\n";

  if (!OpenClientDb(ua)) { return; }

  vol_list = get_vol_list_from_storage(ua, store, true /* listall */,
                                       true /* want to see all slots */);
  if (!vol_list) {
    ua->WarningMsg(_("No Volumes found, or no barcodes.\n"));
    goto bail_out;
  }

  foreach_dlist (vl1, vol_list->contents) {
    switch (vl1->slot_type) {
      case slot_type_t::kSlotTypeDrive:
        switch (vl1->slot_status) {
          case slot_status_t::kSlotStatusFull:
            ua->SendMsg(slot_api_drive_full_format, 'D',
                        vl1->bareos_slot_number,
                        vl1->currently_loaded_slot_number, vl1->VolName);
            break;
          case slot_status_t::kSlotStatusEmpty:
            ua->SendMsg(slot_api_drive_empty_format, 'D',
                        vl1->bareos_slot_number);
            break;
          default:
            break;
        }
        break;
      case slot_type_t::kSlotTypeStorage:
      case slot_type_t::kSlotTypeImport:
        switch (vl1->slot_status) {
          case slot_status_t::kSlotStatusFull:
            switch (vl1->slot_type) {
              case slot_type_t::kSlotTypeStorage:
                ContentSendInfoApi(ua, 'S', vl1->bareos_slot_number,
                                   vl1->VolName);
                break;
              case slot_type_t::kSlotTypeImport:
                ContentSendInfoApi(ua, 'I', vl1->bareos_slot_number,
                                   vl1->VolName);
                break;
              default:
                break;
            }
            break;
          case slot_status_t::kSlotStatusEmpty:
            /*
             * See if this empty slot is empty because the volume is loaded
             * in one of the drives.
             */
            vl2 = vol_is_loaded_in_drive(store, vol_list,
                                         vl1->bareos_slot_number);
            if (vl2) {
              switch (vl1->slot_type) {
                case slot_type_t::kSlotTypeStorage:
                  ContentSendInfoApi(ua, 'S', vl1->bareos_slot_number,
                                     vl2->VolName);
                  break;
                case slot_type_t::kSlotTypeImport:
                  ContentSendInfoApi(ua, 'I', vl1->bareos_slot_number,
                                     vl2->VolName);
                  break;
                default:
                  break;
              }
              continue;
            }

            switch (vl1->slot_type) {
              case slot_type_t::kSlotTypeStorage:
                ua->SendMsg(slot_api_slot_empty_format, 'S',
                            vl1->bareos_slot_number);
                break;
              case slot_type_t::kSlotTypeImport:
                ua->SendMsg(slot_api_slot_empty_format, 'I',
                            vl1->bareos_slot_number);
                break;
              default:
                break;
            }
            break;
          default:
            break;
        }
        break;
      default:
        break;
    }
  }

bail_out:
  if (vol_list) { StorageReleaseVolList(store, vol_list); }
  CloseSdBsock(ua);

  return;
}

static void StatusContentJson(UaContext* ua, StorageResource* store)
{
  vol_list_t *vl1, *vl2;
  changer_vol_list_t* vol_list = NULL;

  if (!OpenClientDb(ua)) { return; }

  vol_list = get_vol_list_from_storage(ua, store, true /* listall */,
                                       true /* want to see all slots */);
  if (!vol_list) {
    ua->WarningMsg(_("No Volumes found, or no barcodes.\n"));
    goto bail_out;
  }

  ua->send->ArrayStart("contents");
  foreach_dlist (vl1, vol_list->contents) {
    switch (vl1->slot_type) {
      case slot_type_t::kSlotTypeDrive:
        ua->send->ObjectStart();
        ua->send->ObjectKeyValue("type", "drive", "%s\n");
        ua->send->ObjectKeyValue("slotnr", vl1->bareos_slot_number, "%hd\n");
        switch (vl1->slot_status) {
          case slot_status_t::kSlotStatusFull:
            ua->send->ObjectKeyValue("content", "full", "%s\n");
            ua->send->ObjectKeyValue(
                "loaded", vl1->currently_loaded_slot_number, "%hd\n");
            ua->send->ObjectKeyValue("volname", vl1->VolName, "%s\n");
            break;
          case slot_status_t::kSlotStatusEmpty:
            ua->send->ObjectKeyValue("content", "empty", "%s\n");
            break;
          default:
            break;
        }
        ua->send->ObjectEnd();
        break;
      case slot_type_t::kSlotTypeStorage:
      case slot_type_t::kSlotTypeImport:
        switch (vl1->slot_status) {
          case slot_status_t::kSlotStatusFull:
            switch (vl1->slot_type) {
              case slot_type_t::kSlotTypeStorage:
                ContentSendInfoJson(ua, "slot", vl1->bareos_slot_number,
                                    vl1->VolName);
                break;
              case slot_type_t::kSlotTypeImport:
                ContentSendInfoJson(ua, "import_slot", vl1->bareos_slot_number,
                                    vl1->VolName);
                break;
              default:
                break;
            }
            break;
          case slot_status_t::kSlotStatusEmpty:
            /*
             * See if this empty slot is empty because the volume is loaded
             * in one of the drives.
             */
            vl2 = vol_is_loaded_in_drive(store, vol_list,
                                         vl1->bareos_slot_number);
            if (vl2) {
              switch (vl1->slot_type) {
                case slot_type_t::kSlotTypeStorage:
                  ContentSendInfoJson(ua, "slot", vl1->bareos_slot_number,
                                      vl2->VolName);
                  break;
                case slot_type_t::kSlotTypeImport:
                  ContentSendInfoJson(ua, "import_slot",
                                      vl1->bareos_slot_number, vl2->VolName);
                  break;
                default:
                  break;
              }
              continue;
            }

            switch (vl1->slot_type) {
              case slot_type_t::kSlotTypeStorage:
                ua->send->ObjectStart();
                ua->send->ObjectKeyValue("type", "slot", "%s\n");
                ua->send->ObjectKeyValue("slotnr", vl1->bareos_slot_number,
                                         "%hd\n");
                ua->send->ObjectKeyValue("content", "empty", "%s\n");
                ua->send->ObjectEnd();
                break;
              case slot_type_t::kSlotTypeImport:
                ua->send->ObjectStart();
                ua->send->ObjectKeyValue("type", "import_slot", "%s\n");
                ua->send->ObjectKeyValue("slotnr", vl1->bareos_slot_number,
                                         "%hd\n");
                ua->send->ObjectKeyValue("content", "empty", "%s\n");
                ua->send->ObjectEnd();
                break;
              default:
                break;
            }
            break;
          default:
            break;
        }
        break;
      default:
        break;
    }
  }
  ua->send->ArrayEnd("contents");

bail_out:
  if (vol_list) { StorageReleaseVolList(store, vol_list); }
  CloseSdBsock(ua);

  return;
}

// Print slots from AutoChanger
static void StatusSlots(UaContext* ua, StorageResource* store)
{
  char* slot_list;
  vol_list_t *vl1, *vl2;
  slot_number_t max_slots;
  changer_vol_list_t* vol_list = NULL;

  ua->jcr->impl->res.write_storage = store;

  // Slot | Volume | Status | MediaType | Pool
  const char* slot_hformat = " %4i%c| %16s | %9s | %14s | %24s |\n";

  if (!OpenClientDb(ua)) { return; }


  max_slots = GetNumSlots(ua, store);
  if (max_slots <= 0) {
    ua->WarningMsg(_("No slots in changer to scan.\n"));
    return;
  }

  slot_list = (char*)malloc(NbytesForBits(max_slots));
  ClearAllBits(max_slots, slot_list);
  if (!GetUserSlotList(ua, slot_list, "slots", max_slots)) {
    free(slot_list);
    return;
  }

  vol_list = get_vol_list_from_storage(ua, store, true /* listall */,
                                       true /* want to see all slots */);
  if (!vol_list) {
    ua->WarningMsg(_("No Volumes found, or no barcodes.\n"));
    goto bail_out;
  }
  ua->SendMsg(
      _(" Slot |   Volume Name    |   Status  |  Media Type    |         Pool  "
        "           |\n"));
  ua->SendMsg(
      _("------+------------------+-----------+----------------+---------------"
        "-----------|\n"));

  /*
   * Walk through the list getting the media records
   * Slots start numbering at 1.
   */
  foreach_dlist (vl1, vol_list->contents) {
    vl2 = NULL;
    switch (vl1->slot_type) {
      case slot_type_t::kSlotTypeDrive:
        // We are not interested in drive slots.
        continue;
      case slot_type_t::kSlotTypeStorage:
      case slot_type_t::kSlotTypeImport:
        if (vl1->bareos_slot_number > max_slots) {
          ua->WarningMsg(_("Slot %hd greater than max %hd ignored.\n"),
                         vl1->bareos_slot_number, max_slots);
          continue;
        }
        // Check if user wants us to look at this slot
        if (!BitIsSet(vl1->bareos_slot_number - 1, slot_list)) {
          Dmsg1(100, "Skipping slot=%hd\n", vl1->bareos_slot_number);
          continue;
        }

        switch (vl1->slot_status) {
          case slot_status_t::kSlotStatusEmpty:
            if (vl1->slot_type == slot_type_t::kSlotTypeStorage) {
              /*
               * See if this empty slot is empty because the volume is loaded
               * in one of the drives.
               */
              vl2 = vol_is_loaded_in_drive(store, vol_list,
                                           vl1->bareos_slot_number);
              if (!vl2) {
                ua->SendMsg(slot_hformat, vl1->bareos_slot_number, '*', "?",
                            "?", "?", "?");
                continue;
              }
            } else {
              ua->SendMsg(slot_hformat, vl1->bareos_slot_number, '@', "?", "?",
                          "?", "?");
              continue;
            }
            FALLTHROUGH_INTENDED;
          case slot_status_t::kSlotStatusFull: {
            /*
             * We get here for all slots with content and for empty
             * slots with their volume loaded in a drive.
             */
            MediaDbRecord mr;
            if (vl1->slot_status == slot_status_t::kSlotStatusFull) {
              if (!vl1->VolName) {
                Dmsg1(100, "No VolName for Slot=%hd.\n",
                      vl1->bareos_slot_number);
                ua->SendMsg(slot_hformat, vl1->bareos_slot_number,
                            (vl1->slot_type == slot_type_t::kSlotTypeImport)
                                ? '@'
                                : '*',
                            "?", "?", "?", "?");
                continue;
              }

              bstrncpy(mr.VolumeName, vl1->VolName, sizeof(mr.VolumeName));
            } else {
              if (!vl2 || !vl2->VolName) {
                Dmsg1(100, "No VolName for Slot=%hd.\n",
                      vl1->bareos_slot_number);
                ua->SendMsg(slot_hformat, vl1->bareos_slot_number,
                            (vl1->slot_type == slot_type_t::kSlotTypeImport)
                                ? '@'
                                : '*',
                            "?", "?", "?", "?");
                continue;
              }

              bstrncpy(mr.VolumeName, vl2->VolName, sizeof(mr.VolumeName));
            }

            if (mr.VolumeName[0] && ua->db->GetMediaRecord(ua->jcr, &mr)) {
              PoolDbRecord pr;
              pr.PoolId = mr.PoolId;
              if (!ua->db->GetPoolRecord(ua->jcr, &pr)) {
                strcpy(pr.Name, "?");
              }

              // Print information
              if (vl1->slot_type == slot_type_t::kSlotTypeImport) {
                ua->SendMsg(slot_hformat, vl1->bareos_slot_number, '@',
                            mr.VolumeName, mr.VolStatus, mr.MediaType, pr.Name);
              } else {
                ua->SendMsg(
                    slot_hformat, vl1->bareos_slot_number,
                    ((vl1->bareos_slot_number == mr.Slot) ? (vl2 ? '%' : ' ')
                                                          : '*'),
                    mr.VolumeName, mr.VolStatus, mr.MediaType, pr.Name);
              }
            } else {
              ua->SendMsg(
                  slot_hformat, vl1->bareos_slot_number,
                  (vl1->slot_type == slot_type_t::kSlotTypeImport) ? '@' : '*',
                  mr.VolumeName, "?", "?", "?");
            }
            break;
          }
          default:
            break;
        }
        break;
      default:
        break;
    }
  }

bail_out:
  if (vol_list) { StorageReleaseVolList(store, vol_list); }
  free(slot_list);
  CloseSdBsock(ua);

  return;
}
} /* namespace directordaemon */
