/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2012 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2020 Bareos GmbH & Co. KG

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
 * Manipulation routines for Job Control Records and
 *  handling of last_jobs_list.
 *
 *  Kern E. Sibbald, December 2000
 *
 *  These routines are thread safe.
 *
 *  The job list routines were re-written in May 2005 to
 *  eliminate the global lock while traversing the list, and
 *  to use the dlist subroutines.  The locking is now done
 *  on the list each time the list is modified or traversed.
 *  That is it is "micro-locked" rather than globally locked.
 *  The result is that there is one lock/unlock for each entry
 *  in the list while traversing it rather than a single lock
 *  at the beginning of a traversal and one at the end.  This
 *  incurs slightly more overhead, but effectively eliminates
 *  the possibilty of race conditions.  In addition, with the
 *  exception of the global locking of the list during the
 *  re-reading of the config file, no recursion is needed.
 *
 */

#include "include/bareos.h"
#include "include/jcr.h"
#include "lib/berrno.h"
#include "lib/bsignal.h"
#include "lib/breg.h"
#include "lib/edit.h"
#include "lib/thread_specific_data.h"
#include "lib/tls_conf.h"
#include "lib/bsock.h"
#include "lib/recent_job_results_list.h"
#include "lib/message_queue_item.h"
#include "lib/volume_session_info.h"
#include "lib/watchdog.h"

#include <algorithm>

const int debuglevel = 3400;

static void JcrTimeoutCheck(watchdog_t* self);

int num_jobs_run;

static std::vector<std::weak_ptr<JobControlRecord>> job_control_record_cache;
static dlist* job_control_record_chain = nullptr;
static int watch_dog_timeout = 0;

static std::mutex jcr_chain_mutex;
static pthread_mutex_t job_start_mutex = PTHREAD_MUTEX_INITIALIZER;

static char Job_status[] = "Status Job=%s JobStatus=%d\n";

void LockJobs() { P(job_start_mutex); }

void UnlockJobs() { V(job_start_mutex); }

/*
 * Get an ASCII representation of the Operation being performed as an english
 * Noun
 */
const char* JobControlRecord::get_OperationName()
{
  switch (JobType_) {
    case JT_BACKUP:
      return _("Backup");
    case JT_VERIFY:
      return _("Verifying");
    case JT_RESTORE:
      return _("Restoring");
    case JT_ARCHIVE:
      return _("Archiving");
    case JT_COPY:
      return _("Copying");
    case JT_MIGRATE:
      return _("Migration");
    case JT_SCAN:
      return _("Scanning");
    case JT_CONSOLIDATE:
      return _("Consolidating");
    default:
      return _("Unknown operation");
  }
}

/*
 * Get an ASCII representation of the Action being performed either an english
 * Verb or Adjective
 */
const char* JobControlRecord::get_ActionName(bool past)
{
  switch (JobType_) {
    case JT_BACKUP:
      return _("backup");
    case JT_VERIFY:
      return (past) ? _("verified") : _("verify");
    case JT_RESTORE:
      return (past) ? _("restored") : _("restore");
    case JT_ARCHIVE:
      return (past) ? _("archived") : _("archive");
    case JT_COPY:
      return (past) ? _("copied") : _("copy");
    case JT_MIGRATE:
      return (past) ? _("migrated") : _("migrate");
    case JT_SCAN:
      return (past) ? _("scanned") : _("scan");
    case JT_CONSOLIDATE:
      return (past) ? _("consolidated") : _("consolidate");
    default:
      return _("unknown action");
  }
}

bool JobControlRecord::JobReads()
{
  switch (JobType_) {
    case JT_VERIFY:
    case JT_RESTORE:
    case JT_COPY:
    case JT_MIGRATE:
      return true;
    case JT_BACKUP:
      if (JobLevel_ == L_VIRTUAL_FULL) { return true; }
      break;
    default:
      break;
  }
  return false;
}

struct job_callback_item {
  void (*JobEndCb)(JobControlRecord* jcr, void*);
  void* ctx{};
};

/*
 * Push a job_callback_item onto the job end callback stack.
 */
void RegisterJobEndCallback(JobControlRecord* jcr,
                            void JobEndCb(JobControlRecord* jcr, void*),
                            void* ctx)
{
  job_callback_item* item;

  item = (job_callback_item*)malloc(sizeof(job_callback_item));

  item->JobEndCb = JobEndCb;
  item->ctx = ctx;

  jcr->job_end_callbacks.push((void*)item);
}

/*
 * Pop each job_callback_item and process it.
 */
static void CallJobEndCallbacks(JobControlRecord* jcr)
{
  job_callback_item* item;

  if (jcr->job_end_callbacks.size() > 0) {
    item = (job_callback_item*)jcr->job_end_callbacks.pop();
    while (item) {
      item->JobEndCb(jcr, item->ctx);
      free(item);
      item = (job_callback_item*)jcr->job_end_callbacks.pop();
    }
  }
}

JobControlRecord::JobControlRecord()
{
  Dmsg0(100, "Construct JobControlRecord\n");

  MessageQueueItem* item = nullptr;
  msg_queue = new dlist(item, &item->link_);  // calculate offset

  int status;
  if ((status = pthread_mutex_init(&msg_queue_mutex, nullptr)) != 0) {
    BErrNo be;
    Jmsg(nullptr, M_ABORT, 0, _("Could not init msg_queue mutex. ERR=%s\n"),
         be.bstrerror(status));
  }

  my_thread_id = pthread_self();
  job_end_callbacks.init(1, false);
  sched_time = time(nullptr);
  initial_sched_time = sched_time;
  InitMutex();
  IncUseCount();
  VolumeName = GetPoolMemory(PM_FNAME);
  VolumeName[0] = 0;
  errmsg = GetPoolMemory(PM_MESSAGE);
  errmsg[0] = 0;
  comment = GetPoolMemory(PM_FNAME);
  comment[0] = 0;

  /*
   * Setup some dummy values
   */
  bstrncpy(Job, "*System*", sizeof(Job));
  JobId = 0;
  setJobType(JT_SYSTEM); /* internal job until defined */
  setJobLevel(L_NONE);
  setJobStatus(JS_Created); /* ready to run */
  SetTimeoutHandler();
}

JobControlRecord* new_jcr(JCR_free_HANDLER* daemon_free_jcr)
{
  Dmsg0(debuglevel, "Enter new_jcr\n");

  JobControlRecord* jcr =
      static_cast<JobControlRecord*>(malloc(sizeof(JobControlRecord)));
  jcr = new (jcr) JobControlRecord();

  jcr->daemon_free_jcr = daemon_free_jcr;

  LockJobs();
  LockJcrChain();
  InitJcrChain();
  job_control_record_chain->append(jcr);
  UnlockJcrChain();
  UnlockJobs();
  return jcr;
}

void InitJcr(std::shared_ptr<JobControlRecord> jcr,
             JCR_free_HANDLER* daemon_free_jcr)
{
  jcr->daemon_free_jcr = daemon_free_jcr;

  LockJobs();
  LockJcrChain();
  job_control_record_cache.emplace_back(jcr);
  UnlockJcrChain();
  UnlockJobs();
}

/*
 * Remove a JobControlRecord from the chain
 *
 * NOTE! The chain must be locked prior to calling this routine.
 */
static void RemoveJcr(JobControlRecord* jcr)
{
  Dmsg0(debuglevel, "Enter RemoveJcr\n");
  if (!jcr) { Emsg0(M_ABORT, 0, _("nullptr jcr.\n")); }
  job_control_record_chain->remove(jcr);
  Dmsg0(debuglevel, "Leave RemoveJcr\n");
}

static void FreeCommonJcr(JobControlRecord* jcr,
                          bool is_destructor_call = false)
{
  Dmsg1(100, "FreeCommonJcr: %p \n", jcr);

  if (!jcr) { Dmsg0(100, "FreeCommonJcr: Invalid jcr\n"); }

  RemoveJcrFromThreadSpecificData(jcr);
  jcr->SetKillable(false);

  jcr->DestroyMutex();

  if (jcr->msg_queue) {
    delete jcr->msg_queue;
    jcr->msg_queue = nullptr;
    pthread_mutex_destroy(&jcr->msg_queue_mutex);
  }

  if (jcr->client_name) {
    FreePoolMemory(jcr->client_name);
    jcr->client_name = nullptr;
  }

  if (jcr->attr) {
    FreePoolMemory(jcr->attr);
    jcr->attr = nullptr;
  }

  if (jcr->sd_auth_key) {
    free(jcr->sd_auth_key);
    jcr->sd_auth_key = nullptr;
  }

  if (jcr->VolumeName) {
    FreePoolMemory(jcr->VolumeName);
    jcr->VolumeName = nullptr;
  }

  if (jcr->dir_bsock) {
    jcr->dir_bsock->close();
    delete jcr->dir_bsock;
    jcr->dir_bsock = nullptr;
  }

  if (jcr->errmsg) {
    FreePoolMemory(jcr->errmsg);
    jcr->errmsg = nullptr;
  }

  if (jcr->where) {
    free(jcr->where);
    jcr->where = nullptr;
  }

  if (jcr->RegexWhere) {
    free(jcr->RegexWhere);
    jcr->RegexWhere = nullptr;
  }

  if (jcr->where_bregexp) {
    FreeBregexps(jcr->where_bregexp);
    delete jcr->where_bregexp;
    jcr->where_bregexp = nullptr;
  }

  if (jcr->cached_path) {
    FreePoolMemory(jcr->cached_path);
    jcr->cached_path = nullptr;
    jcr->cached_pnl = 0;
  }

  if (jcr->id_list) {
    FreeGuidList(jcr->id_list);
    jcr->id_list = nullptr;
  }

  if (jcr->comment) {
    FreePoolMemory(jcr->comment);
    jcr->comment = nullptr;
  }

  if (!is_destructor_call) { free(jcr); }
}

static void JcrCleanup(JobControlRecord* jcr, bool is_destructor_call = false)
{
  DequeueMessages(jcr);
  CallJobEndCallbacks(jcr);

  Dmsg1(debuglevel, "End job=%d\n", jcr->JobId);

  switch (jcr->getJobType()) {
    case JT_BACKUP:
    case JT_VERIFY:
    case JT_RESTORE:
    case JT_MIGRATE:
    case JT_COPY:
    case JT_ADMIN:
      if (jcr->JobId > 0) {  // except Console Jobs
        num_jobs_run++;
        RecentJobResultsList::Append(jcr);
      }
      break;
    default:
      break;
  }

  CloseMsg(jcr);

  if (jcr->daemon_free_jcr) { jcr->daemon_free_jcr(jcr); }

  FreeCommonJcr(jcr, is_destructor_call);
  CloseMsg(nullptr);  // flush any daemon messages
}

JobControlRecord::~JobControlRecord()
{
  Dmsg0(100, "Destruct JobControlRecord\n");
  JcrCleanup(this, true);
  Dmsg0(debuglevel, "JobControlRecord Destructor finished\n");
}

static bool RunJcrGarbageCollector(JobControlRecord* jcr)
{
  LockJcrChain();
  jcr->DecUseCount(); /* decrement use count */
  if (jcr->UseCount() < 0) {
    Jmsg2(jcr, M_ERROR, 0, _("JobControlRecord UseCount=%d JobId=%d\n"),
          jcr->UseCount(), jcr->JobId);
  }
  if (jcr->JobId > 0) {
    Dmsg3(debuglevel, "Dec FreeJcr jid=%u UseCount=%d Job=%s\n", jcr->JobId,
          jcr->UseCount(), jcr->Job);
  }
  if (jcr->UseCount() > 0) { /* if in use */
    UnlockJcrChain();
    return false;
  }
  if (jcr->JobId > 0) {
    Dmsg3(debuglevel, "remove jcr jid=%u UseCount=%d Job=%s\n", jcr->JobId,
          jcr->UseCount(), jcr->Job);
  }
  RemoveJcr(jcr); /* remove Jcr from chain */
  UnlockJcrChain();
  return true;
}

/*
 * Global routine to free a jcr
 */
void b_free_jcr(const char* file, int line, JobControlRecord* jcr)
{
  Dmsg3(debuglevel, "Enter FreeJcr jid=%u from %s:%d\n", jcr->JobId, file,
        line);

  if (RunJcrGarbageCollector(jcr)) { JcrCleanup(jcr); }

  Dmsg0(debuglevel, "Exit FreeJcr\n");
}

void JobControlRecord::SetKillable(bool killable)
{
  lock();

  my_thread_killable = killable;
  if (killable) {
    my_thread_id = pthread_self();
  } else {
    memset(&my_thread_id, 0, sizeof(my_thread_id));
  }

  unlock();
}

void JobControlRecord::MyThreadSendSignal(int sig)
{
  lock();

  if (IsKillable() && !pthread_equal(my_thread_id, pthread_self())) {
    Dmsg1(800, "Send kill to jid=%d\n", JobId);
    pthread_kill(my_thread_id, sig);
  } else if (!IsKillable()) {
    Dmsg1(10, "Warning, can't send kill to jid=%d\n", JobId);
  }

  unlock();
}


/*
 * Given a JobId, find the JobControlRecord
 *
 * Returns: jcr on success
 *          nullptr on failure
 */
JobControlRecord* get_jcr_by_id(uint32_t JobId)
{
  JobControlRecord* jcr;

  foreach_jcr (jcr) {
    if (jcr->JobId == JobId) {
      jcr->IncUseCount();
      Dmsg3(debuglevel, "Inc get_jcr jid=%u UseCount=%d Job=%s\n", jcr->JobId,
            jcr->UseCount(), jcr->Job);
      break;
    }
  }
  endeach_jcr(jcr);

  return jcr;
}

std::size_t GetJcrCount()
{
  LockJcrChain();
  std::size_t count =
      count_if(job_control_record_cache.begin(), job_control_record_cache.end(),
               [](std::weak_ptr<JobControlRecord>& p) { return !p.expired(); });
  UnlockJcrChain();

  return count;
}

static std::shared_ptr<JobControlRecord> GetJcr(
    std::function<bool(const JobControlRecord*)> compare)
{
  std::shared_ptr<JobControlRecord> result;

  LockJcrChain();

  // cleanup chache
  job_control_record_cache.erase(
      std::remove_if(
          job_control_record_cache.begin(), job_control_record_cache.end(),
          [](std::weak_ptr<JobControlRecord>& p) { return p.expired(); }),
      job_control_record_cache.end());

  find_if(job_control_record_cache.begin(), job_control_record_cache.end(),
          [&compare, &result](std::weak_ptr<JobControlRecord>& p) {
            auto jcr = p.lock();
            if (compare(jcr.get())) {
              result = jcr;
              return true;
            }
            return false;
          });

  UnlockJcrChain();

  return result;
}

std::shared_ptr<JobControlRecord> GetJcrById(uint32_t JobId)
{
  return GetJcr(
      [JobId](const JobControlRecord* jcr) { return jcr->JobId == JobId; });
}

std::shared_ptr<JobControlRecord> GetJcrByFullName(std::string name)
{
  return GetJcr([&name](const JobControlRecord* jcr) {
    return std::string{jcr->Job} == name;
  });
}

std::shared_ptr<JobControlRecord> GetJcrByPartialName(std::string name)
{
  return GetJcr([&name](const JobControlRecord* jcr) {
    return std::string{jcr->Job}.find(name) == 0;
  });
}

std::shared_ptr<JobControlRecord> GetJcrBySession(const VolumeSessionInfo& vsi)
{
  return GetJcr([&vsi](const JobControlRecord* jcr) {
    return (VolumeSessionInfo{jcr->VolSessionId, jcr->VolSessionTime} == vsi);
  });
}

/*
 * Given a SessionId and SessionTime, find the JobControlRecord
 *
 * Returns: jcr on success
 *          nullptr on failure
 */
JobControlRecord* get_jcr_by_session(uint32_t SessionId, uint32_t SessionTime)
{
  JobControlRecord* jcr;

  foreach_jcr (jcr) {
    if (jcr->VolSessionId == SessionId && jcr->VolSessionTime == SessionTime) {
      jcr->IncUseCount();
      Dmsg3(debuglevel, "Inc get_jcr jid=%u UseCount=%d Job=%s\n", jcr->JobId,
            jcr->UseCount(), jcr->Job);
      break;
    }
  }
  endeach_jcr(jcr);

  return jcr;
}

/*
 * Given a Job, find the JobControlRecord compares on the number of
 * characters in Job thus allowing partial matches.
 *
 * Returns: jcr on success
 *          nullptr on failure
 */
JobControlRecord* get_jcr_by_partial_name(char* Job)
{
  JobControlRecord* jcr;
  int len;

  if (!Job) { return nullptr; }

  len = strlen(Job);
  foreach_jcr (jcr) {
    if (bstrncmp(Job, jcr->Job, len)) {
      jcr->IncUseCount();
      Dmsg3(debuglevel, "Inc get_jcr jid=%u UseCount=%d Job=%s\n", jcr->JobId,
            jcr->UseCount(), jcr->Job);
      break;
    }
  }
  endeach_jcr(jcr);

  return jcr;
}

/*
 * Given a Job, find the JobControlRecord requires an exact match of names.
 *
 * Returns: jcr on success
 *          nullptr on failure
 */
JobControlRecord* get_jcr_by_full_name(char* Job)
{
  JobControlRecord* jcr;

  if (!Job) { return nullptr; }

  foreach_jcr (jcr) {
    if (bstrcmp(jcr->Job, Job)) {
      jcr->IncUseCount();
      Dmsg3(debuglevel, "Inc get_jcr jid=%u UseCount=%d Job=%s\n", jcr->JobId,
            jcr->UseCount(), jcr->Job);
      break;
    }
  }
  endeach_jcr(jcr);

  return jcr;
}

const char* JcrGetAuthenticateKey(const char* unified_job_name)
{
  if (!unified_job_name) { return nullptr; }

  JobControlRecord* jcr;
  const char* auth_key = nullptr;
  foreach_jcr (jcr) {
    if (bstrcmp(jcr->Job, unified_job_name)) {
      auth_key = jcr->sd_auth_key;
      Dmsg3(debuglevel, "Inc get_jcr jid=%u UseCount=%d Job=%s\n", jcr->JobId,
            jcr->UseCount(), jcr->Job);
      break;
    }
  }
  endeach_jcr(jcr);

  return auth_key;
}

TlsPolicy JcrGetTlsPolicy(const char* unified_job_name)
{
  if (!unified_job_name) { return kBnetTlsUnknown; }

  TlsPolicy policy = kBnetTlsUnknown;
  JobControlRecord* jcr;

  foreach_jcr (jcr) {
    if (bstrcmp(jcr->Job, unified_job_name)) {
      policy = jcr->sd_tls_policy;
      Dmsg4(debuglevel, "Inc get_jcr jid=%u UseCount=%d Job=%s TlsPolicy=%d\n",
            jcr->JobId, jcr->UseCount(), jcr->Job, policy);
      break;
    }
  }
  endeach_jcr(jcr);

  return policy;
}

static void UpdateWaitTime(JobControlRecord* jcr, int newJobStatus)
{
  bool enter_in_waittime;
  int oldJobStatus = jcr->JobStatus;

  switch (newJobStatus) {
    case JS_WaitFD:
    case JS_WaitSD:
    case JS_WaitMedia:
    case JS_WaitMount:
    case JS_WaitStoreRes:
    case JS_WaitJobRes:
    case JS_WaitClientRes:
    case JS_WaitMaxJobs:
    case JS_WaitPriority:
      enter_in_waittime = true;
      break;
    default:
      enter_in_waittime = false; /* not a Wait situation */
      break;
  }

  /*
   * If we were previously waiting and are not any more
   * we want to update the wait_time variable, which is
   * the start of waiting.
   */
  switch (oldJobStatus) {
    case JS_WaitFD:
    case JS_WaitSD:
    case JS_WaitMedia:
    case JS_WaitMount:
    case JS_WaitStoreRes:
    case JS_WaitJobRes:
    case JS_WaitClientRes:
    case JS_WaitMaxJobs:
    case JS_WaitPriority:
      if (!enter_in_waittime) { /* we get out the wait time */
        jcr->wait_time_sum += (time(nullptr) - jcr->wait_time);
        jcr->wait_time = 0;
      }
      break;
    default:
      /*
       * If wait state is new, we keep current time for watchdog MaxWaitTime
       */
      if (enter_in_waittime) { jcr->wait_time = time(nullptr); }
      break;
  }
}

/*
 * Priority runs from 0 (lowest) to 10 (highest)
 */
static int GetStatusPriority(int JobStatus)
{
  int priority = 0;

  switch (JobStatus) {
    case JS_Incomplete:
      priority = 10;
      break;
    case JS_ErrorTerminated:
    case JS_FatalError:
    case JS_Canceled:
      priority = 9;
      break;
    case JS_Error:
      priority = 8;
      break;
    case JS_Differences:
      priority = 7;
      break;
  }

  return priority;
}

/*
 * Send Job status to Director
 */
bool JobControlRecord::sendJobStatus()
{
  if (dir_bsock) { return dir_bsock->fsend(Job_status, Job, JobStatus); }

  return true;
}

/*
 * Set and send Job status to Director
 */
bool JobControlRecord::sendJobStatus(int newJobStatus)
{
  if (!is_JobStatus(newJobStatus)) {
    setJobStatus(newJobStatus);
    if (dir_bsock) { return dir_bsock->fsend(Job_status, Job, JobStatus); }
  }

  return true;
}

void JobControlRecord::setJobStarted()
{
  job_started = true;
  job_started_time = time(nullptr);
}

void JobControlRecord::resetJobStatus(int newJobStatus)
{
  JobStatus = newJobStatus;
}

void JobControlRecord::setJobStatus(int newJobStatus)
{
  int priority;
  int old_priority = 0;
  int oldJobStatus = ' ';

  if (JobStatus) {
    oldJobStatus = JobStatus;
    old_priority = GetStatusPriority(oldJobStatus);
  }
  priority = GetStatusPriority(newJobStatus);

  Dmsg2(800, "setJobStatus(%s, %c)\n", Job, newJobStatus);

  /*
   * Update wait_time depending on newJobStatus and oldJobStatus
   */
  UpdateWaitTime(this, newJobStatus);

  /*
   * For a set of errors, ... keep the current status
   * so it isn't lost. For all others, set it.
   */
  Dmsg2(800, "OnEntry JobStatus=%c newJobstatus=%c\n", oldJobStatus,
        newJobStatus);

  /*
   * If status priority is > than proposed new status, change it.
   * If status priority == new priority and both are zero, take the new
   * status. If it is not zero, then we keep the first non-zero "error" that
   * occurred.
   */
  if (priority > old_priority || (priority == 0 && old_priority == 0)) {
    Dmsg4(800, "Set new stat. old: %c,%d new: %c,%d\n", oldJobStatus,
          old_priority, newJobStatus, priority);
    JobStatus = newJobStatus; /* replace with new status */
  }

  if (oldJobStatus != JobStatus) {
    Dmsg2(800, "leave setJobStatus old=%c new=%c\n", oldJobStatus,
          newJobStatus);
    //    GeneratePluginEvent(this, bEventStatusChange, nullptr);
  }
}

void LockJcrChain() { jcr_chain_mutex.lock(); }

void UnlockJcrChain() { jcr_chain_mutex.unlock(); }

/*
 * Start walk of jcr chain
 * The proper way to walk the jcr chain is:
 *    JobControlRecord *jcr;
 *    foreach_jcr(jcr) {
 *      ...
 *    }
 *    endeach_jcr(jcr);
 *
 * It is possible to leave out the endeach_jcr(jcr), but
 * in that case, the last jcr referenced must be explicitly
 * released with:
 *
 * FreeJcr(jcr);
 */
JobControlRecord* jcr_walk_start()
{
  JobControlRecord* jcr;
  LockJcrChain();
  jcr = (JobControlRecord*)job_control_record_chain->first();
  if (jcr) {
    jcr->IncUseCount();
    if (jcr->JobId > 0) {
      Dmsg3(debuglevel, "Inc walk_start jid=%u UseCount=%d Job=%s\n",
            jcr->JobId, jcr->UseCount(), jcr->Job);
    }
  }
  UnlockJcrChain();
  return jcr;
}

/*
 * Get next jcr from chain, and release current one
 */
JobControlRecord* jcr_walk_next(JobControlRecord* prev_jcr)
{
  JobControlRecord* jcr;

  LockJcrChain();
  jcr = (JobControlRecord*)job_control_record_chain->next(prev_jcr);
  if (jcr) {
    jcr->IncUseCount();
    if (jcr->JobId > 0) {
      Dmsg3(debuglevel, "Inc walk_next jid=%u UseCount=%d Job=%s\n", jcr->JobId,
            jcr->UseCount(), jcr->Job);
    }
  }
  UnlockJcrChain();
  if (prev_jcr) { FreeJcr(prev_jcr); }
  return jcr;
}

/*
 * Release last jcr referenced
 */
void JcrWalkEnd(JobControlRecord* jcr)
{
  if (jcr) {
    if (jcr->JobId > 0) {
      Dmsg3(debuglevel, "Free walk_end jid=%u UseCount=%d Job=%s\n", jcr->JobId,
            jcr->UseCount(), jcr->Job);
    }
    FreeJcr(jcr);
  }
}

/*
 * Return number of Jobs
 */
int JobCount()
{
  JobControlRecord* jcr;
  int count = 0;

  LockJcrChain();
  for (jcr = (JobControlRecord*)job_control_record_chain->first();
       (jcr = (JobControlRecord*)job_control_record_chain->next(jcr));) {
    if (jcr->JobId > 0) { count++; }
  }
  UnlockJcrChain();
  return count;
}

/*
 * Setup to call the timeout check routine every 30 seconds
 * This routine will check any timers that have been enabled.
 */
bool InitJcrSubsystem(int timeout)
{
  watchdog_t* wd = new_watchdog();

  watch_dog_timeout = timeout;
  wd->one_shot = false;
  wd->interval = 30; /* FIXME: should be configurable somewhere, even
                      if only with a #define */
  wd->callback = JcrTimeoutCheck;

  RegisterWatchdog(wd);

  return true;
}

void InitJcrChain()
{
  JobControlRecord* jcr = nullptr;
  if (!job_control_record_chain) {
    job_control_record_chain = new dlist(jcr, &jcr->link);
  }
}

void CleanupJcrChain()
{
  if (job_control_record_chain) {
    delete job_control_record_chain;
    job_control_record_chain = nullptr;
  }
}

static void JcrTimeoutCheck(watchdog_t* /* self */)
{
  JobControlRecord* jcr;
  BareosSocket* bs;
  time_t timer_start;

  Dmsg0(debuglevel, "Start JobControlRecord timeout checks\n");

  /* Walk through all JCRs checking if any one is
   * blocked for more than specified max time.
   */
  foreach_jcr (jcr) {
    Dmsg2(debuglevel, "JcrTimeoutCheck JobId=%u jcr=0x%x\n", jcr->JobId, jcr);
    if (jcr->JobId == 0) { continue; }
    bs = jcr->store_bsock;
    if (bs) {
      timer_start = bs->timer_start;
      if (timer_start && (watchdog_time - timer_start) > watch_dog_timeout) {
        bs->timer_start = 0; /* turn off timer */
        bs->SetTimedOut();
        Qmsg(jcr, M_ERROR, 0,
             _("Watchdog sending kill after %d secs to thread stalled reading "
               "Storage daemon.\n"),
             watchdog_time - timer_start);
        jcr->MyThreadSendSignal(TIMEOUT_SIGNAL);
      }
    }
    bs = jcr->file_bsock;
    if (bs) {
      timer_start = bs->timer_start;
      if (timer_start && (watchdog_time - timer_start) > watch_dog_timeout) {
        bs->timer_start = 0; /* turn off timer */
        bs->SetTimedOut();
        Qmsg(jcr, M_ERROR, 0,
             _("Watchdog sending kill after %d secs to thread stalled reading "
               "File daemon.\n"),
             watchdog_time - timer_start);
        jcr->MyThreadSendSignal(TIMEOUT_SIGNAL);
      }
    }
    bs = jcr->dir_bsock;
    if (bs) {
      timer_start = bs->timer_start;
      if (timer_start && (watchdog_time - timer_start) > watch_dog_timeout) {
        bs->timer_start = 0; /* turn off timer */
        bs->SetTimedOut();
        Qmsg(jcr, M_ERROR, 0,
             _("Watchdog sending kill after %d secs to thread stalled reading "
               "Director.\n"),
             watchdog_time - timer_start);
        jcr->MyThreadSendSignal(TIMEOUT_SIGNAL);
      }
    }
  }
  endeach_jcr(jcr);

  Dmsg0(debuglevel, "Finished JobControlRecord timeout checks\n");
}

/*
 * Return next JobId from comma separated list
 *
 * Returns:
 *   1 if next JobId returned
 *   0 if no more JobIds are in list
 *  -1 there is an error
 */
int GetNextJobidFromList(const char** p, uint32_t* JobId)
{
  const int maxlen = 30;
  char jobid[maxlen + 1];
  const char* q = *p;

  jobid[0] = 0;
  for (int i = 0; i < maxlen; i++) {
    if (*q == 0) {
      break;
    } else if (*q == ',') {
      q++;
      break;
    }
    jobid[i] = *q++;
    jobid[i + 1] = 0;
  }
  if (jobid[0] == 0) {
    return 0;
  } else if (!Is_a_number(jobid)) {
    return -1; /* error */
  }
  *p = q;
  *JobId = str_to_int64(jobid);
  return 1;
}

/*
 * Used to display specific daemon information after a fatal signal
 * (like BareosDb in the director)
 */
#define MAX_DBG_HOOK 10
static dbg_jcr_hook_t* dbg_jcr_hooks[MAX_DBG_HOOK];
static int dbg_jcr_handler_count;

void DbgJcrAddHook(dbg_jcr_hook_t* hook)
{
  ASSERT(dbg_jcr_handler_count < MAX_DBG_HOOK);
  dbg_jcr_hooks[dbg_jcr_handler_count++] = hook;
}

/*
 * !!! WARNING !!!
 *
 * This function should be used ONLY after a fatal signal. We walk through the
 * JobControlRecord chain without doing any lock, BAREOS should not be
 * running.
 */
void DbgPrintJcr(FILE* fp)
{
  char ed1[50], buf1[128], buf2[128], buf3[128], buf4[128];
  if (!job_control_record_chain) { return; }

  fprintf(fp, "Attempt to dump current JCRs. njcrs=%d\n",
          job_control_record_chain->size());

  for (JobControlRecord* jcr =
           (JobControlRecord*)job_control_record_chain->first();
       jcr; jcr = (JobControlRecord*)job_control_record_chain->next(jcr)) {
    fprintf(fp, "threadid=%s JobId=%d JobStatus=%c jcr=%p name=%s\n",
            edit_pthread(jcr->my_thread_id, ed1, sizeof(ed1)), (int)jcr->JobId,
            jcr->JobStatus, jcr, jcr->Job);
    fprintf(fp,
            "threadid=%s killable=%d JobId=%d JobStatus=%c jcr=%p name=%s\n",
            edit_pthread(jcr->my_thread_id, ed1, sizeof(ed1)),
            jcr->IsKillable(), (int)jcr->JobId, jcr->JobStatus, jcr, jcr->Job);
    fprintf(fp, "\tUseCount=%i\n", jcr->UseCount());
    fprintf(fp, "\tJobType=%c JobLevel=%c\n", jcr->getJobType(),
            jcr->getJobLevel());
    bstrftime(buf1, sizeof(buf1), jcr->sched_time);
    bstrftime(buf2, sizeof(buf2), jcr->start_time);
    bstrftime(buf3, sizeof(buf3), jcr->end_time);
    bstrftime(buf4, sizeof(buf4), jcr->wait_time);
    fprintf(fp, "\tsched_time=%s start_time=%s\n\tend_time=%s wait_time=%s\n",
            buf1, buf2, buf3, buf4);
    fprintf(fp, "\tdb=%p db_batch=%p batch_started=%i\n", jcr->db,
            jcr->db_batch, jcr->batch_started);

    /*
     * Call all the jcr debug hooks
     */
    for (int i = 0; i < dbg_jcr_handler_count; i++) {
      dbg_jcr_hook_t* hook = dbg_jcr_hooks[i];
      hook(jcr, fp);
    }
  }
}
