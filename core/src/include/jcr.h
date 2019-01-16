/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2012 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2018 Bareos GmbH & Co. KG

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
/*
 * Kern Sibbald, Nov MM
 *
 */
/** @file
 * Bareos JobControlRecord Structure definition for Daemons and the Library
 *
 *  This definition consists of a "Global" definition common
 *  to all daemons and used by the library routines, and a
 *  daemon specific part that is enabled with #defines.
 */
#ifndef BAREOS_INCLUDE_JCR_H_
#define BAREOS_INCLUDE_JCR_H_ 1

#include <include/bareos.h>
#include "lib/tls_conf.h"

#ifdef STORAGE_DAEMON
#include "stored/read_ctx.h"
#endif

#ifdef DIRECTOR_DAEMON
#include "cats/cats.h"
#include "dird/client_connection_handshake_mode.h"
#endif

namespace directordaemon {
   class JobResource;
   class StorageResource;
   class ClientResource;
   class PoolResource;
   class FilesetResource;
   class CatalogResource;
}

namespace storagedaemon {
   struct VolumeList;
   class DeviceControlRecord;
   class DirectorResource;
   struct BootStrapRecord;
}

namespace filedaemon {
   class BareosAccurateFilelist;
   struct save_pkt;
}

/**
 * Backup/Verify level code. These are stored in the DB
 */
#define L_FULL                        'F' /**< Full backup */
#define L_INCREMENTAL                 'I' /**< Since last backup */
#define L_DIFFERENTIAL                'D' /**< Since last full backup */
#define L_SINCE                       'S'
#define L_VERIFY_CATALOG              'C' /**< Verify from catalog */
#define L_VERIFY_INIT                 'V' /**< Verify save (init DB) */
#define L_VERIFY_VOLUME_TO_CATALOG    'O' /**< Verify Volume to catalog entries */
#define L_VERIFY_DISK_TO_CATALOG      'd' /**< Verify Disk attributes to catalog */
#define L_VERIFY_DATA                 'A' /**< Verify data on volume */
#define L_BASE                        'B' /**< Base level job */
#define L_NONE                        ' ' /**< None, for Restore and Admin */
#define L_VIRTUAL_FULL                'f' /**< Virtual full backup */

/**
 * Job Types. These are stored in the DB
 */
#define JT_BACKUP                     'B' /**< Backup Job */
#define JT_MIGRATED_JOB               'M' /**< A previous backup job that was migrated */
#define JT_VERIFY                     'V' /**< Verify Job */
#define JT_RESTORE                    'R' /**< Restore Job */
#define JT_CONSOLE                    'U' /**< console program */
#define JT_SYSTEM                     'I' /**< internal system "job" */
#define JT_ADMIN                      'D' /**< admin job */
#define JT_ARCHIVE                    'A' /**< Archive Job */
#define JT_JOB_COPY                   'C' /**< Copy of a Job */
#define JT_COPY                       'c' /**< Copy Job */
#define JT_MIGRATE                    'g' /**< Migration Job */
#define JT_SCAN                       'S' /**< Scan Job */
#define JT_CONSOLIDATE                'O' /**< Always Incremental Consolidate Job */

/**
 * Job Status. Some of these are stored in the DB
 */
#define JS_Canceled                   'A' /**< Canceled by user */
#define JS_Blocked                    'B' /**< Blocked */
#define JS_Created                    'C' /**< Created but not yet running */
#define JS_Differences                'D' /**< Verify differences */
#define JS_ErrorTerminated            'E' /**< Job terminated in error */
#define JS_WaitFD                     'F' /**< Waiting on File daemon */
#define JS_Incomplete                 'I' /**< Incomplete Job */
#define JS_DataCommitting             'L' /**< Committing data (last despool) */
#define JS_WaitMount                  'M' /**< Waiting for Mount */
#define JS_Running                    'R' /**< Running */
#define JS_WaitSD                     'S' /**< Waiting on the Storage daemon */
#define JS_Terminated                 'T' /**< Terminated normally */
#define JS_Warnings                   'W' /**< Terminated normally with warnings */

#define JS_AttrDespooling             'a' /**< SD despooling attributes */
#define JS_WaitClientRes              'c' /**< Waiting for Client resource */
#define JS_WaitMaxJobs                'd' /**< Waiting for maximum jobs */
#define JS_Error                      'e' /**< Non-fatal error */
#define JS_FatalError                 'f' /**< Fatal error */
#define JS_AttrInserting              'i' /**< Doing batch insert file records */
#define JS_WaitJobRes                 'j' /**< Waiting for job resource */
#define JS_DataDespooling             'l' /**< Doing data despooling */
#define JS_WaitMedia                  'm' /**< Waiting for new media */
#define JS_WaitPriority               'p' /**< Waiting for higher priority jobs to finish */
#define JS_WaitDevice                 'q' /**< Queued waiting for device */
#define JS_WaitStoreRes               's' /**< Waiting for storage resource */
#define JS_WaitStartTime              't' /**< Waiting for start time */

/**
 * Protocol types
 */
enum {
   PT_NATIVE = 0,
   PT_NDMP_BAREOS, /* alias for PT_NDMP */
   PT_NDMP_NATIVE
};

/**
 * Authentication Protocol types
 */
enum {
   APT_NATIVE = 0,
   APT_NDMPV2,
   APT_NDMPV3,
   APT_NDMPV4
};

/**
 * Authentication types
 */
enum {
   AT_NONE = 0,
   AT_CLEAR,
   AT_MD5,
   AT_VOID
};

/**
 * Migration selection types
 */
enum {
   MT_SMALLEST_VOL = 1,
   MT_OLDEST_VOL,
   MT_POOL_OCCUPANCY,
   MT_POOL_TIME,
   MT_POOL_UNCOPIED_JOBS,
   MT_CLIENT,
   MT_VOLUME,
   MT_JOB,
   MT_SQLQUERY
};

#define JobTerminatedSuccessfully(jcr) \
  (jcr->JobStatus == JS_Terminated || \
   jcr->JobStatus == JS_Warnings \
  )

#define JobCanceled(jcr) \
  (jcr->JobStatus == JS_Canceled || \
   jcr->JobStatus == JS_ErrorTerminated || \
   jcr->JobStatus == JS_FatalError \
  )

#define JobWaiting(jcr) \
 (jcr->job_started && \
  (jcr->JobStatus == JS_WaitFD || \
   jcr->JobStatus == JS_WaitSD || \
   jcr->JobStatus == JS_WaitMedia || \
   jcr->JobStatus == JS_WaitMount || \
   jcr->JobStatus == JS_WaitStoreRes || \
   jcr->JobStatus == JS_WaitJobRes || \
   jcr->JobStatus == JS_WaitClientRes || \
   jcr->JobStatus == JS_WaitMaxJobs || \
   jcr->JobStatus == JS_WaitPriority || \
   jcr->SDJobStatus == JS_WaitMedia || \
   jcr->SDJobStatus == JS_WaitMount || \
   jcr->SDJobStatus == JS_WaitDevice || \
   jcr->SDJobStatus == JS_WaitMaxJobs))

#define foreach_jcr(jcr) \
   for (jcr=jcr_walk_start(); jcr; (jcr=jcr_walk_next(jcr)) )

#define endeach_jcr(jcr) JcrWalkEnd(jcr)

#define SD_APPEND 1
#define SD_READ   0

/**
 * Forward referenced structures
 */
class JobControlRecord;
class BareosSocket;
struct FindFilesPacket;
class BareosDb;
struct AttributesDbRecord;
struct bpContext;
#ifdef HAVE_WIN32
struct CopyThreadContext;
#endif

#ifdef FILE_DAEMON
class htable;
struct acl_data_t;
struct xattr_data_t;

struct CryptoContext {
   bool pki_sign;                         /**< Enable PKI Signatures? */
   bool pki_encrypt;                      /**< Enable PKI Encryption? */
   DIGEST *digest;                        /**< Last file's digest context */
   X509_KEYPAIR *pki_keypair;             /**< Encryption key pair */
   alist *pki_signers;                    /**< Trusted Signers */
   alist *pki_recipients;                 /**< Trusted Recipients */
   CRYPTO_SESSION *pki_session;           /**< PKE Public Keys + Symmetric Session Keys */
   POOLMEM *crypto_buf;                   /**< Encryption/Decryption buffer */
   POOLMEM *pki_session_encoded;          /**< Cached DER-encoded copy of pki_session */
   int32_t pki_session_encoded_size;      /**< Size of DER-encoded pki_session */
};
#endif

#ifdef DIRECTOR_DAEMON
struct Resources {
   directordaemon::JobResource *job;         /**< Job resource */
   directordaemon::JobResource *verify_job;  /**< Job resource of verify previous job */
   directordaemon::JobResource *previous_job;/**< Job resource of migration previous job */
   directordaemon::StorageResource *read_storage;  /**< Selected read storage */
   directordaemon::StorageResource *write_storage;  /**< Selected write storage */
   directordaemon::StorageResource *paired_read_write_storage;  /**< Selected paired storage (saved write_storage or read_storage) */
   directordaemon::ClientResource *client;   /**< Client resource */
   directordaemon::PoolResource *pool;       /**< Pool resource = write for migration */
   directordaemon::PoolResource *rpool;      /**< Read pool. Used only in migration */
   directordaemon::PoolResource *full_pool;  /**< Full backup pool resource */
   directordaemon::PoolResource *vfull_pool; /**< Virtual Full backup pool resource */
   directordaemon::PoolResource *inc_pool;   /**< Incremental backup pool resource */
   directordaemon::PoolResource *diff_pool;  /**< Differential backup pool resource */
   directordaemon::PoolResource *next_pool;  /**< Next Pool used for migration/copy and virtual backup */
   directordaemon::FilesetResource *fileset; /**< FileSet resource */
   directordaemon::CatalogResource *catalog; /**< Catalog resource */
   MessagesResource *messages;            /**< Default message handler */
   POOLMEM *pool_source;                  /**< Where pool came from */
   POOLMEM *npool_source;                 /**< Where next pool came from */
   POOLMEM *rpool_source;                 /**< Where migrate read pool came from */
   POOLMEM *rstore_source;                /**< Where read storage came from */
   POOLMEM *wstore_source;                /**< Where write storage came from */
   POOLMEM *catalog_source;               /**< Where catalog came from */
   alist *read_storage_list;              /**< Read storage possibilities */
   alist *write_storage_list;             /**< Write storage possibilities */
   alist *paired_read_write_storage_list; /**< Paired storage possibilities (saved write_storage_list or read_storage_list) */
   bool run_pool_override;                /**< Pool override was given on run cmdline */
   bool run_full_pool_override;           /**< Full pool override was given on run cmdline */
   bool run_vfull_pool_override;          /**< Virtual Full pool override was given on run cmdline */
   bool run_inc_pool_override;            /**< Incremental pool override was given on run cmdline */
   bool run_diff_pool_override;           /**< Differential pool override was given on run cmdline */
   bool run_next_pool_override;           /**< Next pool override was given on run cmdline */
};
#endif

struct CompressionContext {
   POOLMEM *deflate_buffer;               /**< Buffer used for deflation (compression) */
   POOLMEM *inflate_buffer;               /**< Buffer used for inflation (decompression) */
   uint32_t deflate_buffer_size;          /**< Length of deflation buffer */
   uint32_t inflate_buffer_size;          /**< Length of inflation buffer */
   struct {
#ifdef HAVE_LIBZ
      void *pZLIB;                        /**< ZLIB compression session data */
#endif
#ifdef HAVE_LZO
      void *pLZO;                         /**< LZO compression session data */
#endif
#ifdef HAVE_FASTLZ
      void *pZFAST;                       /**< FASTLZ compression session data */
#endif
   } workset;
};

struct job_callback_item {
   void (*JobEndCb)(JobControlRecord *jcr, void *);
   void *ctx;
};

typedef void (JCR_free_HANDLER)(JobControlRecord *jcr);

/**
 * Job Control Record (JobControlRecord)
 */
class JobControlRecord {
private:
   pthread_mutex_t mutex;                 /**< Jcr mutex */
   volatile int32_t _use_count;           /**< Use count */
   int32_t JobType_;                     /**< Backup, restore, verify ... */
   int32_t JobLevel_;                    /**< Job level */
   int32_t Protocol_;                    /**< Backup Protocol */
   bool my_thread_killable;               /**< Can we kill the thread? */
public:
      JobControlRecord() {
            Dmsg0(100, "Construct JobControlRecord\n");
      }

      ~JobControlRecord() {
            Dmsg0(100, "Destruct JobControlRecord\n");
      }

   void lock() {P(mutex); }
   void unlock() {V(mutex); }
   void IncUseCount(void) {lock(); _use_count++; unlock(); }
   void DecUseCount(void) {lock(); _use_count--; unlock(); }
   int32_t UseCount() const { return _use_count; }
   void InitMutex(void) {pthread_mutex_init(&mutex, NULL); }
   void DestroyMutex(void) {pthread_mutex_destroy(&mutex); }
   bool IsJobCanceled() { return JobCanceled(this); }
   bool IsCanceled() { return JobCanceled(this); }
   bool IsTerminatedOk() { return JobTerminatedSuccessfully(this); }
   bool IsIncomplete() { return JobStatus == JS_Incomplete; }
   bool is_JobLevel(int32_t JobLevel) { return JobLevel == JobLevel_; }
   bool is_JobType(int32_t JobType) { return JobType == JobType_; }
   bool is_JobStatus(int32_t aJobStatus) { return aJobStatus == JobStatus; }
   void setJobLevel(int32_t JobLevel) { JobLevel_ = JobLevel; }
   void setJobType(int32_t JobType) { JobType_ = JobType; }
   void setJobProtocol(int32_t JobProtocol) { Protocol_ = JobProtocol; }
   void forceJobStatus(int32_t aJobStatus) { JobStatus = aJobStatus; }
   void setJobStarted();
   int32_t getJobType() const { return JobType_; }
   int32_t getJobLevel() const { return JobLevel_; }
   int32_t getJobStatus() const { return JobStatus; }
   int32_t getJobProtocol() const { return Protocol_; }
   bool NoClientUsed() const {
      return (JobType_ == JT_MIGRATE ||
              JobType_ == JT_COPY ||
              JobLevel_ == L_VIRTUAL_FULL);
   }
   bool IsPlugin() const {
      return (cmd_plugin ||
              opt_plugin);
   }
   const char *get_OperationName();       /**< in lib/jcr.c */
   const char *get_ActionName(bool past = false); /**< in lib/jcr.c */
   void setJobStatus(int newJobStatus);   /**< in lib/jcr.c */
   void resetJobStatus(int newJobStatus); /**< in lib/jcr.c */
   bool sendJobStatus();                  /**< in lib/jcr.c */
   bool sendJobStatus(int newJobStatus);  /**< in lib/jcr.c */
   bool JobReads();                       /**< in lib/jcr.c */
   void MyThreadSendSignal(int sig);      /**< in lib/jcr.c */
   void SetKillable(bool killable);       /**< in lib/jcr.c */
   bool IsKillable() const { return my_thread_killable; }

   /*
    * Global part of JobControlRecord common to all daemons
    */
   dlink link;                            /**< JobControlRecord chain link */
   pthread_t my_thread_id;                /**< Id of thread controlling jcr */
   BareosSocket *dir_bsock;               /**< Director bsock or NULL if we are him */
   BareosSocket *store_bsock;             /**< Storage connection socket */
   BareosSocket *file_bsock;              /**< File daemon connection socket */
   JCR_free_HANDLER *daemon_free_jcr;     /**< Local free routine */
   dlist *msg_queue;                      /**< Queued messages */
   pthread_mutex_t msg_queue_mutex;       /**< message queue mutex */
   bool dequeuing_msgs;                   /**< Set when dequeuing messages */
   alist job_end_callbacks;               /**< callbacks called at Job end */
   POOLMEM *VolumeName;                   /**< Volume name desired -- pool_memory */
   POOLMEM *errmsg;                       /**< Edited error message */
   char Job[MAX_NAME_LENGTH];             /**< Unique name of this Job */

   uint32_t JobId;                        /**< Director's JobId */
   uint32_t VolSessionId;
   uint32_t VolSessionTime;
   uint32_t JobFiles;                     /**< Number of files written, this job */
   uint32_t JobErrors;                    /**< Number of non-fatal errors this job */
   uint32_t JobWarnings;                  /**< Number of warning messages */
   uint32_t LastRate;                     /**< Last sample bytes/sec */
   uint64_t JobBytes;                     /**< Number of bytes processed this job */
   uint64_t LastJobBytes;                 /**< Last sample number bytes */
   uint64_t ReadBytes;                    /**< Bytes read -- before compression */
   FileId_t FileId;                       /**< Last FileId used */
   volatile int32_t JobStatus;            /**< ready, running, blocked, terminated */
   int32_t JobPriority;                   /**< Job priority */
   time_t sched_time;                     /**< Job schedule time, i.e. when it should start */
   time_t initial_sched_time;             /**< Original sched time before any reschedules are done */
   time_t start_time;                     /**< When job actually started */
   time_t run_time;                       /**< Used for computing speed */
   time_t last_time;                      /**< Last sample time */
   time_t end_time;                       /**< Job end time */
   time_t wait_time_sum;                  /**< Cumulative wait time since job start */
   time_t wait_time;                      /**< Timestamp when job have started to wait */
   time_t job_started_time;               /**< Time when the MaxRunTime start to count */
   POOLMEM *client_name;                  /**< Client name */
   POOLMEM *JobIds;                       /**< User entered string of JobIds */
   POOLMEM *RestoreBootstrap;             /**< Bootstrap file to restore */
   POOLMEM *stime;                        /**< start time for incremental/differential */
   char *sd_auth_key;                     /**< SD auth key */
   TlsPolicy sd_tls_policy;                  /**< SD Tls Policy */
   MessagesResource *jcr_msgs;            /**< Copy of message resource -- actually used */
   uint32_t ClientId;                     /**< Client associated with Job */
   char *where;                           /**< Prefix to restore files to */
   char *RegexWhere;                      /**< File relocation in restore */
   alist *where_bregexp;                  /**< BareosRegex alist for path manipulation */
   int32_t cached_pnl;                    /**< Cached path length */
   POOLMEM *cached_path;                  /**< Cached path */
   bool passive_client;                   /**< Client is a passive client e.g. doesn't initiate any network connection */
   bool prefix_links;                     /**< Prefix links with Where path */
   bool gui;                              /**< Set if gui using console */
   bool authenticated;                    /**< Set when client authenticated */
   bool cached_attribute;                 /**< Set if attribute is cached */
   bool batch_started;                    /**< Set if batch mode started */
   bool cmd_plugin;                       /**< Set when processing a command Plugin = */
   bool opt_plugin;                       /**< Set when processing an option Plugin = */
   bool keep_path_list;                   /**< Keep newly created path in a hash */
   bool accurate;                         /**< True if job is accurate */
   bool HasBase;                          /**< True if job use base jobs */
   bool rerunning;                        /**< Rerunning an incomplete job */
   bool job_started;                      /**< Set when the job is actually started */
   bool suppress_output;                  /**< Set if this JobControlRecord should not output any Jmsgs */
   JobControlRecord *cjcr;                /**< Controlling JobControlRecord when this is a slave JobControlRecord being
                                           * controlled by another JobControlRecord used for sending
                                           * normal and fatal errors.
                                           */
   int32_t buf_size;                      /**< Length of buffer */
   CompressionContext compress;           /**< Compression ctx */
#ifdef HAVE_WIN32
   CopyThreadContext *cp_thread;          /**< Copy Thread ctx */
#endif
   POOLMEM *attr;                         /**< Attribute string from SD */
   BareosDb *db;                          /**< database pointer */
   BareosDb *db_batch;                    /**< database pointer for batch and accurate */
   uint64_t nb_base_files;                /**< Number of base files */
   uint64_t nb_base_files_used;           /**< Number of useful files in base */

   AttributesDbRecord *ar;                /**< DB attribute record */
   guid_list *id_list;                    /**< User/group id to name list */

   alist *plugin_ctx_list;                /**< List of contexts for plugins */
   bpContext *plugin_ctx;                 /**< Current plugin context */
   POOLMEM *comment;                      /**< Comment for this Job */
   int64_t max_bandwidth;                 /**< Bandwidth limit for this Job */
   htable *path_list;                     /**< Directory list (used by findlib) */
   bool is_passive_client_connection_probing;     /**< Set if director probes a passive client connection */

   /*
    * Daemon specific part of JobControlRecord
    * This should be empty in the library
    */

#ifdef DIRECTOR_DAEMON
   /*
    * Director Daemon specific data part of JobControlRecord
    */
   pthread_t SD_msg_chan;                 /**< Message channel thread id */
   bool SD_msg_chan_started;              /**< Message channel thread started */
   pthread_cond_t start_wait;             /**< Wait for FD to start Job */
   pthread_cond_t term_wait;              /**< Wait for job termination */
   pthread_cond_t nextrun_ready;          /**< Wait for job next run to become ready */
   workq_ele_t *work_item;                /**< Work queue item if scheduled */
   BareosSocket *ua;                      /**< User agent */
   Resources res;                         /**< Resources assigned */
   TREE_ROOT *restore_tree_root;          /**< Selected files to restore (some protocols need this info) */
   storagedaemon::BootStrapRecord *bsr;   /**< Bootstrap record -- has everything */
   char *backup_format;                   /**< Backup format used when doing a NDMP backup */
   char *plugin_options;                  /**< User set options for plugin */
   uint32_t SDJobFiles;                   /**< Number of files written, this job */
   uint64_t SDJobBytes;                   /**< Number of bytes processed this job */
   uint32_t SDErrors;                     /**< Number of non-fatal errors */
   volatile int32_t SDJobStatus;          /**< Storage Job Status */
   volatile int32_t FDJobStatus;          /**< File daemon Job Status */
   uint32_t DumpLevel;                    /**< Dump level when doing a NDMP backup */
   uint32_t ExpectedFiles;                /**< Expected restore files */
   uint32_t MediaId;                      /**< DB record IDs associated with this job */
   uint32_t FileIndex;                    /**< Last FileIndex processed */
   utime_t MaxRunSchedTime;               /**< Max run time in seconds from Initial Scheduled time */
   JobDbRecord jr;                        /**< Job DB record for current job */
   JobDbRecord previous_jr;               /**< Previous job database record */
   JobControlRecord *mig_jcr;             /**< JobControlRecord for migration/copy job */
   char FSCreateTime[MAX_TIME_LENGTH];    /**< FileSet CreateTime as returned from DB */
   char since[MAX_TIME_LENGTH];           /**< Since time */
   char PrevJob[MAX_NAME_LENGTH];         /**< Previous job name assiciated with since time */
   union {
      JobId_t RestoreJobId;               /**< Restore JobId specified by UA */
      JobId_t MigrateJobId;               /**< Migration JobId specified by UA */
      JobId_t VerifyJobId;                /**< Verify JobId specified by UA */
   };
   POOLMEM *fname;                        /**< Name to put into catalog */
   POOLMEM *client_uname;                 /**< Client uname */
   POOLMEM *FDSecureEraseCmd;             /**< Report: Secure Erase Command  */
   POOLMEM *SDSecureEraseCmd;             /**< Report: Secure Erase Command  */
   POOLMEM *vf_jobids;                    /**< JobIds to use for Virtual Full */
   uint32_t replace;                      /**< Replace option */
   int32_t NumVols;                       /**< Number of Volume used in pool */
   int32_t reschedule_count;              /**< Number of times rescheduled */
   int32_t FDVersion;                     /**< File daemon version number */
   int64_t spool_size;                    /**< Spool size for this job */
   volatile bool sd_msg_thread_done;      /**< Set when Storage message thread done */
   bool IgnoreDuplicateJobChecking;       /**< Set in migration jobs */
   bool IgnoreLevelPoolOverides;          /**< Set if a cmdline pool was specified */
   bool IgnoreClientConcurrency;          /**< Set in migration jobs */
   bool IgnoreStorageConcurrency;         /**< Set in migration jobs */
   bool spool_data;                       /**< Spool data in SD */
   bool acquired_resource_locks;          /**< Set if resource locks acquired */
   bool start_wait_inited;                /**< Set when cond var inited */
   bool term_wait_inited;                 /**< Set when cond var inited */
   bool nextrun_ready_inited;             /**< Set when cond var inited */
   bool fn_printed;                       /**< Printed filename */
   bool needs_sd;                         /**< Set if SD needed by Job */
   bool cloned;                           /**< Set if cloned */
   bool unlink_bsr;                       /**< Unlink bsr file created */
   bool VSS;                              /**< VSS used by FD */
   bool Encrypt;                          /**< Encryption used by FD */
   bool stats_enabled;                    /**< Keep all job records in a table for long term statistics */
   bool no_maxtime;                       /**< Don't check Max*Time for this JobControlRecord */
   bool keep_sd_auth_key;                 /**< Clear or not the SD auth key after connection*/
   bool use_accurate_chksum;              /**< Use or not checksum option in accurate code */
   bool sd_canceled;                      /**< Set if SD canceled */
   bool remote_replicate;                 /**< Replicate data to remote SD */
   bool RescheduleIncompleteJobs;         /**< Set if incomplete can be rescheduled */
   bool HasQuota;                         /**< Client has quota limits */
   bool HasSelectedJobs;                  /**< Migration/Copy Job did actually select some JobIds */
   directordaemon::ClientConnectionHandshakeMode connection_handshake_try_;
#endif /* DIRECTOR_DAEMON */

#ifdef FILE_DAEMON
   /*
    * File Daemon specific part of JobControlRecord
    */
   uint32_t num_files_examined;           /**< Files examined this job */
   POOLMEM *last_fname;                   /**< Last file saved/verified */
   POOLMEM *job_metadata;                 /**< VSS job metadata */
   acl_data_t *acl_data;                  /**< ACLs for backup/restore */
   xattr_data_t *xattr_data;              /**< Extended Attributes for backup/restore */
   int32_t last_type;                     /**< Type of last file saved/verified */
   bool incremental;                      /**< Set if incremental for SINCE */
   utime_t mtime;                         /**< Begin time for SINCE */
   int listing;                           /**< Job listing in estimate */
   int32_t Ticket;                        /**< Ticket */
   char *big_buf;                         /**< I/O buffer */
   int32_t replace;                       /**< Replace options */
   FindFilesPacket *ff;                   /**< Find Files packet */
   char PrevJob[MAX_NAME_LENGTH];         /**< Previous job name assiciated with since time */
   uint32_t ExpectedFiles;                /**< Expected restore files */
   uint32_t StartFile;
   uint32_t EndFile;
   uint32_t StartBlock;
   uint32_t EndBlock;
   pthread_t heartbeat_id;                /**< Id of heartbeat thread */
   volatile bool hb_started;              /**< Heartbeat running */
   std::shared_ptr<BareosSocket> hb_bsock;       /**< Duped SD socket */
   std::shared_ptr<BareosSocket> hb_dir_bsock;   /**< Duped DIR socket */
   alist *RunScripts;                     /**< Commands to run before and after job */
   CryptoContext crypto;                  /**< Crypto ctx */
   filedaemon::DirectorResource *director;/**< Director resource */
   bool enable_vss;                       /**< VSS used by FD */
   bool got_metadata;                     /**< Set when found job_metadata */
   bool multi_restore;                    /**< Dir can do multiple storage restore */
   filedaemon::BareosAccurateFilelist *file_list; /**< Previous file list (accurate mode) */
   uint64_t base_size;                    /**< Compute space saved with base job */
   filedaemon::save_pkt *plugin_sp;       /**< Plugin save packet */
#ifdef HAVE_WIN32
   VSSClient *pVSSClient;                 /**< VSS Client Instance */
#endif
#endif /* FILE_DAEMON */

#ifdef STORAGE_DAEMON
   /*
    * Storage Daemon specific part of JobControlRecord
    */
   JobControlRecord *next_dev;            /**< Next JobControlRecord attached to device */
   JobControlRecord *prev_dev;            /**< Previous JobControlRecord attached to device */
   char *dir_auth_key;                    /**< Dir auth key */
   pthread_cond_t job_start_wait;         /**< Wait for FD to start Job */
   pthread_cond_t job_end_wait;           /**< Wait for Job to end */
   int32_t type;
   storagedaemon::DeviceControlRecord *read_dcr; /**< Device context for reading */
   storagedaemon::DeviceControlRecord *dcr;      /**< Device context record */
   alist *dcrs;                           /**< List of dcrs open */
   POOLMEM *job_name;                     /**< Base Job name (not unique) */
   POOLMEM *fileset_name;                 /**< FileSet */
   POOLMEM *fileset_md5;                  /**< MD5 for FileSet */
   POOLMEM *backup_format;                /**< Backup format used when doing a NDMP backup */
   storagedaemon::VolumeList *VolList;    /**< List to read */
   int32_t NumWriteVolumes;               /**< Number of volumes written */
   int32_t NumReadVolumes;                /**< Total number of volumes to read */
   int32_t CurReadVolume;                 /**< Current read volume number */
   int32_t label_errors;                  /**< Count of label errors */
   bool session_opened;
   bool remote_replicate;                 /**< Replicate data to remote SD */
   int32_t Ticket;                        /**< Ticket for this job */
   bool ignore_label_errors;              /**< Ignore Volume label errors */
   bool spool_attributes;                 /**< Set if spooling attributes */
   bool no_attributes;                    /**< Set if no attributes wanted */
   int64_t spool_size;                    /**< Spool size for this job */
   bool spool_data;                       /**< Set to spool data */
   int32_t CurVol;                        /**< Current Volume count */
   storagedaemon::DirectorResource *director; /**< Director resource */
   alist *plugin_options;                 /**< Specific Plugin Options sent by DIR */
   alist *write_store;                    /**< List of write storage devices sent by DIR */
   alist *read_store;                     /**< List of read devices sent by DIR */
   alist *reserve_msgs;                   /**< Reserve fail messages */
   bool acquired_storage;                 /**< Did we acquire our reserved storage already or not */
   bool PreferMountedVols;                /**< Prefer mounted vols rather than new */
   bool Resched;                          /**< Job may be rescheduled */
   bool insert_jobmedia_records;          /**< Need to insert job media records */
   uint64_t RemainingQuota;               /**< Available bytes to use as quota */

   /*
    * Parameters for Open Read Session
    */
   storagedaemon::READ_CTX *rctx;         /**< Read context used to keep track of what is processed or not */
   storagedaemon::BootStrapRecord *bsr;   /**< Bootstrap record -- has everything */
   bool mount_next_volume;                /**< Set to cause next volume mount */
   uint32_t read_VolSessionId;
   uint32_t read_VolSessionTime;
   uint32_t read_StartFile;
   uint32_t read_EndFile;
   uint32_t read_StartBlock;
   uint32_t read_EndBlock;

   /*
    * Device wait times
    */
   int32_t min_wait;
   int32_t max_wait;
   int32_t max_num_wait;
   int32_t wait_sec;
   int32_t rem_wait_sec;
   int32_t num_wait;
#endif /* STORAGE_DAEMON */
};

#define INVALID_JCR (nullptr)

/*
 * Structure for all daemons that keeps some summary
 *  info on the last job run.
 */
struct s_last_job {
   dlink link;
   int32_t Errors;                        /** FD/SD errors */
   int32_t JobType;
   int32_t JobStatus;
   int32_t JobLevel;
   uint32_t JobId;
   uint32_t VolSessionId;
   uint32_t VolSessionTime;
   uint32_t JobFiles;
   uint64_t JobBytes;
   utime_t start_time;
   utime_t end_time;
   char Job[MAX_NAME_LENGTH];
};

extern struct s_last_job last_job;
extern dlist *last_jobs;

/*
 * The following routines are found in lib/jcr.c
 */
extern int GetNextJobidFromList(char **p, uint32_t *JobId);
extern bool InitJcrSubsystem(int timeout);
extern JobControlRecord *new_jcr(int size, JCR_free_HANDLER *daemon_free_jcr);
extern JobControlRecord *get_jcr_by_id(uint32_t JobId);
extern JobControlRecord *get_jcr_by_session(uint32_t SessionId, uint32_t SessionTime);
extern JobControlRecord *get_jcr_by_partial_name(char *Job);
extern JobControlRecord *get_jcr_by_full_name(char *Job);
extern const char *JcrGetAuthenticateKey(const char *unified_job_name);
TlsPolicy JcrGetTlsPolicy(const char *unified_job_name);
extern JobControlRecord *get_next_jcr(JobControlRecord *jcr);
extern void SetJcrJobStatus(JobControlRecord *jcr, int JobStatus);
extern int num_jobs_run;

#ifdef DEBUG
extern void b_free_jcr(const char *file, int line, JobControlRecord *jcr);
#define FreeJcr(jcr) b_free_jcr(__FILE__, __LINE__, (jcr))
#else
extern void FreeJcr(JobControlRecord *jcr);
#endif

/*
 * Used to display specific job information after a fatal signal
 */
typedef void (dbg_jcr_hook_t)(JobControlRecord *jcr, FILE *fp);
extern void DbgJcrAddHook(dbg_jcr_hook_t *fct);

#endif /** BAREOS_INCLUDE_JCR_H_ */
