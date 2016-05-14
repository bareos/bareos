/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2012 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
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
 * BAREOS Director -- User Agent Output Commands
 *
 * I.e. messages, listing database, showing resources, ...
 *
 * Kern Sibbald, September MM
 */

#include "bareos.h"
#include "dird.h"

/* Imported subroutines */

/* Imported variables */
extern struct s_jl joblevels[];

/* Imported functions */

/* Forward referenced functions */
static bool do_list_cmd(UAContext *ua, const char *cmd, e_list_type llist);
static bool list_nextvol(UAContext *ua, int ndays);
static bool parse_list_backups_cmd(UAContext *ua, const char *range, e_list_type llist);

/*
 * Some defaults.
 */
#define DEFAULT_LOG_LINES 5
#define DEFAULT_NR_DAYS 50

/*
 * Turn auto display of console messages on/off
 */
bool autodisplay_cmd(UAContext *ua, const char *cmd)
{
   static const char *kw[] = {
      NT_("on"),
      NT_("off"),
      NULL
   };

   switch (find_arg_keyword(ua, kw)) {
   case 0:
      ua->auto_display_messages = true;
      break;
   case 1:
      ua->auto_display_messages = false;
      break;
   default:
      ua->error_msg(_("ON or OFF keyword missing.\n"));
      break;
   }
   return true;
}

/*
 * Turn GUI mode on/off
 */
bool gui_cmd(UAContext *ua, const char *cmd)
{
   static const char *kw[] = {
      NT_("on"),
      NT_("off"),
      NULL
   };

   switch (find_arg_keyword(ua, kw)) {
   case 0:
      ua->jcr->gui = ua->gui = true;
      break;
   case 1:
      ua->jcr->gui = ua->gui = false;
      break;
   default:
      ua->error_msg(_("ON or OFF keyword missing.\n"));
      break;
   }
   return true;
}

/*
 * Enter with Resources locked
 */
static void show_disabled_jobs(UAContext *ua)
{
   JOBRES *job;
   bool first = true;

   foreach_res(job, R_JOB) {
      if (!acl_access_ok(ua, Job_ACL, job->name())) {
         continue;
      }

      if (!job->enabled) {
         if (first) {
            first = false;
            ua->send_msg(_("Disabled Jobs:\n"));
         }
         ua->send_msg("   %s\n", job->name());
      }
   }

   if (first) {
      ua->send_msg(_("No disabled Jobs.\n"));
   }
}

/*
 * Enter with Resources locked
 */
static void show_disabled_clients(UAContext *ua)
{
   CLIENTRES *client;
   bool first = true;

   foreach_res(client, R_CLIENT) {
      if (!acl_access_ok(ua, Client_ACL, client->name())) {
         continue;
      }

      if (!client->enabled) {
         if (first) {
            first = false;
            ua->send_msg(_("Disabled Clients:\n"));
         }
         ua->send_msg("   %s\n", client->name());
      }
   }

   if (first) {
      ua->send_msg(_("No disabled Clients.\n"));
   }
}

/*
 * Enter with Resources locked
 */
static void show_disabled_schedules(UAContext *ua)
{
   SCHEDRES *sched;
   bool first = true;

   foreach_res(sched, R_SCHEDULE) {
      if (!acl_access_ok(ua, Schedule_ACL, sched->name())) {
         continue;
      }

      if (!sched->enabled) {
         if (first) {
            first = false;
            ua->send_msg(_("Disabled Schedules:\n"));
         }
         ua->send_msg("   %s\n", sched->name());
      }
   }

   if (first) {
      ua->send_msg(_("No disabled Schedules.\n"));
   }
}

struct showstruct {
   const char *res_name;
   int type;
};

static struct showstruct avail_resources[] = {
   { NT_("directors"), R_DIRECTOR },
   { NT_("clients"), R_CLIENT },
   { NT_("counters"), R_COUNTER },
   { NT_("devices"), R_DEVICE },
   { NT_("jobs"), R_JOB },
   { NT_("jobdefs"), R_JOBDEFS },
   { NT_("storages"), R_STORAGE },
   { NT_("catalogs"), R_CATALOG },
   { NT_("schedules"), R_SCHEDULE },
   { NT_("filesets"), R_FILESET },
   { NT_("pools"), R_POOL },
   { NT_("messages"), R_MSGS },
   { NT_("profiles"), R_PROFILE },
   { NT_("consoles"), R_CONSOLE },
   { NT_("all"), -1 },
   { NT_("help"), -2 },
   { NULL, 0 }
};


/*
 *  Displays Resources
 *
 *  show all
 *  show <resource-keyword-name> - e.g. show directors
 *  show <resource-keyword-name>=<name> - e.g. show director=HeadMan
 *  show disabled - shows disabled jobs, clients and schedules
 *  show disabled jobs - shows disabled jobs
 *  show disabled clients - shows disabled clients
 *  show disabled schedules - shows disabled schedules
 */
bool show_cmd(UAContext *ua, const char *cmd)
{
   int i, j, type, len;
   int recurse;
   char *res_name;
   RES *res = NULL;
   bool hide_sensitive_data;

   Dmsg1(20, "show: %s\n", ua->UA_sock->msg);

   /*
    * When the console has no access to the configure cmd then any show cmd
    * will suppress all sensitive information like for instance passwords.
    */
   hide_sensitive_data = !acl_access_ok(ua, Command_ACL, "configure", false);

   LockRes();
   for (i = 1; i < ua->argc; i++) {
      if (bstrcasecmp(ua->argk[i], _("disabled"))) {
         if (((i + 1) < ua->argc) &&
             bstrcasecmp(ua->argk[i + 1], NT_("jobs"))) {
            show_disabled_jobs(ua);
         } else if (((i + 1) < ua->argc) &&
                    bstrcasecmp(ua->argk[i + 1], NT_("clients"))) {
            show_disabled_clients(ua);
         } else if (((i + 1) < ua->argc) &&
                    bstrcasecmp(ua->argk[i + 1], NT_("schedules"))) {
            show_disabled_schedules(ua);
         } else {
            show_disabled_jobs(ua);
            show_disabled_clients(ua);
            show_disabled_schedules(ua);
         }

         goto bail_out;
      }

      type = 0;
      res_name = ua->argk[i];
      if (!ua->argv[i]) {          /* was a name given? */
         /*
          * No name, dump all resources of specified type
          */
         recurse = 1;
         len = strlen(res_name);
         for (j = 0; avail_resources[j].res_name; j++) {
            if (bstrncasecmp(res_name, _(avail_resources[j].res_name), len)) {
               type = avail_resources[j].type;
               if (type > 0) {
                  res = my_config->m_res_head[type - my_config->m_r_first];
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
         for (j = 0; avail_resources[j].res_name; j++) {
            if (bstrncasecmp(res_name, _(avail_resources[j].res_name), len)) {
               type = avail_resources[j].type;
               res = (RES *)GetResWithName(type, ua->argv[i]);
               if (!res) {
                  type = -3;
               }
               break;
            }
         }
      }

      switch (type) {
      case -1:                           /* all */
         for (j = my_config->m_r_first; j <= my_config->m_r_last; j++) {
            switch (j) {
            case R_DEVICE:
               /*
                * Skip R_DEVICE since it is really not used or updated
                */
               continue;
            default:
               if (my_config->m_res_head[j - my_config->m_r_first]) {
                  dump_resource(j, my_config->m_res_head[j - my_config->m_r_first],
                                bsendmsg, ua, hide_sensitive_data);
               }
               break;
            }
         }
         break;
      case -2:
         ua->send_msg(_("Keywords for the show command are:\n"));
         for (j = 0; avail_resources[j].res_name; j++) {
            ua->error_msg("%s\n", _(avail_resources[j].res_name));
         }
         goto bail_out;
      case -3:
         ua->error_msg(_("%s resource %s not found.\n"), res_name, ua->argv[i]);
         goto bail_out;
      case 0:
         ua->error_msg(_("Resource %s not found\n"), res_name);
         goto bail_out;
      default:
         dump_resource(recurse ? type : -type, res, bsendmsg, ua, hide_sensitive_data);
         break;
      }
   }

bail_out:
   UnlockRes();
   return true;
}

/*
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
bool llist_cmd(UAContext *ua, const char *cmd)
{
   return do_list_cmd(ua, cmd, VERT_LIST);
}

/* Do short or summary listing */
bool list_cmd(UAContext *ua, const char *cmd)
{
   return do_list_cmd(ua, cmd, HORZ_LIST);
}

static int get_jobid_from_cmdline(UAContext *ua)
{
   int i, jobid;
   JOB_DBR jr;

   jobid = 0;
   memset(&jr, 0, sizeof(jr));
   i = find_arg_with_value(ua, NT_("ujobid"));
   if (i >= 0) {
      bstrncpy(jr.Job, ua->argv[i], MAX_NAME_LENGTH);
      jr.JobId = 0;
      if (db_get_job_record(ua->jcr, ua->db, &jr)) {
         jobid = jr.JobId;
      } else {
         return -1;
      }
   } else {
      i = find_arg_with_value(ua, NT_("jobid"));
      if (i >= 0) {
         jobid = str_to_int64(ua->argv[i]);
      }
   }

   return jobid;
}

static bool do_list_cmd(UAContext *ua, const char *cmd, e_list_type llist)
{
   JOB_DBR jr;
   POOL_DBR pr;
   MEDIA_DBR mr;
   POOL_MEM query_range(PM_MESSAGE);
   int i, d, h, jobid;
   int days = 0,
       hours = 0,
       jobstatus = 0;
   int count = -1;
   int last_run = -1;
   time_t schedtime = 0;
   const int secs_in_day = 86400;
   const int secs_in_hour = 3600;
   char *clientname = NULL;
   char *volumename = NULL;
   utime_t now;
   bool reverse;

   if (!open_client_db(ua, true)) {
      return true;
   }

   memset(&jr, 0, sizeof(jr));
   memset(&pr, 0, sizeof(pr));
   memset(&mr, 0, sizeof(mr));

   Dmsg1(20, "list: %s\n", cmd);

   /*
    * days or hours given?
    */

   d = find_arg_with_value(ua, NT_("days"));
   h = find_arg_with_value(ua, NT_("hours"));

   now = (utime_t)time(NULL);
   if (d > 0) {
      days = str_to_int64(ua->argv[d]);
      schedtime = now - secs_in_day * days;   /* Days in the past */
   }

   if (h > 0) {
      hours = str_to_int64(ua->argv[h]);
      schedtime = now - secs_in_hour * hours; /* Hours in the past */
   }

   /*
    * Apply any limit
    */
   pm_strcpy(query_range, "");
   i = find_arg_with_value(ua, NT_("limit"));
   if (i >= 0) {
      POOL_MEM temp(PM_MESSAGE);

      jr.limit = atoi(ua->argv[i]);
      temp.bsprintf(" LIMIT %d", atoi(ua->argv[i]));
      pm_strcat(query_range, temp.c_str());

      /*
       * offset is only valid, if limit is given
       */
      i = find_arg_with_value(ua, NT_("offset"));
      if (i >= 0) {
         temp.bsprintf(" OFFSET %d", atoi(ua->argv[i]));
         pm_strcat(query_range, temp.c_str());
      }
   }

   /*
    * jobstatus=X
    */
   if (!get_user_job_status_selection(ua, &jobstatus)) {
      ua->error_msg(_("invalid jobstatus parameter\n"));
      return false;
   }

   /*
    * Select what to do based on the first argument.
    */
   if ((bstrcasecmp(ua->argk[1], NT_("jobs")) && (ua->argv[1] == NULL)) ||
       ((bstrcasecmp(ua->argk[1], NT_("job")) || bstrcasecmp(ua->argk[1], NT_("jobname"))) && ua->argv[1])) {
      /*
       * List jobs or List job=xxx
       */
      i = find_arg_with_value(ua, NT_("jobname"));
      if (i < 0) {
         i = find_arg_with_value(ua, NT_("job"));
      }
      if (i >= 0) {
         jr.JobId = 0;
         bstrncpy(jr.Name, ua->argv[i], MAX_NAME_LENGTH);
      }
      i = find_arg_with_value(ua, NT_("client"));
      if (i >= 0) {
         if (GetClientResWithName(ua->argv[i])) {
            clientname = ua->argv[i];
         } else {
            ua->error_msg(_("invalid client parameter\n"));
            return false;
         }
      }

      i = find_arg_with_value(ua, NT_("volume"));
      if (i >= 0) {
         volumename = ua->argv[i];
      }

      last_run = find_arg(ua, NT_("last"));
      count = find_arg(ua, NT_("count"));
      db_list_job_records(ua->jcr, ua->db, &jr, query_range.c_str(), clientname,
                          jobstatus, volumename, schedtime, last_run, count,
                          ua->send, llist);
   } else if (bstrcasecmp(ua->argk[1], NT_("jobtotals"))) {
      /*
       * List JOBTOTALS
       */
      db_list_job_totals(ua->jcr, ua->db, &jr, ua->send);
   } else if (bstrcasecmp(ua->argk[1], NT_("jobid"))) {
      /*
       * List JOBID=nn
       */
      if (ua->argv[1]) {
         jobid = str_to_int64(ua->argv[1]);
         if (jobid > 0) {
            jr.JobId = jobid;
            db_list_job_records(ua->jcr, ua->db, &jr, query_range.c_str(), clientname,
                                jobstatus, volumename, schedtime, last_run, count,
                                ua->send, llist);
         }
      }
  } else if (bstrcasecmp(ua->argk[1], NT_("ujobid")) && ua->argv[1]) {
      /*
       * List UJOBID=xxx
       */
      bstrncpy(jr.Job, ua->argv[1], MAX_NAME_LENGTH);
      jr.JobId = 0;
      db_list_job_records(ua->jcr, ua->db, &jr, query_range.c_str(), clientname,
                          jobstatus, volumename, schedtime, last_run, count,
                          ua->send, llist);
   } else if (bstrcasecmp(ua->argk[1], NT_("basefiles"))) {
      /*
       * List BASEFILES
       */
      jobid = get_jobid_from_cmdline(ua);
      if (jobid > 0) {
         db_list_base_files_for_job(ua->jcr, ua->db, jobid, ua->send);
       } else {
         ua->error_msg(_("missing parameter: jobid\n"));
      }
   } else if (bstrcasecmp(ua->argk[1], NT_("files"))) {
      /*
       * List FILES
       */
      jobid = get_jobid_from_cmdline(ua);
      if (jobid > 0) {
         db_list_files_for_job(ua->jcr, ua->db, jobid, ua->send);
      } else {
         ua->error_msg(_("missing parameter: jobid\n"));
      }
   } else if (bstrcasecmp(ua->argk[1], NT_("fileset"))) {
      int filesetid = 0;

      /*
       * List FILESET
       */
      i = find_arg_with_value(ua, NT_("filesetid"));
      if (i > 0) {
         filesetid = str_to_int64(ua->argv[i]);
      }

      jobid = get_jobid_from_cmdline(ua);
      if (jobid > 0 || filesetid > 0) {
         jr.JobId = jobid;
         jr.FileSetId = filesetid;
         db_list_filesets(ua->jcr, ua->db, &jr, query_range.c_str(), ua->send, llist);
      } else {
         ua->error_msg(_("missing parameter: jobid or filesetid\n"));
      }
   } else if (bstrcasecmp(ua->argk[1], NT_("filesets"))) {
      /*
       * List FILESETs
       */
      db_list_filesets(ua->jcr, ua->db, &jr, query_range.c_str(), ua->send, llist);
   } else if (bstrcasecmp(ua->argk[1], NT_("jobmedia"))) {
      /*
       * List JOBMEDIA
       */
      jobid = get_jobid_from_cmdline(ua);
      if (jobid >= 0) {
         db_list_jobmedia_records(ua->jcr, ua->db, jobid, ua->send, llist);
      } else {
         ua->error_msg(_("missing parameter: jobid\n"));
      }
   } else if (bstrcasecmp(ua->argk[1], NT_("joblog"))) {
      /*
       * List JOBLOG
       */
      jobid = get_jobid_from_cmdline(ua);
      if (jobid >= 0) {
         db_list_joblog_records(ua->jcr, ua->db, jobid, ua->send, llist);
      } else {
         ua->error_msg(_("missing parameter: jobid\n"));
      }
   } else if (bstrcasecmp(ua->argk[1], NT_("log"))) {
      /*
       * List last <limit> LOG entries
       * default is DEFAULT_LOG_LINES entries
       */
      reverse = (find_arg(ua, NT_("reverse")) != -1);

      if (strlen(query_range.c_str()) == 0) {
         Mmsg(query_range, " LIMIT %d", DEFAULT_LOG_LINES);
      }
      db_list_log_records(ua->jcr, ua->db, query_range.c_str(), reverse,  ua->send, llist);
   } else if (bstrcasecmp(ua->argk[1], NT_("pool")) ||
              bstrcasecmp(ua->argk[1], NT_("pools"))) {
      POOL_DBR pr;

      /*
       * List POOLS
       */
      memset(&pr, 0, sizeof(pr));
      if (ua->argv[1]) {
         bstrncpy(pr.Name, ua->argv[1], sizeof(pr.Name));
      }
      db_list_pool_records(ua->jcr, ua->db, &pr, ua->send, llist);
   } else if (bstrcasecmp(ua->argk[1], NT_("clients"))) {
      /*
       * List CLIENTS
       */
      db_list_client_records(ua->jcr, ua->db, NULL, ua->send, llist);
   } else if (bstrcasecmp(ua->argk[1], NT_("client")) && ua->argv[1]) {
      /*
       * List CLIENT=xxx
       */
      db_list_client_records(ua->jcr, ua->db, ua->argv[1], ua->send, llist);
   } else if (bstrcasecmp(ua->argk[1], NT_("storages"))) {
      /*
       * List STORAGES
       */
       db_list_sql_query(ua->jcr, ua->db, "SELECT * FROM Storage", ua->send, llist, "storages");
   } else if (bstrcasecmp(ua->argk[1], NT_("media")) ||
              bstrcasecmp(ua->argk[1], NT_("volume")) ||
              bstrcasecmp(ua->argk[1], NT_("volumes"))) {
      /*
       * List MEDIA or VOLUMES
       */
      jobid = get_jobid_from_cmdline(ua);
      if (jobid > 0) {
         int count;
         POOLMEM *VolumeName;

         VolumeName = get_pool_memory(PM_FNAME);
         count = db_get_job_volume_names(ua->jcr, ua->db, jobid, VolumeName);
         ua->send_msg(_("Jobid %d used %d Volume(s): %s\n"), jobid, count, VolumeName);
         free_pool_memory(VolumeName);
      } else if (jobid == 0) {
         /*
          * List a specific volume?
          */
         if (ua->argv[1]) {
            bstrncpy(mr.VolumeName, ua->argv[1], sizeof(mr.VolumeName));
            ua->send->object_start("volume");
            db_list_media_records(ua->jcr, ua->db, &mr, ua->send, llist);
            ua->send->object_end("volume");
         } else {
            /*
             * If no job or jobid keyword found, then we list all media
             * Is a specific pool wanted?
             */
            i = find_arg_with_value(ua, NT_("pool"));
            if (i >= 0) {
               bstrncpy(pr.Name, ua->argv[i], sizeof(pr.Name));
               if (!get_pool_dbr(ua, &pr)) {
                  ua->error_msg(_("Pool %s doesn't exist.\n"), ua->argv[i]);
                  return true;
               }

               mr.PoolId = pr.PoolId;
               ua->send->array_start("volumes");
               db_list_media_records(ua->jcr, ua->db, &mr, ua->send, llist);
               ua->send->array_end("volumes");
               return true;
            } else {
               int num_pools;
               uint32_t *ids;

               /*
                * List all volumes, flat
                */
               if (find_arg(ua, NT_("all")) > 0) {
                  ua->send->array_start("volumes");
                  db_list_media_records(ua->jcr, ua->db, &mr, ua->send, llist);
                  ua->send->array_end("volumes");
               } else {
                  /*
                   * List Volumes in all pools
                   */
                  if (!db_get_pool_ids(ua->jcr, ua->db, &num_pools, &ids)) {
                     ua->error_msg(_("Error obtaining pool ids. ERR=%s\n"),
                           db_strerror(ua->db));
                     return true;
                  }

                  if (num_pools <= 0) {
                     return true;
                  }

                  ua->send->object_start("volumes");
                  for (i = 0; i < num_pools; i++) {
                     pr.PoolId = ids[i];
                     if (db_get_pool_record(ua->jcr, ua->db, &pr)) {
                        ua->send->decoration( "Pool: %s\n", pr.Name );
                        ua->send->array_start(pr.Name);
                        mr.PoolId = ids[i];
                        db_list_media_records(ua->jcr, ua->db, &mr, ua->send, llist);
                        ua->send->array_end(pr.Name);
                     }
                  }
                  ua->send->object_end("volumes");
                  free(ids);
               }
            }
         }
      }
      return true;
   } else if (bstrcasecmp(ua->argk[1], NT_("nextvol")) ||
              bstrcasecmp(ua->argk[1], NT_("nextvolume"))) {
      int days;

      /*
       * List next volume
       */
      days = 1;

      i = find_arg_with_value(ua, NT_("days"));
      if (i >= 0) {
         days = atoi(ua->argv[i]);
         if ((days < 0) || (days > DEFAULT_NR_DAYS)) {
            ua->warning_msg(_("Ignoring invalid value for days. Max is %d.\n"), DEFAULT_NR_DAYS);
            days = 1;
         }
      }
      list_nextvol(ua, days);
   } else if (bstrcasecmp(ua->argk[1], NT_("copies"))) {
      /*
       * List copies
       */
      i = find_arg_with_value(ua, NT_("jobid"));
      if (i >= 0) {
         if (is_a_number_list(ua->argv[i])) {
            db_list_copies_records(ua->jcr, ua->db, query_range.c_str(), ua->argv[i], ua->send, llist);
         }
      } else {
         db_list_copies_records(ua->jcr, ua->db, query_range.c_str(), NULL, ua->send, llist);
      }
   } else if (bstrcasecmp(ua->argk[1], NT_("backups"))) {
      if (parse_list_backups_cmd(ua, query_range.c_str(), llist)) {
         db_list_sql_query(ua->jcr, ua->db, ua->cmd, ua->send, llist, "backups");
      }
   } else {
      ua->error_msg(_("Unknown list keyword: %s\n"), NPRT(ua->argk[1]));
   }

   return true;
}

static inline bool parse_jobstatus_selection_param(POOL_MEM &selection,
                                                   UAContext *ua,
                                                   const char *default_selection)
{
   int pos;

   selection.strcpy("");
   if ((pos = find_arg_with_value(ua, "jobstatus")) >= 0) {
      int cnt = 0;
      int jobstatus;
      POOL_MEM temp;
      char *cur_stat, *bp;

      cur_stat = ua->argv[pos];
      while (cur_stat) {
         bp = strchr(cur_stat, ',');
         if (bp) {
            *bp++ = '\0';
         }

         /*
          * Try matching the status to an internal Job Termination code.
          */
         if (strlen(cur_stat) == 1 && cur_stat[0] >= 'A' && cur_stat[0] <= 'z') {
            jobstatus = cur_stat[0];
         } else if (bstrcasecmp(cur_stat, "terminated")) {
            jobstatus = JS_Terminated;
         } else if (bstrcasecmp(cur_stat, "warnings")) {
            jobstatus = JS_Warnings;
         } else if (bstrcasecmp(cur_stat, "canceled")) {
            jobstatus = JS_Canceled;
         } else if (bstrcasecmp(cur_stat, "running")) {
            jobstatus = JS_Running;
         } else if (bstrcasecmp(cur_stat, "error")) {
            jobstatus = JS_Error;
         } else if (bstrcasecmp(cur_stat, "fatal")) {
            jobstatus = JS_FatalError;
         } else {
            cur_stat = bp;
            continue;
         }

         if (cnt == 0) {
            Mmsg(temp, " AND JobStatus IN ('%c'", jobstatus);
            pm_strcat(selection, temp.c_str());
         } else {
            Mmsg(temp, ",'%c'", jobstatus);
            pm_strcat(selection, temp.c_str());
         }
         cur_stat = bp;
         cnt++;
      }

      /*
       * Close set if we opened one.
       */
      if (cnt > 0) {
         pm_strcat(selection, ")");
      }
   }

   if (selection.strlen() == 0) {
      /*
       * When no explicit Job Termination code specified use default
       */
      selection.strcpy(default_selection);
   }

   return true;
}

static inline bool parse_level_selection_param(POOL_MEM &selection,
                                               UAContext *ua,
                                               const char *default_selection)
{
   int pos;

   selection.strcpy("");
   if ((pos = find_arg_with_value(ua, "level")) >= 0) {
      int cnt = 0;
      POOL_MEM temp;
      char *cur_level, *bp;

      cur_level = ua->argv[pos];
      while (cur_level) {
         bp = strchr(cur_level, ',');
         if (bp) {
            *bp++ = '\0';
         }

         /*
          * Try mapping from text level to internal level.
          */
         for (int i = 0; joblevels[i].level_name; i++) {
            if (joblevels[i].job_type == JT_BACKUP &&
                bstrncasecmp(joblevels[i].level_name, cur_level,
                             strlen(cur_level))) {

               if (cnt == 0) {
                  Mmsg(temp, " AND Level IN ('%c'", joblevels[i].level);
                  pm_strcat(selection, temp.c_str());
               } else {
                  Mmsg(temp, ",'%c'", joblevels[i].level);
                  pm_strcat(selection, temp.c_str());
               }
            }
         }
         cur_level = bp;
         cnt++;
      }

      /*
       * Close set if we opened one.
       */
      if (cnt > 0) {
         pm_strcat(selection, ")");
      }
   }
   if (selection.strlen() == 0) {
      selection.strcpy(default_selection);
   }

   return true;
}

static inline bool parse_fileset_selection_param(POOL_MEM &selection,
                                                 UAContext *ua,
                                                 bool listall)
{
   int fileset;

   pm_strcpy(selection, "");
   fileset = find_arg_with_value(ua, "fileset");
   if ((fileset >= 0 && bstrcasecmp(ua->argv[fileset], "any")) || (listall && fileset < 0)) {
      FILESETRES *fs;
      POOL_MEM temp(PM_MESSAGE);

      LockRes();
      foreach_res(fs, R_FILESET) {
         if (!acl_access_ok(ua, FileSet_ACL, fs->name(), false)) {
            continue;
         }
         if (selection.strlen() == 0) {
            temp.bsprintf("AND (FileSet='%s'", fs->name());
         } else {
            temp.bsprintf(" OR FileSet='%s'", fs->name());
         }
         pm_strcat(selection, temp.c_str());
      }
      pm_strcat(selection, ") ");
      UnlockRes();
   } else if (fileset >= 0) {
      if (!acl_access_ok(ua, FileSet_ACL, ua->argv[fileset], true)) {
         ua->error_msg(_("Access to specified FileSet not allowed.\n"));
         return false;
      } else {
         selection.bsprintf("AND FileSet='%s' ", ua->argv[fileset]);
      }
   }

   return true;
}

static bool parse_list_backups_cmd(UAContext *ua, const char *range, e_list_type llist)
{
   int pos, client;
   POOL_MEM temp(PM_MESSAGE),
            selection(PM_MESSAGE),
            criteria(PM_MESSAGE);

   client = find_arg_with_value(ua, "client");
   if (client < 0) {
      ua->error_msg(_("missing parameter: client\n"));
      return false;
   }

   if (!acl_access_ok(ua, Client_ACL, ua->argv[client], true)) {
      ua->error_msg(_("Access to specified Client not allowed.\n"));
      return false;
   }

   selection.bsprintf("AND Job.Type='B' AND Client.Name='%s' ", ua->argv[client]);

   /*
    * Build a selection pattern based on the jobstatus and level arguments.
    */
   parse_jobstatus_selection_param(temp, ua, "AND JobStatus IN ('T','W') ");
   pm_strcat(selection, temp.c_str());

   parse_level_selection_param(temp, ua, "");
   pm_strcat(selection, temp.c_str());

   if (!parse_fileset_selection_param(temp, ua, true)) {
      return false;
   }
   pm_strcat(selection, temp.c_str());

   /*
    * Build a criteria pattern if the order and/or limit argument are given.
    */
   pm_strcpy(criteria, "");
   if ((pos = find_arg_with_value(ua, "order")) >= 0) {
      if (bstrncasecmp(ua->argv[pos], "ascending", strlen(ua->argv[pos]))) {
         pm_strcat(criteria, " ASC");
      } else if (bstrncasecmp(ua->argv[pos], "descending", strlen(ua->argv[pos]))) {
         pm_strcat(criteria, " DESC");
      } else {
         return false;
      }
   }

   /*
    * add range settings
    */
   pm_strcat(criteria, range);

   if (llist == VERT_LIST) {
      Mmsg(ua->cmd, list_jobs_long, selection.c_str(), criteria.c_str());
   } else {
      Mmsg(ua->cmd, list_jobs, selection.c_str(), criteria.c_str());
   }

   return true;
}

static bool list_nextvol(UAContext *ua, int ndays)
{
   int i;
   JOBRES *job;
   JCR *jcr;
   USTORERES store;
   RUNRES *run;
   utime_t runtime;
   bool found = false;
   MEDIA_DBR mr;
   POOL_DBR pr;

   memset(&mr, 0, sizeof(mr));

   i = find_arg_with_value(ua, "job");
   if (i <= 0) {
      if ((job = select_job_resource(ua)) == NULL) {
         return false;
      }
   } else {
      job = (JOBRES *)GetResWithName(R_JOB, ua->argv[i]);
      if (!job) {
         Jmsg(ua->jcr, M_ERROR, 0, _("%s is not a job name.\n"), ua->argv[i]);
         if ((job = select_job_resource(ua)) == NULL) {
            return false;
         }
      }
   }

   jcr = new_jcr(sizeof(JCR), dird_free_jcr);
   for (run=NULL; (run = find_next_run(run, job, runtime, ndays)); ) {
      if (!complete_jcr_for_job(jcr, job, run->pool)) {
         found = false;
         goto get_out;
      }
      if (!jcr->jr.PoolId) {
         ua->error_msg(_("Could not find Pool for Job %s\n"), job->name());
         continue;
      }
      memset(&pr, 0, sizeof(pr));
      pr.PoolId = jcr->jr.PoolId;
      if (!db_get_pool_record(jcr, jcr->db, &pr)) {
         bstrncpy(pr.Name, "*UnknownPool*", sizeof(pr.Name));
      }
      mr.PoolId = jcr->jr.PoolId;
      get_job_storage(&store, job, run);
      set_storageid_in_mr(store.store, &mr);
      /* no need to set ScratchPoolId, since we use fnv_no_create_vol */
      if (!find_next_volume_for_append(jcr, &mr, 1, NULL, fnv_no_create_vol, fnv_prune)) {
         ua->error_msg(_("Could not find next Volume for Job %s (Pool=%s, Level=%s).\n"),
            job->name(), pr.Name, level_to_str(run->level));
      } else {
         ua->send_msg(
            _("The next Volume to be used by Job \"%s\" (Pool=%s, Level=%s) will be %s\n"),
            job->name(), pr.Name, level_to_str(run->level), mr.VolumeName);
         found = true;
      }
   }

get_out:
   if (jcr->db) {
      db_sql_close_pooled_connection(jcr, jcr->db);
      jcr->db = NULL;
   }
   free_jcr(jcr);
   if (!found) {
      ua->error_msg(_("Could not find next Volume for Job %s.\n"),
         job->hdr.name);
      return false;
   }
   return true;
}

/*
 * For a given job, we examine all his run records
 *  to see if it is scheduled today or tomorrow.
 */
RUNRES *find_next_run(RUNRES *run, JOBRES *job, utime_t &runtime, int ndays)
{
   time_t now, future, endtime;
   SCHEDRES *sched;
   struct tm tm, runtm;
   int mday, wday, month, wom, i;
   int woy;
   int day;
   int is_scheduled;

   sched = job->schedule;
   if (sched == NULL) {            /* scheduled? */
      return NULL;                 /* no nothing to report */
   }

   /* Break down the time into components */
   now = time(NULL);
   endtime = now + (ndays * 60 * 60 * 24);

   if (run == NULL) {
      run = sched->run;
   } else {
      run = run->next;
   }
   for ( ; run; run=run->next) {
      /*
       * Find runs in next 24 hours.  Day 0 is today, so if
       *   ndays=1, look at today and tomorrow.
       */
      for (day = 0; day <= ndays; day++) {
         future = now + (day * 60 * 60 * 24);

         /* Break down the time into components */
         blocaltime(&future, &tm);
         mday = tm.tm_mday - 1;
         wday = tm.tm_wday;
         month = tm.tm_mon;
         wom = mday / 7;
         woy = tm_woy(future);

         is_scheduled = bit_is_set(mday, run->mday) && bit_is_set(wday, run->wday) &&
            bit_is_set(month, run->month) && bit_is_set(wom, run->wom) &&
            bit_is_set(woy, run->woy);

#ifdef xxx
         Pmsg2(000, "day=%d is_scheduled=%d\n", day, is_scheduled);
         Pmsg1(000, "bit_set_mday=%d\n", bit_is_set(mday, run->mday));
         Pmsg1(000, "bit_set_wday=%d\n", bit_is_set(wday, run->wday));
         Pmsg1(000, "bit_set_month=%d\n", bit_is_set(month, run->month));
         Pmsg1(000, "bit_set_wom=%d\n", bit_is_set(wom, run->wom));
         Pmsg1(000, "bit_set_woy=%d\n", bit_is_set(woy, run->woy));
#endif

         if (is_scheduled) { /* Jobs scheduled on that day */
#ifdef xxx
            char buf[300], num[10];
            bsnprintf(buf, sizeof(buf), "tm.hour=%d hour=", tm.tm_hour);
            for (i=0; i<24; i++) {
               if (bit_is_set(i, run->hour)) {
                  bsnprintf(num, sizeof(num), "%d ", i);
                  bstrncat(buf, num, sizeof(buf));
               }
            }
            bstrncat(buf, "\n", sizeof(buf));
            Pmsg1(000, "%s", buf);
#endif
            /* find time (time_t) job is to be run */
            blocaltime(&future, &runtm);
            for (i= 0; i < 24; i++) {
               if (bit_is_set(i, run->hour)) {
                  runtm.tm_hour = i;
                  runtm.tm_min = run->minute;
                  runtm.tm_sec = 0;
                  runtime = mktime(&runtm);
                  Dmsg2(200, "now=%d runtime=%lld\n", now, runtime);
                  if ((runtime > now) && (runtime < endtime)) {
                     Dmsg2(200, "Found it level=%d %c\n", run->level, run->level);
                     return run;         /* found it, return run resource */
                  }
               }
            }
         }
      }
   } /* end for loop over runs */
   /* Nothing found */
   return NULL;
}

/*
 * Fill in the remaining fields of the jcr as if it is going to run the job.
 */
bool complete_jcr_for_job(JCR *jcr, JOBRES *job, POOLRES *pool)
{
   POOL_DBR pr;

   memset(&pr, 0, sizeof(pr));
   set_jcr_defaults(jcr, job);
   if (pool) {
      jcr->res.pool = pool;           /* override */
   }
   if (jcr->db) {
      Dmsg0(100, "complete_jcr close db\n");
      db_sql_close_pooled_connection(jcr, jcr->db);
      jcr->db = NULL;
   }

   Dmsg0(100, "complete_jcr open db\n");
   jcr->db = db_sql_get_pooled_connection(jcr,
                                          jcr->res.catalog->db_driver,
                                          jcr->res.catalog->db_name,
                                          jcr->res.catalog->db_user,
                                          jcr->res.catalog->db_password.value,
                                          jcr->res.catalog->db_address,
                                          jcr->res.catalog->db_port,
                                          jcr->res.catalog->db_socket,
                                          jcr->res.catalog->mult_db_connections,
                                          jcr->res.catalog->disable_batch_insert,
                                          jcr->res.catalog->try_reconnect,
                                          jcr->res.catalog->exit_on_fatal);
   if (jcr->db == NULL) {
      Jmsg(jcr, M_FATAL, 0, _("Could not open database \"%s\".\n"),
                 jcr->res.catalog->db_name);
      return false;
   }
   bstrncpy(pr.Name, jcr->res.pool->name(), sizeof(pr.Name));
   while (!db_get_pool_record(jcr, jcr->db, &pr)) { /* get by Name */
      /* Try to create the pool */
      if (create_pool(jcr, jcr->db, jcr->res.pool, POOL_OP_CREATE) < 0) {
         Jmsg(jcr, M_FATAL, 0, _("Pool %s not in database. %s"), pr.Name,
            db_strerror(jcr->db));
         if (jcr->db) {
            db_sql_close_pooled_connection(jcr, jcr->db);
            jcr->db = NULL;
         }
         return false;
      } else {
         Jmsg(jcr, M_INFO, 0, _("Pool %s created in database.\n"), pr.Name);
      }
   }
   jcr->jr.PoolId = pr.PoolId;
   return true;
}

static void con_lock_release(void *arg)
{
   Vw(con_lock);
}

void do_messages(UAContext *ua, const char *cmd)
{
   char msg[2000];
   int mlen;
   bool do_truncate = false;

   /*
    * Flush any queued messages.
    */
   if (ua->jcr) {
      dequeue_messages(ua->jcr);
   }

   Pw(con_lock);
   pthread_cleanup_push(con_lock_release, (void *)NULL);
   rewind(con_fd);
   while (fgets(msg, sizeof(msg), con_fd)) {
      mlen = strlen(msg);
      ua->UA_sock->msg = check_pool_memory_size(ua->UA_sock->msg, mlen+1);
      strcpy(ua->UA_sock->msg, msg);
      ua->UA_sock->msglen = mlen;
      ua->UA_sock->send();
      do_truncate = true;
   }
   if (do_truncate) {
      (void)ftruncate(fileno(con_fd), 0L);
   }
   console_msg_pending = FALSE;
   ua->user_notified_msg_pending = FALSE;
   pthread_cleanup_pop(0);
   Vw(con_lock);
}

bool dot_messages_cmd(UAContext *ua, const char *cmd)
{
   if (console_msg_pending && ua->auto_display_messages) {
      do_messages(ua, cmd);
   }
   return true;
}

bool messages_cmd(UAContext *ua, const char *cmd)
{
   if (console_msg_pending) {
      do_messages(ua, cmd);
   } else {
      ua->send->decoration(_("You have no messages.\n"));
   }
   return true;
}

/*
 * Callback routine for "printing" database file listing
 */
bool printit(void *ctx, const char *msg)
{
   bool retval = false;
   UAContext *ua = (UAContext *)ctx;

   if (ua->UA_sock) {
      retval = ua->UA_sock->fsend("%s", msg);
   } else {                           /* No UA, send to Job */
      Jmsg(ua->jcr, M_INFO, 0, "%s", msg);
      retval = true;
   }
   return retval;
}

/*
 * Format message and send to other end.

 * If the UA_sock is NULL, it means that there is no user
 * agent, so we are being called from BAREOS core. In
 * that case direct the messages to the Job.
 */
#ifdef HAVE_VA_COPY
void bmsg(UAContext *ua, const char *fmt, va_list arg_ptr)
{
   BSOCK *bs = ua->UA_sock;
   int maxlen, len;
   POOLMEM *msg = NULL;
   va_list ap;

   if (bs) {
      msg = bs->msg;
   }
   if (!msg) {
      msg = get_pool_memory(PM_EMSG);
   }

again:
   maxlen = sizeof_pool_memory(msg) - 1;
   va_copy(ap, arg_ptr);
   len = bvsnprintf(msg, maxlen, fmt, ap);
   va_end(ap);
   if (len < 0 || len >= maxlen) {
      msg = realloc_pool_memory(msg, maxlen + maxlen/2);
      goto again;
   }

   if (bs) {
      bs->msg = msg;
      bs->msglen = len;
      bs->send();
   } else {                           /* No UA, send to Job */
      Jmsg(ua->jcr, M_INFO, 0, "%s", msg);
      free_pool_memory(msg);
   }

}

#else /* no va_copy() -- brain damaged version of variable arguments */

void bmsg(UAContext *ua, const char *fmt, va_list arg_ptr)
{
   BSOCK *bs = ua->UA_sock;
   int maxlen, len;
   POOLMEM *msg = NULL;

   if (bs) {
      msg = bs->msg;
   }
   if (!msg) {
      msg = get_memory(5000);
   }

   maxlen = sizeof_pool_memory(msg) - 1;
   if (maxlen < 4999) {
      msg = realloc_pool_memory(msg, 5000);
      maxlen = 4999;
   }
   len = bvsnprintf(msg, maxlen, fmt, arg_ptr);
   if (len < 0 || len >= maxlen) {
      pm_strcpy(msg, _("Message too long to display.\n"));
      len = strlen(msg);
   }

   if (bs) {
      bs->msg = msg;
      bs->msglen = len;
      bs->send();
   } else {                           /* No UA, send to Job */
      Jmsg(ua->jcr, M_INFO, 0, "%s", msg);
      free_pool_memory(msg);
   }

}
#endif

void bsendmsg(void *ctx, const char *fmt, ...)
{
   va_list arg_ptr;
   va_start(arg_ptr, fmt);
   bmsg((UAContext *)ctx, fmt, arg_ptr);
   va_end(arg_ptr);
}

/*
 * The following UA methods are mainly intended for GUI
 * programs
 */
/*
 * This is a message that should be displayed on the user's
 *  console.
 */
void UAContext::send_msg(const char *fmt, ...)
{
   va_list arg_ptr;
   POOL_MEM message;

   /* send current buffer */
   send->send_buffer();

   va_start(arg_ptr, fmt);
   message.bvsprintf(fmt, arg_ptr);
   va_end(arg_ptr);
   send->message(NULL, message);
}

/*
 * This is an error condition with a command. The gui should put
 *  up an error or critical dialog box.  The command is aborted.
 */
void UAContext::error_msg(const char *fmt, ...)
{
   va_list arg_ptr;
   BSOCK *bs = UA_sock;
   POOL_MEM message;

   /* send current buffer */
   send->send_buffer();

   if (bs && api) bs->signal(BNET_ERROR_MSG);
   va_start(arg_ptr, fmt);
   message.bvsprintf(fmt, arg_ptr);
   va_end(arg_ptr);
   send->message(MSG_TYPE_ERROR, message);
}

/*
 * This is a warning message, that should bring up a warning
 *  dialog box on the GUI. The command is not aborted, but something
 *  went wrong.
 */
void UAContext::warning_msg(const char *fmt, ...)
{
   va_list arg_ptr;
   BSOCK *bs = UA_sock;
   POOL_MEM message;

   /* send current buffer */
   send->send_buffer();

   if (bs && api) bs->signal(BNET_WARNING_MSG);
   va_start(arg_ptr, fmt);
   message.bvsprintf(fmt, arg_ptr);
   va_end(arg_ptr);
   send->message(MSG_TYPE_WARNING, message);
}

/*
 * This is an information message that should probably be put
 *  into the status line of a GUI program.
 */
void UAContext::info_msg(const char *fmt, ...)
{
   va_list arg_ptr;
   BSOCK *bs = UA_sock;
   POOL_MEM message;

   /* send current buffer */
   send->send_buffer();

   if (bs && api) bs->signal(BNET_INFO_MSG);
   va_start(arg_ptr, fmt);
   message.bvsprintf(fmt, arg_ptr);
   va_end(arg_ptr);
   send->message(MSG_TYPE_INFO, message);
}
