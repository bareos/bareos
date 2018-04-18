/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2001-2012 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2016 Bareos GmbH & Co. KG

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
 * Kern Sibbald, May MMI
 */
/**
 * @file
 * This file handles accepting Director Commands
 *
 * Most Director commands are handled here, with the
 * exception of the Job command command and subsequent
 * subcommands that are handled
 * in job.c.
 *
 * N.B. in this file, in general we must use P(dev->mutex) rather
 * than dev->r_lock() so that we can examine the blocked
 * state rather than blocking ourselves because a Job
 * thread has the device blocked. In some "safe" cases,
 * we can do things to a blocked device. CAREFUL!!!!
 *
 * File daemon commands are handled in fd_cmds.c
 */

#include "bareos.h"
#include "stored.h"
#include "stored/acquire.h"
#include "stored/authenticate.h"
#include "stored/autochanger.h"
#include "stored/fd_cmds.h"
#include "stored/job.h"
#include "stored/label.h"
#include "stored/ndmp_tape.h"
#include "stored/parse_bsr.h"
#include "stored/sd_cmds.h"
#include "stored/sd_stats.h"
#include "stored/wait.h"

/* Exported variables */

/* Imported variables */
extern struct s_last_job last_job;
extern bool init_done;

/* Commands received from director that need scanning */
static char setbandwidth[] =
   "setbandwidth=%lld Job=%127s";
static char setdebugv0cmd[] =
   "setdebug=%d trace=%d";
static char setdebugv1cmd[] =
   "setdebug=%d trace=%d timestamp=%d";
static char cancelcmd[] =
   "cancel Job=%127s";
static char relabelcmd[] =
   "relabel %127s OldName=%127s NewName=%127s PoolName=%127s "
   "MediaType=%127s Slot=%hd drive=%hd MinBlocksize=%d MaxBlocksize=%d";
static char labelcmd[] =
   "label %127s VolumeName=%127s PoolName=%127s "
   "MediaType=%127s Slot=%hd drive=%hd MinBlocksize=%d MaxBlocksize=%d";
static char mountslotcmd[] =
   "mount %127s drive=%hd slot=%hd";
static char mountcmd[] =
   "mount %127s drive=%hd";
static char unmountcmd[] =
   "unmount %127s drive=%hd";
#if 0
static char actionopcmd[] =
   "action_on_purge %127s vol=%127s action=%d";
#endif
static char releasecmd[] =
   "release %127s drive=%hd";
static char readlabelcmd[] =
   "readlabel %127s Slot=%hd drive=%hd";
static char replicatecmd[] =
   "replicate Job=%127s address=%s port=%d ssl=%d Authorization=%100s";
static char passiveclientcmd[] =
   "passive client address=%s port=%d ssl=%d";
static char resolvecmd[] =
   "resolve %s";
static char pluginoptionscmd[] =
   "pluginoptions %s";

/* Responses sent to Director */
static char derrmsg[] =
   "3900 Invalid command:";
static char OKsetdebugv0[] =
   "3000 OK setdebug=%d tracefile=%s\n";
static char OKsetdebugv1[] =
   "3000 OK setdebug=%d trace=%d timestamp=%d tracefile=%s\n";
static char invalid_cmd[] =
   "3997 Invalid command for a Director with Monitor directive enabled.\n";
static char OK_bootstrap[] =
   "3000 OK bootstrap\n";
static char ERROR_bootstrap[] =
   "3904 Error bootstrap\n";
static char OK_replicate[] =
   "3000 OK replicate\n";
static char BADcmd[] =
   "3991 Bad %s command: %s\n";
static char OKBandwidth[] =
   "2000 OK Bandwidth\n";
static char OKpassive[] =
   "2000 OK passive client\n";
static char OKpluginoptions[] =
   "2000 OK plugin options\n";
static char OKsecureerase[] =
   "2000 OK SDSecureEraseCmd %s \n";

/* Imported functions */
extern bool finish_cmd(JobControlRecord *jcr);
extern bool job_cmd(JobControlRecord *jcr);
extern bool nextrun_cmd(JobControlRecord *jcr);
extern bool dotstatus_cmd(JobControlRecord *jcr);
//extern bool query_cmd(JobControlRecord *jcr);
extern bool status_cmd(JobControlRecord *sjcr);
extern bool use_cmd(JobControlRecord *jcr);
extern bool stats_cmd(JobControlRecord *jcr);

extern bool do_job_run(JobControlRecord *jcr);
extern bool do_mac_run(JobControlRecord *jcr);
extern void terminate_child();

/* Forward referenced functions */
//static bool action_on_purge_cmd(JobControlRecord *jcr);
static bool bootstrap_cmd(JobControlRecord *jcr);
static bool cancel_cmd(JobControlRecord *cjcr);
static bool changer_cmd(JobControlRecord *jcr);
static bool die_cmd(JobControlRecord *jcr);
static bool label_cmd(JobControlRecord *jcr);
static bool listen_cmd(JobControlRecord *jcr);
static bool mount_cmd(JobControlRecord *jcr);
static bool passive_cmd(JobControlRecord *jcr);
static bool pluginoptions_cmd(JobControlRecord *jcr);
static bool readlabel_cmd(JobControlRecord *jcr);
static bool resolve_cmd(JobControlRecord *jcr);
static bool relabel_cmd(JobControlRecord *jcr);
static bool release_cmd(JobControlRecord *jcr);
static bool replicate_cmd(JobControlRecord *jcr);
static bool run_cmd(JobControlRecord *jcr);
static bool secureerasereq_cmd(JobControlRecord *jcr);
static bool setbandwidth_cmd(JobControlRecord *jcr);
static bool setdebug_cmd(JobControlRecord *jcr);
static bool unmount_cmd(JobControlRecord *jcr);

static DeviceControlRecord *find_device(JobControlRecord *jcr, PoolMem &dev_name,
                        drive_number_t drive, BlockSizes *blocksizes);
static void read_volume_label(JobControlRecord *jcr, DeviceControlRecord *dcr, Device *dev, slot_number_t slot);
static void label_volume_if_ok(DeviceControlRecord *dcr, char *oldname,
                               char *newname, char *poolname,
                               slot_number_t Slot, bool relabel);
static int try_autoload_device(JobControlRecord *jcr, DeviceControlRecord *dcr, slot_number_t slot, const char *VolName);
static void send_dir_busy_message(BareosSocket *dir, Device *dev);

struct s_cmds {
   const char *cmd;
   bool (*func)(JobControlRecord *jcr);
   bool monitoraccess;                      /* set if monitors can access this cmd */
};

/**
 * The following are the recognized commands from the Director.
 *
 * Keywords are sorted first longest match when the keywords start with the same string.
 */
static struct s_cmds cmds[] = {
// { "action_on_purge",  action_on_purge_cmd, false },
   { "autochanger", changer_cmd, false },
   { "bootstrap", bootstrap_cmd, false },
   { "cancel", cancel_cmd, false },
   { ".die", die_cmd, false },
   { "finish", finish_cmd, false },         /**< End of backup */
   { "JobId=", job_cmd, false },            /**< Start Job */
   { "label", label_cmd, false },           /**< Label a tape */
   { "listen", listen_cmd, false },         /**< Listen for an incoming Storage Job */
   { "mount", mount_cmd, false },
   { "nextrun", nextrun_cmd, false },       /**< Prepare for next backup/restore part of same Job */
   { "passive", passive_cmd, false },
   { "pluginoptions", pluginoptions_cmd, false },
// { "query", query_cmd, false },
   { "readlabel", readlabel_cmd, false },
   { "relabel", relabel_cmd, false },       /**< Relabel a tape */
   { "release", release_cmd, false },
   { "resolve", resolve_cmd, false },
   { "replicate", replicate_cmd, false },   /**< Replicate data to an external SD */
   { "run", run_cmd, false },               /**< Start of Job */
   { "getSecureEraseCmd", secureerasereq_cmd, false },
   { "setbandwidth=", setbandwidth_cmd, false },
   { "setdebug=", setdebug_cmd, false },    /**< Set debug level */
   { "stats", stats_cmd, false },
   { "status", status_cmd, true },
   { ".status", dotstatus_cmd, true },
   { "unmount", unmount_cmd, false },
   { "use storage=", use_cmd, false },
   { NULL, NULL, false } /**< list terminator */
};

 /*
  * Count the number of running jobs.
  */
static inline bool count_running_jobs()
{
   JobControlRecord *jcr;
   unsigned int cnt = 0;

   foreach_jcr(jcr) {
      cnt++;
   }
   endeach_jcr(jcr);

   return (cnt >= me->MaxConcurrentJobs) ? false : true;
}

/**
 * Connection request from an director.
 *
 * Basic tasks done here:
 *  - Create a JobControlRecord record
 *  - Authenticate the Director
 *  - We wait for a command
 *  - We execute the command
 *  - We continue or exit depending on the return status
 */
void *handle_director_connection(BareosSocket *dir)
{
   JobControlRecord *jcr;
   int i, errstat;
   int bnet_stat = 0;
   bool found, quit;

   if (!count_running_jobs()) {
      Emsg0(M_ERROR, 0, _("Number of Jobs exhausted, please increase MaximumConcurrentJobs\n"));
      dir->signal(BNET_TERMINATE);
      return NULL;
   }

   /*
    * This is a connection from the Director, so setup a JobControlRecord
    */
   jcr = new_jcr(sizeof(JobControlRecord), stored_free_jcr); /* create Job Control Record */
   new_plugins(jcr);                            /* instantiate plugins */
   jcr->dir_bsock = dir;                        /* save Director bsock */
   jcr->dir_bsock->set_jcr(jcr);
   jcr->dcrs = New(alist(10, not_owned_by_alist));

   /*
    * Initialize Start Job condition variable
    */
   errstat = pthread_cond_init(&jcr->job_start_wait, NULL);
   if (errstat != 0) {
      berrno be;
      Jmsg1(jcr, M_FATAL, 0, _("Unable to init job start cond variable: ERR=%s\n"), be.bstrerror(errstat));
      goto bail_out;
   }

   /*
    * Initialize End Job condition variable
    */
   errstat = pthread_cond_init(&jcr->job_end_wait, NULL);
   if (errstat != 0) {
      berrno be;
      Jmsg1(jcr, M_FATAL, 0, _("Unable to init job end cond variable: ERR=%s\n"), be.bstrerror(errstat));
      goto bail_out;
   }

   Dmsg0(1000, "stored in start_job\n");

   set_jcr_in_tsd(jcr);

   /*
    * Authenticate the Director
    */
   if (!authenticate_director(jcr)) {
      Jmsg(jcr, M_FATAL, 0, _("Unable to authenticate Director\n"));
      goto bail_out;
   }

   Dmsg0(90, "Message channel init completed.\n");

   quit = false;
   while (!quit) {
      /*
       * Read command
       */
      if ((bnet_stat = dir->recv()) <= 0) {
         break;               /* connection terminated */
      }

      Dmsg1(199, "<dird: %s", dir->msg);

      /*
       * Ensure that device initialization is complete
       */
      while (!init_done) {
         bmicrosleep(1, 0);
      }

      found = false;
      for (i = 0; cmds[i].cmd; i++) {
        if (bstrncmp(cmds[i].cmd, dir->msg, strlen(cmds[i].cmd))) {
           if ((!cmds[i].monitoraccess) && (jcr->director->monitor)) {
              Dmsg1(100, "Command \"%s\" is invalid.\n", cmds[i].cmd);
              dir->fsend(invalid_cmd);
              dir->signal(BNET_EOD);
              break;
           }
           Dmsg1(200, "Do command: %s\n", cmds[i].cmd);
           if (!cmds[i].func(jcr)) { /* do command */
              quit = true; /* error, get out */
              Dmsg1(190, "Command %s requests quit\n", cmds[i].cmd);
           }
           found = true;             /* indicate command found */
           break;
        }
      }
      if (!found) {                   /* command not found */
        PoolMem err_msg;
        Mmsg(err_msg, "%s %s\n", derrmsg, dir->msg);
        dir->fsend(err_msg.c_str());
        break;
      }
   }

bail_out:
   generate_plugin_event(jcr, bsdEventJobEnd);
   dequeue_messages(jcr);             /* send any queued messages */
   dir->signal(BNET_TERMINATE);
   free_plugins(jcr);                 /* release instantiated plugins */
   free_jcr(jcr);

   return NULL;
}

/**
 * Force SD to die, and hopefully dump itself.  Turned on only in development version.
 */
static bool die_cmd(JobControlRecord *jcr)
{
#ifdef DEVELOPER
   JobControlRecord *djcr = NULL;
   int a;
   BareosSocket *dir = jcr->dir_bsock;
   pthread_mutex_t m=PTHREAD_MUTEX_INITIALIZER;

   if (strstr(dir->msg, "deadlock")) {
      Pmsg0(000, "I have been requested to deadlock ...\n");
      P(m);
      P(m);
   }

   Pmsg1(000, "I have been requested to die ... (%s)\n", dir->msg);
   a = djcr->JobId;   /* ref NULL pointer */
   djcr->JobId = a;
#endif
   return false;
}


/**
 * Handles the secureerase request
 * replies the configured secure erase command
 * or "*None*"
 */
static bool secureerasereq_cmd(JobControlRecord *jcr) {
   BareosSocket *dir = jcr->dir_bsock;

   Dmsg1(220,"Secure Erase Cmd Request: %s\n", (me->secure_erase_cmdline ? me->secure_erase_cmdline : "*None*"));

   return dir->fsend(OKsecureerase, (me->secure_erase_cmdline ? me->secure_erase_cmdline : "*None*"));
}

/**
 * Set bandwidth limit as requested by the Director
 */
static bool setbandwidth_cmd(JobControlRecord *jcr)
{
   BareosSocket *dir = jcr->dir_bsock;
   int64_t bw = 0;
   JobControlRecord *cjcr;
   char Job[MAX_NAME_LENGTH];

   *Job = 0;
   if (sscanf(dir->msg, setbandwidth, &bw, Job) != 2 || bw < 0) {
      pm_strcpy(jcr->errmsg, dir->msg);
      dir->fsend(_("2991 Bad setbandwidth command: %s\n"), jcr->errmsg);
      return false;
   }

   if (*Job) {
      if (!(cjcr = get_jcr_by_full_name(Job))) {
         dir->fsend(_("2901 Job %s not found.\n"), Job);
      } else {
         cjcr->max_bandwidth = bw;
         if (cjcr->store_bsock) {
            cjcr->store_bsock->set_bwlimit(bw);
            if (me->allow_bw_bursting) {
               cjcr->store_bsock->set_bwlimit_bursting();
            }
         }
         free_jcr(cjcr);
      }
   } else {                           /* No job requested, apply globally */
      me->max_bandwidth_per_job = bw; /* Overwrite directive */
   }

   return dir->fsend(OKBandwidth);
}

/**
 * Set debug level as requested by the Director
 */
static bool setdebug_cmd(JobControlRecord *jcr)
{
   BareosSocket *dir = jcr->dir_bsock;
   int32_t level, trace_flag, timestamp_flag;
   int scan;

   Dmsg1(10, "setdebug_cmd: %s", dir->msg);
   scan = sscanf(dir->msg, setdebugv1cmd, &level, &trace_flag, &timestamp_flag);
   if (scan != 3) {
      scan = sscanf(dir->msg, setdebugv0cmd, &level, &trace_flag);
   }
   if ((scan != 3 && scan != 2) || level < 0) {
      dir->fsend(BADcmd, "setdebug", dir->msg);
      return false;
   }

   PoolMem tracefilename(PM_FNAME);
   Mmsg(tracefilename, "%s/%s.trace", TRACEFILEDIRECTORY, my_name);

   debug_level = level;
   set_trace(trace_flag);
   if (scan == 3) {
      set_timestamp(timestamp_flag);
      Dmsg4(50, "level=%d trace=%d timestamp=%d tracefilename=%s\n", level, get_trace(), get_timestamp(), tracefilename.c_str());
      return dir->fsend(OKsetdebugv1, level, get_trace(), get_timestamp(), tracefilename.c_str());
   } else {
      Dmsg3(50, "level=%d trace=%d\n", level, get_trace(), tracefilename.c_str());
      return dir->fsend(OKsetdebugv0, level, tracefilename.c_str());
   }
}

/**
 * Cancel a Job
 *   Be careful, we switch to using the job's JobControlRecord! So, using
 *   BSOCKs on that jcr can have two threads in the same code.
 */
static bool cancel_cmd(JobControlRecord *cjcr)
{
   BareosSocket *dir = cjcr->dir_bsock;
   int oldStatus;
   char Job[MAX_NAME_LENGTH];
   JobId_t JobId;
   JobControlRecord *jcr;
   int status;
   const char *reason;

   if (sscanf(dir->msg, cancelcmd, Job) == 1) {
      status = JS_Canceled;
      reason = "canceled";
   } else {
      dir->fsend(_("3903 Error scanning cancel command.\n"));
      goto bail_out;
   }

   /*
    * See if the Jobname is a number only then its a JobId.
    */
   if (is_a_number(Job)) {
      JobId = str_to_int64(Job);
      if (!(jcr = get_jcr_by_id(JobId))) {
         dir->fsend(_("3904 Job %s not found.\n"), Job);
         goto bail_out;
      }
   } else {
      if (!(jcr = get_jcr_by_full_name(Job))) {
         dir->fsend(_("3904 Job %s not found.\n"), Job);
         goto bail_out;
      }
   }

   oldStatus = jcr->JobStatus;
   jcr->setJobStatus(status);

   Dmsg2(800, "Cancel JobId=%d %p\n", jcr->JobId, jcr);
   if (!jcr->authenticated && (oldStatus == JS_WaitFD || oldStatus == JS_WaitSD)) {
      pthread_cond_signal(&jcr->job_start_wait); /* wake waiting thread */
   }

   if (jcr->file_bsock) {
      jcr->file_bsock->set_terminated();
      jcr->file_bsock->set_timed_out();
      Dmsg2(800, "Term bsock jid=%d %p\n", jcr->JobId, jcr);
   } else {
      if (oldStatus != JS_WaitSD) {
         /*
          * Still waiting for FD to connect, release it
          */
         pthread_cond_signal(&jcr->job_start_wait); /* wake waiting job */
         Dmsg2(800, "Signal FD connect jid=%d %p\n", jcr->JobId, jcr);
      }
   }

   /*
    * If thread waiting on mount, wake him
    */
   if (jcr->dcr && jcr->dcr->dev && jcr->dcr->dev->waiting_for_mount()) {
      pthread_cond_broadcast(&jcr->dcr->dev->wait_next_vol);
      Dmsg1(100, "JobId=%u broadcast wait_device_release\n", (uint32_t)jcr->JobId);
      release_device_cond();
   }

   if (jcr->read_dcr && jcr->read_dcr->dev && jcr->read_dcr->dev->waiting_for_mount()) {
      pthread_cond_broadcast(&jcr->read_dcr->dev->wait_next_vol);
      Dmsg1(100, "JobId=%u broadcast wait_device_release\n", (uint32_t)jcr->JobId);
      release_device_cond();
   }

   /*
    * See if the Job has a certain protocol.
    * When canceling a NDMP job make sure we call the end_of_ndmp_* functions.
    */
   switch (jcr->getJobProtocol()) {
   case PT_NDMP_BAREOS:
      switch (jcr->getJobType()) {
      case JT_BACKUP:
         end_of_ndmp_backup(jcr);
         break;
      case JT_RESTORE:
         end_of_ndmp_restore(jcr);
         break;
      default:
         break;
      }
   }

   pthread_cond_signal(&jcr->job_end_wait); /* wake waiting job */
   jcr->my_thread_send_signal(TIMEOUT_SIGNAL);

   dir->fsend(_("3000 JobId=%ld Job=\"%s\" marked to be %s.\n"), jcr->JobId, jcr->Job, reason);
   free_jcr(jcr);

bail_out:
   dir->signal(BNET_EOD);
   return true;
}

/**
 * Resolve a hostname
 */
static bool resolve_cmd(JobControlRecord *jcr)
{
   BareosSocket *dir = jcr->dir_bsock;
   dlist *addr_list;
   const char *errstr;
   char addresses[2048];
   char hostname[2048];

   sscanf(dir->msg, resolvecmd, &hostname);

   if ((addr_list = bnet_host2ipaddrs(hostname, 0, &errstr)) == NULL) {
      dir->fsend(_("%s: Failed to resolve %s\n"), my_name, hostname);
      goto bail_out;
   }

   dir->fsend(_("%s resolves %s to %s\n"), my_name, hostname,
              build_addresses_str(addr_list, addresses, sizeof(addresses), false));
   free_addresses(addr_list);

bail_out:
   dir->signal(BNET_EOD);
   return true;
}

static bool do_label(JobControlRecord *jcr, bool relabel)
{
   int len;
   POOLMEM *newname,
           *oldname,
           *poolname,
           *mediatype;
   PoolMem dev_name;
   BareosSocket *dir = jcr->dir_bsock;
   DeviceControlRecord *dcr;
   Device *dev;
   BlockSizes blocksizes;
   bool ok = false;
   slot_number_t slot;
   drive_number_t drive;

   /*
    * Determine the length of the temporary buffers.
    * If the total length of the incoming message is less
    * then MAX_NAME_LENGTH we can use that as the upper limit.
    * If the incomming message is bigger then MAX_NAME_LENGTH
    * limit the temporary buffer to MAX_NAME_LENGTH bytes as
    * we use a sscanf %127s for reading the temorary buffer.
    */
   len = dir->msglen + 1;
   if (len > MAX_NAME_LENGTH) {
      len = MAX_NAME_LENGTH;
   }

   newname = get_memory(len);
   oldname = get_memory(len);
   poolname = get_memory(len);
   mediatype = get_memory(len);
   if (relabel) {
      if (sscanf(dir->msg, relabelcmd, dev_name.c_str(), oldname,
                 newname, poolname, mediatype, &slot, &drive,
                 &blocksizes.min_block_size, &blocksizes.max_block_size) == 9) {
         ok = true;
      }
   } else {
      *oldname = 0;
      if (sscanf(dir->msg, labelcmd, dev_name.c_str(), newname,
                 poolname, mediatype, &slot, &drive,
                 &blocksizes.min_block_size, &blocksizes.max_block_size) == 8) {
         ok = true;
      }
   }

   if (ok) {
      unbash_spaces(newname);
      unbash_spaces(oldname);
      unbash_spaces(poolname);
      unbash_spaces(mediatype);

      dcr = find_device(jcr, dev_name, drive, &blocksizes);
      if (dcr) {
         dev = dcr->dev;


         dev->Lock();                 /* Use P to avoid indefinite block */
         dcr->VolMinBlocksize = blocksizes.min_block_size;
         dcr->VolMaxBlocksize = blocksizes.max_block_size;
         dev->set_blocksizes(dcr);    /* apply blocksizes from dcr to dev  */

         if (!dev->is_open() && !dev->is_busy()) {
            Dmsg1(400, "Can %slabel. Device is not open\n", relabel ? "re" : "");
            label_volume_if_ok(dcr, oldname, newname, poolname, slot, relabel);
            dev->close(dcr);
         /* Under certain "safe" conditions, we can steal the lock */
         } else if (dev->can_steal_lock()) {
            Dmsg0(400, "Can relabel. can_steal_lock\n");
            label_volume_if_ok(dcr, oldname, newname, poolname, slot, relabel);
         } else if (dev->is_busy() || dev->is_blocked()) {
            send_dir_busy_message(dir, dev);
         } else {                     /* device not being used */
            Dmsg0(400, "Can relabel. device not used\n");
            label_volume_if_ok(dcr, oldname, newname, poolname, slot, relabel);
         }
         dev->Unlock();
         free_dcr(dcr);
      } else {
         dir->fsend(_("3999 Device \"%s\" not found or could not be opened.\n"), dev_name.c_str());
      }
   } else {
      /* NB dir->msg gets clobbered in bnet_fsend, so save command */
      pm_strcpy(jcr->errmsg, dir->msg);
      dir->fsend(_("3903 Error scanning label command: %s\n"), jcr->errmsg);
   }
   free_memory(oldname);
   free_memory(newname);
   free_memory(poolname);
   free_memory(mediatype);
   dir->signal(BNET_EOD);
   return true;
}

/**
 * Label a Volume
 */
static bool label_cmd(JobControlRecord *jcr)
{
   return do_label(jcr, false);
}

static bool relabel_cmd(JobControlRecord *jcr)
{
   return do_label(jcr, true);
}

/**
 * Read the tape label and determine if we can safely
 * label the tape (not a Bareos volume), then label it.
 *
 *  Enter with the mutex set
 */
static void label_volume_if_ok(DeviceControlRecord *dcr, char *oldname,
                               char *newname, char *poolname,
                               slot_number_t slot, bool relabel)
{
   BareosSocket *dir = dcr->jcr->dir_bsock;
   bsteal_lock_t hold;
   Device *dev = dcr->dev;
   int label_status;
   int mode;
   const char *volname = relabel ? oldname : newname;
   char ed1[50];

   steal_device_lock(dev, &hold, BST_WRITING_LABEL);
   Dmsg1(100, "Stole device %s lock, writing label.\n", dev->print_name());

   Dmsg0(90, "try_autoload_device - looking for volume_info\n");
   switch (try_autoload_device(dcr->jcr, dcr, slot, volname)) {
   case -1:
      goto cleanup;
   case -2:
      goto bail_out;
   case 0:
   default:
      break;
   }

   /*
    * Ensure that the device is open -- autoload_device() closes it
    */
   if (dev->is_tape()) {
      mode = OPEN_READ_WRITE;
   } else {
      mode = CREATE_READ_WRITE;
   }

   /*
    * Set old volume name for open if relabeling
    */
   dcr->setVolCatName(volname);

   if (!dev->open(dcr, mode)) {
      dir->fsend(_("3910 Unable to open device \"%s\": ERR=%s\n"), dev->print_name(), dev->bstrerror());
      goto cleanup;
   }

   /*
    * See what we have for a Volume
    */
   label_status = read_dev_volume_label(dcr);

   /*
    * Set new volume name
    */
   dcr->setVolCatName(newname);
   switch(label_status) {
   case VOL_NAME_ERROR:
   case VOL_VERSION_ERROR:
   case VOL_LABEL_ERROR:
   case VOL_OK:
      if (!relabel) {
         dir->fsend(_("3920 Cannot label Volume because it is already labeled: \"%s\"\n"), dev->VolHdr.VolumeName);
         goto cleanup;
      }

      /*
       * Relabel request. If oldname matches, continue
       */
      if (!bstrcmp(oldname, dev->VolHdr.VolumeName)) {
         dir->fsend(_("3921 Wrong volume mounted.\n"));
         goto cleanup;
      }

      if (dev->label_type != B_BAREOS_LABEL) {
         dir->fsend(_("3922 Cannot relabel an ANSI/IBM labeled Volume.\n"));
         goto cleanup;
      }
      /*
       * Fall through wanted!
       */
   case VOL_IO_ERROR:
   case VOL_NO_LABEL:
      if (!write_new_volume_label_to_dev(dcr, newname, poolname, relabel)) {
         dir->fsend(_("3912 Failed to label Volume: ERR=%s\n"), dev->bstrerror());
         goto cleanup;
      }
      bstrncpy(dcr->VolumeName, newname, sizeof(dcr->VolumeName));

      /*
       * The following 3000 OK label. string is scanned in ua_label.c
       */
      dir->fsend("3000 OK label. VolBytes=%s Volume=\"%s\" Device=%s\n",
                 edit_uint64(dev->VolCatInfo.VolCatBytes, ed1),
                 newname, dev->print_name());
      break;
   case VOL_NO_MEDIA:
      dir->fsend(_("3914 Failed to label Volume (no media): ERR=%s\n"), dev->bstrerror());
      break;
   default:
      dir->fsend(_("3913 Cannot label Volume. "
                   "Unknown status %d from read_volume_label()\n"), label_status);
      break;
   }

cleanup:
   if (dev->is_open() && !dev->has_cap(CAP_ALWAYSOPEN)) {
      dev->close(dcr);
   }
   if (!dev->is_open()) {
      dev->clear_volhdr();
   }
   volume_unused(dcr);                   /* no longer using volume */

bail_out:
   give_back_device_lock(dev, &hold);

   return;
}

/**
 * Read the tape label
 *
 *  Enter with the mutex set
 */
static bool read_label(DeviceControlRecord *dcr)
{
   int ok;
   JobControlRecord *jcr = dcr->jcr;
   BareosSocket *dir = jcr->dir_bsock;
   bsteal_lock_t hold;
   Device *dev = dcr->dev;

   steal_device_lock(dev, &hold, BST_DOING_ACQUIRE);

   dcr->VolumeName[0] = 0;
   dev->clear_labeled();              /* force read of label */
   switch (read_dev_volume_label(dcr)) {
   case VOL_OK:
      dir->fsend(_("3001 Mounted Volume: %s\n"), dev->VolHdr.VolumeName);
      ok = true;
      break;
   default:
      dir->fsend(_("3902 Cannot mount Volume on Storage Device \"%s\" because:\n%s"),
         dev->print_name(), jcr->errmsg);
      ok = false;
      break;
   }
   volume_unused(dcr);
   give_back_device_lock(dev, &hold);
   return ok;
}

/**
 * Searches for device by name, and if found, creates a dcr and returns it.
 */
static DeviceControlRecord *find_device(JobControlRecord *jcr, PoolMem &devname, drive_number_t drive, BlockSizes *blocksizes)
{
   DeviceResource *device;
   AutochangerResource *changer;
   bool found = false;
   DeviceControlRecord *dcr = NULL;

   unbash_spaces(devname);
   foreach_res(device, R_DEVICE) {
      /*
       * Find resource, and make sure we were able to open it
       */
      if (bstrcmp(device->name(), devname.c_str())) {
         if (!device->dev) {
            device->dev = init_dev(jcr, device);
         }
         if (!device->dev) {
            Jmsg(jcr, M_WARNING, 0, _("\n"
                 "     Device \"%s\" requested by DIR could not be opened or does not exist.\n"),
                 devname.c_str());
            continue;
         }
         Dmsg1(20, "Found device %s\n", device->name());
         found = true;
         break;
      }
   }

   if (!found) {
      foreach_res(changer, R_AUTOCHANGER) {
         /*
          * Find resource, and make sure we were able to open it
          */
         if (bstrcmp(devname.c_str(), changer->name())) {
            /*
             * Try each device in this AutoChanger
             */
            foreach_alist(device, changer->device) {
               Dmsg1(100, "Try changer device %s\n", device->name());
               if (!device->dev) {
                  device->dev = init_dev(jcr, device);
               }
               if (!device->dev) {
                  Dmsg1(100, "Device %s could not be opened. Skipped\n", devname.c_str());
                  Jmsg(jcr, M_WARNING, 0, _("\n"
                       "     Device \"%s\" in changer \"%s\" requested by DIR could not be opened or does not exist.\n"),
                       device->name(), devname.c_str());
                  continue;
               }
               if (!device->dev->autoselect) {
                  Dmsg1(100, "Device %s not autoselect skipped.\n", devname.c_str());
                  continue;           /* device is not available */
               }
               if (drive < 0 || drive == device->dev->drive) {
                  Dmsg1(20, "Found changer device %s\n", device->name());
                  found = true;
                  break;
               }
               Dmsg3(100, "Device %s drive wrong: want=%hd got=%hd skipping\n",
                     devname.c_str(), drive, device->dev->drive);
            }
            break;                    /* we found it but could not open a device */
         }
      }
   }

   if (found) {
      Dmsg1(100, "Found device %s\n", device->name());
      dcr = New(StorageDaemonDeviceControlRecord);
      setup_new_dcr_device(jcr, dcr, device->dev, blocksizes);
      dcr->set_will_write();
      dcr->device = device;
   }
   return dcr;
}

/**
 * Mount command from Director
 */
static bool mount_cmd(JobControlRecord *jcr)
{
   PoolMem devname;
   BareosSocket *dir = jcr->dir_bsock;
   Device *dev;
   DeviceControlRecord *dcr;
   drive_number_t drive;
   slot_number_t slot = 0;
   bool ok;

   ok = sscanf(dir->msg, mountslotcmd, devname.c_str(), &drive, &slot) == 3;
   if (!ok) {
      ok = sscanf(dir->msg, mountcmd, devname.c_str(), &drive) == 2;
   }

   Dmsg3(100, "ok=%d drive=%hd slot=%hd\n", ok, drive, slot);
   if (ok) {
      dcr = find_device(jcr, devname, drive, NULL);
      if (dcr) {
         dev = dcr->dev;
         dev->Lock();                  /* Use P to avoid indefinite block */
         Dmsg2(100, "mount cmd blocked=%d must_unload=%d\n",
               dev->blocked(), dev->must_unload());
         switch (dev->blocked()) {         /* device blocked? */
         case BST_WAITING_FOR_SYSOP:
            /* Someone is waiting, wake him */
            Dmsg0(100, "Waiting for mount. Attempting to wake thread\n");
            dev->set_blocked(BST_MOUNT);
            dir->fsend("3001 OK mount requested. %sDevice=%s\n",
                      (slot > 0) ? _("Specified slot ignored. ") : "", dev->print_name());
            pthread_cond_broadcast(&dev->wait_next_vol);
            Dmsg1(100, "JobId=%u broadcast wait_device_release\n", (uint32_t)dcr->jcr->JobId);
            release_device_cond();
            break;

         /* In both of these two cases, we (the user) unmounted the Volume */
         case BST_UNMOUNTED_WAITING_FOR_SYSOP:
         case BST_UNMOUNTED:
            Dmsg2(100, "Unmounted changer=%d slot=%hd\n", dev->is_autochanger(), slot);
            if (dev->is_autochanger() && slot > 0) {
               try_autoload_device(jcr, dcr, slot, "");
            }
            /* We freed the device, so reopen it and wake any waiting threads */
            if (!dev->open(dcr, OPEN_READ_ONLY)) {
               dir->fsend(_("3901 Unable to open device %s: ERR=%s\n"), dev->print_name(), dev->bstrerror());
               if (dev->blocked() == BST_UNMOUNTED) {
                  /* We blocked the device, so unblock it */
                  Dmsg0(100, "Unmounted. Unblocking device\n");
                  unblock_device(dev);
               }
               break;
            }
            read_dev_volume_label(dcr);
            if (dev->blocked() == BST_UNMOUNTED) {
               /* We blocked the device, so unblock it */
               Dmsg0(100, "Unmounted. Unblocking device\n");
               read_label(dcr);       /* this should not be necessary */
               unblock_device(dev);
            } else {
               Dmsg0(100, "Unmounted waiting for mount. Attempting to wake thread\n");
               dev->set_blocked(BST_MOUNT);
            }
            if (dev->is_labeled()) {
               dir->fsend(_("3001 Device %s is mounted with Volume \"%s\"\n"), dev->print_name(), dev->VolHdr.VolumeName);
            } else {
               dir->fsend(_("3905 Device %s open but no Bareos volume is mounted.\n"
                            "If this is not a blank tape, try unmounting and remounting the Volume.\n"), dev->print_name());
            }
            pthread_cond_broadcast(&dev->wait_next_vol);
            Dmsg1(100, "JobId=%u broadcast wait_device_release\n", (uint32_t)dcr->jcr->JobId);
            release_device_cond();
            break;

         case BST_DOING_ACQUIRE:
            dir->fsend(_("3001 Device %s is doing acquire.\n"), dev->print_name());
            break;

         case BST_WRITING_LABEL:
            dir->fsend(_("3903 Device %s is being labeled.\n"), dev->print_name());
            break;

         case BST_NOT_BLOCKED:
            Dmsg2(100, "Not blocked changer=%d slot=%hd\n", dev->is_autochanger(), slot);
            if (dev->is_autochanger() && slot > 0) {
               try_autoload_device(jcr, dcr, slot, "");
            }
            if (dev->is_open()) {
               if (dev->is_labeled()) {
                  dir->fsend(_("3001 Device %s is mounted with Volume \"%s\"\n"), dev->print_name(), dev->VolHdr.VolumeName);
               } else {
                  dir->fsend(_("3905 Device %s open but no Bareos volume is mounted.\n"
                               "If this is not a blank tape, try unmounting and remounting the Volume.\n"), dev->print_name());
               }
            } else if (dev->is_tape()) {
               if (!dev->open(dcr, OPEN_READ_ONLY)) {
                  dir->fsend(_("3901 Unable to open device %s: ERR=%s\n"), dev->print_name(), dev->bstrerror());
                  break;
               }
               read_label(dcr);
               if (dev->is_labeled()) {
                  dir->fsend(_("3001 Device %s is already mounted with Volume \"%s\"\n"), dev->print_name(), dev->VolHdr.VolumeName);
               } else {
                  dir->fsend(_("3905 Device %s open but no Bareos volume is mounted.\n"
                               "If this is not a blank tape, try unmounting and remounting the Volume.\n"), dev->print_name());
               }
               if (dev->is_open() && !dev->has_cap(CAP_ALWAYSOPEN)) {
                  dev->close(dcr);
               }
            } else if (dev->is_unmountable()) {
               if (dev->mount(dcr, 1)) {
                  dir->fsend(_("3002 Device %s is mounted.\n"), dev->print_name());
               } else {
                  dir->fsend(_("3907 %s"), dev->bstrerror());
               }
            } else { /* must be file */
               dir->fsend(_("3906 File device %s is always mounted.\n"), dev->print_name());
               pthread_cond_broadcast(&dev->wait_next_vol);
               Dmsg1(100, "JobId=%u broadcast wait_device_release\n", (uint32_t)dcr->jcr->JobId);
               release_device_cond();
            }
            break;

         case BST_RELEASING:
            dir->fsend(_("3930 Device %s is being released.\n"), dev->print_name());
            break;

         default:
            dir->fsend(_("3905 Unknown wait state %d\n"), dev->blocked());
            break;
         }
         dev->Unlock();
         free_dcr(dcr);
      } else {
         dir->fsend(_("3999 Device %s not found or could not be opened.\n"), devname.c_str());
      }
   } else {
      pm_strcpy(jcr->errmsg, dir->msg);
      dir->fsend(_("3909 Error scanning mount command: %s\n"), jcr->errmsg);
   }
   dir->signal(BNET_EOD);
   return true;
}

/**
 * unmount command from Director
 */
static bool unmount_cmd(JobControlRecord *jcr)
{
   PoolMem devname;
   BareosSocket *dir = jcr->dir_bsock;
   Device *dev;
   DeviceControlRecord *dcr;
   drive_number_t drive;

   if (sscanf(dir->msg, unmountcmd, devname.c_str(), &drive) == 2) {
      dcr = find_device(jcr, devname, drive, NULL);
      if (dcr) {
         dev = dcr->dev;
         dev->Lock();                 /* Use P to avoid indefinite block */
         if (!dev->is_open()) {
            if (!dev->is_busy()) {
               unload_autochanger(dcr, -1);
            }
            if (dev->is_unmountable()) {
               if (dev->unmount(dcr, 0)) {
                  dir->fsend(_("3002 Device \"%s\" unmounted.\n"), dev->print_name());
               } else {
                  dir->fsend(_("3907 %s"), dev->bstrerror());
               }
            } else {
               Dmsg0(90, "Device already unmounted\n");
               dir->fsend(_("3901 Device \"%s\" is already unmounted.\n"),
                  dev->print_name());
            }
         } else if (dev->blocked() == BST_WAITING_FOR_SYSOP) {
            Dmsg2(90, "%d waiter dev_block=%d. doing unmount\n", dev->num_waiting,
               dev->blocked());
            if (!unload_autochanger(dcr, -1)) {
               /* ***FIXME**** what is this ????  */
               dev->close(dcr);
               free_volume(dev);
            }
            if (dev->is_unmountable() && !dev->unmount(dcr, 0)) {
               dir->fsend(_("3907 %s"), dev->bstrerror());
            } else {
               dev->set_blocked(BST_UNMOUNTED_WAITING_FOR_SYSOP);
               dir->fsend(_("3001 Device \"%s\" unmounted.\n"), dev->print_name());
            }

         } else if (dev->blocked() == BST_DOING_ACQUIRE) {
            dir->fsend(_("3902 Device \"%s\" is busy in acquire.\n"), dev->print_name());

         } else if (dev->blocked() == BST_WRITING_LABEL) {
            dir->fsend(_("3903 Device \"%s\" is being labeled.\n"), dev->print_name());

         } else if (dev->is_busy()) {
            send_dir_busy_message(dir, dev);
         } else {                     /* device not being used */
            Dmsg0(90, "Device not in use, unmounting\n");
            /* On FreeBSD, I am having ASSERT() failures in block_device()
             * and I can only imagine that the thread id that we are
             * leaving in no_wait_id is being re-used. So here,
             * we simply do it by hand.  Gross, but a solution.
             */
            /*  block_device(dev, BST_UNMOUNTED); replace with 2 lines below */
            dev->set_blocked(BST_UNMOUNTED);
            clear_thread_id(dev->no_wait_id);
            if (!unload_autochanger(dcr, -1)) {
               dev->close(dcr);
               free_volume(dev);
            }
            if (dev->is_unmountable() && !dev->unmount(dcr, 0)) {
               dir->fsend(_("3907 %s"), dev->bstrerror());
            } else {
               dir->fsend(_("3002 Device \"%s\" unmounted.\n"), dev->print_name());
            }
         }
         dev->Unlock();
         free_dcr(dcr);
      } else {
         dir->fsend(_("3999 Device \"%s\" not found or could not be opened.\n"), devname.c_str());
      }
   } else {
      /* NB dir->msg gets clobbered in bnet_fsend, so save command */
      pm_strcpy(jcr->errmsg, dir->msg);
      dir->fsend(_("3907 Error scanning unmount command: %s\n"), jcr->errmsg);
   }
   dir->signal(BNET_EOD);
   return true;
}

#if 0
/**
 * The truncate command will recycle a volume. The director can call this
 * after purging a volume so that disk space will not be wasted. Only useful
 * for File Storage, of course.
 *
 *
 * It is currently disabled
 */
static bool action_on_purge_cmd(JobControlRecord *jcr)
{
   BareosSocket *dir = jcr->dir_bsock;

   char devname[MAX_NAME_LENGTH];
   char volumename[MAX_NAME_LENGTH];
   int32_t action;

   /* TODO: Need to find a free device and ask for slot to the director */
   if (sscanf(dir->msg, actionopcmd, devname, volumename, &action) != 3)
   {
      dir->fsend(_("3916 Error scanning action_on_purge command\n"));
      goto done;
   }
   unbash_spaces(volumename);
   unbash_spaces(devname);

   /* Check if action is correct */
   if (action & AOP_TRUNCTATE) {

   }
   /* ... */

done:
   dir->signal(BNET_EOD);
   return true;
}
#endif

/**
 * Release command from Director. This rewinds the device and if
 *   configured does a offline and ensures that Bareos will
 *   re-read the label of the tape before continuing. This gives
 *   the operator the chance to change the tape anytime before the
 *   next job starts.
 */
static bool release_cmd(JobControlRecord *jcr)
{
   PoolMem devname;
   BareosSocket *dir = jcr->dir_bsock;
   Device *dev;
   DeviceControlRecord *dcr;
   drive_number_t drive;

   if (sscanf(dir->msg, releasecmd, devname.c_str(), &drive) == 2) {
      dcr = find_device(jcr, devname, drive, NULL);
      if (dcr) {
         dev = dcr->dev;
         dev->Lock();                 /* Use P to avoid indefinite block */
         if (!dev->is_open()) {
            if (!dev->is_busy()) {
               unload_autochanger(dcr, -1);
            }
            Dmsg0(90, "Device already released\n");
            dir->fsend(_("3921 Device \"%s\" already released.\n"),
               dev->print_name());

         } else if (dev->blocked() == BST_WAITING_FOR_SYSOP) {
            Dmsg2(90, "%d waiter dev_block=%d.\n", dev->num_waiting,
               dev->blocked());
            unload_autochanger(dcr, -1);
            dir->fsend(_("3922 Device \"%s\" waiting for sysop.\n"),
               dev->print_name());

         } else if (dev->blocked() == BST_UNMOUNTED_WAITING_FOR_SYSOP) {
            Dmsg2(90, "%d waiter dev_block=%d. doing unmount\n", dev->num_waiting,
               dev->blocked());
            dir->fsend(_("3922 Device \"%s\" waiting for mount.\n"),
               dev->print_name());

         } else if (dev->blocked() == BST_DOING_ACQUIRE) {
            dir->fsend(_("3923 Device \"%s\" is busy in acquire.\n"),
               dev->print_name());

         } else if (dev->blocked() == BST_WRITING_LABEL) {
            dir->fsend(_("3914 Device \"%s\" is being labeled.\n"),
               dev->print_name());

         } else if (dev->is_busy()) {
            send_dir_busy_message(dir, dev);
         } else {                     /* device not being used */
            Dmsg0(90, "Device not in use, releasing\n");
            dcr->release_volume();
            dir->fsend(_("3022 Device \"%s\" released.\n"),
               dev->print_name());
         }
         dev->Unlock();
         free_dcr(dcr);
      } else {
         dir->fsend(_("3999 Device \"%s\" not found or could not be opened.\n"), devname.c_str());
      }
   } else {
      /* NB dir->msg gets clobbered in bnet_fsend, so save command */
      pm_strcpy(jcr->errmsg, dir->msg);
      dir->fsend(_("3927 Error scanning release command: %s\n"), jcr->errmsg);
   }
   dir->signal(BNET_EOD);
   return true;
}

static pthread_mutex_t bsr_mutex = PTHREAD_MUTEX_INITIALIZER;
static uint32_t bsr_uniq = 0;

static inline bool get_bootstrap_file(JobControlRecord *jcr, BareosSocket *sock)
{
   POOLMEM *fname = get_pool_memory(PM_FNAME);
   FILE *bs;
   bool ok = false;

   if (jcr->RestoreBootstrap) {
      secure_erase(jcr, jcr->RestoreBootstrap);
      free_pool_memory(jcr->RestoreBootstrap);
   }
   P(bsr_mutex);
   bsr_uniq++;
   Mmsg(fname, "%s/%s.%s.%d.bootstrap", me->working_directory, me->name(),
      jcr->Job, bsr_uniq);
   V(bsr_mutex);
   Dmsg1(400, "bootstrap=%s\n", fname);
   jcr->RestoreBootstrap = fname;
   bs = fopen(fname, "a+b");           /* create file */
   if (!bs) {
      berrno be;
      Jmsg(jcr, M_FATAL, 0, _("Could not create bootstrap file %s: ERR=%s\n"),
           jcr->RestoreBootstrap, be.bstrerror());
      goto bail_out;
   }
   Dmsg0(10, "=== Bootstrap file ===\n");
   while (sock->recv() >= 0) {
       Dmsg1(10, "%s", sock->msg);
       fputs(sock->msg, bs);
   }
   fclose(bs);
   Dmsg0(10, "=== end bootstrap file ===\n");
   jcr->bsr = parse_bsr(jcr, jcr->RestoreBootstrap);
   if (!jcr->bsr) {
      Jmsg(jcr, M_FATAL, 0, _("Error parsing bootstrap file.\n"));
      goto bail_out;
   }
   if (debug_level >= 10) {
      dump_bsr(jcr->bsr, true);
   }
   /* If we got a bootstrap, we are reading, so create read volume list */
   create_restore_volume_list(jcr);
   ok = true;

bail_out:
   secure_erase(jcr, jcr->RestoreBootstrap);
   free_pool_memory(jcr->RestoreBootstrap);
   jcr->RestoreBootstrap = NULL;
   if (!ok) {
      sock->fsend(ERROR_bootstrap);
      return false;
   }
   return sock->fsend(OK_bootstrap);
}

static bool bootstrap_cmd(JobControlRecord *jcr)
{
   return get_bootstrap_file(jcr, jcr->dir_bsock);
}

/**
 * Autochanger command from Director
 */
static bool changer_cmd(JobControlRecord *jcr)
{
   slot_number_t src_slot, dst_slot;
   PoolMem devname;
   BareosSocket *dir = jcr->dir_bsock;
   Device *dev;
   DeviceControlRecord *dcr;
   const char *cmd = NULL;
   bool ok = false;
   bool is_transfer = false;
   /*
    * A safe_cmd may call autochanger script but does not load/unload
    *    slots so it can be done at the same time that the drive is open.
    */
   bool safe_cmd = false;

   if (sscanf(dir->msg, "autochanger listall %127s", devname.c_str()) == 1) {
      cmd = "listall";
      safe_cmd = ok = true;
   } else if (sscanf(dir->msg, "autochanger list %127s", devname.c_str()) == 1) {
      cmd = "list";
      safe_cmd = ok = true;
   } else if (sscanf(dir->msg, "autochanger slots %127s", devname.c_str()) == 1) {
      cmd = "slots";
      safe_cmd = ok = true;
   } else if (sscanf(dir->msg, "autochanger drives %127s", devname.c_str()) == 1) {
      cmd = "drives";
      safe_cmd = ok = true;
   } else if (sscanf(dir->msg, "autochanger transfer %127s %hd %hd",
                     devname.c_str(), &src_slot, &dst_slot) == 3) {
      cmd = "transfer";
      safe_cmd = ok = true;
      is_transfer = true;
   }
   if (ok) {
      dcr = find_device(jcr, devname, -1, NULL);
      if (dcr) {
         dev = dcr->dev;
         dev->Lock();                 /* Use P to avoid indefinite block */
         if (!dev->device->changer_res) {
            dir->fsend(_("3998 Device \"%s\" is not an autochanger.\n"), dev->print_name());
         /* Under certain "safe" conditions, we can steal the lock */
         } else if (safe_cmd || !dev->is_open() || dev->can_steal_lock()) {
            if (is_transfer) {
               autochanger_transfer_cmd(dcr, dir, src_slot, dst_slot);
            } else {
               autochanger_cmd(dcr, dir, cmd);
            }
         } else if (dev->is_busy() || dev->is_blocked()) {
            send_dir_busy_message(dir, dev);
         } else {                     /* device not being used */
            if (is_transfer) {
               autochanger_transfer_cmd(dcr, dir, src_slot, dst_slot);
            } else {
               autochanger_cmd(dcr, dir, cmd);
            }
         }
         dev->Unlock();
         free_dcr(dcr);
      } else {
         dir->fsend(_("3999 Device \"%s\" not found or could not be opened.\n"), devname.c_str());
      }
   } else {  /* error on scanf */
      pm_strcpy(jcr->errmsg, dir->msg);
      dir->fsend(_("3908 Error scanning autochanger drives/list/slots command: %s\n"), jcr->errmsg);
   }
   dir->signal(BNET_EOD);
   return true;
}

/**
 * Read and return the Volume label
 */
static bool readlabel_cmd(JobControlRecord *jcr)
{
   PoolMem devname;
   BareosSocket *dir = jcr->dir_bsock;
   Device *dev;
   DeviceControlRecord *dcr;
   slot_number_t slot;
   drive_number_t drive;

   if (sscanf(dir->msg, readlabelcmd, devname.c_str(), &slot, &drive) == 3) {
      dcr = find_device(jcr, devname, drive, NULL);
      if (dcr) {
         dev = dcr->dev;
         dev->Lock();                 /* Use P to avoid indefinite block */
         if (!dev->is_open()) {
            read_volume_label(jcr, dcr, dev, slot);
            dev->close(dcr);
         /* Under certain "safe" conditions, we can steal the lock */
         } else if (dev->can_steal_lock()) {
            read_volume_label(jcr, dcr, dev, slot);
         } else if (dev->is_busy() || dev->is_blocked()) {
            send_dir_busy_message(dir, dev);
         } else {                     /* device not being used */
            read_volume_label(jcr, dcr, dev, slot);
         }
         dev->Unlock();
         free_dcr(dcr);
      } else {
         dir->fsend(_("3999 Device \"%s\" not found or could not be opened.\n"), devname.c_str());
      }
   } else {
      pm_strcpy(jcr->errmsg, dir->msg);
      dir->fsend(_("3909 Error scanning readlabel command: %s\n"), jcr->errmsg);
   }
   dir->signal(BNET_EOD);
   return true;
}

/**
 * Read the tape label
 *
 *  Enter with the mutex set
 */
static void read_volume_label(JobControlRecord *jcr, DeviceControlRecord *dcr, Device *dev, slot_number_t Slot)
{
   BareosSocket *dir = jcr->dir_bsock;
   bsteal_lock_t hold;

   dcr->set_dev(dev);
   steal_device_lock(dev, &hold, BST_WRITING_LABEL);

   switch (try_autoload_device(dcr->jcr, dcr, Slot, "")) {
   case -1:
   case -2:
      goto bail_out;
   case 0:
   default:
      break;
   }

   dev->clear_labeled();              /* force read of label */
   switch (read_dev_volume_label(dcr)) {
   case VOL_OK:
      /* DO NOT add quotes around the Volume name. It is scanned in the DIR */
      dir->fsend(_("3001 Volume=%s Slot=%hd\n"), dev->VolHdr.VolumeName, Slot);
      Dmsg1(100, "Volume: %s\n", dev->VolHdr.VolumeName);
      break;
   default:
      dir->fsend(_("3902 Cannot mount Volume on Storage Device \"%s\" because:\n%s"), dev->print_name(), jcr->errmsg);
      break;
   }

bail_out:
   give_back_device_lock(dev, &hold);

   return;
}

/**
 * Try autoloading a device.
 *
 * Returns: 1 on success
 *          0 on failure (no changer available)
 *         -1 on error on autochanger
 *         -2 on error locking the autochanger
 */
static int try_autoload_device(JobControlRecord *jcr, DeviceControlRecord *dcr, slot_number_t Slot, const char *VolName)
{
   BareosSocket *dir = jcr->dir_bsock;

   bstrncpy(dcr->VolumeName, VolName, sizeof(dcr->VolumeName));
   dcr->VolCatInfo.Slot = Slot;
   dcr->VolCatInfo.InChanger = Slot > 0;

   return autoload_device(dcr, 0, dir);
}

static void send_dir_busy_message(BareosSocket *dir, Device *dev)
{
   if (dev->is_blocked()) {
      switch (dev->blocked()) {
      case BST_UNMOUNTED:
         dir->fsend(_("3931 Device \"%s\" is BLOCKED. user unmounted.\n"), dev->print_name());
         break;
      case BST_UNMOUNTED_WAITING_FOR_SYSOP:
         dir->fsend(_("3932 Device \"%s\" is BLOCKED. user unmounted during wait for media/mount.\n"), dev->print_name());
         break;
      case BST_WAITING_FOR_SYSOP:
         dir->fsend(_("3933 Device \"%s\" is BLOCKED waiting for media.\n"), dev->print_name());
         break;
      case BST_DOING_ACQUIRE:
         dir->fsend(_("3934 Device \"%s\" is being initialized.\n"),
            dev->print_name());
         break;
      case BST_WRITING_LABEL:
         dir->fsend(_("3935 Device \"%s\" is blocked labeling a Volume.\n"),
            dev->print_name());
         break;
      default:
         dir->fsend(_("3935 Device \"%s\" is blocked for unknown reason.\n"),
            dev->print_name());
         break;
      }
   } else if (dev->can_read()) {
       dir->fsend(_("3936 Device \"%s\" is busy reading.\n"), dev->print_name());
   } else {
       dir->fsend(_("3937 Device \"%s\" is busy with writers=%d reserved=%d.\n"), dev->print_name(), dev->num_writers, dev->num_reserved());
   }
}

static inline void set_storage_auth_key(JobControlRecord *jcr, char *key)
{
   /*
    * If no key don't update anything
    */
   if (!*key) {
      return;
   }

   /*
    * Clear any sd_auth_key which can be a key when we are acting as
    * the endpoint for a backup session which we don't seem to be.
    */
   if (jcr->sd_auth_key) {
      bfree(jcr->sd_auth_key);
   }

   jcr->sd_auth_key = bstrdup(key);
   Dmsg0(5, "set sd auth key\n");
}

/**
 * Listen for incoming replication session from other SD.
 */
static bool listen_cmd(JobControlRecord *jcr)
{
   Dsm_check(200);

   return do_listen_run(jcr);
}

/**
 * Get address of storage daemon from Director
 */
static bool replicate_cmd(JobControlRecord *jcr)
{
   int stored_port;                /* storage daemon port */
   int enable_ssl;                 /* enable ssl to sd */
   char JobName[MAX_NAME_LENGTH];
   char stored_addr[MAX_NAME_LENGTH];
   PoolMem sd_auth_key(PM_MESSAGE);
   BareosSocket *dir = jcr->dir_bsock;
   BareosSocket *sd;                      /* storage daemon bsock */

   sd = New(BareosSocketTCP);
   if (me->nokeepalive) {
      sd->clear_keepalive();
   }
   Dmsg1(100, "ReplicateCmd: %s", dir->msg);
   sd_auth_key.check_size(dir->msglen);

   if (sscanf(dir->msg, replicatecmd, JobName, stored_addr, &stored_port,
              &enable_ssl, sd_auth_key.c_str()) != 5) {
      dir->fsend(BADcmd, "replicate", dir->msg);
      goto bail_out;
   }

   set_storage_auth_key(jcr, sd_auth_key.c_str());

   Dmsg3(110, "Open storage: %s:%d ssl=%d\n", stored_addr, stored_port, enable_ssl);

   sd->set_source_address(me->SDsrc_addr);

   if (!jcr->max_bandwidth) {
      if (jcr->director->max_bandwidth_per_job) {
         jcr->max_bandwidth = jcr->director->max_bandwidth_per_job;
      } else if (me->max_bandwidth_per_job) {
         jcr->max_bandwidth = me->max_bandwidth_per_job;
      }
   }

   sd->set_bwlimit(jcr->max_bandwidth);
   if (me->allow_bw_bursting) {
      sd->set_bwlimit_bursting();
   }

   /*
    * Open command communications with Storage daemon
    */
   if (!sd->connect(jcr, 10, (int)me->SDConnectTimeout, me->heartbeat_interval,
                    _("Storage daemon"), stored_addr, NULL, stored_port, 1)) {
      delete sd;
      sd = NULL;
   }

   if (sd == NULL) {
      Jmsg(jcr, M_FATAL, 0, _("Failed to connect to Storage daemon: %s:%d\n"),
           stored_addr, stored_port);
      Dmsg2(100, "Failed to connect to Storage daemon: %s:%d\n",
            stored_addr, stored_port);
      goto bail_out;
   }
   Dmsg0(110, "Connection OK to SD.\n");

   jcr->store_bsock = sd;

   sd->fsend("Hello Start Storage Job %s\n", JobName);
   if (!authenticate_with_storagedaemon(jcr)) {
      Jmsg(jcr, M_FATAL, 0, _("Failed to authenticate Storage daemon.\n"));
      delete sd;
      goto bail_out;
   }
   Dmsg0(110, "Authenticated with SD.\n");

   /*
    * Keep track that we are replicating to a remote SD.
    */
   jcr->remote_replicate = true;

   /*
    * Send OK to Director
    */
   return dir->fsend(OK_replicate);

bail_out:
   return false;
}

static bool run_cmd(JobControlRecord *jcr)
{
   Dsm_check(200);
   Dmsg1(200, "Run_cmd: %s\n", jcr->dir_bsock->msg);

   /*
    * If we do not need the FD, we are doing a migrate, copy, or virtual backup.
    */
   if (jcr->no_client_used()) {
      return do_mac_run(jcr);
   } else {
      return do_job_run(jcr);
   }
}

static bool passive_cmd(JobControlRecord *jcr)
{
   int filed_port;                 /* file daemon port */
   int enable_ssl;                 /* enable ssl to fd */
   char filed_addr[MAX_NAME_LENGTH];
   BareosSocket *dir = jcr->dir_bsock;
   BareosSocket *fd;                      /* file daemon bsock */

   Dmsg1(100, "PassiveClientCmd: %s", dir->msg);
   if (sscanf(dir->msg, passiveclientcmd, filed_addr, &filed_port, &enable_ssl) != 3) {
      pm_strcpy(jcr->errmsg, dir->msg);
      Jmsg(jcr, M_FATAL, 0, _("Bad passiveclientcmd command: %s"), jcr->errmsg);
      goto bail_out;
   }

   Dmsg3(110, "PassiveClientCmd: %s:%d ssl=%d\n", filed_addr, filed_port, enable_ssl);

   jcr->passive_client = true;

   fd = New(BareosSocketTCP);
   if (me->nokeepalive) {
      fd->clear_keepalive();
   }
   fd->set_source_address(me->SDsrc_addr);

   /*
    * Open command communications with passive filedaemon
    */
   if (!fd->connect(jcr, 10, (int)me->FDConnectTimeout, me->heartbeat_interval,
                    _("File Daemon"), filed_addr, NULL, filed_port, 1)) {
      delete fd;
      fd = NULL;
   }

   if (fd == NULL) {
      Jmsg(jcr, M_FATAL, 0, _("Failed to connect to File daemon: %s:%d\n"),
           filed_addr, filed_port);
      Dmsg2(100, "Failed to connect to File daemon: %s:%d\n",
            filed_addr, filed_port);
      goto bail_out;
   }
   Dmsg0(110, "Connection OK to FD.\n");

   jcr->file_bsock = fd;
   fd->fsend("Hello Storage calling Start Job %s\n", jcr->Job);
   if (!authenticate_with_filedaemon(jcr)) {
      Jmsg(jcr, M_FATAL, 0, _("Failed to authenticate File daemon.\n"));
      delete fd;
      jcr->file_bsock = NULL;
      goto bail_out;
   } else {
      utime_t now;

      Dmsg0(110, "Authenticated with FD.\n");

      /*
       * Update the initial Job Statistics.
       */
      now = (utime_t)time(NULL);
      update_job_statistics(jcr, now);
   }

   /*
    * Send OK to Director
    */
   return dir->fsend(OKpassive);

bail_out:
   dir->fsend(BADcmd, "passive client");
   return false;
}

static bool pluginoptions_cmd(JobControlRecord *jcr)
{
   BareosSocket *dir = jcr->dir_bsock;
   char plugin_options[2048];

   Dmsg1(100, "PluginOptionsCmd: %s", dir->msg);
   if (sscanf(dir->msg, pluginoptionscmd, plugin_options) != 1) {
      pm_strcpy(jcr->errmsg, dir->msg);
      Jmsg(jcr, M_FATAL, 0, _("Bad pluginoptionscmd command: %s"), jcr->errmsg);
      goto bail_out;
   }

   unbash_spaces(plugin_options);
   if (!jcr->plugin_options) {
      jcr->plugin_options = New(alist(10, owned_by_alist));
   }
   jcr->plugin_options->append(bstrdup(plugin_options));

   /*
    * Send OK to Director
    */
   return dir->fsend(OKpluginoptions);

bail_out:
   dir->fsend(BADcmd, "plugin options");
   return false;
}
