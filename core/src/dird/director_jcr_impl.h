/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2012 Free Software Foundation Europe e.V.
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

#ifndef BAREOS_DIRD_DIRECTOR_JCR_IMPL_H_
#define BAREOS_DIRD_DIRECTOR_JCR_IMPL_H_

#include <optional>

#include "cats/cats.h"
#include "lib/mem_pool.h"
#include "dird/client_connection_handshake_mode.h"
#include "dird/job_trigger.h"

#include <condition_variable>

typedef struct s_tree_root TREE_ROOT;

class ConfigResourcesContainer;

namespace directordaemon {
class JobResource;
class StorageResource;
class ClientResource;
class PoolResource;
class FilesetResource;
class CatalogResource;
struct RuntimeJobStatus;
}  // namespace directordaemon

namespace storagedaemon {
struct BootStrapRecord;
}  // namespace storagedaemon

#define JobWaiting(jcr)                                                     \
  (jcr->job_started                                                         \
   && (jcr->getJobStatus() == JS_WaitFD || jcr->getJobStatus() == JS_WaitSD \
       || jcr->getJobStatus() == JS_WaitMedia                               \
       || jcr->getJobStatus() == JS_WaitMount                               \
       || jcr->getJobStatus() == JS_WaitStoreRes                            \
       || jcr->getJobStatus() == JS_WaitJobRes                              \
       || jcr->getJobStatus() == JS_WaitClientRes                           \
       || jcr->getJobStatus() == JS_WaitMaxJobs                             \
       || jcr->getJobStatus() == JS_WaitPriority                            \
       || jcr->dir_impl->SDJobStatus == JS_WaitMedia                        \
       || jcr->dir_impl->SDJobStatus == JS_WaitMount                        \
       || jcr->dir_impl->SDJobStatus == JS_WaitDevice                       \
       || jcr->dir_impl->SDJobStatus == JS_WaitMaxJobs))

/* clang-format off */
struct Resources {
  directordaemon::JobResource* job{};           /**< Job resource */
  directordaemon::JobResource* verify_job{};    /**< Job resource of verify previous job */
  directordaemon::JobResource* previous_job{};  /**< Job resource of migration previous job */
  directordaemon::StorageResource* read_storage{};  /**< Selected read storage */
  directordaemon::StorageResource* write_storage{}; /**< Selected write storage */
  directordaemon::StorageResource* paired_read_write_storage{}; /*< Selected paired storage (savedwrite_storage or read_storage)*/
  directordaemon::ClientResource* client{};     /**< Client resource */
  directordaemon::PoolResource* pool{};         /**< Pool resource = write for migration */
  directordaemon::PoolResource* rpool{};        /**< Read pool. Used only in migration */
  directordaemon::PoolResource* full_pool{};    /**< Full backup pool resource */
  directordaemon::PoolResource* vfull_pool{};   /**< Virtual Full backup pool resource */
  directordaemon::PoolResource* inc_pool{};     /**< Incremental backup pool resource */
  directordaemon::PoolResource* diff_pool{};    /**< Differential backup pool resource */
  directordaemon::PoolResource* next_pool{};    /**< Next Pool used for migration/copy and virtual backup */
  directordaemon::FilesetResource* fileset{};   /**< FileSet resource */
  directordaemon::CatalogResource* catalog{};   /**< Catalog resource */
  directordaemon::RuntimeJobStatus* rjs{};  /**< Runtime Job Status. May point to the rjs of another resource (e.g. for consolidation vf jobs this points to the rjs of the parent consolidation job's resource) */
  MessagesResource* messages{};   /**< Default message handler */
  POOLMEM* pool_source{};         /**< Where pool came from */
  POOLMEM* npool_source{};        /**< Where next pool came from */
  POOLMEM* rpool_source{};        /**< Where migrate read pool came from */
  POOLMEM* rstore_source{};       /**< Where read storage came from */
  POOLMEM* wstore_source{};       /**< Where write storage came from */
  POOLMEM* catalog_source{};      /**< Where catalog came from */
  alist<directordaemon::StorageResource*>* read_storage_list{};     /**< Read storage possibilities */
  alist<directordaemon::StorageResource*>* write_storage_list{};    /**< Write storage possibilities */
  alist<directordaemon::StorageResource*>* paired_read_write_storage_list{}; /**< Paired storage possibilities (saved write_storage_list or read_storage_list) */
  bool run_pool_override{};       /**< Pool override was given on run cmdline */
  bool run_full_pool_override{};  /**< Full pool override was given on run cmdline */
  bool run_vfull_pool_override{}; /**< Virtual Full pool override was given on run cmdline */
  bool run_inc_pool_override{};   /**< Incremental pool override was given on run cmdline */
  bool run_diff_pool_override{};  /**< Differential pool override was given on run cmdline */
  bool run_next_pool_override{};  /**< Next pool override was given on run cmdline */
};

struct DirectorJcrImpl {
  DirectorJcrImpl( std::shared_ptr<ConfigResourcesContainer> configuration_resources_container) : job_config_resources_container_(configuration_resources_container) {
    RestoreJobId = 0; MigrateJobId = 0; VerifyJobId = 0;
  }
  std::shared_ptr<ConfigResourcesContainer> job_config_resources_container_;
  pthread_t SD_msg_chan{};        /**< Message channel thread id */
  bool SD_msg_chan_started{};     /**< Message channel thread started */
  std::condition_variable term_wait{}; /**< Wait for job termination */
  pthread_cond_t nextrun_ready = PTHREAD_COND_INITIALIZER;  /**< Wait for job next run to become ready */
  Resources res;                  /**< Resources assigned */
  TREE_ROOT* restore_tree_root{}; /**< Selected files to restore (some protocols need this info) */
  storagedaemon::BootStrapRecord* bsr{}; /**< Bootstrap record -- has everything */
  char* backup_format{};          /**< Backup format used when doing a NDMP backup */
  char* plugin_options{};         /**< User set options for plugin */
  uint32_t SDJobFiles{};          /**< Number of files written, this job */
  uint64_t SDJobBytes{};          /**< Number of bytes processed this job */
  uint32_t SDErrors{};            /**< Number of non-fatal errors */
  std::atomic<int32_t> SDJobStatus{}; /**< Storage Job Status */
  std::atomic<int32_t> FDJobStatus{}; /**< File daemon Job Status */
  uint32_t DumpLevel{};           /**< Dump level when doing a NDMP backup */
  uint32_t ExpectedFiles{};       /**< Expected restore files */
  uint32_t MediaId{};             /**< DB record IDs associated with this job */
  uint32_t FileIndex{};           /**< Last FileIndex processed */
  utime_t MaxRunSchedTime{};      /**< Max run time in seconds from Initial Scheduled time */
  JobDbRecord jr;                 /**< Job DB record for current job */
  std::optional<JobDbRecord> previous_jr;        /**< Previous job database record */
  JobControlRecord* mig_jcr{};    /**< JobControlRecord for migration/copy job */
  char FSCreateTime[MAX_TIME_LENGTH]{}; /**< FileSet CreateTime as returned from DB */
  char since[MAX_TIME_LENGTH]{};        /**< Since time */
  char PrevJob[MAX_NAME_LENGTH]{};      /**< Previous job name assiciated with since time */
  union {
    JobId_t RestoreJobId;               /**< Restore JobId specified by UA */
    JobId_t MigrateJobId;               /**< Migration JobId specified by UA */
    JobId_t VerifyJobId;                /**< Verify JobId specified by UA */
  };
  PoolMem fname{PM_FNAME};              /**< Name to put into catalog */
  POOLMEM* client_uname{};              /**< Client uname */
  POOLMEM* FDSecureEraseCmd{};          /**< Report: Secure Erase Command  */
  POOLMEM* SDSecureEraseCmd{};          /**< Report: Secure Erase Command  */
  POOLMEM* vf_jobids{};                 /**< JobIds to use for Virtual Full */
  uint32_t replace{};                   /**< Replace option */
  int32_t NumVols{};                    /**< Number of Volume used in pool */
  int32_t reschedule_count{};           /**< Number of times rescheduled */
  int32_t FDVersion{};                  /**< File daemon version number */
  int64_t spool_size{};                 /**< Spool size for this job */
  std::atomic<bool> sd_msg_thread_done{};   /**< Set when Storage message thread done */
  bool IgnoreDuplicateJobChecking{};    /**< Set in migration jobs */
  bool IgnoreLevelPoolOverrides{};       /**< Set if a cmdline pool was specified */
  bool IgnoreClientConcurrency{};       /**< Set in migration jobs */
  bool IgnoreStorageConcurrency{};      /**< Set in migration jobs */
  int32_t max_concurrent_jobs{};        /**< Maximum concurrent jobs */
  bool spool_data{};                    /**< Spool data in SD */
  bool acquired_resource_locks{};       /**< Set if resource locks acquired */
  bool nextrun_ready_inited{};          /**< Set when cond var inited */
  bool fn_printed{};                    /**< Printed filename */
  bool needs_sd{};                      /**< Set if SD needed by Job */
  bool cloned{};                        /**< Set if cloned */
  bool unlink_bsr{};                    /**< Unlink bsr file created */
  bool VSS{};                           /**< VSS used by FD */
  bool Encrypt{};                       /**< Encryption used by FD */
  bool no_maxtime{};                    /**< Don't check Max*Time for this JobControlRecord */
  bool keep_sd_auth_key{};              /**< Clear or not the SD auth key after connection*/
  bool use_accurate_chksum{};           /**< Use or not checksum option in accurate code */
  bool sd_canceled{};                   /**< Set if SD canceled */
  bool remote_replicate{};              /**< Replicate data to remote SD */
  bool HasQuota{};                      /**< Client has quota limits */
  bool HasSelectedJobs{};               /**< Migration/Copy Job did actually select some JobIds */
  directordaemon::ClientConnectionHandshakeMode connection_handshake_try_{
    directordaemon::ClientConnectionHandshakeMode::kUndefined};
  JobTrigger job_trigger{JobTrigger::kUndefined};
};
/* clang-format on */

#endif  // BAREOS_DIRD_DIRECTOR_JCR_IMPL_H_
