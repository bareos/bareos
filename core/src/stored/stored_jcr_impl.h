/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2012 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2025 Bareos GmbH & Co. KG

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

#ifndef BAREOS_STORED_STORED_JCR_IMPL_H_
#define BAREOS_STORED_STORED_JCR_IMPL_H_

#include "stored/read_ctx.h"
#include "stored/stored_conf.h"
#include "lib/thread_util.h"
#include "stored/reserve.h"

template <typename T> class alist;

namespace storagedaemon {

struct VolumeList;
class DeviceControlRecord;
class DirectorResource;
struct BootStrapRecord;
struct director_storage;

struct ReadSession {
  READ_CTX* rctx{};
  bool bsr_generated{false};  // set if the bsr was auto generated
  BootStrapRecord* bsr{};
  bool mount_next_volume{};
  uint32_t read_VolSessionId{};
  uint32_t read_VolSessionTime{};
  uint32_t read_StartFile{};
  uint32_t read_EndFile{};
  uint32_t read_StartBlock{};
  uint32_t read_EndBlock{};
};

struct DeviceWaitTimes {
  int32_t min_wait{};
  int32_t max_wait{};
  int32_t max_num_wait{};
  int32_t wait_sec{};
  int32_t rem_wait_sec{};
  int32_t num_wait{};
};

}  // namespace storagedaemon


/* clang-format off */
struct StoredJcrImpl {
  JobControlRecord* next_dev{}; /**< Next JobControlRecord attached to device */
  JobControlRecord* prev_dev{}; /**< Previous JobControlRecord attached to device */
  pthread_cond_t job_end_wait = PTHREAD_COND_INITIALIZER;   /**< Wait for Job to end */
  synchronized<bool> client_available;
  std::condition_variable job_start_wait; /**< Wait for Client (FD/SD) to start Job */
  storagedaemon::DeviceControlRecord* read_dcr{}; /**< Device context for reading */
  storagedaemon::DeviceControlRecord* dcr{};      /**< Device context record */
  POOLMEM* job_name{};            /**< Base Job name (not unique) */
  POOLMEM* fileset_name{};        /**< FileSet */
  POOLMEM* fileset_md5{};         /**< MD5 for FileSet */
  POOLMEM* backup_format{};       /**< Backup format used when doing a NDMP backup */
  int32_t NumWriteVolumes{};      /**< Number of volumes written */
  int32_t label_errors{};         /**< Count of label errors */
  bool session_opened{};
  bool remote_replicate{};        /**< Replicate data to remote SD */
  int32_t Ticket{};               /**< Ticket for this job */
  bool ignore_label_errors{};     /**< Ignore Volume label errors */
  bool spool_attributes{};        /**< Set if spooling attributes */
  bool no_attributes{};           /**< Set if no attributes wanted */
  int64_t spool_size{};           /**< Spool size for this job */
  bool spool_data{};              /**< Set to spool data */
  storagedaemon::DirectorResource* director{}; /**< Director resource */
  alist<const char*>* plugin_options{};        /**< Specific Plugin Options sent by DIR */
  std::vector<storagedaemon::director_storage> dirstores{};           /**< List of storage devices sent by DIR */
  std::vector<std::string> reserve_msgs{};          /**< Reserve fail messages */
  bool acquired_storage{};        /**< Did we acquire our reserved storage already or not */
  bool PreferMountedVols{};       /**< Prefer mounted vols rather than new */
  bool insert_jobmedia_records{}; /**< Need to insert job media records */
  uint64_t RemainingQuota{};      /**< Available bytes to use as quota */

  storagedaemon::ReadSession read_session;
  storagedaemon::DeviceWaitTimes device_wait_times;
};
/* clang-format on */

#endif  // BAREOS_STORED_STORED_JCR_IMPL_H_
