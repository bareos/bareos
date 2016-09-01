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
/**
 * BAREOS Director -- restore.c -- responsible for restoring files
 *
 * Kern Sibbald, November MM
 *
 * This routine is run as a separate thread.
 *
 * Current implementation is Catalog verification only (i.e. no verification versus tape).
 *
 * Basic tasks done here:
 *    Open DB
 *    Open Message Channel with Storage daemon to tell him a job will be starting.
 *    Open connection with File daemon and pass him commands to do the restore.
 */

#include "bareos.h"
#include "dird.h"

/* Commands sent to File daemon */
static char restorecmd[] =
   "restore replace=%c prelinks=%d where=%s\n";
static char restorecmdR[] =
   "restore replace=%c prelinks=%d regexwhere=%s\n";
static char storaddrcmd[] =
   "storage address=%s port=%d ssl=%d Authorization=%s\n";
static char setauthorizationcmd[] =
   "setauthorization Authorization=%s\n";
static char passiveclientcmd[] =
   "passive client address=%s port=%d ssl=%d\n";

/* Responses received from File daemon */
static char OKrestore[] =
   "2000 OK restore\n";
static char OKstore[] =
   "2000 OK storage\n";
static char OKstoreend[] =
   "2000 OK storage end\n";
static char OKAuthorization[] =
   "2000 OK Authorization\n";
static char OKpassiveclient[] =
   "2000 OK passive client\n";

/* Responses received from the Storage daemon */
static char OKbootstrap[] =
   "3000 OK bootstrap\n";

static void build_restore_command(JCR *jcr, POOL_MEM &ret)
{
   char replace, *where, *cmd;
   char empty = '\0';

   /*
    * Build the restore command
    */
   if (jcr->replace != 0) {
      replace = jcr->replace;
   } else if (jcr->res.job->replace != 0) {
      replace = jcr->res.job->replace;
   } else {
      replace = REPLACE_ALWAYS;       /* always replace */
   }

   if (jcr->RegexWhere) {
      where = jcr->RegexWhere;        /* override */
      cmd = restorecmdR;
   } else if (jcr->res.job->RegexWhere) {
      where = jcr->res.job->RegexWhere;   /* no override take from job */
      cmd = restorecmdR;
   } else if (jcr->where) {
      where = jcr->where;             /* override */
      cmd = restorecmd;
   } else if (jcr->res.job->RestoreWhere) {
      where = jcr->res.job->RestoreWhere; /* no override take from job */
      cmd = restorecmd;
   } else {                           /* nothing was specified */
      where = &empty;                 /* use default */
      cmd   = restorecmd;
   }

   jcr->prefix_links = jcr->res.job->PrefixLinks;

   bash_spaces(where);
   Mmsg(ret, cmd, replace, jcr->prefix_links, where);
   unbash_spaces(where);
}

/**
 * The bootstrap is stored in a file, so open the file, and loop
 *   through it processing each storage device in turn. If the
 *   storage is different from the prior one, we open a new connection
 *   to the new storage and do a restore for that part.
 *
 * This permits handling multiple storage daemons for a single
 *   restore.  E.g. your Full is stored on tape, and Incrementals
 *   on disk.
 */
static inline bool do_native_restore_bootstrap(JCR *jcr)
{
   STORERES *store;
   bootstrap_info info;
   BSOCK *fd = NULL;
   BSOCK *sd = NULL;
   bool first_time = true;
   POOL_MEM restore_cmd(PM_MESSAGE);

   /*
    * This command is used for each part
    */
   build_restore_command(jcr, restore_cmd);

   /*
    * Open the bootstrap file
    */
   if (!open_bootstrap_file(jcr, info)) {
      goto bail_out;
   }

   /*
    * Read the bootstrap file
    */
   jcr->passive_client = jcr->res.client->passive;
   while (!feof(info.bs)) {
      if (!select_next_rstore(jcr, info)) {
         goto bail_out;
      }
      store = jcr->res.rstore;

      /**
       * Open a message channel connection with the Storage
       * daemon. This is to let him know that our client
       * will be contacting him for a backup session.
       *
       */
      Dmsg0(10, "Open connection with storage daemon\n");
      jcr->setJobStatus(JS_WaitSD);

      /*
       * Start conversation with Storage daemon
       */
      if (!connect_to_storage_daemon(jcr, 10, me->SDConnectTimeout, true)) {
         goto bail_out;
      }
      sd = jcr->store_bsock;

      /*
       * Now start a job with the Storage daemon
       */
      if (!start_storage_daemon_job(jcr, jcr->res.rstorage, NULL)) {
         goto bail_out;
      }

      if (first_time) {
         /*
          * Start conversation with File daemon
          */
         jcr->setJobStatus(JS_WaitFD);
         jcr->keep_sd_auth_key = true; /* don't clear the sd_auth_key now */

         if (!connect_to_file_daemon(jcr, 10, me->FDConnectTimeout, true)) {
            goto bail_out;
         }
         send_job_info(jcr);
         fd = jcr->file_bsock;

         if (!send_secure_erase_req_to_fd(jcr)) {
            Dmsg1(500,"Unexpected %s secure erase\n","client");
         }

         /*
          * Check if the file daemon supports passive client mode.
          */
         if (jcr->passive_client && jcr->FDVersion < FD_VERSION_51) {
            Jmsg(jcr, M_FATAL, 0,
                  _("Client \"%s\" doesn't support passive client mode. "
                    "Please upgrade your client or disable compat mode.\n"),
                 jcr->res.client->name());
            goto bail_out;
         }
      }

      jcr->setJobStatus(JS_Running);

      /*
       * Send the bootstrap file -- what Volumes/files to restore
       */
      if (!send_bootstrap_file(jcr, sd, info) ||
          !response(jcr, sd, OKbootstrap, "Bootstrap", DISPLAY_ERROR)) {
         goto bail_out;
      }

      if (!jcr->passive_client) {
         int tls_need = BNET_TLS_NONE;

         /*
          * When the client is not in passive mode we can put the SD in
          * listen mode for the FD connection. And ask the FD to connect
          * to the SD.
          */
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
          * Send Storage daemon address to the File daemon,
          * then wait for File daemon to make connection
          * with Storage daemon.
          */
         if (store->SDDport == 0) {
            store->SDDport = store->SDport;
         }

         /*
          * TLS Requirement
          */
         if (store->tls.enable) {
            if (store->tls.require) {
               tls_need = BNET_TLS_REQUIRED;
            } else {
               tls_need = BNET_TLS_OK;
            }
         }

         fd->fsend(storaddrcmd, store->address,
                   store->SDDport, tls_need, jcr->sd_auth_key);
         memset(jcr->sd_auth_key, 0, strlen(jcr->sd_auth_key));

         Dmsg1(6, "dird>filed: %s", fd->msg);
         if (!response(jcr, fd, OKstore, "Storage", DISPLAY_ERROR)) {
            goto bail_out;
         }
      } else {
         int tls_need = BNET_TLS_NONE;
         CLIENTRES *client = jcr->res.client;
         /*
          * In passive mode we tell the FD what authorization key to use
          * and the ask the SD to initiate the connection.
          */
         fd->fsend(setauthorizationcmd, jcr->sd_auth_key);
         memset(jcr->sd_auth_key, 0, strlen(jcr->sd_auth_key));

         Dmsg1(6, "dird>filed: %s", fd->msg);
         if (!response(jcr, fd, OKAuthorization, "Setauthorization", DISPLAY_ERROR)) {
            goto bail_out;
         }

         /*
          * TLS Requirement
          */
         tls_need = BNET_TLS_NONE;
         if (client->tls.enable) {
            if (client->tls.require) {
               tls_need = BNET_TLS_REQUIRED;
            } else {
               tls_need = BNET_TLS_OK;
            }
         }

         /*
          * Tell the SD to connect to the FD.
          */
         sd->fsend(passiveclientcmd, client->address, client->FDport, tls_need);
         if (!response(jcr, sd, OKpassiveclient, "Passive client", DISPLAY_ERROR)) {
            goto bail_out;
         }

         /*
          * Start the Job in the SD.
          */
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
      }

      /*
       * Declare the job started to start the MaxRunTime check
       */
      jcr->setJobStarted();

      /*
       * Only pass "global" commands to the FD once
       */
      if (first_time) {
         first_time = false;
         if (!send_runscripts_commands(jcr)) {
            goto bail_out;
         }

         /*
          * Only FD version 52 and later understand the sending of plugin options.
          */
         if (jcr->FDVersion >= FD_VERSION_52) {
            if (!send_plugin_options(jcr)) {
               Dmsg0(000, "FAIL: Send plugin options\n");
               goto bail_out;
            }
         } else {
            /*
             * Plugin options specified and not a FD that understands the new protocol keyword.
             */
            if (jcr->plugin_options) {
               Jmsg(jcr, M_FATAL, 0,
                     _("Client \"%s\" doesn't support plugin option passing. "
                       "Please upgrade your client or disable compat mode.\n"),
                    jcr->res.client->name());
               goto bail_out;
            }
         }

         if (!send_restore_objects(jcr, 0, true)) {
            Dmsg0(000, "FAIL: Send restore objects\n");
            goto bail_out;
         }
      }

      fd->fsend("%s", restore_cmd.c_str());

      if (!response(jcr, fd, OKrestore, "Restore", DISPLAY_ERROR)) {
         goto bail_out;
      }

      if (jcr->FDVersion < FD_VERSION_2) { /* Old FD */
         break;                 /* we do only one loop */
      } else {
         if (!response(jcr, fd, OKstoreend, "Store end", DISPLAY_ERROR)) {
            goto bail_out;
         }
         wait_for_storage_daemon_termination(jcr);
      }
   } /* the whole boostrap has been send */

   if (fd && jcr->FDVersion >= FD_VERSION_2) {
      fd->fsend("endrestore");
   }

   close_bootstrap_file(info);
   return true;

bail_out:
   if (jcr->file_bsock) {
      jcr->file_bsock->signal(BNET_TERMINATE);
      jcr->file_bsock->close();
      delete jcr->file_bsock;
      jcr->file_bsock = NULL;
   }

   close_bootstrap_file(info);
   return false;
}

static inline bool validate_client(JCR *jcr)
{
   switch (jcr->res.client->Protocol) {
   case APT_NATIVE:
      return true;
   default:
      Jmsg(jcr, M_FATAL, 0, _("Client %s has illegal backup protocol %s for Native restore\n"),
           jcr->res.client->name(), auth_protocol_to_str(jcr->res.client->Protocol));
      return false;
   }
}

static inline bool validate_storage(JCR *jcr)
{
   STORERES *store;

   foreach_alist(store, jcr->res.rstorage) {
      switch (store->Protocol) {
      case APT_NATIVE:
         continue;
      default:
         Jmsg(jcr, M_FATAL, 0, _("Storage %s has illegal backup protocol %s for Native restore\n"),
              store->name(), auth_protocol_to_str(store->Protocol));
         return false;
      }
   }

   return true;
}

/**
 * Do a restore initialization.
 *
 *  Returns:  false on failure
 *            true on success
 */
bool do_native_restore_init(JCR *jcr)
{
   free_wstorage(jcr);                /* we don't write */

   /*
    * Validate that we have a native client and storage(s).
    */
   if (!validate_client(jcr) || !validate_storage(jcr)) {
      return false;
   }

   return true;
}

/**
 * Do a restore of the specified files
 *
 *  Returns:  false on failure
 *            true on success
 */
bool do_native_restore(JCR *jcr)
{
   JOB_DBR rjr;                       /* restore job record */
   int status;

   memset(&rjr, 0, sizeof(rjr));
   jcr->jr.JobLevel = L_FULL;         /* Full restore */
   if (!db_update_job_start_record(jcr, jcr->db, &jcr->jr)) {
      Jmsg(jcr, M_FATAL, 0, "%s", db_strerror(jcr->db));
      goto bail_out;
   }
   Dmsg0(20, "Updated job start record\n");

   Dmsg1(20, "RestoreJobId=%d\n", jcr->res.job->RestoreJobId);

   if (!jcr->RestoreBootstrap) {
      Jmsg(jcr, M_FATAL, 0, _("Cannot restore without a bootstrap file.\n"
          "You probably ran a restore job directly. All restore jobs must\n"
          "be run using the restore command.\n"));
      goto bail_out;
   }

   /*
    * Print Job Start message
    */
   Jmsg(jcr, M_INFO, 0, _("Start Restore Job %s\n"), jcr->Job);

   /*
    * Read the bootstrap file and do the restore
    */
   if (!do_native_restore_bootstrap(jcr)) {
      goto bail_out;
   }

   /*
    * Wait for Job Termination
    */
   status = wait_for_job_termination(jcr);
   native_restore_cleanup(jcr, status);
   return true;

bail_out:
   native_restore_cleanup(jcr, JS_ErrorTerminated);
   return false;
}

/**
 * Release resources allocated during restore.
 */
void native_restore_cleanup(JCR *jcr, int TermCode)
{
   char term_code[100];
   const char *term_msg;
   int msg_type = M_INFO;

   Dmsg0(20, "In native_restore_cleanup\n");
   update_job_end(jcr, TermCode);

   if (jcr->unlink_bsr && jcr->RestoreBootstrap) {
      secure_erase(jcr, jcr->RestoreBootstrap);
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
         if (jcr->SD_msg_chan_started) {
            pthread_cancel(jcr->SD_msg_chan);
         }
      }
      break;
   case JS_Canceled:
      term_msg = _("Restore Canceled");
      if (jcr->store_bsock) {
         jcr->store_bsock->signal(BNET_TERMINATE);
         if (jcr->SD_msg_chan_started) {
            pthread_cancel(jcr->SD_msg_chan);
         }
      }
      break;
   default:
      term_msg = term_code;
      sprintf(term_code, _("Inappropriate term code: %c\n"), TermCode);
      break;
   }

   generate_restore_summary(jcr, msg_type, term_msg);

   Dmsg0(20, "Leaving native_restore_cleanup\n");
}

/*
 * Generic function which generates a restore summary message.
 * Used by:
 *    - native_restore_cleanup e.g. normal restores
 *    - ndmp_restore_cleanup e.g. NDMP restores
 */
void generate_restore_summary(JCR *jcr, int msg_type, const char *term_msg)
{
   char sdt[MAX_TIME_LENGTH], edt[MAX_TIME_LENGTH];
   char ec1[30], ec2[30], ec3[30], elapsed[50];
   char fd_term_msg[100], sd_term_msg[100];
   utime_t RunTime;
   double kbps;
   POOL_MEM temp, secure_erase_status;

   bstrftimes(sdt, sizeof(sdt), jcr->jr.StartTime);
   bstrftimes(edt, sizeof(edt), jcr->jr.EndTime);
   RunTime = jcr->jr.EndTime - jcr->jr.StartTime;
   if (RunTime <= 0) {
      kbps = 0;
   } else {
      kbps = ((double)jcr->jr.JobBytes) / (1000.0 * (double)RunTime);
   }
   if (kbps < 0.05) {
      kbps = 0;
   }

   jobstatus_to_ascii(jcr->FDJobStatus, fd_term_msg, sizeof(fd_term_msg));
   jobstatus_to_ascii(jcr->SDJobStatus, sd_term_msg, sizeof(sd_term_msg));

   switch (jcr->getJobProtocol()) {
   case PT_NDMP:
      Jmsg(jcr, msg_type, 0, _("%s %s %s (%s):\n"
           "  Build OS:               %s %s %s\n"
           "  JobId:                  %d\n"
           "  Job:                    %s\n"
           "  Restore Client:         %s\n"
           "  Start time:             %s\n"
           "  End time:               %s\n"
           "  Elapsed time:           %s\n"
           "  Files Expected:         %s\n"
           "  Files Restored:         %s\n"
           "  Bytes Restored:         %s\n"
           "  Rate:                   %.1f KB/s\n"
           "  SD termination status:  %s\n"
           "  Termination:            %s\n\n"),
           BAREOS, my_name, VERSION, LSMDATE,
           HOST_OS, DISTNAME, DISTVER,
           jcr->jr.JobId,
           jcr->jr.Job,
           jcr->res.client->name(),
           sdt,
           edt,
           edit_utime(RunTime, elapsed, sizeof(elapsed)),
           edit_uint64_with_commas((uint64_t)jcr->ExpectedFiles, ec1),
           edit_uint64_with_commas((uint64_t)jcr->jr.JobFiles, ec2),
           edit_uint64_with_commas(jcr->jr.JobBytes, ec3),
           (float)kbps,
           sd_term_msg,
           term_msg);
      break;
   default:
      if (me->secure_erase_cmdline) {
         Mmsg(temp,"  Dir Secure Erase Cmd:   %s\n", me->secure_erase_cmdline);
         pm_strcat(secure_erase_status, temp.c_str());
      }
      if (!bstrcmp(jcr->FDSecureEraseCmd, "*None*")) {
         Mmsg(temp,"  FD  Secure Erase Cmd:   %s\n", jcr->FDSecureEraseCmd);
         pm_strcat(secure_erase_status, temp.c_str());
      }
      if (!bstrcmp(jcr->SDSecureEraseCmd, "*None*")) {
         Mmsg(temp,"  SD  Secure Erase Cmd:   %s\n", jcr->SDSecureEraseCmd);
         pm_strcat(secure_erase_status, temp.c_str());
      }

      Jmsg(jcr, msg_type, 0, _("%s %s %s (%s):\n"
           "  Build OS:               %s %s %s\n"
           "  JobId:                  %d\n"
           "  Job:                    %s\n"
           "  Restore Client:         %s\n"
           "  Start time:             %s\n"
           "  End time:               %s\n"
           "  Elapsed time:           %s\n"
           "  Files Expected:         %s\n"
           "  Files Restored:         %s\n"
           "  Bytes Restored:         %s\n"
           "  Rate:                   %.1f KB/s\n"
           "  FD Errors:              %d\n"
           "  FD termination status:  %s\n"
           "  SD termination status:  %s\n"
           "%s"
           "  Termination:            %s\n\n"),
           BAREOS, my_name, VERSION, LSMDATE,
           HOST_OS, DISTNAME, DISTVER,
           jcr->jr.JobId,
           jcr->jr.Job,
           jcr->res.client->name(),
           sdt,
           edt,
           edit_utime(RunTime, elapsed, sizeof(elapsed)),
           edit_uint64_with_commas((uint64_t)jcr->ExpectedFiles, ec1),
           edit_uint64_with_commas((uint64_t)jcr->jr.JobFiles, ec2),
           edit_uint64_with_commas(jcr->jr.JobBytes, ec3),
           (float)kbps,
           jcr->JobErrors,
           fd_term_msg,
           sd_term_msg,
           secure_erase_status.c_str(),
           term_msg);
      break;
   }
}
