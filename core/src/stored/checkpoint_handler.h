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

#ifndef BAREOS_STORED_CHECKPOINT_HANDLER_H_
#define BAREOS_STORED_CHECKPOINT_HANDLER_H_

#include "include/jcr.h"

namespace storagedaemon {

class CheckpointHandler {
 public:
  CheckpointHandler(time_t checkpoint_interval, time_t progress_interval);
  void DoBackupCheckpoint(JobControlRecord* jcr);
  void DoTimedCheckpoint(JobControlRecord* jcr);
  void DoVolumeChangeBackupCheckpoint(JobControlRecord* jcr);
  void SetReadyForCheckpoint() { ready_for_checkpoint_ = true; }
  bool IsReadyForCheckpoint() const { return ready_for_checkpoint_; }

  /** Send a progress update to the Director if the progress interval has
   *  elapsed.  Also sends per-device throughput info.  No-op when
   *  progress_interval_ == 0. */
  void DoTimedProgressUpdate(JobControlRecord* jcr);

  void ResetTimer();

 private:
  void UpdateFileList(JobControlRecord* jcr);
  void UpdateJobmediaRecord(JobControlRecord* jcr);
  void UpdateJobrecord(JobControlRecord* jcr);
  void ClearReadyForCheckpoint() { ready_for_checkpoint_ = false; }

  /** Send job-level progress message and per-device throughput. */
  void SendProgressToDir(JobControlRecord* jcr);

  bool ready_for_checkpoint_ = false;
  time_t next_checkpoint_time_{};
  time_t checkpoint_interval_{};

  /* Progress timer state */
  time_t next_progress_time_{};
  time_t progress_interval_{};

  /* Device throughput tracking: previous sample values */
  uint64_t last_dev_write_bytes_{0};
  uint64_t last_dev_read_bytes_{0};
  time_t   last_progress_sample_time_{0};
};

}  // namespace storagedaemon
#endif  // BAREOS_STORED_CHECKPOINT_HANDLER_H_
