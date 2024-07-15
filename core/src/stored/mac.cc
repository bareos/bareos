/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2006-2012 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2024 Bareos GmbH & Co. KG

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
// Kern Sibbald, January MMVI
/**
 * @file
 * responsible for doing
 * migration, archive, copy, and virtual backup jobs.
 */

#include "include/bareos.h"
#include "stored/stored.h"
#include "stored/stored_globals.h"
#include "stored/acquire.h"
#include "stored/bsr.h"
#include "stored/append.h"
#include "stored/device.h"
#include "stored/device_control_record.h"
#include "stored/stored_jcr_impl.h"
#include "stored/label.h"
#include "stored/mount.h"
#include "stored/read_record.h"
#include "stored/sd_stats.h"
#include "stored/spool.h"
#include "lib/bget_msg.h"
#include "lib/bnet.h"
#include "lib/bsock.h"
#include "lib/edit.h"
#include "include/jcr.h"

namespace storagedaemon {

// Responses sent to the Director
static char Job_end[]
    = "3099 Job %s end JobStatus=%d JobFiles=%d JobBytes=%s JobErrors=%u\n";

// Responses received from Storage Daemon
static char OK_start_replicate[] = "3000 OK start replicate ticket = %d\n";
static char OK_data[] = "3000 OK data\n";
static char OK_replicate[] = "3000 OK replicate data\n";
static char OK_end_replicate[] = "3000 OK end replicate\n";

// Commands sent to Storage Daemon
static char start_replicate[] = "start replicate\n";
static char ReplicateData[] = "replicate data %d\n";
static char end_replicate[] = "end replicate\n";


/* last callback information of our job */
struct cb_data {
  bool found_first_sos_label{false};
  uint32_t last_VolSessionId{0};
  uint32_t last_VolSessionTime{0};
  int32_t last_FileIndex{0};
  int32_t last_Stream{0};
};

/**
 * Get response from Storage daemon to a command we sent.
 * Check that the response is OK.
 *
 * Returns: false on failure
 *          true on success
 */
static bool response(JobControlRecord* jcr,
                     BareosSocket* sd,
                     char* resp,
                     const char* cmd)
{
  if (sd->errors) { return false; }
  if (BgetMsg(sd) > 0) {
    Dmsg1(110, "<stored: %s", sd->msg);
    if (bstrcmp(sd->msg, resp)) { return true; }
  }
  if (jcr->IsJobCanceled()) {
    return false; /* if canceled avoid useless error messages */
  }
  if (IsBnetError(sd)) {
    Jmsg2(jcr, M_FATAL, 0,
          T_("Comm error with SD. bad response to %s. ERR=%s\n"), cmd,
          BnetStrerror(sd));
  } else {
    Jmsg3(jcr, M_FATAL, 0,
          T_("Bad response to %s command. Wanted %s, got %s\n"), cmd, resp,
          sd->msg);
  }
  return false;
}

/**
 * Called here for each record from ReadRecords()
 * This function is used when we do a internal clone of a Job e.g.
 * this SD is both the reading and writing SD.
 *
 * Returns: true if OK
 *           false if error
 */
static bool CloneRecordInternally(DeviceControlRecord* dcr,
                                  DeviceRecord* rec,
                                  cb_data* data)
{
  bool retval = false;
  bool translated_record = false;
  JobControlRecord* jcr = dcr->jcr;
  Device* dev = jcr->sd_impl->dcr->dev;
  char buf1[100], buf2[100];

  /* If label, discard it as we create our SOS and EOS Labels
   * However, we still want the first Start Of Session label as that contains
   * the timestamps from the first original Job.
   *
   * We need to put this info in our own SOS and EOS Labels so that
   * bscan-ing of virtual and always incremental consolidated jobs
   * works. */

  if (rec->FileIndex < 0) {
    if (rec->FileIndex == SOS_LABEL) {
      if (!data->found_first_sos_label) {
        Dmsg0(200, "Found first SOS_LABEL and adopting job info\n");
        data->found_first_sos_label = true;

        Session_Label sos_label;

        UnserSessionLabel(&sos_label, rec);

        if (jcr->is_JobType(JT_MIGRATE) || jcr->is_JobType(JT_COPY)) {
          bstrncpy(jcr->Job, sos_label.Job, sizeof(jcr->Job));
          PmStrcpy(jcr->sd_impl->job_name, sos_label.JobName);
          PmStrcpy(jcr->client_name, sos_label.ClientName);
          PmStrcpy(jcr->sd_impl->fileset_name, sos_label.FileSetName);
          PmStrcpy(jcr->sd_impl->fileset_md5, sos_label.FileSetMD5);
        }
        jcr->setJobType(sos_label.JobType);
        jcr->setJobLevel(sos_label.JobLevel);
        Dmsg1(200, "joblevel from SOS_LABEL is now %c\n", sos_label.JobLevel);

        // make sure this volume wasn't written by bacula 1.26 or earlier
        ASSERT(sos_label.VerNum >= 11);
        jcr->sched_time = BtimeToUnix(sos_label.write_btime);

        jcr->start_time = jcr->sched_time;

        /* write the SOS Label with the existing timestamp infos */
        if (!WriteSessionLabel(jcr->sd_impl->dcr, SOS_LABEL)) {
          Jmsg1(jcr, M_FATAL, 0, T_("Write session label failed. ERR=%s\n"),
                dev->bstrerror());
          jcr->setJobStatusWithPriorityCheck(JS_ErrorTerminated);
          retval = false;
          goto bail_out;
        }
      } else {
        Dmsg0(200, "Found additional SOS_LABEL, ignoring! \n");
      }
    }
    /* Other label than SOS -> skip */
    retval = true;
    goto bail_out;
  }

  /* For normal migration jobs, FileIndex values are sequential because
   *  we are dealing with one job.  However, for Vbackup (consolidation),
   *  we will be getting records from multiple jobs and writing them back
   *  out, so we need to ensure that the output FileIndex is sequential.
   *  We do so by detecting a FileIndex change and incrementing the
   *  JobFiles, which we then use as the output FileIndex. */
  if (rec->FileIndex >= 0) {
    // If something changed, increment FileIndex
    if (rec->VolSessionId != data->last_VolSessionId
        || rec->VolSessionTime != data->last_VolSessionTime
        || rec->FileIndex != data->last_FileIndex) {
      jcr->JobFiles++;
      data->last_VolSessionId = rec->VolSessionId;
      data->last_VolSessionTime = rec->VolSessionTime;
      data->last_FileIndex = rec->FileIndex;
    }
    rec->FileIndex = jcr->JobFiles; /* set sequential output FileIndex */
  }


  // Modify record SessionId and SessionTime to correspond to output.
  rec->VolSessionId = jcr->VolSessionId;
  rec->VolSessionTime = jcr->VolSessionTime;

  Dmsg5(200, "before write JobId=%d FI=%s SessId=%d Strm=%s len=%d\n",
        jcr->JobId, FI_to_ascii(buf1, rec->FileIndex), rec->VolSessionId,
        stream_to_ascii(buf2, rec->Stream, rec->FileIndex), rec->data_len);

  // Perform record translations.
  jcr->sd_impl->dcr->before_rec = rec;
  jcr->sd_impl->dcr->after_rec = NULL;
  if (GeneratePluginEvent(jcr, bSdEventWriteRecordTranslation,
                          jcr->sd_impl->dcr)
      != bRC_OK) {
    goto bail_out;
  }

  /* The record got translated when we got an after_rec pointer after calling
   * the bSdEventWriteRecordTranslation plugin event. If no translation has
   * taken place we just point the after_rec pointer to same DeviceRecord as in
   * the before_rec pointer. */
  if (!jcr->sd_impl->dcr->after_rec) {
    jcr->sd_impl->dcr->after_rec = jcr->sd_impl->dcr->before_rec;
  } else {
    translated_record = true;
  }

  while (!WriteRecordToBlock(jcr->sd_impl->dcr, jcr->sd_impl->dcr->after_rec)) {
    Dmsg4(200, "!WriteRecordToBlock blkpos=%u:%u len=%d rem=%d\n", dev->file,
          dev->block_num, jcr->sd_impl->dcr->after_rec->data_len,
          jcr->sd_impl->dcr->after_rec->remainder);
    if (!jcr->sd_impl->dcr->WriteBlockToDevice()) {
      Dmsg2(90, "Got WriteBlockToDev error on device %s. %s\n",
            dev->print_name(), dev->bstrerror());
      Jmsg2(jcr, M_FATAL, 0, T_("Fatal append error on device %s: ERR=%s\n"),
            dev->print_name(), dev->bstrerror());
      goto bail_out;
    }
    Dmsg2(200, "===== Wrote block new pos %u:%u\n", dev->file, dev->block_num);
  }

  if (rec->FileIndex < 0) {
    retval = true; /* don't send LABELs to Dir */
    goto bail_out;
  }

  jcr->JobBytes += jcr->sd_impl->dcr->after_rec
                       ->data_len; /* increment bytes of this job */

  Dmsg5(500, "wrote_record JobId=%d FI=%s SessId=%d Strm=%s len=%d\n",
        jcr->JobId, FI_to_ascii(buf1, jcr->sd_impl->dcr->after_rec->FileIndex),
        jcr->sd_impl->dcr->after_rec->VolSessionId,
        stream_to_ascii(buf2, jcr->sd_impl->dcr->after_rec->Stream,
                        jcr->sd_impl->dcr->after_rec->FileIndex),
        jcr->sd_impl->dcr->after_rec->data_len);

  if (IsAttribute(jcr->sd_impl->dcr->after_rec)) {
    SendAttrsToDir(jcr, jcr->sd_impl->dcr->after_rec);
  }

  retval = true;

bail_out:

  /* Restore packet -- the read record function uses this information
   * to check if the job changed, so we need to restore it to how it was;
   * otherwise the record will get zeroed on job change. */
  rec->VolSessionId = data->last_VolSessionId;
  rec->VolSessionTime = data->last_VolSessionTime;

  if (translated_record) {
    FreeRecord(jcr->sd_impl->dcr->after_rec);
    jcr->sd_impl->dcr->after_rec = NULL;
  }

  return retval;
}

/**
 * Called here for each record from ReadRecords()
 * This function is used when we do a external clone of a Job e.g.
 * this SD is the reading SD. And a remote SD is the writing SD.
 *
 * Returns: true if OK
 *           false if error
 */
static bool CloneRecordToRemoteSd(DeviceControlRecord* dcr,
                                  DeviceRecord* rec,
                                  cb_data* data)
{
  POOLMEM* msgsave;
  JobControlRecord* jcr = dcr->jcr;
  char buf1[100], buf2[100];
  BareosSocket* sd = jcr->store_bsock;
  bool send_eod, send_header;

  // If label discard it
  if (rec->FileIndex < 0) { return true; }

  // See if this is the first record being processed.
  if (data->last_FileIndex == 0) {
    /* Initialize the last counters so we can compare
     * things in the next run through here. */
    data->last_VolSessionId = rec->VolSessionId;
    data->last_VolSessionTime = rec->VolSessionTime;
    data->last_FileIndex = rec->FileIndex;
    data->last_Stream = rec->Stream;
    jcr->JobFiles = 1;

    // Need to send both a new header only.
    send_eod = false;
    send_header = true;
  } else {
    // See if we are changing file or stream type.
    if (rec->VolSessionId != data->last_VolSessionId
        || rec->VolSessionTime != data->last_VolSessionTime
        || rec->FileIndex != data->last_FileIndex
        || rec->Stream != data->last_Stream) {
      /* See if we are changing the FileIndex e.g.
       * start processing the next file in the backup stream. */
      if (rec->FileIndex != data->last_FileIndex) { jcr->JobFiles++; }

      // Keep track of the new state.
      data->last_VolSessionId = rec->VolSessionId;
      data->last_VolSessionTime = rec->VolSessionTime;
      data->last_FileIndex = rec->FileIndex;
      data->last_Stream = rec->Stream;

      // Need to send both a EOD and a new header.
      send_eod = true;
      send_header = true;
    } else {
      send_eod = false;
      send_header = false;
    }
  }

  // Send a EOD when needed.
  if (send_eod) {
    if (!sd->signal(BNET_EOD)) { /* indicate end of file data */
      if (!jcr->IsJobCanceled()) {
        Jmsg1(jcr, M_FATAL, 0, T_("Network send error to SD. ERR=%s\n"),
              sd->bstrerror());
      }
      return false;
    }
  }

  // Send a header when needed.
  if (send_header) {
    if (!sd->fsend("%ld %d 0", rec->FileIndex, rec->Stream)) {
      if (!jcr->IsJobCanceled()) {
        Jmsg1(jcr, M_FATAL, 0, T_("Network send error to SD. ERR=%s\n"),
              sd->bstrerror());
      }
      return false;
    }
  }

  /* Send the record data.
   * We don't want to copy the data from the record to the socket structure
   * so we save the original msg pointer and use the record data pointer for
   * sending and restore the original msg pointer when done. */
  msgsave = sd->msg;
  sd->msg = rec->data;
  sd->message_length = rec->data_len;

  if (!sd->send()) {
    sd->msg = msgsave;
    sd->message_length = 0;
    if (!jcr->IsJobCanceled()) {
      Jmsg1(jcr, M_FATAL, 0, T_("Network send error to SD. ERR=%s\n"),
            sd->bstrerror());
    }
    return false;
  }

  jcr->JobBytes += sd->message_length;
  sd->msg = msgsave;

  Dmsg5(200, "wrote_record JobId=%d FI=%s SessId=%d Strm=%s len=%d\n",
        jcr->JobId, FI_to_ascii(buf1, rec->FileIndex), rec->VolSessionId,
        stream_to_ascii(buf2, rec->Stream, rec->FileIndex), rec->data_len);

  return true;
}

// Check autoinflation/autodeflation settings.
static inline void CheckAutoXflation(JobControlRecord* jcr)
{
  /* See if the autoxflateonreplication flag is set to true then we allow
   * autodeflation and autoinflation to take place. */
  if (me->autoxflateonreplication) { return; }

  // Check autodeflation.
  switch (jcr->sd_impl->read_dcr->autodeflate) {
    case IODirection::READ:
    case IODirection::READ_WRITE:
      Dmsg0(200, "Clearing autodeflate on read_dcr\n");
      jcr->sd_impl->read_dcr->autodeflate = IODirection::NONE;
      break;
    default:
      break;
  }

  if (jcr->sd_impl->dcr) {
    switch (jcr->sd_impl->dcr->autodeflate) {
      case IODirection::WRITE:
      case IODirection::READ_WRITE:
        Dmsg0(200, "Clearing autodeflate on write dcr\n");
        jcr->sd_impl->dcr->autodeflate = IODirection::NONE;
        break;
      default:
        break;
    }
  }

  // Check autoinflation.
  switch (jcr->sd_impl->read_dcr->autoinflate) {
    case IODirection::READ:
    case IODirection::READ_WRITE:
      Dmsg0(200, "Clearing autoinflate on read_dcr\n");
      jcr->sd_impl->read_dcr->autoinflate = IODirection::NONE;
      break;
    default:
      break;
  }

  if (jcr->sd_impl->dcr) {
    switch (jcr->sd_impl->dcr->autoinflate) {
      case IODirection::WRITE:
      case IODirection::READ_WRITE:
        Dmsg0(200, "Clearing autoinflate on write dcr\n");
        jcr->sd_impl->dcr->autoinflate = IODirection::NONE;
        break;
      default:
        break;
    }
  }
}

// Read Data and commit to new job.
bool DoMacRun(JobControlRecord* jcr)
{
  utime_t now;
  char ec1[50];
  const char* Type;
  bool ok = true;
  bool acquire_fail = false;
  BareosSocket* dir = jcr->dir_bsock;
  if (!jcr->sd_impl->dcr) { TryReserveAfterUse(jcr, true); }
  Device* dev = jcr->sd_impl->dcr->dev;

  switch (jcr->getJobType()) {
    case JT_MIGRATE:
      Type = "Migration";
      break;
    case JT_ARCHIVE:
      Type = "Archive";
      break;
    case JT_COPY:
      Type = "Copy";
      break;
    case JT_BACKUP:
      Type = "Virtual Backup";
      break;
    default:
      Type = "Unknown";
      break;
  }

  Dmsg0(20, "Start read data.\n");

  if (jcr->sd_impl->NumReadVolumes == 0) {
    Jmsg(jcr, M_FATAL, 0, T_("No Volume names found for %s.\n"), Type);
    goto bail_out;
  }

  // Check autoinflation/autodeflation settings.
  CheckAutoXflation(jcr);

  // See if we perform both read and write or read only.
  if (jcr->sd_impl->remote_replicate) {
    BareosSocket* sd;

    if (!jcr->sd_impl->read_dcr) {
      Jmsg(jcr, M_FATAL, 0, T_("Read device not properly initialized.\n"));
      goto bail_out;
    }

    Dmsg1(100, "read_dcr=%p\n", jcr->sd_impl->read_dcr);
    Dmsg3(200, "Found %d volumes names for %s. First=%s\n",
          jcr->sd_impl->NumReadVolumes, Type,
          jcr->sd_impl->VolList->VolumeName);

    // Ready devices for reading.
    if (!AcquireDeviceForRead(jcr->sd_impl->read_dcr)) {
      ok = false;
      acquire_fail = true;
      goto bail_out;
    }

    Dmsg2(200, "===== After acquire pos %u:%u\n",
          jcr->sd_impl->read_dcr->dev->file,
          jcr->sd_impl->read_dcr->dev->block_num);

    // Let SD plugins setup the record translation on read dcr
    if (GeneratePluginEvent(jcr, bSdEventSetupRecordTranslation,
                            jcr->sd_impl->read_dcr)
        != bRC_OK) {
      Jmsg(jcr, M_FATAL, 0,
           T_("bSdEventSetupRecordTranslation call failed for read dcr!\n"));
      ok = false;
      goto bail_out;
    }

    jcr->sendJobStatus(JS_Running);

    // Set network buffering.
    sd = jcr->store_bsock;
    if (!sd->SetBufferSize(me->max_network_buffer_size, BNET_SETBUF_WRITE)) {
      Jmsg(jcr, M_FATAL, 0, T_("Cannot set buffer size SD->SD.\n"));
      ok = false;
      goto bail_out;
    }

    // Let the remote SD know we are about to start the replication.
    sd->fsend(start_replicate);
    Dmsg1(110, ">stored: %s", sd->msg);

    // Expect to receive back the Ticket number.
    if (BgetMsg(sd) >= 0) {
      Dmsg1(110, "<stored: %s", sd->msg);
      if (sscanf(sd->msg, OK_start_replicate, &jcr->sd_impl->Ticket) != 1) {
        Jmsg(jcr, M_FATAL, 0, T_("Bad response to start replicate: %s\n"),
             sd->msg);
        goto bail_out;
      }
      Dmsg1(110, "Got Ticket=%d\n", jcr->sd_impl->Ticket);
    } else {
      Jmsg(jcr, M_FATAL, 0,
           T_("Bad response from stored to start replicate command\n"));
      goto bail_out;
    }

    // Let the remote SD know we are now really going to send the data.
    sd->fsend(ReplicateData, jcr->sd_impl->Ticket);
    Dmsg1(110, ">stored: %s", sd->msg);

    // Expect to get response to the replicate data cmd from Storage daemon
    if (!response(jcr, sd, OK_data, "replicate data")) {
      ok = false;
      goto bail_out;
    }

    // Update the initial Job Statistics.
    now = (utime_t)time(NULL);
    UpdateJobStatistics(jcr, now);

    cb_data data{};
    // Read all data and send it to remote SD.
    ok = ReadRecords(jcr->sd_impl->read_dcr, CloneRecordToRemoteSd,
                     MountNextReadVolume, &data);

    /* Send the last EOD to close the last data transfer and a next EOD to
     * signal the remote we are done. */
    if (!sd->signal(BNET_EOD) || !sd->signal(BNET_EOD)) {
      if (!jcr->IsJobCanceled()) {
        Jmsg1(jcr, M_FATAL, 0, T_("Network send error to SD. ERR=%s\n"),
              sd->bstrerror());
      }
      goto bail_out;
    }

    // Expect to get response that the replicate data succeeded.
    if (!response(jcr, sd, OK_replicate, "replicate data")) {
      ok = false;
      goto bail_out;
    }

    // End replicate session.
    sd->fsend(end_replicate);
    Dmsg1(110, ">stored: %s", sd->msg);

    // Expect to get response to the end replicate cmd from Storage daemon
    if (!response(jcr, sd, OK_end_replicate, "end replicate")) {
      ok = false;
      goto bail_out;
    }

    /* Inform Storage daemon that we are done */
    sd->signal(BNET_TERMINATE);
  } else {
    if (!jcr->sd_impl->read_dcr) {
      Jmsg(jcr, M_FATAL, 0, T_("Read device not properly initialized.\n"));
      goto bail_out;
    }

    Dmsg2(100, "read_dcr=%p write_dcr=%p\n", jcr->sd_impl->read_dcr,
          jcr->sd_impl->dcr);
    Dmsg3(200, "Found %d volumes names for %s. First=%s\n",
          jcr->sd_impl->NumReadVolumes, Type,
          jcr->sd_impl->VolList->VolumeName);

    // Ready devices for reading and writing.
    if (!AcquireDeviceForAppend(jcr->sd_impl->dcr)
        || !AcquireDeviceForRead(jcr->sd_impl->read_dcr)) {
      ok = false;
      acquire_fail = true;
      goto bail_out;
    }

    Dmsg2(200, "===== After acquire pos %u:%u\n", jcr->sd_impl->dcr->dev->file,
          jcr->sd_impl->dcr->dev->block_num);

    // Let SD plugins setup the record translation on read dcr
    if (GeneratePluginEvent(jcr, bSdEventSetupRecordTranslation,
                            jcr->sd_impl->read_dcr)
        != bRC_OK) {
      Jmsg(jcr, M_FATAL, 0,
           T_("bSdEventSetupRecordTranslation call failed for read dcr!\n"));
      ok = false;
      goto bail_out;
    }

    // Let SD plugins setup the record translation on write dcr
    if (GeneratePluginEvent(jcr, bSdEventSetupRecordTranslation,
                            jcr->sd_impl->dcr)
        != bRC_OK) {
      Jmsg(jcr, M_FATAL, 0,
           T_("bSdEventSetupRecordTranslation call failed for write dcr!\n"));
      ok = false;
      goto bail_out;
    }

    jcr->sendJobStatus(JS_Running);

    // Update the initial Job Statistics.
    now = (utime_t)time(NULL);
    UpdateJobStatistics(jcr, now);

    if (!BeginDataSpool(jcr->sd_impl->dcr)) {
      ok = false;
      goto bail_out;
    }

    if (!BeginAttributeSpool(jcr)) {
      ok = false;
      goto bail_out;
    }

    jcr->sd_impl->dcr->VolFirstIndex = jcr->sd_impl->dcr->VolLastIndex = 0;
    jcr->run_time = time(NULL);
    SetStartVolPosition(jcr->sd_impl->dcr);
    jcr->JobFiles = 0;

    cb_data data{};
    // Read all data and make a local clone of it.
    ok = ReadRecords(jcr->sd_impl->read_dcr, CloneRecordInternally,
                     MountNextReadVolume, &data);
  }

bail_out:
  if (!ok) { jcr->setJobStatusWithPriorityCheck(JS_ErrorTerminated); }

  if (!acquire_fail && !jcr->sd_impl->remote_replicate && jcr->sd_impl->dcr) {
    /* Don't use time_t for job_elapsed as time_t can be 32 or 64 bits,
     *   and the subsequent Jmsg() editing will break */
    int32_t job_elapsed;

    Dmsg1(100, "ok=%d\n", ok);

    if (ok || dev->CanWrite()) {
      /*
         memorize current JobStatus and set to
         JS_Terminated to write into EOS_LABEL
       */
      char currentJobStatus = jcr->getJobStatus();
      jcr->setJobStatusWithPriorityCheck(JS_Terminated);

      // Write End Of Session Label
      DeviceControlRecord* dcr = jcr->sd_impl->dcr;
      if (!WriteSessionLabel(dcr, EOS_LABEL)) {
        // Print only if ok and not cancelled to avoid spurious messages

        if (ok && !jcr->IsJobCanceled()) {
          Jmsg1(jcr, M_FATAL, 0,
                T_("Error writing end session label. ERR=%s\n"),
                dev->bstrerror());
        }
        jcr->setJobStatusWithPriorityCheck(JS_ErrorTerminated);
        ok = false;
      } else {
        /* restore JobStatus */
        jcr->setJobStatusWithPriorityCheck(currentJobStatus);
      }
      // Flush out final partial block of this session
      if (!jcr->sd_impl->dcr->WriteBlockToDevice()) {
        Jmsg2(jcr, M_FATAL, 0, T_("Fatal append error on device %s: ERR=%s\n"),
              dev->print_name(), dev->bstrerror());
        Dmsg0(100, T_("Set ok=FALSE after WriteBlockToDevice.\n"));
        ok = false;
      }
      Dmsg2(200, "Flush block to device pos %u:%u\n", dev->file,
            dev->block_num);
    }


    if (!ok) {
      DiscardDataSpool(jcr->sd_impl->dcr);
    } else {
      // Note: if commit is OK, the device will remain blocked
      CommitDataSpool(jcr->sd_impl->dcr);
    }

    job_elapsed = time(NULL) - jcr->run_time;
    if (job_elapsed <= 0) { job_elapsed = 1; }

    Jmsg(jcr, M_INFO, 0,
         T_("Elapsed time=%02d:%02d:%02d, Transfer rate=%s Bytes/second\n"),
         job_elapsed / 3600, job_elapsed % 3600 / 60, job_elapsed % 60,
         edit_uint64_with_suffix(jcr->JobBytes / job_elapsed, ec1));

    // send final Vol info to DIR
    if (!ok || jcr->IsJobCanceled()) {
      DiscardAttributeSpool(jcr);
    } else {
      CommitAttributeSpool(jcr);
    }
  }

  if (!jcr->sd_impl->remote_replicate && jcr->sd_impl->dcr) {
    if (!ReleaseDevice(jcr->sd_impl->dcr)) { ok = false; }
  }
  if (jcr->sd_impl->read_dcr) {
    if (!ReleaseDevice(jcr->sd_impl->read_dcr)) { ok = false; }
  }

  jcr->sendJobStatus(); /* update director */

  Dmsg0(30, "Done reading.\n");
  jcr->end_time = time(NULL);
  DequeueMessages(jcr); /* send any queued messages */
  if (ok) { jcr->setJobStatusWithPriorityCheck(JS_Terminated); }

  GeneratePluginEvent(jcr, bSdEventJobEnd);
  dir->fsend(Job_end, jcr->Job, jcr->getJobStatus(), jcr->JobFiles,
             edit_uint64(jcr->JobBytes, ec1), jcr->JobErrors);
  Dmsg4(100, Job_end, jcr->Job, jcr->getJobStatus(), jcr->JobFiles, ec1);

  dir->signal(BNET_EOD); /* send EOD to Director daemon */
  FreePlugins(jcr);      /* release instantiated plugins */

  return false; /* Continue DIR session ? */
}

} /* namespace storagedaemon */
