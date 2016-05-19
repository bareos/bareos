/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2013 Bareos GmbH & Co. KG

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
 * This file handles accepting Director Commands
 *
 * Kern Sibbald, October MM
 */

#include "bareos.h"
#include "filed.h"
#include "ch.h"

#if defined(WIN32_VSS)
#include "vss.h"

static pthread_mutex_t vss_mutex = PTHREAD_MUTEX_INITIALIZER;
static bool enable_vss = false;
#endif

extern bool backup_only_mode;
extern bool restore_only_mode;

/**
 * As Windows saves ACLs as part of the standard backup stream
 * we just pretend here that is has implicit acl support.
 */
#if defined(HAVE_ACL) || defined(HAVE_WIN32)
const bool have_acl = true;
#else
const bool have_acl = false;
#endif

#if defined(HAVE_XATTR)
const bool have_xattr = true;
#else
const bool have_xattr = false;
#endif

/* Imported functions */
extern bool accurate_cmd(JCR *jcr);
extern bool status_cmd(JCR *jcr);
extern bool qstatus_cmd(JCR *jcr);
extern "C" char *job_code_callback_filed(JCR *jcr, const char* param);

/* Forward referenced functions */
static bool backup_cmd(JCR *jcr);
static bool bootstrap_cmd(JCR *jcr);
static bool cancel_cmd(JCR *jcr);
static bool end_restore_cmd(JCR *jcr);
static bool estimate_cmd(JCR *jcr);
#ifdef DEVELOPER
static bool exit_cmd(JCR *jcr);
#endif
static bool fileset_cmd(JCR *jcr);
static bool job_cmd(JCR *jcr);
static bool level_cmd(JCR *jcr);
static bool pluginoptions_cmd(JCR *jcr);
static bool runafter_cmd(JCR *jcr);
static bool runbeforenow_cmd(JCR *jcr);
static bool runbefore_cmd(JCR *jcr);
static bool runscript_cmd(JCR *jcr);
static bool resolve_cmd(JCR *jcr);
static bool restore_object_cmd(JCR *jcr);
static bool restore_cmd(JCR *jcr);
static bool session_cmd(JCR *jcr);
static bool setauthorization_cmd(JCR *jcr);
static bool setbandwidth_cmd(JCR *jcr);
static bool setdebug_cmd(JCR *jcr);
static bool storage_cmd(JCR *jcr);
static bool sm_dump_cmd(JCR *jcr);
static bool verify_cmd(JCR *jcr);

static bool response(JCR *jcr, BSOCK *sd, char *resp, const char *cmd);
static void filed_free_jcr(JCR *jcr);
static bool open_sd_read_session(JCR *jcr);
static void set_storage_auth_key(JCR *jcr, char *key);

/* Exported functions */

struct s_cmds {
   const char *cmd;
   bool (*func)(JCR *);
   bool monitoraccess; /* specify if monitors have access to this function */
};

/**
 * The following are the recognized commands from the Director.
 *
 * Keywords are sorted first longest match when the keywords start with the same string.
 */
static struct s_cmds cmds[] = {
   { "accurate", accurate_cmd, false },
   { "backup", backup_cmd, false },
   { "bootstrap", bootstrap_cmd, false },
   { "cancel", cancel_cmd, false },
   { "endrestore", end_restore_cmd, false },
   { "estimate", estimate_cmd, false },
#ifdef DEVELOPER
   { "exit", exit_cmd, false },
#endif
   { "fileset", fileset_cmd, false },
   { "JobId=", job_cmd, false },
   { "level = ", level_cmd, false },
   { "pluginoptions", pluginoptions_cmd, false },
   { "RunAfterJob", runafter_cmd, false },
   { "RunBeforeNow", runbeforenow_cmd, false },
   { "RunBeforeJob", runbefore_cmd, false },
   { "Run", runscript_cmd, false },
   { "restoreobject", restore_object_cmd, false },
   { "restore ", restore_cmd, false },
   { "resolve ", resolve_cmd, false },
   { "session", session_cmd, false },
   { "setauthorization", setauthorization_cmd, false },
   { "setbandwidth=", setbandwidth_cmd, false },
   { "setdebug=", setdebug_cmd, false },
   { "status", status_cmd, true },
   { ".status", qstatus_cmd, true },
   { "storage ", storage_cmd, false },
   { "sm_dump", sm_dump_cmd, false },
   { "verify", verify_cmd, false },
   { NULL, NULL, false } /* list terminator */
};

/*
 * Commands received from director that need scanning
 */
static char setauthorizationcmd[] =
   "setauthorization Authorization=%100s";
static char setbandwidthcmd[] =
   "setbandwidth=%lld Job=%127s";
static char setdebugv0cmd[] =
    "setdebug=%d trace=%d";
static char setdebugv1cmd[] =
   "setdebug=%d trace=%d hangup=%d";
static char setdebugv2cmd[] =
   "setdebug=%d trace=%d hangup=%d timestamp=%d";
static char jobcmd[] =
   "JobId=%d Job=%127s SDid=%d SDtime=%d Authorization=%100s";
static char storaddrv0cmd[] =
   "storage address=%s port=%d ssl=%d";
static char storaddrv1cmd[] =
   "storage address=%s port=%d ssl=%d Authorization=%100s";
static char sessioncmd[] =
   "session %127s %ld %ld %ld %ld %ld %ld\n";
static char restorecmd[] =
   "restore replace=%c prelinks=%d where=%s\n";
static char restorecmd1[] =
   "restore replace=%c prelinks=%d where=\n";
static char restorecmdR[] =
   "restore replace=%c prelinks=%d regexwhere=%s\n";
static char restoreobjcmd[] =
   "restoreobject JobId=%u %d,%d,%d,%d,%d,%d,%s";
static char restoreobjcmd1[] =
   "restoreobject JobId=%u %d,%d,%d,%d,%d,%d\n";
static char endrestoreobjectcmd[] =
   "restoreobject end\n";
static char pluginoptionscmd[] =
   "pluginoptions %s";
static char verifycmd[] =
   "verify level=%30s";
static char estimatecmd[] =
   "estimate listing=%d";
static char runbeforecmd[] =
   "RunBeforeJob %s";
static char runaftercmd[] =
   "RunAfterJob %s";
static char runscriptcmd[] =
   "Run OnSuccess=%d OnFailure=%d AbortOnError=%d When=%d Command=%s";
static char resolvecmd[] =
   "resolve %s";

/*
 * Responses sent to Director
 */
static char errmsg[] =
   "2999 Invalid command\n";
static char invalid_cmd[] =
   "2997 Invalid command for a Director with Monitor directive enabled.\n";
static char OkAuthorization[] =
   "2000 OK Authorization\n";
static char OKBandwidth[] =
   "2000 OK Bandwidth\n";
static char OKinc[] =
   "2000 OK include\n";
static char OKest[] =
   "2000 OK estimate files=%s bytes=%s\n";
static char OKlevel[] =
   "2000 OK level\n";
static char OKbackup[] =
   "2000 OK backup\n";
static char OKbootstrap[] =
   "2000 OK bootstrap\n";
static char OKverify[] =
   "2000 OK verify\n";
static char OKrestore[] =
   "2000 OK restore\n";
static char OKsession[] =
   "2000 OK session\n";
static char OKstore[] =
   "2000 OK storage\n";
static char OKstoreend[] =
   "2000 OK storage end\n";
static char OKjob[] =
#if !defined(IS_BUILD_ON_OBS)
   "2000 OK Job %s (%s) %s,%s,%s";
#else
   "2000 OK Job %s (%s) %s,%s,%s,%s,%s";
#endif
static char OKsetdebugv0[] =
   "2000 OK setdebug=%d trace=%d hangup=%d tracefile=%s\n";
static char OKsetdebugv1[] =
   "2000 OK setdebug=%d trace=%d hangup=%d timestamp=%d tracefile=%s\n";
static char BADjob[] =
   "2901 Bad Job\n";
static char EndJob[] =
   "2800 End Job TermCode=%d JobFiles=%u ReadBytes=%s"
   " JobBytes=%s Errors=%u VSS=%d Encrypt=%d\n";
static char OKRunBefore[] =
   "2000 OK RunBefore\n";
static char OKRunBeforeNow[] =
   "2000 OK RunBeforeNow\n";
static char OKRunAfter[] =
   "2000 OK RunAfter\n";
static char OKRunScript[] =
   "2000 OK RunScript\n";
static char BadRunBeforeJob[] =
   "2905 Bad RunBeforeJob command.\n";
static char FailedRunScript[] =
   "2905 Failed RunScript\n";
static char BadRunAfterJob[] =
   "2905 Bad RunAfterJob command.\n";
static char BADcmd[] =
   "2902 Bad %s\n";
static char OKRestoreObject[] =
   "2000 OK ObjectRestored\n";
static char OKPluginOptions[] =
   "2000 OK PluginOptions\n";
static char BadPluginOptions[] =
   "2905 Bad PluginOptions command.\n";

/*
 * Responses received from Storage Daemon
 */
static char OK_end[] =
   "3000 OK end\n";
static char OK_close[] =
   "3000 OK close Status = %d\n";
static char OK_open[] =
   "3000 OK open ticket = %d\n";
static char OK_data[] =
   "3000 OK data\n";
static char OK_append[] =
   "3000 OK append data\n";

/*
 * Commands sent to Storage Daemon
 */
static char append_open[] =
   "append open session\n";
static char append_data[] =
   "append data %d\n";
static char append_end[] =
   "append end session %d\n";
static char append_close[] =
   "append close session %d\n";
static char read_open[] =
   "read open session = %s %ld %ld %ld %ld %ld %ld\n";
static char read_data[] =
   "read data %d\n";
static char read_close[] =
   "read close session %d\n";

/*
 * See if we are allowed to execute the command issued.
 */
static bool validate_command(JCR *jcr, const char *cmd, alist *allowed_job_cmds)
{
   char *allowed_job_cmd;
   bool allowed = false;

   /*
    * If there is no explicit list of allowed cmds allow all cmds.
    */
   if (!allowed_job_cmds) {
      return true;
   }

   foreach_alist(allowed_job_cmd, allowed_job_cmds) {
      if (bstrcasecmp(cmd, allowed_job_cmd)) {
         allowed = true;
         break;
      }
   }

   if (!allowed) {
      Jmsg(jcr, M_FATAL, 0, _("Illegal \"%s\" command not allowed by Allowed Job Cmds setting of this filed.\n"), cmd);
   }

   return allowed;
}

static inline void cleanup_fileset(JCR *jcr)
{
   findFILESET *fileset;
   findINCEXE *incexe;
   findFOPTS *fo;

   fileset = jcr->ff->fileset;
   if (fileset) {
      /*
       * Delete FileSet Include lists
       */
      for (int i = 0; i < fileset->include_list.size(); i++) {
         incexe = (findINCEXE *)fileset->include_list.get(i);
         for (int j = 0; j < incexe->opts_list.size(); j++) {
            fo = (findFOPTS *)incexe->opts_list.get(j);
            if (fo->plugin) {
               free(fo->plugin);
            }
            for (int k = 0; k<fo->regex.size(); k++) {
               regfree((regex_t *)fo->regex.get(k));
            }
            for (int k = 0; k<fo->regexdir.size(); k++) {
               regfree((regex_t *)fo->regexdir.get(k));
            }
            for (int k = 0; k<fo->regexfile.size(); k++) {
               regfree((regex_t *)fo->regexfile.get(k));
            }
            if (fo->size_match) {
               free(fo->size_match);
            }
            fo->regex.destroy();
            fo->regexdir.destroy();
            fo->regexfile.destroy();
            fo->wild.destroy();
            fo->wilddir.destroy();
            fo->wildfile.destroy();
            fo->wildbase.destroy();
            fo->base.destroy();
            fo->fstype.destroy();
            fo->drivetype.destroy();
         }
         incexe->opts_list.destroy();
         incexe->name_list.destroy();
         incexe->plugin_list.destroy();
         incexe->ignoredir.destroy();
      }
      fileset->include_list.destroy();

      /*
       * Delete FileSet Exclude lists
       */
      for (int i = 0; i<fileset->exclude_list.size(); i++) {
         incexe = (findINCEXE *)fileset->exclude_list.get(i);
         for (int j = 0; j<incexe->opts_list.size(); j++) {
            fo = (findFOPTS *)incexe->opts_list.get(j);
            if (fo->size_match) {
               free(fo->size_match);
            }
            fo->regex.destroy();
            fo->regexdir.destroy();
            fo->regexfile.destroy();
            fo->wild.destroy();
            fo->wilddir.destroy();
            fo->wildfile.destroy();
            fo->wildbase.destroy();
            fo->base.destroy();
            fo->fstype.destroy();
            fo->drivetype.destroy();
         }
         incexe->opts_list.destroy();
         incexe->name_list.destroy();
         incexe->plugin_list.destroy();
         incexe->ignoredir.destroy();
      }
      fileset->exclude_list.destroy();
      free(fileset);
   }
   jcr->ff->fileset = NULL;
}

/*
 * Connection request from an director.
 *
 * Accept commands one at a time from the Director and execute them.
 *
 * Concerning ClientRunBefore/After, the sequence of events
 * is rather critical. If they are not done in the right
 * order one can easily get FD->SD timeouts if the script
 * runs a long time.
 *
 * The current sequence of events is:
 *  1. Dir starts job with FD
 *  2. Dir connects to SD
 *  3. Dir connects to FD
 *  4. FD connects to SD
 *  5. FD gets/runs ClientRunBeforeJob and sends ClientRunAfterJob
 *  6. Dir sends include/exclude
 *  7. FD sends data to SD
 *  8. SD/FD disconnects while SD despools data and attributes (optional)
 *  9. FD runs ClientRunAfterJob
 */
static void *handle_director_connection(BSOCK *dir)
{
   JCR *jcr;
   bool found;
   bool quit = false;
   const char jobname[12] = "*Director*";
// saveCWD save_cwd;

#ifdef HAVE_WIN32
   prevent_os_suspensions();
#endif

   jcr = new_jcr(sizeof(JCR), filed_free_jcr); /* create JCR */
   jcr->dir_bsock = dir;
   jcr->ff = init_find_files();
// save_cwd.save(jcr);
   jcr->start_time = time(NULL);
   jcr->RunScripts = New(alist(10, not_owned_by_alist));
   jcr->last_fname = get_pool_memory(PM_FNAME);
   jcr->last_fname[0] = 0;
   jcr->client_name = get_memory(strlen(my_name) + 1);
   pm_strcpy(jcr->client_name, my_name);
   bstrncpy(jcr->Job, jobname, sizeof(jobname));  /* dummy */
   jcr->crypto.pki_sign = me->pki_sign;
   jcr->crypto.pki_encrypt = me->pki_encrypt;
   jcr->crypto.pki_keypair = me->pki_keypair;
   jcr->crypto.pki_signers = me->pki_signers;
   jcr->crypto.pki_recipients = me->pki_recipients;
   dir->set_jcr(jcr);
   set_jcr_in_tsd(jcr);
   enable_backup_privileges(NULL, 1 /* ignore_errors */);

   Dmsg0(120, "Calling Authenticate\n");
   if (!authenticate_director(jcr)) {
      goto bail_out;
   }

   Dmsg0(120, "OK Authenticate\n");
   jcr->authenticated = true;

   /**********FIXME******* add command handler error code */

   while (!quit) {
      /*
       * Read command
       */
      if (dir->recv() < 0) {
         break;               /* connection terminated */
      }

      dir->msg[dir->msglen] = 0;
      Dmsg1(100, "<dird: %s", dir->msg);
      found = false;
      for (int i = 0; cmds[i].cmd; i++) {
         if (bstrncmp(cmds[i].cmd, dir->msg, strlen(cmds[i].cmd))) {
            found = true;         /* indicate command found */
            if ((!cmds[i].monitoraccess) && (jcr->director->monitor)) {
               Dmsg1(100, "Command \"%s\" is invalid.\n", cmds[i].cmd);
               dir->fsend(invalid_cmd);
               dir->signal(BNET_EOD);
               break;
            }
            Dmsg1(100, "Executing %s command.\n", cmds[i].cmd);
            if (!cmds[i].func(jcr)) {         /* do command */
               quit = true;         /* error or fully terminated, get out */
               Dmsg1(100, "Quit command loop. Canceled=%d\n", job_canceled(jcr));
            }
            break;
         }
      }
      if (!found) {              /* command not found */
         dir->fsend(errmsg);
         quit = true;
         break;
      }
   }

   /*
    * Inform Storage daemon that we are done
    */
   if (jcr->store_bsock) {
      jcr->store_bsock->signal(BNET_TERMINATE);
   }

   /*
    * Run the after job
    */
   if (jcr->RunScripts) {
      run_scripts(jcr, jcr->RunScripts, "ClientAfterJob",
                 (jcr->director && jcr->director->allowed_script_dirs) ?
                  jcr->director->allowed_script_dirs :
                  me->allowed_script_dirs);
   }

   if (jcr->JobId) {            /* send EndJob if running a job */
      char ed1[50], ed2[50];
      /* Send termination status back to Dir */
      dir->fsend(EndJob, jcr->JobStatus, jcr->JobFiles,
                 edit_uint64(jcr->ReadBytes, ed1),
                 edit_uint64(jcr->JobBytes, ed2), jcr->JobErrors, jcr->VSS,
                 jcr->crypto.pki_encrypt);
      Dmsg1(110, "End FD msg: %s\n", dir->msg);
   }

bail_out:
   generate_plugin_event(jcr, bEventJobEnd);

   dequeue_messages(jcr);             /* send any queued messages */

   /*
    * Inform Director that we are done
    */
   dir->signal(BNET_TERMINATE);

   free_plugins(jcr);                 /* release instantiated plugins */
   free_and_null_pool_memory(jcr->job_metadata);

   /*
    * Clean up fileset
    */
   cleanup_fileset(jcr);

   Dmsg0(100, "Calling term_find_files\n");
   term_find_files(jcr->ff);
// save_cwd.restore(jcr);
// save_cwd.release();
   jcr->ff = NULL;
   Dmsg0(100, "Done with term_find_files\n");
   free_jcr(jcr);                     /* destroy JCR record */
   Dmsg0(100, "Done with free_jcr\n");
   Dsm_check(100);
   garbage_collect_memory_pool();

#ifdef HAVE_WIN32
   allow_os_suspensions();
#endif

   return NULL;
}

/*
 * Connection request. We accept connections either from the Director or the Storage Daemon
 *
 * NOTE! We are running as a separate thread
 *
 * Send output one line at a time followed by a zero length transmission.
 * Return when the connection is terminated or there is an error.
 *
 * Basic tasks done here:
 *  - If it was a connection from an SD, call handle_stored_connection()
 *  - Otherwise it was a connection from the DIR, call handle_director_connection()
 */
void *handle_connection_request(void *arg)
{
   BSOCK *bs = (BSOCK *)arg;
   char tbuf[100];

   if (bs->recv() <= 0) {
      Emsg1(M_ERROR, 0, _("Connection request from %s failed.\n"), bs->who());
      bmicrosleep(5, 0);   /* make user wait 5 seconds */
      bs->close();
      delete bs;
      return NULL;
   }

   Dmsg1(110, "Conn: %s", bs->msg);

   /*
    * See if its a director making a connection.
    */
   if (bstrncmp(bs->msg, "Hello Director", 14)) {
      Dmsg1(110, "Got a DIR connection at %s\n", bstrftimes(tbuf, sizeof(tbuf), (utime_t)time(NULL)));
      return handle_director_connection(bs);
   }

   /*
    * See if its a storage daemon making a connection.
    */
   if (bstrncmp(bs->msg, "Hello Storage", 13)) {
      Dmsg1(110, "Got a SD connection at %s\n", bstrftimes(tbuf, sizeof(tbuf), (utime_t)time(NULL)));
      return handle_stored_connection(bs);
   }

   Emsg2(M_ERROR, 0, _("Invalid connection from %s. Len=%d\n"), bs->who(), bs->msglen);

   return NULL;
}

static bool sm_dump_cmd(JCR *jcr)
{
   close_memory_pool();
   sm_dump(false, true);
   jcr->dir_bsock->fsend("2000 sm_dump OK\n");
   return true;
}

/*
 * Resolve a hostname
 */
static bool resolve_cmd(JCR *jcr)
{
   BSOCK *dir = jcr->dir_bsock;
   dlist *addr_list;
   const char *errstr;
   char addresses[2048];
   char hostname[2048];

   sscanf(dir->msg, resolvecmd, &hostname);

   if ((addr_list = bnet_host2ipaddrs(hostname, 0, &errstr)) == NULL) {
      dir->fsend(_("%s: Failed to resolve %s\n"), my_name, hostname);
      goto bail_out;
   }

   dir->fsend(_("%s resolves %s to %s\n"),my_name, hostname,
              build_addresses_str(addr_list, addresses, sizeof(addresses), false));
   free_addresses(addr_list);

bail_out:
   dir->signal(BNET_EOD);
   return true;
}

#ifdef DEVELOPER
   static bool exit_cmd(JCR *jcr)
{
   jcr->dir_bsock->fsend("2000 exit OK\n");
   terminate_filed(0);
   return false;
}
#endif

/**
 * Cancel a Job
 */
static bool cancel_cmd(JCR *jcr)
{
   BSOCK *dir = jcr->dir_bsock;
   char Job[MAX_NAME_LENGTH];
   JCR *cjcr;

   if (sscanf(dir->msg, "cancel Job=%127s", Job) == 1) {
      if (!(cjcr=get_jcr_by_full_name(Job))) {
         dir->fsend(_("2901 Job %s not found.\n"), Job);
      } else {
         generate_plugin_event(cjcr, bEventCancelCommand, NULL);
         cjcr->setJobStatus(JS_Canceled);
         if (cjcr->store_bsock) {
            cjcr->store_bsock->set_timed_out();
            cjcr->store_bsock->set_terminated();
            cjcr->my_thread_send_signal(TIMEOUT_SIGNAL);
         }
         free_jcr(cjcr);
         dir->fsend(_("2001 Job %s marked to be canceled.\n"), Job);
      }
   } else {
      dir->fsend(_("2902 Error scanning cancel command.\n"));
   }
   dir->signal(BNET_EOD);
   return true;
}

/**
 * Set new authorization key as requested by the Director
 */
static bool setauthorization_cmd(JCR *jcr)
{
   BSOCK *dir = jcr->dir_bsock;
   POOL_MEM sd_auth_key(PM_MESSAGE);

   sd_auth_key.check_size(dir->msglen);
   if (sscanf(dir->msg, setauthorizationcmd, sd_auth_key.c_str()) != 1) {
      dir->fsend(BADcmd, "setauthorization");
      return false;
   }

   set_storage_auth_key(jcr, sd_auth_key.c_str());
   Dmsg2(120, "JobId=%d Auth=%s\n", jcr->JobId, jcr->sd_auth_key);

   return dir->fsend(OkAuthorization);
}

/**
 * Set bandwidth limit as requested by the Director
 */
static bool setbandwidth_cmd(JCR *jcr)
{
   BSOCK *dir = jcr->dir_bsock;
   int64_t bw = 0;
   JCR *cjcr;
   char Job[MAX_NAME_LENGTH];

   *Job = 0;
   if (sscanf(dir->msg, setbandwidthcmd, &bw, Job) != 2 || bw < 0) {
      pm_strcpy(jcr->errmsg, dir->msg);
      dir->fsend(_("2991 Bad setbandwidth command: %s\n"), jcr->errmsg);
      return false;
   }

   if (*Job) {
      if (!(cjcr = get_jcr_by_full_name(Job))) {
         dir->fsend(_("2901 Job %s not found.\n"), Job);
      } else {
         cjcr->max_bandwidth = bw;
         if (cjcr->store_bsock) {
            cjcr->store_bsock->set_bwlimit(bw);
            if (me->allow_bw_bursting) {
               cjcr->store_bsock->set_bwlimit_bursting();
            }
         }
         free_jcr(cjcr);
      }
   } else {                           /* No job requested, apply globally */
      me->max_bandwidth_per_job = bw; /* Overwrite directive */
   }

   return dir->fsend(OKBandwidth);
}

/**
 * Set debug level as requested by the Director
 */
static bool setdebug_cmd(JCR *jcr)
{
   BSOCK *dir = jcr->dir_bsock;
   int32_t level, trace_flag, hangup_flag, timestamp_flag;
   int scan;

   Dmsg1(50, "setdebug_cmd: %s", dir->msg);
   scan = sscanf(dir->msg, setdebugv2cmd, &level, &trace_flag, &hangup_flag, &timestamp_flag);
   if (scan != 4) {
      scan = sscanf(dir->msg, setdebugv1cmd, &level, &trace_flag, &hangup_flag);
   }
   if (scan != 3 && scan != 4) {
      scan = sscanf(dir->msg, setdebugv0cmd, &level, &trace_flag);
      if (scan != 2) {
         pm_strcpy(jcr->errmsg, dir->msg);
         dir->fsend(_("2991 Bad setdebug command: %s\n"), jcr->errmsg);
         return false;
      } else {
         hangup_flag = -1;
      }
   }

   POOL_MEM tracefilename(PM_FNAME);
   Mmsg(tracefilename, "%s/%s.trace", TRACEFILEDIRECTORY, my_name);

   if (level >= 0) {
      debug_level = level;
   }

   set_trace(trace_flag);
   set_hangup(hangup_flag);
   if (scan == 4) {
      set_timestamp(timestamp_flag);
      Dmsg5(50, "level=%d trace=%d hangup=%d timestamp=%d tracefilename=%s\n", level, get_trace(), get_hangup(), get_timestamp(), tracefilename.c_str());
      return dir->fsend(OKsetdebugv1, level, get_trace(), get_hangup(), get_timestamp(), tracefilename.c_str());
   } else {
      Dmsg4(50, "level=%d trace=%d hangup=%d tracefilename=%s\n", level, get_trace(), get_hangup(), tracefilename.c_str());
      return dir->fsend(OKsetdebugv0, level, get_trace(), get_hangup(), tracefilename.c_str());
   }
}

static bool estimate_cmd(JCR *jcr)
{
   BSOCK *dir = jcr->dir_bsock;
   char ed1[50], ed2[50];

   /*
    * See if we are allowed to run estimate cmds.
    */
   if (!validate_command(jcr, "estimate",
                        (jcr->director && jcr->director->allowed_job_cmds) ?
                         jcr->director->allowed_job_cmds :
                         me->allowed_job_cmds)) {
      dir->fsend(_("2992 Bad estimate command.\n"));
      return 0;
   }

   if (sscanf(dir->msg, estimatecmd, &jcr->listing) != 1) {
      pm_strcpy(jcr->errmsg, dir->msg);
      Jmsg(jcr, M_FATAL, 0, _("Bad estimate command: %s"), jcr->errmsg);
      dir->fsend(_("2992 Bad estimate command.\n"));
      return false;
   }

   make_estimate(jcr);

   dir->fsend(OKest, edit_uint64_with_commas(jcr->num_files_examined, ed1),
              edit_uint64_with_commas(jcr->JobBytes, ed2));
   dir->signal(BNET_EOD);

   return true;
}

/**
 * Get JobId and Storage Daemon Authorization key from Director
 */
static bool job_cmd(JCR *jcr)
{
   BSOCK *dir = jcr->dir_bsock;
   POOL_MEM sd_auth_key(PM_MESSAGE);
   sd_auth_key.check_size(dir->msglen);
   const char *os_version;

   if (sscanf(dir->msg, jobcmd,  &jcr->JobId, jcr->Job,
              &jcr->VolSessionId, &jcr->VolSessionTime,
              sd_auth_key.c_str()) != 5) {
      pm_strcpy(jcr->errmsg, dir->msg);
      Jmsg(jcr, M_FATAL, 0, _("Bad Job Command: %s"), jcr->errmsg);
      dir->fsend(BADjob);
      return false;
   }
   set_storage_auth_key(jcr, sd_auth_key.c_str());
   Dmsg2(120, "JobId=%d Auth=%s\n", jcr->JobId, jcr->sd_auth_key);
   Mmsg(jcr->errmsg, "JobId=%d Job=%s", jcr->JobId, jcr->Job);
   new_plugins(jcr);                  /* instantiate plugins for this jcr */
   generate_plugin_event(jcr, bEventJobStart, (void *)jcr->errmsg);

#ifdef HAVE_WIN32
   os_version = win_os;
#else
   os_version = HOST_OS;
#endif

#if !defined(IS_BUILD_ON_OBS)
   return dir->fsend(OKjob, VERSION, LSMDATE, os_version, DISTNAME, DISTVER);
#else
   return dir->fsend(OKjob, VERSION, LSMDATE, os_version, DISTNAME, DISTVER, OBS_DISTRIBUTION, OBS_ARCH);
#endif
}

static bool runbefore_cmd(JCR *jcr)
{
   bool ok;
   POOLMEM *cmd;
   RUNSCRIPT *script;
   BSOCK *dir = jcr->dir_bsock;

   if (!me->compatible) {
      dir->fsend(BadRunBeforeJob);
      return false;
   }

   Dmsg1(100, "runbefore_cmd: %s", dir->msg);
   cmd = get_memory(dir->msglen + 1);
   if (sscanf(dir->msg, runbeforecmd, cmd) != 1) {
      pm_strcpy(jcr->errmsg, dir->msg);
      Jmsg1(jcr, M_FATAL, 0, _("Bad RunBeforeJob command: %s\n"), jcr->errmsg);
      dir->fsend(BadRunBeforeJob);
      free_memory(cmd);
      return false;
   }
   unbash_spaces(cmd);

   /*
    * Run the command now
    */
   script = new_runscript();
   script->set_job_code_callback(job_code_callback_filed);
   script->set_command(cmd);
   script->when = SCRIPT_Before;
   free_memory(cmd);

   ok = script->run(jcr, "ClientRunBeforeJob");
   free_runscript(script);

   if (ok) {
      dir->fsend(OKRunBefore);
      return true;
   } else {
      dir->fsend(BadRunBeforeJob);
      return false;
   }
}

static bool runbeforenow_cmd(JCR *jcr)
{
   BSOCK *dir = jcr->dir_bsock;

   run_scripts(jcr, jcr->RunScripts, "ClientBeforeJob",
              (jcr->director && jcr->director->allowed_script_dirs) ?
               jcr->director->allowed_script_dirs :
               me->allowed_script_dirs);

   if (job_canceled(jcr)) {
      dir->fsend(FailedRunScript);
      Dmsg0(100, "Back from run_scripts ClientBeforeJob now: FAILED\n");
      return false;
   } else {
      dir->fsend(OKRunBeforeNow);
      Dmsg0(100, "Back from run_scripts ClientBeforeJob now: OK\n");
      return true;
   }
}

static bool runafter_cmd(JCR *jcr)
{
   BSOCK *dir = jcr->dir_bsock;
   POOLMEM *cmd;
   RUNSCRIPT *script;

   if (!me->compatible) {
      dir->fsend(BadRunAfterJob);
      return false;
   }

   Dmsg1(100, "runafter_cmd: %s", dir->msg);
   cmd = get_memory(dir->msglen + 1);
   if (sscanf(dir->msg, runaftercmd, cmd) != 1) {
      pm_strcpy(jcr->errmsg, dir->msg);
      Jmsg1(jcr, M_FATAL, 0, _("Bad RunAfter command: %s\n"), jcr->errmsg);
      dir->fsend(BadRunAfterJob);
      free_memory(cmd);
      return false;
   }
   unbash_spaces(cmd);

   script = new_runscript();
   script->set_job_code_callback(job_code_callback_filed);
   script->set_command(cmd);
   script->on_success = true;
   script->on_failure = false;
   script->when = SCRIPT_After;
   free_memory(cmd);

   jcr->RunScripts->append(script);

   return dir->fsend(OKRunAfter);
}

static bool runscript_cmd(JCR *jcr)
{
   POOLMEM *msg;
   RUNSCRIPT *cmd;
   BSOCK *dir = jcr->dir_bsock;
   int on_success, on_failure, fail_on_error;

   /*
    * See if we are allowed to run runscript cmds.
    */
   if (!validate_command(jcr, "runscript",
                        (jcr->director && jcr->director->allowed_job_cmds) ?
                         jcr->director->allowed_job_cmds :
                         me->allowed_job_cmds)) {
      dir->fsend(FailedRunScript);
      return 0;
   }

   msg = get_memory(dir->msglen + 1);
   cmd = new_runscript();
   cmd->set_job_code_callback(job_code_callback_filed);

   Dmsg1(100, "runscript_cmd: '%s'\n", dir->msg);

   /*
    * Note, we cannot sscanf into bools
    */
   if (sscanf(dir->msg, runscriptcmd, &on_success, &on_failure,
              &fail_on_error, &cmd->when, msg) != 5) {
      pm_strcpy(jcr->errmsg, dir->msg);
      Jmsg1(jcr, M_FATAL, 0, _("Bad RunScript command: %s\n"), jcr->errmsg);
      dir->fsend(FailedRunScript);
      free_runscript(cmd);
      free_memory(msg);
      return false;
   }

   cmd->on_success = on_success;
   cmd->on_failure = on_failure;
   cmd->fail_on_error = fail_on_error;
   unbash_spaces(msg);

   cmd->set_command(msg);
   cmd->debug();
   jcr->RunScripts->append(cmd);

   free_pool_memory(msg);

   return dir->fsend(OKRunScript);
}

/*
 * This passes plugin specific options.
 */
static bool pluginoptions_cmd(JCR *jcr)
{
   BSOCK *dir = jcr->dir_bsock;
   POOLMEM *msg;

   msg = get_memory(dir->msglen + 1);
   if (sscanf(dir->msg, pluginoptionscmd, msg) != 1) {
      pm_strcpy(jcr->errmsg, dir->msg);
      Jmsg1(jcr, M_FATAL, 0, _("Bad Plugin Options command: %s\n"), jcr->errmsg);
      dir->fsend(BadPluginOptions);
      free_memory(msg);
      return false;
   }

   unbash_spaces(msg);
   generate_plugin_event(jcr, bEventNewPluginOptions, (void *)msg);
   free_memory(msg);

   return dir->fsend(OKPluginOptions);
}

/*
 * This reads data sent from the Director from the
 *   RestoreObject table that allows us to get objects
 *   that were backed up (VSS .xml data) and are needed
 *   before starting the restore.
 */
static bool restore_object_cmd(JCR *jcr)
{
   BSOCK *dir = jcr->dir_bsock;
   int32_t FileIndex;
   restore_object_pkt rop;

   memset(&rop, 0, sizeof(rop));
   rop.pkt_size = sizeof(rop);
   rop.pkt_end = sizeof(rop);

   Dmsg1(100, "Enter restoreobject_cmd: %s", dir->msg);
   if (bstrcmp(dir->msg, endrestoreobjectcmd)) {
      generate_plugin_event(jcr, bEventRestoreObject, NULL);
      return dir->fsend(OKRestoreObject);
   }

   rop.plugin_name = (char *) malloc (dir->msglen);
   *rop.plugin_name = 0;

   if (sscanf(dir->msg, restoreobjcmd, &rop.JobId, &rop.object_len,
              &rop.object_full_len, &rop.object_index,
              &rop.object_type, &rop.object_compression, &FileIndex,
              rop.plugin_name) != 8) {

      /*
       * Old version, no plugin_name
       */
      if (sscanf(dir->msg, restoreobjcmd1, &rop.JobId, &rop.object_len,
                 &rop.object_full_len, &rop.object_index,
                 &rop.object_type, &rop.object_compression, &FileIndex) != 7) {
         Dmsg0(5, "Bad restore object command\n");
         pm_strcpy(jcr->errmsg, dir->msg);
         Jmsg1(jcr, M_FATAL, 0, _("Bad RestoreObject command: %s\n"), jcr->errmsg);
         goto bail_out;
      }
   }

   unbash_spaces(rop.plugin_name);

   Dmsg7(100, "Recv object: JobId=%u objlen=%d full_len=%d objinx=%d objtype=%d "
         "FI=%d plugin_name=%s\n",
         rop.JobId, rop.object_len, rop.object_full_len,
         rop.object_index, rop.object_type, FileIndex, rop.plugin_name);

   /*
    * Read Object name
    */
   if (dir->recv() < 0) {
      goto bail_out;
   }
   Dmsg2(100, "Recv Oname object: len=%d Oname=%s\n", dir->msglen, dir->msg);
   rop.object_name = bstrdup(dir->msg);

   /*
    * Read Object
    */
   if (dir->recv() < 0) {
      goto bail_out;
   }

   /*
    * Transfer object from message buffer, and get new message buffer
    */
   rop.object = dir->msg;
   dir->msg = get_pool_memory(PM_MESSAGE);

   /*
    * If object is compressed, uncompress it
    */
   switch (rop.object_compression) {
   case 1: {                          /* zlib level 9 */
      int status;
      int out_len = rop.object_full_len + 100;
      POOLMEM *obj = get_memory(out_len);

      Dmsg2(100, "Inflating from %d to %d\n", rop.object_len, rop.object_full_len);
      status = Zinflate(rop.object, rop.object_len, obj, out_len);
      Dmsg1(100, "Zinflate status=%d\n", status);

      if (out_len != rop.object_full_len) {
         Jmsg3(jcr, M_ERROR, 0, ("Decompression failed. Len wanted=%d got=%d. Object_name=%s\n"),
               rop.object_full_len, out_len, rop.object_name);
      }

      free_pool_memory(rop.object);   /* Release compressed object */
      rop.object = obj;               /* New uncompressed object */
      rop.object_len = out_len;
      break;
   }
   default:
      break;
   }

   if (debug_level >= 100) {
      POOL_MEM object_content(PM_MESSAGE);

      /*
       * Convert the object into a null terminated string.
       */
      object_content.check_size(rop.object_len + 1);
      memset(object_content.c_str(), 0, rop.object_len + 1);
      memcpy(object_content.c_str(), rop.object, rop.object_len);

      Dmsg2(100, "Recv Object: len=%d Object=%s\n", rop.object_len, object_content.c_str());
   }

   /*
    * We still need to do this to detect a vss restore
    */
   if (bstrcmp(rop.object_name, "job_metadata.xml")) {
      Dmsg0(100, "got job metadata\n");
      jcr->got_metadata = true;
   }

   generate_plugin_event(jcr, bEventRestoreObject, (void *)&rop);

   if (rop.object_name) {
      free(rop.object_name);
   }

   if (rop.object) {
      free_pool_memory(rop.object);
   }

   if (rop.plugin_name) {
      free(rop.plugin_name);
   }

   Dmsg1(100, "Send: %s", OKRestoreObject);
   return true;

bail_out:
   dir->fsend(_("2909 Bad RestoreObject command.\n"));
   return false;
}

#if defined(WIN32_VSS)
static inline int count_include_list_file_entries(JCR *jcr)
{
   int cnt = 0;
   findFILESET *fileset;
   findINCEXE *incexe;

   fileset = jcr->ff->fileset;
   if (fileset) {
      for (int i = 0; i < fileset->include_list.size(); i++) {
         incexe = (findINCEXE *)fileset->include_list.get(i);
         cnt += incexe->name_list.size();
      }
   }

   return cnt;
}
#endif

/**
 * Director is passing his Fileset
 */
static bool fileset_cmd(JCR *jcr)
{
   BSOCK *dir = jcr->dir_bsock;
   bool retval;
#if defined(WIN32_VSS)
   int vss = 0;

   sscanf(dir->msg, "fileset vss=%d", &vss);
#endif

   if (!init_fileset(jcr)) {
      return false;
   }

   while (dir->recv() >= 0) {
      strip_trailing_junk(dir->msg);
      Dmsg1(500, "Fileset: %s\n", dir->msg);
      add_fileset(jcr, dir->msg);
   }

   if (!term_fileset(jcr)) {
      return false;
   }

#if defined(WIN32_VSS)
   enable_vss = (vss && (count_include_list_file_entries(jcr) > 0)) ? true : false;
#endif

   retval = dir->fsend(OKinc);
   generate_plugin_event(jcr, bEventEndFileSet);
   check_include_list_shadowing(jcr, jcr->ff->fileset);

   return retval;
}

static void free_bootstrap(JCR *jcr)
{
   if (jcr->RestoreBootstrap) {
      unlink(jcr->RestoreBootstrap);
      free_pool_memory(jcr->RestoreBootstrap);
      jcr->RestoreBootstrap = NULL;
   }
}

static pthread_mutex_t bsr_mutex = PTHREAD_MUTEX_INITIALIZER;
static uint32_t bsr_uniq = 0;

/**
 * The Director sends us the bootstrap file, which
 *   we will in turn pass to the SD.
 *   Deprecated.  The bsr is now sent directly from the
 *   Director to the SD.
 */
static bool bootstrap_cmd(JCR *jcr)
{
   BSOCK *dir = jcr->dir_bsock;
   POOLMEM *fname = get_pool_memory(PM_FNAME);
   FILE *bs;

   free_bootstrap(jcr);
   P(bsr_mutex);
   bsr_uniq++;
   Mmsg(fname, "%s/%s.%s.%d.bootstrap", me->working_directory, me->hdr.name,
      jcr->Job, bsr_uniq);
   V(bsr_mutex);
   Dmsg1(400, "bootstrap=%s\n", fname);
   jcr->RestoreBootstrap = fname;
   bs = fopen(fname, "a+b");           /* create file */
   if (!bs) {
      berrno be;
      Jmsg(jcr, M_FATAL, 0, _("Could not create bootstrap file %s: ERR=%s\n"),
         jcr->RestoreBootstrap, be.bstrerror());
      /*
       * Suck up what he is sending to us so that he will then
       *   read our error message.
       */
      while (dir->recv() >= 0)
        {  }
      free_bootstrap(jcr);
      jcr->setJobStatus(JS_ErrorTerminated);
      return false;
   }

   while (dir->recv() >= 0) {
       Dmsg1(200, "filed<dird: bootstrap: %s", dir->msg);
       fputs(dir->msg, bs);
   }
   fclose(bs);
   /*
    * Note, do not free the bootstrap yet -- it needs to be
    *  sent to the SD
    */
   return dir->fsend(OKbootstrap);
}

/**
 * Get backup level from Director
 *
 */
static bool level_cmd(JCR *jcr)
{
   BSOCK *dir = jcr->dir_bsock;
   POOLMEM *level, *buf = NULL;
   int mtime_only;

   level = get_memory(dir->msglen+1);
   Dmsg1(10, "level_cmd: %s", dir->msg);

   /* keep compatibility with older directors */
   if (strstr(dir->msg, "accurate")) {
      jcr->accurate = true;
   }
   if (strstr(dir->msg, "rerunning")) {
      jcr->rerunning = true;
   }
   if (sscanf(dir->msg, "level = %s ", level) != 1) {
      goto bail_out;
   }
   /* Base backup requested? */
   if (bstrcmp(level, "base")) {
      jcr->setJobLevel(L_BASE);
   /* Full backup requested? */
   } else if (bstrcmp(level, "full")) {
      jcr->setJobLevel(L_FULL);
   } else if (strstr(level, "differential")) {
      jcr->setJobLevel(L_DIFFERENTIAL);
      free_memory(level);
      return true;
   } else if (strstr(level, "incremental")) {
      jcr->setJobLevel(L_INCREMENTAL);
      free_memory(level);
      return true;
   /*
    * We get his UTC since time, then sync the clocks and correct it
    *   to agree with our clock.
    */
   } else if (bstrcmp(level, "since_utime")) {
      buf = get_memory(dir->msglen+1);
      utime_t since_time, adj;
      btime_t his_time, bt_start, rt=0, bt_adj=0;
      if (jcr->getJobLevel() == L_NONE) {
         jcr->setJobLevel(L_SINCE);     /* if no other job level set, do it now */
      }
      if (sscanf(dir->msg, "level = since_utime %s mtime_only=%d prev_job=%127s",
                 buf, &mtime_only, jcr->PrevJob) != 3) {
         if (sscanf(dir->msg, "level = since_utime %s mtime_only=%d",
                    buf, &mtime_only) != 2) {
            goto bail_out;
         }
      }
      since_time = str_to_uint64(buf);  /* this is the since time */
      Dmsg2(100, "since_time=%lld prev_job=%s\n", since_time, jcr->PrevJob);
      char ed1[50], ed2[50];
      /*
       * Sync clocks by polling him for the time. We take
       *   10 samples of his time throwing out the first two.
       */
      for (int i = 0; i < 10; i++) {
         bt_start = get_current_btime();
         dir->signal(BNET_BTIME);     /* poll for time */
         if (dir->recv() <= 0) {      /* get response */
            goto bail_out;
         }
         if (sscanf(dir->msg, "btime %s", buf) != 1) {
            goto bail_out;
         }
         if (i < 2) {                 /* toss first two results */
            continue;
         }
         his_time = str_to_uint64(buf);
         rt = get_current_btime() - bt_start; /* compute round trip time */
         Dmsg2(100, "Dirtime=%s FDtime=%s\n", edit_uint64(his_time, ed1),
               edit_uint64(bt_start, ed2));
         bt_adj +=  bt_start - his_time - rt/2;
         Dmsg2(100, "rt=%s adj=%s\n", edit_uint64(rt, ed1), edit_uint64(bt_adj, ed2));
      }

      bt_adj = bt_adj / 8;            /* compute average time */
      Dmsg2(100, "rt=%s adj=%s\n", edit_uint64(rt, ed1), edit_uint64(bt_adj, ed2));
      adj = btime_to_utime(bt_adj);
      since_time += adj;              /* adjust for clock difference */
      /* Don't notify if time within 3 seconds */
      if (adj > 3 || adj < -3) {
         int type;
         if (adj > 600 || adj < -600) {
            type = M_WARNING;
         } else {
            type = M_INFO;
         }
         Jmsg(jcr, type, 0, _("DIR and FD clocks differ by %lld seconds, FD automatically compensating.\n"), adj);
      }
      dir->signal(BNET_EOD);

      Dmsg2(100, "adj=%lld since_time=%lld\n", adj, since_time);
      jcr->incremental = true;        /* set incremental or decremental backup */
      jcr->mtime = since_time;        /* set since time */
      generate_plugin_event(jcr, bEventSince, (void *)(time_t)jcr->mtime);
   } else {
      Jmsg1(jcr, M_FATAL, 0, _("Unknown backup level: %s\n"), level);
      free_memory(level);
      return false;
   }
   free_memory(level);
   if (buf) {
      free_memory(buf);
   }
   generate_plugin_event(jcr, bEventLevel, (void*)(intptr_t)jcr->getJobLevel());
   return dir->fsend(OKlevel);

bail_out:
   pm_strcpy(jcr->errmsg, dir->msg);
   Jmsg1(jcr, M_FATAL, 0, _("Bad level command: %s\n"), jcr->errmsg);
   free_memory(level);
   if (buf) {
      free_memory(buf);
   }
   return false;
}

/**
 * Get session parameters from Director -- this is for a Restore command
 *   This is deprecated. It is now passed via the bsr.
 */
static bool session_cmd(JCR *jcr)
{
   BSOCK *dir = jcr->dir_bsock;

   Dmsg1(100, "SessionCmd: %s", dir->msg);
   if (sscanf(dir->msg, sessioncmd, jcr->VolumeName,
              &jcr->VolSessionId, &jcr->VolSessionTime,
              &jcr->StartFile, &jcr->EndFile,
              &jcr->StartBlock, &jcr->EndBlock) != 7) {
      pm_strcpy(jcr->errmsg, dir->msg);
      Jmsg(jcr, M_FATAL, 0, _("Bad session command: %s"), jcr->errmsg);
      return false;
   }

   return dir->fsend(OKsession);
}

static void set_storage_auth_key(JCR *jcr, char *key)
{
   /* if no key don't update anything */
   if (!*key) {
      return;
   }

   /**
    * We can be contacting multiple storage daemons.
    * So, make sure that any old jcr->store_bsock is cleaned up.
    */
   if (jcr->store_bsock) {
      delete jcr->store_bsock;
      jcr->store_bsock = NULL;
   }

   /**
    * We can be contacting multiple storage daemons.
    *   So, make sure that any old jcr->sd_auth_key is cleaned up.
    */
   if (jcr->sd_auth_key) {
      /*
       * If we already have a Authorization key, director can do multi
       * storage restore
       */
      Dmsg0(5, "set multi_restore=true\n");
      jcr->multi_restore = true;
      bfree(jcr->sd_auth_key);
   }

   jcr->sd_auth_key = bstrdup(key);
   Dmsg0(5, "set sd auth key\n");
}

/**
 * Get address of storage daemon from Director
 */
static bool storage_cmd(JCR *jcr)
{
   int stored_port;                /* storage daemon port */
   int enable_ssl;                 /* enable ssl to sd */
   char stored_addr[MAX_NAME_LENGTH];
   POOL_MEM sd_auth_key(PM_MESSAGE);
   BSOCK *dir = jcr->dir_bsock;
   BSOCK *sd;                      /* storage daemon bsock */

   sd = New(BSOCK_TCP);
   if (me->nokeepalive) {
      sd->clear_keepalive();
   }
   Dmsg1(100, "StorageCmd: %s", dir->msg);
   sd_auth_key.check_size(dir->msglen);
   if (sscanf(dir->msg, storaddrv1cmd, stored_addr, &stored_port,
              &enable_ssl, sd_auth_key.c_str()) != 4) {
      if (sscanf(dir->msg, storaddrv0cmd, stored_addr,
                 &stored_port, &enable_ssl) != 3) {
         pm_strcpy(jcr->errmsg, dir->msg);
         Jmsg(jcr, M_FATAL, 0, _("Bad storage command: %s"), jcr->errmsg);
         goto bail_out;
      }
   }

   set_storage_auth_key(jcr, sd_auth_key.c_str());

   Dmsg3(110, "Open storage: %s:%d ssl=%d\n", stored_addr, stored_port, enable_ssl);

   sd->set_source_address(me->FDsrc_addr);

   /*
    * TODO: see if we put limit on restore and backup...
    */
   if (!jcr->max_bandwidth) {
      if (jcr->director->max_bandwidth_per_job) {
         jcr->max_bandwidth = jcr->director->max_bandwidth_per_job;
      } else if (me->max_bandwidth_per_job) {
         jcr->max_bandwidth = me->max_bandwidth_per_job;
      }
   }

   sd->set_bwlimit(jcr->max_bandwidth);
   if (me->allow_bw_bursting) {
      sd->set_bwlimit_bursting();
   }

   /*
    * Open command communications with Storage daemon
    */
   if (!sd->connect(jcr, 10, (int)me->SDConnectTimeout, me->heartbeat_interval,
                    _("Storage daemon"), stored_addr, NULL, stored_port, 1)) {
     delete sd;
     sd = NULL;
   }

   if (sd == NULL) {
      Jmsg(jcr, M_FATAL, 0, _("Failed to connect to Storage daemon: %s:%d\n"),
           stored_addr, stored_port);
      Dmsg2(100, "Failed to connect to Storage daemon: %s:%d\n",
            stored_addr, stored_port);
      goto bail_out;
   }
   Dmsg0(110, "Connection OK to SD.\n");

   jcr->store_bsock = sd;

   sd->fsend("Hello Start Job %s\n", jcr->Job);
   if (!authenticate_with_storagedaemon(jcr)) {
      Jmsg(jcr, M_FATAL, 0, _("Failed to authenticate Storage daemon.\n"));
      goto bail_out;
   }
   Dmsg0(110, "Authenticated with SD.\n");

   /*
    * Send OK to Director
    */
   return dir->fsend(OKstore);

bail_out:
   dir->fsend(BADcmd, "storage");
   return false;
}

/**
 * Do a backup.
 */
static bool backup_cmd(JCR *jcr)
{
   BSOCK *dir = jcr->dir_bsock;
   BSOCK *sd = jcr->store_bsock;
   int ok = 0;
   int SDJobStatus;
   int32_t FileIndex;

   /*
    * See if we are in restore only mode then we don't allow a backup to be initiated.
    */
   if (restore_only_mode) {
      Jmsg(jcr, M_FATAL, 0, _("Filed in restore only mode, backups are not allowed, aborting...\n"));
      goto cleanup;
   }

   /*
    * See if we are allowed to run backup cmds.
    */
   if (!validate_command(jcr, "backup",
                        (jcr->director && jcr->director->allowed_job_cmds) ?
                         jcr->director->allowed_job_cmds :
                         me->allowed_job_cmds)) {
      goto cleanup;
   }

#if defined(WIN32_VSS)
   /*
    * Capture state here, if client is backed up by multiple directors
    * and one enables vss and the other does not then enable_vss can change
    * between here and where its evaluated after the job completes.
    */
   jcr->VSS = g_pVSSClient && enable_vss;
   if (jcr->VSS) {
      /*
       * Run only one at a time
       */
      P(vss_mutex);
   }
#endif

   if (sscanf(dir->msg, "backup FileIndex=%ld\n", &FileIndex) == 1) {
      jcr->JobFiles = FileIndex;
      Dmsg1(100, "JobFiles=%ld\n", jcr->JobFiles);
   }

   /**
    * Validate some options given to the backup make sense for the compiled in options of this filed.
    */
   if (bit_is_set(FO_ACL, jcr->ff->flags) && !have_acl) {
      Jmsg(jcr, M_WARNING, 0, _("ACL support requested in fileset but not available on this platform. Disabling ...\n"));
      clear_bit(FO_ACL, jcr->ff->flags);
   }
   if (bit_is_set(FO_XATTR, jcr->ff->flags) && !have_xattr) {
      Jmsg(jcr, M_WARNING, 0, _("XATTR support requested in fileset but not available on this platform. Disabling ...\n"));
      clear_bit(FO_XATTR, jcr->ff->flags);
   }

   jcr->setJobStatus(JS_Blocked);
   jcr->setJobType(JT_BACKUP);
   Dmsg1(100, "begin backup ff=%p\n", jcr->ff);

   if (sd == NULL) {
      Jmsg(jcr, M_FATAL, 0, _("Cannot contact Storage daemon\n"));
      dir->fsend(BADcmd, "backup");
      goto cleanup;
   }

   dir->fsend(OKbackup);
   Dmsg1(110, "filed>dird: %s", dir->msg);

   /**
    * Send Append Open Session to Storage daemon
    */
   sd->fsend(append_open);
   Dmsg1(110, ">stored: %s", sd->msg);
   /**
    * Expect to receive back the Ticket number
    */
   if (bget_msg(sd) >= 0) {
      Dmsg1(110, "<stored: %s", sd->msg);
      if (sscanf(sd->msg, OK_open, &jcr->Ticket) != 1) {
         Jmsg(jcr, M_FATAL, 0, _("Bad response to append open: %s\n"), sd->msg);
         goto cleanup;
      }
      Dmsg1(110, "Got Ticket=%d\n", jcr->Ticket);
   } else {
      Jmsg(jcr, M_FATAL, 0, _("Bad response from stored to open command\n"));
      goto cleanup;
   }

   /**
    * Send Append data command to Storage daemon
    */
   sd->fsend(append_data, jcr->Ticket);
   Dmsg1(110, ">stored: %s", sd->msg);

   /**
    * Expect to get OK data
    */
   if (!response(jcr, sd, OK_data, "Append Data")) {
      Dmsg1(110, "<stored: %s", sd->msg);
      goto cleanup;
   }
   Dmsg1(110, "<stored: %s", sd->msg);

   generate_plugin_event(jcr, bEventStartBackupJob);

#if defined(WIN32_VSS)
   /*
    * START VSS ON WIN32
    */
   if (jcr->VSS) {
      if (g_pVSSClient->InitializeForBackup(jcr)) {
         int drive_count;
         char szWinDriveLetters[27];
         bool onefs_disabled;

         generate_plugin_event(jcr, bEventVssBackupAddComponents);

         /*
          * Tell vss which drives to snapshot
          */
         *szWinDriveLetters = 0;

         /*
          * Plugin driver can return drive letters
          */
         generate_plugin_event(jcr, bEventVssPrepareSnapshot, szWinDriveLetters);

         drive_count = get_win32_driveletters(jcr->ff->fileset, szWinDriveLetters);

         onefs_disabled = win32_onefs_is_disabled(jcr->ff->fileset);

         if (drive_count > 0) {
            Jmsg(jcr, M_INFO, 0, _("Generate VSS snapshots. Driver=\"%s\", Drive(s)=\"%s\"\n"),
                 g_pVSSClient->GetDriverName(), (drive_count) ? szWinDriveLetters : "None");

            if (!g_pVSSClient->CreateSnapshots(szWinDriveLetters, onefs_disabled)) {
               berrno be;
               Jmsg(jcr, M_FATAL, 0, _("CreateSGenerate VSS snapshots failed. ERR=%s\n"), be.bstrerror());
            } else {
               /*
                * Inform about VMPs if we have them
                */
               g_pVSSClient->ShowVolumeMountPointStats(jcr);

               /*
                * Tell user if snapshot creation of a specific drive failed
                */
               for (int i = 0; i < (int)strlen(szWinDriveLetters); i++) {
                  if (islower(szWinDriveLetters[i])) {
                     Jmsg(jcr, M_FATAL, 0, _("Generate VSS snapshot of drive \"%c:\\\" failed.\n"), szWinDriveLetters[i]);
                  }
               }

               /*
                * Inform user about writer states
                */
               for (int i = 0; i < (int)g_pVSSClient->GetWriterCount(); i++) {
                  if (g_pVSSClient->GetWriterState(i) < 1) {
                     Jmsg(jcr, M_INFO, 0, _("VSS Writer (PrepareForBackup): %s\n"), g_pVSSClient->GetWriterInfo(i));
                  }
               }
            }

         } else {
            Jmsg(jcr, M_FATAL, 0, _("No drive letters found for generating VSS snapshots.\n"));
         }
      } else {
         berrno be;

         Jmsg(jcr, M_FATAL, 0, _("VSS was not initialized properly. ERR=%s\n"), be.bstrerror());
      }

      run_scripts(jcr, jcr->RunScripts, "ClientAfterVSS",
                 (jcr->director && jcr->director->allowed_script_dirs) ?
                  jcr->director->allowed_script_dirs :
                  me->allowed_script_dirs);
   }
#endif

   /**
    * Send Files to Storage daemon
    */
   Dmsg1(110, "begin blast ff=%p\n", (FF_PKT *)jcr->ff);
   if (!blast_data_to_storage_daemon(jcr, NULL)) {
      jcr->setJobStatus(JS_ErrorTerminated);
      bnet_suppress_error_messages(sd, 1);
      Dmsg0(110, "Error in blast_data.\n");
   } else {
      jcr->setJobStatus(JS_Terminated);
      /* Note, the above set status will not override an error */
      if (!jcr->is_terminated_ok()) {
         bnet_suppress_error_messages(sd, 1);
         goto cleanup;                /* bail out now */
      }
      /**
       * Expect to get response to append_data from Storage daemon
       */
      if (!response(jcr, sd, OK_append, "Append Data")) {
         jcr->setJobStatus(JS_ErrorTerminated);
         goto cleanup;
      }

      /**
       * Send Append End Data to Storage daemon
       */
      sd->fsend(append_end, jcr->Ticket);
      /* Get end OK */
      if (!response(jcr, sd, OK_end, "Append End")) {
         jcr->setJobStatus(JS_ErrorTerminated);
         goto cleanup;
      }

      /**
       * Send Append Close to Storage daemon
       */
      sd->fsend(append_close, jcr->Ticket);
      while (bget_msg(sd) >= 0) {    /* stop on signal or error */
         if (sscanf(sd->msg, OK_close, &SDJobStatus) == 1) {
            ok = 1;
            Dmsg2(200, "SDJobStatus = %d %c\n", SDJobStatus, (char)SDJobStatus);
         }
      }
      if (!ok) {
         Jmsg(jcr, M_FATAL, 0, _("Append Close with SD failed.\n"));
         goto cleanup;
      }
      if (!(SDJobStatus == JS_Terminated || SDJobStatus == JS_Warnings)) {
         Jmsg(jcr, M_FATAL, 0, _("Bad status %d returned from Storage Daemon.\n"),
            SDJobStatus);
      }
   }

cleanup:
#if defined(WIN32_VSS)
   if (jcr->VSS) {
      Win32ConvCleanupCache();
      g_pVSSClient->DestroyWriterInfo();
      V(vss_mutex);
   }
#endif

   generate_plugin_event(jcr, bEventEndBackupJob);
   return false;                      /* return and stop command loop */
}

/**
 * Do a Verify for Director
 *
 */
static bool verify_cmd(JCR *jcr)
{
   char level[100];
   BSOCK *dir = jcr->dir_bsock;
   BSOCK *sd  = jcr->store_bsock;

   /*
    * See if we are allowed to run verify cmds.
    */
   if (!validate_command(jcr, "verify",
                        (jcr->director && jcr->director->allowed_job_cmds) ?
                         jcr->director->allowed_job_cmds :
                         me->allowed_job_cmds)) {
      dir->fsend(_("2994 Bad verify command: %s\n"), dir->msg);
      return 0;
   }

   jcr->setJobType(JT_VERIFY);
   if (sscanf(dir->msg, verifycmd, level) != 1) {
      dir->fsend(_("2994 Bad verify command: %s\n"), dir->msg);
      return false;
   }

   if (bstrcasecmp(level, "init")) {
      jcr->setJobLevel(L_VERIFY_INIT);
   } else if (bstrcasecmp(level, "catalog")){
      jcr->setJobLevel(L_VERIFY_CATALOG);
   } else if (bstrcasecmp(level, "volume")){
      jcr->setJobLevel(L_VERIFY_VOLUME_TO_CATALOG);
   } else if (bstrcasecmp(level, "data")){
      jcr->setJobLevel(L_VERIFY_DATA);
   } else if (bstrcasecmp(level, "disk_to_catalog")) {
      jcr->setJobLevel(L_VERIFY_DISK_TO_CATALOG);
   } else {
      dir->fsend(_("2994 Bad verify level: %s\n"), dir->msg);
      return false;
   }

   dir->fsend(OKverify);

   generate_plugin_event(jcr, bEventLevel,(void *)(intptr_t)jcr->getJobLevel());
   generate_plugin_event(jcr, bEventStartVerifyJob);

   Dmsg1(110, "filed>dird: %s", dir->msg);

   switch (jcr->getJobLevel()) {
   case L_VERIFY_INIT:
   case L_VERIFY_CATALOG:
      do_verify(jcr);
      break;
   case L_VERIFY_VOLUME_TO_CATALOG:
      if (!open_sd_read_session(jcr)) {
         return false;
      }
      start_dir_heartbeat(jcr);
      do_verify_volume(jcr);
      stop_dir_heartbeat(jcr);
      /*
       * Send Close session command to Storage daemon
       */
      sd->fsend(read_close, jcr->Ticket);
      Dmsg1(130, "filed>stored: %s", sd->msg);

      /* ****FIXME**** check response */
      bget_msg(sd);                      /* get OK */

      /* Inform Storage daemon that we are done */
      sd->signal(BNET_TERMINATE);

      break;
   case L_VERIFY_DISK_TO_CATALOG:
      do_verify(jcr);
      break;
   default:
      dir->fsend(_("2994 Bad verify level: %s\n"), dir->msg);
      return false;
   }

   dir->signal(BNET_EOD);
   generate_plugin_event(jcr, bEventEndVerifyJob);
   return false;                      /* return and terminate command loop */
}

#if 0
#ifdef WIN32_VSS
static bool vss_restore_init_callback(JCR *jcr, int init_type)
{
   switch (init_type) {
   case VSS_INIT_RESTORE_AFTER_INIT:
      generate_plugin_event(jcr, bEventVssRestoreLoadComponentMetadata);
      return true;
   case VSS_INIT_RESTORE_AFTER_GATHER:
      generate_plugin_event(jcr, bEventVssRestoreSetComponentsSelected);
      return true;
   default:
      return false;
      break;
   }
}
#endif
#endif

/**
 * Do a Restore for Director
 *
 */
static bool restore_cmd(JCR *jcr)
{
   BSOCK *dir = jcr->dir_bsock;
   BSOCK *sd = jcr->store_bsock;
   POOLMEM *args;
   bool use_regexwhere = false;
   bool retval;
   int prefix_links;
   char replace;

   /*
    * See if we are in backup only mode then we don't allow a restore to be initiated.
    */
   if (backup_only_mode) {
      Jmsg(jcr, M_FATAL, 0, _("Filed in backup only mode, restores are not allowed, aborting...\n"));
      return false;
   }

   /*
    * See if we are allowed to run restore cmds.
    */
   if (!validate_command(jcr, "restore",
                        (jcr->director && jcr->director->allowed_job_cmds) ?
                         jcr->director->allowed_job_cmds :
                         me->allowed_job_cmds)) {
      return 0;
   }

   /**
    * Scan WHERE (base directory for restore) from command
    */
   Dmsg0(100, "restore command\n");
#if defined(WIN32_VSS)

   /**
    * No need to enable VSS for restore if we do not have plugin data to restore
    */
   enable_vss = jcr->got_metadata;

   Dmsg2(50, "g_pVSSClient = %p, enable_vss = %d\n", g_pVSSClient, enable_vss);
   /*
    * Capture state here, if client is backed up by multiple directors
    * and one enables vss and the other does not then enable_vss can change
    * between here and where its evaluated after the job completes.
    */
   jcr->VSS = g_pVSSClient && enable_vss;
   if (jcr->VSS) {
      /*
       * Run only one at a time
       */
      P(vss_mutex);
   }
#endif
   /*
    * Pickup where string
    */
   args = get_memory(dir->msglen+1);
   *args = 0;

   if (sscanf(dir->msg, restorecmd, &replace, &prefix_links, args) != 3) {
      if (sscanf(dir->msg, restorecmdR, &replace, &prefix_links, args) != 3){
         if (sscanf(dir->msg, restorecmd1, &replace, &prefix_links) != 2) {
            pm_strcpy(jcr->errmsg, dir->msg);
            Jmsg(jcr, M_FATAL, 0, _("Bad replace command. CMD=%s\n"), jcr->errmsg);
            return false;
         }
         *args = 0;
      }
      use_regexwhere = true;
   }
   /* Turn / into nothing */
   if (IsPathSeparator(args[0]) && args[1] == '\0') {
      args[0] = '\0';
   }

   Dmsg2(150, "Got replace %c, where=%s\n", replace, args);
   unbash_spaces(args);

   /* Keep track of newly created directories to apply them correct attributes */
   if (replace == REPLACE_NEVER) {
      jcr->keep_path_list = true;
   }

   if (use_regexwhere) {
      jcr->where_bregexp = get_bregexps(args);
      if (!jcr->where_bregexp) {
         Jmsg(jcr, M_FATAL, 0, _("Bad where regexp. where=%s\n"), args);
         free_pool_memory(args);
         return false;
      }
   } else {
      jcr->where = bstrdup(args);
   }

   free_pool_memory(args);
   jcr->replace = replace;
   jcr->prefix_links = prefix_links;

   dir->fsend(OKrestore);
   Dmsg1(110, "filed>dird: %s", dir->msg);

   jcr->setJobType(JT_RESTORE);

   jcr->setJobStatus(JS_Blocked);

   if (!open_sd_read_session(jcr)) {
      jcr->setJobStatus(JS_ErrorTerminated);
      goto bail_out;
   }

   jcr->setJobStatus(JS_Running);

   /**
    * Do restore of files and data
    */
   start_dir_heartbeat(jcr);
   generate_plugin_event(jcr, bEventStartRestoreJob);

#if defined(WIN32_VSS)
   /* START VSS ON WIN32 */
   if (jcr->VSS) {
      if (!g_pVSSClient->InitializeForRestore(jcr)) {
         berrno be;
         Jmsg(jcr, M_WARNING, 0, _("VSS was not initialized properly. VSS support is disabled. ERR=%s\n"), be.bstrerror());
      }
      //free_and_null_pool_memory(jcr->job_metadata);
      run_scripts(jcr, jcr->RunScripts, "ClientAfterVSS",
                 (jcr->director && jcr->director->allowed_script_dirs) ?
                  jcr->director->allowed_script_dirs :
                  me->allowed_script_dirs);
   }
#endif

   do_restore(jcr);
   stop_dir_heartbeat(jcr);

   jcr->setJobStatus(JS_Terminated);
   if (!jcr->is_JobStatus(JS_Terminated)) {
      bnet_suppress_error_messages(sd, 1);
   }

   /**
    * Send Close session command to Storage daemon
    */
   sd->fsend(read_close, jcr->Ticket);
   Dmsg1(100, "filed>stored: %s", sd->msg);

   bget_msg(sd);                      /* get OK */

   /* Inform Storage daemon that we are done */
   sd->signal(BNET_TERMINATE);

#if defined(WIN32_VSS)
   /* STOP VSS ON WIN32 */
   /* tell vss to close the restore session */
   Dmsg0(100, "About to call CloseRestore\n");
   if (jcr->VSS) {
#if 0
      generate_plugin_event(jcr, bEventVssBeforeCloseRestore);
#endif
      Dmsg0(100, "Really about to call CloseRestore\n");
      if (g_pVSSClient->CloseRestore()) {
         Dmsg0(100, "CloseRestore success\n");
#if 0
         /* inform user about writer states */
         for (int i=0; i<(int)g_pVSSClient->GetWriterCount(); i++) {
            int msg_type = M_INFO;
            if (g_pVSSClient->GetWriterState(i) < 1) {
               //msg_type = M_WARNING;
               //jcr->JobErrors++;
            }
            Jmsg(jcr, msg_type, 0, _("VSS Writer (RestoreComplete): %s\n"), g_pVSSClient->GetWriterInfo(i));
         }
#endif
      }
      else
         Dmsg1(100, "CloseRestore fail - %08x\n", errno);
      V(vss_mutex);
   }
#endif

bail_out:
   bfree_and_null(jcr->where);

   if (jcr->JobErrors) {
      jcr->setJobStatus(JS_ErrorTerminated);
   }

   Dmsg0(100, "Done in job.c\n");

   if (jcr->multi_restore) {
      Dmsg0(100, OKstoreend);
      dir->fsend(OKstoreend);
      retval = true;                  /* we continue the loop, waiting for next part */
   } else {
      retval = false;                 /* we stop here */
   }

   if (job_canceled(jcr)) {
      retval = false;                 /* we stop here */
   }

   if (!retval) {
      end_restore_cmd(jcr);           /* stopping so send bEventEndRestoreJob */
   }

   return retval;
}

static bool end_restore_cmd(JCR *jcr)
{
   Dmsg0(5, "end_restore_cmd\n");
   generate_plugin_event(jcr, bEventEndRestoreJob);
   return false;                      /* return and terminate command loop */
}

static bool open_sd_read_session(JCR *jcr)
{
   BSOCK *sd = jcr->store_bsock;

   if (!sd) {
      Jmsg(jcr, M_FATAL, 0, _("Improper calling sequence.\n"));
      return false;
   }
   Dmsg4(120, "VolSessId=%ld VolsessT=%ld SF=%ld EF=%ld\n",
      jcr->VolSessionId, jcr->VolSessionTime, jcr->StartFile, jcr->EndFile);
   Dmsg2(120, "JobId=%d vol=%s\n", jcr->JobId, "DummyVolume");
   /*
    * Open Read Session with Storage daemon
    */
   sd->fsend(read_open, "DummyVolume",
      jcr->VolSessionId, jcr->VolSessionTime, jcr->StartFile, jcr->EndFile,
      jcr->StartBlock, jcr->EndBlock);
   Dmsg1(110, ">stored: %s", sd->msg);

   /*
    * Get ticket number
    */
   if (bget_msg(sd) >= 0) {
      Dmsg1(110, "filed<stored: %s", sd->msg);
      if (sscanf(sd->msg, OK_open, &jcr->Ticket) != 1) {
         Jmsg(jcr, M_FATAL, 0, _("Bad response to SD read open: %s\n"), sd->msg);
         return false;
      }
      Dmsg1(110, "filed: got Ticket=%d\n", jcr->Ticket);
   } else {
      Jmsg(jcr, M_FATAL, 0, _("Bad response from stored to read open command\n"));
      return false;
   }

   /*
    * Start read of data with Storage daemon
    */
   sd->fsend(read_data, jcr->Ticket);
   Dmsg1(110, ">stored: %s", sd->msg);

   /*
    * Get OK data
    */
   if (!response(jcr, sd, OK_data, "Read Data")) {
      return false;
   }

   return true;
}

/**
 * Destroy the Job Control Record and associated
 * resources (sockets).
 */
static void filed_free_jcr(JCR *jcr)
{
   if (jcr->store_bsock) {
      jcr->store_bsock->close();
      delete jcr->store_bsock;
      jcr->store_bsock = NULL;
   }

   if (jcr->dir_bsock) {
      jcr->dir_bsock->close();
      delete jcr->dir_bsock;
      jcr->dir_bsock = NULL;
   }

   if (jcr->last_fname) {
      free_pool_memory(jcr->last_fname);
   }

   free_bootstrap(jcr);
   free_runscripts(jcr->RunScripts);
   delete jcr->RunScripts;

   if (jcr->path_list) {
      free_path_list(jcr->path_list);
      jcr->path_list = NULL;
   }

   if (jcr->JobId != 0) {
      write_state_file(me->working_directory, "bareos-fd", get_first_port_host_order(me->FDaddrs));
   }

   return;
}

/**
 * Get response from Storage daemon to a command we
 * sent. Check that the response is OK.
 *
 *  Returns: false on failure
 *           true on success
 */
bool response(JCR *jcr, BSOCK *sd, char *resp, const char *cmd)
{
   if (sd->errors) {
      return false;
   }
   if (bget_msg(sd) > 0) {
      Dmsg0(110, sd->msg);
      if (bstrcmp(sd->msg, resp)) {
         return true;
      }
   }
   if (job_canceled(jcr)) {
      return false;                   /* if canceled avoid useless error messages */
   }
   if (is_bnet_error(sd)) {
      Jmsg2(jcr, M_FATAL, 0, _("Comm error with SD. bad response to %s. ERR=%s\n"),
         cmd, bnet_strerror(sd));
   } else {
      Jmsg3(jcr, M_FATAL, 0, _("Bad response to %s command. Wanted %s, got %s\n"),
         cmd, resp, sd->msg);
   }
   return false;
}
