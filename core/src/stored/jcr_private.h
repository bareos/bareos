/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2012 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
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

#ifndef BAREOS_SRC_STORED_JCR_PRIVATE_H_
#define BAREOS_SRC_STORED_JCR_PRIVATE_H_

#include "stored/read_ctx.h"

namespace storagedaemon {
struct VolumeList;
class DeviceControlRecord;
class DirectorResource;
struct BootStrapRecord;
}  // namespace storagedaemon

/* clang-format off */
struct JobControlRecordPrivate {
  JobControlRecord* next_dev = nullptr; /**< Next JobControlRecord attached to device */
  JobControlRecord* prev_dev = nullptr; /**< Previous JobControlRecord attached to device */
  pthread_cond_t job_start_wait = PTHREAD_COND_INITIALIZER; /**< Wait for FD to start Job */
  pthread_cond_t job_end_wait = PTHREAD_COND_INITIALIZER; /**< Wait for Job to end */
  storagedaemon::DeviceControlRecord* read_dcr = nullptr; /**< Device context for reading */
  storagedaemon::DeviceControlRecord* dcr = nullptr;      /**< Device context record */
  POOLMEM* job_name = nullptr;      /**< Base Job name (not unique) */
  POOLMEM* fileset_name = nullptr;  /**< FileSet */
  POOLMEM* fileset_md5 = nullptr;   /**< MD5 for FileSet */
  POOLMEM* backup_format = nullptr; /**< Backup format used when doing a NDMP backup */
  storagedaemon::VolumeList* VolList = nullptr; /**< List to read */
  int32_t NumWriteVolumes = 0; /**< Number of volumes written */
  int32_t NumReadVolumes = 0;  /**< Total number of volumes to read */
  int32_t CurReadVolume = 0;   /**< Current read volume number */
  int32_t label_errors = 0;    /**< Count of label errors */
  bool session_opened = false;
  bool remote_replicate = false;    /**< Replicate data to remote SD */
  int32_t Ticket = 0;               /**< Ticket for this job */
  bool ignore_label_errors = false; /**< Ignore Volume label errors */
  bool spool_attributes = false;    /**< Set if spooling attributes */
  bool no_attributes = false;       /**< Set if no attributes wanted */
  int64_t spool_size = 0;           /**< Spool size for this job */
  bool spool_data = false;          /**< Set to spool data */
  storagedaemon::DirectorResource* director = nullptr; /**< Director resource */
  alist* plugin_options = nullptr;  /**< Specific Plugin Options sent by DIR */
  alist* write_store = nullptr;     /**< List of write storage devices sent by DIR */
  alist* read_store = nullptr;      /**< List of read devices sent by DIR */
  alist* reserve_msgs = nullptr;    /**< Reserve fail messages */
  bool acquired_storage = false;    /**< Did we acquire our reserved storage already or not */
  bool PreferMountedVols = false;   /**< Prefer mounted vols rather than new */
  bool insert_jobmedia_records = false; /**< Need to insert job media records */
  uint64_t RemainingQuota = 0;          /**< Available bytes to use as quota */

  /*
   * Parameters for Open Read Session
   */
  storagedaemon::READ_CTX* rctx = nullptr; /**< Read context used to keep track of what is processed or not */
  storagedaemon::BootStrapRecord* bsr = nullptr; /**< Bootstrap record -- has everything */
  bool mount_next_volume = false; /**< Set to cause next volume mount */
  uint32_t read_VolSessionId = 0;
  uint32_t read_VolSessionTime = 0;
  uint32_t read_StartFile = 0;
  uint32_t read_EndFile = 0;
  uint32_t read_StartBlock = 0;
  uint32_t read_EndBlock = 0;

  /*
   * Device wait times
   */
  int32_t min_wait = 0;
  int32_t max_wait = 0;
  int32_t max_num_wait = 0;
  int32_t wait_sec = 0;
  int32_t rem_wait_sec = 0;
  int32_t num_wait = 0;
};
/* clang-format on */

#endif  // BAREOS_SRC_STORED_JCR_PRIVATE_H_
