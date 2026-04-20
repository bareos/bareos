/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2001-2011 Free Software Foundation Europe e.V.
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
// Kern Sibbald, August MMI
/**
 * @file
 * Bareos File Daemon Status routines
 */

#include "include/bareos.h"
#include "filed/filed.h"
#include "filed/filed_globals.h"
#include "filed/filed_jcr_impl.h"
#include "lib/status_packet.h"
#include "lib/bsock.h"
#include "lib/edit.h"
#include "lib/parse_conf.h"
#include "lib/recent_job_results_list.h"
#include "findlib/enable_priv.h"
#include "lib/util.h"

#if HAVE_JANSSON
#  include <jansson.h>
#endif

namespace filedaemon {

/* Forward referenced functions */
static void ListTerminatedJobs(StatusPacket* sp);
static void ListRunningJobs(StatusPacket* sp);
static void ListStatusHeader(StatusPacket* sp);
static const char* JobLevelToString(int level);
#if HAVE_JANSSON
static void OutputStatusJson(StatusPacket* sp);
#endif

/* Static variables */
inline constexpr const char qstatus[] = ".status %s\n";

inline constexpr const char OKqstatus[] = "2000 OK .status\n";
inline constexpr const char DotStatusJob[]
    = "JobId=%d JobStatus=%c JobErrors=%d\n";

#if defined(HAVE_WIN32)
static int privs = 0;
#endif
#ifdef WIN32_VSS
#  include "vss.h"
#  define VSS " VSS"
#else
#  define VSS ""
#endif

// General status generator
static void OutputStatus(StatusPacket* sp)
{
  ListStatusHeader(sp);
  ListRunningJobs(sp);
  ListTerminatedJobs(sp);
}

static void ListStatusHeader(StatusPacket* sp)
{
  int len;
  char dt[MAX_TIME_LENGTH];
  PoolMem msg(PM_MESSAGE);
  char b1[32];

  len = Mmsg(msg, "%s Version: %s (%s)" VSS " %s\n", my_name,
             kBareosVersionStrings.Full, kBareosVersionStrings.Date,
             kBareosVersionStrings.GetOsInfo());
  sp->send(msg, len);
  bstrftime_nc(dt, sizeof(dt), daemon_start_time);
  len = Mmsg(
      msg,
      T_("Daemon started %s. Jobs: run=%" PRIuz " running=%d, %s binary\n"), dt,
      NumJobsRun(), JobCount(), kBareosVersionStrings.BinaryInfo);
  sp->send(msg, len);

#if defined(HAVE_WIN32)
  if (debug_level > 0) {
    if (!privs) { privs = EnableBackupPrivileges(NULL, 1); }
    len = Mmsg(msg, "Priv 0x%x\n", privs);
    sp->send(msg, len);
    len = Mmsg(msg, "APIs=%sOPT,%sATP,%sLPV,%sCFA,%sCFW,\n",
               p_OpenProcessToken ? "" : "!",
               p_AdjustTokenPrivileges ? "" : "!",
               p_LookupPrivilegeValue ? "" : "!", p_CreateFileA ? "" : "!",
               p_CreateFileW ? "" : "!");
    sp->send(msg, len);
    len = Mmsg(msg,
               " %sWUL,%sWMKD,%sGFAA,%sGFAW,%sGFAEA,%sGFAEW,%sSFAA,%sSFAW,%sBR,"
               "%sBW,%sSPSP,\n",
               p_wunlink ? "" : "!", p_wmkdir ? "" : "!",
               p_GetFileAttributesA ? "" : "!", p_GetFileAttributesW ? "" : "!",
               p_GetFileAttributesExA ? "" : "!",
               p_GetFileAttributesExW ? "" : "!",
               p_SetFileAttributesA ? "" : "!", p_SetFileAttributesW ? "" : "!",
               p_BackupRead ? "" : "!", p_BackupWrite ? "" : "!",
               p_SetProcessShutdownParameters ? "" : "!");
    sp->send(msg, len);
    len = Mmsg(
        msg, " %sWC2MB,%sMB2WC,%sFFFA,%sFFFW,%sFNFA,%sFNFW,%sSCDA,%sSCDW,\n",
        p_WideCharToMultiByte ? "" : "!", p_MultiByteToWideChar ? "" : "!",
        p_FindFirstFileA ? "" : "!", p_FindFirstFileW ? "" : "!",
        p_FindNextFileA ? "" : "!", p_FindNextFileW ? "" : "!",
        p_SetCurrentDirectoryA ? "" : "!", p_SetCurrentDirectoryW ? "" : "!");
    sp->send(msg, len);
    len = Mmsg(msg, " %sGCDA,%sGCDW,%sGVPNW,%sGVNFVMPW\n",
               p_GetCurrentDirectoryA ? "" : "!",
               p_GetCurrentDirectoryW ? "" : "!",
               p_GetVolumePathNameW ? "" : "!",
               p_GetVolumeNameForVolumeMountPointW ? "" : "!");
    sp->send(msg, len);
  }
#endif

  len = Mmsg(
      msg,
      T_(" Sizeof: boffset_t=%" PRIuz " size_t=%" PRIuz " debug=%d trace=%d "
         "bwlimit=%skB/s\n"),
      sizeof(boffset_t), sizeof(size_t), debug_level, GetTrace(),
      edit_uint64_with_commas(me->max_bandwidth_per_job / 1024, b1));
  sp->send(msg, len);

  if (me->secure_erase_cmdline) {
    len = Mmsg(msg, T_(" secure erase command='%s'\n"),
               me->secure_erase_cmdline);
    sp->send(msg, len);
  }

  len = ListFdPlugins(msg);
  if (len > 0) { sp->send(msg, len); }

  if (my_config->HasWarnings()) {
    sp->send(
        "\n"
        "There are WARNINGS for this filedaemon's configuration!\n"
        "See output of 'bareos-fd -t' for details.\n");
  }
}

static void ListRunningJobsPlain(StatusPacket* sp)
{
  JobControlRecord* njcr;
  int len, sec, bps;
  bool found = false;
  PoolMem msg(PM_MESSAGE);
  char dt[MAX_TIME_LENGTH], b1[32], b2[32], b3[32], b4[32];

  // List running jobs
  Dmsg0(1000, "Begin status jcr loop.\n");
  len = Mmsg(msg, T_("\nRunning Jobs:\n"));
  sp->send(msg, len);

  foreach_jcr (njcr) {
    bstrftime_nc(dt, sizeof(dt), njcr->start_time);
    if (njcr->JobId > 0) {
      len = Mmsg(msg, T_("JobId %d Job %s is running.\n"), njcr->JobId,
                 njcr->Job);
      sp->send(msg, len);
#ifdef WIN32_VSS
      len = Mmsg(msg, T_("    %s%s %s Job started: %s\n"),
                 (njcr->fd_impl->pVSSClient
                  && njcr->fd_impl->pVSSClient->IsInitialized())
                     ? "VSS "
                     : "",
                 JobLevelToString(njcr->getJobLevel()),
                 job_type_to_str(njcr->getJobType()), dt);
#else
      len = Mmsg(msg, T_("    %s %s Job started: %s\n"),
                 JobLevelToString(njcr->getJobLevel()),
                 job_type_to_str(njcr->getJobType()), dt);
#endif
    } else if ((njcr->JobId == 0) && (njcr->fd_impl->director)) {
      len = Mmsg(msg, T_("%s (director) connected at: %s\n"),
                 njcr->fd_impl->director->resource_name_, dt);
    } else {
      /* This should only occur shortly, until the JobControlRecord values are
       * set. */
      len = Mmsg(msg, T_("Unknown connection, started at: %s\n"), dt);
    }
    sp->send(msg, len);
    if (njcr->JobId == 0) { continue; }
    sec = time(NULL) - njcr->start_time;
    if (sec <= 0) { sec = 1; }
    bps = (int)(njcr->JobBytes / sec);
    len = Mmsg(msg,
               T_("    Files=%s Bytes=%s Bytes/sec=%s Errors=%d\n"
                  "    Bwlimit=%s\n"),
               edit_uint64_with_commas(njcr->JobFiles, b1),
               edit_uint64_with_commas(njcr->JobBytes, b2),
               edit_uint64_with_commas(bps, b3), njcr->JobErrors,
               edit_uint64_with_commas(njcr->max_bandwidth, b4));
    sp->send(msg, len);
    len = Mmsg(msg, T_("    Files Examined=%s\n"),
               edit_uint64_with_commas(njcr->fd_impl->num_files_examined, b1));
    sp->send(msg, len);
    if (njcr->JobFiles > 0) {
      {
        std::unique_lock l(njcr->mutex_guard());
        len = Mmsg(msg, T_("    Processing file: %s\n"),
                   njcr->fd_impl->last_fname);
      }
      sp->send(msg, len);
    }

    found = true;
    if (njcr->store_bsock) {
      len = Mmsg(msg, "    SDReadSeqNo=%" PRIu64 " fd=%d\n",
                 njcr->store_bsock->read_seqno, njcr->store_bsock->fd_);
      sp->send(msg, len);
    } else {
      len = Mmsg(msg, T_("    SDSocket closed.\n"));
      sp->send(msg, len);
    }
  }
  endeach_jcr(njcr);

  if (!found) {
    len = Mmsg(msg, T_("No Jobs running.\n"));
    sp->send(msg, len);
  }

  len = PmStrcpy(msg, T_("====\n"));
  sp->send(msg, len);
}

static void ListRunningJobsApi(StatusPacket* sp)
{
  JobControlRecord* njcr;
  int len, sec, bps;
  PoolMem msg(PM_MESSAGE);
  char dt[MAX_TIME_LENGTH], b1[32], b2[32], b3[32], b4[32];

  // List running jobs for Bat/Bweb (sfd_imple to parse)
  foreach_jcr (njcr) {
    bstrutime(dt, sizeof(dt), njcr->start_time);
    if (njcr->JobId == 0) {
      len = Mmsg(msg, "DirectorConnected=%s\n", dt);
    } else {
      len = Mmsg(msg, "JobId=%d\n Job=%s\n", njcr->JobId, njcr->Job);
      sp->send(msg, len);
#ifdef WIN32_VSS
      len = Mmsg(msg, " VSS=%d\n Level=%c\n JobType=%c\n JobStarted=%s\n",
                 (njcr->fd_impl->pVSSClient
                  && njcr->fd_impl->pVSSClient->IsInitialized())
                     ? 1
                     : 0,
                 njcr->getJobLevel(), njcr->getJobType(), dt);
#else
      len = Mmsg(msg, " VSS=%d\n Level=%c\n JobType=%c\n JobStarted=%s\n", 0,
                 njcr->getJobLevel(), njcr->getJobType(), dt);
#endif
    }
    sp->send(msg, len);
    if (njcr->JobId == 0) { continue; }
    sec = time(NULL) - njcr->start_time;
    if (sec <= 0) { sec = 1; }
    bps = (int)(njcr->JobBytes / sec);
    len = Mmsg(msg,
               " Files=%s\n Bytes=%s\n Bytes/sec=%s\n Errors=%d\n"
               " Bwlimit=%s\n",
               edit_uint64(njcr->JobFiles, b1), edit_uint64(njcr->JobBytes, b2),
               edit_uint64(bps, b3), njcr->JobErrors,
               edit_int64(njcr->max_bandwidth, b4));
    sp->send(msg, len);
    len = Mmsg(msg, " Files Examined=%s\n",
               edit_uint64(njcr->fd_impl->num_files_examined, b1));
    sp->send(msg, len);
    if (njcr->JobFiles > 0) {
      {
        std::unique_lock l(njcr->mutex_guard());
        len = Mmsg(msg, " Processing file=%s\n", njcr->fd_impl->last_fname);
      }
      sp->send(msg, len);
    }

    if (njcr->store_bsock) {
      len = Mmsg(msg, " SDReadSeqNo=%" PRIu64 "\n fd=%d\n",
                 njcr->store_bsock->read_seqno, njcr->store_bsock->fd_);
      sp->send(msg, len);
    } else {
      len = Mmsg(msg, T_(" SDSocket=closed\n"));
      sp->send(msg, len);
    }
  }
  endeach_jcr(njcr);
}

static void ListRunningJobs(StatusPacket* sp)
{
  if (sp->api) {
    ListRunningJobsApi(sp);
  } else {
    ListRunningJobsPlain(sp);
  }
}

static void ListTerminatedJobs(StatusPacket* sp)
{
  int len;
  PoolMem msg(PM_MESSAGE);
  char level[10], dt[MAX_TIME_LENGTH], b1[30], b2[30];

  if (!sp->api) {
    len = PmStrcpy(msg, T_("\nTerminated Jobs:\n"));
    sp->send(msg, len);
  }

  if (RecentJobResultsList::Count() == 0) {
    if (!sp->api) {
      len = PmStrcpy(msg, T_("====\n"));
      sp->send(msg, len);
    }
    return;
  }

  if (!sp->api) {
    len = PmStrcpy(msg, T_(" JobId  Level    Files      Bytes   Status   "
                           "Finished        Name \n"));
    sp->send(msg, len);
    len = PmStrcpy(msg,
                   T_("===================================================="
                      "==================\n"));
    sp->send(msg, len);
  }

  for (const RecentJobResultsList::JobResult& je :
       RecentJobResultsList::Get()) {
    char* p;
    char JobName[MAX_NAME_LENGTH];
    const char* termstat;

    bstrftime_nc(dt, sizeof(dt), je.end_time);

    switch (je.JobType) {
      case JT_ADMIN:
      case JT_RESTORE:
        bstrncpy(level, "    ", sizeof(level));
        break;
      default:
        bstrncpy(level, JobLevelToString(je.JobLevel), sizeof(level));
        level[4] = 0;
        break;
    }

    switch (je.JobStatus) {
      case JS_Created:
        termstat = T_("Created");
        break;
      case JS_FatalError:
      case JS_ErrorTerminated:
        termstat = T_("Error");
        break;
      case JS_Differences:
        termstat = T_("Diffs");
        break;
      case JS_Canceled:
        termstat = T_("Cancel");
        break;
      case JS_Terminated:
        termstat = T_("OK");
        break;
      default:
        termstat = T_("Other");
        break;
    }
    bstrncpy(JobName, je.Job, sizeof(JobName));

    // There are three periods after the Job name
    for (int i = 0; i < 3; i++) {
      if ((p = strrchr(JobName, '.')) != NULL) { *p = 0; }
    }

    if (sp->api) {
      len = Mmsg(msg, T_("%6d\t%-6s\t%8s\t%10s\t%-7s\t%-8s\t%s\n"), je.JobId,
                 level, edit_uint64_with_commas(je.JobFiles, b1),
                 edit_uint64_with_suffix(je.JobBytes, b2), termstat, dt,
                 JobName);
    } else {
      len = Mmsg(msg, T_("%6d  %-6s %8s %10s  %-7s  %-8s %s\n"), je.JobId,
                 level, edit_uint64_with_commas(je.JobFiles, b1),
                 edit_uint64_with_suffix(je.JobBytes, b2), termstat, dt,
                 JobName);
    }
    sp->send(msg, len);
  }

  if (!sp->api) {
    len = PmStrcpy(msg, T_("====\n"));
    sp->send(msg, len);
  }
}

#if HAVE_JANSSON

/* Long-form status string for a terminated job, mirroring the text emitter
 * in ListTerminatedJobs(). */
static const char* TerminatedStatusToLongString(int js)
{
  switch (js) {
    case JS_Created:
      return "Created";
    case JS_FatalError:
    case JS_ErrorTerminated:
      return "Error";
    case JS_Differences:
      return "Diffs";
    case JS_Canceled:
      return "Cancel";
    case JS_Terminated:
      return "OK";
    default:
      return "Other";
  }
}

static json_t* TimeAsIsoJson(utime_t t)
{
  if (t == 0) { return json_null(); }
  char dt[MAX_TIME_LENGTH];
  bstrftime(dt, sizeof(dt), t, "%Y-%m-%dT%H:%M:%S");
  return json_string(dt);
}

static json_t* CharAsStringJson(char c)
{
  char buf[2] = {c, 0};
  return json_string(buf);
}

static json_t* BuildStatusHeaderJson()
{
  json_t* obj = json_object();
  json_object_set_new(obj, "name", json_string(my_name));
  json_object_set_new(obj, "version", json_string(kBareosVersionStrings.Full));
  json_object_set_new(obj, "version_date",
                      json_string(kBareosVersionStrings.Date));
  json_object_set_new(obj, "os_info",
                      json_string(kBareosVersionStrings.GetOsInfo()));
  json_object_set_new(obj, "daemon_started", TimeAsIsoJson(daemon_start_time));
  json_object_set_new(obj, "jobs_run",
                      json_integer(static_cast<json_int_t>(NumJobsRun())));
  json_object_set_new(obj, "jobs_running",
                      json_integer(static_cast<json_int_t>(JobCount())));
  json_object_set_new(obj, "binary_info",
                      json_string(kBareosVersionStrings.BinaryInfo));
#ifdef WIN32_VSS
  json_object_set_new(obj, "vss_supported", json_true());
#else
  json_object_set_new(obj, "vss_supported", json_false());
#endif
  json_object_set_new(obj, "secure_erase_command",
                      me->secure_erase_cmdline
                          ? json_string(me->secure_erase_cmdline)
                          : json_null());
  json_object_set_new(obj, "config_warnings",
                      json_boolean(my_config->HasWarnings()));
  return obj;
}

static json_t* BuildRunningJobsJson()
{
  json_t* arr = json_array();
  JobControlRecord* njcr;

  foreach_jcr (njcr) {
    if (njcr->JobId == 0) {
      /* Director control connections are emitted separately. */
      continue;
    }
    json_t* j = json_object();
    json_object_set_new(j, "jobid",
                        json_integer(static_cast<json_int_t>(njcr->JobId)));
    json_object_set_new(j, "job", json_string(njcr->Job));
    json_object_set_new(j, "started", TimeAsIsoJson(njcr->start_time));
    json_object_set_new(j, "level",
                        CharAsStringJson(njcr->getJobLevel()));
    json_object_set_new(j, "job_type",
                        CharAsStringJson(njcr->getJobType()));
#ifdef WIN32_VSS
    json_object_set_new(
        j, "vss",
        json_boolean(njcr->fd_impl->pVSSClient
                     && njcr->fd_impl->pVSSClient->IsInitialized()));
#else
    json_object_set_new(j, "vss", json_false());
#endif
    int sec = time(NULL) - njcr->start_time;
    if (sec <= 0) { sec = 1; }
    uint64_t bps = njcr->JobBytes / sec;
    json_object_set_new(j, "files",
                        json_integer(static_cast<json_int_t>(njcr->JobFiles)));
    json_object_set_new(j, "bytes",
                        json_integer(static_cast<json_int_t>(njcr->JobBytes)));
    json_object_set_new(j, "bytes_per_sec",
                        json_integer(static_cast<json_int_t>(bps)));
    json_object_set_new(j, "errors",
                        json_integer(static_cast<json_int_t>(njcr->JobErrors)));
    json_object_set_new(
        j, "bwlimit",
        json_integer(static_cast<json_int_t>(njcr->max_bandwidth)));
    json_object_set_new(
        j, "files_examined",
        json_integer(
            static_cast<json_int_t>(njcr->fd_impl->num_files_examined)));
    if (njcr->JobFiles > 0) {
      std::unique_lock l(njcr->mutex_guard());
      json_object_set_new(j, "processing_file",
                          njcr->fd_impl->last_fname
                              ? json_string(njcr->fd_impl->last_fname)
                              : json_null());
    } else {
      json_object_set_new(j, "processing_file", json_null());
    }
    if (njcr->store_bsock) {
      json_t* sdsock = json_object();
      json_object_set_new(
          sdsock, "read_seqno",
          json_integer(
              static_cast<json_int_t>(njcr->store_bsock->read_seqno)));
      json_object_set_new(
          sdsock, "fd",
          json_integer(static_cast<json_int_t>(njcr->store_bsock->fd_)));
      json_object_set_new(j, "sd_socket", sdsock);
    } else {
      json_object_set_new(j, "sd_socket", json_null());
    }
    json_array_append_new(arr, j);
  }
  endeach_jcr(njcr);
  return arr;
}

static json_t* BuildDirectorsConnectedJson()
{
  json_t* arr = json_array();
  JobControlRecord* njcr;

  foreach_jcr (njcr) {
    if (njcr->JobId == 0 && njcr->fd_impl && njcr->fd_impl->director) {
      json_t* o = json_object();
      json_object_set_new(
          o, "name",
          json_string(njcr->fd_impl->director->resource_name_));
      json_object_set_new(o, "connected_at", TimeAsIsoJson(njcr->start_time));
      json_array_append_new(arr, o);
    }
  }
  endeach_jcr(njcr);
  return arr;
}

static json_t* BuildTerminatedJobsJson()
{
  json_t* arr = json_array();

  for (const RecentJobResultsList::JobResult& je :
       RecentJobResultsList::Get()) {
    json_t* j = json_object();
    json_object_set_new(j, "jobid",
                        json_integer(static_cast<json_int_t>(je.JobId)));

    const char* level_str;
    switch (je.JobType) {
      case JT_ADMIN:
      case JT_RESTORE:
        level_str = "";
        break;
      default:
        level_str = JobLevelToString(je.JobLevel);
        break;
    }
    json_object_set_new(j, "level", json_string(level_str));
    json_object_set_new(j, "files",
                        json_integer(static_cast<json_int_t>(je.JobFiles)));
    json_object_set_new(j, "bytes",
                        json_integer(static_cast<json_int_t>(je.JobBytes)));
    json_object_set_new(j, "status_code",
                        CharAsStringJson(static_cast<char>(je.JobStatus)));
    json_object_set_new(j, "status",
                        json_string(TerminatedStatusToLongString(je.JobStatus)));
    json_object_set_new(j, "finished", TimeAsIsoJson(je.end_time));

    /* Strip the three trailing ".<timestamp>.<seq>" pieces from the Job
     * name, matching the text emitter's behavior. */
    char JobName[MAX_NAME_LENGTH];
    bstrncpy(JobName, je.Job, sizeof(JobName));
    for (int i = 0; i < 3; i++) {
      char* p = strrchr(JobName, '.');
      if (p) { *p = 0; }
    }
    json_object_set_new(j, "job_name", json_string(JobName));

    json_array_append_new(arr, j);
  }
  return arr;
}

static void OutputStatusJson(StatusPacket* sp)
{
  json_t* root = json_object();
  json_object_set_new(root, "header", BuildStatusHeaderJson());
  json_object_set_new(root, "directors_connected",
                      BuildDirectorsConnectedJson());
  json_object_set_new(root, "running_jobs", BuildRunningJobsJson());
  json_object_set_new(root, "terminated_jobs", BuildTerminatedJobsJson());

  char* serialized = json_dumps(root, JSON_INDENT(2));
  if (serialized) {
    int len = strlen(serialized);
    if (sp->bs) {
      /* StatusPacket::send() assumes bs->msg is already large enough; JSON
       * payloads can exceed the default PM_MESSAGE buffer, so grow it
       * explicitly before sending. */
      sp->bs->msg = CheckPoolMemorySize(sp->bs->msg, len + 1);
      memcpy(sp->bs->msg, serialized, len + 1);
      sp->bs->message_length = len + 1;
      sp->bs->send();
    } else if (sp->callback) {
      sp->callback(serialized, len, sp->context);
    }
    free(serialized);
  }
  json_decref(root);
}

#endif  // HAVE_JANSSON

// Status command from Director
bool StatusCmd(JobControlRecord* jcr)
{
  BareosSocket* user = jcr->dir_bsock;
  StatusPacket sp;

  user->fsend("\n");
  sp.bs = user;
  sp.api = false; /* no API output */
  OutputStatus(&sp);

  user->signal(BNET_EOD);
  return true;
}

// .status command from Director
bool QstatusCmd(JobControlRecord* jcr)
{
  BareosSocket* dir = jcr->dir_bsock;
  POOLMEM* cmd;
  JobControlRecord* njcr;
  StatusPacket sp;

  sp.bs = dir;
  cmd = GetMemory(dir->message_length + 1);

  if (sscanf(dir->msg, qstatus, cmd) != 1) {
    PmStrcpy(jcr->errmsg, dir->msg);
    Jmsg1(jcr, M_FATAL, 0, T_("Bad .status command: %s\n"), jcr->errmsg);
    dir->fsend(T_("2900 Bad .status command, missing argument.\n"));
    dir->signal(BNET_EOD);
    FreeMemory(cmd);
    return false;
  }
  UnbashSpaces(cmd);

  if (bstrcmp(cmd, "current")) {
    dir->fsend(OKqstatus);
    foreach_jcr (njcr) {
      if (njcr->JobId != 0) {
        dir->fsend(DotStatusJob, njcr->JobId, njcr->getJobStatus(),
                   njcr->JobErrors);
      }
    }
    endeach_jcr(njcr);
  } else if (bstrcmp(cmd, "last")) {
    dir->fsend(OKqstatus);
    if (RecentJobResultsList::Count() > 0) {
      RecentJobResultsList::JobResult job
          = RecentJobResultsList::GetMostRecentJobResult();
      dir->fsend(DotStatusJob, job.JobId, job.JobStatus, job.Errors);
    }
  } else if (Bstrcasecmp(cmd, "header")) {
    sp.api = true;
    ListStatusHeader(&sp);
  } else if (Bstrcasecmp(cmd, "running")) {
    sp.api = true;
    ListRunningJobs(&sp);
  } else if (Bstrcasecmp(cmd, "terminated")) {
    sp.api = true;
    ListTerminatedJobs(&sp);
#if HAVE_JANSSON
  } else if (Bstrcasecmp(cmd, "json")) {
    OutputStatusJson(&sp);
#endif
  } else {
    PmStrcpy(jcr->errmsg, dir->msg);
    Jmsg1(jcr, M_FATAL, 0, T_("Bad .status command: %s\n"), jcr->errmsg);
    dir->fsend(T_("2900 Bad .status command, wrong argument.\n"));
    dir->signal(BNET_EOD);
    FreeMemory(cmd);
    return false;
  }

  dir->signal(BNET_EOD);
  FreeMemory(cmd);
  return true;
}

// Convert Job Level into a string
static const char* JobLevelToString(int level)
{
  const char* str;

  switch (level) {
    case L_BASE:
      str = T_("Base");
      break;
    case L_FULL:
      str = T_("Full");
      break;
    case L_INCREMENTAL:
      str = T_("Incremental");
      break;
    case L_DIFFERENTIAL:
      str = T_("Differential");
      break;
    case L_SINCE:
      str = T_("Since");
      break;
    case L_VERIFY_CATALOG:
      str = T_("Verify Catalog");
      break;
    case L_VERIFY_INIT:
      str = T_("Init Catalog");
      break;
    case L_VERIFY_VOLUME_TO_CATALOG:
      str = T_("Volume to Catalog");
      break;
    case L_VERIFY_DISK_TO_CATALOG:
      str = T_("Disk to Catalog");
      break;
    case L_VERIFY_DATA:
      str = T_("Data");
      break;
    case L_NONE:
      str = " ";
      break;
    default:
      str = T_("Unknown Job Level");
      break;
  }
  return str;
}

} /* namespace filedaemon */
