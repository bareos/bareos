/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2000-2011 Free Software Foundation Europe e.V.

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
/**
 *   Bacula Director -- restore.c -- responsible for restoring files
 *
 *     Kern Sibbald, November MM
 *
 *    This routine is run as a separate thread.
 *
 * Current implementation is Catalog verification only (i.e. no
 *  verification versus tape).
 *
 *  Basic tasks done here:
 *     Open DB
 *     Open Message Channel with Storage daemon to tell him a job will be starting.
 *     Open connection with File daemon and pass him commands
 *       to do the restore.
 *     Update the DB according to what files where restored????
 *
 */


#include "bacula.h"
#include "dird.h"

/* Commands sent to File daemon */
static char restorecmd[]  = "restore replace=%c prelinks=%d where=%s\n";
static char restorecmdR[] = "restore replace=%c prelinks=%d regexwhere=%s\n";
static char storaddr[]    = "storage address=%s port=%d ssl=0 Authorization=%s\n";

/* Responses received from File daemon */
static char OKrestore[]   = "2000 OK restore\n";
static char OKstore[]     = "2000 OK storage\n";
static char OKstoreend[]  = "2000 OK storage end\n";

/* Responses received from the Storage daemon */
static char OKbootstrap[] = "3000 OK bootstrap\n";

static void build_restore_command(JCR *jcr, POOL_MEM &ret)
{
   char replace, *where, *cmd;
   char empty = '\0';

   /* Build the restore command */

   if (jcr->replace != 0) {
      replace = jcr->replace;
   } else if (jcr->job->replace != 0) {
      replace = jcr->job->replace;
   } else {
      replace = REPLACE_ALWAYS;       /* always replace */
   }
   
   if (jcr->RegexWhere) {
      where = jcr->RegexWhere;             /* override */
      cmd = restorecmdR;
   } else if (jcr->job->RegexWhere) {
      where = jcr->job->RegexWhere;   /* no override take from job */
      cmd = restorecmdR;

   } else if (jcr->where) {
      where = jcr->where;             /* override */
      cmd = restorecmd;
   } else if (jcr->job->RestoreWhere) {
      where = jcr->job->RestoreWhere; /* no override take from job */
      cmd = restorecmd;

   } else {                           /* nothing was specified */
      where = &empty;                 /* use default */
      cmd   = restorecmd;                    
   }
   
   jcr->prefix_links = jcr->job->PrefixLinks;

   bash_spaces(where);
   Mmsg(ret, cmd, replace, jcr->prefix_links, where);
   unbash_spaces(where);
}

struct bootstrap_info
{
   FILE *bs;
   UAContext *ua;
   char storage[MAX_NAME_LENGTH+1];
};

#define UA_CMD_SIZE 1000

/**
 * Open the bootstrap file and find the first Storage=
 * Returns ok if able to open
 * It fills the storage name (should be the first line) 
 * and the file descriptor to the bootstrap file, 
 * it should be used for next operations, and need to be closed
 * at the end.
 */
static bool open_bootstrap_file(JCR *jcr, bootstrap_info &info)
{
   FILE *bs;
   UAContext *ua;
   info.bs = NULL;
   info.ua = NULL;

   if (!jcr->RestoreBootstrap) {
      return false;
   }
   strncpy(info.storage, jcr->rstore->name(), MAX_NAME_LENGTH);

   bs = fopen(jcr->RestoreBootstrap, "rb");
   if (!bs) {
      berrno be;
      Jmsg(jcr, M_FATAL, 0, _("Could not open bootstrap file %s: ERR=%s\n"),
         jcr->RestoreBootstrap, be.bstrerror());
      jcr->setJobStatus(JS_ErrorTerminated);
      return false;
   }

   ua = new_ua_context(jcr);
   ua->cmd = check_pool_memory_size(ua->cmd, UA_CMD_SIZE+1);
   while (!fgets(ua->cmd, UA_CMD_SIZE, bs)) {
      parse_ua_args(ua);
      if (ua->argc != 1) {
         continue;
      }
      if (!strcasecmp(ua->argk[0], "Storage")) {
         strncpy(info.storage, ua->argv[0], MAX_NAME_LENGTH);
         break;
      }
   }
   info.bs = bs;
   info.ua = ua;
   fseek(bs, 0, SEEK_SET);      /* return to the top of the file */
   return true;
}

/**
 * This function compare the given storage name with the
 * the current one. We compare the name and the address:port.
 * Returns true if we use the same storage.
 */
static bool is_on_same_storage(JCR *jcr, char *new_one)
{
   STORE *new_store;

   /* with old FD, we send the whole bootstrap to the storage */
   if (jcr->FDVersion < 2) {
      return true;
   }
   /* we are in init loop ? shoudn't fail here */
   if (!*new_one) {
      return true;
   }
   /* same name */
   if (!strcmp(new_one, jcr->rstore->name())) {
      return true;
   }
   new_store = (STORE *)GetResWithName(R_STORAGE, new_one);
   if (!new_store) {
      Jmsg(jcr, M_WARNING, 0,
           _("Could not get storage resource '%s'.\n"), new_one);
      return false;
   }
   /* if Port and Hostname/IP are same, we are talking to the same
    * Storage Daemon
    */
   if (jcr->rstore->SDport != new_store->SDport ||
       strcmp(jcr->rstore->address, new_store->address))
   {
      return false;
   }
   return true;
}

/**
 * Check if the current line contains Storage="xxx", and compare the
 * result to the current storage. We use UAContext to analyse the bsr 
 * string.
 *
 * Returns true if we need to change the storage, and it set the new
 * Storage resource name in "storage" arg. 
 */
static bool check_for_new_storage(JCR *jcr, bootstrap_info &info)
{
   UAContext *ua = info.ua;
   parse_ua_args(ua);
   if (ua->argc != 1) {
      return false;
   }
   if (!strcasecmp(ua->argk[0], "Storage")) {
      /* Continue if this is a volume from the same storage. */
      if (is_on_same_storage(jcr, ua->argv[0])) {
         return false;
      }
      /* note the next storage name */
      strncpy(info.storage, ua->argv[0], MAX_NAME_LENGTH);
      Dmsg1(5, "Change storage to %s\n", info.storage);
      return true;
   }
   return false;
}

/**
 * Send bootstrap file to Storage daemon section by section.
 */
static bool send_bootstrap_file(JCR *jcr, BSOCK *sock,
                                bootstrap_info &info)
{
   boffset_t pos;
   const char *bootstrap = "bootstrap\n";
   UAContext *ua = info.ua;
   FILE *bs = info.bs;

   Dmsg1(400, "send_bootstrap_file: %s\n", jcr->RestoreBootstrap);
   if (!jcr->RestoreBootstrap) {
      return false;
   }
   sock->fsend(bootstrap);
   pos = ftello(bs);
   while(fgets(ua->cmd, UA_CMD_SIZE, bs)) {
      if (check_for_new_storage(jcr, info)) {
         /* Otherwise, we need to contact another storage daemon.
          * Reset bs to the beginning of the current segment. 
          */
         fseeko(bs, pos, SEEK_SET);
         break;
      }
      sock->fsend("%s", ua->cmd);
      pos = ftello(bs);
   }
   sock->signal(BNET_EOD);
   return true;
}

#define MAX_TRIES 6 * 360   /* 6 hours */

/**
 * Change the read storage resource for the current job.
 */
static bool select_rstore(JCR *jcr, bootstrap_info &info)
{
   USTORE ustore;
   int i;


   if (!strcmp(jcr->rstore->name(), info.storage)) {
      return true;                 /* same SD nothing to change */
   }

   if (!(ustore.store = (STORE *)GetResWithName(R_STORAGE,info.storage))) {
      Jmsg(jcr, M_FATAL, 0,
           _("Could not get storage resource '%s'.\n"), info.storage);
      jcr->setJobStatus(JS_ErrorTerminated);
      return false;
   }
   
   /*
    * What does this do???????????  KES
    */
   if (jcr->store_bsock) {
      jcr->store_bsock->destroy();
      jcr->store_bsock = NULL;
   }
   
   /*
    * release current read storage and get a new one 
    */
   dec_read_store(jcr);
   free_rstorage(jcr);
   set_rstorage(jcr, &ustore);
   jcr->setJobStatus(JS_WaitSD);
   /*
    * Wait for up to 6 hours to increment read stoage counter 
    */
   for (i=0; i < MAX_TRIES; i++) {
      /* try to get read storage counter incremented */
      if (inc_read_store(jcr)) {
         jcr->setJobStatus(JS_Running);
         return true;
      }
      bmicrosleep(10, 0);       /* sleep 10 secs */
      if (job_canceled(jcr)) {
         free_rstorage(jcr);
         return false;
      }
   }
   /* Failed to inc_read_store() */
   free_rstorage(jcr);
   Jmsg(jcr, M_FATAL, 0, 
      _("Could not acquire read storage lock for \"%s\""), info.storage);
   return false;
}

/* 
 * Clean the bootstrap_info struct
 */
static void close_bootstrap_file(bootstrap_info &info)
{
   if (info.bs) {
      fclose(info.bs);
   }
   if (info.ua) {
      free_ua_context(info.ua);
   }
}

/**
 * The bootstrap is stored in a file, so open the file, and loop
 *   through it processing each storage device in turn. If the
 *   storage is different from the prior one, we open a new connection
 *   to the new storage and do a restore for that part.
 * This permits handling multiple storage daemons for a single
 *   restore.  E.g. your Full is stored on tape, and Incrementals
 *   on disk.
 */
bool restore_bootstrap(JCR *jcr)
{
   BSOCK *fd = NULL;
   BSOCK *sd;
   bool first_time = true;
   bootstrap_info info;
   POOL_MEM restore_cmd(PM_MESSAGE);
   bool ret = false;

   /* this command is used for each part */
   build_restore_command(jcr, restore_cmd);
   
   /* Open the bootstrap file */
   if (!open_bootstrap_file(jcr, info)) {
      goto bail_out;
   }
   /* Read the bootstrap file */
   while (!feof(info.bs)) {
      
      if (!select_rstore(jcr, info)) {
         goto bail_out;
      }

      /**
       * Open a message channel connection with the Storage
       * daemon. This is to let him know that our client
       * will be contacting him for a backup  session.
       *
       */
      Dmsg0(10, "Open connection with storage daemon\n");
      jcr->setJobStatus(JS_WaitSD);
      /*
       * Start conversation with Storage daemon
       */
      if (!connect_to_storage_daemon(jcr, 10, SDConnectTimeout, 1)) {
         goto bail_out;
      }
      sd = jcr->store_bsock;
      /*
       * Now start a job with the Storage daemon
       */
      if (!start_storage_daemon_job(jcr, jcr->rstorage, NULL)) {
         goto bail_out;
      }

      if (first_time) {
         /*
          * Start conversation with File daemon
          */
         jcr->setJobStatus(JS_WaitFD);
         jcr->keep_sd_auth_key = true; /* don't clear the sd_auth_key now */
         if (!connect_to_file_daemon(jcr, 10, FDConnectTimeout, 1)) {
            goto bail_out;
         }
         fd = jcr->file_bsock;
      }

      jcr->setJobStatus(JS_Running);

      /*
       * Send the bootstrap file -- what Volumes/files to restore
       */
      if (!send_bootstrap_file(jcr, sd, info) ||
          !response(jcr, sd, OKbootstrap, "Bootstrap", DISPLAY_ERROR)) {
         goto bail_out;
      }

      if (!sd->fsend("run")) {
         goto bail_out;
      }
      /*
       * Now start a Storage daemon message thread
       */
      if (!start_storage_daemon_message_thread(jcr)) {
         goto bail_out;
      }
      Dmsg0(50, "Storage daemon connection OK\n");

      /*
       * send Storage daemon address to the File daemon,
       *   then wait for File daemon to make connection
       *   with Storage daemon.
       */
      if (jcr->rstore->SDDport == 0) {
         jcr->rstore->SDDport = jcr->rstore->SDport;
      }
      fd->fsend(storaddr, jcr->rstore->address, jcr->rstore->SDDport,
                jcr->sd_auth_key);
      memset(jcr->sd_auth_key, 0, strlen(jcr->sd_auth_key));

      Dmsg1(6, "dird>filed: %s\n", fd->msg);
      if (!response(jcr, fd, OKstore, "Storage", DISPLAY_ERROR)) {
         goto bail_out;
      }

      /* Declare the job started to start the MaxRunTime check */
      jcr->setJobStarted();

      /* Only pass "global" commands to the FD once */
      if (first_time) {
         first_time = false;
         if (!send_runscripts_commands(jcr)) {
            goto bail_out;
         }
         if (!send_restore_objects(jcr)) {
            Dmsg0(000, "FAIL: Send restore objects\n");
            goto bail_out;
         }
      }

      fd->fsend("%s", restore_cmd.c_str());

      if (!response(jcr, fd, OKrestore, "Restore", DISPLAY_ERROR)) {
         goto bail_out;
      }

      if (jcr->FDVersion < 2) { /* Old FD */
         break;                 /* we do only one loop */
      } else {
         if (!response(jcr, fd, OKstoreend, "Store end", DISPLAY_ERROR)) {
            goto bail_out;
         }
         wait_for_storage_daemon_termination(jcr);
      }
   } /* the whole boostrap has been send */

   if (fd && jcr->FDVersion >= 2) {
      fd->fsend("endrestore");
   }

   ret = true;

bail_out:
   close_bootstrap_file(info);
   return ret;
}

/**
 * Do a restore of the specified files
 *
 *  Returns:  0 on failure
 *            1 on success
 */
bool do_restore(JCR *jcr)
{
   JOB_DBR rjr;                       /* restore job record */
   int stat;

   free_wstorage(jcr);                /* we don't write */

   if (!allow_duplicate_job(jcr)) {
      goto bail_out;
   }

   memset(&rjr, 0, sizeof(rjr));
   jcr->jr.JobLevel = L_FULL;         /* Full restore */
   if (!db_update_job_start_record(jcr, jcr->db, &jcr->jr)) {
      Jmsg(jcr, M_FATAL, 0, "%s", db_strerror(jcr->db));
      goto bail_out;
   }
   Dmsg0(20, "Updated job start record\n");

   Dmsg1(20, "RestoreJobId=%d\n", jcr->job->RestoreJobId);

   if (!jcr->RestoreBootstrap) {
      Jmsg(jcr, M_FATAL, 0, _("Cannot restore without a bootstrap file.\n"
          "You probably ran a restore job directly. All restore jobs must\n"
          "be run using the restore command.\n"));
      goto bail_out;
   }


   /* Print Job Start message */
   Jmsg(jcr, M_INFO, 0, _("Start Restore Job %s\n"), jcr->Job);

   /* Read the bootstrap file and do the restore */
   if (!restore_bootstrap(jcr)) {
      goto bail_out;
   }

   /* Wait for Job Termination */
   stat = wait_for_job_termination(jcr);
   restore_cleanup(jcr, stat);
   return true;

bail_out:
   restore_cleanup(jcr, JS_ErrorTerminated);
   return false;
}

bool do_restore_init(JCR *jcr) 
{
   free_wstorage(jcr);
   return true;
}

/**
 * Release resources allocated during restore.
 *
 */
void restore_cleanup(JCR *jcr, int TermCode)
{
   char sdt[MAX_TIME_LENGTH], edt[MAX_TIME_LENGTH];
   char ec1[30], ec2[30], ec3[30];
   char term_code[100], fd_term_msg[100], sd_term_msg[100];
   const char *term_msg;
   int msg_type = M_INFO;
   double kbps;

   Dmsg0(20, "In restore_cleanup\n");
   update_job_end(jcr, TermCode);

   if (jcr->unlink_bsr && jcr->RestoreBootstrap) {
      unlink(jcr->RestoreBootstrap);
      jcr->unlink_bsr = false;
   }

   if (job_canceled(jcr)) {
      cancel_storage_daemon_job(jcr);
   }  

   switch (TermCode) {
   case JS_Terminated:
      if (jcr->ExpectedFiles > jcr->jr.JobFiles) {
         term_msg = _("Restore OK -- warning file count mismatch");
      } else {
         term_msg = _("Restore OK");
      }
      break;
   case JS_Warnings:
         term_msg = _("Restore OK -- with warnings");
         break;
   case JS_FatalError:
   case JS_ErrorTerminated:
      term_msg = _("*** Restore Error ***");
      msg_type = M_ERROR;          /* Generate error message */
      if (jcr->store_bsock) {
         jcr->store_bsock->signal(BNET_TERMINATE);
         if (jcr->SD_msg_chan) {
            pthread_cancel(jcr->SD_msg_chan);
         }
      }
      break;
   case JS_Canceled:
      term_msg = _("Restore Canceled");
      if (jcr->store_bsock) {
         jcr->store_bsock->signal(BNET_TERMINATE);
         if (jcr->SD_msg_chan) {
            pthread_cancel(jcr->SD_msg_chan);
         }
      }
      break;
   default:
      term_msg = term_code;
      sprintf(term_code, _("Inappropriate term code: %c\n"), TermCode);
      break;
   }
   bstrftimes(sdt, sizeof(sdt), jcr->jr.StartTime);
   bstrftimes(edt, sizeof(edt), jcr->jr.EndTime);
   if (jcr->jr.EndTime - jcr->jr.StartTime > 0) {
      kbps = (double)jcr->jr.JobBytes / (1000 * (jcr->jr.EndTime - jcr->jr.StartTime));
   } else {
      kbps = 0;
   }
   if (kbps < 0.05) {
      kbps = 0;
   }

   jobstatus_to_ascii(jcr->FDJobStatus, fd_term_msg, sizeof(fd_term_msg));
   jobstatus_to_ascii(jcr->SDJobStatus, sd_term_msg, sizeof(sd_term_msg));

   Jmsg(jcr, msg_type, 0, _("%s %s %s (%s):\n"
"  Build OS:               %s %s %s\n"
"  JobId:                  %d\n"
"  Job:                    %s\n"
"  Restore Client:         %s\n"
"  Start time:             %s\n"
"  End time:               %s\n"
"  Files Expected:         %s\n"
"  Files Restored:         %s\n"
"  Bytes Restored:         %s\n"
"  Rate:                   %.1f KB/s\n"
"  FD Errors:              %d\n"
"  FD termination status:  %s\n"
"  SD termination status:  %s\n"
"  Termination:            %s\n\n"),
        BACULA, my_name, VERSION, LSMDATE,
        HOST_OS, DISTNAME, DISTVER,
        jcr->jr.JobId,
        jcr->jr.Job,
        jcr->client->name(),
        sdt,
        edt,
        edit_uint64_with_commas((uint64_t)jcr->ExpectedFiles, ec1),
        edit_uint64_with_commas((uint64_t)jcr->jr.JobFiles, ec2),
        edit_uint64_with_commas(jcr->jr.JobBytes, ec3),
        (float)kbps,
        jcr->JobErrors,
        fd_term_msg,
        sd_term_msg,
        term_msg);

   Dmsg0(20, "Leaving restore_cleanup\n");
}
