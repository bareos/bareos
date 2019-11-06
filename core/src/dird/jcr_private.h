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

#ifndef BAREOS_SRC_DIRD_JCR_PRIVATE_H_
#define BAREOS_SRC_DIRD_JCR_PRIVATE_H_

namespace directordaemon {
class JobResource;
class StorageResource;
class ClientResource;
class PoolResource;
class FilesetResource;
class CatalogResource;
}  // namespace directordaemon

#define JobWaiting(jcr)                                                       \
  (jcr->job_started &&                                                        \
   (jcr->JobStatus == JS_WaitFD || jcr->JobStatus == JS_WaitSD ||             \
    jcr->JobStatus == JS_WaitMedia || jcr->JobStatus == JS_WaitMount ||       \
    jcr->JobStatus == JS_WaitStoreRes || jcr->JobStatus == JS_WaitJobRes ||   \
    jcr->JobStatus == JS_WaitClientRes || jcr->JobStatus == JS_WaitMaxJobs || \
    jcr->JobStatus == JS_WaitPriority ||                                      \
    jcr->impl->SDJobStatus == JS_WaitMedia ||                                 \
    jcr->impl->SDJobStatus == JS_WaitMount ||                                 \
    jcr->impl->SDJobStatus == JS_WaitDevice ||                                \
    jcr->impl->SDJobStatus == JS_WaitMaxJobs))

/* clang-format off */
struct Resources {
  directordaemon::JobResource* job = nullptr;        /**< Job resource */
  directordaemon::JobResource* verify_job = nullptr; /**< Job resource of verify previous job */
  directordaemon::JobResource* previous_job = nullptr; /**< Job resource of migration previous job */
  directordaemon::StorageResource* read_storage = nullptr; /**< Selected read storage */
  directordaemon::StorageResource* write_storage = nullptr; /**< Selected write storage */
  directordaemon::StorageResource* paired_read_write_storage = nullptr; /*< Selected paired storage (savedwrite_storage or read_storage)*/
  directordaemon::ClientResource* client = nullptr; /**< Client resource */
  directordaemon::PoolResource* pool = nullptr; /**< Pool resource = write for migration */
  directordaemon::PoolResource* rpool = nullptr; /**< Read pool. Used only in migration */
  directordaemon::PoolResource* full_pool = nullptr; /**< Full backup pool resource */
  directordaemon::PoolResource* vfull_pool = nullptr; /**< Virtual Full backup pool resource */
  directordaemon::PoolResource* inc_pool = nullptr; /**< Incremental backup pool resource */
  directordaemon::PoolResource* diff_pool = nullptr; /**< Differential backup pool resource */
  directordaemon::PoolResource* next_pool = nullptr; /**< Next Pool used for migration/copy and virtual backup */
  directordaemon::FilesetResource* fileset = nullptr; /**< FileSet resource */
  directordaemon::CatalogResource* catalog = nullptr; /**< Catalog resource */
  MessagesResource* messages = nullptr; /**< Default message handler */
  POOLMEM* pool_source = nullptr;       /**< Where pool came from */
  POOLMEM* npool_source = nullptr;      /**< Where next pool came from */
  POOLMEM* rpool_source = nullptr;     /**< Where migrate read pool came from */
  POOLMEM* rstore_source = nullptr;    /**< Where read storage came from */
  POOLMEM* wstore_source = nullptr;    /**< Where write storage came from */
  POOLMEM* catalog_source = nullptr;   /**< Where catalog came from */
  alist* read_storage_list = nullptr;  /**< Read storage possibilities */
  alist* write_storage_list = nullptr; /**< Write storage possibilities */
  alist* paired_read_write_storage_list = nullptr; /**< Paired storage possibilities
                                                     *  (saved write_storage_list or read_storage_list) */
  bool run_pool_override = false; /**< Pool override was given on run cmdline */
  bool run_full_pool_override = false; /**< Full pool override was given on run cmdline */
  bool run_vfull_pool_override = false; /**< Virtual Full pool override was given on run cmdline */
  bool run_inc_pool_override = false;   /**< Incremental pool override was given on run cmdline */
  bool run_diff_pool_override = false; /**< Differential pool override was given on run cmdline */
  bool run_next_pool_override = false; /**< Next pool override was given on run cmdline */
};

struct JobControlRecordPrivate {
  JobControlRecordPrivate() {
    RestoreJobId = 0; MigrateJobId = 0; VerifyJobId = 0;
  }
  pthread_t SD_msg_chan = 0;        /**< Message channel thread id */
  bool SD_msg_chan_started = false; /**< Message channel thread started */
  pthread_cond_t term_wait = PTHREAD_COND_INITIALIZER; /**< Wait for job termination */
  pthread_cond_t nextrun_ready = PTHREAD_COND_INITIALIZER; /**< Wait for job next run to become ready */
  Resources res;                /**< Resources assigned */
  TREE_ROOT* restore_tree_root = nullptr; /**< Selected files to restore (some protocols need this info) */
  storagedaemon::BootStrapRecord* bsr = nullptr; /**< Bootstrap record -- has everything */
  char* backup_format = nullptr; /**< Backup format used when doing a NDMP backup */
  char* plugin_options = nullptr;   /**< User set options for plugin */
  uint32_t SDJobFiles = 0;          /**< Number of files written, this job */
  uint64_t SDJobBytes = 0;          /**< Number of bytes processed this job */
  uint32_t SDErrors = 0;            /**< Number of non-fatal errors */
  volatile int32_t SDJobStatus = 0; /**< Storage Job Status */
  volatile int32_t FDJobStatus = 0; /**< File daemon Job Status */
  uint32_t DumpLevel = 0;           /**< Dump level when doing a NDMP backup */
  uint32_t ExpectedFiles = 0;       /**< Expected restore files */
  uint32_t MediaId = 0;        /**< DB record IDs associated with this job */
  uint32_t FileIndex = 0;      /**< Last FileIndex processed */
  utime_t MaxRunSchedTime = 0; /**< Max run time in seconds from Initial Scheduled time */
  JobDbRecord jr;            /**< Job DB record for current job */
  JobDbRecord previous_jr;   /**< Previous job database record */
  JobControlRecord* mig_jcr = nullptr; /**< JobControlRecord for migration/copy job */
  char FSCreateTime[MAX_TIME_LENGTH]{0}; /**< FileSet CreateTime as returned from DB */
  char since[MAX_TIME_LENGTH]{0};        /**< Since time */
  char PrevJob[MAX_NAME_LENGTH]{0}; /**< Previous job name assiciated with since time */
  union {
    JobId_t RestoreJobId; /**< Restore JobId specified by UA */
    JobId_t MigrateJobId; /**< Migration JobId specified by UA */
    JobId_t VerifyJobId;  /**< Verify JobId specified by UA */
  };
  POOLMEM* fname = nullptr;            /**< Name to put into catalog */
  POOLMEM* client_uname = nullptr;     /**< Client uname */
  POOLMEM* FDSecureEraseCmd = nullptr; /**< Report: Secure Erase Command  */
  POOLMEM* SDSecureEraseCmd = nullptr; /**< Report: Secure Erase Command  */
  POOLMEM* vf_jobids = nullptr;        /**< JobIds to use for Virtual Full */
  uint32_t replace = 0;                /**< Replace option */
  int32_t NumVols = 0;                 /**< Number of Volume used in pool */
  int32_t reschedule_count = 0;        /**< Number of times rescheduled */
  int32_t FDVersion = 0;               /**< File daemon version number */
  int64_t spool_size = 0;              /**< Spool size for this job */
  volatile bool sd_msg_thread_done = false; /**< Set when Storage message thread done */
  bool IgnoreDuplicateJobChecking = false; /**< Set in migration jobs */
  bool IgnoreLevelPoolOverides = false; /**< Set if a cmdline pool was specified */
  bool IgnoreClientConcurrency = false;  /**< Set in migration jobs */
  bool IgnoreStorageConcurrency = false; /**< Set in migration jobs */
  bool spool_data = false;               /**< Spool data in SD */
  bool acquired_resource_locks = false;  /**< Set if resource locks acquired */
  bool term_wait_inited = false;         /**< Set when cond var inited */
  bool nextrun_ready_inited = false;     /**< Set when cond var inited */
  bool fn_printed = false;               /**< Printed filename */
  bool needs_sd = false;                 /**< Set if SD needed by Job */
  bool cloned = false;                   /**< Set if cloned */
  bool unlink_bsr = false;               /**< Unlink bsr file created */
  bool VSS = false;                      /**< VSS used by FD */
  bool Encrypt = false;                  /**< Encryption used by FD */
  bool no_maxtime = false; /**< Don't check Max*Time for this JobControlRecord */
  bool keep_sd_auth_key = false; /**< Clear or not the SD auth key after connection*/
  bool use_accurate_chksum = false;                /**< Use or not checksum option in accurate code */
  bool sd_canceled = false; /**< Set if SD canceled */
  bool remote_replicate = false; /**< Replicate data to remote SD */
  bool HasQuota = false; /**< Client has quota limits */
  bool HasSelectedJobs = false; /**< Migration/Copy Job did actually select some JobIds */
  directordaemon::ClientConnectionHandshakeMode connection_handshake_try_ =
      directordaemon::ClientConnectionHandshakeMode::kUndefined;
};
/* clang-format on */

#endif  // BAREOS_SRC_DIRD_JCR_PRIVATE_H_
