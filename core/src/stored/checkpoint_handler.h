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
  CheckpointHandler(time_t interval);
  void DoBackupCheckpoint(JobControlRecord* jcr);
  void DoTimedCheckpoint(JobControlRecord* jcr);
  void DoVolumeChangeBackupCheckpoint(JobControlRecord* jcr);
  void SetReadyForCheckpoint() { ready_for_checkpoint_ = true; }
  bool IsReadyForCheckpoint() const { return ready_for_checkpoint_; }

  void ResetTimer();

 private:
  void UpdateFileList(JobControlRecord* jcr);
  void UpdateJobmediaRecord(JobControlRecord* jcr);
  void UpdateJobrecord(JobControlRecord* jcr);
  void ClearReadyForCheckpoint() { ready_for_checkpoint_ = false; }

  bool ready_for_checkpoint_ = false;
  time_t next_checkpoint_time_{};
  time_t checkpoint_interval_{};
};

}  // namespace storagedaemon
#endif  // BAREOS_STORED_CHECKPOINT_HANDLER_H_
