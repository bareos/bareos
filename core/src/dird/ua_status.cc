/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2001-2012 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2016 Planets Communications B.V.
   Copyright (C) 2013-2018 Bareos GmbH & Co. KG

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
 * Kern Sibbald, August MMI
 */
/**
 * @file
 * User Agent Status Command
 */

#include "include/bareos.h"
#include "dird.h"
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
#include "lib/edit.h"

#define DEFAULT_STATUS_SCHED_DAYS 7

extern void *start_heap;

static void list_scheduled_jobs(UaContext *ua);
static void list_running_jobs(UaContext *ua);
static void list_terminated_jobs(UaContext *ua);
static void list_connected_clients(UaContext *ua);
static void do_director_status(UaContext *ua);
static void do_scheduler_status(UaContext *ua);
static bool do_subscription_status(UaContext *ua);
static void do_all_status(UaContext *ua);
static void status_slots(UaContext *ua, StoreResource *store);
static void status_content_api(UaContext *ua, StoreResource *store);
static void status_content_json(UaContext *ua, StoreResource *store);

static char OKdotstatus[] =
   "1000 OK .status\n";
static char DotStatusJob[] =
   "JobId=%s JobStatus=%c JobErrors=%d\n";

static void client_status(UaContext *ua, ClientResource *client, char *cmd)
{
   switch (client->Protocol) {
   case APT_NATIVE:
      do_native_client_status(ua, client, cmd);
      break;
   case APT_NDMPV2:
   case APT_NDMPV3:
   case APT_NDMPV4:
      do_ndmp_client_status(ua, client, cmd);
      break;
   default:
      break;
   }
}

/**
 * .status command
 */
bool dot_status_cmd(UaContext *ua, const char *cmd)
{
   StoreResource *store;
   ClientResource *client;
   JobControlRecord* njcr = NULL;
   s_last_job* job;
   char ed1[50];
   char *statuscmd = NULL;

   Dmsg2(20, "status=\"%s\" argc=%d\n", cmd, ua->argc);

   if (ua->argc < 2) {
      ua->send_msg("1900 Bad .status command, missing arguments.\n");
      return false;
   }

   if (bstrcasecmp(ua->argk[1], "dir")) {
      if (bstrcasecmp(ua->argk[2], "current")) {
         ua->send_msg(OKdotstatus, ua->argk[2]);
         foreach_jcr(njcr) {
            if (njcr->JobId != 0 && ua->acl_access_ok(Job_ACL, njcr->res.job->name())) {
               ua->send_msg(DotStatusJob, edit_int64(njcr->JobId, ed1), njcr->JobStatus, njcr->JobErrors);
            }
         }
         endeach_jcr(njcr);
      } else if (bstrcasecmp(ua->argk[2], "last")) {
         ua->send_msg(OKdotstatus, ua->argk[2]);
         if ((last_jobs) && (last_jobs->size() > 0)) {
            job = (s_last_job*)last_jobs->last();
            if (ua->acl_access_ok(Job_ACL, job->Job)) {
               ua->send_msg(DotStatusJob, edit_int64(job->JobId, ed1), job->JobStatus, job->Errors);
            }
         }
      } else if (bstrcasecmp(ua->argk[2], "header")) {
          list_dir_status_header(ua);
      } else if (bstrcasecmp(ua->argk[2], "scheduled")) {
          list_scheduled_jobs(ua);
      } else if (bstrcasecmp(ua->argk[2], "running")) {
          list_running_jobs(ua);
      } else if (bstrcasecmp(ua->argk[2], "terminated")) {
          list_terminated_jobs(ua);
      } else {
         ua->send_msg("1900 Bad .status command, wrong argument.\n");
         return false;
      }
   } else if (bstrcasecmp(ua->argk[1], "client")) {
      client = get_client_resource(ua);
      if (client) {
         if (ua->argc == 3) {
            statuscmd = ua->argk[2];
         }
         Dmsg2(200, "Client=%s arg=%s\n", client->name(), NPRT(statuscmd));
         client_status(ua, client, statuscmd);
      }
   } else if (bstrcasecmp(ua->argk[1], "storage")) {
      store = get_storage_resource(ua);
      if (store) {
         if (ua->argc == 3) {
            statuscmd = ua->argk[2];
         }
         storage_status(ua, store, statuscmd);
      }
   } else {
      ua->send_msg("1900 Bad .status command, wrong argument.\n");
      return false;
   }

   return true;
}

/**
 * status command
 */
bool status_cmd(UaContext *ua, const char *cmd)
{
   StoreResource *store;
   ClientResource *client;
   int item, i;
   bool autochangers_only;

   Dmsg1(20, "status:%s:\n", cmd);

   for (i = 1; i < ua->argc; i++) {
      if (bstrcasecmp(ua->argk[i], NT_("all"))) {
         do_all_status(ua);
         return true;
      } else if (bstrncasecmp(ua->argk[i], NT_("dir"), 3)) {
         do_director_status(ua);
         return true;
      } else if (bstrcasecmp(ua->argk[i], NT_("client"))) {
         client = get_client_resource(ua);
         if (client) {
            client_status(ua, client, NULL);
         }
         return true;
      } else if (bstrncasecmp(ua->argk[i], NT_("sched"), 5)) {
         do_scheduler_status(ua);
         return true;
      } else if (bstrncasecmp(ua->argk[i], NT_("sub"), 3)) {
         if (do_subscription_status(ua)) {
            return true;
         } else {
            return false;
         }
      } else {
         /*
          * limit storages to autochangers if slots is given
          */
         autochangers_only = (find_arg(ua, NT_("slots")) > 0);
         store = get_storage_resource(ua, false, autochangers_only);

         if (store) {
            if (find_arg(ua, NT_("slots")) > 0) {
               switch (ua->api) {
               case API_MODE_OFF:
                  status_slots(ua, store);
                  break;
               case API_MODE_ON:
                  status_content_api(ua, store);
                  break;
               case API_MODE_JSON:
                  status_content_json(ua, store);
                  break;
               }
            } else {
               storage_status(ua, store, NULL);
            }
         }

         return true;
      }
   }
   /* If no args, ask for status type */
   if (ua->argc == 1) {
      char prmt[MAX_NAME_LENGTH];

      start_prompt(ua, _("Status available for:\n"));
      add_prompt(ua, NT_("Director"));
      add_prompt(ua, NT_("Storage"));
      add_prompt(ua, NT_("Client"));
      add_prompt(ua, NT_("Scheduler"));
      add_prompt(ua, NT_("All"));
      Dmsg0(20, "do_prompt: select daemon\n");
      if ((item=do_prompt(ua, "",  _("Select daemon type for status"), prmt, sizeof(prmt))) < 0) {
         return true;
      }
      Dmsg1(20, "item=%d\n", item);
      switch (item) {
      case 0:                         /* Director */
         do_director_status(ua);
         break;
      case 1:
         store = select_storage_resource(ua);
         if (store) {
            storage_status(ua, store, NULL);
         }
         break;
      case 2:
         client = select_client_resource(ua);
         if (client) {
            client_status(ua, client, NULL);
         }
         break;
      case 3:
         do_scheduler_status(ua);
         break;
      case 4:
         do_all_status(ua);
         break;
      default:
         break;
      }
   }
   return true;
}

static void do_all_status(UaContext *ua)
{
   StoreResource *store, **unique_store;
   ClientResource *client, **unique_client;
   int i, j;
   bool found;
   int32_t previous_JobStatus = 0;

   do_director_status(ua);

   /* Count Storage items */
   LockRes();
   i = 0;
   foreach_res(store, R_STORAGE) {
      i++;
   }
   unique_store = (StoreResource **) malloc(i * sizeof(StoreResource));
   /* Find Unique Storage address/port */
   i = 0;
   foreach_res(store, R_STORAGE) {
      found = false;
      if (!ua->acl_access_ok(Storage_ACL, store->name())) {
         continue;
      }
      for (j = 0; j < i; j++) {
         if (bstrcmp(unique_store[j]->address, store->address) &&
             unique_store[j]->SDport == store->SDport) {
            found = true;
            break;
         }
      }
      if (!found) {
         unique_store[i++] = store;
         Dmsg2(40, "Stuffing: %s:%d\n", store->address, store->SDport);
      }
   }
   UnlockRes();

   previous_JobStatus = ua->jcr->JobStatus;

   /* Call each unique Storage daemon */
   for (j = 0; j < i; j++) {
      storage_status(ua, unique_store[j], NULL);
      ua->jcr->JobStatus = previous_JobStatus;
   }
   free(unique_store);

   /* Count Client items */
   LockRes();
   i = 0;
   foreach_res(client, R_CLIENT) {
      i++;
   }
   unique_client = (ClientResource **)malloc(i * sizeof(ClientResource));
   /* Find Unique Client address/port */
   i = 0;
   foreach_res(client, R_CLIENT) {
      found = false;
      if (!ua->acl_access_ok(Client_ACL, client->name())) {
         continue;
      }
      for (j = 0; j < i; j++) {
         if (bstrcmp(unique_client[j]->address, client->address) &&
             unique_client[j]->FDport == client->FDport) {
            found = true;
            break;
         }
      }
      if (!found) {
         unique_client[i++] = client;
         Dmsg2(40, "Stuffing: %s:%d\n", client->address, client->FDport);
      }
   }
   UnlockRes();

   previous_JobStatus = ua->jcr->JobStatus;

   /* Call each unique File daemon */
   for (j = 0; j < i; j++) {
      client_status(ua, unique_client[j], NULL);
      ua->jcr->JobStatus = previous_JobStatus;
   }
   free(unique_client);

}

void list_dir_status_header(UaContext *ua)
{
   int len, cnt;
   CatalogResource *catalog;
   char dt[MAX_TIME_LENGTH];
   char b1[35], b2[35], b3[35], b4[35], b5[35];
   PoolMem msg(PM_FNAME),
            dbdrivers(PM_FNAME);

   cnt = 0;
   foreach_res(catalog, R_CATALOG) {
      if (cnt) {
         dbdrivers.strcat(" ");
      }
      dbdrivers.strcat(catalog->db_driver);
      cnt++;
   }
   ua->send_msg(_("%s Version: %s (%s) %s %s %s\n"), my_name, VERSION, BDATE,
                HOST_OS, DISTNAME, DISTVER);
   bstrftime_nc(dt, sizeof(dt), daemon_start_time);
   ua->send_msg(_("Daemon started %s. Jobs: run=%d, running=%d mode=%d db=%s\n"),
                dt, num_jobs_run, job_count(), (int)DEVELOPER_MODE, dbdrivers.c_str() );
   ua->send_msg(_(" Heap: heap=%s smbytes=%s max_bytes=%s bufs=%s max_bufs=%s\n"),
                edit_uint64_with_commas((char *)sbrk(0)-(char *)start_heap, b1),
                edit_uint64_with_commas(sm_bytes, b2),
                edit_uint64_with_commas(sm_max_bytes, b3),
                edit_uint64_with_commas(sm_buffers, b4),
                edit_uint64_with_commas(sm_max_buffers, b5));

   if (me->secure_erase_cmdline) {
      ua->send_msg(_(" secure erase command='%s'\n"), me->secure_erase_cmdline);
   }

   len = list_dir_plugins(msg);
   if (len > 0) {
      ua->send_msg("%s\n", msg.c_str());
   }
}

static bool show_scheduled_preview(UaContext *ua, ScheduleResource *sched,
                                   PoolMem &overview, int *max_date_len,
                                   struct tm tm, time_t time_to_check)
{
   int date_len, hour, mday, wday, month, wom, woy, yday;
   bool is_last_week = false;                      /* Are we in the last week of a month? */
   char dt[MAX_TIME_LENGTH];
   time_t runtime;
   RunResource *run;
   PoolMem temp(PM_NAME);

   hour = tm.tm_hour;
   mday = tm.tm_mday - 1;
   wday = tm.tm_wday;
   month = tm.tm_mon;
   wom = mday / 7;
   woy = tm_woy(time_to_check);                    /* Get week of year */
   yday = tm.tm_yday;                              /* Get day of year */

   is_last_week = is_doy_in_last_week(tm.tm_year + 1900 , yday);

   for (run = sched->run; run; run = run->next) {
      bool run_now;
      int cnt = 0;

      run_now = bit_is_set(hour, run->hour) &&
                bit_is_set(mday, run->mday) &&
                bit_is_set(wday, run->wday) &&
                bit_is_set(month, run->month) &&
               (bit_is_set(wom, run->wom) ||
                (run->last_set && is_last_week)) &&
                bit_is_set(woy, run->woy);

      if (run_now) {
         /*
          * Find time (time_t) job is to be run
          */
         blocaltime(&time_to_check, &tm);        /* Reset tm structure */
         tm.tm_min = run->minute;                /* Set run minute */
         tm.tm_sec = 0;                          /* Zero secs */

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
                * When the datelen changes during the loop the locale generates a date string that
                * is variable. Only thing we can do about that is start from scratch again.
                * We invoke this by return false from this function.
                */
               *max_date_len = date_len;
               pm_strcpy(overview, "");
               return false;
            } else {
               /*
                * This is the first determined length we use this until we are proven wrong.
                */
               *max_date_len = date_len;
            }
         }

         Mmsg(temp, "%-*s  %-22.22s  ", *max_date_len, dt, sched->hdr.name);
         pm_strcat(overview, temp.c_str());

         if (run->level) {
            if (cnt++ > 0) {
               pm_strcat(overview, " ");
            }
            Mmsg(temp, "Level=%s", level_to_str(run->level));
            pm_strcat(overview, temp.c_str());
         }

         if (run->Priority) {
            if (cnt++ > 0) {
               pm_strcat(overview, " ");
            }
            Mmsg(temp, "Priority=%d", run->Priority);
            pm_strcat(overview, temp.c_str());
         }

         if (run->spool_data_set) {
            if (cnt++ > 0) {
               pm_strcat(overview, " ");
            }
            Mmsg(temp, "Spool Data=%d", run->spool_data);
            pm_strcat(overview, temp.c_str());
         }

         if (run->accurate_set) {
            if (cnt++ > 0) {
               pm_strcat(overview, " ");
            }
            Mmsg(temp, "Accurate=%d", run->accurate);
            pm_strcat(overview, temp.c_str());
         }

         if (run->pool) {
            if (cnt++ > 0) {
               pm_strcat(overview, " ");
            }
            Mmsg(temp, "Pool=%s", run->pool->name());
            pm_strcat(overview, temp.c_str());
         }

         if (run->storage) {
            if (cnt++ > 0) {
               pm_strcat(overview, " ");
            }
            Mmsg(temp, "Storage=%s", run->storage->name());
            pm_strcat(overview, temp.c_str());
         }

         if (run->msgs) {
            if (cnt++ > 0) {
               pm_strcat(overview, " ");
            }
            Mmsg(temp, "Messages=%s", run->msgs->name());
            pm_strcat(overview, temp.c_str());
         }

         pm_strcat(overview, "\n");
      }
   }

   /*
    * If we make it till here the length of the datefield is constant or didn't change.
    */
   return true;
}

/**
 * Check the number of clients in the DB against the configured number of subscriptions
 *
 * Return true if (number of clients < number of subscriptions), else
 * return false
 */
static bool do_subscription_status(UaContext *ua)
{
   int available;
   bool retval = false;

   /*
    * See if we need to check.
    */
   if (me->subscriptions == 0) {
      ua->send_msg(_("No subscriptions configured in director.\n"));
      retval = true;
      goto bail_out;
   }

   if (me->subscriptions_used <= 0) {
      ua->error_msg(_("No clients defined.\n"));
      goto bail_out;
   } else {
      available = me->subscriptions - me->subscriptions_used;
      if (available < 0) {
         ua->send_msg(_("Warning! No available subscriptions: %d (%d/%d) (used/total)\n"),
                      available, me->subscriptions_used, me->subscriptions);
      } else {
         ua->send_msg(_("Ok: available subscriptions: %d (%d/%d) (used/total)\n"),
                      available, me->subscriptions_used, me->subscriptions);
         retval = true;
      }
   }

bail_out:
   return retval;
}

static void do_scheduler_status(UaContext *ua)
{
   int i;
   int max_date_len = 0;
   int days = DEFAULT_STATUS_SCHED_DAYS;         /* Default days for preview */
   bool schedulegiven = false;
   time_t time_to_check, now, start, stop;
   char schedulename[MAX_NAME_LENGTH];
   const int seconds_per_day = 86400;            /* Number of seconds in one day */
   const int seconds_per_hour = 3600;            /* Number of seconds in one hour */
   struct tm tm;
   ClientResource *client = NULL;
   JobResource *job = NULL;
   ScheduleResource *sched;
   PoolMem overview(PM_MESSAGE);

   now = time(NULL);                             /* Initialize to now */
   time_to_check = now;

   i = find_arg_with_value(ua, NT_("days"));
   if (i >= 0) {
      days = atoi(ua->argv[i]);
      if (((days < -366) || (days > 366)) && !ua->api) {
         ua->send_msg(_("Ignoring invalid value for days. Allowed is -366 < days < 366.\n"));
         days = DEFAULT_STATUS_SCHED_DAYS;
      }
   }

   /*
    * Schedule given ?
    */
   i = find_arg_with_value(ua, NT_("schedule"));
   if (i >= 0) {
      bstrncpy(schedulename, ua->argv[i], sizeof(schedulename));
      schedulegiven = true;
   }

   /*
    * Client given ?
    */
   i = find_arg_with_value(ua, NT_("client"));
   if (i >= 0) {
      client = get_client_resource(ua);
   }

   /*
    * Jobname given ?
    */
   i = find_arg_with_value(ua, NT_("job"));
   if (i >= 0) {
      job = ua->GetJobResWithName(ua->argv[i]);

      /*
       * If a bogus jobname was given ask for it interactively.
       */
      if (!job) {
         job = select_job_resource(ua);
      }
   }

   ua->send_msg("Scheduler Jobs:\n\n");
   ua->send_msg("Schedule               Jobs Triggered\n");
   ua->send_msg("===========================================================\n");

   LockRes();
   foreach_res(sched, R_SCHEDULE) {
      int cnt = 0;

      if (!schedulegiven && !sched->enabled) {
         continue;
      }

      if (!ua->acl_access_ok(Schedule_ACL, sched->hdr.name)) {
         continue;
      }

      if (schedulegiven) {
         if (!bstrcmp(sched->hdr.name, schedulename)) {
           continue;
         }
      }

      if (job) {
         if (job->schedule && bstrcmp(sched->hdr.name, job->schedule->hdr.name)) {
            if (cnt++ == 0) {
               ua->send_msg("%s\n", sched->hdr.name);
            }
            ua->send_msg("                       %s\n", job->name());
         }
      } else {
         foreach_res(job, R_JOB) {
            if (!ua->acl_access_ok(Job_ACL, job->hdr.name)) {
               continue;
            }

            if (client && job->client != client) {
               continue;
            }

            if (job->schedule && bstrcmp(sched->hdr.name, job->schedule->hdr.name)) {
               if (cnt++ == 0) {
                  ua->send_msg("%s\n", sched->hdr.name);
               }
               if (job->enabled &&
                   (!job->client || job->client->enabled)) {
                  ua->send_msg("                       %s\n", job->name());
               } else {
                  ua->send_msg("                       %s (disabled)\n", job->name());
               }
            }
         }
      }

      if (cnt > 0) {
         ua->send_msg("\n");
      }
   }
   UnlockRes();

   /*
    * Build an overview.
    */
   if (days > 0) {                            /* future */
      start = now;
      stop = now + (days * seconds_per_day);
   } else {                                     /* past */
      start = now + (days * seconds_per_day);
      stop = now;
   }

start_again:
   time_to_check = start;
   while (time_to_check < stop) {
      blocaltime(&time_to_check, &tm);

      if (client || job) {
         /*
          * List specific schedule.
          */
         if (job) {
            if (job->schedule) {
               if (!show_scheduled_preview(ua, job->schedule, overview,
                                           &max_date_len, tm, time_to_check)) {
                  goto start_again;
               }
            }
         } else {
            LockRes();
            foreach_res(job, R_JOB) {
               if (!ua->acl_access_ok(Job_ACL, job->hdr.name)) {
                  continue;
               }

               if (job->schedule && job->client == client) {
                  if (!show_scheduled_preview(ua, job->schedule, overview,
                                              &max_date_len, tm, time_to_check)) {
                     job = NULL;
                     UnlockRes();
                     goto start_again;
                  }
               }
            }
            UnlockRes();
            job = NULL;
         }
      } else {
         /*
          * List all schedules.
          */
         LockRes();
         foreach_res(sched, R_SCHEDULE) {
            if (!schedulegiven && !sched->enabled) {
               continue;
            }

            if (!ua->acl_access_ok(Schedule_ACL, sched->hdr.name)) {
               continue;
            }

            if (schedulegiven) {
               if (!bstrcmp(sched->hdr.name, schedulename)) {
                  continue;
               }
            }

            if (!show_scheduled_preview(ua, sched, overview,
                                        &max_date_len, tm, time_to_check)) {
               UnlockRes();
               goto start_again;
            }
         }
         UnlockRes();
      }

      time_to_check += seconds_per_hour; /* next hour */
   }

   ua->send_msg("====\n\n");
   ua->send_msg("Scheduler Preview for %d days:\n\n", days);
   ua->send_msg("%-*s  %-22s  %s\n", max_date_len, _("Date"), _("Schedule"), _("Overrides"));
   ua->send_msg("==============================================================\n");
   ua->send_msg(overview.c_str());
   ua->send_msg("====\n");
}

static void do_director_status(UaContext *ua)
{
   list_dir_status_header(ua);

   /*
    * List scheduled Jobs
    */
   list_scheduled_jobs(ua);

   /*
    * List running jobs
    */
   list_running_jobs(ua);

   /*
    * List terminated jobs
    */
   list_terminated_jobs(ua);

   list_connected_clients(ua);

   ua->send_msg("====\n");
}

static void prt_runhdr(UaContext *ua)
{
   if (!ua->api) {
      ua->send_msg(_("\nScheduled Jobs:\n"));
      ua->send_msg(_("Level          Type     Pri  Scheduled          Name               Volume\n"));
      ua->send_msg(_("===================================================================================\n"));
   }
}

/* Scheduling packet */
struct sched_pkt {
   dlink link;                        /* keep this as first item!!! */
   JobResource *job;
   int level;
   int priority;
   utime_t runtime;
   PoolResource *pool;
   StoreResource *store;
};

static void prt_runtime(UaContext *ua, sched_pkt *sp)
{
   char dt[MAX_TIME_LENGTH];
   const char *level_ptr;
   bool ok = false;
   bool close_db = false;
   JobControlRecord *jcr = ua->jcr;
   MediaDbRecord mr;
   int orig_jobtype;

   memset(&mr, 0, sizeof(mr));

   orig_jobtype = jcr->getJobType();
   if (sp->job->JobType == JT_BACKUP) {
      jcr->db = NULL;
      ok = complete_jcr_for_job(jcr, sp->job, sp->pool);
      Dmsg1(250, "Using pool=%s\n", jcr->res.pool->name());
      if (jcr->db) {
         close_db = true;             /* new db opened, remember to close it */
      }
      if (ok) {
         mr.PoolId = jcr->jr.PoolId;
         jcr->res.wstore = sp->store;
         set_storageid_in_mr(jcr->res.wstore, &mr);
         Dmsg0(250, "call find_next_volume_for_append\n");
         /* no need to set ScratchPoolId, since we use fnv_no_create_vol */
         ok = find_next_volume_for_append(jcr, &mr, 1, NULL, fnv_no_create_vol, fnv_no_prune);
      }
      if (!ok) {
         bstrncpy(mr.VolumeName, "*unknown*", sizeof(mr.VolumeName));
      }
   }
   bstrftime_nc(dt, sizeof(dt), sp->runtime);
   switch (sp->job->JobType) {
   case JT_ADMIN:
   case JT_ARCHIVE:
   case JT_RESTORE:
      level_ptr = " ";
      break;
   default:
      level_ptr = level_to_str(sp->level);
      break;
   }
   if (ua->api) {
      ua->send_msg(_("%-14s\t%-8s\t%3d\t%-18s\t%-18s\t%s\n"),
         level_ptr, job_type_to_str(sp->job->JobType), sp->priority, dt,
         sp->job->name(), mr.VolumeName);
   } else {
      ua->send_msg(_("%-14s %-8s %3d  %-18s %-18s %s\n"),
         level_ptr, job_type_to_str(sp->job->JobType), sp->priority, dt,
         sp->job->name(), mr.VolumeName);
   }
   if (close_db) {
      db_sql_close_pooled_connection(jcr, jcr->db);
   }
   jcr->db = ua->db;                  /* restore ua db to jcr */
   jcr->setJobType(orig_jobtype);
}

/**
 * Sort items by runtime, priority
 */
static int compare_by_runtime_priority(void *item1, void *item2)
{
   sched_pkt *p1 = (sched_pkt *)item1;
   sched_pkt *p2 = (sched_pkt *)item2;

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

/**
 * Find all jobs to be run in roughly the next 24 hours.
 */
static void list_scheduled_jobs(UaContext *ua)
{
   utime_t runtime;
   RunResource *run;
   JobResource *job;
   int level, num_jobs = 0;
   int priority;
   bool hdr_printed = false;
   dlist sched;
   sched_pkt *sp;
   int days, i;

   Dmsg0(200, "enter list_sched_jobs()\n");

   days = 1;
   i = find_arg_with_value(ua, NT_("days"));
   if (i >= 0) {
     days = atoi(ua->argv[i]);
     if (((days < 0) || (days > 500)) && !ua->api) {
       ua->send_msg(_("Ignoring invalid value for days. Max is 500.\n"));
       days = 1;
     }
   }

   /*
    * Loop through all jobs
    */
   LockRes();
   foreach_res(job, R_JOB) {
      if (!ua->acl_access_ok(Job_ACL, job->name()) ||
          !job->enabled ||
          (job->client && !job->client->enabled)) {
         continue;
      }
      for (run = NULL; (run = find_next_run(run, job, runtime, days)); ) {
         UnifiedStoreResource store;
         level = job->JobLevel;
         if (run->level) {
            level = run->level;
         }
         priority = job->Priority;
         if (run->Priority) {
            priority = run->Priority;
         }
         if (!hdr_printed) {
            prt_runhdr(ua);
            hdr_printed = true;
         }
         sp = (sched_pkt *)malloc(sizeof(sched_pkt));
         sp->job = job;
         sp->level = level;
         sp->priority = priority;
         sp->runtime = runtime;
         sp->pool = run->pool;
         get_job_storage(&store, job, run);
         sp->store = store.store;
         Dmsg3(250, "job=%s store=%s MediaType=%s\n", job->name(), sp->store->name(), sp->store->media_type);
         sched.binary_insert_multiple(sp, compare_by_runtime_priority);
         num_jobs++;
      }
   } /* end for loop over resources */
   UnlockRes();
   foreach_dlist(sp, &sched) {
      prt_runtime(ua, sp);
   }
   if (num_jobs == 0 && !ua->api) {
      ua->send_msg(_("No Scheduled Jobs.\n"));
   }
   if (!ua->api) ua->send_msg("====\n");
   Dmsg0(200, "Leave list_sched_jobs_runs()\n");
}

static void list_running_jobs(UaContext *ua)
{
   JobControlRecord *jcr;
   int njobs = 0;
   const char *msg;
   char *emsg;                        /* edited message */
   char dt[MAX_TIME_LENGTH];
   char level[10];
   bool pool_mem = false;

   Dmsg0(200, "enter list_run_jobs()\n");
   if (!ua->api) ua->send_msg(_("\nRunning Jobs:\n"));
   foreach_jcr(jcr) {
      if (jcr->JobId == 0) {      /* this is us */
         /* this is a console or other control job. We only show console
          * jobs in the status output.
          */
         if (jcr->is_JobType(JT_CONSOLE) && !ua->api) {
            bstrftime_nc(dt, sizeof(dt), jcr->start_time);
            ua->send_msg(_("Console connected at %s\n"), dt);
         }
         continue;
      }
      njobs++;
   }
   endeach_jcr(jcr);

   if (njobs == 0) {
      /* Note the following message is used in regress -- don't change */
      if (!ua->api)  ua->send_msg(_("No Jobs running.\n====\n"));
      Dmsg0(200, "leave list_run_jobs()\n");
      return;
   }
   njobs = 0;
   if (!ua->api) {
      ua->send_msg(_(" JobId Level   Name                       Status\n"));
      ua->send_msg(_("======================================================================\n"));
   }
   foreach_jcr(jcr) {
      if (jcr->JobId == 0 || !ua->acl_access_ok(Job_ACL, jcr->res.job->name())) {
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
         emsg = (char *) get_pool_memory(PM_FNAME);
         if (!jcr->res.client) {
            Mmsg(emsg, _("is waiting on Client"));
         } else {
            Mmsg(emsg, _("is waiting on Client %s"), jcr->res.client->name());
         }
         pool_mem = true;
         msg = emsg;
         break;
      case JS_WaitSD:
         emsg = (char *) get_pool_memory(PM_FNAME);
         if (jcr->res.wstore) {
            Mmsg(emsg, _("is waiting on Storage \"%s\""), jcr->res.wstore->name());
         } else if (jcr->res.rstore) {
            Mmsg(emsg, _("is waiting on Storage \"%s\""), jcr->res.rstore->name());
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
         emsg = (char *) get_pool_memory(PM_FNAME);
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
         emsg = (char *)get_pool_memory(PM_FNAME);
         Mmsg(emsg, _("is in unknown state %c"), jcr->JobStatus);
         pool_mem = true;
         msg = emsg;
         break;
      }
      /*
       * Now report Storage daemon status code
       */
      switch (jcr->SDJobStatus) {
      case JS_WaitMount:
         if (pool_mem) {
            free_pool_memory(emsg);
            pool_mem = false;
         }
         msg = _("is waiting for a mount request");
         break;
      case JS_WaitMedia:
         if (pool_mem) {
            free_pool_memory(emsg);
            pool_mem = false;
         }
         msg = _("is waiting for an appendable Volume");
         break;
      case JS_WaitFD:
         if (!pool_mem) {
            emsg = (char *)get_pool_memory(PM_FNAME);
            pool_mem = true;
         }
         if (!jcr->file_bsock) {
            /*
             * client initiated connection
             */
            Mmsg(emsg, _("is waiting for Client to connect (Client Initiated Connection)"));
         } else if (!jcr->res.client || !jcr->res.wstore) {
            Mmsg(emsg, _("is waiting for Client to connect to Storage daemon"));
         } else {
            Mmsg(emsg, _("is waiting for Client %s to connect to Storage %s"),
                 jcr->res.client->name(), jcr->res.wstore->name());
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
         bstrncpy(level, level_to_str(jcr->getJobLevel()), sizeof(level));
         level[7] = 0;
         break;
      }

      if (ua->api) {
         bash_spaces(jcr->comment);
         ua->send_msg(_("%6d\t%-6s\t%-20s\t%s\t%s\n"),
                      jcr->JobId, level, jcr->Job, msg, jcr->comment);
         unbash_spaces(jcr->comment);
      } else {
         ua->send_msg(_("%6d %-6s  %-20s %s\n"),
            jcr->JobId, level, jcr->Job, msg);
         /* Display comments if any */
         if (*jcr->comment) {
            ua->send_msg(_("               %-30s\n"), jcr->comment);
         }
      }

      if (pool_mem) {
         free_pool_memory(emsg);
         pool_mem = false;
      }
   }
   endeach_jcr(jcr);
   if (!ua->api) ua->send_msg("====\n");
   Dmsg0(200, "leave list_run_jobs()\n");
}

static void list_terminated_jobs(UaContext *ua)
{
   char dt[MAX_TIME_LENGTH], b1[30], b2[30];
   char level[10];

   if (last_jobs->empty()) {
      if (!ua->api) ua->send_msg(_("No Terminated Jobs.\n"));
      return;
   }
   lock_last_jobs_list();
   struct s_last_job *je;
   if (!ua->api) {
      ua->send_msg(_("\nTerminated Jobs:\n"));
      ua->send_msg(_(" JobId  Level    Files      Bytes   Status   Finished        Name \n"));
      ua->send_msg(_("====================================================================\n"));
   }
   foreach_dlist(je, last_jobs) {
      char JobName[MAX_NAME_LENGTH];
      const char *termstat;

      bstrncpy(JobName, je->Job, sizeof(JobName));
      /* There are three periods after the Job name */
      char *p;
      for (int i = 0; i < 3; i++) {
         if ((p=strrchr(JobName, '.')) != NULL) {
            *p = 0;
         }
      }

      if (!ua->acl_access_ok(Job_ACL, JobName)) {
         continue;
      }

      bstrftime_nc(dt, sizeof(dt), je->end_time);
      switch (je->JobType) {
      case JT_ADMIN:
      case JT_ARCHIVE:
      case JT_RESTORE:
         bstrncpy(level, "    ", sizeof(level));
         break;
      default:
         bstrncpy(level, level_to_str(je->JobLevel), sizeof(level));
         level[4] = 0;
         break;
      }
      switch (je->JobStatus) {
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
         ua->send_msg(_("%6d\t%-6s\t%8s\t%10s\t%-7s\t%-8s\t%s\n"),
            je->JobId,
            level,
            edit_uint64_with_commas(je->JobFiles, b1),
            edit_uint64_with_suffix(je->JobBytes, b2),
            termstat,
            dt, JobName);
      } else {
         ua->send_msg(_("%6d  %-6s %8s %10s  %-7s  %-8s %s\n"),
            je->JobId,
            level,
            edit_uint64_with_commas(je->JobFiles, b1),
            edit_uint64_with_suffix(je->JobBytes, b2),
            termstat,
            dt, JobName);
      }
   }
   if (!ua->api) ua->send_msg(_("\n"));
   unlock_last_jobs_list();
}


static void list_connected_clients(UaContext *ua)
{
   CONNECTION *connection = NULL;
   alist *connections = NULL;
   const char *separator = "====================";
   char dt[MAX_TIME_LENGTH];

   ua->send->decoration("\n");
   ua->send->decoration("Client Initiated Connections (waiting for jobs):\n");
   connections = get_client_connections()->get_as_alist();
   ua->send->decoration("%-20s%-20s%-20s%-40s\n", "Connect time", "Protocol", "Authenticated", "Name");
   ua->send->decoration("%-20s%-20s%-20s%-20s%-20s\n", separator, separator, separator, separator, separator);
   ua->send->array_start("client-connection");
   foreach_alist(connection, connections) {
      ua->send->object_start();
      bstrftime_nc(dt, sizeof(dt), connection->connect_time());
      ua->send->object_key_value("connect_time", dt, "%-20s");
      ua->send->object_key_value("protocol_version", connection->protocol_version(), "%-20d");
      ua->send->object_key_value("authenticated", connection->authenticated(), "%-20d");
      ua->send->object_key_value("name", connection->name(), "%-40s");
      ua->send->object_end();
      ua->send->decoration("\n");
   }
   ua->send->array_end("client-connection");
}

static void content_send_info_api(UaContext *ua, char type, int Slot, char *vol_name)
{
   char ed1[50], ed2[50], ed3[50];
   PoolDbRecord pr;
   MediaDbRecord mr;
   /* Type|Slot|RealSlot|Volume|Bytes|Status|MediaType|Pool|LastW|Expire */
   const char *slot_api_full_format="%c|%hd|%hd|%s|%s|%s|%s|%s|%s|%s\n";

   memset(&pr, 0, sizeof(pr));
   memset(&mr, 0, sizeof(mr));

   bstrncpy(mr.VolumeName, vol_name, sizeof(mr.VolumeName));
   if (ua->db->get_media_record(ua->jcr, &mr)) {
      pr.PoolId = mr.PoolId;
      if (!ua->db->get_pool_record(ua->jcr, &pr)) {
         strcpy(pr.Name, "?");
      }
      ua->send_msg(slot_api_full_format, type,
                   Slot, mr.Slot, mr.VolumeName,
                   edit_uint64(mr.VolBytes, ed1),
                   mr.VolStatus, mr.MediaType, pr.Name,
                   edit_uint64(mr.LastWritten, ed2),
                   edit_uint64(mr.LastWritten + mr.VolRetention, ed3));
   } else {                  /* Media unknown */
      ua->send_msg(slot_api_full_format,
                   type, Slot, 0, mr.VolumeName, "?", "?", "?", "?",
                   "0", "0");
   }
}

static void content_send_info_json(UaContext *ua, const char *type, int Slot, char *vol_name)
{
   PoolDbRecord pr;
   MediaDbRecord mr;

   memset(&pr, 0, sizeof(pr));
   memset(&mr, 0, sizeof(mr));

   bstrncpy(mr.VolumeName, vol_name, sizeof(mr.VolumeName));
   if (ua->db->get_media_record(ua->jcr, &mr)) {
      pr.PoolId = mr.PoolId;
      if (!ua->db->get_pool_record(ua->jcr, &pr)) {
         strcpy(pr.Name, "?");
      }

      ua->send->object_start();
      ua->send->object_key_value("type", type, "%s\n");
      ua->send->object_key_value("slotnr", Slot, "%hd\n");
      ua->send->object_key_value("content", "full", "%s\n");
      ua->send->object_key_value("mr_slotnr", mr.Slot, "%lld\n");
      ua->send->object_key_value("mr_volname", mr.VolumeName, "%s\n");
      ua->send->object_key_value("mr_volbytes", mr.VolBytes, "%lld\n");
      ua->send->object_key_value("mr_volstatus", mr.VolStatus, "%s\n");
      ua->send->object_key_value("mr_mediatype", mr.MediaType, "%s\n");
      ua->send->object_key_value("pr_name", pr.Name, "%s\n");
      ua->send->object_key_value("mr_lastwritten", mr.LastWritten, "%lld\n");
      ua->send->object_key_value("mr_expire", mr.LastWritten + mr.VolRetention, "%lld\n");
      ua->send->object_end();
   } else {                  /* Media unknown */
      ua->send->object_start();
      ua->send->object_key_value("type", type, "%s\n");
      ua->send->object_key_value("slotnr", Slot, "%hd\n");
      ua->send->object_key_value("content", "full", "%s\n");
      ua->send->object_key_value("mr_slotnr", (uint64_t)0, "%lld\n");
      ua->send->object_key_value("mr_volname", mr.VolumeName, "%s\n");
      ua->send->object_key_value("mr_volbytes", "?", "%s\n");
      ua->send->object_key_value("mr_volstatus", "?", "%s\n");
      ua->send->object_key_value("mr_mediatype", "?", "%s\n");
      ua->send->object_key_value("pr_name", "?", "%s\n");
      ua->send->object_key_value("mr_lastwritten", (uint64_t)0, "%lld\n");
      ua->send->object_key_value("mr_expire", (uint64_t)0, "%lld\n");
      ua->send->object_end();
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
static void status_content_api(UaContext *ua, StoreResource *store)
{
   vol_list_t *vl1, *vl2;
   changer_vol_list_t *vol_list = NULL;
   const char *slot_api_drive_full_format="%c|%hd|%hd|%s\n";
   const char *slot_api_drive_empty_format="%c|%hd||\n";
   const char *slot_api_slot_empty_format="%c|%hd||||||||\n";

   if (!open_client_db(ua)) {
      return;
   }

   vol_list = get_vol_list_from_storage(ua, store, true /* listall */ , true /* want to see all slots */);
   if (!vol_list) {
      ua->warning_msg(_("No Volumes found, or no barcodes.\n"));
      goto bail_out;
   }

   foreach_dlist(vl1, vol_list->contents) {
      switch (vl1->Type) {
      case slot_type_drive:
         switch (vl1->Content) {
         case slot_content_full:
            ua->send_msg(slot_api_drive_full_format, 'D', vl1->Slot, vl1->Loaded, vl1->VolName);
            break;
         case slot_content_empty:
            ua->send_msg(slot_api_drive_empty_format, 'D', vl1->Slot);
            break;
         default:
            break;
         }
         break;
      case slot_type_normal:
      case slot_type_import:
         switch (vl1->Content) {
         case slot_content_full:
            switch (vl1->Type) {
            case slot_type_normal:
               content_send_info_api(ua, 'S', vl1->Slot, vl1->VolName);
               break;
            case slot_type_import:
               content_send_info_api(ua, 'I', vl1->Slot, vl1->VolName);
               break;
            default:
               break;
            }
            break;
         case slot_content_empty:
            /*
             * See if this empty slot is empty because the volume is loaded
             * in one of the drives.
             */
            vl2 = vol_is_loaded_in_drive(store, vol_list, vl1->Slot);
            if (vl2) {
               switch (vl1->Type) {
               case slot_type_normal:
                  content_send_info_api(ua, 'S', vl1->Slot, vl2->VolName);
                  break;
               case slot_type_import:
                  content_send_info_api(ua, 'I', vl1->Slot, vl2->VolName);
                  break;
               default:
                  break;
               }
               continue;
            }

            switch (vl1->Type) {
            case slot_type_normal:
               ua->send_msg(slot_api_slot_empty_format, 'S', vl1->Slot);
               break;
            case slot_type_import:
               ua->send_msg(slot_api_slot_empty_format, 'I', vl1->Slot);
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
   if (vol_list) {
      storage_release_vol_list(store, vol_list);
   }
   close_sd_bsock(ua);

   return;
}

static void status_content_json(UaContext *ua, StoreResource *store)
{
   vol_list_t *vl1, *vl2;
   changer_vol_list_t *vol_list = NULL;

   if (!open_client_db(ua)) {
      return;
   }

   vol_list = get_vol_list_from_storage(ua, store, true /* listall */ , true /* want to see all slots */);
   if (!vol_list) {
      ua->warning_msg(_("No Volumes found, or no barcodes.\n"));
      goto bail_out;
   }

   ua->send->array_start("contents");
   foreach_dlist(vl1, vol_list->contents) {
      switch (vl1->Type) {
      case slot_type_drive:
         ua->send->object_start();
         ua->send->object_key_value("type", "drive", "%s\n");
         ua->send->object_key_value("slotnr", vl1->Slot, "%hd\n");
         switch (vl1->Content) {
         case slot_content_full:
            ua->send->object_key_value("content", "full", "%s\n");
            ua->send->object_key_value("loaded", vl1->Loaded, "%hd\n");
            ua->send->object_key_value("volname", vl1->VolName, "%s\n");
            break;
         case slot_content_empty:
            ua->send->object_key_value("content", "empty", "%s\n");
            break;
         default:
            break;
         }
         ua->send->object_end();
         break;
      case slot_type_normal:
      case slot_type_import:
         switch (vl1->Content) {
         case slot_content_full:
            switch (vl1->Type) {
            case slot_type_normal:
               content_send_info_json(ua, "slot", vl1->Slot, vl1->VolName);
               break;
            case slot_type_import:
               content_send_info_json(ua, "import_slot", vl1->Slot, vl1->VolName);
               break;
            default:
               break;
            }
            break;
         case slot_content_empty:
            /*
             * See if this empty slot is empty because the volume is loaded
             * in one of the drives.
             */
            vl2 = vol_is_loaded_in_drive(store, vol_list, vl1->Slot);
            if (vl2) {
               switch (vl1->Type) {
               case slot_type_normal:
                  content_send_info_json(ua, "slot", vl1->Slot, vl2->VolName);
                  break;
               case slot_type_import:
                  content_send_info_json(ua, "import_slot", vl1->Slot, vl2->VolName);
                  break;
               default:
                  break;
               }
               continue;
            }

            switch (vl1->Type) {
            case slot_type_normal:
               ua->send->object_start();
               ua->send->object_key_value("type", "slot", "%s\n");
               ua->send->object_key_value("slotnr", vl1->Slot, "%hd\n");
               ua->send->object_key_value("content", "empty", "%s\n");
               ua->send->object_end();
               break;
            case slot_type_import:
               ua->send->object_start();
               ua->send->object_key_value("type", "import_slot", "%s\n");
               ua->send->object_key_value("slotnr", vl1->Slot, "%hd\n");
               ua->send->object_key_value("content", "empty", "%s\n");
               ua->send->object_end();
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
   ua->send->array_end("contents");

bail_out:
   if (vol_list) {
      storage_release_vol_list(store, vol_list);
   }
   close_sd_bsock(ua);

   return;
}

/**
 * Print slots from AutoChanger
 */
static void status_slots(UaContext *ua, StoreResource *store)
{
   PoolDbRecord pr;
   MediaDbRecord mr;
   char *slot_list;
   vol_list_t *vl1, *vl2;
   slot_number_t max_slots;
   changer_vol_list_t *vol_list = NULL;

   ua->jcr->res.wstore = store;

   /*
    * Slot | Volume | Status | MediaType | Pool
    */
   const char *slot_hformat=" %4i%c| %16s | %9s | %14s | %24s |\n";

   if (!open_client_db(ua)) {
      return;
   }

   memset(&mr, 0, sizeof(mr));

   max_slots = get_num_slots(ua, store);
   if (max_slots <= 0) {
      ua->warning_msg(_("No slots in changer to scan.\n"));
      return;
   }

   slot_list = (char *)malloc(nbytes_for_bits(max_slots));
   clear_all_bits(max_slots, slot_list);
   if (!get_user_slot_list(ua, slot_list, "slots", max_slots)) {
      free(slot_list);
      return;
   }

   vol_list = get_vol_list_from_storage(ua, store, true /* listall */ , true /* want to see all slots */);
   if (!vol_list) {
      ua->warning_msg(_("No Volumes found, or no barcodes.\n"));
      goto bail_out;
   }
   ua->send_msg(_(" Slot |   Volume Name    |   Status  |  Media Type    |         Pool             |\n"));
   ua->send_msg(_("------+------------------+-----------+----------------+--------------------------|\n"));

   /*
    * Walk through the list getting the media records
    * Slots start numbering at 1.
    */
   foreach_dlist(vl1, vol_list->contents) {
      vl2 = NULL;
      switch (vl1->Type) {
      case slot_type_drive:
         /*
          * We are not interested in drive slots.
          */
         continue;
      case slot_type_normal:
      case slot_type_import:
         if (vl1->Slot > max_slots) {
            ua->warning_msg(_("Slot %hd greater than max %hd ignored.\n"),
                            vl1->Slot, max_slots);
            continue;
         }
         /*
          * Check if user wants us to look at this slot
          */
         if (!bit_is_set(vl1->Slot - 1, slot_list)) {
            Dmsg1(100, "Skipping slot=%hd\n", vl1->Slot);
            continue;
         }

         switch (vl1->Content) {
         case slot_content_empty:
            if (vl1->Type == slot_type_normal) {
               /*
                * See if this empty slot is empty because the volume is loaded
                * in one of the drives.
                */
               vl2 = vol_is_loaded_in_drive(store, vol_list, vl1->Slot);
               if (!vl2) {
                  ua->send_msg(slot_hformat,
                               vl1->Slot, '*',
                               "?", "?", "?", "?");
                  continue;
               }
            } else {
               ua->send_msg(slot_hformat,
                            vl1->Slot, '@',
                            "?", "?", "?", "?");
               continue;
            }
            /*
             * Note, fall through wanted
             */
         case slot_content_full:
            /*
             * We get here for all slots with content and for empty
             * slots with their volume loaded in a drive.
             */
            if (vl1->Content == slot_content_full) {
               if (!vl1->VolName) {
                  Dmsg1(100, "No VolName for Slot=%hd.\n", vl1->Slot);
                  ua->send_msg(slot_hformat,
                               vl1->Slot,
                              (vl1->Type == slot_type_import) ? '@' : '*',
                               "?", "?", "?", "?");
                  continue;
               }

               memset(&mr, 0, sizeof(mr));
               bstrncpy(mr.VolumeName, vl1->VolName, sizeof(mr.VolumeName));
            } else {
               if (!vl2 || !vl2->VolName) {
                  Dmsg1(100, "No VolName for Slot=%hd.\n", vl1->Slot);
                  ua->send_msg(slot_hformat,
                               vl1->Slot,
                              (vl1->Type == slot_type_import) ? '@' : '*',
                               "?", "?", "?", "?");
                  continue;
               }

               memset(&mr, 0, sizeof(mr));
               bstrncpy(mr.VolumeName, vl2->VolName, sizeof(mr.VolumeName));
            }

            if (mr.VolumeName[0] && ua->db->get_media_record(ua->jcr, &mr)) {
               memset(&pr, 0, sizeof(pr));
               pr.PoolId = mr.PoolId;
               if (!ua->db->get_pool_record(ua->jcr, &pr)) {
                  strcpy(pr.Name, "?");
               }

               /*
                * Print information
                */
               if (vl1->Type == slot_type_import) {
                  ua->send_msg(slot_hformat,
                               vl1->Slot, '@',
                               mr.VolumeName, mr.VolStatus, mr.MediaType, pr.Name);
               } else {
                  ua->send_msg(slot_hformat,
                               vl1->Slot,
                               ((vl1->Slot == mr.Slot) ? (vl2 ? '%' : ' ') : '*'),
                               mr.VolumeName, mr.VolStatus, mr.MediaType, pr.Name);
               }
            } else {
               ua->send_msg(slot_hformat,
                            vl1->Slot,
                           (vl1->Type == slot_type_import) ? '@' : '*',
                            mr.VolumeName, "?", "?", "?");
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
   if (vol_list) {
      storage_release_vol_list(store, vol_list);
   }
   free(slot_list);
   close_sd_bsock(ua);

   return;
}
