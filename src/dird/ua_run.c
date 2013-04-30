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
 *   Bacula Director -- Run Command
 *
 *     Kern Sibbald, December MMI
 *
 */

#include "bacula.h"
#include "dird.h"

class run_ctx {
public:
   char *job_name, *level_name, *jid, *store_name, *pool_name;
   char *where, *fileset_name, *client_name, *bootstrap, *regexwhere;
   char *restore_client_name, *comment;
   const char *replace;
   char *when, *verify_job_name, *catalog_name;
   char *previous_job_name;
   char *since;
   char *plugin_options;
   const char *verify_list;
   JOB *job;
   JOB *verify_job;
   JOB *previous_job;
   USTORE *store;
   CLIENT *client;
   FILESET *fileset;
   POOL *pool;
   CAT *catalog;
   int Priority;
   int files;
   bool cloned;
   bool mod;
   int spool_data;
   bool spool_data_set;
   int accurate;
   bool accurate_set;
   int ignoreduplicatecheck;
   bool ignoreduplicatecheck_set;

   /* Methods */
   run_ctx() { memset(this, 0, sizeof(run_ctx)); 
               store = new USTORE; };
   ~run_ctx() { delete store; };
};

/* Forward referenced subroutines */
static void select_job_level(UAContext *ua, JCR *jcr);
static bool display_job_parameters(UAContext *ua, JCR *jcr, JOB *job, 
                const char *verify_list, char *jid, const char *replace,
                char *client_name);
static void select_where_regexp(UAContext *ua, JCR *jcr);
static bool scan_command_line_arguments(UAContext *ua, run_ctx &rc);
static bool reset_restore_context(UAContext *ua, JCR *jcr, run_ctx &rc);
static int modify_job_parameters(UAContext *ua, JCR *jcr, run_ctx &rc);

/* Imported variables */
extern struct s_kw ReplaceOptions[];

/*
 * For Backup and Verify Jobs
 *     run [job=]<job-name> level=<level-name>
 *
 * For Restore Jobs
 *     run <job-name> 
 *
 *  Returns: 0 on error
 *           JobId if OK
 *
 */
int run_cmd(UAContext *ua, const char *cmd)
{
   JCR *jcr = NULL;
   run_ctx rc;
   int status;

   if (!open_client_db(ua)) {
      return 1;
   }

   if (!scan_command_line_arguments(ua, rc)) {
      return 0;
   }

   if (find_arg(ua, NT_("fdcalled")) > 0) {
      jcr->file_bsock = dup_bsock(ua->UA_sock);
      ua->quit = true;
   }

   /*
    * Create JCR to run job.  NOTE!!! after this point, free_jcr()
    *  before returning.
    */
   if (!jcr) {
      jcr = new_jcr(sizeof(JCR), dird_free_jcr);
      set_jcr_defaults(jcr, rc.job);
      jcr->unlink_bsr = ua->jcr->unlink_bsr;    /* copy unlink flag from caller */
      ua->jcr->unlink_bsr = false;
   }
   /* Transfer JobIds to new restore Job */
   if (ua->jcr->JobIds) {
      jcr->JobIds = ua->jcr->JobIds;
      ua->jcr->JobIds = NULL;
   }

try_again:
   if (!reset_restore_context(ua, jcr, rc)) {
      goto bail_out;
   }


   /* Run without prompting? */
   if (ua->batch || find_arg(ua, NT_("yes")) > 0) {
      goto start_job;
   }

   /*
    * Prompt User to see if all run job parameters are correct, and
    *   allow him to modify them.
    */
   if (!display_job_parameters(ua, jcr, rc.job, rc.verify_list, rc.jid, rc.replace,
        rc.client_name)) {
      goto bail_out;
   }

   if (!get_cmd(ua, _("OK to run? (yes/mod/no): "))) {
      goto bail_out;
   }

   if (strncasecmp(ua->cmd, ".mod ", 5) == 0 || 
       (strncasecmp(ua->cmd, "mod ", 4) == 0 && strlen(ua->cmd) > 6)) {
      parse_ua_args(ua);
      rc.mod = true;
      if (!scan_command_line_arguments(ua, rc)) {
         return 0;
      }
      goto try_again;
   }

   /* Allow the user to modify the settings */
   status = modify_job_parameters(ua, jcr, rc);
   switch (status) {
   case 0:
      goto try_again;
   case 1:
      break;
   case -1:
      goto bail_out;
   }


   if (ua->cmd[0] == 0 || strncasecmp(ua->cmd, _("yes"), strlen(ua->cmd)) == 0) {
      JobId_t JobId;
      Dmsg1(800, "Calling run_job job=%x\n", jcr->job);

start_job:
      Dmsg3(100, "JobId=%u using pool %s priority=%d\n", (int)jcr->JobId, 
            jcr->pool->name(), jcr->JobPriority);
      Dmsg1(900, "Running a job; its spool_data = %d\n", jcr->spool_data);
      JobId = run_job(jcr);
      Dmsg4(100, "JobId=%u NewJobId=%d using pool %s priority=%d\n", (int)jcr->JobId, 
            JobId, jcr->pool->name(), jcr->JobPriority);
      free_jcr(jcr);                  /* release jcr */
      if (JobId == 0) {
         ua->error_msg(_("Job failed.\n"));
      } else {
         char ed1[50];
         ua->send_msg(_("Job queued. JobId=%s\n"), edit_int64(JobId, ed1));
      }
      return JobId;
   }
   if (strncasecmp(ua->cmd, _("no"), strlen(ua->cmd)) == 0) {
      goto bail_out;
   }
   ua->send_msg(_("\nBad response: %s. You must answer yes, mod, or no.\n\n"), ua->cmd);
   goto try_again;

bail_out:
   ua->send_msg(_("Job not run.\n"));
   free_jcr(jcr);
   return 0;                       /* do not run */
}

int modify_job_parameters(UAContext *ua, JCR *jcr, run_ctx &rc)
{
   int i, opt;
   
   /*
    * At user request modify parameters of job to be run.
    */
   if (ua->cmd[0] != 0 && strncasecmp(ua->cmd, _("mod"), strlen(ua->cmd)) == 0) {
      FILE *fd;

      start_prompt(ua, _("Parameters to modify:\n"));
      add_prompt(ua, _("Level"));            /* 0 */
      add_prompt(ua, _("Storage"));          /* 1 */
      add_prompt(ua, _("Job"));              /* 2 */
      add_prompt(ua, _("FileSet"));          /* 3 */
      if (jcr->getJobType() == JT_RESTORE) {
         add_prompt(ua, _("Restore Client"));   /* 4 */
      } else {
         add_prompt(ua, _("Client"));        /* 4 */
      }
      add_prompt(ua, _("When"));             /* 5 */
      add_prompt(ua, _("Priority"));         /* 6 */
      if (jcr->getJobType() == JT_BACKUP ||
          jcr->getJobType() == JT_COPY ||
          jcr->getJobType() == JT_MIGRATE ||
          jcr->getJobType() == JT_VERIFY) {
         add_prompt(ua, _("Pool"));          /* 7 */
         if (jcr->getJobType() == JT_VERIFY) {
            add_prompt(ua, _("Verify Job"));  /* 8 */
         }
      } else if (jcr->getJobType() == JT_RESTORE) {
         add_prompt(ua, _("Bootstrap"));     /* 7 */
         add_prompt(ua, _("Where"));         /* 8 */
         add_prompt(ua, _("File Relocation"));/* 9 */    
         add_prompt(ua, _("Replace"));       /* 10 */
         add_prompt(ua, _("JobId"));         /* 11 */
      }
      if (jcr->getJobType() == JT_BACKUP || jcr->getJobType() == JT_RESTORE) {
         add_prompt(ua, _("Plugin Options")); /* 12 */
      }
      switch (do_prompt(ua, "", _("Select parameter to modify"), NULL, 0)) {
      case 0:
         /* Level */
         select_job_level(ua, jcr);
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
            jcr->job = rc.job;
            set_jcr_defaults(jcr, rc.job);
            goto try_again;
         }
         break;
      case 3:
         /* FileSet */
         rc.fileset = select_fileset_resource(ua);
         if (rc.fileset) {
            jcr->fileset = rc.fileset;
            goto try_again;
         }
         break;
      case 4:
         /* Client */
         rc.client = select_client_resource(ua);
         if (rc.client) {
            jcr->client = rc.client;
            goto try_again;
         }
         break;
      case 5:
         /* When */
         if (!get_cmd(ua, _("Please enter desired start time as YYYY-MM-DD HH:MM:SS (return for now): "))) {
            break;
         }
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
      case 6:
         /* Priority */
         if (!get_pint(ua, _("Enter new Priority: "))) {
            break;
         }
         if (ua->pint32_val == 0) {
            ua->send_msg(_("Priority must be a positive integer.\n"));
         } else {
            jcr->JobPriority = ua->pint32_val;
         }
         goto try_again;
      case 7: 
         /* Pool or Bootstrap depending on JobType */
         if (jcr->getJobType() == JT_BACKUP ||
             jcr->getJobType() == JT_COPY ||
             jcr->getJobType() == JT_MIGRATE ||
             jcr->getJobType() == JT_VERIFY) {      /* Pool */
            rc.pool = select_pool_resource(ua);
            if (rc.pool) {
               jcr->pool = rc.pool;
               Dmsg1(100, "Set new pool=%s\n", jcr->pool->name());
               goto try_again;
            }
            break;
         }

         /* Bootstrap */
         if (!get_cmd(ua, _("Please enter the Bootstrap file name: "))) {
            break;
         }
         if (jcr->RestoreBootstrap) {
            free(jcr->RestoreBootstrap);
            jcr->RestoreBootstrap = NULL;
         }
         if (ua->cmd[0] != 0) {
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
      case 8:
         /* Verify Job */
         if (jcr->getJobType() == JT_VERIFY) {
            rc.verify_job = select_job_resource(ua);
            if (rc.verify_job) {
              jcr->verify_job = rc.verify_job;
            }
            goto try_again;
         }
         /* Where */
         if (!get_cmd(ua, _("Please enter the full path prefix for restore (/ for none): "))) {
            break;
         }
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
      case 9: 
         /* File relocation */
         select_where_regexp(ua, jcr);
         goto try_again;
      case 10:
         /* Replace */
         start_prompt(ua, _("Replace:\n"));
         for (i=0; ReplaceOptions[i].name; i++) {
            add_prompt(ua, ReplaceOptions[i].name);
         }
         opt = do_prompt(ua, "", _("Select replace option"), NULL, 0);
         if (opt >=  0) {
            rc.replace = ReplaceOptions[opt].name;
            jcr->replace = ReplaceOptions[opt].token;
         }
         goto try_again;
      case 11:
         /* JobId */
         rc.jid = NULL;                  /* force reprompt */
         jcr->RestoreJobId = 0;
         if (jcr->RestoreBootstrap) {
            ua->send_msg(_("You must set the bootstrap file to NULL to be able to specify a JobId.\n"));
         }
         goto try_again;
      case 12:        
         /* Plugin Options */
         if (!get_cmd(ua, _("Please Plugin Options string: "))) {
            break;
         }
         if (jcr->plugin_options) {
            free(jcr->plugin_options);
            jcr->plugin_options = NULL;
         }
         jcr->plugin_options = bstrdup(ua->cmd);
         goto try_again;
      case -1:                        /* error or cancel */
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

/*
 * Reset the restore context. 
 * This subroutine can be called multiple times, so it
 *  must keep any prior settings.
 */
static bool reset_restore_context(UAContext *ua, JCR *jcr, run_ctx &rc)
{
   int i;

   jcr->verify_job = rc.verify_job;
   jcr->previous_job = rc.previous_job;
   jcr->pool = rc.pool;
   if (jcr->pool != jcr->job->pool) {
      pm_strcpy(jcr->pool_source, _("User input"));
   }
   set_rwstorage(jcr, rc.store);
   jcr->client = rc.client;
   pm_strcpy(jcr->client_name, rc.client->name());
   jcr->fileset = rc.fileset;
   jcr->ExpectedFiles = rc.files;
   if (rc.catalog) {
      jcr->catalog = rc.catalog;
      pm_strcpy(jcr->catalog_source, _("User input"));
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
      for (i=0; ReplaceOptions[i].name; i++) {
         if (strcasecmp(rc.replace, ReplaceOptions[i].name) == 0) {
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

   /* If pool changed, update migration write storage */
   if (jcr->is_JobType(JT_MIGRATE) || jcr->is_JobType(JT_COPY) ||
      (jcr->is_JobType(JT_BACKUP) && jcr->is_JobLevel(L_VIRTUAL_FULL))) {
      if (!set_migration_wstorage(jcr, rc.pool)) {
         return false;
      }
   }
   rc.replace = ReplaceOptions[0].name;
   for (i=0; ReplaceOptions[i].name; i++) {
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
      /* Note, this is also MigrateJobId and a VerifyJobId */
      jcr->RestoreJobId = str_to_int64(rc.jid);
      rc.jid = 0;
   }

   /* Some options are not available through the menu
    * TODO: Add an advanced menu?
    */
   if (rc.spool_data_set) {
      jcr->spool_data = rc.spool_data;
   }

   if (rc.accurate_set) {
      jcr->accurate = rc.accurate;
   }

   /* Used by migration jobs that can have the same name,
    * but can run at the same time
    */
   if (rc.ignoreduplicatecheck_set) {
      jcr->IgnoreDuplicateJobChecking = rc.ignoreduplicatecheck;
   }

   return true;
}

static void select_where_regexp(UAContext *ua, JCR *jcr)
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
   if (strip_prefix) bfree(strip_prefix);
   if (add_prefix)   bfree(add_prefix);
   if (add_suffix)   bfree(add_suffix);
   if (rwhere)       bfree(rwhere);
}

static void select_job_level(UAContext *ua, JCR *jcr)
{
   if (jcr->getJobType() == JT_BACKUP) {
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
   } else if (jcr->getJobType() == JT_VERIFY) {
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

static bool display_job_parameters(UAContext *ua, JCR *jcr, JOB *job, const char *verify_list,
   char *jid, const char *replace, char *client_name) 
{
   char ec1[30];
   char dt[MAX_TIME_LENGTH];

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
                 jcr->fileset->name(),
                 NPRT(jcr->client->name()),
                 jcr->wstore?jcr->wstore->name():"*None*",
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
                 jcr->fileset->name(),
                 NPRT(jcr->client->name()),
                 jcr->wstore?jcr->wstore->name():"*None*",
                 bstrutime(dt, sizeof(dt), jcr->sched_time),
                 jcr->JobPriority);
      }
      jcr->setJobLevel(L_FULL);
      break;
   case JT_BACKUP:
   case JT_VERIFY:
      if (jcr->getJobType() == JT_BACKUP) {
         if (ua->api) {
            ua->signal(BNET_RUN_CMD);   
            ua->send_msg("Type: Backup\n"
                        "Title: Run Backup Job\n"
                        "JobName:  %s\n"
                        "Level:    %s\n"
                        "Client:   %s\n"
                        "FileSet:  %s\n"
                        "Pool:     %s\n"
                        "Storage:  %s\n"
                        "When:     %s\n"
                        "Priority: %d\n"
                        "%s%s%s",
                 job->name(),
                 level_to_str(jcr->getJobLevel()),
                 jcr->client->name(),
                 jcr->fileset->name(),
                 NPRT(jcr->pool->name()),
                 jcr->wstore?jcr->wstore->name():"*None*",
                 bstrutime(dt, sizeof(dt), jcr->sched_time),
                 jcr->JobPriority,
                 jcr->plugin_options?"Plugin Options: ":"",
                 jcr->plugin_options?jcr->plugin_options:"",
                 jcr->plugin_options?"\n":"");
         } else {
            ua->send_msg(_("Run Backup job\n"
                        "JobName:  %s\n"
                        "Level:    %s\n"
                        "Client:   %s\n"
                        "FileSet:  %s\n"
                        "Pool:     %s (From %s)\n"
                        "Storage:  %s (From %s)\n"
                        "When:     %s\n"
                        "Priority: %d\n"
                        "%s%s%s"),
                 job->name(),
                 level_to_str(jcr->getJobLevel()),
                 jcr->client->name(),
                 jcr->fileset->name(),
                 NPRT(jcr->pool->name()), jcr->pool_source,
                 jcr->wstore?jcr->wstore->name():"*None*", jcr->wstore_source,
                 bstrutime(dt, sizeof(dt), jcr->sched_time),
                 jcr->JobPriority,
                 jcr->plugin_options?"Plugin Options: ":"",
                 jcr->plugin_options?jcr->plugin_options:"",
                 jcr->plugin_options?"\n":"");
         }
      } else {  /* JT_VERIFY */
         JOB_DBR jr;
         const char *Name;
         if (jcr->verify_job) {
            Name = jcr->verify_job->name();
         } else if (jcr->RestoreJobId) { /* Display job name if jobid requested */
            memset(&jr, 0, sizeof(jr));
            jr.JobId = jcr->RestoreJobId;
            if (!db_get_job_record(jcr, ua->db, &jr)) {
               ua->error_msg(_("Could not get job record for selected JobId. ERR=%s"),
                    db_strerror(ua->db));
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
              jcr->client->name(),
              jcr->fileset->name(),
              NPRT(jcr->pool->name()), jcr->pool_source,
              jcr->rstore->name(), jcr->rstore_source,
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
              jcr->client->name(),
              jcr->fileset->name(),
              NPRT(jcr->pool->name()), jcr->pool_source,
              jcr->rstore->name(), jcr->rstore_source,
              Name,
              verify_list,
              bstrutime(dt, sizeof(dt), jcr->sched_time),
              jcr->JobPriority);
         }
      }
      break;
   case JT_RESTORE:
      if (jcr->RestoreJobId == 0 && !jcr->RestoreBootstrap) {
         if (jid) {
            jcr->RestoreJobId = str_to_int64(jid);
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
                        "Storage:         %s\n"
                        "When:            %s\n"
                        "Catalog:         %s\n"
                        "Priority:        %d\n"
                        "Plugin Options:  %s\n",
                 job->name(),
                 NPRT(jcr->RestoreBootstrap), 
                 jcr->RegexWhere?jcr->RegexWhere:job->RegexWhere,
                 replace,
                 jcr->fileset->name(),
                 client_name,
                 jcr->client->name(),
                 jcr->rstore->name(),
                 bstrutime(dt, sizeof(dt), jcr->sched_time),
                 jcr->catalog->name(),
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
                        "Storage:         %s\n"
                        "When:            %s\n"
                        "Catalog:         %s\n"
                        "Priority:        %d\n"
                        "Plugin Options:  %s\n"),
                 job->name(),
                 NPRT(jcr->RestoreBootstrap), 
                 jcr->RegexWhere?jcr->RegexWhere:job->RegexWhere,
                 replace,
                 jcr->fileset->name(),
                 client_name,
                 jcr->client->name(),
                 jcr->rstore->name(),
                 bstrutime(dt, sizeof(dt), jcr->sched_time),
                 jcr->catalog->name(),
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
                        "Storage:         %s\n"
                        "When:            %s\n"
                        "Catalog:         %s\n"
                        "Priority:        %d\n"
                        "Plugin Options:  %s\n",
                 job->name(),
                 NPRT(jcr->RestoreBootstrap), 
                 jcr->where?jcr->where:NPRT(job->RestoreWhere), 
                 replace,
                 jcr->fileset->name(),
                 client_name,
                 jcr->client->name(),
                 jcr->rstore->name(),
                 bstrutime(dt, sizeof(dt), jcr->sched_time),
                 jcr->catalog->name(),
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
                        "Storage:         %s\n"
                        "When:            %s\n"
                        "Catalog:         %s\n"
                        "Priority:        %d\n"
                        "Plugin Options:  %s\n"),
                 job->name(),
                 NPRT(jcr->RestoreBootstrap), 
                 jcr->where?jcr->where:NPRT(job->RestoreWhere), 
                 replace,
                 jcr->fileset->name(),
                 client_name,
                 jcr->client->name(),
                 jcr->rstore->name(),
                 bstrutime(dt, sizeof(dt), jcr->sched_time),
                 jcr->catalog->name(),
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
                         jcr->RegexWhere?jcr->RegexWhere:job->RegexWhere);
         } else {
            ua->send_msg(_("Where:      %s\n"),
                         jcr->where?jcr->where:NPRT(job->RestoreWhere));
         }

         ua->send_msg(_("Replace:         %s\n"
                        "Client:          %s\n"
                        "Storage:         %s\n"
                        "JobId:           %s\n"
                        "When:            %s\n"
                        "Catalog:         %s\n"
                        "Priority:        %d\n"
                        "Plugin Options:  %s\n"),
              replace,
              jcr->client->name(),
              jcr->rstore->name(),
              jcr->RestoreJobId==0?"*None*":edit_uint64(jcr->RestoreJobId, ec1),
              bstrutime(dt, sizeof(dt), jcr->sched_time),
              jcr->catalog->name(),
              jcr->JobPriority,
              NPRT(jcr->plugin_options));
      }
      break;
   case JT_COPY:
   case JT_MIGRATE:
      char *prt_type;
      jcr->setJobLevel(L_FULL);      /* default level */
      if (ua->api) {
         ua->signal(BNET_RUN_CMD);
         if (jcr->getJobType() == JT_COPY) {
            prt_type = (char *)"Type: Copy\nTitle: Run Copy Job\n";
         } else {
            prt_type = (char *)"Type: Migration\nTitle: Run Migration Job\n";
         }
         ua->send_msg("%s"
                     "JobName:       %s\n"
                     "Bootstrap:     %s\n"
                     "Client:        %s\n"
                     "FileSet:       %s\n"
                     "Pool:          %s\n"
                     "Read Storage:  %s\n"
                     "Write Storage: %s\n"
                     "JobId:         %s\n"
                     "When:          %s\n"
                     "Catalog:       %s\n"
                     "Priority:      %d\n",
           prt_type,
           job->name(),
           NPRT(jcr->RestoreBootstrap),
           jcr->client->name(),
           jcr->fileset->name(),
           NPRT(jcr->pool->name()),
           jcr->rstore->name(),
           jcr->wstore?jcr->wstore->name():"*None*",
           jcr->MigrateJobId==0?"*None*":edit_uint64(jcr->MigrateJobId, ec1),
           bstrutime(dt, sizeof(dt), jcr->sched_time),
           jcr->catalog->name(),
           jcr->JobPriority);
      } else {
         if (jcr->getJobType() == JT_COPY) {
            prt_type = _("Run Copy job\n");
         } else {
            prt_type = _("Run Migration job\n");
         }
         ua->send_msg("%s"
                     "JobName:       %s\n"
                     "Bootstrap:     %s\n"
                     "Client:        %s\n"
                     "FileSet:       %s\n"
                     "Pool:          %s (From %s)\n"
                     "Read Storage:  %s (From %s)\n"
                     "Write Storage: %s (From %s)\n"
                     "JobId:         %s\n"
                     "When:          %s\n"
                     "Catalog:       %s\n"
                     "Priority:      %d\n",
           prt_type,
           job->name(),
           NPRT(jcr->RestoreBootstrap),
           jcr->client->name(),
           jcr->fileset->name(),
           NPRT(jcr->pool->name()), jcr->pool_source,
           jcr->rstore->name(), jcr->rstore_source,
           jcr->wstore?jcr->wstore->name():"*None*", jcr->wstore_source,
           jcr->MigrateJobId==0?"*None*":edit_uint64(jcr->MigrateJobId, ec1),
           bstrutime(dt, sizeof(dt), jcr->sched_time),
           jcr->catalog->name(),
           jcr->JobPriority);
      }
      break;
   default:
      ua->error_msg(_("Unknown Job Type=%d\n"), jcr->getJobType());
      return false;
   }
   return true;
}


static bool scan_command_line_arguments(UAContext *ua, run_ctx &rc)
{
   bool kw_ok;
   int i, j;
   static const char *kw[] = {        /* command line arguments */
      "job",                          /*  Used in a switch() */
      "jobid",                        /* 1 */
      "client",                       /* 2 */
      "fd",                           /* 3 */
      "fileset",                      /* 4 */
      "level",                        /* 5 */
      "storage",                      /* 6 */
      "sd",                           /* 7 */
      "regexwhere",                   /* 8 where string as a bregexp */
      "where",                        /* 9 */
      "bootstrap",                    /* 10 */
      "replace",                      /* 11 */
      "when",                         /* 12 */
      "priority",                     /* 13 */
      "yes",                          /* 14  -- if you change this change YES_POS too */
      "verifyjob",                    /* 15 */
      "files",                        /* 16 number of files to restore */
      "catalog",                      /* 17 override catalog */
      "since",                        /* 18 since */
      "cloned",                       /* 19 cloned */
      "verifylist",                   /* 20 verify output list */
      "migrationjob",                 /* 21 migration job name */
      "pool",                         /* 22 */
      "backupclient",                 /* 23 */
      "restoreclient",                /* 24 */
      "pluginoptions",                /* 25 */
      "spooldata",                    /* 26 */
      "comment",                      /* 27 */
      "ignoreduplicatecheck",         /* 28 */
      "accurate",                     /* 29 */
      NULL
   };

#define YES_POS 14

   rc.catalog_name = NULL;
   rc.job_name = NULL;
   rc.pool_name = NULL;
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

   for (i=1; i<ua->argc; i++) {
      Dmsg2(800, "Doing arg %d = %s\n", i, ua->argk[i]);
      kw_ok = false;
      /* Keep looking until we find a good keyword */
      for (j=0; !kw_ok && kw[j]; j++) {
         if (strcasecmp(ua->argk[i], kw[j]) == 0) {
            /* Note, yes and run have no value, so do not fail */
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
               if (!acl_access_ok(ua, Where_ACL, rc.regexwhere)) {
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
               if (!acl_access_ok(ua, Where_ACL, rc.where)) {
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
            case 23: /* backupclient */
               if (rc.client_name) {
                  ua->send_msg(_("Client specified twice.\n"));
                  return 0;
               }
               rc.client_name = ua->argv[i];
               kw_ok = true;
               break;
            case 24: /* restoreclient */
               if (rc.restore_client_name && !rc.mod) {
                  ua->send_msg(_("Restore Client specified twice.\n"));
                  return false;
               }
               rc.restore_client_name = ua->argv[i];
               kw_ok = true;
               break;
            case 25: /* pluginoptions */
               ua->send_msg(_("Plugin Options not yet implemented.\n"));
               return false;
               if (rc.plugin_options) {
                  ua->send_msg(_("Plugin Options specified twice.\n"));
                  return false;
               }
               rc.plugin_options = ua->argv[i];
               if (!acl_access_ok(ua, PluginOptions_ACL, rc.plugin_options)) {
                  ua->send_msg(_("No authoriztion for \"PluginOptions\" specification.\n"));
                  return false;
               }
               kw_ok = true;
               break;
            case 26: /* spooldata */
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
            case 27: /* comment */
               rc.comment = ua->argv[i];
               kw_ok = true;
               break;
            case 28: /* ignoreduplicatecheck */
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
            case 29: /* accurate */
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
       rc.catalog = GetCatalogResWithName(rc.catalog_name);
       if (rc.catalog == NULL) {
            ua->error_msg(_("Catalog \"%s\" not found\n"), rc.catalog_name);
           return false;
       }
       if (!acl_access_ok(ua, Catalog_ACL, rc.catalog->name())) {
          ua->error_msg(_("No authorization. Catalog \"%s\".\n"), rc.catalog->name());
          return false;
       }
   }
   Dmsg1(800, "Using catalog=%s\n", NPRT(rc.catalog_name));

   if (rc.job_name) {
      /* Find Job */
      rc.job = GetJobResWithName(rc.job_name);
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
   } else if (!acl_access_ok(ua, Job_ACL, rc.job->name())) {
      ua->error_msg( _("No authorization. Job \"%s\".\n"), rc.job->name());
      return false;
   }

   if (rc.pool_name) {
      rc.pool = GetPoolResWithName(rc.pool_name);
      if (!rc.pool) {
         if (*rc.pool_name != 0) {
            ua->warning_msg(_("Pool \"%s\" not found.\n"), rc.pool_name);
         }
         rc.pool = select_pool_resource(ua);
      }
   } else if (!rc.pool) {
      rc.pool = rc.job->pool;             /* use default */
   }
   if (!rc.pool) {
      return false;
   } else if (!acl_access_ok(ua, Pool_ACL, rc.pool->name())) {
      ua->error_msg(_("No authorization. Pool \"%s\".\n"), rc.pool->name());
      return false;
   }
   Dmsg1(100, "Using pool %s\n", rc.pool->name());

   if (rc.store_name) {
      rc.store->store = GetStoreResWithName(rc.store_name);
      pm_strcpy(rc.store->store_source, _("command line"));
      if (!rc.store->store) {
         if (*rc.store_name != 0) {
            ua->warning_msg(_("Storage \"%s\" not found.\n"), rc.store_name);
         }
         rc.store->store = select_storage_resource(ua);
         pm_strcpy(rc.store->store_source, _("user selection"));
      }
   } else if (!rc.store->store) {
      get_job_storage(rc.store, rc.job, NULL);      /* use default */
   }
   if (!rc.store->store) {
      ua->error_msg(_("No storage specified.\n"));
      return false;
   } else if (!acl_access_ok(ua, Storage_ACL, rc.store->store->name())) {
      ua->error_msg(_("No authorization. Storage \"%s\".\n"),
               rc.store->store->name());
      return false;
   }
   Dmsg1(800, "Using storage=%s\n", rc.store->store->name());

   if (rc.client_name) {
      rc.client = GetClientResWithName(rc.client_name);
      if (!rc.client) {
         if (*rc.client_name != 0) {
            ua->warning_msg(_("Client \"%s\" not found.\n"), rc.client_name);
         }
         rc.client = select_client_resource(ua);
      }
   } else if (!rc.client) {
      rc.client = rc.job->client;           /* use default */
   }
   if (!rc.client) {
      return false;
   } else if (!acl_access_ok(ua, Client_ACL, rc.client->name())) {
      ua->error_msg(_("No authorization. Client \"%s\".\n"),
               rc.client->name());
      return false;
   }
   Dmsg1(800, "Using client=%s\n", rc.client->name());

   if (rc.restore_client_name) {
      rc.client = GetClientResWithName(rc.restore_client_name);
      if (!rc.client) {
         if (*rc.restore_client_name != 0) {
            ua->warning_msg(_("Restore Client \"%s\" not found.\n"), rc.restore_client_name);
         }
         rc.client = select_client_resource(ua);
      }
   } else if (!rc.client) {
      rc.client = rc.job->client;           /* use default */
   }
   if (!rc.client) {
      return false;
   } else if (!acl_access_ok(ua, Client_ACL, rc.client->name())) {
      ua->error_msg(_("No authorization. Client \"%s\".\n"),
               rc.client->name());
      return false;
   }
   Dmsg1(800, "Using restore client=%s\n", rc.client->name());

   if (rc.fileset_name) {
      rc.fileset = GetFileSetResWithName(rc.fileset_name);
      if (!rc.fileset) {
         ua->send_msg(_("FileSet \"%s\" not found.\n"), rc.fileset_name);
         rc.fileset = select_fileset_resource(ua);
      }
   } else if (!rc.fileset) {
      rc.fileset = rc.job->fileset;           /* use default */
   }
   if (!rc.fileset) {
      return false;
   } else if (!acl_access_ok(ua, FileSet_ACL, rc.fileset->name())) {
      ua->send_msg(_("No authorization. FileSet \"%s\".\n"),
               rc.fileset->name());
      return false;
   }

   if (rc.verify_job_name) {
      rc.verify_job = GetJobResWithName(rc.verify_job_name);
      if (!rc.verify_job) {
         ua->send_msg(_("Verify Job \"%s\" not found.\n"), rc.verify_job_name);
         rc.verify_job = select_job_resource(ua);
      }
   } else if (!rc.verify_job) {
      rc.verify_job = rc.job->verify_job;
   }

   if (rc.previous_job_name) {
      rc.previous_job = GetJobResWithName(rc.previous_job_name);
      if (!rc.previous_job) {
         ua->send_msg(_("Migration Job \"%s\" not found.\n"), rc.previous_job_name);
         rc.previous_job = select_job_resource(ua);
      }
   } else {
      rc.previous_job = rc.job->verify_job;
   }
   return true;
}
