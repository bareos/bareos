/* BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2001-2012 Free Software Foundation Europe e.V.
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
 * Kern Sibbald, December MMI
 */
/**
 * @file
 * BAREOS Director -- Run Command
 */
#include "bareos.h"
#include "dird.h"

/* Forward referenced subroutines */
static void select_job_level(UaContext *ua, JobControlRecord *jcr);
static bool display_job_parameters(UaContext *ua, JobControlRecord *jcr, RunContext &rc);
static void select_where_regexp(UaContext *ua, JobControlRecord *jcr);
static bool scan_command_line_arguments(UaContext *ua, RunContext &rc);
static bool reset_restore_context(UaContext *ua, JobControlRecord *jcr, RunContext &rc);
static int modify_job_parameters(UaContext *ua, JobControlRecord *jcr, RunContext &rc);

/* Imported variables */
extern struct s_kw ReplaceOptions[];

/**
 * Rerun a job by jobid. Lookup the job data and rerun the job with that data.
 *
 * Returns: false on error
 *          true if OK
 */
static inline bool rerun_job(UaContext *ua, JobId_t JobId, bool yes, utime_t now)
{
   JobDbRecord jr;
   char dt[MAX_TIME_LENGTH];
   PoolMem cmdline(PM_MESSAGE);

   memset(&jr, 0, sizeof(jr));
   jr.JobId = JobId;
   ua->send_msg("rerunning jobid %d\n", jr.JobId);
   if (!ua->db->get_job_record(ua->jcr, &jr)) {
      Jmsg(ua->jcr, M_WARNING, 0, _("Error getting Job record for Job rerun: ERR=%s"), ua->db->strerror());
      goto bail_out;
   }

   /*
    * Only perform rerun on JobTypes where it makes sense.
    */
   switch (jr.JobType) {
   case JT_BACKUP:
   case JT_COPY:
   case JT_MIGRATE:
      break;
   default:
      return true;
   }

   if (jr.JobLevel == L_NONE) {
      Mmsg(cmdline, "run job=\"%s\"", jr.Name);
   } else {
      Mmsg(cmdline, "run job=\"%s\" level=\"%s\"", jr.Name, level_to_str(jr.JobLevel));
   }
   pm_strcpy(ua->cmd, cmdline);

   if (jr.ClientId) {
      ClientDbRecord cr;

      memset(&cr, 0, sizeof(cr));
      cr.ClientId = jr.ClientId;
      if (!ua->db->get_client_record(ua->jcr, &cr)) {
         Jmsg(ua->jcr, M_WARNING, 0, _("Error getting Client record for Job rerun: ERR=%s"), ua->db->strerror());
         goto bail_out;
      }
      Mmsg(cmdline, " client=\"%s\"", cr.Name);
      pm_strcat(ua->cmd, cmdline);
   }

   if (jr.PoolId) {
      PoolDbRecord pr;

      memset(&pr, 0, sizeof(pr));
      pr.PoolId = jr.PoolId;
      if (!ua->db->get_pool_record(ua->jcr, &pr)) {
         Jmsg(ua->jcr, M_WARNING, 0, _("Error getting Pool record for Job rerun: ERR=%s"), ua->db->strerror());
         goto bail_out;
      }

      /*
       * Source pool.
       */
      switch (jr.JobType) {
      case JT_COPY:
      case JT_MIGRATE: {
            JobResource *job = NULL;

            job = ua->GetJobResWithName(jr.Name, false);
            if (job) {
               Mmsg(cmdline, " pool=\"%s\"", job->pool->name());
               pm_strcat(ua->cmd, cmdline);
            }
         break;
      }
      case JT_BACKUP:
         switch (jr.JobLevel) {
         case L_VIRTUAL_FULL: {
            JobResource *job = NULL;

            job = ua->GetJobResWithName(jr.Name, false);
            if (job) {
               Mmsg(cmdline, " pool=\"%s\"", job->pool->name());
               pm_strcat(ua->cmd, cmdline);
            }
            break;
         }
         default:
            Mmsg(cmdline, " pool=\"%s\"", pr.Name);
            pm_strcat(ua->cmd, cmdline);
            break;
         }
      }

      /*
       * Next pool.
       */
      switch (jr.JobType) {
      case JT_COPY:
      case JT_MIGRATE:
         Mmsg(cmdline, " nextpool=\"%s\"", pr.Name);
         pm_strcat(ua->cmd, cmdline);
         break;
      case JT_BACKUP:
         switch (jr.JobLevel) {
         case L_VIRTUAL_FULL:
            Mmsg(cmdline, " nextpool=\"%s\"", pr.Name);
            pm_strcat(ua->cmd, cmdline);

            break;
         default:
            break;
         }
         break;
      }
   }

   if (jr.FileSetId) {
      FileSetDbRecord fs;

      memset(&fs, 0, sizeof(fs));
      fs.FileSetId = jr.FileSetId;
      if (!ua->db->get_fileset_record(ua->jcr, &fs)) {
         Jmsg(ua->jcr, M_WARNING, 0, _("Error getting FileSet record for Job rerun: ERR=%s"), ua->db->strerror());
         goto bail_out;
      }
      Mmsg(cmdline, " fileset=\"%s\"", fs.FileSet);
      pm_strcat(ua->cmd, cmdline);
   }

   bstrutime(dt, sizeof(dt), now);
   Mmsg(cmdline, " when=\"%s\"", dt);
   pm_strcat(ua->cmd, cmdline);

   if (yes) {
      pm_strcat(ua->cmd, " yes");
   }

   Dmsg1(100, "rerun cmdline=%s\n", ua->cmd);

   parse_ua_args(ua);
   return run_cmd(ua, ua->cmd);

bail_out:
   return false;
}

/**
 * Rerun a job selection.
 *
 * Returns: 0 on error
 *          1 if OK
 */
bool rerun_cmd(UaContext *ua, const char *cmd)
{
   int i, j, d, h, s, u;
   int days = 0;
   int hours = 0;
   int since_jobid = 0;
   int until_jobid = 0;
   JobId_t JobId;
   dbid_list ids;
   PoolMem query(PM_MESSAGE);
   utime_t now;
   time_t schedtime;
   char dt[MAX_TIME_LENGTH];
   char ed1[50];
   char ed2[50];
   bool yes = false;                 /* Was "yes" given on cmdline*/
   bool timeframe = false;           /* Should the selection happen based on timeframe? */
   bool since_jobid_given = false;   /* Was since_jobid given? */
   bool until_jobid_given = false;   /* Was until_jobid given? */
   const int secs_in_day = 86400;
   const int secs_in_hour = 3600;

   if (!open_client_db(ua)) {
      return true;
   }

   now = (utime_t)time(NULL);

   /*
    * Determine what cmdline arguments are given.
    */
   j = find_arg_with_value(ua, NT_("jobid"));
   d = find_arg_with_value(ua, NT_("days"));
   h = find_arg_with_value(ua, NT_("hours"));
   s = find_arg_with_value(ua, NT_("since_jobid"));
   u = find_arg_with_value(ua, NT_("until_jobid"));

   if (s > 0) {
      since_jobid = str_to_int64(ua->argv[s]);
      since_jobid_given = true;
   }

   if (u > 0) {
      until_jobid = str_to_int64(ua->argv[u]);
      until_jobid_given = true;
   }

   if (d > 0 || h > 0) {
      timeframe = true;
   }

   if (find_arg(ua, NT_("yes")) > 0) {
      yes = true;
   }

   if (j < 0 && !timeframe && !since_jobid_given) {
      ua->send_msg("Please specify jobid, since_jobid, hours or days\n");
      goto bail_out;
   }

   if (j >= 0 && since_jobid_given) {
      ua->send_msg("Please specify either jobid or since_jobid (and optionally until_jobid)\n");
      goto bail_out;
   }

   if (j >= 0 && timeframe) {
      ua->send_msg("Please specify either jobid or timeframe\n");
      goto bail_out;
   }

   if (timeframe || since_jobid_given) {
      schedtime = now;
      if (d > 0) {
         days = str_to_int64(ua->argv[d]);
         schedtime = now - secs_in_day * days;   /* Days in the past */
      }
      if (h > 0) {
         hours = str_to_int64(ua->argv[h]);
         schedtime = now - secs_in_hour * hours; /* Hours in the past */
      }

      /*
       * Job Query Start
       */
      bstrutime(dt, sizeof(dt), schedtime);

      if (since_jobid_given) {
         if (until_jobid_given) {
            Mmsg(query, "SELECT JobId FROM Job WHERE JobStatus = 'f' AND JobId >= %s AND JobId <= %s ORDER BY JobId",
                 edit_int64(since_jobid, ed1), edit_int64(until_jobid, ed2));
         } else {
            Mmsg(query, "SELECT JobId FROM Job WHERE JobStatus = 'f' AND JobId >= %s ORDER BY JobId",
                 edit_int64(since_jobid, ed1));
         }

      } else {
         Mmsg(query, "SELECT JobId FROM Job WHERE JobStatus = 'f' AND SchedTime > '%s' ORDER BY JobId", dt);
      }

      ua->db->get_query_dbids(ua->jcr, query, ids);

      ua->send_msg("The following ids were selected for rerun:\n");
      for (i = 0; i < ids.num_ids; i++) {
         if (i > 0) {
            ua->send_msg(",%d", ids.DBId[i]);
         } else {
            ua->send_msg("%d", ids.DBId[i]);
         }
      }
      ua->send_msg("\n");

      if (!yes &&
          (!get_yesno(ua, _("rerun these jobids? (yes/no): ")) || !ua->pint32_val)) {
         goto bail_out;
      }
      /*
       * Job Query End
       */

      /*
       * Loop over all selected JobIds.
       */
      for (i = 0; i < ids.num_ids; i++) {
         JobId = ids.DBId[i];
         if (!rerun_job(ua, JobId, yes, now)) {
            goto bail_out;
         }
      }
   } else {
      JobId = str_to_int64(ua->argv[j]);
      if (!rerun_job(ua, JobId, yes, now)) {
         goto bail_out;
      }
   }

   return true;

bail_out:
   return false;
}

/**
 * For Backup and Verify Jobs
 *     run [job=]<job-name> level=<level-name>
 *
 * For Restore Jobs
 *     run <job-name>
 *
 * Returns: 0 on error
 *          JobId if OK
 *
 */
int do_run_cmd(UaContext *ua, const char *cmd)
{
   JobControlRecord *jcr = NULL;
   RunContext rc;
   int status, length;
   bool valid_response;
   bool do_pool_overrides = true;

   if (!open_client_db(ua)) {
      return 0;
   }

   if (!scan_command_line_arguments(ua, rc)) {
      return 0;
   }

   if (find_arg(ua, NT_("fdcalled")) > 0) {
      jcr->file_bsock = ua->UA_sock->clone();
      ua->quit = true;
   }

   /*
    * Create JobControlRecord to run job.  NOTE!!! after this point, free_jcr() before returning.
    */
   if (!jcr) {
      jcr = new_jcr(sizeof(JobControlRecord), dird_free_jcr);
      set_jcr_defaults(jcr, rc.job);
      jcr->unlink_bsr = ua->jcr->unlink_bsr;    /* copy unlink flag from caller */
      ua->jcr->unlink_bsr = false;
   }

   /*
    * Transfer JobIds to new restore Job
    */
   if (ua->jcr->JobIds) {
      jcr->JobIds = ua->jcr->JobIds;
      ua->jcr->JobIds = NULL;
   }

   /*
    * Transfer selected restore tree to new restore Job
    */
   if (ua->jcr->restore_tree_root) {
      jcr->restore_tree_root = ua->jcr->restore_tree_root;
      ua->jcr->restore_tree_root = NULL;
   }

try_again:
   if (!reset_restore_context(ua, jcr, rc)) {
      goto bail_out;
   }

   /*
    * Run without prompting?
    */
   if (ua->batch || find_arg(ua, NT_("yes")) > 0) {
      goto start_job;
   }

   /*
    * When doing interactive runs perform the pool level overrides
    * early this way the user doesn't get nasty surprisses when
    * a level override changes the pool the data will be saved to
    * later. We only want to do these overrides once so we use
    * a tracking boolean do_pool_overrides to see if we still
    * need to do this (e.g. we pass by here multiple times when
    * the user interactivly changes options.
    */
   if (do_pool_overrides) {
      switch (jcr->getJobType()) {
      case JT_BACKUP:
         if (!jcr->is_JobLevel(L_VIRTUAL_FULL)) {
            apply_pool_overrides(jcr);
         }
         break;
      default:
         break;
      }
      do_pool_overrides = false;
   }

   /*
    * Prompt User to see if all run job parameters are correct, and
    * allow him to modify them.
    */
   if (!display_job_parameters(ua, jcr, rc)) {
      goto bail_out;
   }

   /*
    * Prompt User until we have a valid response.
    */
   do {
      if (!get_cmd(ua, _("OK to run? (yes/mod/no): "))) {
         goto bail_out;
      }

      /*
       * Empty line equals yes, anything other we compare
       * the cmdline for the length of the given input unless
       * its mod or .mod where we compare only the keyword
       * and a space as it can be followed by a full cmdline
       * with new cmdline arguments that need to be parsed.
       */
      valid_response = false;
      length = strlen(ua->cmd);
      if (ua->cmd[0] == 0 ||
          bstrncasecmp(ua->cmd, ".mod ", MIN(length, 5)) ||
          bstrncasecmp(ua->cmd, "mod ", MIN(length, 4)) ||
          bstrncasecmp(ua->cmd, NT_("yes"), length) ||
          bstrncasecmp(ua->cmd, _("yes"), length) ||
          bstrncasecmp(ua->cmd, NT_("no"), length) ||
          bstrncasecmp(ua->cmd, _("no"), length)) {
         valid_response = true;
      }

      if (!valid_response) {
         ua->warning_msg(_("Illegal response %s\n"), ua->cmd);
      }
   } while (!valid_response);

   /*
    * See if the .mod or mod has arguments.
    */
   if (bstrncasecmp(ua->cmd, ".mod ", 5) ||
      (bstrncasecmp(ua->cmd, "mod ", 4) && strlen(ua->cmd) > 6)) {
      parse_ua_args(ua);
      rc.mod = true;
      if (!scan_command_line_arguments(ua, rc)) {
         return 0;
      }
      goto try_again;
   }

   /*
    * Allow the user to modify the settings
    */
   status = modify_job_parameters(ua, jcr, rc);
   switch (status) {
   case 0:
      goto try_again;
   case 1:
      break;
   case -1:
      goto bail_out;
   }

   /*
    * For interactive runs we set IgnoreLevelPoolOverrides as we already
    * performed the actual overrrides.
    */
   jcr->IgnoreLevelPoolOverides = true;

   if (ua->cmd[0] == 0 ||
       bstrncasecmp(ua->cmd, NT_("yes"), strlen(ua->cmd)) ||
       bstrncasecmp(ua->cmd, _("yes"), strlen(ua->cmd))) {
      JobId_t JobId;
      Dmsg1(800, "Calling run_job job=%x\n", jcr->res.job);

start_job:
      Dmsg3(100, "JobId=%u using pool %s priority=%d\n", (int)jcr->JobId,
            jcr->res.pool->name(), jcr->JobPriority);
      Dmsg1(900, "Running a job; its spool_data = %d\n", jcr->spool_data);

      JobId = run_job(jcr);

      Dmsg4(100, "JobId=%u NewJobId=%d using pool %s priority=%d\n", (int)jcr->JobId,
            JobId, jcr->res.pool->name(), jcr->JobPriority);

      free_jcr(jcr);                  /* release jcr */

      if (JobId == 0) {
         ua->error_msg(_("Job failed.\n"));
      } else {
         char ed1[50];
         ua->send->object_start("run");
         ua->send->object_key_value("jobid", edit_int64(JobId, ed1), _("Job queued. JobId=%s\n"));
         ua->send->object_end("run");
      }

      return JobId;
   }

bail_out:
   ua->send_msg(_("Job not run.\n"));
   free_jcr(jcr);

   return 0;                       /* do not run */
}

bool run_cmd(UaContext *ua, const char *cmd)
{
   return (do_run_cmd(ua, ua->cmd) != 0);
}

int modify_job_parameters(UaContext *ua, JobControlRecord *jcr, RunContext &rc)
{
   int opt;

   /*
    * At user request modify parameters of job to be run.
    */
   if (ua->cmd[0] != 0 && bstrncasecmp(ua->cmd, _("mod"), strlen(ua->cmd))) {
      start_prompt(ua, _("Parameters to modify:\n"));

      add_prompt(ua, _("Level"));                   /* 0 */
      add_prompt(ua, _("Storage"));                 /* 1 */
      add_prompt(ua, _("Job"));                     /* 2 */
      add_prompt(ua, _("FileSet"));                 /* 3 */
      if (jcr->is_JobType(JT_RESTORE)) {
         add_prompt(ua, _("Restore Client"));       /* 4 */
      } else {
         add_prompt(ua, _("Client"));               /* 4 */
      }
      add_prompt(ua, _("Backup Format"));           /* 5 */
      add_prompt(ua, _("When"));                    /* 6 */
      add_prompt(ua, _("Priority"));                /* 7 */
      if (jcr->is_JobType(JT_BACKUP) ||
          jcr->is_JobType(JT_COPY) ||
          jcr->is_JobType(JT_MIGRATE) ||
          jcr->is_JobType(JT_VERIFY)) {
         add_prompt(ua, _("Pool"));                 /* 8 */
         if (jcr->is_JobType(JT_MIGRATE) ||
             jcr->is_JobType(JT_COPY) ||
            (jcr->is_JobType(JT_BACKUP) &&
             jcr->is_JobLevel(L_VIRTUAL_FULL))) { /* NextPool */
            add_prompt(ua, _("NextPool"));          /* 9 */
            if (jcr->is_JobType(JT_BACKUP)) {
               add_prompt(ua, _("Plugin Options")); /* 10 */
            }
         } else if (jcr->is_JobType(JT_VERIFY)) {
            add_prompt(ua, _("Verify Job"));        /* 9 */
         } else if (jcr->is_JobType(JT_BACKUP)) {
            add_prompt(ua, _("Plugin Options"));    /* 9 */
         }
      } else if (jcr->is_JobType(JT_RESTORE)) {
         add_prompt(ua, _("Bootstrap"));            /* 8 */
         add_prompt(ua, _("Where"));                /* 9 */
         add_prompt(ua, _("File Relocation"));      /* 10 */
         add_prompt(ua, _("Replace"));              /* 11 */
         add_prompt(ua, _("JobId"));                /* 12 */
         add_prompt(ua, _("Plugin Options"));       /* 13 */
      }

      switch (do_prompt(ua, "", _("Select parameter to modify"), NULL, 0)) {
      case 0:
         /* Level */
         select_job_level(ua, jcr);
         switch (jcr->getJobType()) {
         case JT_BACKUP:
            if (!rc.pool_override && !jcr->is_JobLevel(L_VIRTUAL_FULL)) {
               apply_pool_overrides(jcr, true);
               rc.pool = jcr->res.pool;
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
            pm_strcpy(rc.store->store_source, _("user selection"));
            set_rwstorage(jcr, rc.store);
            goto try_again;
         }
         break;
      case 2:
         /* Job */
         rc.job = select_job_resource(ua);
         if (rc.job) {
            jcr->res.job = rc.job;
            set_jcr_defaults(jcr, rc.job);
            goto try_again;
         }
         break;
      case 3:
         /* FileSet */
         rc.fileset = select_fileset_resource(ua);
         if (rc.fileset) {
            jcr->res.fileset = rc.fileset;
            goto try_again;
         }
         break;
      case 4:
         /* Client */
         rc.client = select_client_resource(ua);
         if (rc.client) {
            jcr->res.client = rc.client;
            goto try_again;
         }
         break;
      case 5:
         /* Backup Format */
         if (get_cmd(ua, _("Please enter Backup Format: "))) {
            if (jcr->backup_format) {
               free(jcr->backup_format);
               jcr->backup_format = NULL;
            }
            jcr->backup_format = bstrdup(ua->cmd);
            goto try_again;
         }
         break;
      case 6:
         /* When */
         if (get_cmd(ua, _("Please enter desired start time as YYYY-MM-DD HH:MM:SS (return for now): "))) {
            if (ua->cmd[0] == 0) {
               jcr->sched_time = time(NULL);
            } else {
               jcr->sched_time = str_to_utime(ua->cmd);
               if (jcr->sched_time == 0) {
                  ua->send_msg(_("Invalid time, using current time.\n"));
                  jcr->sched_time = time(NULL);
               }
            }
            goto try_again;
         }
         break;
      case 7:
         /* Priority */
         if (get_pint(ua, _("Enter new Priority: "))) {
            if (!ua->pint32_val) {
               ua->send_msg(_("Priority must be a positive integer.\n"));
            } else {
               jcr->JobPriority = ua->pint32_val;
            }
            goto try_again;
         }
         break;
      case 8:
         /* Pool or Bootstrap depending on JobType */
         if (jcr->is_JobType(JT_BACKUP) ||
             jcr->is_JobType(JT_COPY) ||
             jcr->is_JobType(JT_MIGRATE) ||
             jcr->is_JobType(JT_VERIFY)) {      /* Pool */
            rc.pool = select_pool_resource(ua);
            if (rc.pool) {
               jcr->res.pool = rc.pool;
               rc.level_override = false;
               rc.pool_override = true;
               Dmsg1(100, "Set new pool=%s\n", jcr->res.pool->name());
               goto try_again;
            }
         } else {
            /* Bootstrap */
            if (get_cmd(ua, _("Please enter the Bootstrap file name: "))) {
               if (jcr->RestoreBootstrap) {
                  free(jcr->RestoreBootstrap);
                  jcr->RestoreBootstrap = NULL;
               }
               if (ua->cmd[0] != 0) {
                  FILE *fd;

                  jcr->RestoreBootstrap = bstrdup(ua->cmd);
                  fd = fopen(jcr->RestoreBootstrap, "rb");
                  if (!fd) {
                     berrno be;
                     ua->send_msg(_("Warning cannot open %s: ERR=%s\n"),
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
         if (jcr->is_JobType(JT_MIGRATE) ||
             jcr->is_JobType(JT_COPY) ||
            (jcr->is_JobType(JT_BACKUP) && jcr->is_JobLevel(L_VIRTUAL_FULL))) { /* NextPool */
            rc.next_pool = select_pool_resource(ua);
            if (rc.next_pool) {
               jcr->res.next_pool = rc.next_pool;
               Dmsg1(100, "Set new next_pool=%s\n", jcr->res.next_pool->name());
               goto try_again;
            }
         } else if (jcr->is_JobType(JT_VERIFY)) { /* Verify Job */
            rc.verify_job = select_job_resource(ua);
            if (rc.verify_job) {
              jcr->res.verify_job = rc.verify_job;
            }
            goto try_again;
         } else if (jcr->is_JobType(JT_RESTORE)) { /* Where */
            if (get_cmd(ua, _("Please enter the full path prefix for restore (/ for none): "))) {
               if (jcr->RegexWhere) { /* cannot use regexwhere and where */
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
               jcr->where = bstrdup(ua->cmd);
               goto try_again;
            }
         } else { /* Plugin Options */
            if (get_cmd(ua, _("Please enter Plugin Options string: "))) {
               if (jcr->plugin_options) {
                  free(jcr->plugin_options);
                  jcr->plugin_options = NULL;
               }
               jcr->plugin_options = bstrdup(ua->cmd);
               goto try_again;
            }
         }
         break;
      case 10:
         /* File relocation/Plugin Options depending on JobType */
         if (jcr->is_JobType(JT_RESTORE)) {
            select_where_regexp(ua, jcr);
            goto try_again;
         } else if (jcr->is_JobType(JT_BACKUP)) {
            if (get_cmd(ua, _("Please enter Plugin Options string: "))) {
               if (jcr->plugin_options) {
                  free(jcr->plugin_options);
                  jcr->plugin_options = NULL;
               }
               jcr->plugin_options = bstrdup(ua->cmd);
               goto try_again;
            }
         }
         break;
      case 11:
         /* Replace */
         start_prompt(ua, _("Replace:\n"));
         for (int i = 0; ReplaceOptions[i].name; i++) {
            add_prompt(ua, ReplaceOptions[i].name);
         }
         opt = do_prompt(ua, "", _("Select replace option"), NULL, 0);
         if (opt >=  0) {
            rc.replace = ReplaceOptions[opt].name;
            jcr->replace = ReplaceOptions[opt].token;
         }
         goto try_again;
      case 12:
         /* JobId */
         rc.jid = NULL; /* force reprompt */
         jcr->RestoreJobId = 0;
         if (jcr->RestoreBootstrap) {
            ua->send_msg(_("You must set the bootstrap file to NULL to be able to specify a JobId.\n"));
         }
         goto try_again;
      case 13:
         /* Plugin Options */
         if (get_cmd(ua, _("Please enter Plugin Options string: "))) {
            if (jcr->plugin_options) {
               free(jcr->plugin_options);
               jcr->plugin_options = NULL;
            }
            jcr->plugin_options = bstrdup(ua->cmd);
            goto try_again;
         }
         break;
      case -1: /* error or cancel */
         goto bail_out;
      default:
         goto try_again;
      }
      goto bail_out;
   }
   return 1;

bail_out:
   return -1;

try_again:
   return 0;
}

/**
 * Reset the restore context.
 * This subroutine can be called multiple times, so it must keep any prior settings.
 */
static bool reset_restore_context(UaContext *ua, JobControlRecord *jcr, RunContext &rc)
{
   jcr->res.verify_job = rc.verify_job;
   jcr->res.previous_job = rc.previous_job;
   jcr->res.pool = rc.pool;
   jcr->res.next_pool = rc.next_pool;

   /*
    * See if an explicit pool override was performed.
    * If so set the pool_source to command line and
    * set the IgnoreLevelPoolOverides so any level Pool
    * overrides are ignored.
    */
   if (rc.pool_name) {
      pm_strcpy(jcr->res.pool_source, _("command line"));
      jcr->IgnoreLevelPoolOverides = true;
   } else if (!rc.level_override && jcr->res.pool != jcr->res.job->pool) {
      pm_strcpy(jcr->res.pool_source, _("user input"));
   }
   set_rwstorage(jcr, rc.store);

   if (rc.next_pool_name) {
      pm_strcpy(jcr->res.npool_source, _("command line"));
      jcr->res.run_next_pool_override = true;
   } else if (jcr->res.next_pool != jcr->res.pool->NextPool) {
      pm_strcpy(jcr->res.npool_source, _("user input"));
      jcr->res.run_next_pool_override = true;
   }

   jcr->res.client = rc.client;
   if (jcr->res.client) {
      pm_strcpy(jcr->client_name, rc.client->name());
   }
   jcr->res.fileset = rc.fileset;
   jcr->ExpectedFiles = rc.files;

   if (rc.catalog) {
      jcr->res.catalog = rc.catalog;
      pm_strcpy(jcr->res.catalog_source, _("user input"));
   }

   pm_strcpy(jcr->comment, rc.comment);

   if (rc.where) {
      if (jcr->where) {
         free(jcr->where);
      }
      jcr->where = bstrdup(rc.where);
      rc.where = NULL;
   }

   if (rc.regexwhere) {
      if (jcr->RegexWhere) {
         free(jcr->RegexWhere);
      }
      jcr->RegexWhere = bstrdup(rc.regexwhere);
      rc.regexwhere = NULL;
   }

   if (rc.when) {
      jcr->sched_time = str_to_utime(rc.when);
      if (jcr->sched_time == 0) {
         ua->send_msg(_("Invalid time, using current time.\n"));
         jcr->sched_time = time(NULL);
      }
      rc.when = NULL;
   }

   if (rc.bootstrap) {
      if (jcr->RestoreBootstrap) {
         free(jcr->RestoreBootstrap);
      }
      jcr->RestoreBootstrap = bstrdup(rc.bootstrap);
      rc.bootstrap = NULL;
   }

   if (rc.plugin_options) {
      if (jcr->plugin_options) {
         free(jcr->plugin_options);
      }
      jcr->plugin_options = bstrdup(rc.plugin_options);
      rc.plugin_options = NULL;
   }

   if (rc.replace) {
      jcr->replace = 0;
      for (int i = 0; ReplaceOptions[i].name; i++) {
         if (bstrcasecmp(rc.replace, ReplaceOptions[i].name)) {
            jcr->replace = ReplaceOptions[i].token;
         }
      }
      if (!jcr->replace) {
         ua->send_msg(_("Invalid replace option: %s\n"), rc.replace);
         return false;
      }
   } else if (rc.job->replace) {
      jcr->replace = rc.job->replace;
   } else {
      jcr->replace = REPLACE_ALWAYS;
   }
   rc.replace = NULL;

   if (rc.Priority) {
      jcr->JobPriority = rc.Priority;
      rc.Priority = 0;
   }

   if (rc.since) {
      if (!jcr->stime) {
         jcr->stime = get_pool_memory(PM_MESSAGE);
      }
      pm_strcpy(jcr->stime, rc.since);
      rc.since = NULL;
   }

   if (rc.cloned) {
      jcr->cloned = rc.cloned;
      rc.cloned = false;
   }

   /*
    * If pool changed, update migration write storage
    */
   if (jcr->is_JobType(JT_MIGRATE) ||
       jcr->is_JobType(JT_COPY) ||
      (jcr->is_JobType(JT_BACKUP) && jcr->is_JobLevel(L_VIRTUAL_FULL))) {
      if (!set_migration_wstorage(jcr, rc.pool, rc.next_pool,
                                  _("Storage from Run NextPool override"))) {
         return false;
      }
   }
   rc.replace = ReplaceOptions[0].name;
   for (int i = 0; ReplaceOptions[i].name; i++) {
      if (ReplaceOptions[i].token == jcr->replace) {
         rc.replace = ReplaceOptions[i].name;
      }
   }

   if (rc.level_name) {
      if (!get_level_from_name(jcr, rc.level_name)) {
         ua->send_msg(_("Level \"%s\" not valid.\n"), rc.level_name);
         return false;
      }
      rc.level_name = NULL;
   }

   if (rc.jid) {
      if (jcr->is_JobType(JT_BACKUP) && jcr->is_JobLevel(L_VIRTUAL_FULL)) {
         if (!jcr->vf_jobids) {
            jcr->vf_jobids = get_pool_memory(PM_MESSAGE);
         }
         pm_strcpy(jcr->vf_jobids, rc.jid);
      } else {
         /*
          * Note, this is also MigrateJobId and a VerifyJobId
          */
         jcr->RestoreJobId = str_to_int64(rc.jid);
      }
      rc.jid = NULL;
   }

   if (rc.backup_format) {
      if (jcr->backup_format) {
         free(jcr->backup_format);
      }
      jcr->backup_format = bstrdup(rc.backup_format);
      rc.backup_format = NULL;
   }

   /*
    * Some options are not available through the menu
    * TODO: Add an advanced menu?
    */
   if (rc.spool_data_set) {
      jcr->spool_data = rc.spool_data;
   }

   if (rc.accurate_set) {
      jcr->accurate = rc.accurate;
   }

   /*
    * Used by migration jobs that can have the same name,
    * but can run at the same time
    */
   if (rc.ignoreduplicatecheck_set) {
      jcr->IgnoreDuplicateJobChecking = rc.ignoreduplicatecheck;
   }

   return true;
}

static void select_where_regexp(UaContext *ua, JobControlRecord *jcr)
{
   alist *regs;
   char *strip_prefix, *add_prefix, *add_suffix, *rwhere;
   strip_prefix = add_suffix = rwhere = add_prefix = NULL;

try_again_reg:
   ua->send_msg(_("strip_prefix=%s add_prefix=%s add_suffix=%s\n"),
                NPRT(strip_prefix), NPRT(add_prefix), NPRT(add_suffix));

   start_prompt(ua, _("This will replace your current Where value\n"));
   add_prompt(ua, _("Strip prefix"));                /* 0 */
   add_prompt(ua, _("Add prefix"));                  /* 1 */
   add_prompt(ua, _("Add file suffix"));             /* 2 */
   add_prompt(ua, _("Enter a regexp"));              /* 3 */
   add_prompt(ua, _("Test filename manipulation"));  /* 4 */
   add_prompt(ua, _("Use this ?"));                  /* 5 */

   switch (do_prompt(ua, "", _("Select parameter to modify"), NULL, 0)) {
   case 0:
      /* Strip prefix */
      if (get_cmd(ua, _("Please enter the path prefix to strip: "))) {
         if (strip_prefix) bfree(strip_prefix);
         strip_prefix = bstrdup(ua->cmd);
      }
      goto try_again_reg;
   case 1:
      /* Add prefix */
      if (get_cmd(ua, _("Please enter the path prefix to add (/ for none): "))) {
         if (IsPathSeparator(ua->cmd[0]) && ua->cmd[1] == '\0') {
            ua->cmd[0] = 0;
         }

         if (add_prefix) bfree(add_prefix);
         add_prefix = bstrdup(ua->cmd);
      }
      goto try_again_reg;
   case 2:
      /* Add suffix */
      if (get_cmd(ua, _("Please enter the file suffix to add: "))) {
         if (add_suffix) bfree(add_suffix);
         add_suffix = bstrdup(ua->cmd);
      }
      goto try_again_reg;
   case 3:
      /* Add rwhere */
      if (get_cmd(ua, _("Please enter a valid regexp (!from!to!): "))) {
         if (rwhere) bfree(rwhere);
         rwhere = bstrdup(ua->cmd);
      }
      goto try_again_reg;
   case 4:
      /* Test regexp */
      char *result;
      char *regexp;

      if (rwhere && rwhere[0] != '\0') {
         regs = get_bregexps(rwhere);
         ua->send_msg(_("regexwhere=%s\n"), NPRT(rwhere));
      } else {
         int len = bregexp_get_build_where_size(strip_prefix, add_prefix, add_suffix);
         regexp = (char *) bmalloc (len * sizeof(char));
         bregexp_build_where(regexp, len, strip_prefix, add_prefix, add_suffix);
         regs = get_bregexps(regexp);
         ua->send_msg(_("strip_prefix=%s add_prefix=%s add_suffix=%s result=%s\n"),
                      NPRT(strip_prefix), NPRT(add_prefix), NPRT(add_suffix), NPRT(regexp));

         bfree(regexp);
      }

      if (!regs) {
         ua->send_msg(_("Cannot use your regexp\n"));
         goto try_again_reg;
      }
      ua->send_msg(_("Enter a period (.) to stop this test\n"));
      while (get_cmd(ua, _("Please enter filename to test: "))) {
         apply_bregexps(ua->cmd, regs, &result);
         ua->send_msg(_("%s -> %s\n"), ua->cmd, result);
      }
      free_bregexps(regs);
      delete regs;
      goto try_again_reg;
   case 5:
      /* OK */
      break;
   case -1:                        /* error or cancel */
      goto bail_out_reg;
   default:
      goto try_again_reg;
   }

   /* replace the existing where */
   if (jcr->where) {
      bfree(jcr->where);
      jcr->where = NULL;
   }

   /* replace the existing regexwhere */
   if (jcr->RegexWhere) {
      bfree(jcr->RegexWhere);
      jcr->RegexWhere = NULL;
   }

   if (rwhere) {
      jcr->RegexWhere = bstrdup(rwhere);
   } else if (strip_prefix || add_prefix || add_suffix) {
      int len = bregexp_get_build_where_size(strip_prefix, add_prefix, add_suffix);
      jcr->RegexWhere = (char *) bmalloc(len*sizeof(char));
      bregexp_build_where(jcr->RegexWhere, len, strip_prefix, add_prefix, add_suffix);
   }

   regs = get_bregexps(jcr->RegexWhere);
   if (regs) {
      free_bregexps(regs);
      delete regs;
   } else {
      if (jcr->RegexWhere) {
         bfree(jcr->RegexWhere);
         jcr->RegexWhere = NULL;
      }
      ua->send_msg(_("Cannot use your regexp.\n"));
   }

bail_out_reg:
   if (strip_prefix) {
      bfree(strip_prefix);
   }
   if (add_prefix) {
      bfree(add_prefix);
   }
   if (add_suffix) {
      bfree(add_suffix);
   }
   if (rwhere) {
      bfree(rwhere);
   }
}

static void select_job_level(UaContext *ua, JobControlRecord *jcr)
{
   if (jcr->is_JobType(JT_BACKUP)) {
      start_prompt(ua, _("Levels:\n"));
//    add_prompt(ua, _("Base"));
      add_prompt(ua, _("Full"));
      add_prompt(ua, _("Incremental"));
      add_prompt(ua, _("Differential"));
      add_prompt(ua, _("Since"));
      add_prompt(ua, _("VirtualFull"));
      switch (do_prompt(ua, "", _("Select level"), NULL, 0)) {
//    case 0:
//       jcr->JobLevel = L_BASE;
//       break;
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
      start_prompt(ua, _("Levels:\n"));
      add_prompt(ua, _("Initialize Catalog"));
      add_prompt(ua, _("Verify Catalog"));
      add_prompt(ua, _("Verify Volume to Catalog"));
      add_prompt(ua, _("Verify Disk to Catalog"));
      add_prompt(ua, _("Verify Volume Data (not yet implemented)"));
      switch (do_prompt(ua, "",  _("Select level"), NULL, 0)) {
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
      ua->warning_msg(_("Level not appropriate for this Job. Cannot be changed.\n"));
   }
   return;
}

static bool display_job_parameters(UaContext *ua, JobControlRecord *jcr, RunContext &rc)
{
   char ec1[30];
   JobResource *job = rc.job;
   char dt[MAX_TIME_LENGTH];
   const char *verify_list = rc.verify_list;

   Dmsg1(800, "JobType=%c\n", jcr->getJobType());
   switch (jcr->getJobType()) {
   case JT_ADMIN:
      if (ua->api) {
         ua->signal(BNET_RUN_CMD);
         ua->send_msg("Type: Admin\n"
                      "Title: Run Admin Job\n"
                      "JobName:  %s\n"
                      "FileSet:  %s\n"
                      "Client:   %s\n"
                      "Storage:  %s\n"
                      "When:     %s\n"
                      "Priority: %d\n",
                      job->name(),
                      jcr->res.fileset->name(),
                      NPRT(jcr->res.client->name()),
                      jcr->res.wstore ? jcr->res.wstore->name() : _("*None*"),
                      bstrutime(dt, sizeof(dt), jcr->sched_time),
                      jcr->JobPriority);
      } else {
         ua->send_msg(_("Run Admin Job\n"
                        "JobName:  %s\n"
                        "FileSet:  %s\n"
                        "Client:   %s\n"
                        "Storage:  %s\n"
                        "When:     %s\n"
                        "Priority: %d\n"),
                      job->name(),
                      jcr->res.fileset->name(),
                      NPRT(jcr->res.client->name()),
                      jcr->res.wstore ? jcr->res.wstore->name() : _("*None*"),
                      bstrutime(dt, sizeof(dt), jcr->sched_time),
                      jcr->JobPriority);
      }
      jcr->setJobLevel(L_FULL);
      break;
   case JT_ARCHIVE:
      if (ua->api) {
         ua->signal(BNET_RUN_CMD);
         ua->send_msg("Type: Archive\n"
                      "Title: Run Archive Job\n"
                      "JobName:  %s\n"
                      "FileSet:  %s\n"
                      "Client:   %s\n"
                      "Storage:  %s\n"
                      "When:     %s\n"
                      "Priority: %d\n",
                      job->name(),
                      jcr->res.fileset->name(),
                      NPRT(jcr->res.client->name()),
                      jcr->res.wstore ? jcr->res.wstore->name() : _("*None*"),
                      bstrutime(dt, sizeof(dt), jcr->sched_time),
                      jcr->JobPriority);
      } else {
         ua->send_msg(_("Run Archive Job\n"
                        "JobName:  %s\n"
                        "FileSet:  %s\n"
                        "Client:   %s\n"
                        "Storage:  %s\n"
                        "When:     %s\n"
                        "Priority: %d\n"),
                      job->name(),
                      jcr->res.fileset->name(),
                      NPRT(jcr->res.client->name()),
                      jcr->res.wstore ? jcr->res.wstore->name() : _("*None*"),
                      bstrutime(dt, sizeof(dt), jcr->sched_time),
                      jcr->JobPriority);
      }
      jcr->setJobLevel(L_FULL);
      break;
   case JT_CONSOLIDATE:
      if (ua->api) {
         ua->signal(BNET_RUN_CMD);
         ua->send_msg("Type: Consolidate\n"
                      "Title: Run Consolidate Job\n"
                      "JobName:  %s\n"
                      "FileSet:  %s\n"
                      "Client:   %s\n"
                      "Storage:  %s\n"
                      "When:     %s\n"
                      "Priority: %d\n",
                      job->name(),
                      jcr->res.fileset->name(),
                      NPRT(jcr->res.client->name()),
                      jcr->res.wstore ? jcr->res.wstore->name() : _("*None*"),
                      bstrutime(dt, sizeof(dt), jcr->sched_time),
                      jcr->JobPriority);
      } else {
         ua->send_msg(_("Run Consolidate Job\n"
                        "JobName:  %s\n"
                        "FileSet:  %s\n"
                        "Client:   %s\n"
                        "Storage:  %s\n"
                        "When:     %s\n"
                        "Priority: %d\n"),
                      job->name(),
                      jcr->res.fileset->name(),
                      NPRT(jcr->res.client->name()),
                      jcr->res.wstore ? jcr->res.wstore->name() : _("*None*"),
                      bstrutime(dt, sizeof(dt), jcr->sched_time),
                      jcr->JobPriority);
      }
      jcr->setJobLevel(L_FULL);
      break;
   case JT_BACKUP:
   case JT_VERIFY:
      if (jcr->is_JobType(JT_BACKUP)) {
         bool is_virtual = jcr->is_JobLevel(L_VIRTUAL_FULL);

         if (ua->api) {
            ua->signal(BNET_RUN_CMD);
            ua->send_msg("Type: Backup\n"
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
                         job->name(),
                         level_to_str(jcr->getJobLevel()),
                         jcr->res.client->name(),
                         jcr->backup_format,
                         jcr->res.fileset->name(),
                         NPRT(jcr->res.pool->name()),
                         is_virtual ? "NextPool: " : "",
                         is_virtual ? (jcr->res.next_pool ? jcr->res.next_pool->name() : _("*None*")) : "",
                         is_virtual ? "\n" : "",
                         jcr->res.wstore ? jcr->res.wstore->name() : _("*None*"),
                         bstrutime(dt, sizeof(dt), jcr->sched_time),
                         jcr->JobPriority,
                         jcr->plugin_options ? "Plugin Options: " : "",
                         jcr->plugin_options ? jcr->plugin_options : "",
                         jcr->plugin_options ? "\n" : "");
         } else {
            ua->send_msg(_("Run Backup job\n"
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
                         job->name(),
                         level_to_str(jcr->getJobLevel()),
                         jcr->res.client->name(),
                         jcr->backup_format,
                         jcr->res.fileset->name(),
                         NPRT(jcr->res.pool->name()), jcr->res.pool_source,
                         is_virtual ? "NextPool: " : "",
                         is_virtual ? (jcr->res.next_pool ? jcr->res.next_pool->name() : _("*None*")) : "",
                         is_virtual ? " (From " : "",
                         is_virtual ? jcr->res.npool_source : "",
                         is_virtual ? ")\n" : "",
                         jcr->res.wstore ? jcr->res.wstore->name() : _("*None*"),
                         jcr->res.wstore_source,
                         bstrutime(dt, sizeof(dt), jcr->sched_time),
                         jcr->JobPriority,
                         jcr->plugin_options ? "Plugin Options: " : "",
                         jcr->plugin_options ? jcr->plugin_options : "",
                         jcr->plugin_options ? "\n" : "");
         }
      } else {  /* JT_VERIFY */
         JobDbRecord jr;
         const char *Name;
         if (jcr->res.verify_job) {
            Name = jcr->res.verify_job->name();
         } else if (jcr->RestoreJobId) { /* Display job name if jobid requested */
            memset(&jr, 0, sizeof(jr));
            jr.JobId = jcr->RestoreJobId;
            if (!ua->db->get_job_record(jcr, &jr)) {
               ua->error_msg(_("Could not get job record for selected JobId. ERR=%s"), ua->db->strerror());
               return false;
            }
            Name = jr.Job;
         } else {
            Name = "";
         }
         if (!verify_list) {
            verify_list = job->WriteVerifyList;
         }
         if (!verify_list) {
            verify_list = "";
         }
         if (ua->api) {
            ua->signal(BNET_RUN_CMD);
            ua->send_msg("Type: Verify\n"
                         "Title: Run Verify Job\n"
                         "JobName:     %s\n"
                         "Level:       %s\n"
                         "Client:      %s\n"
                         "FileSet:     %s\n"
                         "Pool:        %s (From %s)\n"
                         "Storage:     %s (From %s)\n"
                         "Verify Job:  %s\n"
                         "Verify List: %s\n"
                         "When:        %s\n"
                         "Priority:    %d\n",
                         job->name(),
                         level_to_str(jcr->getJobLevel()),
                         jcr->res.client->name(),
                         jcr->res.fileset->name(),
                         NPRT(jcr->res.pool->name()), jcr->res.pool_source,
                         jcr->res.rstore->name(), jcr->res.rstore_source,
                         Name,
                         verify_list,
                         bstrutime(dt, sizeof(dt), jcr->sched_time),
                         jcr->JobPriority);
         } else {
            ua->send_msg(_("Run Verify Job\n"
                           "JobName:     %s\n"
                           "Level:       %s\n"
                           "Client:      %s\n"
                           "FileSet:     %s\n"
                           "Pool:        %s (From %s)\n"
                           "Storage:     %s (From %s)\n"
                           "Verify Job:  %s\n"
                           "Verify List: %s\n"
                           "When:        %s\n"
                           "Priority:    %d\n"),
                         job->name(),
                         level_to_str(jcr->getJobLevel()),
                         jcr->res.client->name(),
                         jcr->res.fileset->name(),
                         NPRT(jcr->res.pool->name()), jcr->res.pool_source,
                         jcr->res.rstore->name(), jcr->res.rstore_source,
                         Name,
                         verify_list,
                         bstrutime(dt, sizeof(dt), jcr->sched_time),
                         jcr->JobPriority);
         }
      }
      break;
   case JT_RESTORE:
      if (jcr->RestoreJobId == 0 && !jcr->RestoreBootstrap) {
         if (rc.jid) {
            jcr->RestoreJobId = str_to_int64(rc.jid);
         } else {
            if (!get_pint(ua, _("Please enter a JobId for restore: "))) {
               return false;
            }
            jcr->RestoreJobId = ua->int64_val;
         }
      }
      jcr->setJobLevel(L_FULL);      /* default level */
      Dmsg1(800, "JobId to restore=%d\n", jcr->RestoreJobId);
      if (jcr->RestoreJobId == 0) {
         /* RegexWhere is take before RestoreWhere */
         if (jcr->RegexWhere || (job->RegexWhere && !jcr->where)) {
            if (ua->api) {
               ua->signal(BNET_RUN_CMD);
               ua->send_msg("Type: Restore\n"
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
                            job->name(),
                            NPRT(jcr->RestoreBootstrap),
                            jcr->RegexWhere ? jcr->RegexWhere : job->RegexWhere,
                            rc.replace,
                            jcr->res.fileset->name(),
                            rc.client_name,
                            jcr->res.client->name(),
                            jcr->backup_format,
                            jcr->res.rstore->name(),
                            bstrutime(dt, sizeof(dt), jcr->sched_time),
                            jcr->res.catalog->name(),
                            jcr->JobPriority,
                            NPRT(jcr->plugin_options));
            } else {
               ua->send_msg(_("Run Restore job\n"
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
                            job->name(),
                            NPRT(jcr->RestoreBootstrap),
                            jcr->RegexWhere ? jcr->RegexWhere : job->RegexWhere,
                            rc.replace,
                            jcr->res.fileset->name(),
                            rc.client_name,
                            jcr->res.client->name(),
                            jcr->backup_format,
                            jcr->res.rstore->name(),
                            bstrutime(dt, sizeof(dt), jcr->sched_time),
                            jcr->res.catalog->name(),
                            jcr->JobPriority,
                            NPRT(jcr->plugin_options));
            }
         } else {
            if (ua->api) {
               ua->signal(BNET_RUN_CMD);
               ua->send_msg("Type: Restore\n"
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
                            job->name(),
                            NPRT(jcr->RestoreBootstrap),
                            jcr->where ? jcr->where : NPRT(job->RestoreWhere),
                            rc.replace,
                            jcr->res.fileset->name(),
                            rc.client_name,
                            jcr->res.client->name(),
                            jcr->backup_format,
                            jcr->res.rstore->name(),
                            bstrutime(dt, sizeof(dt), jcr->sched_time),
                            jcr->res.catalog->name(),
                            jcr->JobPriority,
                            NPRT(jcr->plugin_options));
            } else {
               ua->send_msg(_("Run Restore job\n"
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
                            job->name(),
                            NPRT(jcr->RestoreBootstrap),
                            jcr->where ? jcr->where : NPRT(job->RestoreWhere),
                            rc.replace,
                            jcr->res.fileset->name(),
                            rc.client_name,
                            jcr->res.client->name(),
                            jcr->backup_format,
                            jcr->res.rstore->name(),
                            bstrutime(dt, sizeof(dt), jcr->sched_time),
                            jcr->res.catalog->name(),
                            jcr->JobPriority,
                            NPRT(jcr->plugin_options));
            }
         }

      } else {
         /* ***FIXME*** This needs to be fixed for bat */
         if (ua->api) ua->signal(BNET_RUN_CMD);
         ua->send_msg(_("Run Restore job\n"
                        "JobName:    %s\n"
                        "Bootstrap:  %s\n"),
                      job->name(),
                      NPRT(jcr->RestoreBootstrap));

         /* RegexWhere is take before RestoreWhere */
         if (jcr->RegexWhere || (job->RegexWhere && !jcr->where)) {
            ua->send_msg(_("RegexWhere: %s\n"),
                         jcr->RegexWhere ? jcr->RegexWhere : job->RegexWhere);
         } else {
            ua->send_msg(_("Where:      %s\n"),
                         jcr->where ? jcr->where : NPRT(job->RestoreWhere));
         }

         ua->send_msg(_("Replace:         %s\n"
                        "Client:          %s\n"
                        "Format:          %s\n"
                        "Storage:         %s\n"
                        "JobId:           %s\n"
                        "When:            %s\n"
                        "Catalog:         %s\n"
                        "Priority:        %d\n"
                        "Plugin Options:  %s\n"),
                      rc.replace,
                      jcr->res.client->name(),
                      jcr->backup_format,
                      jcr->res.rstore->name(),
                      (jcr->RestoreJobId == 0) ? _("*None*") : edit_uint64(jcr->RestoreJobId, ec1),
                      bstrutime(dt, sizeof(dt), jcr->sched_time),
                      jcr->res.catalog->name(),
                      jcr->JobPriority,
                      NPRT(jcr->plugin_options));
      }
      break;
   case JT_COPY:
   case JT_MIGRATE:
      const char *prt_type;

      jcr->setJobLevel(L_FULL);      /* default level */
      if (ua->api) {
         ua->signal(BNET_RUN_CMD);
         if (jcr->is_JobType(JT_COPY)) {
            prt_type = _("Type: Copy\nTitle: Run Copy Job\n");
         } else {
            prt_type = _("Type: Migration\nTitle: Run Migration Job\n");
         }
         ua->send_msg("%s"
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
                      prt_type,
                      job->name(),
                      NPRT(jcr->RestoreBootstrap),
                      jcr->res.rstore ? jcr->res.rstore->name() : _("*None*"),
                      NPRT(jcr->res.pool->name()),
                      jcr->res.next_pool ? jcr->res.next_pool->name() : _("*None*"),
                      jcr->res.wstore ? jcr->res.wstore->name() : _("*None*"),
                      (jcr->MigrateJobId == 0) ? _("*None*") : edit_uint64(jcr->MigrateJobId, ec1),
                      bstrutime(dt, sizeof(dt), jcr->sched_time),
                      jcr->res.catalog->name(),
                      jcr->JobPriority);
      } else {
         if (jcr->is_JobType(JT_COPY)) {
            prt_type = _("Run Copy job\n");
         } else {
            prt_type = _("Run Migration job\n");
         }
         ua->send_msg(_("%s"
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
                      prt_type,
                      job->name(),
                      NPRT(jcr->RestoreBootstrap),
                      jcr->res.rstore ? jcr->res.rstore->name() : _("*None*"),
                      jcr->res.rstore_source,
                      NPRT(jcr->res.pool->name()), jcr->res.pool_source,
                      jcr->res.wstore ? jcr->res.wstore->name() : _("*None*"),
                      jcr->res.wstore_source,
                      jcr->res.next_pool ? jcr->res.next_pool->name() : _("*None*"),
                      NPRT(jcr->res.npool_source),
                      jcr->MigrateJobId == 0 ? _("*None*") : edit_uint64(jcr->MigrateJobId, ec1),
                      bstrutime(dt, sizeof(dt), jcr->sched_time),
                      jcr->res.catalog->name(),
                      jcr->JobPriority);
      }
      break;
   default:
      ua->error_msg(_("Unknown Job Type=%d\n"), jcr->getJobType());
      return false;
   }
   return true;
}

static bool scan_command_line_arguments(UaContext *ua, RunContext &rc)
{
   bool kw_ok;
   int i, j;
   static const char *kw[] = { /* command line arguments */
      "job",                   /* 0 */
      "jobid",                 /* 1 */
      "client",                /* 2 */
      "fd",                    /* 3 */
      "fileset",               /* 4 */
      "level",                 /* 5 */
      "storage",               /* 6 */
      "sd",                    /* 7 */
      "regexwhere",            /* 8 - where string as a bregexp */
      "where",                 /* 9 */
      "bootstrap",             /* 10 */
      "replace",               /* 11 */
      "when",                  /* 12 */
      "priority",              /* 13 */
      "yes",                   /* 14 -- if you change this change YES_POS too */
      "verifyjob",             /* 15 */
      "files",                 /* 16 - number of files to restore */
      "catalog",               /* 17 - override catalog */
      "since",                 /* 18 - since */
      "cloned",                /* 19 - cloned */
      "verifylist",            /* 20 - verify output list */
      "migrationjob",          /* 21 - migration job name */
      "pool",                  /* 22 */
      "nextpool",              /* 23 */
      "backupclient",          /* 24 */
      "restoreclient",         /* 25 */
      "pluginoptions",         /* 26 */
      "spooldata",             /* 27 */
      "comment",               /* 28 */
      "ignoreduplicatecheck",  /* 29 */
      "accurate",              /* 30 */
      "backupformat",          /* 31 */
      NULL
   };

#define YES_POS 14

   rc.catalog_name = NULL;
   rc.job_name = NULL;
   rc.pool_name = NULL;
   rc.next_pool_name = NULL;
   rc.store_name = NULL;
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

   for (i = 1; i < ua->argc; i++) {
      Dmsg2(800, "Doing arg %d = %s\n", i, ua->argk[i]);
      kw_ok = false;

      /*
       * Keep looking until we find a good keyword
       */
      for (j = 0; !kw_ok && kw[j]; j++) {
         if (bstrcasecmp(ua->argk[i], kw[j])) {
            /*
             * Note, yes and run have no value, so do not fail
             */
            if (!ua->argv[i] && j != YES_POS /*yes*/) {
               ua->send_msg(_("Value missing for keyword %s\n"), ua->argk[i]);
               return false;
            }
            Dmsg1(800, "Got keyword=%s\n", NPRT(kw[j]));
            switch (j) {
            case 0: /* job */
               if (rc.job_name) {
                  ua->send_msg(_("Job name specified twice.\n"));
                  return false;
               }
               rc.job_name = ua->argv[i];
               kw_ok = true;
               break;
            case 1: /* JobId */
               if (rc.jid && !rc.mod) {
                  ua->send_msg(_("JobId specified twice.\n"));
                  return false;
               }
               rc.jid = ua->argv[i];
               kw_ok = true;
               break;
            case 2: /* client */
            case 3: /* fd */
               if (rc.client_name) {
                  ua->send_msg(_("Client specified twice.\n"));
                  return false;
               }
               rc.client_name = ua->argv[i];
               kw_ok = true;
               break;
            case 4: /* fileset */
               if (rc.fileset_name) {
                  ua->send_msg(_("FileSet specified twice.\n"));
                  return false;
               }
               rc.fileset_name = ua->argv[i];
               kw_ok = true;
               break;
            case 5: /* level */
               if (rc.level_name) {
                  ua->send_msg(_("Level specified twice.\n"));
                  return false;
               }
               rc.level_name = ua->argv[i];
               kw_ok = true;
               break;
            case 6: /* storage */
            case 7: /* sd */
               if (rc.store_name) {
                  ua->send_msg(_("Storage specified twice.\n"));
                  return false;
               }
               rc.store_name = ua->argv[i];
               kw_ok = true;
               break;
            case 8: /* regexwhere */
                if ((rc.regexwhere || rc.where) && !rc.mod) {
                  ua->send_msg(_("RegexWhere or Where specified twice.\n"));
                  return false;
               }
               rc.regexwhere = ua->argv[i];
               if (!ua->acl_access_ok(Where_ACL, rc.regexwhere, true)) {
                  ua->send_msg(_("No authorization for \"regexwhere\" specification.\n"));
                  return false;
               }
               kw_ok = true;
               break;
           case 9: /* where */
               if ((rc.where || rc.regexwhere) && !rc.mod) {
                  ua->send_msg(_("Where or RegexWhere specified twice.\n"));
                  return false;
               }
               rc.where = ua->argv[i];
               if (!ua->acl_access_ok(Where_ACL, rc.where, true)) {
                  ua->send_msg(_("No authoriztion for \"where\" specification.\n"));
                  return false;
               }
               kw_ok = true;
               break;
            case 10: /* bootstrap */
               if (rc.bootstrap && !rc.mod) {
                  ua->send_msg(_("Bootstrap specified twice.\n"));
                  return false;
               }
               rc.bootstrap = ua->argv[i];
               kw_ok = true;
               break;
            case 11: /* replace */
               if (rc.replace && !rc.mod) {
                  ua->send_msg(_("Replace specified twice.\n"));
                  return false;
               }
               rc.replace = ua->argv[i];
               kw_ok = true;
               break;
            case 12: /* When */
               if (rc.when && !rc.mod) {
                  ua->send_msg(_("When specified twice.\n"));
                  return false;
               }
               rc.when = ua->argv[i];
               kw_ok = true;
               break;
            case 13:  /* Priority */
               if (rc.Priority && !rc.mod) {
                  ua->send_msg(_("Priority specified twice.\n"));
                  return false;
               }
               rc.Priority = atoi(ua->argv[i]);
               if (rc.Priority <= 0) {
                  ua->send_msg(_("Priority must be positive nonzero setting it to 10.\n"));
                  rc.Priority = 10;
               }
               kw_ok = true;
               break;
            case 14: /* yes */
               kw_ok = true;
               break;
            case 15: /* Verify Job */
               if (rc.verify_job_name) {
                  ua->send_msg(_("Verify Job specified twice.\n"));
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
               rc. cloned = true;
               kw_ok = true;
               break;
            case 20: /* write verify list output */
               rc.verify_list = ua->argv[i];
               kw_ok = true;
               break;
            case 21: /* Migration Job */
               if (rc.previous_job_name) {
                  ua->send_msg(_("Migration Job specified twice.\n"));
                  return false;
               }
               rc.previous_job_name = ua->argv[i];
               kw_ok = true;
               break;
            case 22: /* pool */
               if (rc.pool_name) {
                  ua->send_msg(_("Pool specified twice.\n"));
                  return false;
               }
               rc.pool_name = ua->argv[i];
               kw_ok = true;
               break;
            case 23: /* nextpool */
               if (rc.next_pool_name) {
                  ua->send_msg(_("NextPool specified twice.\n"));
                  return false;
               }
               rc.next_pool_name = ua->argv[i];
               kw_ok = true;
               break;
            case 24: /* backupclient */
               if (rc.client_name) {
                  ua->send_msg(_("Client specified twice.\n"));
                  return 0;
               }
               rc.client_name = ua->argv[i];
               kw_ok = true;
               break;
            case 25: /* restoreclient */
               if (rc.restore_client_name && !rc.mod) {
                  ua->send_msg(_("Restore Client specified twice.\n"));
                  return false;
               }
               rc.restore_client_name = ua->argv[i];
               kw_ok = true;
               break;
            case 26: /* pluginoptions */
               if (rc.plugin_options) {
                  ua->send_msg(_("Plugin Options specified twice.\n"));
                  return false;
               }
               rc.plugin_options = ua->argv[i];
               if (!ua->acl_access_ok(PluginOptions_ACL, rc.plugin_options, true)) {
                  ua->send_msg(_("No authorization for \"PluginOptions\" specification.\n"));
                  return false;
               }
               kw_ok = true;
               break;
            case 27: /* spooldata */
               if (rc.spool_data_set) {
                  ua->send_msg(_("Spool flag specified twice.\n"));
                  return false;
               }
               if (is_yesno(ua->argv[i], &rc.spool_data)) {
                  rc.spool_data_set = true;
                  kw_ok = true;
               } else {
                  ua->send_msg(_("Invalid spooldata flag.\n"));
               }
               break;
            case 28: /* comment */
               rc.comment = ua->argv[i];
               kw_ok = true;
               break;
            case 29: /* ignoreduplicatecheck */
               if (rc.ignoreduplicatecheck_set) {
                  ua->send_msg(_("IgnoreDuplicateCheck flag specified twice.\n"));
                  return false;
               }
               if (is_yesno(ua->argv[i], &rc.ignoreduplicatecheck)) {
                  rc.ignoreduplicatecheck_set = true;
                  kw_ok = true;
               } else {
                  ua->send_msg(_("Invalid ignoreduplicatecheck flag.\n"));
               }
               break;
            case 30: /* accurate */
               if (rc.accurate_set) {
                  ua->send_msg(_("Accurate flag specified twice.\n"));
                  return false;
               }
               if (is_yesno(ua->argv[i], &rc.accurate)) {
                  rc.accurate_set = true;
                  kw_ok = true;
               } else {
                  ua->send_msg(_("Invalid accurate flag.\n"));
               }
               break;
            case 31: /* backupformat */
               if (rc.backup_format && !rc.mod) {
                  ua->send_msg(_("Backup Format specified twice.\n"));
                  return false;
               }
               rc.backup_format = ua->argv[i];
               kw_ok = true;
               break;
            default:
               break;
            }
         } /* end strcase compare */
      } /* end keyword loop */

      /*
       * End of keyword for loop -- if not found, we got a bogus keyword
       */
      if (!kw_ok) {
         Dmsg1(800, "%s not found\n", ua->argk[i]);
         /*
          * Special case for Job Name, it can be the first
          * keyword that has no value.
          */
         if (!rc.job_name && !ua->argv[i]) {
            rc.job_name = ua->argk[i];   /* use keyword as job name */
            Dmsg1(800, "Set jobname=%s\n", rc.job_name);
         } else {
            ua->send_msg(_("Invalid keyword: %s\n"), ua->argk[i]);
            return false;
         }
      }
   } /* end argc loop */

   Dmsg0(800, "Done scan.\n");
   if (rc.comment) {
      if (!is_comment_legal(ua, rc.comment)) {
         return false;
      }
   }
   if (rc.catalog_name) {
      rc.catalog = ua->GetCatalogResWithName(rc.catalog_name);
      if (rc.catalog == NULL) {
         ua->error_msg(_("Catalog \"%s\" not found\n"), rc.catalog_name);
         return false;
      }
   }
   Dmsg1(800, "Using catalog=%s\n", NPRT(rc.catalog_name));

   if (rc.job_name) {
      /* Find Job */
      rc.job = ua->GetJobResWithName(rc.job_name);
      if (!rc.job) {
         if (*rc.job_name != 0) {
            ua->send_msg(_("Job \"%s\" not found\n"), rc.job_name);
         }
         rc.job = select_job_resource(ua);
      } else {
         Dmsg1(800, "Found job=%s\n", rc.job_name);
      }
   } else if (!rc.job) {
      ua->send_msg(_("A job name must be specified.\n"));
      rc.job = select_job_resource(ua);
   }
   if (!rc.job) {
      return false;
   }

   if (rc.pool_name) {
      rc.pool = ua->GetPoolResWithName(rc.pool_name);
      if (!rc.pool) {
         if (*rc.pool_name != 0) {
            ua->warning_msg(_("Pool \"%s\" not found.\n"), rc.pool_name);
         }
         rc.pool = select_pool_resource(ua);
      }
   } else if (!rc.pool) {
      rc.pool = rc.job->pool;               /* use default */
   }
   if (!rc.pool) {
      return false;
   }
   Dmsg1(100, "Using pool %s\n", rc.pool->name());

   if (rc.next_pool_name) {
      rc.next_pool = ua->GetPoolResWithName(rc.next_pool_name);
      if (!rc.next_pool) {
         if (*rc.next_pool_name != 0) {
            ua->warning_msg(_("Pool \"%s\" not found.\n"), rc.next_pool_name);
         }
         rc.next_pool = select_pool_resource(ua);
      }
   } else if (!rc.next_pool) {
      rc.next_pool = rc.pool->NextPool;     /* use default */
   }
   if (rc.next_pool) {
      Dmsg1(100, "Using next pool %s\n", rc.next_pool->name());
   }

   if (rc.store_name) {
      rc.store->store = ua->GetStoreResWithName(rc.store_name);
      pm_strcpy(rc.store->store_source, _("command line"));
      if (!rc.store->store) {
         if (*rc.store_name != 0) {
            ua->warning_msg(_("Storage \"%s\" not found.\n"), rc.store_name);
         }
         rc.store->store = select_storage_resource(ua);
         pm_strcpy(rc.store->store_source, _("user selection"));
      }
   } else if (!rc.store->store) {
      get_job_storage(rc.store, rc.job, NULL); /* use default */
   }

   /*
    * For certain Jobs an explicit setting of the read storage is not
    * required as its determined when the Job is executed automatically.
    */
   switch (rc.job->JobType) {
   case JT_COPY:
   case JT_MIGRATE:
      break;
   default:
      if (!rc.store->store) {
         ua->error_msg(_("No storage specified.\n"));
         return false;
      } else if (!ua->acl_access_ok(Storage_ACL, rc.store->store->name(), true)) {
         ua->error_msg(_("No authorization. Storage \"%s\".\n"), rc.store->store->name());
         return false;
      }
      Dmsg1(800, "Using storage=%s\n", rc.store->store->name());
      break;
   }

   if (rc.client_name) {
      rc.client = ua->GetClientResWithName(rc.client_name);
      if (!rc.client) {
         if (*rc.client_name != 0) {
            ua->warning_msg(_("Client \"%s\" not found.\n"), rc.client_name);
         }
         rc.client = select_client_resource(ua);
      }
   } else if (!rc.client) {
      rc.client = rc.job->client;           /* use default */
   }

   if (rc.client) {
      if (!ua->acl_access_ok(Client_ACL, rc.client->name(), true)) {
         ua->error_msg(_("No authorization. Client \"%s\".\n"), rc.client->name());
         return false;
      } else {
         Dmsg1(800, "Using client=%s\n", rc.client->name());
      }
   }

   if (rc.restore_client_name) {
      rc.client = ua->GetClientResWithName(rc.restore_client_name);
      if (!rc.client) {
         if (*rc.restore_client_name != 0) {
            ua->warning_msg(_("Restore Client \"%s\" not found.\n"), rc.restore_client_name);
         }
         rc.client = select_client_resource(ua);
      }
   } else if (!rc.client) {
      rc.client = rc.job->client;           /* use default */
   }

   if (rc.client) {
      if (!ua->acl_access_ok(Client_ACL, rc.client->name(), true)) {
         ua->error_msg(_("No authorization. Client \"%s\".\n"), rc.client->name());
         return false;
      } else {
         Dmsg1(800, "Using restore client=%s\n", rc.client->name());
      }
   }

   if (rc.fileset_name) {
      rc.fileset = ua->GetFileSetResWithName(rc.fileset_name);
      if (!rc.fileset) {
         ua->send_msg(_("FileSet \"%s\" not found.\n"), rc.fileset_name);
         rc.fileset = select_fileset_resource(ua);
         if (!rc.fileset) {
            return false;
         }
      }
   } else if (!rc.fileset) {
      rc.fileset = rc.job->fileset;           /* use default */
   }

   if (rc.verify_job_name) {
      rc.verify_job = ua->GetJobResWithName(rc.verify_job_name);
      if (!rc.verify_job) {
         ua->send_msg(_("Verify Job \"%s\" not found.\n"), rc.verify_job_name);
         rc.verify_job = select_job_resource(ua);
      }
   } else if (!rc.verify_job) {
      rc.verify_job = rc.job->verify_job;
   }

   if (rc.previous_job_name) {
      rc.previous_job = ua->GetJobResWithName(rc.previous_job_name);
      if (!rc.previous_job) {
         ua->send_msg(_("Migration Job \"%s\" not found.\n"), rc.previous_job_name);
         rc.previous_job = select_job_resource(ua);
      }
   } else {
      rc.previous_job = rc.job->verify_job;
   }
   return true;
}
