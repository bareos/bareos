/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2012 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2026 Bareos GmbH & Co. KG

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
#define BAREOS_INCLUDE_JCR_H_

#include "include/bareos.h"
#include "include/compression_context.h"
#include "include/job_level.h"
#include "include/job_status.h"
#include "include/job_types.h"
#include "lib/message_queue_item.h"
#include "lib/alist.h"
#include "lib/tls_conf.h"
#include "lib/breg.h"
#include "lib/dlink.h"
#include "lib/path_list.h"
#include "lib/guid_to_name.h"
#include "lib/jcr.h"

#include <atomic>

struct job_callback_item;
class BareosDb;
class BareosSocket;
template <typename T> class dlist;
class JobControlRecord;

struct AttributesDbRecord;
struct PluginContext;
struct DirectorJcrImpl;
struct StoredJcrImpl;
struct FiledJcrImpl;
struct VolumeSessionInfo;

#ifdef HAVE_WIN32
struct CopyThreadContext;
#endif

typedef void(JCR_free_HANDLER)(JobControlRecord* jcr);

#define foreach_jcr(jcr) \
  for (jcr = jcr_walk_start(); jcr; (jcr = jcr_walk_next(jcr)))

#define endeach_jcr(jcr) JcrWalkEnd(jcr)

/* clang-format off */
class JobControlRecord {
 private:
  std::mutex mutex_; /**< Jcr mutex */
  std::atomic<int32_t> use_count_{};                   /**< Use count */
  std::atomic<int32_t> JobStatus_{}; /**< ready, running, blocked, terminated */
  int32_t JobType_{};            /**< Backup, restore, verify ... */
  int32_t JobLevel_{};           /**< Job level */
  int32_t Protocol_{};           /**< Backup Protocol */
  bool my_thread_killable_{};
  enum class cancel_status : int8_t {
    None,
    InProcess,
    Finished,
  };
  std::atomic<cancel_status> canceled_status{cancel_status::None};
 public:
  /* tells the jcr that it will get canceled soon.  This can happen only once.
   * Returns true if you may cancel it (in that case you _must_ call
   * CancelFinished() when done); otherwise false is returned (in which case
   * you _may not_ call CancelFinished()) */
  bool PrepareCancel();
  void CancelFinished();

  /* Once called, this function stops other threads from canceling this job;
   * If this job is currently getting canceled, then it wait until
   * that is done, i.e. until CancelFinished() is called. */
  void EnterFinish();

  JobControlRecord();
  ~JobControlRecord();
  JobControlRecord(const JobControlRecord &other) = delete;
  JobControlRecord(const JobControlRecord &&other) = delete;
  JobControlRecord& operator=(const JobControlRecord& other) = delete;
  JobControlRecord& operator=(const JobControlRecord&& other) = delete;

  [[nodiscard]] std::mutex& mutex_guard() { return mutex_; }

  void IncUseCount(void)
  {
    ++use_count_;
  }
  void DecUseCount(void)
  {
    --use_count_;
  }
  int32_t UseCount() const { return use_count_; }
  bool IsJobCanceled() { return  JobStatus_ == JS_Canceled
                              || JobStatus_ == JS_ErrorTerminated
                              || JobStatus_ == JS_FatalError; }

  bool IsTerminatedOk() { return JobStatus_ == JS_Terminated || JobStatus_ == JS_Warnings; }
  bool IsIncomplete() { return JobStatus_ == JS_Incomplete; }
  bool is_JobLevel(int32_t JobLevel) { return JobLevel == JobLevel_; }
  bool is_JobType(int32_t JobType) { return JobType == JobType_; }
  bool is_JobStatus(int32_t aJobStatus) { return aJobStatus == JobStatus_; }
  void setJobLevel(int32_t JobLevel) { JobLevel_ = JobLevel; }
  void setJobType(int32_t JobType) { JobType_ = JobType; }
  void setJobProtocol(int32_t JobProtocol) { Protocol_ = JobProtocol; }
  void setJobStatus(int32_t aJobStatus) { JobStatus_ = aJobStatus; }
  void setJobStarted();
  int32_t getJobType() const { return JobType_; }
  int32_t getJobLevel() const { return JobLevel_; }
  int32_t getJobStatus() const { return JobStatus_; }
  int32_t getJobProtocol() const { return Protocol_; }
  bool NoClientUsed() const
  {
    return (JobType_ == JT_MIGRATE || JobType_ == JT_COPY ||
            JobLevel_ == L_VIRTUAL_FULL);
  }
  bool IsPlugin() const { return (cmd_plugin || opt_plugin); }
  const char* get_OperationName();               /**< in lib/jcr.c */
  const char* get_ActionName(bool past = false); /**< in lib/jcr.c */
  void setJobStatusWithPriorityCheck(int newJobStatus); /**< in lib/jcr.c */
  bool sendJobStatus();                          /**< in lib/jcr.c */
  bool sendJobStatus(int newJobStatus);          /**< in lib/jcr.c */
  bool JobReads();                               /**< in lib/jcr.c */
  void MyThreadSendSignal(int sig);              /**< in lib/jcr.c */
  void SetKillable(bool killable);               /**< in lib/jcr.c */
  bool IsKillable() const { return my_thread_killable_; }
  void UpdateJobStats();

  dlink<JobControlRecord> link;                     /**< JobControlRecord chain link */
  pthread_t my_thread_id{};       /**< Id of thread controlling jcr */
  BareosSocket* dir_bsock{};      /**< Director bsock or NULL if we are him */
  BareosSocket* store_bsock{};    /**< Storage connection socket */
  BareosSocket* file_bsock{};     /**< File daemon connection socket */
  JCR_free_HANDLER* daemon_free_jcr{}; /**< Local free routine */
  dlist<MessageQueueItem>* msg_queue{};             /**< Queued messages */
  pthread_mutex_t msg_queue_mutex = PTHREAD_MUTEX_INITIALIZER; /**< message queue mutex */
  bool dequeuing_msgs{};          /**< Set when dequeuing messages */
  alist<job_callback_item*> job_end_callbacks;        /**< callbacks called at Job end */
  POOLMEM* VolumeName{};          /**< Volume name desired -- pool_memory */
  POOLMEM* errmsg{};              /**< Edited error message */
  char Job[MAX_NAME_LENGTH]{};    /**< Unique name of this Job */

  uint32_t JobId{};             /**< Director's JobId */
  uint32_t VolSessionId{};
  uint32_t VolSessionTime{};
  uint32_t JobFiles{};          /**< Number of files written, this job */
  uint32_t JobErrors{};         /**< Number of non-fatal errors this job */
  uint32_t JobWarnings{};       /**< Number of warning messages */
  uint32_t AverageRate{};       /**< Last average bytes/sec */
  uint32_t LastRate{};            /**< Last sample bytes/sec */
  uint64_t JobBytes{};          /**< Number of bytes processed this job */
  uint64_t LastJobBytes{};      /**< Last sample number bytes */
  uint64_t ReadBytes{};         /**< Bytes read -- before compression */
  FileId_t FileId{};            /**< Last FileId used */
  int32_t JobPriority{};        /**< Job priority */
  bool allow_mixed_priority{};  /**< Allow jobs with higher priority concurrently with this */
  time_t sched_time{};          /**< Job schedule time, i.e. when it should start */
  time_t initial_sched_time{};  /**< Original sched time before any reschedules are done */
  time_t start_time{};          /**< When job actually started */
  time_t run_time{};            /**< Used for computing speed */
  time_t last_time{};           /**< Last sample time */
  time_t end_time{};            /**< Job end time */
  time_t wait_time_sum{};       /**< Cumulative wait time since job start */
  time_t wait_time{};           /**< Timestamp when job have started to wait */
  time_t job_started_time{};    /**< Time when the MaxRunTime start to count */
  POOLMEM* client_name{};       /**< Client name */
  POOLMEM* JobIds{};            /**< User entered string of JobIds */
  POOLMEM* RestoreBootstrap{};  /**< Bootstrap file to restore */
  POOLMEM* starttime_string{};  /**< start time for incremental/differential
                                  as string "yyyy-mm-dd hh:mm:ss" */
  char* sd_auth_key{};          /**< SD auth key */
  TlsPolicy sd_tls_policy{kBnetTlsNone};      /**< SD Tls Policy */
  MessagesResource* jcr_msgs{}; /**< Copy of message resource -- actually used */
  uint32_t ClientId{};          /**< Client associated with Job */
  char* where{};                /**< Prefix to restore files to */
  char* RegexWhere{};           /**< File relocation in restore */
  alist<BareosRegex*>* where_bregexp{};       /**< BareosRegex alist for path manipulation */
  int32_t cached_pnl{};         /**< Cached path length */
  POOLMEM* cached_path{};   /**< Cached path */
  bool passive_client{};    /**< Client is a passive client e.g. doesn't initiate any network connection */
  bool prefix_links{};      /**< Prefix links with Where path */
  bool gui{};               /**< Set if gui using console */
  bool authenticated{};     /**< Set when client authenticated */
  bool cached_attribute{};  /**< Set if attribute is cached */
  bool batch_started{};     /**< Set if batch mode started */
  bool cmd_plugin{};        /**< Set when processing a command Plugin = */
  bool opt_plugin{};        /**< Set when processing an option Plugin = */
  bool keep_path_list{};    /**< Keep newly created path in a hash */
  bool accurate{};          /**< True if job is accurate */
  bool rerunning{};         /**< Rerunning an incomplete job */
  bool job_started{};       /**< Set when the job is actually started */
  bool suppress_output{};   /**< Set if this JobControlRecord should not output any Jmsgs */
  JobControlRecord* cjcr{}; /**< Controlling JobControlRecord when this
                                 is a slave JobControlRecord being
                                 controlled by another JobControlRecord
                                 used for sending normal and fatal errors. */
  int32_t buf_size{};          /**< Length of buffer */
  CompressionContext compress; /**< Compression ctx */
#ifdef HAVE_WIN32
  CopyThreadContext* cp_thread{}; /**< Copy Thread ctx */
#endif
  POOLMEM* attr{};      /**< Attribute string from SD */
  BareosDb* db{};       /**< database pointer */
  BareosDb* db_batch{}; /**< database pointer for batch and accurate */
  uint64_t nb_base_files{};       /**< Number of base files */
  uint64_t nb_base_files_used{};  /**< Number of useful files in base */

  AttributesDbRecord* ar{}; /**< DB attribute record */
  guid_list* id_list{};     /**< User/group id to name list */

  alist<PluginContext*>* plugin_ctx_list{}; /**< List of contexts for plugins */
  PluginContext* plugin_ctx{};  /**< Current plugin context */
  POOLMEM* comment{};       /**< Comment for this Job */
  int64_t max_bandwidth{};  /**< Bandwidth limit for this Job */
  PathList* path_list{};      /**< Directory list (used by findlib) */
  bool is_passive_client_connection_probing{}; /**< Set if director probes a passive client connection */

  union {
    DirectorJcrImpl* dir_impl;
    StoredJcrImpl* sd_impl;
    FiledJcrImpl* fd_impl;
  };

};
/* clang-format on */

// The following routines are found in lib/jcr.c
BAREOS_IMPORT int GetNextJobidFromList(const char** p, uint32_t* JobId);
BAREOS_IMPORT bool InitJcrSubsystem(int timeout);
BAREOS_IMPORT JobControlRecord* new_jcr(JCR_free_HANDLER* daemon_free_jcr);
BAREOS_IMPORT void register_jcr(JobControlRecord* jcr);
BAREOS_IMPORT JobControlRecord* get_jcr_by_id(uint32_t JobId);
BAREOS_IMPORT JobControlRecord* get_jcr_by_session(uint32_t SessionId,
                                                   uint32_t SessionTime);
BAREOS_IMPORT JobControlRecord* get_jcr_by_partial_name(char* Job);
BAREOS_IMPORT JobControlRecord* get_jcr_by_full_name(char* Job);
BAREOS_IMPORT const char* JcrGetAuthenticateKey(const char* unified_job_name);
TlsPolicy JcrGetTlsPolicy(const char* unified_job_name);
BAREOS_IMPORT std::size_t NumJobsRun();

BAREOS_IMPORT void b_free_jcr(const char* file,
                              int line,
                              JobControlRecord* jcr);
BAREOS_IMPORT void b_free_jcr(const char* file,
                              int line,
                              JobControlRecord* jcr);
#define FreeJcr(jcr) b_free_jcr(__FILE__, __LINE__, (jcr))

// Used to display specific job information after a fatal signal
typedef void(dbg_jcr_hook_t)(JobControlRecord* jcr, FILE* fp);
BAREOS_IMPORT void DbgJcrAddHook(dbg_jcr_hook_t* fct);

/* new-2019 interface */
void InitJcr(std::shared_ptr<JobControlRecord> jcr,
             JCR_free_HANDLER* daemon_free_jcr);
std::size_t GetJcrCount();
std::shared_ptr<JobControlRecord> GetJcrById(uint32_t JobId);
std::shared_ptr<JobControlRecord> GetJcrByFullName(std::string_view name);
std::shared_ptr<JobControlRecord> GetJcrByPartialName(std::string_view name);
std::shared_ptr<JobControlRecord> GetJcrBySession(VolumeSessionInfo vsi);
uint32_t GetJobIdByThreadId(pthread_t tid);
/* ************* */


#endif  // BAREOS_INCLUDE_JCR_H_
