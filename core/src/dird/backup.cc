/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2012 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2016 Planets Communications B.V.
   Copyright (C) 2013-2023 Bareos GmbH & Co. KG

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
// Kern Sibbald, March MM
/**
 * @file
 * responsible for doing backup jobs
 *
 * Basic tasks done here:
 *    Open DB and create records for this job.
 *    Open Message Channel with Storage daemon to tell him a job will be
 * starting. Open connection with File daemon and pass him commands to do the
 * backup. When the File daemon finishes the job, update the DB.
 */

#include "include/bareos.h"
#include "dird.h"
#include "dird/dird_globals.h"
#include "dird/backup.h"
#include "dird/fd_cmds.h"
#include "dird/getmsg.h"
#include "dird/inc_conf.h"
#include "dird/director_jcr_impl.h"
#include "dird/job.h"
#include "dird/msgchan.h"
#include "dird/quota.h"
#include "dird/sd_cmds.h"
#include "ndmp/smc.h"
#include "dird/storage.h"
#include "include/auth_protocol_types.h"
#include "include/protocol_types.h"

#include "cats/sql.h"
#include "lib/bnet.h"
#include "lib/edit.h"
#include "lib/berrno.h"
#include "lib/util.h"
#include "lib/version.h"
#include "lib/bpipe.h"

namespace directordaemon {

/* Commands sent to File daemon */
static char backupcmd[] = "backup FileIndex=%ld\n";
static char storaddrcmd[] = "storage address=%s port=%d ssl=%d\n";
static char passiveclientcmd[] = "passive client address=%s port=%d ssl=%d\n";

/* Responses received from File daemon */
static char OKbackup[] = "2000 OK backup\n";
static char OKstore[] = "2000 OK storage\n";
static char OKpassiveclient[] = "2000 OK passive client\n";
static char EndJob[]
    = "2800 End Job TermCode=%d JobFiles=%u "
      "ReadBytes=%llu JobBytes=%llu Errors=%u "
      "VSS=%d Encrypt=%d\n";

static inline bool ValidateClient(JobControlRecord* jcr)
{
  switch (jcr->dir_impl->res.client->Protocol) {
    case APT_NATIVE:
      return true;
    default:
      Jmsg(jcr, M_FATAL, 0,
           T_("Client %s has illegal backup protocol %s for Native backup\n"),
           jcr->dir_impl->res.client->resource_name_,
           AuthenticationProtocolTypeToString(
               jcr->dir_impl->res.client->Protocol));
      return false;
  }
}

/**
 * if both FD and SD have LanAddress set, use the storages' LanAddress to
 * connect to.
 */
char* StorageAddressToContact(ClientResource* client, StorageResource* store)
{
  if (store->lanaddress && client->lanaddress) {
    return store->lanaddress;
  } else {
    return store->address;
  }
}

/**
 * if both FD and SD have LanAddress set, use the clients' LanAddress to connect
 * to.
 *
 */
char* ClientAddressToContact(ClientResource* client, StorageResource* store)
{
  if (store->lanaddress && client->lanaddress) {
    return client->lanaddress;
  } else {
    return client->address;
  }
}

/**
 * if both readstorage and writestorage have LanAddress set,
 * use wstores' LanAddress to connect to.
 *
 */
char* StorageAddressToContact(StorageResource* read_storage,
                              StorageResource* write_storage)
{
  if (read_storage->lanaddress && write_storage->lanaddress) {
    return write_storage->lanaddress;
  } else {
    return write_storage->address;
  }
}

static inline bool ValidateStorage(JobControlRecord* jcr)
{
  StorageResource* store = nullptr;

  foreach_alist (store, jcr->dir_impl->res.write_storage_list) {
    switch (store->Protocol) {
      case APT_NATIVE:
        continue;
      default:
        Jmsg(
            jcr, M_FATAL, 0,
            T_("Storage %s has illegal backup protocol %s for Native backup\n"),
            store->resource_name_,
            AuthenticationProtocolTypeToString(store->Protocol));
        return false;
    }
  }

  return true;
}

bool DoNativeBackupInit(JobControlRecord* jcr)
{
  FreeRstorage(jcr); /* we don't read so release */

  if (!AllowDuplicateJob(jcr)) { return false; }

  jcr->dir_impl->jr.PoolId
      = GetOrCreatePoolRecord(jcr, jcr->dir_impl->res.pool->resource_name_);
  if (jcr->dir_impl->jr.PoolId == 0) { return false; }

  // If pool storage specified, use it instead of job storage
  CopyWstorage(jcr, jcr->dir_impl->res.pool->storage, T_("Pool resource"));
  if (!jcr->dir_impl->res.write_storage_list) {
    Jmsg(jcr, M_FATAL, 0,
         T_("No Storage specification found in Job or Pool.\n"));
    return false;
  }

  if (!ValidateClient(jcr) || !ValidateStorage(jcr)) { return false; }

  CreateClones(jcr); /* run any clone jobs */

  return true;
}

// Take all base jobs from job resource and find the last L_BASE jobid.
static bool GetBaseJobids(JobControlRecord* jcr, db_list_ctx* jobids)
{
  JobDbRecord jr;
  JobResource* job = nullptr;
  JobId_t id;

  if (!jcr->dir_impl->res.job->base) {
    return false; /* no base job, stop accurate */
  }

  jr.StartTime = jcr->dir_impl->jr.StartTime;

  foreach_alist (job, jcr->dir_impl->res.job->base) {
    bstrncpy(jr.Name, job->resource_name_, sizeof(jr.Name));
    jcr->db->GetBaseJobid(jcr, &jr, &id);

    if (id) { jobids->add(id); }
  }

  return (!jobids->empty());
}

/*
 * Foreach files in currrent list, send "/path/fname\0LStat\0MD5\0Delta" to FD
 *      row[0]=Path, row[1]=Filename, row[2]=FileIndex
 *      row[3]=JobId row[4]=LStat row[5]=DeltaSeq row[6]=MD5
 */
static int AccurateListHandler(void* ctx, int num_fields, char** row)
{
  JobControlRecord* jcr = (JobControlRecord*)ctx;

  if (jcr->IsJobCanceled()) { return 1; }

  if (row[2][0] == '0') { /* discard when file_index == 0 */
    return 0;
  }

  /* sending with checksum */
  if (jcr->dir_impl->use_accurate_chksum && num_fields == 9 && row[6][0]
      && /* skip checksum = '0' */
      row[6][1]) {
    jcr->file_bsock->fsend("%s%s%c%s%c%s%c%s", row[0], row[1], 0, row[4], 0,
                           row[6], 0, row[5]);
  } else {
    jcr->file_bsock->fsend("%s%s%c%s%c%c%s", row[0], row[1], 0, row[4], 0, 0,
                           row[5]);
  }
  return 0;
}

/* In this procedure, we check if the current fileset is using checksum
 * FileSet-> Include-> Options-> Accurate/Verify/BaseJob=checksum
 * This procedure uses jcr->HasBase, so it must be call after the initialization
 */
static bool IsChecksumNeededByFileset(JobControlRecord* jcr)
{
  IncludeExcludeItem* inc;
  FileOptions* fopts;
  FilesetResource* fs;
  bool in_block = false;
  bool have_basejob_option = false;

  if (!jcr->dir_impl->res.job || !jcr->dir_impl->res.job->fileset) {
    return false;
  }

  fs = jcr->dir_impl->res.job->fileset;
  for (std::size_t i = 0; i < fs->include_items.size(); i++) {
    inc = fs->include_items[i];

    for (std::size_t j = 0; j < inc->file_options_list.size(); j++) {
      fopts = inc->file_options_list[j];

      for (char* k = fopts->opts; *k; k++) { /* Try to find one request */
        switch (*k) {
          case 'V':                                /* verify */
            in_block = jcr->is_JobType(JT_VERIFY); /* not used now */
            break;
          case 'J': /* Basejob keyword */
            have_basejob_option = in_block = jcr->HasBase;
            break;
          case 'C': /* Accurate keyword */
            in_block = !jcr->is_JobLevel(L_FULL);
            break;
          case ':': /* End of keyword */
            in_block = false;
            break;
          case '5': /* MD5  */
          case '1': /* SHA1 */
            if (in_block) {
              Dmsg0(50, "Checksum will be sent to FD\n");
              return true;
            }
            break;
          default:
            break;
        }
      }
    }
  }

  /* By default for BaseJobs, we send the checksum */
  if (!have_basejob_option && jcr->HasBase) { return true; }

  Dmsg0(50, "Checksum will be sent to FD\n");
  return false;
}

/*
 * Send current file list to FD
 *    DIR -> FD : accurate files=xxxx
 *    DIR -> FD : /path/to/file\0Lstat\0MD5\0Delta
 *    DIR -> FD : /path/to/dir/\0Lstat\0MD5\0Delta
 *    ...
 *    DIR -> FD : EOD
 */
bool SendAccurateCurrentFiles(JobControlRecord* jcr)
{
  PoolMem buf;
  db_list_ctx jobids;
  db_list_ctx nb;

  // In base level, no previous job is used and no restart incomplete jobs
  if (jcr->IsJobCanceled() || jcr->is_JobLevel(L_BASE)) { return true; }

  if (!jcr->accurate) { return true; }

  if (jcr->is_JobLevel(L_FULL)) {
    // On Full mode, if no previous base job, no accurate things
    if (GetBaseJobids(jcr, &jobids)) {
      jcr->HasBase = true;
      Jmsg(jcr, M_INFO, 0, T_("Using BaseJobId(s): %s\n"),
           jobids.GetAsString().c_str());
    } else {
      return true;
    }
  } else {
    // For Incr/Diff level, we search for older jobs
    jcr->db->AccurateGetJobids(jcr, &jcr->dir_impl->jr, &jobids);

    // We are in Incr/Diff, but no Full to build the accurate list...
    if (jobids.empty()) {
      Jmsg(jcr, M_FATAL, 0, T_("Cannot find previous jobids.\n"));
      return false; /* fail */
    }
  }

  // Don't send and store the checksum if fileset doesn't require it
  jcr->dir_impl->use_accurate_chksum = IsChecksumNeededByFileset(jcr);
  if (jcr->JobId) { /* display the message only for real jobs */
    Jmsg(jcr, M_INFO, 0, T_("Sending Accurate information.\n"));
  }

  // To be able to allocate the right size for htable
  Mmsg(buf, "SELECT sum(JobFiles) FROM Job WHERE JobId IN (%s)",
       jobids.GetAsString().c_str());
  jcr->db->SqlQuery(buf.c_str(), DbListHandler, &nb);
  Dmsg2(200, "jobids=%s nb=%s\n", jobids.GetAsString().c_str(),
        nb.GetAsString().c_str());
  jcr->file_bsock->fsend("accurate files=%s\n", nb.GetAsString().c_str());

  if (jcr->HasBase) {
    jcr->nb_base_files = nb.GetFrontAsInteger();
    if (!jcr->db->CreateBaseFileList(jcr, jobids.GetAsString().c_str())) {
      Jmsg(jcr, M_FATAL, 0, "error in jcr->db->CreateBaseFileList:%s\n",
           jcr->db->strerror());
      return false;
    }
    if (!jcr->db->GetBaseFileList(jcr, jcr->dir_impl->use_accurate_chksum,
                                  AccurateListHandler, (void*)jcr)) {
      Jmsg(jcr, M_FATAL, 0, "error in jcr->db->GetBaseFileList:%s\n",
           jcr->db->strerror());
      return false;
    }
  } else {
    if (!jcr->db->OpenBatchConnection(jcr)) {
      Jmsg0(jcr, M_FATAL, 0, "Can't get batch sql connection");
      return false; /* Fail */
    }

    if (!jcr->db_batch->GetFileList(jcr, jobids.GetAsString().c_str(),
                                    jcr->dir_impl->use_accurate_chksum,
                                    false /* no delta */, AccurateListHandler,
                                    (void*)jcr)) {
      Jmsg(jcr, M_FATAL, 0, "error in jcr->db_batch->GetBaseFileList:%s\n",
           jcr->db_batch->strerror());
      return false;
    }
  }

  jcr->file_bsock->signal(BNET_EOD);
  return true;
}

static void TerminateBackupWithError(JobControlRecord* jcr)
{
  jcr->setJobStatusWithPriorityCheck(JS_ErrorTerminated);
  WaitForJobTermination(jcr, me->FDConnectTimeout);
}

static void CloseFdConnection(JobControlRecord* jcr)
{
  if (jcr->file_bsock) {
    jcr->file_bsock->signal(BNET_TERMINATE);
    jcr->file_bsock->close();
    delete jcr->file_bsock;
    jcr->file_bsock = nullptr;
  }
}

static bool ConfigureTlsRequirementsPassiveClient(JobControlRecord* jcr)
{
  ClientResource* client = jcr->dir_impl->res.client;
  StorageResource* store = jcr->dir_impl->res.write_storage;
  char* connection_target_address;

  if (!jcr->passive_client) {
    // TLS Requirement

    TlsPolicy tls_policy;
    if (jcr->dir_impl->res.client->connection_successful_handshake_
        != ClientConnectionHandshakeMode::kTlsFirst) {
      tls_policy = store->GetPolicy();
    } else {
      tls_policy = store->IsTlsConfigured() ? TlsPolicy::kBnetTlsAuto
                                            : TlsPolicy::kBnetTlsNone;
    }

    Dmsg1(200, "Tls Policy for active client is: %d\n", tls_policy);

    connection_target_address = StorageAddressToContact(client, store);

    jcr->file_bsock->fsend(storaddrcmd, connection_target_address,
                           store->SDport, tls_policy);
    if (!response(jcr, jcr->file_bsock, OKstore, "Storage", DISPLAY_ERROR)) {
      Dmsg0(200, "Error from active client on storeaddrcmd\n");
      TerminateBackupWithError(jcr);
      return false;
    }

  } else {  // passive client

    TlsPolicy tls_policy;
    if (jcr->dir_impl->res.client->connection_successful_handshake_
        != ClientConnectionHandshakeMode::kTlsFirst) {
      tls_policy = client->GetPolicy();
    } else {
      tls_policy = client->IsTlsConfigured() ? TlsPolicy::kBnetTlsAuto
                                             : TlsPolicy::kBnetTlsNone;
    }
    Dmsg1(200, "Tls Policy for passive client is: %d\n", tls_policy);

    connection_target_address = ClientAddressToContact(client, store);

    jcr->store_bsock->fsend(passiveclientcmd, connection_target_address,
                            client->FDport, tls_policy);
    Bmicrosleep(2, 0);
    if (!response(jcr, jcr->store_bsock, OKpassiveclient, "Passive client",
                  DISPLAY_ERROR)) {
      TerminateBackupWithError(jcr);
      return false;
    }
  }  // if (!jcr->passive_client)
  return true;
}

static bool ConfigureMessageThread(JobControlRecord* jcr)
{
  /* When the client is not in passive mode we can put the SD in
   * listen mode for the FD connection. */
  jcr->passive_client = jcr->dir_impl->res.client->passive;

  if (!ConfigureTlsRequirementsPassiveClient(jcr)) { return false; }

  /* Start the job prior to starting the message thread below
   * to avoid two threads from using the BareosSocket structure at
   * the same time. */
  if (!jcr->store_bsock->fsend("run")) { return false; }

  /* Now start a Storage daemon message thread.  Note,
   * this thread is used to provide the catalog services
   * for the backup job, including inserting the attributes
   * into the catalog.  See CatalogUpdate() in catreq.c */
  if (!StartStorageDaemonMessageThread(jcr)) { return false; }

  Dmsg0(150, "Storage daemon connection OK\n");

  return true;
}

/*
 * Do a backup of the specified FileSet
 *
 *  Returns:  false on failure
 *            true  on success
 */
bool DoNativeBackup(JobControlRecord* jcr)
{
  Jmsg(jcr, M_INFO, 0, T_("Start Backup JobId %llu, Job=%s\n"), jcr->JobId,
       jcr->Job);

  jcr->setJobStatusWithPriorityCheck(JS_Running);
  Dmsg2(100, "JobId=%d JobLevel=%c\n", jcr->dir_impl->jr.JobId,
        jcr->dir_impl->jr.JobLevel);
  if (!jcr->db->UpdateJobStartRecord(jcr, &jcr->dir_impl->jr)) {
    Jmsg(jcr, M_FATAL, 0, "%s", jcr->db->strerror());
    return false;
  }

  if (CheckHardquotas(jcr)) {
    Jmsg(jcr, M_FATAL, 0, T_("Quota Exceeded. Job terminated.\n"));
    return false;
  }

  if (CheckSoftquotas(jcr)) {
    Dmsg0(10, "Quota exceeded\n");
    Jmsg(jcr, M_FATAL, 0,
         T_("Soft Quota exceeded / Grace Time expired. Job terminated.\n"));
    return false;
  }

  /* Open a message channel connection with the Storage
   * daemon. This is to let him know that our client
   * will be contacting him for a backup  session. */
  Dmsg0(110, "Open connection with storage daemon\n");
  jcr->setJobStatusWithPriorityCheck(JS_WaitSD);

  if (!ConnectToStorageDaemon(jcr, 10, me->SDConnectTimeout, true)) {
    return false;
  }

  if (!StartStorageDaemonJob(jcr)) { return false; }

  jcr->setJobStatusWithPriorityCheck(JS_WaitFD);
  if (!ConnectToFileDaemon(jcr, 10, me->FDConnectTimeout, true)) {
    TerminateBackupWithError(jcr);
    return false;
  }
  Dmsg1(120, "jobid: %d: connected\n", jcr->JobId);
  SendJobInfoToFileDaemon(jcr);

  if (jcr->passive_client && jcr->dir_impl->FDVersion < FD_VERSION_51) {
    Jmsg(jcr, M_FATAL, 0,
         T_("Client \"%s\" doesn't support passive client mode. "
            "Please upgrade your client or disable compat mode.\n"),
         jcr->dir_impl->res.client->resource_name_);
    CloseFdConnection(jcr);
    TerminateBackupWithError(jcr);
    return false;
  }

  jcr->setJobStatusWithPriorityCheck(JS_Running);

  if (!SendLevelCommand(jcr)) {
    TerminateBackupWithError(jcr);
    return false;
  }

  if (!SendIncludeExcludeLists(jcr)) {
    TerminateBackupWithError(jcr);
    return false;
  }

  if (!SendPluginOptions(jcr)) {
    TerminateBackupWithError(jcr);
    return false;
  }

  if (!SendPreviousRestoreObjects(jcr)) {
    TerminateBackupWithError(jcr);
    return false;
  }

  if (!SendSecureEraseReqToFd(jcr)) {
    Dmsg1(500, "Unexpected %s secure erase\n", "client");
  }

  if (jcr->dir_impl->res.job->max_bandwidth > 0) {
    jcr->max_bandwidth = jcr->dir_impl->res.job->max_bandwidth;
  } else if (jcr->dir_impl->res.client->max_bandwidth > 0) {
    jcr->max_bandwidth = jcr->dir_impl->res.client->max_bandwidth;
  }

  if (jcr->max_bandwidth > 0) {
    SendBwlimitToFd(jcr, jcr->Job);  // Old clients don't have this command
  }

  // Declare the job started to start the MaxRunTime check
  jcr->setJobStarted();

  if (!SendRunscriptsCommands(jcr)) {
    TerminateBackupWithError(jcr);
    return false;
  }

  /* We re-update the job start record so that the start
   * time is set after the run before job.  This avoids
   * that any files created by the run before job will
   * be saved twice.  They will be backed up in the current
   * job, but not in the next one unless they are changed.
   * Without this, they will be backed up in this job and
   * in the next job run because in that case, their date
   * is after the start of this run. */
  jcr->start_time = time(nullptr);
  jcr->dir_impl->jr.StartTime = jcr->start_time;
  if (!jcr->db->UpdateJobStartRecord(jcr, &jcr->dir_impl->jr)) {
    Jmsg(jcr, M_FATAL, 0, "%s", jcr->db->strerror());
  }

  /* If backup is in accurate mode, we send the list of
   * all files to FD. */
  if (!SendAccurateCurrentFiles(jcr)) {
    TerminateBackupWithError(jcr);
    return false;  // error
  }

  if (!ReserveWriteDevice(jcr, jcr->dir_impl->res.write_storage_list)) {
    return false;
  }

  if (!ConfigureMessageThread(jcr)) { return false; }

  jcr->file_bsock->fsend(backupcmd, jcr->JobFiles);
  Dmsg1(100, ">filed: %s", jcr->file_bsock->msg);
  if (!response(jcr, jcr->file_bsock, OKbackup, "Backup", DISPLAY_ERROR)) {
    TerminateBackupWithError(jcr);
    return false;
  }

  int status = WaitForJobTermination(jcr);
  if (jcr->batch_started) {
    jcr->db_batch->WriteBatchFileRecords(
        jcr);  // used by bulk batch file insert
  }
  if (jcr->HasBase && !jcr->db->CommitBaseFileAttributesRecord(jcr)) {
    Jmsg(jcr, M_FATAL, 0, "%s", jcr->db->strerror());
  }

  /* Check softquotas after job did run.
   * If quota is exceeded now, set the GraceTime. */
  CheckSoftquotas(jcr);

  if (status == JS_Terminated) {
    NativeBackupCleanup(jcr, status);
    return true;
  }

  return false;
}

/*
 * Here we wait for the File daemon to signal termination,
 * then we wait for the Storage daemon. When both are done,
 * we return the job status.
 *
 * Also used by restore.c
 */
int WaitForJobTermination(JobControlRecord* jcr, int timeout)
{
  int32_t n = 0;
  BareosSocket* fd = jcr->file_bsock;
  bool fd_ok = false;
  uint32_t JobFiles, JobErrors;
  uint32_t JobWarnings = 0;
  uint64_t ReadBytes = 0;
  uint64_t JobBytes = 0;
  int VSS = 0;
  int Encrypt = 0;
  btimer_t* tid = nullptr;

  jcr->setJobStatusWithPriorityCheck(JS_Running);

  if (fd) {
    if (timeout) {
      tid = StartBsockTimer(fd, timeout); /* TODO: New timeout directive??? */
    }

    // Wait for Client to terminate
    while ((n = BgetDirmsg(fd)) >= 0) {
      if (!fd_ok
          && sscanf(fd->msg, EndJob, &jcr->dir_impl->FDJobStatus, &JobFiles,
                    &ReadBytes, &JobBytes, &JobErrors, &VSS, &Encrypt)
                 == 7) {
        fd_ok = true;
        jcr->setJobStatusWithPriorityCheck(jcr->dir_impl->FDJobStatus);
        Dmsg1(100, "FDStatus=%c\n", (char)jcr->getJobStatus());
      } else {
        Jmsg(jcr, M_WARNING, 0, T_("Unexpected Client Job message: %s\n"),
             fd->msg);
      }
      if (jcr->IsJobCanceled()) { break; }
    }
    if (tid) { StopBsockTimer(tid); }

    if (IsBnetError(fd)) {
      int i = 0;
      Jmsg(jcr, M_FATAL, 0, T_("Network error with FD during %s: ERR=%s\n"),
           job_type_to_str(jcr->getJobType()), fd->bstrerror());
      while (i++ < 10 && jcr->dir_impl->res.job->RescheduleIncompleteJobs
             && jcr->IsJobCanceled()) {
        Bmicrosleep(3, 0);
      }
    }
    fd->signal(BNET_TERMINATE); /* tell Client we are terminating */
  }

  /* Force cancel in SD if failing, but not for Incomplete jobs so that we let
   * the SD despool. */
  Dmsg5(100, "cancel=%d fd_ok=%d FDJS=%d JS=%d SDJS=%d\n", jcr->IsJobCanceled(),
        fd_ok, jcr->dir_impl->FDJobStatus.load(), jcr->getJobStatus(),
        jcr->dir_impl->SDJobStatus.load());
  if (jcr->IsJobCanceled()
      || (!jcr->dir_impl->res.job->RescheduleIncompleteJobs && !fd_ok)) {
    Dmsg4(100, "fd_ok=%d FDJS=%d JS=%d SDJS=%d\n", fd_ok,
          jcr->dir_impl->FDJobStatus.load(), jcr->getJobStatus(),
          jcr->dir_impl->SDJobStatus.load());
    CancelStorageDaemonJob(jcr);
  }

  // Note, the SD stores in jcr->JobFiles/ReadBytes/JobBytes/JobErrors
  WaitForStorageDaemonTermination(jcr);

  // Return values from FD
  if (fd_ok) {
    jcr->JobFiles = JobFiles;
    jcr->JobErrors += JobErrors; /* Keep total errors */
    jcr->ReadBytes = ReadBytes;
    jcr->JobBytes = JobBytes;
    jcr->JobWarnings = JobWarnings;
    jcr->dir_impl->VSS = VSS;
    jcr->dir_impl->Encrypt = Encrypt;
  } else {
    Jmsg(jcr, M_FATAL, 0, T_("No Job status returned from FD.\n"));
  }

  // Dmsg4(100, "fd_ok=%d FDJS=%d JS=%d SDJS=%d\n", fd_ok,
  // jcr->dir_impl_->FDJobStatus,
  //   jcr->JobStatus, jcr->dir_impl_->SDJobStatus);

  // Return the first error status we find Dir, FD, or SD
  if (!fd_ok || IsBnetError(fd)) { /* if fd not set, that use !fd_ok */
    jcr->dir_impl->FDJobStatus = JS_ErrorTerminated;
  }
  if (jcr->getJobStatus() != JS_Terminated) { return jcr->getJobStatus(); }
  if (jcr->dir_impl->FDJobStatus != JS_Terminated) {
    return jcr->dir_impl->FDJobStatus;
  }
  return jcr->dir_impl->SDJobStatus;
}

// Release resources allocated during backup.
void NativeBackupCleanup(JobControlRecord* jcr, int TermCode)
{
  const char* TermMsg;
  char term_code[100];
  int msg_type = M_INFO;
  ClientDbRecord cr;

  Dmsg2(100, "Enter backup_cleanup %d %c\n", TermCode, TermCode);

  if (jcr->is_JobStatus(JS_Terminated)
      && (jcr->JobErrors || jcr->dir_impl->SDErrors || jcr->JobWarnings)) {
    TermCode = JS_Warnings;
  }

  UpdateJobEnd(jcr, TermCode);

  if (!jcr->db->GetJobRecord(jcr, &jcr->dir_impl->jr)) {
    Jmsg(jcr, M_WARNING, 0,
         T_("Error getting Job record for Job report: ERR=%s\n"),
         jcr->db->strerror());
    jcr->setJobStatusWithPriorityCheck(JS_ErrorTerminated);
  }

  bstrncpy(cr.Name, jcr->dir_impl->res.client->resource_name_, sizeof(cr.Name));
  if (!jcr->db->GetClientRecord(jcr, &cr)) {
    Jmsg(jcr, M_WARNING, 0,
         T_("Error getting Client record for Job report: ERR=%s\n"),
         jcr->db->strerror());
  }

  UpdateBootstrapFile(jcr);

  switch (jcr->getJobStatus()) {
    case JS_Terminated:
      TermMsg = T_("Backup OK");
      break;
    case JS_Incomplete:
      TermMsg = T_("Backup failed -- incomplete");
      break;
    case JS_Warnings:
      TermMsg = T_("Backup OK -- with warnings");
      break;
    case JS_FatalError:
    case JS_ErrorTerminated:
      TermMsg = T_("*** Backup Error ***");
      msg_type = M_ERROR; /* Generate error message */
      if (jcr->store_bsock) {
        jcr->store_bsock->signal(BNET_TERMINATE);
        if (jcr->dir_impl->SD_msg_chan_started) {
          pthread_cancel(jcr->dir_impl->SD_msg_chan);
        }
      }
      break;
    case JS_Canceled:
      TermMsg = T_("Backup Canceled");
      if (jcr->store_bsock) {
        jcr->store_bsock->signal(BNET_TERMINATE);
        if (jcr->dir_impl->SD_msg_chan_started) {
          pthread_cancel(jcr->dir_impl->SD_msg_chan);
        }
      }
      break;
    default:
      TermMsg = term_code;
      sprintf(term_code, T_("Inappropriate term code: %c\n"),
              jcr->getJobStatus());
      break;
  }

  GenerateBackupSummary(jcr, &cr, msg_type, TermMsg);

  Dmsg0(100, "Leave backup_cleanup()\n");
}

void UpdateBootstrapFile(JobControlRecord* jcr)
{
  // Now update the bootstrap file if any
  if (jcr->IsTerminatedOk() && jcr->dir_impl->jr.JobBytes
      && jcr->dir_impl->res.job->WriteBootstrap) {
    FILE* fd;
    int VolCount;
    int got_pipe = 0;
    Bpipe* bpipe = nullptr;
    VolumeParameters* VolParams = nullptr;
    char edt[50], ed1[50], ed2[50];
    POOLMEM* fname = GetPoolMemory(PM_FNAME);

    fname = edit_job_codes(jcr, fname, jcr->dir_impl->res.job->WriteBootstrap,
                           "");
    if (*fname == '|') {
      got_pipe = 1;
      bpipe = OpenBpipe(fname + 1, 0, "w"); /* skip first char "|" */
      fd = bpipe ? bpipe->wfd : nullptr;
    } else {
      /* ***FIXME*** handle BASE */
      fd = fopen(fname, jcr->is_JobLevel(L_FULL) ? "w+b" : "a+b");
    }
    if (fd) {
      VolCount = jcr->db->GetJobVolumeParameters(jcr, jcr->JobId, &VolParams);
      if (VolCount == 0) {
        Jmsg(jcr, M_ERROR, 0,
             T_("Could not get Job Volume Parameters to "
                "update Bootstrap file. ERR=%s\n"),
             jcr->db->strerror());
        if (jcr->dir_impl->SDJobFiles != 0) {
          jcr->setJobStatusWithPriorityCheck(JS_ErrorTerminated);
        }
      }
      /* Start output with when and who wrote it */
      bstrftimes(edt, sizeof(edt), time(nullptr));
      fprintf(fd, "# %s - %s - %s%s\n", edt, jcr->dir_impl->jr.Job,
              JobLevelToString(jcr->getJobLevel()), jcr->dir_impl->since);
      for (int i = 0; i < VolCount; i++) {
        /* Write the record */
        fprintf(fd, "Volume=\"%s\"\n", VolParams[i].VolumeName);
        fprintf(fd, "MediaType=\"%s\"\n", VolParams[i].MediaType);
        if (VolParams[i].Slot > 0) {
          fprintf(fd, "Slot=%d\n", VolParams[i].Slot);
        }
        fprintf(fd, "VolSessionId=%u\n", jcr->VolSessionId);
        fprintf(fd, "VolSessionTime=%u\n", jcr->VolSessionTime);
        fprintf(fd, "VolAddr=%s-%s\n", edit_uint64(VolParams[i].StartAddr, ed1),
                edit_uint64(VolParams[i].EndAddr, ed2));
        fprintf(fd, "FileIndex=%d-%d\n", VolParams[i].FirstIndex,
                VolParams[i].LastIndex);
      }
      if (VolParams) { free(VolParams); }
      if (got_pipe) {
        int status = CloseBpipe(bpipe);
        if (status) {
          BErrNo err;
          Jmsg(jcr, M_ERROR, 0,
               T_("Error running program when updating bootstrap file: "
                  "%s: ERR=%s\n"),
               fname, err.bstrerror(status));
          jcr->setJobStatus(JS_ErrorTerminated);
        }
      } else {
        fclose(fd);
      }
    } else {
      BErrNo be;
      Jmsg(jcr, M_ERROR, 0,
           T_("Could not open WriteBootstrap file:\n"
              "%s: ERR=%s\n"),
           fname, be.bstrerror());
      jcr->setJobStatusWithPriorityCheck(JS_ErrorTerminated);
    }
    FreePoolMemory(fname);
  }
}

/* clang-format off */

/*
 * Generic function which generates a backup summary message.
 * Used by:
 *    - NativeBackupCleanup e.g. normal backups
 *    - NativeVbackupCleanup e.g. virtual backups
 *    - NdmpBackupCleanup e.g. NDMP backups
 */
void GenerateBackupSummary(JobControlRecord *jcr, ClientDbRecord *cr, int msg_type, const char *TermMsg)
{
   char sdt[50], edt[50], schedt[50], gdt[50];
   char ec1[30], ec2[30], ec3[30], ec4[30], ec5[30], compress[50];
   char ec6[30], ec7[30], ec8[30], elapsed[50];
   double kbps, compression;
   utime_t RunTime;
   MediaDbRecord mr;
   PoolMem temp,
            level_info,
            statistics,
            quota_info,
            client_options,
            daemon_status,
            secure_erase_status,
            compress_algo_list;

   bstrftimes(schedt, sizeof(schedt), jcr->dir_impl->jr.SchedTime);
   bstrftimes(sdt, sizeof(sdt), jcr->dir_impl->jr.StartTime);
   bstrftimes(edt, sizeof(edt), jcr->dir_impl->jr.EndTime);
   RunTime = jcr->dir_impl->jr.EndTime - jcr->dir_impl->jr.StartTime;
   bstrftimes(gdt, sizeof(gdt),
              jcr->dir_impl->res.client->GraceTime +
              jcr->dir_impl->res.client->SoftQuotaGracePeriod);

   if (RunTime <= 0) {
      kbps = 0;
   } else {
      kbps = ((double)jcr->dir_impl->jr.JobBytes) / (1000.0 * (double)RunTime);
   }

   if (!jcr->db->GetJobVolumeNames(jcr, jcr->dir_impl->jr.JobId, jcr->VolumeName)) {
      /* Note, if the job has erred, most likely it did not write any
       * tape, so suppress this "error" message since in that case
       * it is normal.  Or look at it the other way, only for a
       * normal exit should we complain about this error. */
      if (jcr->IsTerminatedOk() && jcr->dir_impl->jr.JobBytes) {
         Jmsg(jcr, M_ERROR, 0, "%s", jcr->db->strerror());
      }
      jcr->VolumeName[0] = 0;         /* none */
   }

   if (jcr->VolumeName[0]) {
      // Find last volume name. Multiple vols are separated by |
      char *p = strrchr(jcr->VolumeName, '|');
      if (p) {
         p++;                         /* skip | */
      } else {
         p = jcr->VolumeName;     /* no |, take full name */
      }
      bstrncpy(mr.VolumeName, p, sizeof(mr.VolumeName));
      if (!jcr->db->GetMediaRecord(jcr, &mr)) {
         Jmsg(jcr, M_WARNING, 0, T_("Error getting Media record for Volume \"%s\": ERR=%s\n"),
              mr.VolumeName, jcr->db->strerror());
      }
   }

   if (jcr->ReadBytes == 0) {
      bstrncpy(compress, "None", sizeof(compress));
   } else {
      compression = (double)100 - 100.0 * ((double)jcr->JobBytes / (double)jcr->ReadBytes);
      if (compression < 0.5) {
         bstrncpy(compress, "None", sizeof(compress));
      } else {
         Bsnprintf(compress, sizeof(compress), "%.1f %%", compression);
         FindUsedCompressalgos(&compress_algo_list, jcr);
      }
   }

   std::string fd_term_msg = JobstatusToAscii(jcr->dir_impl->FDJobStatus);
   std::string sd_term_msg = JobstatusToAscii(jcr->dir_impl->SDJobStatus);

   switch (jcr->getJobProtocol()) {
   case PT_NDMP_BAREOS:
      Mmsg(level_info, T_(
           "  Backup Level:           %s%s\n"),
           JobLevelToString(jcr->getJobLevel()), jcr->dir_impl->since);
      Mmsg(statistics, T_(
           "  NDMP Files Written:     %s\n"
           "  SD Files Written:       %s\n"
           "  NDMP Bytes Written:     %s (%sB)\n"
           "  SD Bytes Written:       %s (%sB)\n"),
           edit_uint64_with_commas(jcr->dir_impl->jr.JobFiles, ec1),
           edit_uint64_with_commas(jcr->dir_impl->SDJobFiles, ec2),
           edit_uint64_with_commas(jcr->dir_impl->jr.JobBytes, ec3),
           edit_uint64_with_suffix(jcr->dir_impl->jr.JobBytes, ec4),
           edit_uint64_with_commas(jcr->dir_impl->SDJobBytes, ec5),
           edit_uint64_with_suffix(jcr->dir_impl->SDJobBytes, ec6));
      break;
   case PT_NDMP_NATIVE:
      Mmsg(level_info, T_(
           "  Backup Level:           %s%s\n"),
           JobLevelToString(jcr->getJobLevel()), jcr->dir_impl->since);
      Mmsg(statistics, T_(
           "  NDMP Files Written:     %s\n"
           "  NDMP Bytes Written:     %s (%sB)\n"),
           edit_uint64_with_commas(jcr->dir_impl->jr.JobFiles, ec1),
           edit_uint64_with_commas(jcr->dir_impl->jr.JobBytes, ec3),
           edit_uint64_with_suffix(jcr->dir_impl->jr.JobBytes, ec4));
      break;
   default:
      if (jcr->is_JobLevel(L_VIRTUAL_FULL)) {
         Mmsg(level_info, T_(
              "  Backup Level:           Virtual Full\n"));
         Mmsg(statistics, T_(
              "  SD Files Written:       %s\n"
              "  SD Bytes Written:       %s (%sB)\n"),
              edit_uint64_with_commas(jcr->dir_impl->SDJobFiles, ec2),
              edit_uint64_with_commas(jcr->dir_impl->SDJobBytes, ec5),
              edit_uint64_with_suffix(jcr->dir_impl->SDJobBytes, ec6));
      } else {
         Mmsg(level_info, T_(
              "  Backup Level:           %s%s\n"),
              JobLevelToString(jcr->getJobLevel()), jcr->dir_impl->since);
         Mmsg(statistics, T_(
              "  FD Files Written:       %s\n"
              "  SD Files Written:       %s\n"
              "  FD Bytes Written:       %s (%sB)\n"
              "  SD Bytes Written:       %s (%sB)\n"),
              edit_uint64_with_commas(jcr->dir_impl->jr.JobFiles, ec1),
              edit_uint64_with_commas(jcr->dir_impl->SDJobFiles, ec2),
              edit_uint64_with_commas(jcr->dir_impl->jr.JobBytes, ec3),
              edit_uint64_with_suffix(jcr->dir_impl->jr.JobBytes, ec4),
              edit_uint64_with_commas(jcr->dir_impl->SDJobBytes, ec5),
              edit_uint64_with_suffix(jcr->dir_impl->SDJobBytes, ec6));
      }
      break;
   }

   if (jcr->dir_impl->HasQuota) {
      if (jcr->dir_impl->res.client->GraceTime != 0) {
         bstrftimes(gdt, sizeof(gdt), jcr->dir_impl->res.client->GraceTime +
                                      jcr->dir_impl->res.client->SoftQuotaGracePeriod);
      } else {
         bstrncpy(gdt, "Soft Quota not exceeded", sizeof(gdt));
      }
      Mmsg(quota_info, T_(
           "  Quota Used:             %s (%sB)\n"
           "  Burst Quota:            %s (%sB)\n"
           "  Soft Quota:             %s (%sB)\n"
           "  Hard Quota:             %s (%sB)\n"
           "  Grace Expiry Date:      %s\n"),
           edit_uint64_with_commas(jcr->dir_impl->jr.JobSumTotalBytes+jcr->dir_impl->SDJobBytes, ec1),
           edit_uint64_with_suffix(jcr->dir_impl->jr.JobSumTotalBytes+jcr->dir_impl->SDJobBytes, ec2),
           edit_uint64_with_commas(jcr->dir_impl->res.client->QuotaLimit, ec3),
           edit_uint64_with_suffix(jcr->dir_impl->res.client->QuotaLimit, ec4),
           edit_uint64_with_commas(jcr->dir_impl->res.client->SoftQuota, ec5),
           edit_uint64_with_suffix(jcr->dir_impl->res.client->SoftQuota, ec6),
           edit_uint64_with_commas(jcr->dir_impl->res.client->HardQuota, ec7),
           edit_uint64_with_suffix(jcr->dir_impl->res.client->HardQuota, ec8),
           gdt);
   }

   switch (jcr->getJobProtocol()) {
   case PT_NDMP_BAREOS:
   case PT_NDMP_NATIVE:
      break;
   default:
      if (jcr->is_JobLevel(L_VIRTUAL_FULL)) {
         Mmsg(daemon_status, T_(
              "  SD Errors:              %d\n"
              "  SD termination status:  %s\n"
              "  Accurate:               %s\n"),
           jcr->dir_impl->SDErrors,
           sd_term_msg.c_str(),
           jcr->accurate ? T_("yes") : T_("no"));
      } else {
         if (jcr->HasBase) {
            Mmsg(client_options, T_(
                 "  Software Compression:   %s%s\n"
                 "  Base files/Used files:  %lld/%lld (%.2f%%)\n"
                 "  VSS:                    %s\n"
                 "  Encryption:             %s\n"
                 "  Accurate:               %s\n"),
                 compress,
                 compress_algo_list.c_str(),
                 jcr->nb_base_files,
                 jcr->nb_base_files_used,
                 jcr->nb_base_files_used * 100.0 / jcr->nb_base_files,
                 jcr->dir_impl->VSS ? T_("yes") : T_("no"),
                 jcr->dir_impl->Encrypt ? T_("yes") : T_("no"),
                 jcr->accurate ? T_("yes") : T_("no"));
         } else {
            Mmsg(client_options, T_(
                 "  Software Compression:   %s%s\n"
                 "  VSS:                    %s\n"
                 "  Encryption:             %s\n"
                 "  Accurate:               %s\n"),
                 compress,
                 compress_algo_list.c_str(),
                 jcr->dir_impl->VSS ? T_("yes") : T_("no"),
                 jcr->dir_impl->Encrypt ? T_("yes") : T_("no"),
                 jcr->accurate ? T_("yes") : T_("no"));
         }

         Mmsg(daemon_status, T_(
              "  Non-fatal FD errors:    %d\n"
              "  SD Errors:              %d\n"
              "  FD termination status:  %s\n"
              "  SD termination status:  %s\n"),
           jcr->JobErrors,
           jcr->dir_impl->SDErrors,
           fd_term_msg.c_str(),
           sd_term_msg.c_str());

         if (me->secure_erase_cmdline) {
            Mmsg(temp,"  Dir Secure Erase Cmd:   %s\n", me->secure_erase_cmdline);
            PmStrcat(secure_erase_status, temp.c_str());
         }
         if (!bstrcmp(jcr->dir_impl->FDSecureEraseCmd, "*None*")) {
            Mmsg(temp, "  FD  Secure Erase Cmd:   %s\n", jcr->dir_impl->FDSecureEraseCmd);
            PmStrcat(secure_erase_status, temp.c_str());
         }
         if (!bstrcmp(jcr->dir_impl->SDSecureEraseCmd, "*None*")) {
            Mmsg(temp, "  SD  Secure Erase Cmd:   %s\n", jcr->dir_impl->SDSecureEraseCmd);
            PmStrcat(secure_erase_status, temp.c_str());
         }
      }
      break;
   }

// Bmicrosleep(15, 0);                /* for debugging SIGHUP */

   Jmsg(jcr, msg_type, 0, T_("%s %s %s (%s):\n"
        "  Build OS:               %s\n"
        "  JobId:                  %d\n"
        "  Job:                    %s\n"
        "%s"
        "  Client:                 \"%s\" %s\n"
        "  FileSet:                \"%s\" %s\n"
        "  Pool:                   \"%s\" (From %s)\n"
        "  Catalog:                \"%s\" (From %s)\n"
        "  Storage:                \"%s\" (From %s)\n"
        "  Scheduled time:         %s\n"
        "  Start time:             %s\n"
        "  End time:               %s\n"
        "  Elapsed time:           %s\n"
        "  Priority:               %d\n"
        "  Allow Mixed Priority:   %s\n"
        "%s"                                         /* FD/SD Statistics */
        "%s"                                         /* Quota info */
        "  Rate:                   %.1f KB/s\n"
        "%s"                                         /* Client options */
        "  Volume name(s):         %s\n"
        "  Volume Session Id:      %d\n"
        "  Volume Session Time:    %d\n"
        "  Last Volume Bytes:      %s (%sB)\n"
        "%s"                                        /* Daemon status info */
        "%s"                                        /* SecureErase status */
        "  Bareos binary info:     %s\n"
        "  Job triggered by:       %s\n"
        "  Termination:            %s\n\n"),
        BAREOS, my_name, kBareosVersionStrings.Full,
        kBareosVersionStrings.ShortDate, kBareosVersionStrings.GetOsInfo(),
        jcr->dir_impl->jr.JobId,
        jcr->dir_impl->jr.Job,
        level_info.c_str(),
        jcr->dir_impl->res.client->resource_name_, cr->Uname,
        jcr->dir_impl->res.fileset->resource_name_, jcr->dir_impl->FSCreateTime,
        jcr->dir_impl->res.pool->resource_name_, jcr->dir_impl->res.pool_source,
        jcr->dir_impl->res.catalog->resource_name_, jcr->dir_impl->res.catalog_source,
        jcr->dir_impl->res.write_storage->resource_name_, jcr->dir_impl->res.wstore_source,
        schedt,
        sdt,
        edt,
        edit_utime(RunTime, elapsed, sizeof(elapsed)),
        jcr->JobPriority,
        jcr->allow_mixed_priority ? "yes" : "no",
        statistics.c_str(),
        quota_info.c_str(),
        kbps,
        client_options.c_str(),
        jcr->VolumeName,
        jcr->VolSessionId,
        jcr->VolSessionTime,
        edit_uint64_with_commas(mr.VolBytes, ec7),
        edit_uint64_with_suffix(mr.VolBytes, ec8),
        daemon_status.c_str(),
        secure_erase_status.c_str(),
        kBareosVersionStrings.JoblogMessage,
        JobTriggerToString(jcr->dir_impl->job_trigger).c_str(),
        TermMsg);

  /* clang-format on */
}
} /* namespace directordaemon */
