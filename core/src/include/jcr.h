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
typedef struct s_tree_root TREE_ROOT;
#endif

#include "lib/alist.h"
#include "lib/volume_session_info.h"

class dlist;

namespace storagedaemon {
struct VolumeList;
class DeviceControlRecord;
class DirectorResource;
struct BootStrapRecord;
}  // namespace storagedaemon

namespace filedaemon {
class BareosAccurateFilelist;
struct save_pkt;
}  // namespace filedaemon

/**
 * Backup/Verify level code. These are stored in the DB
 */
#define L_FULL 'F'         /**< Full backup */
#define L_INCREMENTAL 'I'  /**< Since last backup */
#define L_DIFFERENTIAL 'D' /**< Since last full backup */
#define L_SINCE 'S'
#define L_VERIFY_CATALOG 'C' /**< Verify from catalog */
#define L_VERIFY_INIT 'V'    /**< Verify save (init DB) */
#define L_VERIFY_VOLUME_TO_CATALOG                                         \
  'O'                                /**< Verify Volume to catalog entries \
                                      */
#define L_VERIFY_DISK_TO_CATALOG 'd' /**< Verify Disk attributes to catalog */
#define L_VERIFY_DATA 'A'            /**< Verify data on volume */
#define L_BASE 'B'                   /**< Base level job */
#define L_NONE ' '                   /**< None, for Restore and Admin */
#define L_VIRTUAL_FULL 'f'           /**< Virtual full backup */

/**
 * Job Types. These are stored in the DB
 */
#define JT_BACKUP 'B'       /**< Backup Job */
#define JT_MIGRATED_JOB 'M' /**< A previous backup job that was migrated */
#define JT_VERIFY 'V'       /**< Verify Job */
#define JT_RESTORE 'R'      /**< Restore Job */
#define JT_CONSOLE 'U'      /**< console program */
#define JT_SYSTEM 'I'       /**< internal system "job" */
#define JT_ADMIN 'D'        /**< admin job */
#define JT_ARCHIVE 'A'      /**< Archive Job */
#define JT_JOB_COPY 'C'     /**< Copy of a Job */
#define JT_COPY 'c'         /**< Copy Job */
#define JT_MIGRATE 'g'      /**< Migration Job */
#define JT_SCAN 'S'         /**< Scan Job */
#define JT_CONSOLIDATE 'O'  /**< Always Incremental Consolidate Job */

/**
 * Job Status. Some of these are stored in the DB
 */
#define JS_Canceled 'A'        /**< Canceled by user */
#define JS_Blocked 'B'         /**< Blocked */
#define JS_Created 'C'         /**< Created but not yet running */
#define JS_Differences 'D'     /**< Verify differences */
#define JS_ErrorTerminated 'E' /**< Job terminated in error */
#define JS_WaitFD 'F'          /**< Waiting on File daemon */
#define JS_Incomplete 'I'      /**< Incomplete Job */
#define JS_DataCommitting 'L'  /**< Committing data (last despool) */
#define JS_WaitMount 'M'       /**< Waiting for Mount */
#define JS_Running 'R'         /**< Running */
#define JS_WaitSD 'S'          /**< Waiting on the Storage daemon */
#define JS_Terminated 'T'      /**< Terminated normally */
#define JS_Warnings 'W'        /**< Terminated normally with warnings */

#define JS_AttrDespooling 'a' /**< SD despooling attributes */
#define JS_WaitClientRes 'c'  /**< Waiting for Client resource */
#define JS_WaitMaxJobs 'd'    /**< Waiting for maximum jobs */
#define JS_Error 'e'          /**< Non-fatal error */
#define JS_FatalError 'f'     /**< Fatal error */
#define JS_AttrInserting 'i'  /**< Doing batch insert file records */
#define JS_WaitJobRes 'j'     /**< Waiting for job resource */
#define JS_DataDespooling 'l' /**< Doing data despooling */
#define JS_WaitMedia 'm'      /**< Waiting for new media */
#define JS_WaitPriority 'p'   /**< Waiting for higher priority jobs to finish */
#define JS_WaitDevice 'q'     /**< Queued waiting for device */
#define JS_WaitStoreRes 's'   /**< Waiting for storage resource */
#define JS_WaitStartTime 't'  /**< Waiting for start time */

/**
 * Protocol types
 */
enum
{
  PT_NATIVE = 0,
  PT_NDMP_BAREOS, /* alias for PT_NDMP */
  PT_NDMP_NATIVE
};

/**
 * Authentication Protocol types
 */
enum
{
  APT_NATIVE = 0,
  APT_NDMPV2,
  APT_NDMPV3,
  APT_NDMPV4
};

/**
 * Authentication types
 */
enum
{
  AT_NONE = 0,
  AT_CLEAR,
  AT_MD5,
  AT_VOID
};

/**
 * Migration selection types
 */
enum
{
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
  (jcr->JobStatus == JS_Terminated || jcr->JobStatus == JS_Warnings)

#define JobCanceled(jcr)                                                    \
  (jcr->JobStatus == JS_Canceled || jcr->JobStatus == JS_ErrorTerminated || \
   jcr->JobStatus == JS_FatalError)

#define foreach_jcr(jcr) \
  for (jcr = jcr_walk_start(); jcr; (jcr = jcr_walk_next(jcr)))

#define endeach_jcr(jcr) JcrWalkEnd(jcr)

#define SD_APPEND 1
#define SD_READ 0

/**
 * Forward referenced structures
 */
class JobControlRecord;
class BareosSocket;
class BareosDb;
class htable;

struct AttributesDbRecord;
struct FindFilesPacket;
struct bpContext;
#ifdef HAVE_WIN32
struct CopyThreadContext;
#endif

struct JobControlRecordPrivate;

/* clang-format off */
struct CompressionContext {
  POOLMEM* deflate_buffer{nullptr}; /**< Buffer used for deflation (compression) */
  POOLMEM* inflate_buffer{nullptr}; /**< Buffer used for inflation (decompression) */
  uint32_t deflate_buffer_size{0}; /**< Length of deflation buffer */
  uint32_t inflate_buffer_size{0}; /**< Length of inflation buffer */
  struct {
#ifdef HAVE_LIBZ
    void* pZLIB{nullptr}; /**< ZLIB compression session data */
#endif
#ifdef HAVE_LZO
    void* pLZO{nullptr}; /**< LZO compression session data */
#endif
    void* pZFAST{nullptr}; /**< FASTLZ compression session data */
  } workset;
};
/* clang-format on */

struct job_callback_item {
  void (*JobEndCb)(JobControlRecord* jcr, void*);
  void* ctx = nullptr;
};

typedef void(JCR_free_HANDLER)(JobControlRecord* jcr);

/* clang-format off */
class JobControlRecord {
 private:
  pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER; /**< Jcr mutex */
  volatile int32_t _use_count = 0;                   /**< Use count */
  int32_t JobType_ = 0;            /**< Backup, restore, verify ... */
  int32_t JobLevel_ = 0;           /**< Job level */
  int32_t Protocol_ = 0;           /**< Backup Protocol */
  bool my_thread_killable = false; /**< Can we kill the thread? */
 public:
  JobControlRecord();
  ~JobControlRecord();

  void lock() { P(mutex); }
  void unlock() { V(mutex); }
  void IncUseCount(void)
  {
    lock();
    _use_count++;
    unlock();
  }
  void DecUseCount(void)
  {
    lock();
    _use_count--;
    unlock();
  }
  int32_t UseCount() const { return _use_count; }
  void InitMutex(void) { pthread_mutex_init(&mutex, NULL); }
  void DestroyMutex(void) { pthread_mutex_destroy(&mutex); }
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
  bool NoClientUsed() const
  {
    return (JobType_ == JT_MIGRATE || JobType_ == JT_COPY ||
            JobLevel_ == L_VIRTUAL_FULL);
  }
  bool IsPlugin() const { return (cmd_plugin || opt_plugin); }
  const char* get_OperationName();               /**< in lib/jcr.c */
  const char* get_ActionName(bool past = false); /**< in lib/jcr.c */
  void setJobStatus(int newJobStatus);           /**< in lib/jcr.c */
  void resetJobStatus(int newJobStatus);         /**< in lib/jcr.c */
  bool sendJobStatus();                          /**< in lib/jcr.c */
  bool sendJobStatus(int newJobStatus);          /**< in lib/jcr.c */
  bool JobReads();                               /**< in lib/jcr.c */
  void MyThreadSendSignal(int sig);              /**< in lib/jcr.c */
  void SetKillable(bool killable);               /**< in lib/jcr.c */
  bool IsKillable() const { return my_thread_killable; }

  /*
   * Global part of JobControlRecord common to all daemons
   */
  dlink link;                /**< JobControlRecord chain link */
  pthread_t my_thread_id = 0;  /**< Id of thread controlling jcr */
  BareosSocket* dir_bsock = nullptr; /**< Director bsock or NULL if we are him */
  BareosSocket* store_bsock = nullptr; /**< Storage connection socket */
  BareosSocket* file_bsock;  /**< File daemon connection socket */
  JCR_free_HANDLER* daemon_free_jcr = nullptr; /**< Local free routine */
  dlist* msg_queue = nullptr;     /**< Queued messages */
  pthread_mutex_t msg_queue_mutex = PTHREAD_MUTEX_INITIALIZER; /**< message queue mutex */
  bool dequeuing_msgs = false;    /**< Set when dequeuing messages */
  alist job_end_callbacks;           /**< callbacks called at Job end */
  POOLMEM* VolumeName = nullptr;  /**< Volume name desired -- pool_memory */
  POOLMEM* errmsg = nullptr;      /**< Edited error message */
  char Job[MAX_NAME_LENGTH]{0};   /**< Unique name of this Job */

  uint32_t JobId = 0; /**< Director's JobId */
  uint32_t VolSessionId = 0;
  uint32_t VolSessionTime = 0;
  uint32_t JobFiles = 0;          /**< Number of files written, this job */
  uint32_t JobErrors = 0;         /**< Number of non-fatal errors this job */
  uint32_t JobWarnings = 0;       /**< Number of warning messages */
  uint32_t LastRate = 0;          /**< Last sample bytes/sec */
  uint64_t JobBytes = 0;          /**< Number of bytes processed this job */
  uint64_t LastJobBytes = 0;      /**< Last sample number bytes */
  uint64_t ReadBytes = 0;         /**< Bytes read -- before compression */
  FileId_t FileId = 0;            /**< Last FileId used */
  volatile int32_t JobStatus = 0; /**< ready, running, blocked, terminated */
  int32_t JobPriority = 0;        /**< Job priority */
  time_t sched_time = 0;          /**< Job schedule time, i.e. when it should start */
  time_t initial_sched_time = 0;  /**< Original sched time before any reschedules are done */
  time_t start_time = 0;          /**< When job actually started */
  time_t run_time = 0;            /**< Used for computing speed */
  time_t last_time = 0;           /**< Last sample time */
  time_t end_time = 0;            /**< Job end time */
  time_t wait_time_sum = 0;       /**< Cumulative wait time since job start */
  time_t wait_time = 0;           /**< Timestamp when job have started to wait */
  time_t job_started_time = 0;    /**< Time when the MaxRunTime start to count */
  POOLMEM* client_name = nullptr; /**< Client name */
  POOLMEM* JobIds = nullptr;      /**< User entered string of JobIds */
  POOLMEM* RestoreBootstrap = nullptr; /**< Bootstrap file to restore */
  POOLMEM* stime = nullptr;    /**< start time for incremental/differential */
  char* sd_auth_key = nullptr; /**< SD auth key */
  TlsPolicy sd_tls_policy;   /**< SD Tls Policy */
  MessagesResource* jcr_msgs = nullptr; /**< Copy of message resource -- actually used */
  uint32_t ClientId = 0;      /**< Client associated with Job */
  char* where = nullptr;      /**< Prefix to restore files to */
  char* RegexWhere = nullptr; /**< File relocation in restore */
  alist* where_bregexp = nullptr; /**< BareosRegex alist for path manipulation */
  int32_t cached_pnl = 0; /**< Cached path length */
  POOLMEM* cached_path = nullptr; /**< Cached path */
  bool passive_client = false;    /**< Client is a passive client e.g. doesn't initiate any network connection */
  bool prefix_links = false;      /**< Prefix links with Where path */
  bool gui = false;               /**< Set if gui using console */
  bool authenticated = false;     /**< Set when client authenticated */
  bool cached_attribute = false;  /**< Set if attribute is cached */
  bool batch_started = false;     /**< Set if batch mode started */
  bool cmd_plugin = false;        /**< Set when processing a command Plugin = */
  bool opt_plugin = false;        /**< Set when processing an option Plugin = */
  bool keep_path_list = false;    /**< Keep newly created path in a hash */
  bool accurate = false;          /**< True if job is accurate */
  bool HasBase = false;           /**< True if job use base jobs */
  bool rerunning = false;         /**< Rerunning an incomplete job */
  bool job_started = false;       /**< Set when the job is actually started */
  bool suppress_output = false;   /**< Set if this JobControlRecord should not output any Jmsgs */
  JobControlRecord* cjcr = nullptr; /**< Controlling JobControlRecord when this
                                     * is a slave JobControlRecord being
                                     * controlled by another JobControlRecord
                                     * used for sending normal and fatal errors.
                           */
  int32_t buf_size = 0;             /**< Length of buffer */
  CompressionContext compress; /**< Compression ctx */
#ifdef HAVE_WIN32
  CopyThreadContext* cp_thread = nullptr; /**< Copy Thread ctx */
#endif
  POOLMEM* attr = nullptr;      /**< Attribute string from SD */
  BareosDb* db = nullptr;       /**< database pointer */
  BareosDb* db_batch = nullptr; /**< database pointer for batch and accurate */
  uint64_t nb_base_files = 0;   /**< Number of base files */
  uint64_t nb_base_files_used = 0; /**< Number of useful files in base */

  AttributesDbRecord* ar = nullptr; /**< DB attribute record */
  guid_list* id_list = nullptr;     /**< User/group id to name list */

  alist* plugin_ctx_list = nullptr; /**< List of contexts for plugins */
  bpContext* plugin_ctx = nullptr;  /**< Current plugin context */
  POOLMEM* comment = nullptr;       /**< Comment for this Job */
  int64_t max_bandwidth = 0;        /**< Bandwidth limit for this Job */
  htable* path_list = nullptr;      /**< Directory list (used by findlib) */
  bool is_passive_client_connection_probing = false; /**< Set if director probes a passive client connection */

  JobControlRecordPrivate* impl_;

#ifdef STORAGE_DAEMON
  /*
   * Storage Daemon specific part of JobControlRecord
   */
  JobControlRecord* next_dev = nullptr; /**< Next JobControlRecord attached to device */
  JobControlRecord* prev_dev = nullptr; /**< Previous JobControlRecord attached to device */
  char* dir_auth_key = nullptr; /**< Dir auth key */
  pthread_cond_t job_start_wait = PTHREAD_COND_INITIALIZER; /**< Wait for FD to start Job */
  pthread_cond_t job_end_wait = PTHREAD_COND_INITIALIZER; /**< Wait for Job to end */
  int32_t type = 0;
  storagedaemon::DeviceControlRecord* read_dcr = nullptr; /**< Device context for reading */
  storagedaemon::DeviceControlRecord* dcr = nullptr;      /**< Device context record */
  alist* dcrs = nullptr;            /**< List of dcrs open */
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
  int32_t CurVol = 0;               /**< Current Volume count */
  storagedaemon::DirectorResource* director = nullptr; /**< Director resource */
  alist* plugin_options = nullptr;  /**< Specific Plugin Options sent by DIR */
  alist* write_store = nullptr;     /**< List of write storage devices sent by DIR */
  alist* read_store = nullptr;      /**< List of read devices sent by DIR */
  alist* reserve_msgs = nullptr;    /**< Reserve fail messages */
  bool acquired_storage = false;    /**< Did we acquire our reserved storage already or not */
  bool PreferMountedVols = false;   /**< Prefer mounted vols rather than new */
  bool Resched = false;             /**< Job may be rescheduled */
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
#endif /* STORAGE_DAEMON */
};
/* clang-format on */

/*
 * The following routines are found in lib/jcr.c
 */
extern int GetNextJobidFromList(char** p, uint32_t* JobId);
extern bool InitJcrSubsystem(int timeout);
extern JobControlRecord* new_jcr(int size, JCR_free_HANDLER* daemon_free_jcr);
extern JobControlRecord* get_jcr_by_id(uint32_t JobId);
extern JobControlRecord* get_jcr_by_session(uint32_t SessionId,
                                            uint32_t SessionTime);
extern JobControlRecord* get_jcr_by_partial_name(char* Job);
extern JobControlRecord* get_jcr_by_full_name(char* Job);
extern const char* JcrGetAuthenticateKey(const char* unified_job_name);
TlsPolicy JcrGetTlsPolicy(const char* unified_job_name);
extern int num_jobs_run;

extern void b_free_jcr(const char* file, int line, JobControlRecord* jcr);
#define FreeJcr(jcr) b_free_jcr(__FILE__, __LINE__, (jcr))

/*
 * Used to display specific job information after a fatal signal
 */
typedef void(dbg_jcr_hook_t)(JobControlRecord* jcr, FILE* fp);
extern void DbgJcrAddHook(dbg_jcr_hook_t* fct);

/* new-2019 interface */
void InitJcr(std::shared_ptr<JobControlRecord> jcr,
             JCR_free_HANDLER* daemon_free_jcr);
std::size_t GetJcrCount();
std::shared_ptr<JobControlRecord> GetJcrById(uint32_t JobId);
std::shared_ptr<JobControlRecord> GetJcrByFullName(std::string name);
std::shared_ptr<JobControlRecord> GetJcrByPartialName(std::string name);
std::shared_ptr<JobControlRecord> GetJcrBySession(const VolumeSessionInfo& vsi);
uint32_t GetJobIdByThreadId(pthread_t tid);
/* ************* */


#endif /** BAREOS_INCLUDE_JCR_H_ */
