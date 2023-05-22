/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2001-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2022 Bareos GmbH & Co. KG


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

namespace filedaemon {

/* Forward referenced functions */
static void ListTerminatedJobs(StatusPacket* sp);
static void ListRunningJobs(StatusPacket* sp);
static void ListStatusHeader(StatusPacket* sp);
static const char* JobLevelToString(int level);

/* Static variables */
static char qstatus[] = ".status %s\n";

static char OKqstatus[] = "2000 OK .status\n";
static char DotStatusJob[] = "JobId=%d JobStatus=%c JobErrors=%d\n";

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

  len = Mmsg(msg, _("%s Version: %s (%s) %s %s\n"), my_name,
             kBareosVersionStrings.Full, kBareosVersionStrings.Date, VSS,
             kBareosVersionStrings.GetOsInfo());
  sp->send(msg, len);
  bstrftime_nc(dt, sizeof(dt), daemon_start_time);
  len = Mmsg(msg, _("Daemon started %s. Jobs: run=%d running=%d, %s binary\n"),
             dt, num_jobs_run, JobCount(), kBareosVersionStrings.BinaryInfo);
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

  len = Mmsg(msg,
             _(" Sizeof: boffset_t=%d size_t=%d debug=%d trace=%d "
               "bwlimit=%skB/s\n"),
             sizeof(boffset_t), sizeof(size_t), debug_level, GetTrace(),
             edit_uint64_with_commas(me->max_bandwidth_per_job / 1024, b1));
  sp->send(msg, len);

  if (me->secure_erase_cmdline) {
    len = Mmsg(msg, _(" secure erase command='%s'\n"),
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
  len = Mmsg(msg, _("\nRunning Jobs:\n"));
  sp->send(msg, len);

  foreach_jcr (njcr) {
    bstrftime_nc(dt, sizeof(dt), njcr->start_time);
    if (njcr->JobId > 0) {
      len = Mmsg(msg, _("JobId %d Job %s is running.\n"), njcr->JobId,
                 njcr->Job);
      sp->send(msg, len);
#ifdef WIN32_VSS
      len = Mmsg(msg, _("    %s%s %s Job started: %s\n"),
                 (njcr->fd_impl->pVSSClient
                  && njcr->fd_impl->pVSSClient->IsInitialized())
                     ? "VSS "
                     : "",
                 JobLevelToString(njcr->getJobLevel()),
                 job_type_to_str(njcr->getJobType()), dt);
#else
      len = Mmsg(msg, _("    %s %s Job started: %s\n"),
                 JobLevelToString(njcr->getJobLevel()),
                 job_type_to_str(njcr->getJobType()), dt);
#endif
    } else if ((njcr->JobId == 0) && (njcr->fd_impl->director)) {
      len = Mmsg(msg, _("%s (director) connected at: %s\n"),
                 njcr->fd_impl->director->resource_name_, dt);
    } else {
      /*
       * This should only occur shortly, until the JobControlRecord values are
       * set.
       */
      len = Mmsg(msg, _("Unknown connection, started at: %s\n"), dt);
    }
    sp->send(msg, len);
    if (njcr->JobId == 0) { continue; }
    sec = time(NULL) - njcr->start_time;
    if (sec <= 0) { sec = 1; }
    bps = (int)(njcr->JobBytes / sec);
    len = Mmsg(msg,
               _("    Files=%s Bytes=%s Bytes/sec=%s Errors=%d\n"
                 "    Bwlimit=%s\n"),
               edit_uint64_with_commas(njcr->JobFiles, b1),
               edit_uint64_with_commas(njcr->JobBytes, b2),
               edit_uint64_with_commas(bps, b3), njcr->JobErrors,
               edit_uint64_with_commas(njcr->max_bandwidth, b4));
    sp->send(msg, len);
    len = Mmsg(msg, _("    Files Examined=%s\n"),
               edit_uint64_with_commas(njcr->fd_impl->num_files_examined, b1));
    sp->send(msg, len);
    if (njcr->JobFiles > 0) {
      njcr->lock();
      len = Mmsg(msg, _("    Processing file: %s\n"),
                 njcr->fd_impl->last_fname);
      njcr->unlock();
      sp->send(msg, len);
    }

    found = true;
    if (njcr->store_bsock) {
      len = Mmsg(msg, "    SDReadSeqNo=%lld fd=%d\n",
                 njcr->store_bsock->read_seqno, njcr->store_bsock->fd_);
      sp->send(msg, len);
    } else {
      len = Mmsg(msg, _("    SDSocket closed.\n"));
      sp->send(msg, len);
    }
    len = PmStrcpy(msg, njcr->timer.str().c_str());
    sp->send(msg, len);
  }
  endeach_jcr(njcr);

  if (!found) {
    len = Mmsg(msg, _("No Jobs running.\n"));
    sp->send(msg, len);
  }

  len = PmStrcpy(msg, _("====\n"));
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
      njcr->lock();
      len = Mmsg(msg, " Processing file=%s\n", njcr->fd_impl->last_fname);
      njcr->unlock();
      sp->send(msg, len);
    }

    if (njcr->store_bsock) {
      len = Mmsg(msg, " SDReadSeqNo=%lld\n fd=%d\n",
                 njcr->store_bsock->read_seqno, njcr->store_bsock->fd_);
      sp->send(msg, len);
    } else {
      len = Mmsg(msg, _(" SDSocket=closed\n"));
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
    len = PmStrcpy(msg, _("\nTerminated Jobs:\n"));
    sp->send(msg, len);
  }

  if (RecentJobResultsList::Count() == 0) {
    if (!sp->api) {
      len = PmStrcpy(msg, _("====\n"));
      sp->send(msg, len);
    }
    return;
  }

  if (!sp->api) {
    len = PmStrcpy(msg, _(" JobId  Level    Files      Bytes   Status   "
                          "Finished        Name \n"));
    sp->send(msg, len);
    len = PmStrcpy(msg, _("===================================================="
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
        termstat = _("Created");
        break;
      case JS_FatalError:
      case JS_ErrorTerminated:
        termstat = _("Error");
        break;
      case JS_Differences:
        termstat = _("Diffs");
        break;
      case JS_Canceled:
        termstat = _("Cancel");
        break;
      case JS_Terminated:
        termstat = _("OK");
        break;
      default:
        termstat = _("Other");
        break;
    }
    bstrncpy(JobName, je.Job, sizeof(JobName));

    // There are three periods after the Job name
    for (int i = 0; i < 3; i++) {
      if ((p = strrchr(JobName, '.')) != NULL) { *p = 0; }
    }

    if (sp->api) {
      len = Mmsg(msg, _("%6d\t%-6s\t%8s\t%10s\t%-7s\t%-8s\t%s\n"), je.JobId,
                 level, edit_uint64_with_commas(je.JobFiles, b1),
                 edit_uint64_with_suffix(je.JobBytes, b2), termstat, dt,
                 JobName);
    } else {
      len = Mmsg(msg, _("%6d  %-6s %8s %10s  %-7s  %-8s %s\n"), je.JobId, level,
                 edit_uint64_with_commas(je.JobFiles, b1),
                 edit_uint64_with_suffix(je.JobBytes, b2), termstat, dt,
                 JobName);
    }
    sp->send(msg, len);
  }

  if (!sp->api) {
    len = PmStrcpy(msg, _("====\n"));
    sp->send(msg, len);
  }
}

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
    Jmsg1(jcr, M_FATAL, 0, _("Bad .status command: %s\n"), jcr->errmsg);
    dir->fsend(_("2900 Bad .status command, missing argument.\n"));
    dir->signal(BNET_EOD);
    FreeMemory(cmd);
    return false;
  }
  UnbashSpaces(cmd);

  if (bstrcmp(cmd, "current")) {
    dir->fsend(OKqstatus, cmd);
    foreach_jcr (njcr) {
      if (njcr->JobId != 0) {
        dir->fsend(DotStatusJob, njcr->JobId, njcr->getJobStatus(),
                   njcr->JobErrors);
      }
    }
    endeach_jcr(njcr);
  } else if (bstrcmp(cmd, "last")) {
    dir->fsend(OKqstatus, cmd);
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
  } else {
    PmStrcpy(jcr->errmsg, dir->msg);
    Jmsg1(jcr, M_FATAL, 0, _("Bad .status command: %s\n"), jcr->errmsg);
    dir->fsend(_("2900 Bad .status command, wrong argument.\n"));
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
      str = _("Base");
      break;
    case L_FULL:
      str = _("Full");
      break;
    case L_INCREMENTAL:
      str = _("Incremental");
      break;
    case L_DIFFERENTIAL:
      str = _("Differential");
      break;
    case L_SINCE:
      str = _("Since");
      break;
    case L_VERIFY_CATALOG:
      str = _("Verify Catalog");
      break;
    case L_VERIFY_INIT:
      str = _("Init Catalog");
      break;
    case L_VERIFY_VOLUME_TO_CATALOG:
      str = _("Volume to Catalog");
      break;
    case L_VERIFY_DISK_TO_CATALOG:
      str = _("Disk to Catalog");
      break;
    case L_VERIFY_DATA:
      str = _("Data");
      break;
    case L_NONE:
      str = " ";
      break;
    default:
      str = _("Unknown Job Level");
      break;
  }
  return str;
}

} /* namespace filedaemon */
