/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2011 Free Software Foundation Europe e.V.
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
// Kern Sibbald, November MM
/**
 * @file
 * responsible for restoring files
 *
 * This routine is run as a separate thread.
 *
 * Current implementation is Catalog verification only (i.e. no verification
 * versus tape).
 *
 * Basic tasks done here:
 *    Open DB
 *    Open Message Channel with Storage daemon to tell him a job will be
 * starting. Open connection with File daemon and pass him commands to do the
 * restore.
 */


#include "include/bareos.h"
#include "dird.h"
#include "dird/dird_globals.h"
#include "dird/backup.h"
#include "dird/fd_cmds.h"
#include "dird/getmsg.h"
#include "dird/jcr_private.h"
#include "dird/job.h"
#include "dird/msgchan.h"
#include "dird/restore.h"
#include "dird/sd_cmds.h"
#include "dird/storage.h"
#include "include/protocol_types.h"
#include "lib/edit.h"
#include "lib/util.h"

namespace directordaemon {

/* Commands sent to File daemon */
static char restorecmd[] = "restore replace=%c prelinks=%d where=%s\n";
static char restorecmdR[] = "restore replace=%c prelinks=%d regexwhere=%s\n";
static char storaddrcmd[]
    = "storage address=%s port=%d ssl=%d Authorization=%s\n";
static char setauthorizationcmd[] = "setauthorization Authorization=%s\n";
static char passiveclientcmd[] = "passive client address=%s port=%d ssl=%d\n";

/* Responses received from File daemon */
static char OKrestore[] = "2000 OK restore\n";
static char OKstore[] = "2000 OK storage\n";
static char OKstoreend[] = "2000 OK storage end\n";
static char OKAuthorization[] = "2000 OK Authorization\n";
static char OKpassiveclient[] = "2000 OK passive client\n";

/* Responses received from the Storage daemon */
static char OKbootstrap[] = "3000 OK bootstrap\n";

static void BuildRestoreCommand(JobControlRecord* jcr, PoolMem& ret)
{
  char replace, *where, *cmd;
  char empty = '\0';

  // Build the restore command
  if (jcr->impl->replace != 0) {
    replace = jcr->impl->replace;
  } else if (jcr->impl->res.job->replace != 0) {
    replace = jcr->impl->res.job->replace;
  } else {
    replace = REPLACE_ALWAYS; /* always replace */
  }

  if (jcr->RegexWhere) {
    where = jcr->RegexWhere; /* override */
    cmd = restorecmdR;
  } else if (jcr->impl->res.job->RegexWhere) {
    where = jcr->impl->res.job->RegexWhere; /* no override take from job */
    cmd = restorecmdR;
  } else if (jcr->where) {
    where = jcr->where; /* override */
    cmd = restorecmd;
  } else if (jcr->impl->res.job->RestoreWhere) {
    where = jcr->impl->res.job->RestoreWhere; /* no override take from job */
    cmd = restorecmd;
  } else {          /* nothing was specified */
    where = &empty; /* use default */
    cmd = restorecmd;
  }

  jcr->prefix_links = jcr->impl->res.job->PrefixLinks;

  BashSpaces(where);
  Mmsg(ret, cmd, replace, jcr->prefix_links, where);
  UnbashSpaces(where);
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
static inline bool DoNativeRestoreBootstrap(JobControlRecord* jcr)
{
  StorageResource* store;
  ClientResource* client;
  bootstrap_info info;
  BareosSocket* fd = NULL;
  BareosSocket* sd = NULL;
  bool first_time = true;
  PoolMem RestoreCmd(PM_MESSAGE);
  char* connection_target_address;

  client = jcr->impl->res.client;
  // This command is used for each part
  BuildRestoreCommand(jcr, RestoreCmd);

  // Open the bootstrap file
  if (!OpenBootstrapFile(jcr, info)) { goto bail_out; }

  // Read the bootstrap file
  jcr->passive_client = client->passive;
  while (!feof(info.bs)) {
    if (!SelectNextRstore(jcr, info)) { goto bail_out; }
    store = jcr->impl->res.read_storage;

    /**
     * Open a message channel connection with the Storage
     * daemon. This is to let him know that our client
     * will be contacting him for a backup session.
     *
     */
    Dmsg0(10, "Open connection with storage daemon\n");
    jcr->setJobStatus(JS_WaitSD);

    // Start conversation with Storage daemon
    if (!ConnectToStorageDaemon(jcr, 10, me->SDConnectTimeout, true)) {
      goto bail_out;
    }
    sd = jcr->store_bsock;

    // Now start a job with the Storage daemon
    if (!StartStorageDaemonJob(jcr, jcr->impl->res.read_storage_list, NULL)) {
      goto bail_out;
    }

    if (first_time) {
      // Start conversation with File daemon
      jcr->setJobStatus(JS_WaitFD);
      jcr->impl->keep_sd_auth_key = true; /* don't clear the sd_auth_key now */

      if (!ConnectToFileDaemon(jcr, 10, me->FDConnectTimeout, true)) {
        goto bail_out;
      }
      SendJobInfoToFileDaemon(jcr);
      fd = jcr->file_bsock;

      if (!SendSecureEraseReqToFd(jcr)) {
        Dmsg1(500, "Unexpected %s secure erase\n", "client");
      }

      // Check if the file daemon supports passive client mode.
      if (jcr->passive_client && jcr->impl->FDVersion < FD_VERSION_51) {
        Jmsg(jcr, M_FATAL, 0,
             _("Client \"%s\" doesn't support passive client mode. "
               "Please upgrade your client or disable compat mode.\n"),
             jcr->impl->res.client->resource_name_);
        goto bail_out;
      }
    }

    jcr->setJobStatus(JS_Running);

    // Send the bootstrap file -- what Volumes/files to restore
    bool success = false;
    if (SendBootstrapFile(jcr, sd, info)) {
      Bmicrosleep(2, 0);
      if (response(jcr, sd, OKbootstrap, "Bootstrap", DISPLAY_ERROR)) {
        success = true;
      }
    }
    if (!success) { goto bail_out; }

    if (!jcr->passive_client) {
      /*
       * When the client is not in passive mode we can put the SD in
       * listen mode for the FD connection. And ask the FD to connect
       * to the SD.
       */
      if (!sd->fsend("run")) { goto bail_out; }

      // Now start a Storage daemon message thread
      if (!StartStorageDaemonMessageThread(jcr)) { goto bail_out; }
      Dmsg0(50, "Storage daemon connection OK\n");

      /*
       * Send Storage daemon address to the File daemon,
       * then wait for File daemon to make connection
       * with Storage daemon.
       */

      // TLS Requirement

      TlsPolicy tls_policy;
      if (jcr->impl->res.client->connection_successful_handshake_
          != ClientConnectionHandshakeMode::kTlsFirst) {
        tls_policy = store->GetPolicy();
      } else {
        tls_policy = store->IsTlsConfigured() ? TlsPolicy::kBnetTlsAuto
                                              : TlsPolicy::kBnetTlsNone;
      }

      Dmsg1(200, "Tls Policy for active client is: %d\n", tls_policy);

      connection_target_address = StorageAddressToContact(client, store);

      fd->fsend(storaddrcmd, connection_target_address, store->SDport,
                tls_policy, jcr->sd_auth_key);
      memset(jcr->sd_auth_key, 0, strlen(jcr->sd_auth_key));

      Dmsg1(6, "dird>filed: %s", fd->msg);
      if (!response(jcr, fd, OKstore, "Storage", DISPLAY_ERROR)) {
        goto bail_out;
      }
    } else {
      /*
       * In passive mode we tell the FD what authorization key to use
       * and the ask the SD to initiate the connection.
       */
      fd->fsend(setauthorizationcmd, jcr->sd_auth_key);
      memset(jcr->sd_auth_key, 0, strlen(jcr->sd_auth_key));

      Dmsg1(6, "dird>filed: %s", fd->msg);
      if (!response(jcr, fd, OKAuthorization, "Setauthorization",
                    DISPLAY_ERROR)) {
        goto bail_out;
      }

      TlsPolicy tls_policy;

      if (jcr->impl->res.client->connection_successful_handshake_
          != ClientConnectionHandshakeMode::kTlsFirst) {
        tls_policy = client->GetPolicy();
      } else {
        tls_policy = client->IsTlsConfigured() ? TlsPolicy::kBnetTlsAuto
                                               : TlsPolicy::kBnetTlsNone;
      }

      Dmsg1(200, "Tls Policy for passive client is: %d\n", tls_policy);

      connection_target_address = ClientAddressToContact(client, store);
      // Tell the SD to connect to the FD.
      sd->fsend(passiveclientcmd, connection_target_address, client->FDport,
                tls_policy);
      Bmicrosleep(2, 0);
      if (!response(jcr, sd, OKpassiveclient, "Passive client",
                    DISPLAY_ERROR)) {
        goto bail_out;
      }

      // Start the Job in the SD.
      if (!sd->fsend("run")) { goto bail_out; }

      // Now start a Storage daemon message thread
      if (!StartStorageDaemonMessageThread(jcr)) { goto bail_out; }
      Dmsg0(50, "Storage daemon connection OK\n");
    }

    // Declare the job started to start the MaxRunTime check
    jcr->setJobStarted();

    // Only pass "global" commands to the FD once
    if (first_time) {
      first_time = false;
      if (!SendRunscriptsCommands(jcr)) { goto bail_out; }

      // Only FD version 52 and later understand the sending of plugin options.
      if (jcr->impl->FDVersion >= FD_VERSION_52) {
        if (!SendPluginOptions(jcr)) {
          Dmsg0(000, "FAIL: Send plugin options\n");
          goto bail_out;
        }
      } else {
        /*
         * Plugin options specified and not a FD that understands the new
         * protocol keyword.
         */
        if (jcr->impl->plugin_options) {
          Jmsg(jcr, M_FATAL, 0,
               _("Client \"%s\" doesn't support plugin option passing. "
                 "Please upgrade your client or disable compat mode.\n"),
               jcr->impl->res.client->resource_name_);
          goto bail_out;
        }
      }

      if (!SendRestoreObjects(jcr, 0, true)) {
        Dmsg0(000, "FAIL: Send restore objects\n");
        goto bail_out;
      }
    }

    fd->fsend("%s", RestoreCmd.c_str());

    if (!response(jcr, fd, OKrestore, "Restore", DISPLAY_ERROR)) {
      goto bail_out;
    }

    if (jcr->impl->FDVersion < FD_VERSION_2) { /* Old FD */
      break;                                   /* we do only one loop */
    } else {
      if (!response(jcr, fd, OKstoreend, "Store end", DISPLAY_ERROR)) {
        goto bail_out;
      }
      WaitForStorageDaemonTermination(jcr);
    }
  } /* the whole boostrap has been send */

  if (fd && jcr->impl->FDVersion >= FD_VERSION_2) { fd->fsend("endrestore"); }

  CloseBootstrapFile(info);
  return true;

bail_out:
  if (jcr->file_bsock) {
    jcr->file_bsock->signal(BNET_TERMINATE);
    jcr->file_bsock->close();
    delete jcr->file_bsock;
    jcr->file_bsock = NULL;
  }

  CloseBootstrapFile(info);
  return false;
}

/**
 * Do a restore initialization.
 *
 *  Returns:  false on failure
 *            true on success
 */
bool DoNativeRestoreInit(JobControlRecord* jcr)
{
  FreeWstorage(jcr); /* we don't write */

  return true;
}

/**
 * Do a restore of the specified files
 *
 *  Returns:  false on failure
 *            true on success
 */
bool DoNativeRestore(JobControlRecord* jcr)
{
  int status;

  jcr->impl->jr.JobLevel = L_FULL; /* Full restore */
  if (!jcr->db->UpdateJobStartRecord(jcr, &jcr->impl->jr)) {
    Jmsg(jcr, M_FATAL, 0, "%s", jcr->db->strerror());
    goto bail_out;
  }
  Dmsg0(20, "Updated job start record\n");

  Dmsg1(20, "RestoreJobId=%d\n", jcr->impl->res.job->RestoreJobId);

  if (!jcr->RestoreBootstrap) {
    Jmsg(jcr, M_FATAL, 0,
         _("Cannot restore without a bootstrap file.\n"
           "You probably ran a restore job directly. All restore jobs must\n"
           "be run using the restore command.\n"));
    goto bail_out;
  }

  // Print Job Start message
  Jmsg(jcr, M_INFO, 0, _("Start Restore Job %s\n"), jcr->Job);

  // Read the bootstrap file and do the restore
  if (!DoNativeRestoreBootstrap(jcr)) { goto bail_out; }

  // Wait for Job Termination
  status = WaitForJobTermination(jcr);
  NativeRestoreCleanup(jcr, status);
  return true;

bail_out:
  NativeRestoreCleanup(jcr, JS_ErrorTerminated);
  return false;
}

// Release resources allocated during restore.
void NativeRestoreCleanup(JobControlRecord* jcr, int TermCode)
{
  char term_code[100];
  const char* TermMsg;
  int msg_type = M_INFO;

  Dmsg0(20, "In NativeRestoreCleanup\n");
  UpdateJobEnd(jcr, TermCode);

  if (jcr->impl->unlink_bsr && jcr->RestoreBootstrap) {
    SecureErase(jcr, jcr->RestoreBootstrap);
    jcr->impl->unlink_bsr = false;
  }

  if (JobCanceled(jcr)) { CancelStorageDaemonJob(jcr); }

  switch (TermCode) {
    case JS_Terminated:
      if (jcr->impl->ExpectedFiles > jcr->impl->jr.JobFiles) {
        TermMsg = _("Restore OK -- warning file count mismatch");
      } else {
        TermMsg = _("Restore OK");
      }
      break;
    case JS_Warnings:
      TermMsg = _("Restore OK -- with warnings");
      break;
    case JS_FatalError:
    case JS_ErrorTerminated:
      TermMsg = _("*** Restore Error ***");
      msg_type = M_ERROR; /* Generate error message */
      if (jcr->store_bsock) {
        jcr->store_bsock->signal(BNET_TERMINATE);
        if (jcr->impl->SD_msg_chan_started) {
          pthread_cancel(jcr->impl->SD_msg_chan);
        }
      }
      break;
    case JS_Canceled:
      TermMsg = _("Restore Canceled");
      if (jcr->store_bsock) {
        jcr->store_bsock->signal(BNET_TERMINATE);
        if (jcr->impl->SD_msg_chan_started) {
          pthread_cancel(jcr->impl->SD_msg_chan);
        }
      }
      break;
    default:
      TermMsg = term_code;
      sprintf(term_code, _("Inappropriate term code: %c\n"), TermCode);
      break;
  }

  GenerateRestoreSummary(jcr, msg_type, TermMsg);

  Dmsg0(20, "Leaving NativeRestoreCleanup\n");
}

/*
 * Generic function which generates a restore summary message.
 * Used by:
 *    - NativeRestoreCleanup e.g. normal restores
 *    - NdmpRestoreCleanup e.g. NDMP restores
 */
void GenerateRestoreSummary(JobControlRecord* jcr,
                            int msg_type,
                            const char* TermMsg)
{
  char sdt[MAX_TIME_LENGTH], edt[MAX_TIME_LENGTH];
  char ec1[30], ec2[30], ec3[30], elapsed[50];
  char fd_term_msg[100], sd_term_msg[100];
  utime_t RunTime;
  double kbps;
  PoolMem temp, secure_erase_status;

  bstrftimes(sdt, sizeof(sdt), jcr->impl->jr.StartTime);
  bstrftimes(edt, sizeof(edt), jcr->impl->jr.EndTime);
  RunTime = jcr->impl->jr.EndTime - jcr->impl->jr.StartTime;
  if (RunTime <= 0) {
    kbps = 0;
  } else {
    kbps = ((double)jcr->impl->jr.JobBytes) / (1000.0 * (double)RunTime);
  }
  if (kbps < 0.05) { kbps = 0; }

  JobstatusToAscii(jcr->impl->FDJobStatus, fd_term_msg, sizeof(fd_term_msg));
  JobstatusToAscii(jcr->impl->SDJobStatus, sd_term_msg, sizeof(sd_term_msg));

  switch (jcr->getJobProtocol()) {
    case PT_NDMP_BAREOS:
    case PT_NDMP_NATIVE:
      Jmsg(jcr, msg_type, 0,
           _("%s %s %s (%s):\n"
             "  Build OS:               %s\n"
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
             "  Bareos binary info:     %s\n"
             "  Job triggered by:       %s\n"
             "  Termination:            %s\n\n"),
           BAREOS, my_name, kBareosVersionStrings.Full,
           kBareosVersionStrings.ShortDate, kBareosVersionStrings.GetOsInfo(),
           jcr->impl->jr.JobId, jcr->impl->jr.Job,
           jcr->impl->res.client->resource_name_, sdt, edt,
           edit_utime(RunTime, elapsed, sizeof(elapsed)),
           edit_uint64_with_commas((uint64_t)jcr->impl->ExpectedFiles, ec1),
           edit_uint64_with_commas((uint64_t)jcr->impl->jr.JobFiles, ec2),
           edit_uint64_with_commas(jcr->impl->jr.JobBytes, ec3), (float)kbps,
           sd_term_msg, kBareosVersionStrings.JoblogMessage,
           JobTriggerToString(jcr->impl->job_trigger).c_str(), TermMsg);
      break;
    default:
      if (me->secure_erase_cmdline) {
        Mmsg(temp, "  Dir Secure Erase Cmd:   %s\n", me->secure_erase_cmdline);
        PmStrcat(secure_erase_status, temp.c_str());
      }
      if (!bstrcmp(jcr->impl->FDSecureEraseCmd, "*None*")) {
        Mmsg(temp, "  FD  Secure Erase Cmd:   %s\n",
             jcr->impl->FDSecureEraseCmd);
        PmStrcat(secure_erase_status, temp.c_str());
      }
      if (!bstrcmp(jcr->impl->SDSecureEraseCmd, "*None*")) {
        Mmsg(temp, "  SD  Secure Erase Cmd:   %s\n",
             jcr->impl->SDSecureEraseCmd);
        PmStrcat(secure_erase_status, temp.c_str());
      }

      Jmsg(jcr, msg_type, 0,
           _("%s %s %s (%s):\n"
             "  Build OS:               %s\n"
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
             "  Bareos binary info:     %s\n"
             "  Job triggered by:       %s\n"
             "  Termination:            %s\n\n"),
           BAREOS, my_name, kBareosVersionStrings.Full,
           kBareosVersionStrings.ShortDate, kBareosVersionStrings.GetOsInfo(),
           jcr->impl->jr.JobId, jcr->impl->jr.Job,
           jcr->impl->res.client->resource_name_, sdt, edt,
           edit_utime(RunTime, elapsed, sizeof(elapsed)),
           edit_uint64_with_commas((uint64_t)jcr->impl->ExpectedFiles, ec1),
           edit_uint64_with_commas((uint64_t)jcr->impl->jr.JobFiles, ec2),
           edit_uint64_with_commas(jcr->impl->jr.JobBytes, ec3), (float)kbps,
           jcr->JobErrors, fd_term_msg, sd_term_msg,
           secure_erase_status.c_str(), kBareosVersionStrings.JoblogMessage,
           JobTriggerToString(jcr->impl->job_trigger).c_str(), TermMsg);
      break;
  }
}
} /* namespace directordaemon */
