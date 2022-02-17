/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2012 Free Software Foundation Europe e.V.
   Copyright (C) 2016-2022 Bareos GmbH & Co. KG

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
// Kern Sibbald, May MM
/**
 * @file
 * Append code for Storage daemon
 */

#include "stored/append.h"
#include "stored/stored.h"
#include "stored/acquire.h"
#include "stored/fd_cmds.h"
#include "stored/stored_globals.h"
#include "stored/jcr_private.h"
#include "stored/label.h"
#include "stored/spool.h"
#include "lib/bget_msg.h"
#include "lib/edit.h"
#include "include/jcr.h"
#include "lib/berrno.h"
#include "lib/berrno.h"

namespace storagedaemon {

/* Responses sent to the daemon */
static char OK_data[] = "3000 OK data\n";
static char OK_append[] = "3000 OK append data\n";
static char OK_replicate[] = "3000 OK replicate data\n";

/* Forward referenced functions */

void PossibleIncompleteJob(JobControlRecord* jcr, int32_t last_file_index) {}

FileData::~FileData()
{
  for (auto devicerecord : device_records_) { FreeMemory(devicerecord.data); }
}

FileData::FileData(const FileData& other) : fileindex_(other.fileindex_)
{
  for (auto device : other.device_records_) { this->AddDeviceRecord(&device); }
}

FileData& FileData::operator=(const FileData& other)
{
  this->fileindex_ = other.fileindex_;
  for (auto device : other.device_records_) { this->AddDeviceRecord(&device); }

  return *this;
}

void FileData::SendAttributesToDirector(JobControlRecord* jcr)
{
  for (auto devicerecord : device_records_) {
    SendAttrsToDir(jcr, &devicerecord);
  }
}
void FileData::Initialize(int32_t index)
{
  fileindex_ = index;
  for (auto devicerecord : device_records_) { FreeMemory(devicerecord.data); }
  device_records_.clear();
}
void FileData::AddDeviceRecord(DeviceRecord* record)
{
  DeviceRecord devicerecord_copy{*record};

  // get a copy of the data rather than a pointer to the previous data
  devicerecord_copy.data = GetMemory(record->data_len);
  PmMemcpy(devicerecord_copy.data, record->data, record->data_len);
  device_records_.push_back(devicerecord_copy);
}

static void UpdateFileList(JobControlRecord* jcr)
{
  Dmsg0(100, _("... update file list\n"));
  jcr->impl->dcr->DirAskToUpdateFileList(jcr);
}

static void UpdateJobmediaRecord(JobControlRecord* jcr)
{
  Dmsg0(100, _("... create job media record\n"));
  jcr->impl->dcr->DirCreateJobmediaRecord(false);
}

static void UpdateJobrecord(JobControlRecord* jcr)
{
  Dmsg2(100, _("... update job record: %llu bytes %lu files\n"), jcr->JobBytes,
        jcr->JobFiles);
  jcr->impl->dcr->DirAskToUpdateJobRecord(jcr);
}

void DoBackupCheckpoint(JobControlRecord* jcr)
{
  Jmsg(jcr, M_INFO, 0,
       _("Checkpoint: Syncing current backup status to catalog\n"));
  UpdateJobrecord(jcr);
  UpdateFileList(jcr);
  UpdateJobmediaRecord(jcr);
  Jmsg(jcr, M_INFO, 0, _("Checkpoint completed\n"));
}

static time_t DoTimedCheckpoint(JobControlRecord* jcr,
                                time_t checkpoint_time,
                                time_t checkpoint_interval)
{
  time_t now
      = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
  if (now > checkpoint_time) {
    checkpoint_time = now + checkpoint_interval;
    Jmsg(jcr, M_INFO, 0,
         _("Doing timed backup checkpoint. Next checkpoint in %d seconds\n"),
         checkpoint_interval);
    DoBackupCheckpoint(jcr);
  }

  return checkpoint_time;
}

static void SaveFullyProcessedFiles(JobControlRecord* jcr,
                                    std::vector<FileData>& processed_files)
{
  if (!processed_files.empty()) {
    for (auto file : processed_files) { file.SendAttributesToDirector(jcr); }
    jcr->JobFiles = processed_files.back().GetFileIndex();
    processed_files.clear();
  }
}

// Append Data sent from File daemon
bool DoAppendData(JobControlRecord* jcr, BareosSocket* bs, const char* what)
{
  int32_t n, file_index, stream, last_file_index, job_elapsed;
  bool ok = true;
  char buf1[100];
  Device* dev;
  POOLMEM* rec_data;
  char ec[50];

  if (!jcr->impl->dcr) {
    Jmsg0(jcr, M_FATAL, 0, _("DeviceControlRecord is NULL!!!\n"));
    return false;
  }
  dev = jcr->impl->dcr->dev;
  if (!dev) {
    Jmsg0(jcr, M_FATAL, 0, _("Device is NULL!!!\n"));
    return false;
  }

  Dmsg1(100, "Start append data. res=%d\n", dev->NumReserved());

  if (!bs->SetBufferSize(
          jcr->impl->dcr->device_resource->max_network_buffer_size,
          BNET_SETBUF_WRITE)) {
    Jmsg0(jcr, M_FATAL, 0, _("Unable to set network buffer size.\n"));
    jcr->setJobStatus(JS_ErrorTerminated);
    return false;
  }

  if (!AcquireDeviceForAppend(jcr->impl->dcr)) {
    jcr->setJobStatus(JS_ErrorTerminated);
    return false;
  }

  if (GeneratePluginEvent(jcr, bSdEventSetupRecordTranslation, jcr->impl->dcr)
      != bRC_OK) {
    jcr->setJobStatus(JS_ErrorTerminated);
    return false;
  }

  jcr->sendJobStatus(JS_Running);

  if (dev->VolCatInfo.VolCatName[0] == 0) {
    Pmsg0(000, _("NULL Volume name. This shouldn't happen!!!\n"));
  }
  Dmsg1(50, "Begin append device=%s\n", dev->print_name());

  if (!BeginDataSpool(jcr->impl->dcr)) {
    jcr->setJobStatus(JS_ErrorTerminated);
    return false;
  }

  if (!BeginAttributeSpool(jcr)) {
    DiscardDataSpool(jcr->impl->dcr);
    jcr->setJobStatus(JS_ErrorTerminated);
    return false;
  }

  Dmsg0(100, "Just after AcquireDeviceForAppend\n");
  if (dev->VolCatInfo.VolCatName[0] == 0) {
    Pmsg0(000, _("NULL Volume name. This shouldn't happen!!!\n"));
  }

  // Write Begin Session Record
  if (!WriteSessionLabel(jcr->impl->dcr, SOS_LABEL)) {
    Jmsg1(jcr, M_FATAL, 0, _("Write session label failed. ERR=%s\n"),
          dev->bstrerror());
    jcr->setJobStatus(JS_ErrorTerminated);
    ok = false;
  }
  if (dev->VolCatInfo.VolCatName[0] == 0) {
    Pmsg0(000, _("NULL Volume name. This shouldn't happen!!!\n"));
  }

  // Tell daemon to send data
  if (!bs->fsend(OK_data)) {
    BErrNo be;
    Jmsg2(jcr, M_FATAL, 0, _("Network send error to %s. ERR=%s\n"), what,
          be.bstrerror(bs->b_errno));
    ok = false;
  }

  /*
   * Get Data from daemon, write to device.  To clarify what is
   * going on here.  We expect:
   * - A stream header
   * - Multiple records of data
   * - EOD record
   *
   * The Stream header is just used to synchronize things, and
   * none of the stream header is written to tape.
   * The Multiple records of data, contain first the Attributes,
   * then after another stream header, the file data, then
   * after another stream header, the MD5 data if any.
   *
   * So we get the (stream header, data, EOD) three time for each
   * file. 1. for the Attributes, 2. for the file data if any,
   * and 3. for the MD5 if any.
   */
  jcr->impl->dcr->VolFirstIndex = jcr->impl->dcr->VolLastIndex = 0;
  jcr->run_time = time(NULL); /* start counting time for rates */

  auto now
      = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
  time_t checkpointinterval = me->checkpoint_interval;
  time_t next_checkpoint_time = now + checkpointinterval;

  std::vector<FileData> processed_files{};
  int64_t current_volumeid = jcr->impl->dcr->VolMediaId;

  FileData file_currently_processed;
  uint32_t current_block_number = jcr->impl->dcr->block->BlockNumber;

  for (last_file_index = 0; ok && !jcr->IsJobCanceled();) {
    /*
     * Read Stream header from the daemon.
     *
     * The stream header consists of the following:
     * - file_index (sequential Bareos file index, base 1)
     * - stream     (Bareos number to distinguish parts of data)
     * - info       (Info for Storage daemon -- compressed, encrypted, ...)
     *               info is not currently used, so is read, but ignored!
     */
    if ((n = BgetMsg(bs)) <= 0) {
      if (n == BNET_SIGNAL && bs->message_length == BNET_EOD) {
        break; /* end of data */
      }
      Jmsg2(jcr, M_FATAL, 0, _("Error reading data header from %s. ERR=%s\n"),
            what, bs->bstrerror());
      PossibleIncompleteJob(jcr, last_file_index);
      ok = false;
      break;
    }

    if (sscanf(bs->msg, "%ld %ld", &file_index, &stream) != 2) {
      Jmsg2(jcr, M_FATAL, 0, _("Malformed data header from %s: %s\n"), what,
            bs->msg);
      ok = false;
      PossibleIncompleteJob(jcr, last_file_index);
      break;
    }

    Dmsg2(890, "<filed: Header FilInx=%d stream=%d\n", file_index, stream);

    /*
     * We make sure the file_index is advancing sequentially.
     * An incomplete job can start the file_index at any number.
     * otherwise, it must start at 1.
     */
    if (!(jcr->rerunning && file_index > 0 && last_file_index == 0)
        && !(file_index > 0
             && (file_index == last_file_index
                 || file_index == last_file_index + 1))) {
      Jmsg3(jcr, M_FATAL, 0, _("FI=%d from %s not positive or sequential=%d\n"),
            file_index, what, last_file_index);
      PossibleIncompleteJob(jcr, last_file_index);
      ok = false;
      break;
    }

    if (file_index != last_file_index) {
      last_file_index = file_index;
      if (file_currently_processed.GetFileIndex() > 0) {
        processed_files.push_back(file_currently_processed);
      }
      file_currently_processed.Initialize(file_index);
    }

    /*
     * Read data stream from the daemon. The data stream is just raw bytes.
     * We save the original data pointer from the record so we can restore
     * that after the loop ends.
     */
    rec_data = jcr->impl->dcr->rec->data;
    while ((n = BgetMsg(bs)) > 0 && !jcr->IsJobCanceled()) {
      jcr->impl->dcr->rec->VolSessionId = jcr->VolSessionId;
      jcr->impl->dcr->rec->VolSessionTime = jcr->VolSessionTime;
      jcr->impl->dcr->rec->FileIndex = file_index;
      jcr->impl->dcr->rec->Stream = stream;
      jcr->impl->dcr->rec->maskedStream
          = stream & STREAMMASK_TYPE; /* strip high bits */
      jcr->impl->dcr->rec->data_len = bs->message_length;
      jcr->impl->dcr->rec->data = bs->msg; /* use message buffer */

      Dmsg4(850, "before writ_rec FI=%d SessId=%d Strm=%s len=%d\n",
            jcr->impl->dcr->rec->FileIndex, jcr->impl->dcr->rec->VolSessionId,
            stream_to_ascii(buf1, jcr->impl->dcr->rec->Stream,
                            jcr->impl->dcr->rec->FileIndex),
            jcr->impl->dcr->rec->data_len);

      ok = jcr->impl->dcr->WriteRecord();
      if (!ok) {
        Dmsg2(90, "Got WriteBlockToDev error on device %s. %s\n",
              jcr->impl->dcr->dev->print_name(),
              jcr->impl->dcr->dev->bstrerror());
        break;
      }

      if (current_block_number != jcr->impl->dcr->block->BlockNumber) {
        current_block_number = jcr->impl->dcr->block->BlockNumber;
        SaveFullyProcessedFiles(jcr, processed_files);
      }

      file_currently_processed.AddDeviceRecord(jcr->impl->dcr->rec);

      if (jcr->impl->dcr->VolMediaId != current_volumeid) {
        Jmsg0(jcr, M_INFO, 0, _("Volume changed, doing checkpoint:\n"));
        UpdateFileList(jcr);
        UpdateJobrecord(jcr);
        current_volumeid = jcr->impl->dcr->VolMediaId;
      } else if (checkpointinterval) {
        next_checkpoint_time
            = DoTimedCheckpoint(jcr, next_checkpoint_time, checkpointinterval);
      }

      Dmsg0(650, "Enter bnet_get\n");
    }
    Dmsg2(650, "End read loop with %s. Stat=%d\n", what, n);

    // Restore the original data pointer.
    jcr->impl->dcr->rec->data = rec_data;

    if (bs->IsError()) {
      if (!jcr->IsJobCanceled()) {
        Dmsg2(350, "Network read error from %s. ERR=%s\n", what,
              bs->bstrerror());
        Jmsg2(jcr, M_FATAL, 0, _("Network error reading from %s. ERR=%s\n"),
              what, bs->bstrerror());
        PossibleIncompleteJob(jcr, last_file_index);
      }
      ok = false;
      break;
    }
  }

  // Create Job status for end of session label
  jcr->setJobStatus(ok ? JS_Terminated : JS_ErrorTerminated);

  if (ok && bs == jcr->file_bsock) {
    // Terminate connection with FD
    bs->fsend(OK_append);
    DoFdCommands(jcr); /* finish dialog with FD */
  } else if (bs == jcr->store_bsock) {
    bs->fsend(OK_replicate);
  } else {
    bs->fsend("3999 Failed append\n");
  }

  Dmsg1(200, "Write EOS label JobStatus=%c\n", jcr->JobStatus);

  /*
   * Check if we can still write. This may not be the case
   * if we are at the end of the tape or we got a fatal I/O error.
   */
  if (ok || dev->CanWrite()) {
    if (!WriteSessionLabel(jcr->impl->dcr, EOS_LABEL)) {
      // Print only if ok and not cancelled to avoid spurious messages
      if (ok && !jcr->IsJobCanceled()) {
        Jmsg1(jcr, M_FATAL, 0, _("Error writing end session label. ERR=%s\n"),
              dev->bstrerror());
        PossibleIncompleteJob(jcr, last_file_index);
      }
      jcr->setJobStatus(JS_ErrorTerminated);
      ok = false;
    }
    Dmsg0(90, "back from write_end_session_label()\n");

    // Flush out final partial block of this session
    if (!jcr->impl->dcr->WriteBlockToDevice()) {
      // Print only if ok and not cancelled to avoid spurious messages
      if (ok && !jcr->IsJobCanceled()) {
        Jmsg2(jcr, M_FATAL, 0, _("Fatal append error on device %s: ERR=%s\n"),
              dev->print_name(), dev->bstrerror());
        Dmsg0(100, _("Set ok=FALSE after WriteBlockToDevice.\n"));
        PossibleIncompleteJob(jcr, last_file_index);
      }
      jcr->setJobStatus(JS_ErrorTerminated);
      ok = false;
    } else {
      // Send attributes of the final partial block of the session
      processed_files.push_back(file_currently_processed);
      SaveFullyProcessedFiles(jcr, processed_files);
    }
  }

  if (!ok && !jcr->is_JobStatus(JS_Incomplete)) {
    DiscardDataSpool(jcr->impl->dcr);
  } else {
    // Note: if commit is OK, the device will remain blocked
    CommitDataSpool(jcr->impl->dcr);
  }

  // Release the device -- and send final Vol info to DIR and unlock it.
  ReleaseDevice(jcr->impl->dcr);

  /*
   * Don't use time_t for job_elapsed as time_t can be 32 or 64 bits,
   * and the subsequent Jmsg() editing will break
   */
  job_elapsed = time(NULL) - jcr->run_time;
  if (job_elapsed <= 0) { job_elapsed = 1; }

  Jmsg(jcr, M_INFO, 0,
       _("Elapsed time=%02d:%02d:%02d, Transfer rate=%s Bytes/second\n"),
       job_elapsed / 3600, job_elapsed % 3600 / 60, job_elapsed % 60,
       edit_uint64_with_suffix(jcr->JobBytes / job_elapsed, ec));

  if ((!ok || jcr->IsJobCanceled()) && !jcr->is_JobStatus(JS_Incomplete)) {
    DiscardAttributeSpool(jcr);
  } else {
    CommitAttributeSpool(jcr);
  }

  jcr->sendJobStatus(); /* update director */

  Dmsg1(100, "return from DoAppendData() ok=%d\n", ok);
  return ok;
}

// Send attributes and digest to Director for Catalog
bool SendAttrsToDir(JobControlRecord* jcr, DeviceRecord* rec)
{
  if (rec->maskedStream == STREAM_UNIX_ATTRIBUTES
      || rec->maskedStream == STREAM_UNIX_ATTRIBUTES_EX
      || rec->maskedStream == STREAM_RESTORE_OBJECT
      || CryptoDigestStreamType(rec->maskedStream) != CRYPTO_DIGEST_NONE) {
    if (!jcr->impl->no_attributes) {
      BareosSocket* dir = jcr->dir_bsock;
      if (AreAttributesSpooled(jcr)) { dir->SetSpooling(); }
      Dmsg0(850, "Send attributes to dir.\n");
      if (!jcr->impl->dcr->DirUpdateFileAttributes(rec)) {
        Jmsg(jcr, M_FATAL, 0, _("Error updating file attributes. ERR=%s\n"),
             dir->bstrerror());
        dir->ClearSpooling();
        return false;
      }
      dir->ClearSpooling();
    }
  }
  return true;
}
} /* namespace storagedaemon */
