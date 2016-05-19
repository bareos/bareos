/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2001-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2013 Bareos GmbH & Co. KG

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
 * Bareos File Daemon Status routines
 *
 * Kern Sibbald, August MMI
 */

#include "bareos.h"
#include "filed.h"
#include "lib/status.h"

extern void *start_heap;
extern bool GetWindowsVersionString(char *buf, int maxsiz);

/* Forward referenced functions */
static void list_terminated_jobs(STATUS_PKT *sp);
static void list_running_jobs(STATUS_PKT *sp);
static void list_status_header(STATUS_PKT *sp);
static void sendit(POOL_MEM &msg, int len, STATUS_PKT *sp);
static const char *level_to_str(int level);

/* Static variables */
static char qstatus[] =
   ".status %s\n";

static char OKqstatus[] =
   "2000 OK .status\n";
static char DotStatusJob[] =
   "JobId=%d JobStatus=%c JobErrors=%d\n";

#if defined(HAVE_WIN32)
static int privs = 0;
#endif
#ifdef WIN32_VSS
#include "vss.h"
#define VSS " VSS"
#else
#define VSS ""
#endif

/*
 * General status generator
 */
static void output_status(STATUS_PKT *sp)
{
   list_status_header(sp);
   list_running_jobs(sp);
   list_terminated_jobs(sp);
}

static void list_status_header(STATUS_PKT *sp)
{
   int len;
   char dt[MAX_TIME_LENGTH];
   POOL_MEM msg(PM_MESSAGE);
   char b1[32], b2[32], b3[32], b4[32], b5[35];
#if defined(HAVE_WIN32)
   char buf[300];
#endif

   len = Mmsg(msg, _("%s Version: %s (%s) %s %s %s %s\n"),
              my_name, VERSION, BDATE, VSS, HOST_OS, DISTNAME, DISTVER);
   sendit(msg, len, sp);
   bstrftime_nc(dt, sizeof(dt), daemon_start_time);
   len = Mmsg(msg, _("Daemon started %s. Jobs: run=%d running=%d.\n"),
        dt, num_jobs_run, job_count());
   sendit(msg, len, sp);

#if defined(HAVE_WIN32)
   if (GetWindowsVersionString(buf, sizeof(buf))) {
      len = Mmsg(msg, "%s\n", buf);
      sendit(msg, len, sp);
   }

   if (debug_level > 0) {
      if (!privs) {
         privs = enable_backup_privileges(NULL, 1);
      }
      len = Mmsg(msg, "Priv 0x%x\n", privs);
      sendit(msg, len, sp);
      len = Mmsg(msg, "APIs=%sOPT,%sATP,%sLPV,%sCFA,%sCFW,\n",
                 p_OpenProcessToken ? "" : "!",
                 p_AdjustTokenPrivileges ? "" : "!",
                 p_LookupPrivilegeValue ? "" : "!",
                 p_CreateFileA ? "" : "!",
                 p_CreateFileW ? "" : "!");
      sendit(msg, len, sp);
      len = Mmsg(msg, " %sWUL,%sWMKD,%sGFAA,%sGFAW,%sGFAEA,%sGFAEW,%sSFAA,%sSFAW,%sBR,%sBW,%sSPSP,\n",
                 p_wunlink ? "" : "!",
                 p_wmkdir ? "" : "!",
                 p_GetFileAttributesA ? "" : "!",
                 p_GetFileAttributesW ? "" : "!",
                 p_GetFileAttributesExA ? "" : "!",
                 p_GetFileAttributesExW ? "" : "!",
                 p_SetFileAttributesA ? "" : "!",
                 p_SetFileAttributesW ? "" : "!",
                 p_BackupRead ? "" : "!",
                 p_BackupWrite ? "" : "!",
                 p_SetProcessShutdownParameters ? "" : "!");
      sendit(msg, len, sp);
      len = Mmsg(msg, " %sWC2MB,%sMB2WC,%sFFFA,%sFFFW,%sFNFA,%sFNFW,%sSCDA,%sSCDW,\n",
                 p_WideCharToMultiByte ? "" : "!",
                 p_MultiByteToWideChar ? "" : "!",
                 p_FindFirstFileA ? "" : "!",
                 p_FindFirstFileW ? "" : "!",
                 p_FindNextFileA ? "" : "!",
                 p_FindNextFileW ? "" : "!",
                 p_SetCurrentDirectoryA ? "" : "!",
                 p_SetCurrentDirectoryW ? "" : "!");
      sendit(msg, len, sp);
      len = Mmsg(msg, " %sGCDA,%sGCDW,%sGVPNW,%sGVNFVMPW\n",
                 p_GetCurrentDirectoryA ? "" : "!",
                 p_GetCurrentDirectoryW ? "" : "!",
                 p_GetVolumePathNameW ? "" : "!",
                 p_GetVolumeNameForVolumeMountPointW ? "" : "!");
      sendit(msg, len, sp);
   }
#endif

   len = Mmsg(msg, _(" Heap: heap=%s smbytes=%s max_bytes=%s bufs=%s max_bufs=%s\n"),
              edit_uint64_with_commas((char *)sbrk(0)-(char *)start_heap, b1),
              edit_uint64_with_commas(sm_bytes, b2),
              edit_uint64_with_commas(sm_max_bytes, b3),
              edit_uint64_with_commas(sm_buffers, b4),
              edit_uint64_with_commas(sm_max_buffers, b5));
   sendit(msg, len, sp);
   len = Mmsg(msg, _(" Sizeof: boffset_t=%d size_t=%d debug=%d trace=%d "
                     "bwlimit=%skB/s\n"), sizeof(boffset_t), sizeof(size_t),
              debug_level, get_trace(), edit_uint64_with_commas(me->max_bandwidth_per_job / 1024, b1));
   sendit(msg, len, sp);

   if (me->secure_erase_cmdline) {
      len = Mmsg(msg, _(" secure erase command='%s'\n"), me->secure_erase_cmdline);
      sendit(msg, len, sp);
   }

   len = list_fd_plugins(msg);
   if (len > 0) {
      sendit(msg, len, sp);
   }
}

static void list_running_jobs_plain(STATUS_PKT *sp)
{
   JCR *njcr;
   int len, sec, bps;
   bool found = false;
   POOL_MEM msg(PM_MESSAGE);
   char dt[MAX_TIME_LENGTH], b1[32], b2[32], b3[32], b4[32];

   /*
    * List running jobs
    */
   Dmsg0(1000, "Begin status jcr loop.\n");
   len = Mmsg(msg, _("\nRunning Jobs:\n"));
   sendit(msg, len, sp);

   foreach_jcr(njcr) {
      bstrftime_nc(dt, sizeof(dt), njcr->start_time);
      if (njcr->JobId > 0) {
         len = Mmsg(msg, _("JobId %d Job %s is running.\n"),
                    njcr->JobId, njcr->Job);
         sendit(msg, len, sp);
#ifdef WIN32_VSS
         len = Mmsg(msg, _("    %s%s %s Job started: %s\n"),
                    (njcr->pVSSClient && njcr->pVSSClient->IsInitialized()) ? "VSS "  : "",
                    level_to_str(njcr->getJobLevel()),
                    job_type_to_str(njcr->getJobType()), dt);
#else
         len = Mmsg(msg, _("    %s %s Job started: %s\n"),
                    level_to_str(njcr->getJobLevel()),
                    job_type_to_str(njcr->getJobType()), dt);
#endif
      } else if ((njcr->JobId == 0) && (njcr->director)) {
         len = Mmsg(msg, _("%s (director) connected at: %s\n"), njcr->director->name(), dt);
      } else {
         /*
          * This should not occur shortly, until the JCR values are set.
          */
         len = Mmsg(msg, _("Unknown connection, started at: %s\n"), dt);
      }
      sendit(msg, len, sp);
      if (njcr->JobId == 0) {
         continue;
      }
      sec = time(NULL) - njcr->start_time;
      if (sec <= 0) {
         sec = 1;
      }
      bps = (int)(njcr->JobBytes / sec);
      len = Mmsg(msg,  _("    Files=%s Bytes=%s Bytes/sec=%s Errors=%d\n"
                         "    Bwlimit=%s\n"),
                 edit_uint64_with_commas(njcr->JobFiles, b1),
                 edit_uint64_with_commas(njcr->JobBytes, b2),
                 edit_uint64_with_commas(bps, b3),
                 njcr->JobErrors,
                 edit_uint64_with_commas(njcr->max_bandwidth, b4));
      sendit(msg, len, sp);
      len = Mmsg(msg, _("    Files Examined=%s\n"),
                 edit_uint64_with_commas(njcr->num_files_examined, b1));
      sendit(msg, len, sp);
      if (njcr->JobFiles > 0) {
         njcr->lock();
         len = Mmsg(msg, _("    Processing file: %s\n"), njcr->last_fname);
         njcr->unlock();
         sendit(msg, len, sp);
      }

      found = true;
      if (njcr->store_bsock) {
         len = Mmsg(msg, "    SDReadSeqNo=%" lld " fd=%d\n",
                    njcr->store_bsock->read_seqno, njcr->store_bsock->m_fd);
         sendit(msg, len, sp);
      } else {
         len = Mmsg(msg, _("    SDSocket closed.\n"));
         sendit(msg, len, sp);
      }
   }
   endeach_jcr(njcr);

   if (!found) {
      len = Mmsg(msg, _("No Jobs running.\n"));
      sendit(msg, len, sp);
   }

   len = pm_strcpy(msg, _("====\n"));
   sendit(msg, len, sp);
}

static void list_running_jobs_api(STATUS_PKT *sp)
{
   JCR *njcr;
   int len, sec, bps;
   POOL_MEM msg(PM_MESSAGE);
   char dt[MAX_TIME_LENGTH], b1[32], b2[32], b3[32], b4[32];

   /*
    * List running jobs for Bat/Bweb (simple to parse)
    */
   foreach_jcr(njcr) {
      bstrutime(dt, sizeof(dt), njcr->start_time);
      if (njcr->JobId == 0) {
         len = Mmsg(msg, "DirectorConnected=%s\n", dt);
      } else {
         len = Mmsg(msg, "JobId=%d\n Job=%s\n",
                    njcr->JobId, njcr->Job);
         sendit(msg, len, sp);
#ifdef WIN32_VSS
         len = Mmsg(msg," VSS=%d\n Level=%c\n JobType=%c\n JobStarted=%s\n",
                    (njcr->pVSSClient && njcr->pVSSClient->IsInitialized()) ? 1 : 0,
                    njcr->getJobLevel(), njcr->getJobType(), dt);
#else
         len = Mmsg(msg," VSS=%d\n Level=%c\n JobType=%c\n JobStarted=%s\n",
                    0, njcr->getJobLevel(), njcr->getJobType(), dt);
#endif
      }
      sendit(msg, len, sp);
      if (njcr->JobId == 0) {
         continue;
      }
      sec = time(NULL) - njcr->start_time;
      if (sec <= 0) {
         sec = 1;
      }
      bps = (int)(njcr->JobBytes / sec);
      len = Mmsg(msg, " Files=%s\n Bytes=%s\n Bytes/sec=%s\n Errors=%d\n"
                      " Bwlimit=%s\n",
                 edit_uint64(njcr->JobFiles, b1),
                 edit_uint64(njcr->JobBytes, b2),
                 edit_uint64(bps, b3),
                 njcr->JobErrors, edit_int64(njcr->max_bandwidth, b4));
      sendit(msg, len, sp);
      len = Mmsg(msg, " Files Examined=%s\n",
           edit_uint64(njcr->num_files_examined, b1));
      sendit(msg, len, sp);
      if (njcr->JobFiles > 0) {
         njcr->lock();
         len = Mmsg(msg, " Processing file=%s\n", njcr->last_fname);
         njcr->unlock();
         sendit(msg, len, sp);
      }

      if (njcr->store_bsock) {
         len = Mmsg(msg, " SDReadSeqNo=%" lld "\n fd=%d\n",
             njcr->store_bsock->read_seqno, njcr->store_bsock->m_fd);
         sendit(msg, len, sp);
      } else {
         len = Mmsg(msg, _(" SDSocket=closed\n"));
         sendit(msg, len, sp);
      }
   }
   endeach_jcr(njcr);
}

static void  list_running_jobs(STATUS_PKT *sp)
{
   if (sp->api) {
      list_running_jobs_api(sp);
   } else {
      list_running_jobs_plain(sp);
   }
}

static void list_terminated_jobs(STATUS_PKT *sp)
{
   int len;
   struct s_last_job *je;
   POOL_MEM msg(PM_MESSAGE);
   char level[10], dt[MAX_TIME_LENGTH], b1[30], b2[30];

   if (!sp->api) {
      len = pm_strcpy(msg, _("\nTerminated Jobs:\n"));
      sendit(msg, len, sp);
   }

   if (last_jobs->size() == 0) {
      if (!sp->api) {
         len = pm_strcpy(msg, _("====\n"));
         sendit(msg, len, sp);
      }
      return;
   }

   lock_last_jobs_list();

   if (!sp->api) {
      len = pm_strcpy(msg, _(" JobId  Level    Files      Bytes   Status   Finished        Name \n"));
      sendit(msg, len, sp);
      len = pm_strcpy(msg, _("======================================================================\n"));
      sendit(msg, len, sp);
   }

   foreach_dlist(je, last_jobs) {
      char *p;
      char JobName[MAX_NAME_LENGTH];
      const char *termstat;

      bstrftime_nc(dt, sizeof(dt), je->end_time);

      switch (je->JobType) {
      case JT_ADMIN:
      case JT_RESTORE:
         bstrncpy(level, "    ", sizeof(level));
         break;
      default:
         bstrncpy(level, level_to_str(je->JobLevel), sizeof(level));
         level[4] = 0;
         break;
      }

      switch (je->JobStatus) {
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
      bstrncpy(JobName, je->Job, sizeof(JobName));

      /*
       * There are three periods after the Job name
       */
      for (int i=0; i<3; i++) {
         if ((p=strrchr(JobName, '.')) != NULL) {
            *p = 0;
         }
      }

      if (sp->api) {
         len = Mmsg(msg, _("%6d\t%-6s\t%8s\t%10s\t%-7s\t%-8s\t%s\n"),
                    je->JobId,
                    level,
                    edit_uint64_with_commas(je->JobFiles, b1),
                    edit_uint64_with_suffix(je->JobBytes, b2),
                    termstat,
                    dt,
                    JobName);
      } else {
         len = Mmsg(msg, _("%6d  %-6s %8s %10s  %-7s  %-8s %s\n"),
                    je->JobId,
                    level,
                    edit_uint64_with_commas(je->JobFiles, b1),
                    edit_uint64_with_suffix(je->JobBytes, b2),
                    termstat,
                    dt,
                    JobName);
      }
      sendit(msg, len, sp);
   }

   unlock_last_jobs_list();

   if (!sp->api) {
      len = pm_strcpy(msg, _("====\n"));
      sendit(msg, len, sp);
   }
}

/*
 * Send to bsock (Director or Console)
 */
static void sendit(POOL_MEM &msg, int len, STATUS_PKT *sp)
{
   BSOCK *bs = sp->bs;

   if (bs) {
      memcpy(bs->msg, msg.c_str(), len+1);
      bs->msglen = len+1;
      bs->send();
   } else {
      sp->callback(msg.c_str(), len, sp->context);
   }
}

/*
 * Status command from Director
 */
bool status_cmd(JCR *jcr)
{
   BSOCK *user = jcr->dir_bsock;
   STATUS_PKT sp;

   user->fsend("\n");
   sp.bs = user;
   sp.api = false;                         /* no API output */
   output_status(&sp);

   user->signal(BNET_EOD);
   return true;
}

/*
 * .status command from Director
 */
bool qstatus_cmd(JCR *jcr)
{
   BSOCK *dir = jcr->dir_bsock;
   POOLMEM *cmd;
   JCR *njcr;
   s_last_job* job;
   STATUS_PKT sp;

   sp.bs = dir;
   cmd = get_memory(dir->msglen+1);

   if (sscanf(dir->msg, qstatus, cmd) != 1) {
      pm_strcpy(&jcr->errmsg, dir->msg);
      Jmsg1(jcr, M_FATAL, 0, _("Bad .status command: %s\n"), jcr->errmsg);
      dir->fsend(_("2900 Bad .status command, missing argument.\n"));
      dir->signal(BNET_EOD);
      free_memory(cmd);
      return false;
   }
   unbash_spaces(cmd);

   if (bstrcmp(cmd, "current")) {
      dir->fsend(OKqstatus, cmd);
      foreach_jcr(njcr) {
         if (njcr->JobId != 0) {
            dir->fsend(DotStatusJob, njcr->JobId, njcr->JobStatus, njcr->JobErrors);
         }
      }
      endeach_jcr(njcr);
   } else if (bstrcmp(cmd, "last")) {
      dir->fsend(OKqstatus, cmd);
      if ((last_jobs) && (last_jobs->size() > 0)) {
         job = (s_last_job*)last_jobs->last();
         dir->fsend(DotStatusJob, job->JobId, job->JobStatus, job->Errors);
      }
   } else if (bstrcasecmp(cmd, "header")) {
       sp.api = true;
       list_status_header(&sp);
   } else if (bstrcasecmp(cmd, "running")) {
       sp.api = true;
       list_running_jobs(&sp);
   } else if (bstrcasecmp(cmd, "terminated")) {
       sp.api = true;
       list_terminated_jobs(&sp);
   } else {
      pm_strcpy(&jcr->errmsg, dir->msg);
      Jmsg1(jcr, M_FATAL, 0, _("Bad .status command: %s\n"), jcr->errmsg);
      dir->fsend(_("2900 Bad .status command, wrong argument.\n"));
      dir->signal(BNET_EOD);
      free_memory(cmd);
      return false;
   }

   dir->signal(BNET_EOD);
   free_memory(cmd);
   return true;
}

/*
 * Convert Job Level into a string
 */
static const char *level_to_str(int level)
{
   const char *str;

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


#if defined(HAVE_WIN32)
int bareosstat = 0;

/*
 * Put message in Window List Box
 */
char *bareos_status(char *buf, int buf_len)
{
   JCR *njcr;
   const char *termstat = _("Bareos Client: Idle");
   struct s_last_job *job;
   int status = 0;                      /* Idle */

   if (!last_jobs) {
      goto done;
   }
   Dmsg0(1000, "Begin bareos_status jcr loop.\n");
   foreach_jcr(njcr) {
      if (njcr->JobId != 0) {
         status = JS_Running;
         termstat = _("Bareos Client: Running");
         break;
      }
   }
   endeach_jcr(njcr);

   if (status != 0) {
      goto done;
   }
   if (last_jobs->size() > 0) {
      job = (struct s_last_job *)last_jobs->last();
      status = job->JobStatus;
      switch (job->JobStatus) {
      case JS_Canceled:
         termstat = _("Bareos Client: Last Job Canceled");
         break;
      case JS_ErrorTerminated:
      case JS_FatalError:
         termstat = _("Bareos Client: Last Job Failed");
         break;
      default:
         if (job->Errors) {
            termstat = _("Bareos Client: Last Job had Warnings");
         }
         break;
      }
   }
   Dmsg0(1000, "End bareos_status jcr loop.\n");
done:
   bareosstat = status;
   if (buf) {
      bstrncpy(buf, termstat, buf_len);
   }
   return buf;
}

#endif /* HAVE_WIN32 */
