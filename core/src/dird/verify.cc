/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2016 Planets Communications B.V.
   Copyright (C) 2013-2019 Bareos GmbH & Co. KG

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
 * Kern Sibbald, October MM
 */
/**
 * @file
 * responsible for running file verification
 *
 * Basic tasks done here:
 *    * Open DB
 *    * Open connection with File daemon and pass him commands to do the verify.
 *    * When the File daemon sends the attributes, compare them to what is in
 * the DB.
 */

#include "include/bareos.h"
#include "dird.h"
#include "dird/dird_globals.h"
#include "findlib/find.h"
#include "dird/backup.h"
#include "dird/fd_cmds.h"
#include "dird/getmsg.h"
#include "dird/job.h"
#include "dird/msgchan.h"
#include "dird/sd_cmds.h"
#include "dird/storage.h"
#include "dird/verify.h"
#include "lib/berrno.h"
#include "lib/bnet.h"
#include "lib/edit.h"
#include "lib/util.h"

namespace directordaemon {

/* Commands sent to File daemon */
static char verifycmd[] = "verify level=%s\n";
static char storaddrcmd[] =
    "storage address=%s port=%d ssl=%d Authorization=%s\n";
static char passiveclientcmd[] = "passive client address=%s port=%d ssl=%d\n";

/* Responses received from File daemon */
static char OKverify[] = "2000 OK verify\n";
static char OKstore[] = "2000 OK storage\n";
static char OKpassiveclient[] = "2000 OK passive client\n";

/* Forward referenced functions */
static void PrtFname(JobControlRecord* jcr);
static int MissingHandler(void* ctx, int num_fields, char** row);

/**
 * Called here before the job is run to do the job
 *   specific setup.
 */
bool DoVerifyInit(JobControlRecord* jcr)
{
  int JobLevel;

  if (!AllowDuplicateJob(jcr)) { return false; }

  JobLevel = jcr->getJobLevel();
  switch (JobLevel) {
    case L_VERIFY_INIT:
    case L_VERIFY_CATALOG:
    case L_VERIFY_DISK_TO_CATALOG:
      FreeRstorage(jcr);
      FreeWstorage(jcr);
      break;
    case L_VERIFY_VOLUME_TO_CATALOG:
      FreeWstorage(jcr);
      break;
    case L_VERIFY_DATA:
      break;
    default:
      Jmsg2(jcr, M_FATAL, 0, _("Unimplemented Verify level %d(%c)\n"), JobLevel,
            JobLevel);
      return false;
  }
  return true;
}

/**
 * Do a verification of the specified files against the Catalog
 *
 *  Returns:  false on failure
 *            true  on success
 */
bool DoVerify(JobControlRecord* jcr)
{
  int JobLevel;
  const char* level;
  BareosSocket* fd = NULL;
  BareosSocket* sd = NULL;
  int status;
  char ed1[100];
  JobDbRecord jr;
  JobId_t verify_jobid = 0;
  const char* Name;

  FreeWstorage(jcr); /* we don't write */

  new (&jcr->previous_jr) JobDbRecord();  // placement new instead of memset

  /*
   * Find JobId of last job that ran. Note, we do this when
   *   the job actually starts running, not at schedule time,
   *   so that we find the last job that terminated before
   *   this job runs rather than before it is scheduled. This
   *   permits scheduling a Backup and Verify at the same time,
   *   but with the Verify at a lower priority.
   *
   *   For VERIFY_CATALOG we want the JobId of the last INIT.
   *   For VERIFY_VOLUME_TO_CATALOG, we want the JobId of the
   *       last backup Job.
   */
  JobLevel = jcr->getJobLevel();
  switch (JobLevel) {
    case L_VERIFY_CATALOG:
    case L_VERIFY_VOLUME_TO_CATALOG:
    case L_VERIFY_DISK_TO_CATALOG:
      jr = jcr->jr;  // Ueb
      if (jcr->res.verify_job && (JobLevel == L_VERIFY_VOLUME_TO_CATALOG ||
                                  JobLevel == L_VERIFY_DISK_TO_CATALOG)) {
        Name = jcr->res.verify_job->resource_name_;
      } else {
        Name = NULL;
      }
      Dmsg1(100, "find last jobid for: %s\n", NPRT(Name));

      /*
       * See if user supplied a jobid= as run argument or from menu
       */
      if (jcr->VerifyJobId) {
        verify_jobid = jcr->VerifyJobId;
        Dmsg1(100, "Supplied jobid=%d\n", verify_jobid);

      } else {
        if (!jcr->db->FindLastJobid(jcr, Name, &jr)) {
          if (JobLevel == L_VERIFY_CATALOG) {
            Jmsg(jcr, M_FATAL, 0,
                 _("Unable to find JobId of previous InitCatalog Job.\n"
                   "Please run a Verify with Level=InitCatalog before\n"
                   "running the current Job.\n"));
          } else {
            Jmsg(jcr, M_FATAL, 0,
                 _("Unable to find JobId of previous Job for this client.\n"));
          }
          return false;
        }
        verify_jobid = jr.JobId;
      }
      Dmsg1(100, "Last full jobid=%d\n", verify_jobid);

      /*
       * Now get the job record for the previous backup that interests
       *   us. We use the verify_jobid that we found above.
       */
      jcr->previous_jr.JobId = verify_jobid;
      if (!jcr->db->GetJobRecord(jcr, &jcr->previous_jr)) {
        Jmsg(jcr, M_FATAL, 0,
             _("Could not get job record for previous Job. ERR=%s"),
             jcr->db->strerror());
        return false;
      }
      if (!(jcr->previous_jr.JobStatus == JS_Terminated ||
            jcr->previous_jr.JobStatus == JS_Warnings)) {
        Jmsg(jcr, M_FATAL, 0,
             _("Last Job %d did not Terminate normally. JobStatus=%c\n"),
             verify_jobid, jcr->previous_jr.JobStatus);
        return false;
      }
      Jmsg(jcr, M_INFO, 0, _("Verifying against JobId=%d Job=%s\n"),
           jcr->previous_jr.JobId, jcr->previous_jr.Job);
  }

  /*
   * If we are verifying a Volume, we need the Storage
   *   daemon, so open a connection, otherwise, just
   *   create a dummy authorization key (passed to
   *   File daemon but not used).
   */
  switch (JobLevel) {
    case L_VERIFY_VOLUME_TO_CATALOG:
      /*
       * Note: negative status is an error, zero status, means
       *  no files were backed up, so skip calling SD and
       *  client.
       */
      status = CreateRestoreBootstrapFile(jcr);
      if (status < 0) { /* error */
        return false;
      } else if (status == 0) {            /* No files, nothing to do */
        VerifyCleanup(jcr, JS_Terminated); /* clean up */
        return true;                       /* get out */
      }

      if (jcr->res.verify_job) {
        jcr->res.fileset = jcr->res.verify_job->fileset;
      }
      break;
    default:
      jcr->sd_auth_key = strdup("dummy"); /* dummy Storage daemon key */
      break;
  }

  Dmsg2(100, "ClientId=%u JobLevel=%c\n", jcr->previous_jr.ClientId, JobLevel);

  if (!jcr->db->UpdateJobStartRecord(jcr, &jcr->jr)) {
    Jmsg(jcr, M_FATAL, 0, "%s", jcr->db->strerror());
    return false;
  }

  /*
   * Print Job Start message
   */
  Jmsg(jcr, M_INFO, 0, _("Start Verify JobId=%s Level=%s Job=%s\n"),
       edit_uint64(jcr->JobId, ed1), level_to_str(JobLevel), jcr->Job);

  switch (JobLevel) {
    case L_VERIFY_VOLUME_TO_CATALOG:
      /*
       * Start conversation with Storage daemon
       */
      jcr->setJobStatus(JS_Blocked);
      if (!ConnectToStorageDaemon(jcr, 10, me->SDConnectTimeout, true)) {
        return false;
      }
      sd = jcr->store_bsock;

      /*
       * Now start a job with the Storage daemon
       */
      if (!StartStorageDaemonJob(jcr, jcr->res.read_storage_list, NULL,
                                 /* send_bsr */ true)) {
        return false;
      }

      jcr->passive_client = jcr->res.client->passive;
      if (!jcr->passive_client) {
        /*
         * Start the Job in the SD.
         */
        if (!sd->fsend("run")) { return false; }

        /*
         * Now start a Storage daemon message thread
         */
        if (!StartStorageDaemonMessageThread(jcr)) { return false; }
        Dmsg0(50, "Storage daemon connection OK\n");
      }

      /*
       * OK, now connect to the File daemon and ask him for the files.
       */
      jcr->setJobStatus(JS_Blocked);
      if (!ConnectToFileDaemon(jcr, 10, me->FDConnectTimeout, true)) {
        goto bail_out;
      }
      SendJobInfoToFileDaemon(jcr);
      fd = jcr->file_bsock;

      /*
       * Check if the file daemon supports passive client mode.
       */
      if (jcr->passive_client && jcr->FDVersion < FD_VERSION_51) {
        Jmsg(jcr, M_FATAL, 0,
             _("Client \"%s\" doesn't support passive client mode. "
               "Please upgrade your client or disable compat mode.\n"),
             jcr->res.client->resource_name_);
        goto bail_out;
      }
      break;
    default:
      /*
       * OK, now connect to the File daemon and ask him for the files.
       */
      jcr->setJobStatus(JS_Blocked);
      if (!ConnectToFileDaemon(jcr, 10, me->FDConnectTimeout, true)) {
        goto bail_out;
      }
      SendJobInfoToFileDaemon(jcr);
      fd = jcr->file_bsock;
      break;
  }

  jcr->setJobStatus(JS_Running);

  Dmsg0(30, ">filed: Send include list\n");
  if (!SendIncludeList(jcr)) { goto bail_out; }

  Dmsg0(30, ">filed: Send exclude list\n");
  if (!SendExcludeList(jcr)) { goto bail_out; }

  /*
   * Send Level command to File daemon, as well as the Storage address if
   * appropriate.
   */
  switch (JobLevel) {
    case L_VERIFY_INIT:
      level = "init";
      break;
    case L_VERIFY_CATALOG:
      level = "catalog";
      break;
    case L_VERIFY_VOLUME_TO_CATALOG:
      if (!jcr->RestoreBootstrap) {
        Jmsg0(jcr, M_FATAL, 0, _("Deprecated feature ... use bootstrap.\n"));
        goto bail_out;
      }

      if (!jcr->passive_client) {
        StorageResource* store = jcr->res.read_storage;

        /*
         * Send Storage daemon address to the File daemon
         */
        if (store->SDDport == 0) { store->SDDport = store->SDport; }

        TlsPolicy tls_policy;
        if (jcr->res.client->connection_successful_handshake_ !=
            ClientConnectionHandshakeMode::kTlsFirst) {
          tls_policy = store->GetPolicy();
        } else {
          tls_policy = store->IsTlsConfigured() ? TlsPolicy::kBnetTlsAuto
                                                : TlsPolicy::kBnetTlsNone;
        }

        Dmsg1(200, "Tls Policy for active client is: %d\n", tls_policy);

        fd->fsend(storaddrcmd, store->address, store->SDDport, tls_policy,
                  jcr->sd_auth_key);
        if (!response(jcr, fd, OKstore, "Storage", DISPLAY_ERROR)) {
          goto bail_out;
        }
      } else {
        ClientResource* client = jcr->res.client;

        TlsPolicy tls_policy;
        if (jcr->res.client->connection_successful_handshake_ !=
            ClientConnectionHandshakeMode::kTlsFirst) {
          tls_policy = client->GetPolicy();
        } else {
          tls_policy = client->IsTlsConfigured() ? TlsPolicy::kBnetTlsAuto
                                                 : TlsPolicy::kBnetTlsNone;
        }

        Dmsg1(200, "Tls Policy for passive client is: %d\n", tls_policy);

        /*
         * Tell the SD to connect to the FD.
         */
        sd->fsend(passiveclientcmd, client->address, client->FDport,
                  tls_policy);
        Bmicrosleep(2, 0);
        if (!response(jcr, sd, OKpassiveclient, "Passive client",
                      DISPLAY_ERROR)) {
          goto bail_out;
        }

        /*
         * Start the Job in the SD.
         */
        if (!sd->fsend("run")) { goto bail_out; }

        /*
         * Now start a Storage daemon message thread
         */
        if (!StartStorageDaemonMessageThread(jcr)) { goto bail_out; }
        Dmsg0(50, "Storage daemon connection OK\n");
      }

      level = "volume";
      break;
    case L_VERIFY_DATA:
      level = "data";
      break;
    case L_VERIFY_DISK_TO_CATALOG:
      level = "disk_to_catalog";
      break;
    default:
      Jmsg2(jcr, M_FATAL, 0, _("Unimplemented Verify level %d(%c)\n"), JobLevel,
            JobLevel);
      goto bail_out;
  }

  if (!SendRunscriptsCommands(jcr)) { goto bail_out; }

  /*
   * Send verify command/level to File daemon
   */
  fd->fsend(verifycmd, level);
  if (!response(jcr, fd, OKverify, "Verify", DISPLAY_ERROR)) { goto bail_out; }

  /*
   * Now get data back from File daemon and
   *  compare it to the catalog or store it in the
   *  catalog depending on the run type.
   */
  switch (JobLevel) {
    case L_VERIFY_CATALOG:
      /*
       * Verify from catalog
       */
      Dmsg0(10, "Verify level=catalog\n");
      jcr->sd_msg_thread_done = true; /* no SD msg thread, so it is done */
      jcr->SDJobStatus = JS_Terminated;
      GetAttributesAndCompareToCatalog(jcr, jcr->previous_jr.JobId);
      break;
    case L_VERIFY_VOLUME_TO_CATALOG:
      /*
       * Verify Volume to catalog entries
       */
      Dmsg0(10, "Verify level=volume\n");
      GetAttributesAndCompareToCatalog(jcr, jcr->previous_jr.JobId);
      break;
    case L_VERIFY_DISK_TO_CATALOG:
      /*
       * Verify Disk attributes to catalog
       */
      Dmsg0(10, "Verify level=disk_to_catalog\n");
      jcr->sd_msg_thread_done = true; /* no SD msg thread, so it is done */
      jcr->SDJobStatus = JS_Terminated;
      GetAttributesAndCompareToCatalog(jcr, jcr->previous_jr.JobId);
      break;
    case L_VERIFY_INIT:
      /*
       * Build catalog
       */
      Dmsg0(10, "Verify level=init\n");
      jcr->sd_msg_thread_done = true; /* no SD msg thread, so it is done */
      jcr->SDJobStatus = JS_Terminated;
      GetAttributesAndPutInCatalog(jcr);
      jcr->db->EndTransaction(jcr); /* Terminate any open transaction */
      jcr->db_batch->WriteBatchFileRecords(jcr);
      break;
    default:
      Jmsg1(jcr, M_FATAL, 0, _("Unimplemented verify level %d\n"), JobLevel);
      goto bail_out;
  }

  status = WaitForJobTermination(jcr);
  VerifyCleanup(jcr, status);
  return true;

bail_out:
  if (jcr->file_bsock) {
    jcr->file_bsock->signal(BNET_TERMINATE);
    jcr->file_bsock->close();
    delete jcr->file_bsock;
    jcr->file_bsock = NULL;
  }

  return false;
}

/**
 * Release resources allocated during verify.
 */
void VerifyCleanup(JobControlRecord* jcr, int TermCode)
{
  int JobLevel;
  char sdt[50], edt[50];
  char ec1[30], ec2[30];
  char term_code[100], fd_term_msg[100], sd_term_msg[100];
  const char* TermMsg;
  int msg_type;
  const char* Name;

  // Dmsg1(100, "Enter VerifyCleanup() TermCod=%d\n", TermCode);

  JobLevel = jcr->getJobLevel();
  Dmsg3(900, "JobLevel=%c Expected=%u JobFiles=%u\n", JobLevel,
        jcr->ExpectedFiles, jcr->JobFiles);
  if (JobLevel == L_VERIFY_VOLUME_TO_CATALOG &&
      jcr->ExpectedFiles != jcr->JobFiles) {
    TermCode = JS_ErrorTerminated;
  }

  UpdateJobEnd(jcr, TermCode);

  if (JobCanceled(jcr)) { CancelStorageDaemonJob(jcr); }

  if (jcr->unlink_bsr && jcr->RestoreBootstrap) {
    SecureErase(jcr, jcr->RestoreBootstrap);
    jcr->unlink_bsr = false;
  }

  msg_type = M_INFO; /* By default INFO message */
  switch (TermCode) {
    case JS_Terminated:
      TermMsg = _("Verify OK");
      break;
    case JS_FatalError:
    case JS_ErrorTerminated:
      TermMsg = _("*** Verify Error ***");
      msg_type = M_ERROR; /* Generate error message */
      break;
    case JS_Error:
      TermMsg = _("Verify warnings");
      break;
    case JS_Canceled:
      TermMsg = _("Verify Canceled");
      break;
    case JS_Differences:
      TermMsg = _("Verify Differences");
      break;
    default:
      TermMsg = term_code;
      Bsnprintf(term_code, sizeof(term_code),
                _("Inappropriate term code: %d %c\n"), TermCode, TermCode);
      break;
  }
  bstrftimes(sdt, sizeof(sdt), jcr->jr.StartTime);
  bstrftimes(edt, sizeof(edt), jcr->jr.EndTime);
  if (jcr->res.verify_job) {
    Name = jcr->res.verify_job->resource_name_;
  } else {
    Name = "";
  }

  JobstatusToAscii(jcr->FDJobStatus, fd_term_msg, sizeof(fd_term_msg));
  switch (JobLevel) {
    case L_VERIFY_VOLUME_TO_CATALOG:
      JobstatusToAscii(jcr->SDJobStatus, sd_term_msg, sizeof(sd_term_msg));
      Jmsg(jcr, msg_type, 0,
           _("%s %s %s (%s):\n"
             "  Build OS:               %s %s %s\n"
             "  JobId:                  %d\n"
             "  Job:                    %s\n"
             "  FileSet:                %s\n"
             "  Verify Level:           %s\n"
             "  Client:                 %s\n"
             "  Verify JobId:           %d\n"
             "  Verify Job:             %s\n"
             "  Start time:             %s\n"
             "  End time:               %s\n"
             "  Files Expected:         %s\n"
             "  Files Examined:         %s\n"
             "  Non-fatal FD errors:    %d\n"
             "  FD termination status:  %s\n"
             "  SD termination status:  %s\n"
             "  Bareos binary info:     %s\n"
             "  Termination:            %s\n\n"),
           BAREOS, my_name, VERSION, LSMDATE, HOST_OS, DISTNAME, DISTVER,
           jcr->jr.JobId, jcr->jr.Job, jcr->res.fileset->resource_name_,
           level_to_str(JobLevel), jcr->res.client->resource_name_,
           jcr->previous_jr.JobId, Name, sdt, edt,
           edit_uint64_with_commas(jcr->ExpectedFiles, ec1),
           edit_uint64_with_commas(jcr->JobFiles, ec2), jcr->JobErrors,
           fd_term_msg, sd_term_msg, BAREOS_JOBLOG_MESSAGE, TermMsg);
      break;
    default:
      Jmsg(jcr, msg_type, 0,
           _("%s %s %s (%s):\n"
             "  Build:                  %s %s %s\n"
             "  JobId:                  %d\n"
             "  Job:                    %s\n"
             "  FileSet:                %s\n"
             "  Verify Level:           %s\n"
             "  Client:                 %s\n"
             "  Verify JobId:           %d\n"
             "  Verify Job:             %s\n"
             "  Start time:             %s\n"
             "  End time:               %s\n"
             "  Files Examined:         %s\n"
             "  Non-fatal FD errors:    %d\n"
             "  FD termination status:  %s\n"
             "  Bareos binary info:     %s\n"
             "  Termination:            %s\n\n"),
           BAREOS, my_name, VERSION, LSMDATE, HOST_OS, DISTNAME, DISTVER,
           jcr->jr.JobId, jcr->jr.Job, jcr->res.fileset->resource_name_,
           level_to_str(JobLevel), jcr->res.client->resource_name_,
           jcr->previous_jr.JobId, Name, sdt, edt,
           edit_uint64_with_commas(jcr->JobFiles, ec1), jcr->JobErrors,
           fd_term_msg, BAREOS_JOBLOG_MESSAGE, TermMsg);
      break;
  }

  Dmsg0(100, "Leave VerifyCleanup()\n");
}

/**
 * This routine is called only during a Verify
 */
void GetAttributesAndCompareToCatalog(JobControlRecord* jcr, JobId_t JobId)
{
  BareosSocket* fd;
  int n, len;
  FileDbRecord fdbr;
  struct stat statf; /* file stat */
  struct stat statc; /* catalog stat */
  PoolMem buf(PM_MESSAGE);
  POOLMEM* fname = GetPoolMemory(PM_FNAME);
  int do_Digest = CRYPTO_DIGEST_NONE;
  int32_t file_index = 0;

  memset(&fdbr, 0, sizeof(fdbr));
  fd = jcr->file_bsock;
  fdbr.JobId = JobId;
  jcr->FileIndex = 0;

  Dmsg0(20, "dir: waiting to receive file attributes\n");
  /*
   * Get Attributes and Signature from File daemon
   * We expect:
   *   FileIndex
   *   Stream
   *   Options or Digest (MD5/SHA1)
   *   Filename
   *   Attributes
   *   Link name  ???
   */
  while ((n = BgetDirmsg(fd)) >= 0 && !JobCanceled(jcr)) {
    int stream;
    char *attr, *p, *fn;
    PoolMem Opts_Digest(PM_MESSAGE); /* Verify Opts or MD5/SHA1 digest */

    if (JobCanceled(jcr)) { goto bail_out; }
    fname = CheckPoolMemorySize(fname, fd->message_length);
    jcr->fname = CheckPoolMemorySize(jcr->fname, fd->message_length);
    Dmsg1(200, "Atts+Digest=%s\n", fd->msg);
    if ((len = sscanf(fd->msg, "%ld %d %100s", &file_index, &stream, fname)) !=
        3) {
      Jmsg3(jcr, M_FATAL, 0,
            _("dird<filed: bad attributes, expected 3 fields got %d\n"
              " mslen=%d msg=%s\n"),
            len, fd->message_length, fd->msg);
      goto bail_out;
    }
    /*
     * We read the Options or Signature into fname
     *  to prevent overrun, now copy it to proper location.
     */
    PmStrcpy(Opts_Digest, fname);
    p = fd->msg;
    SkipNonspaces(&p); /* skip FileIndex */
    SkipSpaces(&p);
    SkipNonspaces(&p); /* skip Stream */
    SkipSpaces(&p);
    SkipNonspaces(&p); /* skip Opts_Digest */
    p++;               /* skip space */
    fn = fname;
    while (*p != 0) { *fn++ = *p++; /* copy filename */ }
    *fn = *p++; /* term filename and point to attribs */
    attr = p;

    /*
     * Got attributes stream, decode it
     */
    switch (stream) {
      case STREAM_UNIX_ATTRIBUTES:
      case STREAM_UNIX_ATTRIBUTES_EX:
        int32_t LinkFIf, LinkFIc;
        Dmsg2(400, "file_index=%d attr=%s\n", file_index, attr);
        jcr->JobFiles++;
        jcr->FileIndex = file_index; /* remember attribute file_index */
        jcr->previous_jr.FileIndex = file_index;
        DecodeStat(attr, &statf, sizeof(statf),
                   &LinkFIf); /* decode file stat packet */
        do_Digest = CRYPTO_DIGEST_NONE;
        jcr->fn_printed = false;
        PmStrcpy(jcr->fname, fname); /* move filename into JobControlRecord */

        Dmsg2(040, "dird<filed: stream=%d %s\n", stream, jcr->fname);
        Dmsg1(020, "dird<filed: attr=%s\n", attr);

        /*
         * Find equivalent record in the database
         */
        fdbr.FileId = 0;
        if (!jcr->db->GetFileAttributesRecord(jcr, jcr->fname,
                                              &jcr->previous_jr, &fdbr)) {
          Jmsg(jcr, M_INFO, 0, _("New file: %s\n"), jcr->fname);
          Dmsg1(020, _("File not in catalog: %s\n"), jcr->fname);
          jcr->setJobStatus(JS_Differences);
          continue;
        } else {
          /*
           * mark file record as visited by stuffing the
           * current JobId, which is unique, into the MarkId field.
           */
          jcr->db->MarkFileRecord(jcr, fdbr.FileId, jcr->JobId);
        }

        Dmsg3(400, "Found %s in catalog. inx=%d Opts=%s\n", jcr->fname,
              file_index, Opts_Digest.c_str());
        DecodeStat(fdbr.LStat, &statc, sizeof(statc),
                   &LinkFIc); /* decode catalog stat */
        /*
         * Loop over options supplied by user and verify the
         * fields he requests.
         */
        for (p = Opts_Digest.c_str(); *p; p++) {
          char ed1[30], ed2[30];
          switch (*p) {
            case 'i': /* compare INODEs */
              if (statc.st_ino != statf.st_ino) {
                PrtFname(jcr);
                Jmsg(jcr, M_INFO, 0,
                     _("      st_ino   differ. Cat: %s File: %s\n"),
                     edit_uint64((uint64_t)statc.st_ino, ed1),
                     edit_uint64((uint64_t)statf.st_ino, ed2));
                jcr->setJobStatus(JS_Differences);
              }
              break;
            case 'p': /* permissions bits */
              if (statc.st_mode != statf.st_mode) {
                PrtFname(jcr);
                Jmsg(jcr, M_INFO, 0,
                     _("      st_mode  differ. Cat: %x File: %x\n"),
                     (uint32_t)statc.st_mode, (uint32_t)statf.st_mode);
                jcr->setJobStatus(JS_Differences);
              }
              break;
            case 'n': /* number of links */
              if (statc.st_nlink != statf.st_nlink) {
                PrtFname(jcr);
                Jmsg(jcr, M_INFO, 0,
                     _("      st_nlink differ. Cat: %d File: %d\n"),
                     (uint32_t)statc.st_nlink, (uint32_t)statf.st_nlink);
                jcr->setJobStatus(JS_Differences);
              }
              break;
            case 'u': /* user id */
              if (statc.st_uid != statf.st_uid) {
                PrtFname(jcr);
                Jmsg(jcr, M_INFO, 0,
                     _("      st_uid   differ. Cat: %u File: %u\n"),
                     (uint32_t)statc.st_uid, (uint32_t)statf.st_uid);
                jcr->setJobStatus(JS_Differences);
              }
              break;
            case 'g': /* group id */
              if (statc.st_gid != statf.st_gid) {
                PrtFname(jcr);
                Jmsg(jcr, M_INFO, 0,
                     _("      st_gid   differ. Cat: %u File: %u\n"),
                     (uint32_t)statc.st_gid, (uint32_t)statf.st_gid);
                jcr->setJobStatus(JS_Differences);
              }
              break;
            case 's': /* size */
              if (statc.st_size != statf.st_size) {
                PrtFname(jcr);
                Jmsg(jcr, M_INFO, 0,
                     _("      st_size  differ. Cat: %s File: %s\n"),
                     edit_uint64((uint64_t)statc.st_size, ed1),
                     edit_uint64((uint64_t)statf.st_size, ed2));
                jcr->setJobStatus(JS_Differences);
              }
              break;
            case 'a': /* access time */
              if (statc.st_atime != statf.st_atime) {
                PrtFname(jcr);
                Jmsg(jcr, M_INFO, 0, _("      st_atime differs\n"));
                jcr->setJobStatus(JS_Differences);
              }
              break;
            case 'm':
              if (statc.st_mtime != statf.st_mtime) {
                PrtFname(jcr);
                Jmsg(jcr, M_INFO, 0, _("      st_mtime differs\n"));
                jcr->setJobStatus(JS_Differences);
              }
              break;
            case 'c': /* ctime */
              if (statc.st_ctime != statf.st_ctime) {
                PrtFname(jcr);
                Jmsg(jcr, M_INFO, 0, _("      st_ctime differs\n"));
                jcr->setJobStatus(JS_Differences);
              }
              break;
            case 'd': /* file size decrease */
              if (statc.st_size > statf.st_size) {
                PrtFname(jcr);
                Jmsg(jcr, M_INFO, 0,
                     _("      st_size  decrease. Cat: %s File: %s\n"),
                     edit_uint64((uint64_t)statc.st_size, ed1),
                     edit_uint64((uint64_t)statf.st_size, ed2));
                jcr->setJobStatus(JS_Differences);
              }
              break;
            case '5': /* compare MD5 */
              Dmsg1(500, "set Do_MD5 for %s\n", jcr->fname);
              do_Digest = CRYPTO_DIGEST_MD5;
              break;
            case '1': /* compare SHA1 */
              do_Digest = CRYPTO_DIGEST_SHA1;
              break;
            case ':':
            case 'V':
            default:
              break;
          }
        }
        break;

      case STREAM_RESTORE_OBJECT:
        Dmsg1(400, "RESTORE_OBJECT %s\n", jcr->fname);
        break;

      default:
        /*
         * Got Digest Signature from Storage daemon
         *  It came across in the Opts_Digest field.
         */
        if (CryptoDigestStreamType(stream) != CRYPTO_DIGEST_NONE) {
          Dmsg2(400, "stream=Digest inx=%d Digest=%s\n", file_index,
                Opts_Digest.c_str());
          /*
           * When ever we get a digest it MUST have been
           * preceded by an attributes record, which sets attr_file_index
           */
          if (jcr->FileIndex != (uint32_t)file_index) {
            Jmsg2(jcr, M_FATAL, 0,
                  _("MD5/SHA1 index %d not same as attributes %d\n"),
                  file_index, jcr->FileIndex);
            goto bail_out;
          }
          if (do_Digest != CRYPTO_DIGEST_NONE) {
            jcr->db->EscapeString(jcr, buf.c_str(), Opts_Digest.c_str(),
                                  strlen(Opts_Digest.c_str()));
            if (!bstrcmp(buf.c_str(), fdbr.Digest)) {
              PrtFname(jcr);
              Jmsg(jcr, M_INFO, 0, _("      %s differs. File=%s Cat=%s\n"),
                   stream_to_ascii(stream), buf.c_str(), fdbr.Digest);
              jcr->setJobStatus(JS_Differences);
            }
            do_Digest = CRYPTO_DIGEST_NONE;
          }
        }
        break;
    }
    jcr->JobFiles = file_index;
  }

  if (IsBnetError(fd)) {
    BErrNo be;
    Jmsg2(jcr, M_FATAL, 0,
          _("dir<filed: bad attributes from filed n=%d : %s\n"), n,
          be.bstrerror());
    goto bail_out;
  }

  /* Now find all the files that are missing -- i.e. all files in
   *  the database where the MarkId != current JobId
   */
  jcr->fn_printed = false;
  Mmsg(buf,
       "SELECT Path.Path,File.Name FROM File,Path "
       "WHERE File.JobId=%d AND File.FileIndex > 0 "
       "AND File.MarkId!=%d AND File.PathId=Path.PathId ",
       JobId, jcr->JobId);
  /* MissingHandler is called for each file found */
  jcr->db->SqlQuery(buf.c_str(), MissingHandler, (void*)jcr);
  if (jcr->fn_printed) { jcr->setJobStatus(JS_Differences); }

bail_out:
  FreePoolMemory(fname);
}

/**
 * We are called here for each record that matches the above
 *  SQL query -- that is for each file contained in the Catalog
 *  that was not marked earlier. This means that the file in
 *  question is a missing file (in the Catalog but not on Disk).
 */
static int MissingHandler(void* ctx, int num_fields, char** row)
{
  JobControlRecord* jcr = (JobControlRecord*)ctx;

  if (JobCanceled(jcr)) { return 1; }
  if (!jcr->fn_printed) {
    Qmsg(jcr, M_WARNING, 0,
         _("The following files are in the Catalog but not on %s:\n"),
         jcr->getJobLevel() == L_VERIFY_VOLUME_TO_CATALOG ? "the Volume(s)"
                                                          : "disk");
    jcr->fn_printed = true;
  }
  Qmsg(jcr, M_INFO, 0, "      %s%s\n", row[0] ? row[0] : "",
       row[1] ? row[1] : "");
  return 0;
}

/**
 * Print filename for verify
 */
static void PrtFname(JobControlRecord* jcr)
{
  if (!jcr->fn_printed) {
    Jmsg(jcr, M_INFO, 0, _("File: %s\n"), jcr->fname);
    jcr->fn_printed = true;
  }
}
} /* namespace directordaemon */
