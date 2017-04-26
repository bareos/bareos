/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2012 Free Software Foundation Europe e.V.
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
 * Kern Sibbald, September MM
 */
/**
 * @file
 * User Agent Output Commands
 *
 * I.e. messages, listing database, showing resources, ...
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

/**
 * Some defaults.
 */
#define DEFAULT_LOG_LINES 5
#define DEFAULT_NR_DAYS 50

/**
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

/**
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

/**
 * Enter with Resources locked
 */
static void show_disabled_jobs(UAContext *ua)
{
   JOBRES *job;
   bool first = true;

   foreach_res(job, R_JOB) {
      if (!ua->acl_access_ok(Job_ACL, job->name(), false)) {
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

/**
 * Enter with Resources locked
 */
static void show_disabled_clients(UAContext *ua)
{
   CLIENTRES *client;
   bool first = true;

   foreach_res(client, R_CLIENT) {
      if (!ua->acl_access_ok(Client_ACL, client->name(), false)) {
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

/**
 * Enter with Resources locked
 */
static void show_disabled_schedules(UAContext *ua)
{
   SCHEDRES *sched;
   bool first = true;

   foreach_res(sched, R_SCHEDULE) {
      if (!ua->acl_access_ok(Schedule_ACL, sched->name(), false)) {
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

/**
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
   bool verbose = false;
   bool hide_sensitive_data;

   Dmsg1(20, "show: %s\n", ua->UA_sock->msg);

   /*
    * When the console has no access to the configure cmd then any show cmd
    * will suppress all sensitive information like for instance passwords.
    */
   hide_sensitive_data = !ua->acl_access_ok(Command_ACL, "configure", false);

   if (find_arg(ua, NT_("verbose")) > 0) {
      verbose = true;
   }

   LockRes();
   for (i = 1; i < ua->argc; i++) {
      /*
       * skip verbose keyword, already handled earlier.
       */
      if (bstrcasecmp(ua->argk[i], NT_("verbose"))) {
         continue;
      }

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
               res = (RES *)ua->GetResWithName(type, ua->argv[i], true);
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
                                bsendmsg, ua, hide_sensitive_data, verbose);
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
         dump_resource(recurse ? type : -type, res, bsendmsg, ua, hide_sensitive_data, verbose);
         break;
      }
   }

bail_out:
   UnlockRes();
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
   CLIENT_DBR cr;

   memset(&jr, 0, sizeof(jr));

   i = find_arg_with_value(ua, NT_("ujobid"));
   if (i >= 0) {
      bstrncpy(jr.Job, ua->argv[i], MAX_NAME_LENGTH);
      jr.JobId = 0;
      if (ua->db->get_job_record(ua->jcr, &jr)) {
         jobid = jr.JobId;
      } else {
         return -1;
      }
   } else {
      i = find_arg_with_value(ua, NT_("jobid"));
      if (i >= 0) {
         jr.JobId = str_to_int64(ua->argv[i]);
      } else {
         jobid = 0;
         goto bail_out;
      }
   }

   if (ua->db->get_job_record(ua->jcr, &jr)) {
      jobid = jr.JobId;
   } else {
      Dmsg1(200, "get_jobid_from_cmdline: Failed to get job record for JobId %d\n", jr.JobId);
      jobid = -1;
      goto bail_out;
   }

   if (!ua->acl_access_ok(Job_ACL, jr.Name, true)) {
      Dmsg1(200, "get_jobid_from_cmdline: No access to Job %s\n", jr.Name);
      jobid = -1;
      goto bail_out;
   }

   if (jr.ClientId) {
      cr.ClientId = jr.ClientId;
      if (ua->db->get_client_record(ua->jcr, &cr)) {
         if (!ua->acl_access_ok(Client_ACL, cr.Name, true)) {
            Dmsg1(200, "get_jobid_from_cmdline: No access to Client %s\n", cr.Name);
            jobid = -1;
            goto bail_out;
         }
      } else {
         Dmsg1(200, "get_jobid_from_cmdline: Failed to get client record for ClientId %d\n", jr.ClientId);
         jobid = -1;
         goto bail_out;
      }
   }

bail_out:
   return jobid;
}

/**
 * Filter convience functions that abstract the actions needed to
 * perform a certain type of acl or resource filtering.
 */
static inline void set_acl_filter(UAContext *ua, int column, int acltype)
{
   if (ua->acl_has_restrictions(acltype)) {
      ua->send->add_acl_filter_tuple(column, acltype);
   }
}

static inline void set_res_filter(UAContext *ua, int column, int restype)
{
   ua->send->add_res_filter_tuple(column, restype);
}

static inline void set_enabled_filter(UAContext *ua, int column, int restype)
{
   ua->send->add_enabled_filter_tuple(column, restype);
}

static inline void set_disabled_filter(UAContext *ua, int column, int restype)
{
   ua->send->add_disabled_filter_tuple(column, restype);
}

static inline void set_hidden_column_acl_filter(UAContext *ua, int column, int acltype)
{
   ua->send->add_hidden_column(column);
   if (ua->acl_has_restrictions(acltype)) {
      ua->send->add_acl_filter_tuple(column, acltype);
   }
}

static inline void set_hidden_column(UAContext *ua, int column)
{
   ua->send->add_hidden_column(column);
}

static void set_query_range(POOL_MEM &query_range, UAContext *ua, JOB_DBR *jr)
{
   int i;

   /*
    * See if this is a second call to set_query_range() if so and any acl
    * filters have been set we setup a new query_range filter including a
    * limit filter.
    */
   if (query_range.strlen()) {
      if (!ua->send->has_acl_filters()) {
         return;
      }
      pm_strcpy(query_range, "");
   }

   /*
    * Apply any limit
    */
   i = find_arg_with_value(ua, NT_("limit"));
   if (i >= 0) {
      POOL_MEM temp(PM_MESSAGE);

      jr->limit = atoi(ua->argv[i]);

      /*
       * When any acl filters are set create a limit filter using the filter framework
       * and increase the database LIMIT clause to 10 * the limit. The filter framework
       * will make sure the end-user will only see the wanted limit.
       */
      if (ua->send->has_acl_filters()) {
         ua->send->add_limit_filter_tuple(jr->limit);
         jr->limit = jr->limit * 10;
      }
      temp.bsprintf(" LIMIT %d", jr->limit);
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
}

static bool do_list_cmd(UAContext *ua, const char *cmd, e_list_type llist)
{
   JOB_DBR jr;
   POOL_DBR pr;
   utime_t now;
   MEDIA_DBR mr;
   int days = 0,
       hours = 0,
       jobstatus = 0;
   bool count,
        last,
        current,
        enabled,
        disabled;
   int i, d, h, jobid;
   time_t schedtime = 0;
   char *clientname = NULL;
   char *volumename = NULL;
   const int secs_in_day = 86400;
   const int secs_in_hour = 3600;
   POOL_MEM query_range(PM_MESSAGE);

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

   current = find_arg(ua, NT_("current")) >= 0;
   enabled = find_arg(ua, NT_("enabled")) >= 0;
   disabled = find_arg(ua, NT_("disabled")) >= 0;
   count = find_arg(ua, NT_("count")) >= 0;
   last = find_arg(ua, NT_("last")) >= 0;

   pm_strcpy(query_range, "");
   set_query_range(query_range, ua, &jr);

   i = find_arg_with_value(ua, NT_("client"));
   if (i >= 0) {
      if (ua->GetClientResWithName(ua->argv[i])) {
         clientname = ua->argv[i];
      } else {
         ua->error_msg(_("invalid client parameter\n"));
         return false;
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

      i = find_arg_with_value(ua, NT_("volume"));
      if (i >= 0) {
         volumename = ua->argv[i];
      }

      switch (llist) {
      case VERT_LIST:
         if (!count) {
            set_acl_filter(ua, 2, Job_ACL); /* JobName */
            set_acl_filter(ua, 7, Client_ACL); /* ClientName */
            set_acl_filter(ua, 21, Pool_ACL); /* PoolName */
            set_acl_filter(ua, 24, FileSet_ACL); /* FilesetName */
         }
         if (current) {
            set_res_filter(ua, 2, R_JOB); /* JobName */
            set_res_filter(ua, 7, R_CLIENT); /* ClientName */
            set_res_filter(ua, 21, R_POOL); /* PoolName */
            set_res_filter(ua, 24, R_FILESET); /* FilesetName */
         }
         if (enabled) {
            set_enabled_filter(ua, 2, R_JOB); /* JobName */
         }
         if (disabled) {
            set_disabled_filter(ua, 2, R_JOB); /* JobName */
         }
         break;
      default:
         if (!count) {
            set_acl_filter(ua, 1, Job_ACL); /* JobName */
            set_acl_filter(ua, 2, Client_ACL); /* ClientName */
         }
         if (current) {
            set_res_filter(ua, 1, R_JOB); /* JobName */
            set_res_filter(ua, 2, R_CLIENT); /* ClientName */
         }
         if (enabled) {
            set_enabled_filter(ua, 1, R_JOB); /* JobName */
         }
         if (disabled) {
            set_disabled_filter(ua, 1, R_JOB); /* JobName */
         }
         break;
      }

      set_query_range(query_range, ua, &jr);

      ua->db->list_job_records(ua->jcr, &jr, query_range.c_str(), clientname,
                               jobstatus, volumename, schedtime, last, count, ua->send, llist);
   } else if (bstrcasecmp(ua->argk[1], NT_("jobtotals"))) {
      /*
       * List JOBTOTALS
       */
      ua->db->list_job_totals(ua->jcr, &jr, ua->send);
   } else if ((bstrcasecmp(ua->argk[1], NT_("jobid")) ||
               bstrcasecmp(ua->argk[1], NT_("ujobid"))) && ua->argv[1]) {
      /*
       * List JOBID=nn
       * List UJOBID=xxx
       */
      if (ua->argv[1]) {
         jobid = get_jobid_from_cmdline(ua);
         if (jobid > 0) {
            jr.JobId = jobid;

            set_acl_filter(ua, 1, Job_ACL); /* JobName */
            set_acl_filter(ua, 2, Client_ACL); /* ClientName */
            if (current) {
               set_res_filter(ua, 1, R_JOB); /* JobName */
               set_res_filter(ua, 2, R_CLIENT); /* ClientName */
            }
            if (enabled) {
               set_enabled_filter(ua, 1, R_JOB); /* JobName */
            }
            if (disabled) {
               set_disabled_filter(ua, 1, R_JOB); /* JobName */
            }

            set_query_range(query_range, ua, &jr);

            ua->db->list_job_records(ua->jcr, &jr, query_range.c_str(), clientname,
                                     jobstatus, volumename, schedtime, last, count, ua->send, llist);
         }
      }
   } else if (bstrcasecmp(ua->argk[1], NT_("basefiles"))) {
      /*
       * List BASEFILES
       */
      jobid = get_jobid_from_cmdline(ua);
      if (jobid > 0) {
         ua->db->list_base_files_for_job(ua->jcr, jobid, ua->send);
      } else {
         ua->error_msg(_("missing parameter: jobid\n"));
      }
   } else if (bstrcasecmp(ua->argk[1], NT_("files"))) {
      /*
       * List FILES
       */
      jobid = get_jobid_from_cmdline(ua);
      if (jobid > 0) {
         ua->db->list_files_for_job(ua->jcr, jobid, ua->send);
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

         set_acl_filter(ua, 1, FileSet_ACL); /* FilesetName */
         if (current) {
            set_res_filter(ua, 1, R_FILESET); /* FilesetName */
         }

         set_query_range(query_range, ua, &jr);

         ua->db->list_filesets(ua->jcr, &jr, query_range.c_str(), ua->send, llist);
      } else {
         ua->error_msg(_("missing parameter: jobid or filesetid\n"));
      }
   } else if (bstrcasecmp(ua->argk[1], NT_("filesets"))) {
      /*
       * List FILESETs
       */
      set_acl_filter(ua, 1, FileSet_ACL); /* FilesetName */
      if (current) {
         set_res_filter(ua, 1, R_FILESET); /* FilesetName */
      }

      set_query_range(query_range, ua, &jr);

      ua->db->list_filesets(ua->jcr, &jr, query_range.c_str(), ua->send, llist);
   } else if (bstrcasecmp(ua->argk[1], NT_("jobmedia"))) {
      /*
       * List JOBMEDIA
       */
      jobid = get_jobid_from_cmdline(ua);
      if (jobid >= 0) {
         ua->db->list_jobmedia_records(ua->jcr, jobid, ua->send, llist);
      } else {
         ua->error_msg(_("missing parameter: jobid\n"));
      }
   } else if (bstrcasecmp(ua->argk[1], NT_("joblog"))) {
      /*
       * List JOBLOG
       */
      jobid = get_jobid_from_cmdline(ua);
      if (jobid >= 0) {
         ua->db->list_joblog_records(ua->jcr, jobid, ua->send, llist);
      } else {
         ua->error_msg(_("missing parameter: jobid\n"));
      }
   } else if (bstrcasecmp(ua->argk[1], NT_("log"))) {
      bool reverse;

      /*
       * List last <limit> LOG entries
       * default is DEFAULT_LOG_LINES entries
       */
      reverse = find_arg(ua, NT_("reverse")) >= 0;

      if (strlen(query_range.c_str()) == 0) {
         Mmsg(query_range, " LIMIT %d", DEFAULT_LOG_LINES);
      }

      if (ua->api != API_MODE_JSON) {
         set_hidden_column(ua, 0); /* LogId */
         set_hidden_column_acl_filter(ua, 1, Job_ACL); /* JobName */
         set_hidden_column_acl_filter(ua, 2, Client_ACL); /* ClientName */
         set_hidden_column(ua, 3); /* LogTime */
      } else {
         set_acl_filter(ua, 1, Job_ACL); /* JobName */
         set_acl_filter(ua, 2, Client_ACL); /* ClientName */
      }

      set_query_range(query_range, ua, &jr);

      ua->db->list_log_records(ua->jcr, clientname, query_range.c_str(), reverse,  ua->send, llist);
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

      set_acl_filter(ua, 1, Pool_ACL); /* PoolName */
      if (current) {
         set_res_filter(ua, 1, R_POOL); /* PoolName */
      }

      set_query_range(query_range, ua, &jr);

      ua->db->list_pool_records(ua->jcr, &pr, ua->send, llist);
   } else if (bstrcasecmp(ua->argk[1], NT_("clients"))) {
      /*
       * List CLIENTS
       */
      set_acl_filter(ua, 1, Client_ACL); /* ClientName */
      if (current) {
         set_res_filter(ua, 1, R_CLIENT); /* ClientName */
      }
      if (enabled) {
         set_enabled_filter(ua, 1, R_CLIENT); /* ClientName */
      }
      if (disabled) {
         set_disabled_filter(ua, 1, R_CLIENT); /* ClientName */
      }

      set_query_range(query_range, ua, &jr);

      ua->db->list_client_records(ua->jcr, NULL, ua->send, llist);
   } else if (bstrcasecmp(ua->argk[1], NT_("client")) && ua->argv[1]) {
      /*
       * List CLIENT=xxx
       */
      set_acl_filter(ua, 1, Client_ACL); /* ClientName */
      if (current) {
         set_res_filter(ua, 1, R_CLIENT); /* ClientName */
      }
      if (enabled) {
         set_enabled_filter(ua, 1, R_CLIENT); /* ClientName */
      }
      if (disabled) {
         set_disabled_filter(ua, 1, R_CLIENT); /* ClientName */
      }

      set_query_range(query_range, ua, &jr);

      ua->db->list_client_records(ua->jcr, ua->argv[1], ua->send, llist);
   } else if (bstrcasecmp(ua->argk[1], NT_("storages"))) {
      /*
       * List STORAGES
       */
      set_acl_filter(ua, 1, Storage_ACL); /* StorageName */
      if (current) {
         set_res_filter(ua, 1, R_STORAGE); /* StorageName */
      }
      if (enabled) {
         set_enabled_filter(ua, 1, R_STORAGE); /* StorageName */
      }
      if (disabled) {
         set_disabled_filter(ua, 1, R_STORAGE); /* StorageName */
      }

      set_query_range(query_range, ua, &jr);

      ua->db->list_sql_query(ua->jcr, "SELECT * FROM Storage", ua->send, llist, "storages");
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
         count = ua->db->get_job_volume_names(ua->jcr, jobid, VolumeName);
         ua->send_msg(_("Jobid %d used %d Volume(s): %s\n"), jobid, count, VolumeName);
         free_pool_memory(VolumeName);
      } else if (jobid == 0) {
         /*
          * List a specific volume?
          */
         if (ua->argv[1]) {
            bstrncpy(mr.VolumeName, ua->argv[1], sizeof(mr.VolumeName));
            ua->send->object_start("volume");
            ua->db->list_media_records(ua->jcr, &mr, ua->send, llist);
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
               ua->db->list_media_records(ua->jcr, &mr, ua->send, llist);
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
                  set_acl_filter(ua, 1, Pool_ACL); /* PoolName */
                  if (current) {
                     set_res_filter(ua, 1, R_POOL); /* PoolName */
                  }
                  ua->db->list_media_records(ua->jcr, &mr, ua->send, llist);
                  ua->send->array_end("volumes");
               } else {
                  /*
                   * List Volumes in all pools
                   */
                  if (!ua->db->get_pool_ids(ua->jcr, &num_pools, &ids)) {
                     ua->error_msg(_("Error obtaining pool ids. ERR=%s\n"), ua->db->strerror());
                     return true;
                  }

                  if (num_pools <= 0) {
                     return true;
                  }

                  ua->send->object_start("volumes");
                  for (i = 0; i < num_pools; i++) {
                     pr.PoolId = ids[i];
                     if (ua->db->get_pool_record(ua->jcr, &pr)) {
                        if (ua->acl_access_ok(Pool_ACL, pr.Name, false)) {
                           ua->send->decoration( "Pool: %s\n", pr.Name );
                           ua->send->array_start(pr.Name);
                           mr.PoolId = ids[i];
                           ua->db->list_media_records(ua->jcr, &mr, ua->send, llist);
                           ua->send->array_end(pr.Name);
                        }
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
            ua->db->list_copies_records(ua->jcr, query_range.c_str(), ua->argv[i], ua->send, llist);
         }
      } else {
         ua->db->list_copies_records(ua->jcr, query_range.c_str(), NULL, ua->send, llist);
      }
   } else if (bstrcasecmp(ua->argk[1], NT_("backups"))) {
      if (parse_list_backups_cmd(ua, query_range.c_str(), llist)) {
         switch (llist) {
         case VERT_LIST:
            set_acl_filter(ua, 2, Job_ACL); /* JobName */
            set_acl_filter(ua, 21, Pool_ACL); /* PoolName */
            if (current) {
               set_res_filter(ua, 2, R_JOB); /* JobName */
               set_res_filter(ua, 21, R_POOL); /* PoolName */
            }
            if (enabled) {
               set_enabled_filter(ua, 2, R_JOB); /* JobName */
            }
            if (disabled) {
               set_disabled_filter(ua, 2, R_JOB); /* JobName */
            }
            break;
         default:
            set_acl_filter(ua, 1, Job_ACL); /* JobName */
            if (current) {
               set_res_filter(ua, 1, R_JOB); /* JobName */
            }
            if (enabled) {
               set_enabled_filter(ua, 1, R_JOB); /* JobName */
            }
            if (disabled) {
               set_disabled_filter(ua, 1, R_JOB); /* JobName */
            }
            break;
         }

         ua->db->list_sql_query(ua->jcr, ua->cmd, ua->send, llist, "backups");
      }
   } else if ( bstrcasecmp(ua->argk[1], NT_("jobstatistics") )
            || bstrcasecmp(ua->argk[1], NT_("jobstats") ) ) {
      jobid = get_jobid_from_cmdline(ua);
      if (jobid > 0) {
         ua->db->list_jobstatistics_records(ua->jcr, jobid, ua->send, llist);
      } else {
         ua->error_msg(_("no jobid given\n"));
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
         if (!ua->acl_access_ok(FileSet_ACL, fs->name(), false)) {
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
      if (!ua->acl_access_ok(FileSet_ACL, ua->argv[fileset], true)) {
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

   if (!ua->acl_access_ok(Client_ACL, ua->argv[client], true)) {
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
      ua->db->fill_query(ua->cmd, B_DB::SQL_QUERY_list_jobs_long, selection.c_str(), criteria.c_str());
   } else {
      ua->db->fill_query(ua->cmd, B_DB::SQL_QUERY_list_jobs, selection.c_str(), criteria.c_str());
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
      job = ua->GetJobResWithName(ua->argv[i]);
      if (!job) {
         Jmsg(ua->jcr, M_ERROR, 0, _("%s is not a job name.\n"), ua->argv[i]);
         if ((job = select_job_resource(ua)) == NULL) {
            return false;
         }
      }
   }

   jcr = new_jcr(sizeof(JCR), dird_free_jcr);
   for (run = NULL; (run = find_next_run(run, job, runtime, ndays)); ) {
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
      if (!ua->db->get_pool_record(jcr, &pr)) {
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

/**
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

/**
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
   while (!jcr->db->get_pool_record(jcr, &pr)) { /* get by Name */
      /* Try to create the pool */
      if (create_pool(jcr, jcr->db, jcr->res.pool, POOL_OP_CREATE) < 0) {
         Jmsg(jcr, M_FATAL, 0, _("Pool %s not in database. %s"), pr.Name, jcr->db->strerror());
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
   if (console_msg_pending &&
       ua->acl_no_restrictions(Command_ACL) &&
       ua->auto_display_messages) {
      do_messages(ua, cmd);
   }
   return true;
}

bool messages_cmd(UAContext *ua, const char *cmd)
{
   if (console_msg_pending &&
       ua->acl_no_restrictions(Command_ACL)) {
      do_messages(ua, cmd);
   } else {
      ua->send->decoration(_("You have no messages.\n"));
   }
   return true;
}

/**
 * Callback routine for "filtering" database listing.
 */
of_filter_state filterit(void *ctx, void *data, of_filter_tuple *tuple)
{
   char **row = (char **)data;
   UAContext *ua = (UAContext *)ctx;
   of_filter_state retval = OF_FILTER_STATE_SHOW;

   switch (tuple->type) {
   case OF_FILTER_LIMIT:
      if (tuple->u.limit_filter.limit > 0) {
         Dmsg1(200, "filterit: limit filter still %d entries to display\n",
               tuple->u.limit_filter.limit);
         tuple->u.limit_filter.limit--;
      } else {
         Dmsg0(200, "filterit: limit filter reached don't display entry\n");
         retval = OF_FILTER_STATE_SUPPRESS;
      }
      goto bail_out;
   case OF_FILTER_ACL:
      if (!row[tuple->u.acl_filter.column] ||
          strlen(row[tuple->u.acl_filter.column]) == 0) {
         retval = OF_FILTER_STATE_UNKNOWN;
      } else {
         if (!ua->acl_access_ok(tuple->u.acl_filter.acltype,
                                row[tuple->u.acl_filter.column], false)) {
            Dmsg2(200, "filterit: Filter on acl_type %d value %s, suppress output\n",
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
         if (!GetResWithName(tuple->u.res_filter.restype,
                             row[tuple->u.res_filter.column], false)) {
            Dmsg2(200, "filterit: Filter on resource_type %d value %s, suppress output\n",
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

      if (tuple->type == OF_FILTER_DISABLED) {
         enabled = false;
      }

      switch (tuple->u.res_filter.restype) {
      case R_CLIENT: {
         CLIENTRES *client;

         client = ua->GetClientResWithName(row[tuple->u.res_filter.column], false, false);
         if (!client || client->enabled != enabled) {
            Dmsg2(200, "filterit: Filter on Client, %s is not %sabled\n",
                  row[tuple->u.res_filter.column], (enabled) ? "En" : "Dis");
            retval = OF_FILTER_STATE_SUPPRESS;
         }
         goto bail_out;
      }
      case R_JOB: {
         JOBRES *job;

         job = ua->GetJobResWithName(row[tuple->u.res_filter.column], false, false);
         if (!job || job->enabled != enabled) {
            Dmsg2(200, "filterit: Filter on Job, %s is not %sabled\n",
                  row[tuple->u.res_filter.column], (enabled) ? "En" : "Dis");
            retval = OF_FILTER_STATE_SUPPRESS;
         }
         goto bail_out;
      }
      case R_STORAGE: {
         STORERES *store;

         store = ua->GetStoreResWithName(row[tuple->u.res_filter.column], false, false);
         if (!store || store->enabled != enabled) {
            Dmsg2(200, "filterit: Filter on Storage, %s is not %sabled\n",
                  row[tuple->u.res_filter.column], (enabled) ? "En" : "Dis");
            retval = OF_FILTER_STATE_SUPPRESS;
         }
         goto bail_out;
      }
      case R_SCHEDULE: {
         SCHEDRES *schedule;

         schedule = ua->GetScheduleResWithName(row[tuple->u.res_filter.column], false, false);
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

/**
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
/**
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

/**
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

/**
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

/**
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


void UAContext::send_cmd_usage(const char *fmt, ...)
{
   va_list arg_ptr;
   POOL_MEM message;
   POOL_MEM usage;

   /* send current buffer */
   send->send_buffer();

   va_start(arg_ptr, fmt);
   message.bvsprintf(fmt, arg_ptr);
   va_end(arg_ptr);

   if (cmddef) {
      if (cmddef->key && cmddef->usage) {
         usage.bsprintf("\nUSAGE: %s %s\n", cmddef->key, cmddef->usage);
         message.strcat(usage);
      }
   }

   send->message(NULL, message);
}

