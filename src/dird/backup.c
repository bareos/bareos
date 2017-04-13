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
 * Kern Sibbald, March MM
 */
/**
 * @file
 * responsible for doing backup jobs
 *
 * Basic tasks done here:
 *    Open DB and create records for this job.
 *    Open Message Channel with Storage daemon to tell him a job will be starting.
 *    Open connection with File daemon and pass him commands to do the backup.
 *    When the File daemon finishes the job, update the DB.
 */

#include "bareos.h"
#include "dird.h"

/* Commands sent to File daemon */
static char backupcmd[] =
   "backup FileIndex=%ld\n";
static char storaddrcmd[] =
   "storage address=%s port=%d ssl=%d\n";
static char passiveclientcmd[] =
   "passive client address=%s port=%d ssl=%d\n";

/* Responses received from File daemon */
static char OKbackup[] =
   "2000 OK backup\n";
static char OKstore[] =
   "2000 OK storage\n";
static char OKpassiveclient[] =
   "2000 OK passive client\n";
static char EndJob[] =
   "2800 End Job TermCode=%d JobFiles=%u "
   "ReadBytes=%llu JobBytes=%llu Errors=%u "
   "VSS=%d Encrypt=%d\n";

static inline bool validate_client(JCR *jcr)
{
   switch (jcr->res.client->Protocol) {
   case APT_NATIVE:
      return true;
   default:
      Jmsg(jcr, M_FATAL, 0, _("Client %s has illegal backup protocol %s for Native backup\n"),
           jcr->res.client->name(), auth_protocol_to_str(jcr->res.client->Protocol));
      return false;
   }
}

/**
 * if both FD and SD have LanAddress set, use the storages' LanAddress to connect to.
 */
char* storage_address_to_contact(CLIENTRES *client, STORERES *store)
{
   if (store->lanaddress && client->lanaddress) {
      return store->lanaddress;
   } else {
      return store->address;
   }
}

/**
 * if both FD and SD have LanAddress set, use the clients' LanAddress to connect to.
 *
 */
char* client_address_to_contact(CLIENTRES *client, STORERES *store)
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
char* storage_address_to_contact(STORERES *rstore, STORERES *wstore)
{
   if (rstore->lanaddress && wstore->lanaddress) {
      return wstore->lanaddress;
   } else {
      return wstore->address;
   }
}

static inline bool validate_storage(JCR *jcr)
{
   STORERES *store;

   foreach_alist(store, jcr->res.wstorage) {
      switch (store->Protocol) {
      case APT_NATIVE:
         continue;
      default:
         Jmsg(jcr, M_FATAL, 0, _("Storage %s has illegal backup protocol %s for Native backup\n"),
              store->name(), auth_protocol_to_str(store->Protocol));
         return false;
      }
   }

   return true;
}

/*
 * Called here before the job is run to do the job specific setup.
 */
bool do_native_backup_init(JCR *jcr)
{
   free_rstorage(jcr);                   /* we don't read so release */

   if (!allow_duplicate_job(jcr)) {
      return false;
   }

   jcr->jr.PoolId = get_or_create_pool_record(jcr, jcr->res.pool->name());
   if (jcr->jr.PoolId == 0) {
      return false;
   }

   /*
    * If pool storage specified, use it instead of job storage
    */
   copy_wstorage(jcr, jcr->res.pool->storage, _("Pool resource"));
   if (!jcr->res.wstorage) {
      Jmsg(jcr, M_FATAL, 0, _("No Storage specification found in Job or Pool.\n"));
      return false;
   }

   /*
    * Validate that we have a native client and storage(s).
    */
   if (!validate_client(jcr) || !validate_storage(jcr)) {
      return false;
   }

   create_clones(jcr);                /* run any clone jobs */

   return true;
}

/*
 * Take all base jobs from job resource and find the last L_BASE jobid.
 */
static bool get_base_jobids(JCR *jcr, db_list_ctx *jobids)
{
   JOB_DBR jr;
   JOBRES *job;
   JobId_t id;
   char str_jobid[50];

   if (!jcr->res.job->base) {
      return false;             /* no base job, stop accurate */
   }

   memset(&jr, 0, sizeof(jr));
   jr.StartTime = jcr->jr.StartTime;

   foreach_alist(job, jcr->res.job->base) {
      bstrncpy(jr.Name, job->name(), sizeof(jr.Name));
      jcr->db->get_base_jobid(jcr, &jr, &id);

      if (id) {
         if (jobids->count) {
            pm_strcat(jobids->list, ",");
         }
         pm_strcat(jobids->list, edit_uint64(id, str_jobid));
         jobids->count++;
      }
   }

   return jobids->count > 0;
}

/*
 * Foreach files in currrent list, send "/path/fname\0LStat\0MD5\0Delta" to FD
 *      row[0]=Path, row[1]=Filename, row[2]=FileIndex
 *      row[3]=JobId row[4]=LStat row[5]=DeltaSeq row[6]=MD5
 */
static int accurate_list_handler(void *ctx, int num_fields, char **row)
{
   JCR *jcr = (JCR *)ctx;

   if (job_canceled(jcr)) {
      return 1;
   }

   if (row[2][0] == '0') {           /* discard when file_index == 0 */
      return 0;
   }

   /* sending with checksum */
   if (jcr->use_accurate_chksum &&
       num_fields == 7 &&
       row[6][0] && /* skip checksum = '0' */
       row[6][1]) {
      jcr->file_bsock->fsend("%s%s%c%s%c%s%c%s",
                             row[0], row[1], 0, row[4], 0, row[6], 0, row[5]);
   } else {
      jcr->file_bsock->fsend("%s%s%c%s%c%c%s",
                             row[0], row[1], 0, row[4], 0, 0, row[5]);
   }
   return 0;
}

/* In this procedure, we check if the current fileset is using checksum
 * FileSet-> Include-> Options-> Accurate/Verify/BaseJob=checksum
 * This procedure uses jcr->HasBase, so it must be call after the initialization
 */
static bool is_checksum_needed_by_fileset(JCR *jcr)
{
   INCEXE *inc;
   FOPTS *fopts;
   FILESETRES *fs;
   bool in_block=false;
   bool have_basejob_option=false;

   if (!jcr->res.job || !jcr->res.job->fileset) {
      return false;
   }

   fs = jcr->res.job->fileset;
   for (int i = 0; i < fs->num_includes; i++) { /* Parse all Include {} */
      inc = fs->include_items[i];

      for (int j = 0; j < inc->num_opts; j++) { /* Parse all Options {} */
         fopts = inc->opts_list[j];

         for (char *k = fopts->opts; *k; k++) { /* Try to find one request */
            switch (*k) {
            case 'V':           /* verify */
               in_block = jcr->is_JobType(JT_VERIFY); /* not used now */
               break;
            case 'J':           /* Basejob keyword */
               have_basejob_option = in_block = jcr->HasBase;
               break;
            case 'C':           /* Accurate keyword */
               in_block = !jcr->is_JobLevel(L_FULL);
               break;
            case ':':           /* End of keyword */
               in_block = false;
               break;
            case '5':           /* MD5  */
            case '1':           /* SHA1 */
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
   if (!have_basejob_option && jcr->HasBase) {
      return true;
   }

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
bool send_accurate_current_files(JCR *jcr)
{
   POOL_MEM buf;
   db_list_ctx jobids;
   db_list_ctx nb;

   /*
    * In base level, no previous job is used and no restart incomplete jobs
    */
   if (jcr->is_canceled() || jcr->is_JobLevel(L_BASE)) {
      return true;
   }

   if (!jcr->accurate) {
      return true;
   }

   if (jcr->is_JobLevel(L_FULL)) {
      /*
       * On Full mode, if no previous base job, no accurate things
       */
      if (get_base_jobids(jcr, &jobids)) {
         jcr->HasBase = true;
         Jmsg(jcr, M_INFO, 0, _("Using BaseJobId(s): %s\n"), jobids.list);
      } else {
         return true;
      }
   } else {
      /*
       * For Incr/Diff level, we search for older jobs
       */
      jcr->db->accurate_get_jobids(jcr, &jcr->jr, &jobids);

      /*
       * We are in Incr/Diff, but no Full to build the accurate list...
       */
      if (jobids.count == 0) {
         Jmsg(jcr, M_FATAL, 0, _("Cannot find previous jobids.\n"));
         return false;  /* fail */
      }
   }

   /*
    * Don't send and store the checksum if fileset doesn't require it
    */
   jcr->use_accurate_chksum = is_checksum_needed_by_fileset(jcr);
   if (jcr->JobId) {            /* display the message only for real jobs */
      Jmsg(jcr, M_INFO, 0, _("Sending Accurate information.\n"));
   }

   /*
    * To be able to allocate the right size for htable
    */
   Mmsg(buf, "SELECT sum(JobFiles) FROM Job WHERE JobId IN (%s)", jobids.list);
   jcr->db->sql_query(buf.c_str(), db_list_handler, &nb);
   Dmsg2(200, "jobids=%s nb=%s\n", jobids.list, nb.list);
   jcr->file_bsock->fsend("accurate files=%s\n", nb.list);

   if (jcr->HasBase) {
      jcr->nb_base_files = str_to_int64(nb.list);
      if (!jcr->db->create_base_file_list(jcr, jobids.list)){
         Jmsg0(jcr, M_FATAL, 0, "error in jcr->db->create_base_file_list\n");
         return false;
      }
      if (!jcr->db->get_base_file_list(jcr, jcr->use_accurate_chksum,
                                  accurate_list_handler, (void *)jcr)) {
         Jmsg0(jcr, M_FATAL, 0, "error in jcr->db->get_base_file_list\n");
         return false;
      }
   } else {
      if (!jcr->db->open_batch_connection(jcr)) {
         Jmsg0(jcr, M_FATAL, 0, "Can't get batch sql connection");
         return false;  /* Fail */
      }

      jcr->db_batch->get_file_list(jcr, jobids.list, jcr->use_accurate_chksum,
                                   false /* no delta */, accurate_list_handler, (void *)jcr);
   }

   jcr->file_bsock->signal(BNET_EOD);
   return true;
}

/*
 * Do a backup of the specified FileSet
 *
 *  Returns:  false on failure
 *            true  on success
 */
bool do_native_backup(JCR *jcr)
{
   int status;
   int tls_need = BNET_TLS_NONE;
   BSOCK *fd = NULL;
   BSOCK *sd = NULL;
   STORERES *store = NULL;
   CLIENTRES *client = NULL;
   char ed1[100];
   db_int64_ctx job;
   POOL_MEM buf;

   /* Print Job Start message */
   Jmsg(jcr, M_INFO, 0, _("Start Backup JobId %s, Job=%s\n"),
        edit_uint64(jcr->JobId, ed1), jcr->Job);

   jcr->setJobStatus(JS_Running);
   Dmsg2(100, "JobId=%d JobLevel=%c\n", jcr->jr.JobId, jcr->jr.JobLevel);
   if (!jcr->db->update_job_start_record(jcr, &jcr->jr)) {
      Jmsg(jcr, M_FATAL, 0, "%s", jcr->db->strerror());
      return false;
   }

   if (check_hardquotas(jcr)) {
      Jmsg(jcr, M_FATAL, 0, _("Quota Exceeded. Job terminated.\n"));
      return false;
   }

   if (check_softquotas(jcr)) {
      Dmsg0(10, "Quota exceeded\n");
      Jmsg(jcr, M_FATAL, 0, _("Soft Quota exceeded / Grace Time expired. Job terminated.\n"));
      return false;
   }

   /*
    * Open a message channel connection with the Storage
    * daemon. This is to let him know that our client
    * will be contacting him for a backup  session.
    */
   Dmsg0(110, "Open connection with storage daemon\n");
   jcr->setJobStatus(JS_WaitSD);

   /*
    * Start conversation with Storage daemon
    */
   if (!connect_to_storage_daemon(jcr, 10, me->SDConnectTimeout, true)) {
      return false;
   }
   sd = jcr->store_bsock;

   /*
    * Now start a job with the Storage daemon
    */
   if (!start_storage_daemon_job(jcr, NULL, jcr->res.wstorage)) {
      return false;
   }

   /*
    * When the client is not in passive mode we can put the SD in
    * listen mode for the FD connection.
    */
   jcr->passive_client = jcr->res.client->passive;
   if (!jcr->passive_client) {
      /*
       * Start the job prior to starting the message thread below
       * to avoid two threads from using the BSOCK structure at
       * the same time.
       */
      if (!sd->fsend("run")) {
         return false;
      }

      /*
       * Now start a Storage daemon message thread.  Note,
       * this thread is used to provide the catalog services
       * for the backup job, including inserting the attributes
       * into the catalog.  See catalog_update() in catreq.c
       */
      if (!start_storage_daemon_message_thread(jcr)) {
         return false;
      }

      Dmsg0(150, "Storage daemon connection OK\n");
   }

   jcr->setJobStatus(JS_WaitFD);
   if (!connect_to_file_daemon(jcr, 10, me->FDConnectTimeout, true)) {
      goto bail_out;
   }
   Dmsg1(120, "jobid: %d: connected\n", jcr->JobId);
   send_job_info(jcr);
   fd = jcr->file_bsock;

   /*
    * Check if the file daemon supports passive client mode.
    */
   if (jcr->passive_client && jcr->FDVersion < FD_VERSION_51) {
      Jmsg(jcr, M_FATAL, 0,
            _("Client \"%s\" doesn't support passive client mode. "
              "Please upgrade your client or disable compat mode.\n"),
           jcr->res.client->name());
      goto close_fd;
   }

   jcr->setJobStatus(JS_Running);

   if (!send_level_command(jcr)) {
      goto bail_out;
   }

   if (!send_include_list(jcr)) {
      goto bail_out;
   }

   if (!send_exclude_list(jcr)) {
      goto bail_out;
   }

   if (!send_previous_restore_objects(jcr)) {
      goto bail_out;
   }

   if (!send_secure_erase_req_to_fd(jcr)) {
      Dmsg1(500,"Unexpected %s secure erase\n","client");
   }

   if (jcr->res.job->max_bandwidth > 0) {
      jcr->max_bandwidth = jcr->res.job->max_bandwidth;
   } else if (jcr->res.client->max_bandwidth > 0) {
      jcr->max_bandwidth = jcr->res.client->max_bandwidth;
   }

   if (jcr->max_bandwidth > 0) {
      send_bwlimit_to_fd(jcr, jcr->Job); /* Old clients don't have this command */
   }

   client = jcr->res.client;
   store = jcr->res.wstore;
   char *connection_target_address;

   /*
    * See if the client is a passive client or not.
    */
   if (!jcr->passive_client) {
      /*
       * Send Storage daemon address to the File daemon
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

      connection_target_address = storage_address_to_contact(client, store);

      fd->fsend(storaddrcmd, connection_target_address, store->SDDport, tls_need);
      if (!response(jcr, fd, OKstore, "Storage", DISPLAY_ERROR)) {
         goto bail_out;
      }
   } else {

      /*
       * TLS Requirement
       */
      if (client->tls.enable) {
         if (client->tls.require) {
            tls_need = BNET_TLS_REQUIRED;
         } else {
            tls_need = BNET_TLS_OK;
         }
      }

      connection_target_address = client_address_to_contact(client, store);

      /*
       * Tell the SD to connect to the FD.
       */
      sd->fsend(passiveclientcmd, connection_target_address, client->FDport, tls_need);
      if (!response(jcr, sd, OKpassiveclient, "Passive client", DISPLAY_ERROR)) {
         goto bail_out;
      }

      /*
       * Start the job prior to starting the message thread below
       * to avoid two threads from using the BSOCK structure at
       * the same time.
       */
      if (!jcr->store_bsock->fsend("run")) {
         return false;
      }

      /*
       * Now start a Storage daemon message thread.  Note,
       * this thread is used to provide the catalog services
       * for the backup job, including inserting the attributes
       * into the catalog.  See catalog_update() in catreq.c
       */
      if (!start_storage_daemon_message_thread(jcr)) {
         return false;
      }

      Dmsg0(150, "Storage daemon connection OK\n");
   }

   /*
    * Declare the job started to start the MaxRunTime check
    */
   jcr->setJobStarted();

   /*
    * Send and run the RunBefore
    */
   if (!send_runscripts_commands(jcr)) {
      goto bail_out;
   }

   /*
    * We re-update the job start record so that the start
    * time is set after the run before job.  This avoids
    * that any files created by the run before job will
    * be saved twice.  They will be backed up in the current
    * job, but not in the next one unless they are changed.
    * Without this, they will be backed up in this job and
    * in the next job run because in that case, their date
    * is after the start of this run.
    */
   jcr->start_time = time(NULL);
   jcr->jr.StartTime = jcr->start_time;
   if (!jcr->db->update_job_start_record(jcr, &jcr->jr)) {
      Jmsg(jcr, M_FATAL, 0, "%s", jcr->db->strerror());
   }

   /*
    * If backup is in accurate mode, we send the list of
    * all files to FD.
    */
   if (!send_accurate_current_files(jcr)) {
      goto bail_out;     /* error */
   }

   /*
    * Send backup command
    */
   fd->fsend(backupcmd, jcr->JobFiles);
   Dmsg1(100, ">filed: %s", fd->msg);
   if (!response(jcr, fd, OKbackup, "Backup", DISPLAY_ERROR)) {
      goto bail_out;
   }

   /*
    * Pickup Job termination data
    */
   status = wait_for_job_termination(jcr);
   if (jcr->batch_started) {
      jcr->db_batch->write_batch_file_records(jcr); /* used by bulk batch file insert */
   }

   if (jcr->HasBase && !jcr->db->commit_base_file_attributes_record(jcr))  {
      Jmsg(jcr, M_FATAL, 0, "%s", jcr->db->strerror());
   }

   /*
    * Check softquotas after job did run.
    * If quota is exceeded now, set the GraceTime.
    */
   check_softquotas(jcr);

   if (status == JS_Terminated) {
      native_backup_cleanup(jcr, status);
      return true;
   }

   return false;

close_fd:
   if (jcr->file_bsock) {
      jcr->file_bsock->signal(BNET_TERMINATE);
      jcr->file_bsock->close();
      delete jcr->file_bsock;
      jcr->file_bsock = NULL;
   }

bail_out:
   jcr->setJobStatus(JS_ErrorTerminated);
   wait_for_job_termination(jcr, me->FDConnectTimeout);

   return false;
}

/*
 * Here we wait for the File daemon to signal termination,
 * then we wait for the Storage daemon. When both are done,
 * we return the job status.
 *
 * Also used by restore.c
 */
int wait_for_job_termination(JCR *jcr, int timeout)
{
   int32_t n = 0;
   BSOCK *fd = jcr->file_bsock;
   bool fd_ok = false;
   uint32_t JobFiles, JobErrors;
   uint32_t JobWarnings = 0;
   uint64_t ReadBytes = 0;
   uint64_t JobBytes = 0;
   int VSS = 0;
   int Encrypt = 0;
   btimer_t *tid=NULL;

   jcr->setJobStatus(JS_Running);

   if (fd) {
      if (timeout) {
         tid = start_bsock_timer(fd, timeout); /* TODO: New timeout directive??? */
      }

      /*
       * Wait for Client to terminate
       */
      while ((n = bget_dirmsg(fd)) >= 0) {
         if (!fd_ok &&
             sscanf(fd->msg, EndJob, &jcr->FDJobStatus, &JobFiles,
                     &ReadBytes, &JobBytes, &JobErrors, &VSS, &Encrypt) == 7) {
            fd_ok = true;
            jcr->setJobStatus(jcr->FDJobStatus);
            Dmsg1(100, "FDStatus=%c\n", (char)jcr->JobStatus);
         } else {
            Jmsg(jcr, M_WARNING, 0, _("Unexpected Client Job message: %s\n"),
                 fd->msg);
         }
         if (job_canceled(jcr)) {
            break;
         }
      }
      if (tid) {
         stop_bsock_timer(tid);
      }

      if (is_bnet_error(fd)) {
         int i = 0;
         Jmsg(jcr, M_FATAL, 0, _("Network error with FD during %s: ERR=%s\n"),
              job_type_to_str(jcr->getJobType()), fd->bstrerror());
         while (i++ < 10 && jcr->res.job->RescheduleIncompleteJobs && jcr->is_canceled()) {
            bmicrosleep(3, 0);
         }

      }
      fd->signal(BNET_TERMINATE);   /* tell Client we are terminating */
   }

   /*
    * Force cancel in SD if failing, but not for Incomplete jobs so that we let the SD despool.
    */
   Dmsg5(100, "cancel=%d fd_ok=%d FDJS=%d JS=%d SDJS=%d\n", jcr->is_canceled(), fd_ok, jcr->FDJobStatus,
        jcr->JobStatus, jcr->SDJobStatus);
   if (jcr->is_canceled() || (!jcr->res.job->RescheduleIncompleteJobs && !fd_ok)) {
      Dmsg4(100, "fd_ok=%d FDJS=%d JS=%d SDJS=%d\n", fd_ok, jcr->FDJobStatus,
           jcr->JobStatus, jcr->SDJobStatus);
      cancel_storage_daemon_job(jcr);
   }

   /*
    * Note, the SD stores in jcr->JobFiles/ReadBytes/JobBytes/JobErrors
    */
   wait_for_storage_daemon_termination(jcr);

   /*
    * Return values from FD
    */
   if (fd_ok) {
      jcr->JobFiles = JobFiles;
      jcr->JobErrors += JobErrors;       /* Keep total errors */
      jcr->ReadBytes = ReadBytes;
      jcr->JobBytes = JobBytes;
      jcr->JobWarnings = JobWarnings;
      jcr->VSS = VSS;
      jcr->Encrypt = Encrypt;
   } else {
      Jmsg(jcr, M_FATAL, 0, _("No Job status returned from FD.\n"));
   }

// Dmsg4(100, "fd_ok=%d FDJS=%d JS=%d SDJS=%d\n", fd_ok, jcr->FDJobStatus,
//   jcr->JobStatus, jcr->SDJobStatus);

   /*
    * Return the first error status we find Dir, FD, or SD
    */
   if (!fd_ok || is_bnet_error(fd)) { /* if fd not set, that use !fd_ok */
      jcr->FDJobStatus = JS_ErrorTerminated;
   }
   if (jcr->JobStatus != JS_Terminated) {
      return jcr->JobStatus;
   }
   if (jcr->FDJobStatus != JS_Terminated) {
      return jcr->FDJobStatus;
   }
   return jcr->SDJobStatus;
}

/*
 * Release resources allocated during backup.
 */
void native_backup_cleanup(JCR *jcr, int TermCode)
{
   const char *term_msg;
   char term_code[100];
   int msg_type = M_INFO;
   CLIENT_DBR cr;

   Dmsg2(100, "Enter backup_cleanup %d %c\n", TermCode, TermCode);
   memset(&cr, 0, sizeof(cr));

   if (jcr->is_JobStatus(JS_Terminated) &&
      (jcr->JobErrors || jcr->SDErrors || jcr->JobWarnings)) {
      TermCode = JS_Warnings;
   }

   update_job_end(jcr, TermCode);

   if (!jcr->db->get_job_record(jcr, &jcr->jr)) {
      Jmsg(jcr, M_WARNING, 0, _("Error getting Job record for Job report: ERR=%s"), jcr->db->strerror());
      jcr->setJobStatus(JS_ErrorTerminated);
   }

   bstrncpy(cr.Name, jcr->res.client->name(), sizeof(cr.Name));
   if (!jcr->db->get_client_record(jcr, &cr)) {
      Jmsg(jcr, M_WARNING, 0, _("Error getting Client record for Job report: ERR=%s"), jcr->db->strerror());
   }

   update_bootstrap_file(jcr);

   switch (jcr->JobStatus) {
      case JS_Terminated:
         term_msg = _("Backup OK");
         break;
      case JS_Incomplete:
         term_msg = _("Backup failed -- incomplete");
         break;
      case JS_Warnings:
         term_msg = _("Backup OK -- with warnings");
         break;
      case JS_FatalError:
      case JS_ErrorTerminated:
         term_msg = _("*** Backup Error ***");
         msg_type = M_ERROR;          /* Generate error message */
         if (jcr->store_bsock) {
            jcr->store_bsock->signal(BNET_TERMINATE);
            if (jcr->SD_msg_chan_started) {
               pthread_cancel(jcr->SD_msg_chan);
            }
         }
         break;
      case JS_Canceled:
         term_msg = _("Backup Canceled");
         if (jcr->store_bsock) {
            jcr->store_bsock->signal(BNET_TERMINATE);
            if (jcr->SD_msg_chan_started) {
               pthread_cancel(jcr->SD_msg_chan);
            }
         }
         break;
      default:
         term_msg = term_code;
         sprintf(term_code, _("Inappropriate term code: %c\n"), jcr->JobStatus);
         break;
   }

   generate_backup_summary(jcr, &cr, msg_type, term_msg);

   Dmsg0(100, "Leave backup_cleanup()\n");
}

void update_bootstrap_file(JCR *jcr)
{
   /*
    * Now update the bootstrap file if any
    */
   if (jcr->is_terminated_ok() &&
       jcr->jr.JobBytes &&
       jcr->res.job->WriteBootstrap) {
      FILE *fd;
      int VolCount;
      int got_pipe = 0;
      BPIPE *bpipe = NULL;
      VOL_PARAMS *VolParams = NULL;
      char edt[50], ed1[50], ed2[50];
      POOLMEM *fname = get_pool_memory(PM_FNAME);

      fname = edit_job_codes(jcr, fname, jcr->res.job->WriteBootstrap, "");
      if (*fname == '|') {
         got_pipe = 1;
         bpipe = open_bpipe(fname+1, 0, "w"); /* skip first char "|" */
         fd = bpipe ? bpipe->wfd : NULL;
      } else {
         /* ***FIXME*** handle BASE */
         fd = fopen(fname, jcr->is_JobLevel(L_FULL)?"w+b":"a+b");
      }
      if (fd) {
         VolCount = jcr->db->get_job_volume_parameters(jcr, jcr->JobId, &VolParams);
         if (VolCount == 0) {
            Jmsg(jcr, M_ERROR, 0, _("Could not get Job Volume Parameters to "
                 "update Bootstrap file. ERR=%s\n"), jcr->db->strerror());
             if (jcr->SDJobFiles != 0) {
                jcr->setJobStatus(JS_ErrorTerminated);
             }

         }
         /* Start output with when and who wrote it */
         bstrftimes(edt, sizeof(edt), time(NULL));
         fprintf(fd, "# %s - %s - %s%s\n", edt, jcr->jr.Job,
                 level_to_str(jcr->getJobLevel()), jcr->since);
         for (int i=0; i < VolCount; i++) {
            /* Write the record */
            fprintf(fd, "Volume=\"%s\"\n", VolParams[i].VolumeName);
            fprintf(fd, "MediaType=\"%s\"\n", VolParams[i].MediaType);
            if (VolParams[i].Slot > 0) {
               fprintf(fd, "Slot=%d\n", VolParams[i].Slot);
            }
            fprintf(fd, "VolSessionId=%u\n", jcr->VolSessionId);
            fprintf(fd, "VolSessionTime=%u\n", jcr->VolSessionTime);
            fprintf(fd, "VolAddr=%s-%s\n",
                    edit_uint64(VolParams[i].StartAddr, ed1),
                    edit_uint64(VolParams[i].EndAddr, ed2));
            fprintf(fd, "FileIndex=%d-%d\n", VolParams[i].FirstIndex,
                         VolParams[i].LastIndex);
         }
         if (VolParams) {
            free(VolParams);
         }
         if (got_pipe) {
            close_bpipe(bpipe);
         } else {
            fclose(fd);
         }
      } else {
         berrno be;
         Jmsg(jcr, M_ERROR, 0, _("Could not open WriteBootstrap file:\n"
              "%s: ERR=%s\n"), fname, be.bstrerror());
         jcr->setJobStatus(JS_ErrorTerminated);
      }
      free_pool_memory(fname);
   }
}

/*
 * Generic function which generates a backup summary message.
 * Used by:
 *    - native_backup_cleanup e.g. normal backups
 *    - native_vbackup_cleanup e.g. virtual backups
 *    - ndmp_backup_cleanup e.g. NDMP backups
 */
void generate_backup_summary(JCR *jcr, CLIENT_DBR *cr, int msg_type, const char *term_msg)
{
   char sdt[50], edt[50], schedt[50], gdt[50];
   char ec1[30], ec2[30], ec3[30], ec4[30], ec5[30], compress[50];
   char ec6[30], ec7[30], ec8[30], elapsed[50];
   char fd_term_msg[100], sd_term_msg[100];
   double kbps, compression;
   utime_t RunTime;
   MEDIA_DBR mr;
   POOL_MEM temp,
            level_info,
            statistics,
            quota_info,
            client_options,
            daemon_status,
            secure_erase_status,
            compress_algo_list;

   memset(&mr, 0, sizeof(mr));
   bstrftimes(schedt, sizeof(schedt), jcr->jr.SchedTime);
   bstrftimes(sdt, sizeof(sdt), jcr->jr.StartTime);
   bstrftimes(edt, sizeof(edt), jcr->jr.EndTime);
   RunTime = jcr->jr.EndTime - jcr->jr.StartTime;
   bstrftimes(gdt, sizeof(gdt),
              jcr->res.client->GraceTime +
              jcr->res.client->SoftQuotaGracePeriod);

   if (RunTime <= 0) {
      kbps = 0;
   } else {
      kbps = ((double)jcr->jr.JobBytes) / (1000.0 * (double)RunTime);
   }

   if (!jcr->db->get_job_volume_names(jcr, jcr->jr.JobId, jcr->VolumeName)) {
      /*
       * Note, if the job has erred, most likely it did not write any
       * tape, so suppress this "error" message since in that case
       * it is normal.  Or look at it the other way, only for a
       * normal exit should we complain about this error.
       */
      if (jcr->is_terminated_ok() && jcr->jr.JobBytes) {
         Jmsg(jcr, M_ERROR, 0, "%s", jcr->db->strerror());
      }
      jcr->VolumeName[0] = 0;         /* none */
   }

   if (jcr->VolumeName[0]) {
      /*
       * Find last volume name. Multiple vols are separated by |
       */
      char *p = strrchr(jcr->VolumeName, '|');
      if (p) {
         p++;                         /* skip | */
      } else {
         p = jcr->VolumeName;     /* no |, take full name */
      }
      bstrncpy(mr.VolumeName, p, sizeof(mr.VolumeName));
      if (!jcr->db->get_media_record(jcr, &mr)) {
         Jmsg(jcr, M_WARNING, 0, _("Error getting Media record for Volume \"%s\": ERR=%s"),
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
         bsnprintf(compress, sizeof(compress), "%.1f %%", compression);
         find_used_compressalgos(&compress_algo_list, jcr);
      }
   }

   jobstatus_to_ascii(jcr->FDJobStatus, fd_term_msg, sizeof(fd_term_msg));
   jobstatus_to_ascii(jcr->SDJobStatus, sd_term_msg, sizeof(sd_term_msg));

   switch (jcr->getJobProtocol()) {
   case PT_NDMP:
      Mmsg(level_info, _(
           "  Backup Level:           %s%s\n"),
           level_to_str(jcr->getJobLevel()), jcr->since);
      Mmsg(statistics, _(
           "  NDMP Files Written:     %s\n"
           "  SD Files Written:       %s\n"
           "  NDMP Bytes Written:     %s (%sB)\n"
           "  SD Bytes Written:       %s (%sB)\n"),
           edit_uint64_with_commas(jcr->jr.JobFiles, ec1),
           edit_uint64_with_commas(jcr->SDJobFiles, ec2),
           edit_uint64_with_commas(jcr->jr.JobBytes, ec3),
           edit_uint64_with_suffix(jcr->jr.JobBytes, ec4),
           edit_uint64_with_commas(jcr->SDJobBytes, ec5),
           edit_uint64_with_suffix(jcr->SDJobBytes, ec6));
      break;
   default:
      if (jcr->is_JobLevel(L_VIRTUAL_FULL)) {
         Mmsg(level_info, _(
              "  Backup Level:           Virtual Full\n"));
         Mmsg(statistics, _(
              "  SD Files Written:       %s\n"
              "  SD Bytes Written:       %s (%sB)\n"),
              edit_uint64_with_commas(jcr->SDJobFiles, ec2),
              edit_uint64_with_commas(jcr->SDJobBytes, ec5),
              edit_uint64_with_suffix(jcr->SDJobBytes, ec6));
      } else {
         Mmsg(level_info, _(
              "  Backup Level:           %s%s\n"),
              level_to_str(jcr->getJobLevel()), jcr->since);
         Mmsg(statistics, _(
              "  FD Files Written:       %s\n"
              "  SD Files Written:       %s\n"
              "  FD Bytes Written:       %s (%sB)\n"
              "  SD Bytes Written:       %s (%sB)\n"),
              edit_uint64_with_commas(jcr->jr.JobFiles, ec1),
              edit_uint64_with_commas(jcr->SDJobFiles, ec2),
              edit_uint64_with_commas(jcr->jr.JobBytes, ec3),
              edit_uint64_with_suffix(jcr->jr.JobBytes, ec4),
              edit_uint64_with_commas(jcr->SDJobBytes, ec5),
              edit_uint64_with_suffix(jcr->SDJobBytes, ec6));
      }
      break;
   }

   if (jcr->HasQuota) {
      if (jcr->res.client->GraceTime != 0) {
         bstrftimes(gdt, sizeof(gdt), jcr->res.client->GraceTime +
                                      jcr->res.client->SoftQuotaGracePeriod);
      } else {
         bstrncpy(gdt, "Soft Quota not exceeded", sizeof(gdt));
      }
      Mmsg(quota_info, _(
           "  Quota Used:             %s (%sB)\n"
           "  Burst Quota:            %s (%sB)\n"
           "  Soft Quota:             %s (%sB)\n"
           "  Hard Quota:             %s (%sB)\n"
           "  Grace Expiry Date:      %s\n"),
           edit_uint64_with_commas(jcr->jr.JobSumTotalBytes+jcr->SDJobBytes, ec1),
           edit_uint64_with_suffix(jcr->jr.JobSumTotalBytes+jcr->SDJobBytes, ec2),
           edit_uint64_with_commas(jcr->res.client->QuotaLimit, ec3),
           edit_uint64_with_suffix(jcr->res.client->QuotaLimit, ec4),
           edit_uint64_with_commas(jcr->res.client->SoftQuota, ec5),
           edit_uint64_with_suffix(jcr->res.client->SoftQuota, ec6),
           edit_uint64_with_commas(jcr->res.client->HardQuota, ec7),
           edit_uint64_with_suffix(jcr->res.client->HardQuota, ec8),
           gdt);
   }

   switch (jcr->getJobProtocol()) {
   case PT_NDMP:
      break;
   default:
      if (jcr->is_JobLevel(L_VIRTUAL_FULL)) {
         Mmsg(daemon_status, _(
              "  SD Errors:              %d\n"
              "  SD termination status:  %s\n"
              "  Accurate:               %s\n"),
           jcr->SDErrors,
           sd_term_msg,
           jcr->accurate ? _("yes") : _("no"));
      } else {
         if (jcr->HasBase) {
            Mmsg(client_options, _(
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
                 jcr->VSS ? _("yes") : _("no"),
                 jcr->Encrypt ? _("yes") : _("no"),
                 jcr->accurate ? _("yes") : _("no"));
         } else {
            Mmsg(client_options, _(
                 "  Software Compression:   %s%s\n"
                 "  VSS:                    %s\n"
                 "  Encryption:             %s\n"
                 "  Accurate:               %s\n"),
                 compress,
                 compress_algo_list.c_str(),
                 jcr->VSS ? _("yes") : _("no"),
                 jcr->Encrypt ? _("yes") : _("no"),
                 jcr->accurate ? _("yes") : _("no"));
         }

         Mmsg(daemon_status, _(
              "  Non-fatal FD errors:    %d\n"
              "  SD Errors:              %d\n"
              "  FD termination status:  %s\n"
              "  SD termination status:  %s\n"),
           jcr->JobErrors,
           jcr->SDErrors,
           fd_term_msg,
           sd_term_msg);

         if (me->secure_erase_cmdline) {
            Mmsg(temp,"  Dir Secure Erase Cmd:   %s\n", me->secure_erase_cmdline);
            pm_strcat(secure_erase_status, temp.c_str());
         }
         if (!bstrcmp(jcr->FDSecureEraseCmd, "*None*")) {
            Mmsg(temp, "  FD  Secure Erase Cmd:   %s\n", jcr->FDSecureEraseCmd);
            pm_strcat(secure_erase_status, temp.c_str());
         }
         if (!bstrcmp(jcr->SDSecureEraseCmd, "*None*")) {
            Mmsg(temp, "  SD  Secure Erase Cmd:   %s\n", jcr->SDSecureEraseCmd);
            pm_strcat(secure_erase_status, temp.c_str());
         }
      }
      break;
   }

// bmicrosleep(15, 0);                /* for debugging SIGHUP */

   Jmsg(jcr, msg_type, 0, _("%s %s %s (%s):\n"
        "  Build OS:               %s %s %s\n"
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
        "  Termination:            %s\n\n"),
        BAREOS, my_name, VERSION, LSMDATE,
        HOST_OS, DISTNAME, DISTVER,
        jcr->jr.JobId,
        jcr->jr.Job,
        level_info.c_str(),
        jcr->res.client->name(), cr->Uname,
        jcr->res.fileset->name(), jcr->FSCreateTime,
        jcr->res.pool->name(), jcr->res.pool_source,
        jcr->res.catalog->name(), jcr->res.catalog_source,
        jcr->res.wstore->name(), jcr->res.wstore_source,
        schedt,
        sdt,
        edt,
        edit_utime(RunTime, elapsed, sizeof(elapsed)),
        jcr->JobPriority,
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
        term_msg);
}
