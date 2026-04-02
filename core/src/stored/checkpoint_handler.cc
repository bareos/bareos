/*
   BAREOS® - Backup Archiving REcovery Open Sourced

Copyright (C) 2023-2025 Bareos GmbH & Co. KG

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

#include "checkpoint_handler.h"

#include "stored/stored_jcr_impl.h"
#include "stored/device_control_record.h"
#include "stored/askdir.h"
#include "stored/dev.h"

#include <chrono>

namespace storagedaemon {

CheckpointHandler::CheckpointHandler(time_t checkpoint_interval,
                                     time_t progress_interval)
    : checkpoint_interval_(checkpoint_interval)
    , progress_interval_(progress_interval)
{
  auto now
      = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
  this->next_checkpoint_time_ = now + checkpoint_interval_;
  this->next_progress_time_
      = (progress_interval_ > 0) ? now + progress_interval_ : 0;
  this->last_progress_sample_time_ = now;
}


void CheckpointHandler::UpdateFileList(JobControlRecord* jcr)
{
  Dmsg0(100, T_("... update file list\n"));
  jcr->sd_impl->dcr->DirAskToUpdateFileList();
}

void CheckpointHandler::UpdateJobmediaRecord(JobControlRecord* jcr)
{
  Dmsg0(100, T_("... create job media record\n"));
  jcr->sd_impl->dcr->DirCreateJobmediaRecord(false);

  jcr->sd_impl->dcr->VolFirstIndex = jcr->sd_impl->dcr->VolLastIndex;
  jcr->sd_impl->dcr->StartFile = jcr->sd_impl->dcr->EndFile;
  jcr->sd_impl->dcr->StartBlock = jcr->sd_impl->dcr->EndBlock + 1;
}

void CheckpointHandler::UpdateJobrecord(JobControlRecord* jcr)
{
  Dmsg2(100, T_("... update job record: %" PRIu64 " bytes %" PRIu32 " files\n"),
        jcr->JobBytes, jcr->JobFiles);
  jcr->sd_impl->dcr->DirAskToUpdateJobRecord();
}

void CheckpointHandler::DoBackupCheckpoint(JobControlRecord* jcr)
{
  Dmsg0(100, T_("Checkpoint: Syncing current backup status to catalog\n"));
  UpdateJobrecord(jcr);
  UpdateFileList(jcr);
  UpdateJobmediaRecord(jcr);

  ClearReadyForCheckpoint();

  Dmsg0(100, T_("Checkpoint completed\n"));
}

void CheckpointHandler::DoVolumeChangeBackupCheckpoint(JobControlRecord* jcr)
{
  Jmsg0(jcr, M_INFO, 0, T_("Volume changed, doing checkpoint:\n"));
  DoBackupCheckpoint(jcr);
}

void CheckpointHandler::DoTimedCheckpoint(JobControlRecord* jcr)
{
  const time_t now
      = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
  if (now > next_checkpoint_time_) {
    while (next_checkpoint_time_ <= now) {
      next_checkpoint_time_ += checkpoint_interval_;
    }
    Jmsg(jcr, M_INFO, 0,
         T_("Doing timed backup checkpoint. Next checkpoint in %lld seconds\n"),
         static_cast<long long>(checkpoint_interval_));
    DoBackupCheckpoint(jcr);
  }
}

void CheckpointHandler::ResetTimer()
{
  const time_t now
      = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

  next_checkpoint_time_ = now + checkpoint_interval_;

  Dmsg2(100, T_("Reset checkpoint time to %lld (now = %lld)"),
        static_cast<long long>(next_checkpoint_time_),
        static_cast<long long>(now));
}

void CheckpointHandler::SendProgressToDir(JobControlRecord* jcr)
{
  DirSendProgressUpdate(jcr);

  if (!jcr->sd_impl->dcr || !jcr->sd_impl->dcr->dev) { return; }

  Device* dev = jcr->sd_impl->dcr->dev;

  const time_t now
      = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
  const time_t elapsed = now - last_progress_sample_time_;

  uint64_t write_rate = 0;
  uint64_t read_rate = 0;
  if (elapsed > 0) {
    const uint64_t cur_write = dev->DevWriteBytes;
    const uint64_t cur_read  = dev->DevReadBytes;
    write_rate = (cur_write >= last_dev_write_bytes_)
                     ? (cur_write - last_dev_write_bytes_) / elapsed
                     : 0;
    read_rate  = (cur_read >= last_dev_read_bytes_)
                     ? (cur_read - last_dev_read_bytes_) / elapsed
                     : 0;
    last_dev_write_bytes_ = cur_write;
    last_dev_read_bytes_  = cur_read;
  }
  last_progress_sample_time_ = now;

  DirSendDeviceProgressUpdate(jcr, dev->print_name(), write_rate, read_rate,
                              dev->VolCatInfo.VolCatBytes, dev->spool_size);
}

void CheckpointHandler::DoTimedProgressUpdate(JobControlRecord* jcr)
{
  if (progress_interval_ <= 0) { return; }

  const time_t now
      = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
  if (now >= next_progress_time_) {
    while (next_progress_time_ <= now) {
      next_progress_time_ += progress_interval_;
    }
    SendProgressToDir(jcr);
  }
}

}  // namespace storagedaemon
