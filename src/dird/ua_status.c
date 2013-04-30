/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2001-2012 Free Software Foundation Europe e.V.

   The main author of Bacula is Kern Sibbald, with contributions from
   many others, a complete list can be found in the file AUTHORS.
   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation and included
   in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   General Public License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.

   Bacula® is a registered trademark of Kern Sibbald.
   The licensor of Bacula is the Free Software Foundation Europe
   (FSFE), Fiduciary Program, Sumatrastrasse 25, 8006 Zürich,
   Switzerland, email:ftf@fsfeurope.org.
*/
/*
 *
 *   Bacula Director -- User Agent Status Command
 *
 *     Kern Sibbald, August MMI
 *
 */


#include "bacula.h"
#include "dird.h"

extern void *start_heap;

static void list_scheduled_jobs(UAContext *ua);
static void list_running_jobs(UAContext *ua);
static void list_terminated_jobs(UAContext *ua);
static void do_storage_status(UAContext *ua, STORE *store, char *cmd);
static void do_client_status(UAContext *ua, CLIENT *client, char *cmd);
static void do_director_status(UAContext *ua);
static void do_all_status(UAContext *ua);
void status_slots(UAContext *ua, STORE *store);
void status_content(UAContext *ua, STORE *store);

static char OKqstatus[]   = "1000 OK .status\n";
static char DotStatusJob[] = "JobId=%s JobStatus=%c JobErrors=%d\n";

/*
 * .status command
 */

bool dot_status_cmd(UAContext *ua, const char *cmd)
{
   STORE *store;
   CLIENT *client;
   JCR* njcr = NULL;
   s_last_job* job;
   char ed1[50];

   Dmsg2(20, "status=\"%s\" argc=%d\n", cmd, ua->argc);

   if (ua->argc < 3) {
      ua->send_msg("1900 Bad .status command, missing arguments.\n");
      return false;
   }

   if (strcasecmp(ua->argk[1], "dir") == 0) {
      if (strcasecmp(ua->argk[2], "current") == 0) {
         ua->send_msg(OKqstatus, ua->argk[2]);
         foreach_jcr(njcr) {
            if (njcr->JobId != 0 && acl_access_ok(ua, Job_ACL, njcr->job->name())) {
               ua->send_msg(DotStatusJob, edit_int64(njcr->JobId, ed1), 
                        njcr->JobStatus, njcr->JobErrors);
            }
         }
         endeach_jcr(njcr);
      } else if (strcasecmp(ua->argk[2], "last") == 0) {
         ua->send_msg(OKqstatus, ua->argk[2]);
         if ((last_jobs) && (last_jobs->size() > 0)) {
            job = (s_last_job*)last_jobs->last();
            if (acl_access_ok(ua, Job_ACL, job->Job)) {
               ua->send_msg(DotStatusJob, edit_int64(job->JobId, ed1), 
                     job->JobStatus, job->Errors);
            }
         }
      } else if (strcasecmp(ua->argk[2], "header") == 0) {
          list_dir_status_header(ua);
      } else if (strcasecmp(ua->argk[2], "scheduled") == 0) {
          list_scheduled_jobs(ua);
      } else if (strcasecmp(ua->argk[2], "running") == 0) {
          list_running_jobs(ua);
      } else if (strcasecmp(ua->argk[2], "terminated") == 0) {
          list_terminated_jobs(ua);
      } else {
         ua->send_msg("1900 Bad .status command, wrong argument.\n");
         return false;
      }
   } else if (strcasecmp(ua->argk[1], "client") == 0) {
      client = get_client_resource(ua);
      if (client) {
         Dmsg2(200, "Client=%s arg=%s\n", client->name(), NPRT(ua->argk[2]));
         do_client_status(ua, client, ua->argk[2]);
      }
   } else if (strcasecmp(ua->argk[1], "storage") == 0) {
      store = get_storage_resource(ua, false /*no default*/);
      if (store) {
         do_storage_status(ua, store, ua->argk[2]);
      }
   } else {
      ua->send_msg("1900 Bad .status command, wrong argument.\n");
      return false;
   }

   return true;
}

/* This is the *old* command handler, so we must return
 *  1 or it closes the connection
 */
int qstatus_cmd(UAContext *ua, const char *cmd)
{
   dot_status_cmd(ua, cmd);
   return 1;
}

/*
 * status command
 */
int status_cmd(UAContext *ua, const char *cmd)
{
   STORE *store;
   CLIENT *client;
   int item, i;

   Dmsg1(20, "status:%s:\n", cmd);

   for (i=1; i<ua->argc; i++) {
      if (strcasecmp(ua->argk[i], NT_("all")) == 0) {
         do_all_status(ua);
         return 1;
      } else if (strcasecmp(ua->argk[i], NT_("dir")) == 0 ||
                 strcasecmp(ua->argk[i], NT_("director")) == 0) {
         do_director_status(ua);
         return 1;
      } else if (strcasecmp(ua->argk[i], NT_("client")) == 0) {
         client = get_client_resource(ua);
         if (client) {
            do_client_status(ua, client, NULL);
         }
         return 1;
      } else {
         store = get_storage_resource(ua, false/*no default*/);
         if (store) {
            if (find_arg(ua, NT_("slots")) > 0) {
               status_slots(ua, store);
            } else {
               do_storage_status(ua, store, NULL);
            }
         }
         return 1;
      }
   }
   /* If no args, ask for status type */
   if (ua->argc == 1) {
       char prmt[MAX_NAME_LENGTH];

      start_prompt(ua, _("Status available for:\n"));
      add_prompt(ua, NT_("Director"));
      add_prompt(ua, NT_("Storage"));
      add_prompt(ua, NT_("Client"));
      add_prompt(ua, NT_("All"));
      Dmsg0(20, "do_prompt: select daemon\n");
      if ((item=do_prompt(ua, "",  _("Select daemon type for status"), prmt, sizeof(prmt))) < 0) {
         return 1;
      }
      Dmsg1(20, "item=%d\n", item);
      switch (item) {
      case 0:                         /* Director */
         do_director_status(ua);
         break;
      case 1:
         store = select_storage_resource(ua);
         if (store) {
            do_storage_status(ua, store, NULL);
         }
         break;
      case 2:
         client = select_client_resource(ua);
         if (client) {
            do_client_status(ua, client, NULL);
         }
         break;
      case 3:
         do_all_status(ua);
         break;
      default:
         break;
      }
   }
   return 1;
}

static void do_all_status(UAContext *ua)
{
   STORE *store, **unique_store;
   CLIENT *client, **unique_client;
   int i, j;
   bool found;

   do_director_status(ua);

   /* Count Storage items */
   LockRes();
   i = 0;
   foreach_res(store, R_STORAGE) {
      i++;
   }
   unique_store = (STORE **) malloc(i * sizeof(STORE));
   /* Find Unique Storage address/port */
   i = 0;
   foreach_res(store, R_STORAGE) {
      found = false;
      if (!acl_access_ok(ua, Storage_ACL, store->name())) {
         continue;
      }
      for (j=0; j<i; j++) {
         if (strcmp(unique_store[j]->address, store->address) == 0 &&
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

   /* Call each unique Storage daemon */
   for (j=0; j<i; j++) {
      do_storage_status(ua, unique_store[j], NULL);
   }
   free(unique_store);

   /* Count Client items */
   LockRes();
   i = 0;
   foreach_res(client, R_CLIENT) {
      i++;
   }
   unique_client = (CLIENT **)malloc(i * sizeof(CLIENT));
   /* Find Unique Client address/port */
   i = 0;
   foreach_res(client, R_CLIENT) {
      found = false;
      if (!acl_access_ok(ua, Client_ACL, client->name())) {
         continue;
      }
      for (j=0; j<i; j++) {
         if (strcmp(unique_client[j]->address, client->address) == 0 &&
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

   /* Call each unique File daemon */
   for (j=0; j<i; j++) {
      do_client_status(ua, unique_client[j], NULL);
   }
   free(unique_client);

}

void list_dir_status_header(UAContext *ua)
{
   char dt[MAX_TIME_LENGTH];
   char b1[35], b2[35], b3[35], b4[35], b5[35];

   ua->send_msg(_("%s Version: %s (%s) %s %s %s\n"), my_name, VERSION, BDATE,
            HOST_OS, DISTNAME, DISTVER);
   bstrftime_nc(dt, sizeof(dt), daemon_start_time);
   ua->send_msg(_("Daemon started %s. Jobs: run=%d, running=%d "
                  "mode=%d,%d\n"), dt,
                num_jobs_run, job_count(), (int)DEVELOPER_MODE, (int)BEEF);
   ua->send_msg(_(" Heap: heap=%s smbytes=%s max_bytes=%s bufs=%s max_bufs=%s\n"),
            edit_uint64_with_commas((char *)sbrk(0)-(char *)start_heap, b1),
            edit_uint64_with_commas(sm_bytes, b2),
            edit_uint64_with_commas(sm_max_bytes, b3),
            edit_uint64_with_commas(sm_buffers, b4),
            edit_uint64_with_commas(sm_max_buffers, b5));

   /* TODO: use this function once for all daemons */
   if (debug_level > 0 && bplugin_list->size() > 0) {
      int len;
      Plugin *plugin;
      POOL_MEM msg(PM_FNAME);
      pm_strcpy(msg, " Plugin: ");
      foreach_alist(plugin, bplugin_list) {
         len = pm_strcat(msg, plugin->file);
         if (len > 80) {
            pm_strcat(msg, "\n   ");
         } else {
            pm_strcat(msg, " ");
         }
      }
      ua->send_msg("%s\n", msg.c_str());
   }
}

static void do_director_status(UAContext *ua)
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
   ua->send_msg("====\n");
}

static void do_storage_status(UAContext *ua, STORE *store, char *cmd)
{
   BSOCK *sd;
   USTORE lstore;

   lstore.store = store;
   pm_strcpy(lstore.store_source, _("unknown source"));
   set_wstorage(ua->jcr, &lstore);
   /* Try connecting for up to 15 seconds */
   if (!ua->api) ua->send_msg(_("Connecting to Storage daemon %s at %s:%d\n"),
      store->name(), store->address, store->SDport);
   if (!connect_to_storage_daemon(ua->jcr, 1, 15, 0)) {
      ua->send_msg(_("\nFailed to connect to Storage daemon %s.\n====\n"),
         store->name());
      if (ua->jcr->store_bsock) {
         bnet_close(ua->jcr->store_bsock);
         ua->jcr->store_bsock = NULL;
      }
      return;
   }
   Dmsg0(20, _("Connected to storage daemon\n"));
   sd = ua->jcr->store_bsock;
   if (cmd) {
      sd->fsend(".status %s", cmd);
   } else {
      sd->fsend("status");
   }
   while (sd->recv() >= 0) {
      ua->send_msg("%s", sd->msg);
   }
   sd->signal( BNET_TERMINATE);
   sd->close();
   ua->jcr->store_bsock = NULL;
   return;
}

static void do_client_status(UAContext *ua, CLIENT *client, char *cmd)
{
   BSOCK *fd;

   /* Connect to File daemon */

   ua->jcr->client = client;
   /* Release any old dummy key */
   if (ua->jcr->sd_auth_key) {
      free(ua->jcr->sd_auth_key);
   }
   /* Create a new dummy SD auth key */
   ua->jcr->sd_auth_key = bstrdup("dummy");

   /* Try to connect for 15 seconds */
   if (!ua->api) ua->send_msg(_("Connecting to Client %s at %s:%d\n"),
      client->name(), client->address, client->FDport);
   if (!connect_to_file_daemon(ua->jcr, 1, 15, 0)) {
      ua->send_msg(_("Failed to connect to Client %s.\n====\n"),
         client->name());
      if (ua->jcr->file_bsock) {
         bnet_close(ua->jcr->file_bsock);
         ua->jcr->file_bsock = NULL;
      }
      return;
   }
   Dmsg0(20, _("Connected to file daemon\n"));
   fd = ua->jcr->file_bsock;
   if (cmd) {
      fd->fsend(".status %s", cmd);
   } else {
      fd->fsend("status");
   }
   while (fd->recv() >= 0) {
      ua->send_msg("%s", fd->msg);
   }
   fd->signal(BNET_TERMINATE);
   fd->close();
   ua->jcr->file_bsock = NULL;

   return;
}

static void prt_runhdr(UAContext *ua)
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
   JOB *job;
   int level;
   int priority;
   utime_t runtime;
   POOL *pool;
   STORE *store;
};

static void prt_runtime(UAContext *ua, sched_pkt *sp)
{
   char dt[MAX_TIME_LENGTH];
   const char *level_ptr;
   bool ok = false;
   bool close_db = false;
   JCR *jcr = ua->jcr;
   MEDIA_DBR mr;
   int orig_jobtype;

   orig_jobtype = jcr->getJobType();
   if (sp->job->JobType == JT_BACKUP) {
      jcr->db = NULL;
      ok = complete_jcr_for_job(jcr, sp->job, sp->pool);
      Dmsg1(250, "Using pool=%s\n", jcr->pool->name());
      if (jcr->db) {
         close_db = true;             /* new db opened, remember to close it */
      }
      if (ok) {
         mr.PoolId = jcr->jr.PoolId;
         jcr->wstore = sp->store;
         set_storageid_in_mr(jcr->wstore, &mr);
         Dmsg0(250, "call find_next_volume_for_append\n");
         /* no need to set ScratchPoolId, since we use fnv_no_create_vol */
         ok = find_next_volume_for_append(jcr, &mr, 1, fnv_no_create_vol, fnv_no_prune);
      }
      if (!ok) {
         bstrncpy(mr.VolumeName, "*unknown*", sizeof(mr.VolumeName));
      }
   }
   bstrftime_nc(dt, sizeof(dt), sp->runtime);
   switch (sp->job->JobType) {
   case JT_ADMIN:
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
      db_close_database(jcr, jcr->db);
   }
   jcr->db = ua->db;                  /* restore ua db to jcr */
   jcr->setJobType(orig_jobtype);
}

/*
 * Sort items by runtime, priority
 */
static int my_compare(void *item1, void *item2)
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

/*
 * Find all jobs to be run in roughly the
 *  next 24 hours.
 */
static void list_scheduled_jobs(UAContext *ua)
{
   utime_t runtime;
   RUN *run;
   JOB *job;
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

   /* Loop through all jobs */
   LockRes();
   foreach_res(job, R_JOB) {
      if (!acl_access_ok(ua, Job_ACL, job->name()) || !job->enabled) {
         continue;
      }
      for (run=NULL; (run = find_next_run(run, job, runtime, days)); ) {
         USTORE store;
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
         sched.binary_insert_multiple(sp, my_compare);
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

static void list_running_jobs(UAContext *ua)
{
   JCR *jcr;
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
         if (jcr->getJobType() == JT_CONSOLE && !ua->api) {
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
      if (jcr->JobId == 0 || !acl_access_ok(ua, Job_ACL, jcr->job->name())) {
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
         if (!jcr->client) {
            Mmsg(emsg, _("is waiting on Client"));
         } else {
            Mmsg(emsg, _("is waiting on Client %s"), jcr->client->name());
         }
         pool_mem = true;
         msg = emsg;
         break;
      case JS_WaitSD:
         emsg = (char *) get_pool_memory(PM_FNAME);
         if (jcr->wstore) {
            Mmsg(emsg, _("is waiting on Storage \"%s\""), jcr->wstore->name());
         } else if (jcr->rstore) {
            Mmsg(emsg, _("is waiting on Storage \"%s\""), jcr->rstore->name());
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
         msg = _("is waiting for its start time");
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
         if (!jcr->client || !jcr->wstore) {
            Mmsg(emsg, _("is waiting for Client to connect to Storage daemon"));
         } else {
            Mmsg(emsg, _("is waiting for Client %s to connect to Storage %s"),
                 jcr->client->name(), jcr->wstore->name());
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

static void list_terminated_jobs(UAContext *ua)
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
      for (int i=0; i<3; i++) {
         if ((p=strrchr(JobName, '.')) != NULL) {
            *p = 0;
         }
      }

      if (!acl_access_ok(ua, Job_ACL, JobName)) {
         continue;
      }

      bstrftime_nc(dt, sizeof(dt), je->end_time);
      switch (je->JobType) {
      case JT_ADMIN:
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
