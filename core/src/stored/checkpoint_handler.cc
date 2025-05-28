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

#include <chrono>

namespace storagedaemon {

CheckpointHandler::CheckpointHandler(time_t interval)
    : checkpoint_interval_(interval)
{
  auto now
      = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
  this->next_checkpoint_time_ = now + checkpoint_interval_;
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
  Dmsg2(100, T_("... update job record: %llu bytes %lu files\n"), jcr->JobBytes,
        jcr->JobFiles);
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
         T_("Doing timed backup checkpoint. Next checkpoint in %d seconds\n"),
         checkpoint_interval_);
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
}  // namespace storagedaemon
