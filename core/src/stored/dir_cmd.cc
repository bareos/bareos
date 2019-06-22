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

#include "include/bareos.h"
#include "stored/stored.h"
#include "stored/acquire.h"
#include "stored/authenticate.h"
#include "stored/autochanger.h"
#include "stored/bsr.h"
#include "stored/fd_cmds.h"
#include "stored/job.h"
#include "stored/label.h"
#include "stored/ndmp_tape.h"
#include "stored/reserve.h"
#include "stored/sd_cmds.h"
#include "stored/status.h"
#include "stored/sd_stats.h"
#include "stored/stored_globals.h"
#include "stored/wait.h"
#include "stored/job.h"
#include "stored/mac.h"
#include "include/make_unique.h"
#include "lib/berrno.h"
#include "lib/bnet.h"
#include "lib/bsock.h"
#include "lib/bsock_tcp.h"
#include "lib/edit.h"
#include "lib/parse_bsr.h"
#include "lib/parse_conf.h"
#include "lib/util.h"
#include "lib/watchdog.h"
#include "lib/qualified_resource_name_type_converter.h"
#include "include/jcr.h"

/* Imported variables */
extern struct s_last_job last_job;

extern void terminate_child();

namespace storagedaemon {

/* Commands received from director that need scanning */
static char setbandwidth[] = "setbandwidth=%lld Job=%127s";
static char setdebugv0cmd[] = "setdebug=%d trace=%d";
static char setdebugv1cmd[] = "setdebug=%d trace=%d timestamp=%d";
static char cancelcmd[] = "cancel Job=%127s";
static char relabelcmd[] =
    "relabel %127s OldName=%127s NewName=%127s PoolName=%127s "
    "MediaType=%127s Slot=%hd drive=%hd MinBlocksize=%d MaxBlocksize=%d";
static char labelcmd[] =
    "label %127s VolumeName=%127s PoolName=%127s "
    "MediaType=%127s Slot=%hd drive=%hd MinBlocksize=%d MaxBlocksize=%d";
static char mountslotcmd[] = "mount %127s drive=%hd slot=%hd";
static char mountcmd[] = "mount %127s drive=%hd";
static char unmountcmd[] = "unmount %127s drive=%hd";
static char releasecmd[] = "release %127s drive=%hd";
static char readlabelcmd[] = "readlabel %127s Slot=%hd drive=%hd";
static char replicatecmd[] =
    "replicate JobId=%d Job=%127s address=%s port=%d ssl=%d "
    "Authorization=%100s";
static char passiveclientcmd[] = "passive client address=%s port=%d ssl=%d";
static char resolvecmd[] = "resolve %s";
static char pluginoptionscmd[] = "pluginoptions %s";

/* Responses sent to Director */
static char derrmsg[] = "3900 Invalid command:";
static char OKsetdebugv0[] = "3000 OK setdebug=%d tracefile=%s\n";
static char OKsetdebugv1[] =
    "3000 OK setdebug=%d trace=%d timestamp=%d tracefile=%s\n";
static char invalid_cmd[] =
    "3997 Invalid command for a Director with Monitor directive enabled.\n";
static char OK_bootstrap[] = "3000 OK bootstrap\n";
static char ERROR_bootstrap[] = "3904 Error bootstrap\n";
static char OK_replicate[] = "3000 OK replicate\n";
static char BADcmd[] = "3991 Bad %s command: %s\n";
static char OKBandwidth[] = "2000 OK Bandwidth\n";
static char OKpassive[] = "2000 OK passive client\n";
static char OKpluginoptions[] = "2000 OK plugin options\n";
static char OKsecureerase[] = "2000 OK SDSecureEraseCmd %s \n";

/* Forward referenced functions */
// static bool ActionOnPurgeCmd(JobControlRecord *jcr);
static bool BootstrapCmd(JobControlRecord* jcr);
static bool CancelCmd(JobControlRecord* cjcr);
static bool ChangerCmd(JobControlRecord* jcr);
static bool die_cmd(JobControlRecord* jcr);
static bool LabelCmd(JobControlRecord* jcr);
static bool ListenCmd(JobControlRecord* jcr);
static bool MountCmd(JobControlRecord* jcr);
static bool PassiveCmd(JobControlRecord* jcr);
static bool PluginoptionsCmd(JobControlRecord* jcr);
static bool ReadlabelCmd(JobControlRecord* jcr);
static bool ResolveCmd(JobControlRecord* jcr);
static bool RelabelCmd(JobControlRecord* jcr);
static bool ReleaseCmd(JobControlRecord* jcr);
static bool ReplicateCmd(JobControlRecord* jcr);
static bool RunCmd(JobControlRecord* jcr);
static bool SecureerasereqCmd(JobControlRecord* jcr);
static bool SetbandwidthCmd(JobControlRecord* jcr);
static bool SetdebugCmd(JobControlRecord* jcr);
static bool UnmountCmd(JobControlRecord* jcr);

static DeviceControlRecord* FindDevice(JobControlRecord* jcr,
                                       PoolMem& dev_name,
                                       drive_number_t drive,
                                       BlockSizes* blocksizes);
static void ReadVolumeLabel(JobControlRecord* jcr,
                            DeviceControlRecord* dcr,
                            Device* dev,
                            slot_number_t slot);
static void LabelVolumeIfOk(DeviceControlRecord* dcr,
                            char* oldname,
                            char* newname,
                            char* poolname,
                            slot_number_t Slot,
                            bool relabel);
static int TryAutoloadDevice(JobControlRecord* jcr,
                             DeviceControlRecord* dcr,
                             slot_number_t slot,
                             const char* VolName);
static void SendDirBusyMessage(BareosSocket* dir, Device* dev);

struct s_cmds {
  const char* cmd;
  bool (*func)(JobControlRecord* jcr);
  bool monitoraccess; /* set if monitors can access this cmd */
};

/**
 * The following are the recognized commands from the Director.
 *
 * Keywords are sorted first longest match when the keywords start with the same
 * string.
 */
static struct s_cmds cmds[] = {
    // { "action_on_purge",  ActionOnPurgeCmd, false },
    {"autochanger", ChangerCmd, false},
    {"bootstrap", BootstrapCmd, false},
    {"cancel", CancelCmd, false},
    {".die", die_cmd, false},
    {"finish", FinishCmd, false}, /**< End of backup */
    {"JobId=", job_cmd, false},   /**< Start Job */
    {"label", LabelCmd, false},   /**< Label a tape */
    {"listen", ListenCmd, false}, /**< Listen for an incoming Storage Job */
    {"mount", MountCmd, false},
    {"nextrun", nextRunCmd,
     false}, /**< Prepare for next backup/restore part of same Job */
    {"passive", PassiveCmd, false},
    {"pluginoptions", PluginoptionsCmd, false},
    {"readlabel", ReadlabelCmd, false},
    {"relabel", RelabelCmd, false}, /**< Relabel a tape */
    {"release", ReleaseCmd, false},
    {"resolve", ResolveCmd, false},
    {"replicate", ReplicateCmd, false}, /**< Replicate data to an external SD */
    {"run", RunCmd, false},             /**< Start of Job */
    {"getSecureEraseCmd", SecureerasereqCmd, false},
    {"setbandwidth=", SetbandwidthCmd, false},
    {"setdebug=", SetdebugCmd, false}, /**< Set debug level */
    {"stats", StatsCmd, false},
    {"status", StatusCmd, true},
    {".status", DotstatusCmd, true},
    {"unmount", UnmountCmd, false},
    {"use storage=", use_cmd, false},
    {NULL, NULL, false} /**< list terminator */
};

static inline bool AreMaxConcurrentJobsExceeded()
{
  JobControlRecord* jcr;
  unsigned int cnt = 0;

  foreach_jcr (jcr) {
    cnt++;
  }
  endeach_jcr(jcr);

  return (cnt >= me->MaxConcurrentJobs) ? true : false;
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
void* HandleDirectorConnection(BareosSocket* dir)
{
  JobControlRecord* jcr;
  int i, errstat;
  int bnet_stat = 0;
  bool found, quit;

  if (AreMaxConcurrentJobsExceeded()) {
    Emsg0(
        M_ERROR, 0,
        _("Number of Jobs exhausted, please increase MaximumConcurrentJobs\n"));
    dir->signal(BNET_TERMINATE);
    return NULL;
  }

  /*
   * This is a connection from the Director, so setup a JobControlRecord
   */
  jcr = new_jcr(sizeof(JobControlRecord),
                StoredFreeJcr); /* create Job Control Record */
  NewPlugins(jcr);              /* instantiate plugins */
  jcr->dir_bsock = dir;         /* save Director bsock */
  jcr->dir_bsock->SetJcr(jcr);
  jcr->dcrs = new alist(10, not_owned_by_alist);

  /*
   * Initialize Start Job condition variable
   */
  errstat = pthread_cond_init(&jcr->job_start_wait, NULL);
  if (errstat != 0) {
    BErrNo be;
    Jmsg1(jcr, M_FATAL, 0,
          _("Unable to init job start cond variable: ERR=%s\n"),
          be.bstrerror(errstat));
    goto bail_out;
  }

  /*
   * Initialize End Job condition variable
   */
  errstat = pthread_cond_init(&jcr->job_end_wait, NULL);
  if (errstat != 0) {
    BErrNo be;
    Jmsg1(jcr, M_FATAL, 0, _("Unable to init job end cond variable: ERR=%s\n"),
          be.bstrerror(errstat));
    goto bail_out;
  }

  Dmsg0(1000, "stored in start_job\n");

  SetJcrInTsd(jcr);

  /*
   * Authenticate the Director
   */
  if (!AuthenticateDirector(jcr)) {
    Jmsg(jcr, M_FATAL, 0, _("Unable to authenticate Director\n"));
    goto bail_out;
  }

  Dmsg0(90, "Message channel init completed.\n");

  quit = false;
  while (!quit) {
    /*
     * Read command
     */
    if ((bnet_stat = dir->recv()) <= 0) { break; /* connection terminated */ }

    Dmsg1(199, "<dird: %s", dir->msg);

    /*
     * Ensure that device initialization is complete
     */
    while (!init_done) { Bmicrosleep(1, 0); }

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
          quit = true;            /* error, get out */
          Dmsg1(190, "Command %s requests quit\n", cmds[i].cmd);
        }
        found = true; /* indicate command found */
        break;
      }
    }
    if (!found) { /* command not found */
      PoolMem err_msg;
      Mmsg(err_msg, "%s %s\n", derrmsg, dir->msg);
      dir->fsend(err_msg.c_str());
      break;
    }
  }

bail_out:
  GeneratePluginEvent(jcr, bsdEventJobEnd);
  DequeueMessages(jcr); /* send any queued messages */
  dir->signal(BNET_TERMINATE);
  FreePlugins(jcr); /* release instantiated plugins */
  FreeJcr(jcr);

  return NULL;
}

/**
 * Force SD to die, and hopefully dump itself.  Turned on only in development
 * version.
 */
static bool die_cmd(JobControlRecord* jcr)
{
#ifdef DEVELOPER
  JobControlRecord* djcr = NULL;
  int a;
  BareosSocket* dir = jcr->dir_bsock;
  pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;

  if (strstr(dir->msg, "deadlock")) {
    Pmsg0(000, "I have been requested to deadlock ...\n");
    P(m);
    P(m);
  }

  Pmsg1(000, "I have been requested to die ... (%s)\n", dir->msg);
  a = djcr->JobId; /* ref NULL pointer */
  djcr->JobId = a;
#endif
  return false;
}


/**
 * Handles the secureerase request
 * replies the configured secure erase command
 * or "*None*"
 */
static bool SecureerasereqCmd(JobControlRecord* jcr)
{
  BareosSocket* dir = jcr->dir_bsock;

  Dmsg1(220, "Secure Erase Cmd Request: %s\n",
        (me->secure_erase_cmdline ? me->secure_erase_cmdline : "*None*"));

  return dir->fsend(
      OKsecureerase,
      (me->secure_erase_cmdline ? me->secure_erase_cmdline : "*None*"));
}

/**
 * Set bandwidth limit as requested by the Director
 */
static bool SetbandwidthCmd(JobControlRecord* jcr)
{
  BareosSocket* dir = jcr->dir_bsock;
  int64_t bw = 0;
  JobControlRecord* cjcr;
  char Job[MAX_NAME_LENGTH];

  *Job = 0;
  if (sscanf(dir->msg, setbandwidth, &bw, Job) != 2 || bw < 0) {
    PmStrcpy(jcr->errmsg, dir->msg);
    dir->fsend(_("2991 Bad setbandwidth command: %s\n"), jcr->errmsg);
    return false;
  }

  if (*Job) {
    if (!(cjcr = get_jcr_by_full_name(Job))) {
      dir->fsend(_("2901 Job %s not found.\n"), Job);
    } else {
      cjcr->max_bandwidth = bw;
      if (cjcr->store_bsock) {
        cjcr->store_bsock->SetBwlimit(bw);
        if (me->allow_bw_bursting) { cjcr->store_bsock->SetBwlimitBursting(); }
      }
      FreeJcr(cjcr);
    }
  } else {                          /* No job requested, apply globally */
    me->max_bandwidth_per_job = bw; /* Overwrite directive */
  }

  return dir->fsend(OKBandwidth);
}

/**
 * Set debug level as requested by the Director
 */
static bool SetdebugCmd(JobControlRecord* jcr)
{
  BareosSocket* dir = jcr->dir_bsock;
  int32_t level, trace_flag, timestamp_flag;
  int scan;

  Dmsg1(10, "SetdebugCmd: %s", dir->msg);
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
  SetTrace(trace_flag);
  if (scan == 3) {
    SetTimestamp(timestamp_flag);
    Dmsg4(50, "level=%d trace=%d timestamp=%d tracefilename=%s\n", level,
          GetTrace(), GetTimestamp(), tracefilename.c_str());
    return dir->fsend(OKsetdebugv1, level, GetTrace(), GetTimestamp(),
                      tracefilename.c_str());
  } else {
    Dmsg3(50, "level=%d trace=%d\n", level, GetTrace(), tracefilename.c_str());
    return dir->fsend(OKsetdebugv0, level, tracefilename.c_str());
  }
}

/**
 * Cancel a Job
 *   Be careful, we switch to using the job's JobControlRecord! So, using
 *   BSOCKs on that jcr can have two threads in the same code.
 */
static bool CancelCmd(JobControlRecord* cjcr)
{
  BareosSocket* dir = cjcr->dir_bsock;
  int oldStatus;
  char Job[MAX_NAME_LENGTH];
  JobId_t JobId;
  JobControlRecord* jcr;
  int status;
  const char* reason;

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
  if (Is_a_number(Job)) {
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
  if (!jcr->authenticated &&
      (oldStatus == JS_WaitFD || oldStatus == JS_WaitSD)) {
    pthread_cond_signal(&jcr->job_start_wait); /* wake waiting thread */
  }

  if (jcr->file_bsock) {
    jcr->file_bsock->SetTerminated();
    jcr->file_bsock->SetTimedOut();
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
    Dmsg1(100, "JobId=%u broadcast wait_device_release\n",
          (uint32_t)jcr->JobId);
    ReleaseDeviceCond();
  }

  if (jcr->read_dcr && jcr->read_dcr->dev &&
      jcr->read_dcr->dev->waiting_for_mount()) {
    pthread_cond_broadcast(&jcr->read_dcr->dev->wait_next_vol);
    Dmsg1(100, "JobId=%u broadcast wait_device_release\n",
          (uint32_t)jcr->JobId);
    ReleaseDeviceCond();
  }

  /*
   * See if the Job has a certain protocol.
   * When canceling a NDMP job make sure we call the end_of_ndmp_* functions.
   */
  switch (jcr->getJobProtocol()) {
    case PT_NDMP_BAREOS:
      switch (jcr->getJobType()) {
        case JT_BACKUP:
          EndOfNdmpBackup(jcr);
          break;
        case JT_RESTORE:
          EndOfNdmpRestore(jcr);
          break;
        default:
          break;
      }
  }

  pthread_cond_signal(&jcr->job_end_wait); /* wake waiting job */
  jcr->MyThreadSendSignal(TIMEOUT_SIGNAL);

  dir->fsend(_("3000 JobId=%ld Job=\"%s\" marked to be %s.\n"), jcr->JobId,
             jcr->Job, reason);
  FreeJcr(jcr);

bail_out:
  dir->signal(BNET_EOD);
  return true;
}

/**
 * Resolve a hostname
 */
static bool ResolveCmd(JobControlRecord* jcr)
{
  BareosSocket* dir = jcr->dir_bsock;
  dlist* addr_list;
  const char* errstr;
  char addresses[2048];
  char hostname[2048];

  sscanf(dir->msg, resolvecmd, &hostname);

  if ((addr_list = BnetHost2IpAddrs(hostname, 0, &errstr)) == NULL) {
    dir->fsend(_("%s: Failed to resolve %s\n"), my_name, hostname);
    goto bail_out;
  }

  dir->fsend(
      _("%s resolves %s to %s\n"), my_name, hostname,
      BuildAddressesString(addr_list, addresses, sizeof(addresses), false));
  FreeAddresses(addr_list);

bail_out:
  dir->signal(BNET_EOD);
  return true;
}

static drive_number_t IntToDriveNumber(int drive_input)
{
  return (drive_input == -1) ? kInvalidDriveNumber
                             : static_cast<drive_number_t>(drive_input);
}

static bool DoLabel(JobControlRecord* jcr, bool relabel)
{
  int len;
  POOLMEM *newname, *oldname, *poolname, *mediatype;
  PoolMem dev_name;
  BareosSocket* dir = jcr->dir_bsock;
  DeviceControlRecord* dcr;
  Device* dev;
  BlockSizes blocksizes;
  bool ok = false;
  slot_number_t slot;

  /*
   * Determine the length of the temporary buffers.
   * If the total length of the incoming message is less
   * then MAX_NAME_LENGTH we can use that as the upper limit.
   * If the incomming message is bigger then MAX_NAME_LENGTH
   * limit the temporary buffer to MAX_NAME_LENGTH bytes as
   * we use a sscanf %127s for reading the temorary buffer.
   */
  len = dir->message_length + 1;
  if (len > MAX_NAME_LENGTH) { len = MAX_NAME_LENGTH; }

  newname = GetMemory(len);
  oldname = GetMemory(len);
  poolname = GetMemory(len);
  mediatype = GetMemory(len);

  int drive_input = -1;

  if (relabel) {
    if (sscanf(dir->msg, relabelcmd, dev_name.c_str(), oldname, newname,
               poolname, mediatype, &slot, &drive_input,
               &blocksizes.min_block_size, &blocksizes.max_block_size) == 9) {
      ok = true;
    }
  } else {
    *oldname = 0;
    if (sscanf(dir->msg, labelcmd, dev_name.c_str(), newname, poolname,
               mediatype, &slot, &drive_input, &blocksizes.min_block_size,
               &blocksizes.max_block_size) == 8) {
      ok = true;
    }
  }

  if (ok) {
    UnbashSpaces(newname);
    UnbashSpaces(oldname);
    UnbashSpaces(poolname);
    UnbashSpaces(mediatype);

    drive_number_t drive = IntToDriveNumber(drive_input);

    dcr = FindDevice(jcr, dev_name, drive, &blocksizes);
    if (dcr) {
      dev = dcr->dev;


      dev->Lock(); /* Use P to avoid indefinite block */
      dcr->VolMinBlocksize = blocksizes.min_block_size;
      dcr->VolMaxBlocksize = blocksizes.max_block_size;
      dev->SetBlocksizes(dcr); /* apply blocksizes from dcr to dev  */

      if (!dev->IsOpen() && !dev->IsBusy()) {
        Dmsg1(400, "Can %slabel. Device is not open\n", relabel ? "re" : "");
        LabelVolumeIfOk(dcr, oldname, newname, poolname, slot, relabel);
        dev->close(dcr);
        /* Under certain "safe" conditions, we can steal the lock */
      } else if (dev->CanStealLock()) {
        Dmsg0(400, "Can relabel. CanStealLock\n");
        LabelVolumeIfOk(dcr, oldname, newname, poolname, slot, relabel);
      } else if (dev->IsBusy() || dev->IsBlocked()) {
        SendDirBusyMessage(dir, dev);
      } else { /* device not being used */
        Dmsg0(400, "Can relabel. device not used\n");
        LabelVolumeIfOk(dcr, oldname, newname, poolname, slot, relabel);
      }
      dev->Unlock();
      FreeDeviceControlRecord(dcr);
    } else {
      dir->fsend(_("3999 Device \"%s\" not found or could not be opened.\n"),
                 dev_name.c_str());
    }
  } else {
    /* NB dir->msg gets clobbered in BnetFsend, so save command */
    PmStrcpy(jcr->errmsg, dir->msg);
    dir->fsend(_("3903 Error scanning label command: %s\n"), jcr->errmsg);
  }
  FreeMemory(oldname);
  FreeMemory(newname);
  FreeMemory(poolname);
  FreeMemory(mediatype);
  dir->signal(BNET_EOD);
  return true;
}

/**
 * Label a Volume
 */
static bool LabelCmd(JobControlRecord* jcr) { return DoLabel(jcr, false); }

static bool RelabelCmd(JobControlRecord* jcr) { return DoLabel(jcr, true); }

/**
 * Read the tape label and determine if we can safely
 * label the tape (not a Bareos volume), then label it.
 *
 *  Enter with the mutex set
 */
static void LabelVolumeIfOk(DeviceControlRecord* dcr,
                            char* oldname,
                            char* newname,
                            char* poolname,
                            slot_number_t slot,
                            bool relabel)
{
  BareosSocket* dir = dcr->jcr->dir_bsock;
  bsteal_lock_t hold;
  Device* dev = dcr->dev;
  int label_status;
  int mode;
  const char* volname = relabel ? oldname : newname;
  char ed1[50];

  StealDeviceLock(dev, &hold, BST_WRITING_LABEL);
  Dmsg1(100, "Stole device %s lock, writing label.\n", dev->print_name());

  Dmsg0(90, "TryAutoloadDevice - looking for volume_info\n");
  switch (TryAutoloadDevice(dcr->jcr, dcr, slot, volname)) {
    case -1:
      goto cleanup;
    case -2:
      goto bail_out;
    case 0:
    default:
      break;
  }

  /*
   * Ensure that the device is open -- AutoloadDevice() closes it
   */
  if (dev->IsTape()) {
    mode = OPEN_READ_WRITE;
  } else {
    mode = CREATE_READ_WRITE;
  }

  /*
   * Set old volume name for open if relabeling
   */
  dcr->setVolCatName(volname);

  if (!dev->open(dcr, mode)) {
    dir->fsend(_("3910 Unable to open device \"%s\": ERR=%s\n"),
               dev->print_name(), dev->bstrerror());
    goto cleanup;
  }

  /*
   * See what we have for a Volume
   */
  label_status = ReadDevVolumeLabel(dcr);

  /*
   * Set new volume name
   */
  dcr->setVolCatName(newname);
  switch (label_status) {
    case VOL_NAME_ERROR:
    case VOL_VERSION_ERROR:
    case VOL_LABEL_ERROR:
    case VOL_OK:
      if (!relabel) {
        dir->fsend(_("3920 Cannot label Volume because it is already labeled: "
                     "\"%s\"\n"),
                   dev->VolHdr.VolumeName);
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
      if (!WriteNewVolumeLabelToDev(dcr, newname, poolname, relabel)) {
        dir->fsend(_("3912 Failed to label Volume: ERR=%s\n"),
                   dev->bstrerror());
        goto cleanup;
      }
      bstrncpy(dcr->VolumeName, newname, sizeof(dcr->VolumeName));

      /*
       * The following 3000 OK label. string is scanned in ua_label.c
       */
      dir->fsend("3000 OK label. VolBytes=%s Volume=\"%s\" Device=%s\n",
                 edit_uint64(dev->VolCatInfo.VolCatBytes, ed1), newname,
                 dev->print_name());
      break;
    case VOL_NO_MEDIA:
      dir->fsend(_("3914 Failed to label Volume (no media): ERR=%s\n"),
                 dev->bstrerror());
      break;
    default:
      dir->fsend(_("3913 Cannot label Volume. "
                   "Unknown status %d from ReadVolumeLabel()\n"),
                 label_status);
      break;
  }

cleanup:
  if (dev->IsOpen() && !dev->HasCap(CAP_ALWAYSOPEN)) { dev->close(dcr); }
  if (!dev->IsOpen()) { dev->ClearVolhdr(); }
  VolumeUnused(dcr); /* no longer using volume */

bail_out:
  GiveBackDeviceLock(dev, &hold);

  return;
}

/**
 * Read the tape label
 *
 *  Enter with the mutex set
 */
static bool ReadLabel(DeviceControlRecord* dcr)
{
  int ok;
  JobControlRecord* jcr = dcr->jcr;
  BareosSocket* dir = jcr->dir_bsock;
  bsteal_lock_t hold;
  Device* dev = dcr->dev;

  StealDeviceLock(dev, &hold, BST_DOING_ACQUIRE);

  dcr->VolumeName[0] = 0;
  dev->ClearLabeled(); /* force read of label */
  switch (ReadDevVolumeLabel(dcr)) {
    case VOL_OK:
      dir->fsend(_("3001 Mounted Volume: %s\n"), dev->VolHdr.VolumeName);
      ok = true;
      break;
    default:
      dir->fsend(
          _("3902 Cannot mount Volume on Storage Device \"%s\" because:\n%s"),
          dev->print_name(), jcr->errmsg);
      ok = false;
      break;
  }
  VolumeUnused(dcr);
  GiveBackDeviceLock(dev, &hold);
  return ok;
}

/**
 * Searches for device by name, and if found, creates a dcr and returns it.
 */
static DeviceControlRecord* FindDevice(JobControlRecord* jcr,
                                       PoolMem& devname,
                                       drive_number_t drive,
                                       BlockSizes* blocksizes)
{
  DeviceResource* device;
  AutochangerResource* changer;
  bool found = false;
  DeviceControlRecord* dcr = NULL;

  UnbashSpaces(devname);
  foreach_res (device, R_DEVICE) {
    /*
     * Find resource, and make sure we were able to open it
     */
    if (bstrcmp(device->resource_name_, devname.c_str())) {
      if (!device->dev) { device->dev = InitDev(jcr, device); }
      if (!device->dev) {
        Jmsg(jcr, M_WARNING, 0,
             _("\n"
               "     Device \"%s\" requested by DIR could not be opened or "
               "does not exist.\n"),
             devname.c_str());
        continue;
      }
      Dmsg1(20, "Found device %s\n", device->resource_name_);
      found = true;
      break;
    }
  }

  if (!found) {
    foreach_res (changer, R_AUTOCHANGER) {
      /*
       * Find resource, and make sure we were able to open it
       */
      if (bstrcmp(devname.c_str(), changer->resource_name_)) {
        /*
         * Try each device in this AutoChanger
         */
        foreach_alist (device, changer->device) {
          Dmsg1(100, "Try changer device %s\n", device->resource_name_);
          if (!device->dev) { device->dev = InitDev(jcr, device); }
          if (!device->dev) {
            Dmsg1(100, "Device %s could not be opened. Skipped\n",
                  devname.c_str());
            Jmsg(jcr, M_WARNING, 0,
                 _("\n"
                   "     Device \"%s\" in changer \"%s\" requested by DIR "
                   "could not be opened or does not exist.\n"),
                 device->resource_name_, devname.c_str());
            continue;
          }
          if (!device->dev->autoselect) {
            Dmsg1(100, "Device %s not autoselect skipped.\n", devname.c_str());
            continue; /* device is not available */
          }
          if (drive == kInvalidDriveNumber || drive == device->dev->drive) {
            Dmsg1(20, "Found changer device %s\n", device->resource_name_);
            found = true;
            break;
          }
          Dmsg3(100, "Device %s drive wrong: want=%hd got=%hd skipping\n",
                devname.c_str(), drive, device->dev->drive);
        }
        break; /* we found it but could not open a device */
      }
    }
  }

  if (found) {
    Dmsg1(100, "Found device %s\n", device->resource_name_);
    dcr = new StorageDaemonDeviceControlRecord;
    SetupNewDcrDevice(jcr, dcr, device->dev, blocksizes);
    dcr->SetWillWrite();
    dcr->device = device;
  }
  return dcr;
}

/**
 * Mount command from Director
 */
static bool MountCmd(JobControlRecord* jcr)
{
  PoolMem devname;
  BareosSocket* dir = jcr->dir_bsock;
  Device* dev;
  DeviceControlRecord* dcr;
  slot_number_t slot = 0;

  int drive_input = -1;
  bool ok =
      sscanf(dir->msg, mountslotcmd, devname.c_str(), &drive_input, &slot) == 3;

  if (!ok) {
    ok = sscanf(dir->msg, mountcmd, devname.c_str(), &drive_input) == 2;
  }

  Dmsg3(100, "ok=%d drive=%hd slot=%hd\n", ok, drive_input, slot);
  if (ok) {
    drive_number_t drive = IntToDriveNumber(drive_input);
    dcr = FindDevice(jcr, devname, drive, NULL);
    if (dcr) {
      dev = dcr->dev;
      dev->Lock(); /* Use P to avoid indefinite block */
      Dmsg2(100, "mount cmd blocked=%d MustUnload=%d\n", dev->blocked(),
            dev->MustUnload());
      switch (dev->blocked()) { /* device blocked? */
        case BST_WAITING_FOR_SYSOP:
          /* Someone is waiting, wake him */
          Dmsg0(100, "Waiting for mount. Attempting to wake thread\n");
          dev->SetBlocked(BST_MOUNT);
          dir->fsend("3001 OK mount requested. %sDevice=%s\n",
                     (slot > 0) ? _("Specified slot ignored. ") : "",
                     dev->print_name());
          pthread_cond_broadcast(&dev->wait_next_vol);
          Dmsg1(100, "JobId=%u broadcast wait_device_release\n",
                (uint32_t)dcr->jcr->JobId);
          ReleaseDeviceCond();
          break;

        /* In both of these two cases, we (the user) unmounted the Volume */
        case BST_UNMOUNTED_WAITING_FOR_SYSOP:
        case BST_UNMOUNTED:
          Dmsg2(100, "Unmounted changer=%d slot=%hd\n", dev->IsAutochanger(),
                slot);
          if (dev->IsAutochanger() && slot > 0) {
            TryAutoloadDevice(jcr, dcr, slot, "");
          }
          /* We freed the device, so reopen it and wake any waiting threads */
          if (!dev->open(dcr, OPEN_READ_ONLY)) {
            dir->fsend(_("3901 Unable to open device %s: ERR=%s\n"),
                       dev->print_name(), dev->bstrerror());
            if (dev->blocked() == BST_UNMOUNTED) {
              /* We blocked the device, so unblock it */
              Dmsg0(100, "Unmounted. Unblocking device\n");
              UnblockDevice(dev);
            }
            break;
          }
          ReadDevVolumeLabel(dcr);
          if (dev->blocked() == BST_UNMOUNTED) {
            /* We blocked the device, so unblock it */
            Dmsg0(100, "Unmounted. Unblocking device\n");
            ReadLabel(dcr); /* this should not be necessary */
            UnblockDevice(dev);
          } else {
            Dmsg0(100,
                  "Unmounted waiting for mount. Attempting to wake thread\n");
            dev->SetBlocked(BST_MOUNT);
          }
          if (dev->IsLabeled()) {
            dir->fsend(_("3001 Device %s is mounted with Volume \"%s\"\n"),
                       dev->print_name(), dev->VolHdr.VolumeName);
          } else {
            dir->fsend(
                _("3905 Device %s open but no Bareos volume is mounted.\n"
                  "If this is not a blank tape, try unmounting and remounting "
                  "the Volume.\n"),
                dev->print_name());
          }
          pthread_cond_broadcast(&dev->wait_next_vol);
          Dmsg1(100, "JobId=%u broadcast wait_device_release\n",
                (uint32_t)dcr->jcr->JobId);
          ReleaseDeviceCond();
          break;

        case BST_DOING_ACQUIRE:
          dir->fsend(_("3001 Device %s is doing acquire.\n"),
                     dev->print_name());
          break;

        case BST_WRITING_LABEL:
          dir->fsend(_("3903 Device %s is being labeled.\n"),
                     dev->print_name());
          break;

        case BST_NOT_BLOCKED:
          Dmsg2(100, "Not blocked changer=%d slot=%hd\n", dev->IsAutochanger(),
                slot);
          if (dev->IsAutochanger() && slot > 0) {
            TryAutoloadDevice(jcr, dcr, slot, "");
          }
          if (dev->IsOpen()) {
            if (dev->IsLabeled()) {
              dir->fsend(_("3001 Device %s is mounted with Volume \"%s\"\n"),
                         dev->print_name(), dev->VolHdr.VolumeName);
            } else {
              dir->fsend(
                  _("3905 Device %s open but no Bareos volume is mounted.\n"
                    "If this is not a blank tape, try unmounting and "
                    "remounting the Volume.\n"),
                  dev->print_name());
            }
          } else if (dev->IsTape()) {
            if (!dev->open(dcr, OPEN_READ_ONLY)) {
              dir->fsend(_("3901 Unable to open device %s: ERR=%s\n"),
                         dev->print_name(), dev->bstrerror());
              break;
            }
            ReadLabel(dcr);
            if (dev->IsLabeled()) {
              dir->fsend(
                  _("3001 Device %s is already mounted with Volume \"%s\"\n"),
                  dev->print_name(), dev->VolHdr.VolumeName);
            } else {
              dir->fsend(
                  _("3905 Device %s open but no Bareos volume is mounted.\n"
                    "If this is not a blank tape, try unmounting and "
                    "remounting the Volume.\n"),
                  dev->print_name());
            }
            if (dev->IsOpen() && !dev->HasCap(CAP_ALWAYSOPEN)) {
              dev->close(dcr);
            }
          } else if (dev->IsUnmountable()) {
            if (dev->mount(dcr, 1)) {
              dir->fsend(_("3002 Device %s is mounted.\n"), dev->print_name());
            } else {
              dir->fsend(_("3907 %s"), dev->bstrerror());
            }
          } else { /* must be file */
            dir->fsend(_("3906 File device %s is always mounted.\n"),
                       dev->print_name());
            pthread_cond_broadcast(&dev->wait_next_vol);
            Dmsg1(100, "JobId=%u broadcast wait_device_release\n",
                  (uint32_t)dcr->jcr->JobId);
            ReleaseDeviceCond();
          }
          break;

        case BST_RELEASING:
          dir->fsend(_("3930 Device %s is being released.\n"),
                     dev->print_name());
          break;

        default:
          dir->fsend(_("3905 Unknown wait state %d\n"), dev->blocked());
          break;
      }
      dev->Unlock();
      FreeDeviceControlRecord(dcr);
    } else {
      dir->fsend(_("3999 Device %s not found or could not be opened.\n"),
                 devname.c_str());
    }
  } else {
    PmStrcpy(jcr->errmsg, dir->msg);
    dir->fsend(_("3909 Error scanning mount command: %s\n"), jcr->errmsg);
  }
  dir->signal(BNET_EOD);
  return true;
}

/**
 * unmount command from Director
 */
static bool UnmountCmd(JobControlRecord* jcr)
{
  PoolMem devname;
  BareosSocket* dir = jcr->dir_bsock;
  Device* dev;
  DeviceControlRecord* dcr;

  int drive_input = -1;
  if (sscanf(dir->msg, unmountcmd, devname.c_str(), &drive_input) == 2) {
    drive_number_t drive = IntToDriveNumber(drive_input);
    dcr = FindDevice(jcr, devname, drive, NULL);
    if (dcr) {
      dev = dcr->dev;
      dev->Lock(); /* Use P to avoid indefinite block */
      if (!dev->IsOpen()) {
        if (!dev->IsBusy()) { UnloadAutochanger(dcr, kInvalidSlotNumber); }
        if (dev->IsUnmountable()) {
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
        if (!UnloadAutochanger(dcr, kInvalidSlotNumber)) {
          /* ***FIXME**** what is this ????  */
          dev->close(dcr);
          FreeVolume(dev);
        }
        if (dev->IsUnmountable() && !dev->unmount(dcr, 0)) {
          dir->fsend(_("3907 %s"), dev->bstrerror());
        } else {
          dev->SetBlocked(BST_UNMOUNTED_WAITING_FOR_SYSOP);
          dir->fsend(_("3001 Device \"%s\" unmounted.\n"), dev->print_name());
        }

      } else if (dev->blocked() == BST_DOING_ACQUIRE) {
        dir->fsend(_("3902 Device \"%s\" is busy in acquire.\n"),
                   dev->print_name());

      } else if (dev->blocked() == BST_WRITING_LABEL) {
        dir->fsend(_("3903 Device \"%s\" is being labeled.\n"),
                   dev->print_name());

      } else if (dev->IsBusy()) {
        SendDirBusyMessage(dir, dev);
      } else { /* device not being used */
        Dmsg0(90, "Device not in use, unmounting\n");
        /* On FreeBSD, I am having ASSERT() failures in BlockDevice()
         * and I can only imagine that the thread id that we are
         * leaving in no_wait_id is being re-used. So here,
         * we simply do it by hand.  Gross, but a solution.
         */
        /*  BlockDevice(dev, BST_UNMOUNTED); replace with 2 lines below */
        dev->SetBlocked(BST_UNMOUNTED);
        ClearThreadId(dev->no_wait_id);
        if (!UnloadAutochanger(dcr, kInvalidSlotNumber)) {
          dev->close(dcr);
          FreeVolume(dev);
        }
        if (dev->IsUnmountable() && !dev->unmount(dcr, 0)) {
          dir->fsend(_("3907 %s"), dev->bstrerror());
        } else {
          dir->fsend(_("3002 Device \"%s\" unmounted.\n"), dev->print_name());
        }
      }
      dev->Unlock();
      FreeDeviceControlRecord(dcr);
    } else {
      dir->fsend(_("3999 Device \"%s\" not found or could not be opened.\n"),
                 devname.c_str());
    }
  } else {
    /* NB dir->msg gets clobbered in BnetFsend, so save command */
    PmStrcpy(jcr->errmsg, dir->msg);
    dir->fsend(_("3907 Error scanning unmount command: %s\n"), jcr->errmsg);
  }
  dir->signal(BNET_EOD);
  return true;
}

/**
 * Release command from Director. This rewinds the device and if
 *   configured does a offline and ensures that Bareos will
 *   re-read the label of the tape before continuing. This gives
 *   the operator the chance to change the tape anytime before the
 *   next job starts.
 */
static bool ReleaseCmd(JobControlRecord* jcr)
{
  PoolMem devname;
  BareosSocket* dir = jcr->dir_bsock;
  Device* dev;
  DeviceControlRecord* dcr;
  int drive_input = -1;

  if (sscanf(dir->msg, releasecmd, devname.c_str(), &drive_input) == 2) {
    drive_number_t drive = IntToDriveNumber(drive_input);
    dcr = FindDevice(jcr, devname, drive, NULL);
    if (dcr) {
      dev = dcr->dev;
      dev->Lock(); /* Use P to avoid indefinite block */
      if (!dev->IsOpen()) {
        if (!dev->IsBusy()) { UnloadAutochanger(dcr, kInvalidSlotNumber); }
        Dmsg0(90, "Device already released\n");
        dir->fsend(_("3921 Device \"%s\" already released.\n"),
                   dev->print_name());

      } else if (dev->blocked() == BST_WAITING_FOR_SYSOP) {
        Dmsg2(90, "%d waiter dev_block=%d.\n", dev->num_waiting,
              dev->blocked());
        UnloadAutochanger(dcr, kInvalidSlotNumber);
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

      } else if (dev->IsBusy()) {
        SendDirBusyMessage(dir, dev);
      } else { /* device not being used */
        Dmsg0(90, "Device not in use, releasing\n");
        dcr->ReleaseVolume();
        dir->fsend(_("3022 Device \"%s\" released.\n"), dev->print_name());
      }
      dev->Unlock();
      FreeDeviceControlRecord(dcr);
    } else {
      dir->fsend(_("3999 Device \"%s\" not found or could not be opened.\n"),
                 devname.c_str());
    }
  } else {
    /* NB dir->msg gets clobbered in BnetFsend, so save command */
    PmStrcpy(jcr->errmsg, dir->msg);
    dir->fsend(_("3927 Error scanning release command: %s\n"), jcr->errmsg);
  }
  dir->signal(BNET_EOD);
  return true;
}

static pthread_mutex_t bsr_mutex = PTHREAD_MUTEX_INITIALIZER;
static uint32_t bsr_uniq = 0;

static inline bool GetBootstrapFile(JobControlRecord* jcr, BareosSocket* sock)
{
  POOLMEM* fname = GetPoolMemory(PM_FNAME);
  FILE* bs;
  bool ok = false;

  if (jcr->RestoreBootstrap) {
    SecureErase(jcr, jcr->RestoreBootstrap);
    FreePoolMemory(jcr->RestoreBootstrap);
  }
  P(bsr_mutex);
  bsr_uniq++;
  Mmsg(fname, "%s/%s.%s.%d.bootstrap", me->working_directory,
       me->resource_name_, jcr->Job, bsr_uniq);
  V(bsr_mutex);
  Dmsg1(400, "bootstrap=%s\n", fname);
  jcr->RestoreBootstrap = fname;
  bs = fopen(fname, "a+b"); /* create file */
  if (!bs) {
    BErrNo be;
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
  jcr->bsr = libbareos::parse_bsr(jcr, jcr->RestoreBootstrap);
  if (!jcr->bsr) {
    Jmsg(jcr, M_FATAL, 0, _("Error parsing bootstrap file.\n"));
    goto bail_out;
  }
  if (debug_level >= 10) { libbareos::DumpBsr(jcr->bsr, true); }
  /* If we got a bootstrap, we are reading, so create read volume list */
  CreateRestoreVolumeList(jcr);
  ok = true;

bail_out:
  SecureErase(jcr, jcr->RestoreBootstrap);
  FreePoolMemory(jcr->RestoreBootstrap);
  jcr->RestoreBootstrap = NULL;
  if (!ok) {
    sock->fsend(ERROR_bootstrap);
    return false;
  }
  return sock->fsend(OK_bootstrap);
}

static bool BootstrapCmd(JobControlRecord* jcr)
{
  return GetBootstrapFile(jcr, jcr->dir_bsock);
}

/**
 * Autochanger command from Director
 */
static bool ChangerCmd(JobControlRecord* jcr)
{
  slot_number_t src_slot, dst_slot;
  PoolMem devname;
  BareosSocket* dir = jcr->dir_bsock;
  Device* dev;
  DeviceControlRecord* dcr;
  const char* cmd = NULL;
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
  } else if (sscanf(dir->msg, "autochanger slots %127s", devname.c_str()) ==
             1) {
    cmd = "slots";
    safe_cmd = ok = true;
  } else if (sscanf(dir->msg, "autochanger drives %127s", devname.c_str()) ==
             1) {
    cmd = "drives";
    safe_cmd = ok = true;
  } else if (sscanf(dir->msg, "autochanger transfer %127s %hd %hd",
                    devname.c_str(), &src_slot, &dst_slot) == 3) {
    cmd = "transfer";
    safe_cmd = ok = true;
    is_transfer = true;
  }
  if (ok) {
    dcr = FindDevice(jcr, devname, kInvalidDriveNumber, NULL);
    if (dcr) {
      dev = dcr->dev;
      dev->Lock(); /* Use P to avoid indefinite block */
      if (!dev->device->changer_res) {
        dir->fsend(_("3998 Device \"%s\" is not an autochanger.\n"),
                   dev->print_name());
        /* Under certain "safe" conditions, we can steal the lock */
      } else if (safe_cmd || !dev->IsOpen() || dev->CanStealLock()) {
        if (is_transfer) {
          AutochangerTransferCmd(dcr, dir, src_slot, dst_slot);
        } else {
          AutochangerCmd(dcr, dir, cmd);
        }
      } else if (dev->IsBusy() || dev->IsBlocked()) {
        SendDirBusyMessage(dir, dev);
      } else { /* device not being used */
        if (is_transfer) {
          AutochangerTransferCmd(dcr, dir, src_slot, dst_slot);
        } else {
          AutochangerCmd(dcr, dir, cmd);
        }
      }
      dev->Unlock();
      FreeDeviceControlRecord(dcr);
    } else {
      dir->fsend(_("3999 Device \"%s\" not found or could not be opened.\n"),
                 devname.c_str());
    }
  } else { /* error on scanf */
    PmStrcpy(jcr->errmsg, dir->msg);
    dir->fsend(
        _("3908 Error scanning autochanger drives/list/slots command: %s\n"),
        jcr->errmsg);
  }
  dir->signal(BNET_EOD);
  return true;
}

/**
 * Read and return the Volume label
 */
static bool ReadlabelCmd(JobControlRecord* jcr)
{
  PoolMem devname;
  BareosSocket* dir = jcr->dir_bsock;
  Device* dev;
  DeviceControlRecord* dcr;
  slot_number_t slot;
  int drive_input = -1;

  if (sscanf(dir->msg, readlabelcmd, devname.c_str(), &slot, &drive_input) ==
      3) {
    drive_number_t drive = IntToDriveNumber(drive_input);
    dcr = FindDevice(jcr, devname, drive, NULL);
    if (dcr) {
      dev = dcr->dev;
      dev->Lock(); /* Use P to avoid indefinite block */
      if (!dev->IsOpen()) {
        ReadVolumeLabel(jcr, dcr, dev, slot);
        dev->close(dcr);
        /* Under certain "safe" conditions, we can steal the lock */
      } else if (dev->CanStealLock()) {
        ReadVolumeLabel(jcr, dcr, dev, slot);
      } else if (dev->IsBusy() || dev->IsBlocked()) {
        SendDirBusyMessage(dir, dev);
      } else { /* device not being used */
        ReadVolumeLabel(jcr, dcr, dev, slot);
      }
      dev->Unlock();
      FreeDeviceControlRecord(dcr);
    } else {
      dir->fsend(_("3999 Device \"%s\" not found or could not be opened.\n"),
                 devname.c_str());
    }
  } else {
    PmStrcpy(jcr->errmsg, dir->msg);
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
static void ReadVolumeLabel(JobControlRecord* jcr,
                            DeviceControlRecord* dcr,
                            Device* dev,
                            slot_number_t Slot)
{
  BareosSocket* dir = jcr->dir_bsock;
  bsteal_lock_t hold;

  dcr->SetDev(dev);
  StealDeviceLock(dev, &hold, BST_WRITING_LABEL);

  switch (TryAutoloadDevice(dcr->jcr, dcr, Slot, "")) {
    case -1:
    case -2:
      goto bail_out;
    case 0:
    default:
      break;
  }

  dev->ClearLabeled(); /* force read of label */
  switch (ReadDevVolumeLabel(dcr)) {
    case VOL_OK:
      /* DO NOT add quotes around the Volume name. It is scanned in the DIR */
      dir->fsend(_("3001 Volume=%s Slot=%hd\n"), dev->VolHdr.VolumeName, Slot);
      Dmsg1(100, "Volume: %s\n", dev->VolHdr.VolumeName);
      break;
    default:
      dir->fsend(
          _("3902 Cannot mount Volume on Storage Device \"%s\" because:\n%s"),
          dev->print_name(), jcr->errmsg);
      break;
  }

bail_out:
  GiveBackDeviceLock(dev, &hold);

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
static int TryAutoloadDevice(JobControlRecord* jcr,
                             DeviceControlRecord* dcr,
                             slot_number_t Slot,
                             const char* VolName)
{
  BareosSocket* dir = jcr->dir_bsock;

  bstrncpy(dcr->VolumeName, VolName, sizeof(dcr->VolumeName));
  dcr->VolCatInfo.Slot = Slot;
  dcr->VolCatInfo.InChanger = Slot > 0;

  return AutoloadDevice(dcr, 0, dir);
}

static void SendDirBusyMessage(BareosSocket* dir, Device* dev)
{
  if (dev->IsBlocked()) {
    switch (dev->blocked()) {
      case BST_UNMOUNTED:
        dir->fsend(_("3931 Device \"%s\" is BLOCKED. user unmounted.\n"),
                   dev->print_name());
        break;
      case BST_UNMOUNTED_WAITING_FOR_SYSOP:
        dir->fsend(_("3932 Device \"%s\" is BLOCKED. user unmounted during "
                     "wait for media/mount.\n"),
                   dev->print_name());
        break;
      case BST_WAITING_FOR_SYSOP:
        dir->fsend(_("3933 Device \"%s\" is BLOCKED waiting for media.\n"),
                   dev->print_name());
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
  } else if (dev->CanRead()) {
    dir->fsend(_("3936 Device \"%s\" is busy reading.\n"), dev->print_name());
  } else {
    dir->fsend(_("3937 Device \"%s\" is busy with writers=%d reserved=%d.\n"),
               dev->print_name(), dev->num_writers, dev->NumReserved());
  }
}

static void SetStorageAuthKeyAndTlsPolicy(JobControlRecord* jcr,
                                          char* key,
                                          TlsPolicy policy)
{
  if (!*key) { return; }

  if (jcr->sd_auth_key) { free(jcr->sd_auth_key); }

  jcr->sd_auth_key = strdup(key);
  Dmsg0(5, "set sd auth key\n");

  jcr->sd_tls_policy = policy;
  Dmsg1(5, "set sd ssl_policy to %d\n", policy);
}

/**
 * Listen for incoming replication session from other SD.
 */
static bool ListenCmd(JobControlRecord* jcr) { return DoListenRun(jcr); }

enum class ReplicateCmdState
{
  kInit,
  kConnected,
  kTlsEstablished,
  kAuthenticated,
  kError
};

class ReplicateCmdConnectState {
  JobControlRecord* jcr_;
  ReplicateCmdState state_;

 public:
  ReplicateCmdConnectState(JobControlRecord* jcr)
      : jcr_(jcr), state_(ReplicateCmdState::kInit)
  {
  }

  ~ReplicateCmdConnectState()
  {
    if (state_ == ReplicateCmdState::kInit ||
        state_ == ReplicateCmdState::kError) {
      if (!jcr_) { return; }
      if (jcr_->dir_bsock) {
        jcr_->dir_bsock->fsend(BADcmd, "replicate", jcr_->dir_bsock->msg);
      }
      jcr_->store_bsock = nullptr;
    }
  }
  void operator()(ReplicateCmdState state) { state_ = state; }
};

static bool ReplicateCmd(JobControlRecord* jcr)
{
  int stored_port;      /* storage daemon port */
  TlsPolicy tls_policy; /* enable ssl to sd */
  char JobName[MAX_NAME_LENGTH];
  char stored_addr[MAX_NAME_LENGTH];
  uint32_t JobId = 0;
  PoolMem sd_auth_key(PM_MESSAGE);
  BareosSocket* dir = jcr->dir_bsock;
  std::unique_ptr<BareosSocket> storage_daemon_socket =
      std::make_unique<BareosSocketTCP>();

  if (me->nokeepalive) { storage_daemon_socket->ClearKeepalive(); }
  Dmsg1(100, "ReplicateCmd: %s", dir->msg);
  sd_auth_key.check_size(dir->message_length);

  if (sscanf(dir->msg, replicatecmd, &JobId, JobName, stored_addr, &stored_port,
             &tls_policy, sd_auth_key.c_str()) != 6) {
    dir->fsend(BADcmd, "replicate", dir->msg);
    return false;
  }

  SetStorageAuthKeyAndTlsPolicy(jcr, sd_auth_key.c_str(), tls_policy);

  Dmsg3(110, "Open storage: %s:%d ssl=%d\n", stored_addr, stored_port,
        tls_policy);

  storage_daemon_socket->SetSourceAddress(me->SDsrc_addr);

  if (!jcr->max_bandwidth) {
    if (jcr->director->max_bandwidth_per_job) {
      jcr->max_bandwidth = jcr->director->max_bandwidth_per_job;
    } else if (me->max_bandwidth_per_job) {
      jcr->max_bandwidth = me->max_bandwidth_per_job;
    }
  }

  storage_daemon_socket->SetBwlimit(jcr->max_bandwidth);
  if (me->allow_bw_bursting) { storage_daemon_socket->SetBwlimitBursting(); }

  ReplicateCmdConnectState connect_state(jcr);

  if (!storage_daemon_socket->connect(
          jcr, 10, (int)me->SDConnectTimeout, me->heartbeat_interval,
          _("Storage daemon"), stored_addr, NULL, stored_port, 1)) {
    Jmsg(jcr, M_FATAL, 0, _("Failed to connect to Storage daemon: %s:%d\n"),
         stored_addr, stored_port);
    Dmsg2(100, "Failed to connect to Storage daemon: %s:%d\n", stored_addr,
          stored_port);
    connect_state(ReplicateCmdState::kError);
    return false;
  }
  Dmsg0(110, "Connection OK to SD.\n");
  connect_state(ReplicateCmdState::kConnected);

  if (tls_policy == TlsPolicy::kBnetTlsAuto) {
    std::string qualified_resource_name;
    if (!my_config->GetQualifiedResourceNameTypeConverter()->ResourceToString(
            JobName, R_JOB, qualified_resource_name)) {
      connect_state(ReplicateCmdState::kError);
      return false;
    } else if (!storage_daemon_socket->DoTlsHandshake(
                   TlsPolicy::kBnetTlsAuto, me, false,
                   qualified_resource_name.c_str(), jcr->sd_auth_key, jcr)) {
      Dmsg0(110, "TLS direct handshake failed\n");
      connect_state(ReplicateCmdState::kError);
      return false;
    } else {
      connect_state(ReplicateCmdState::kTlsEstablished);
    }
  }

  jcr->store_bsock = storage_daemon_socket.get();
  jcr->store_bsock->SetNwdump(BnetDump::Create(
      me, R_STORAGE, *my_config->GetQualifiedResourceNameTypeConverter()));

  storage_daemon_socket->fsend("Hello Start Storage Job %s\n", JobName);

  if (!AuthenticateWithStoragedaemon(jcr)) {
    Jmsg(jcr, M_FATAL, 0, _("Failed to authenticate Storage daemon.\n"));
    connect_state(ReplicateCmdState::kError);
    return false;
  } else {
    connect_state(ReplicateCmdState::kAuthenticated);
    Dmsg0(110, "Authenticated with SD.\n");

    jcr->remote_replicate = true;

    storage_daemon_socket.release(); /* jcr->store_bsock */
    return dir->fsend(OK_replicate);
  }
}

static bool RunCmd(JobControlRecord* jcr)
{
  Dmsg1(200, "Run_cmd: %s\n", jcr->dir_bsock->msg);

  /*
   * If we do not need the FD, we are doing a migrate, copy, or virtual backup.
   */
  if (jcr->NoClientUsed()) {
    return DoMacRun(jcr);
  } else {
    return DoJobRun(jcr);
  }
}

static bool PassiveCmd(JobControlRecord* jcr)
{
  int filed_port;       /* file daemon port */
  TlsPolicy tls_policy; /* enable ssl to fd */
  char filed_addr[MAX_NAME_LENGTH];
  BareosSocket* dir = jcr->dir_bsock;
  BareosSocket* fd; /* file daemon bsock */

  Dmsg1(100, "PassiveClientCmd: %s", dir->msg);
  if (sscanf(dir->msg, passiveclientcmd, filed_addr, &filed_port,
             &tls_policy) != 3) {
    PmStrcpy(jcr->errmsg, dir->msg);
    Jmsg(jcr, M_FATAL, 0, _("Bad passiveclientcmd command: %s"), jcr->errmsg);
    goto bail_out;
  }

  Dmsg3(110, "PassiveClientCmd: %s:%d ssl=%d\n", filed_addr, filed_port,
        tls_policy);

  jcr->passive_client = true;

  fd = new BareosSocketTCP;
  if (me->nokeepalive) { fd->ClearKeepalive(); }
  fd->SetSourceAddress(me->SDsrc_addr);

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
    Dmsg2(100, "Failed to connect to File daemon: %s:%d\n", filed_addr,
          filed_port);
    goto bail_out;
  }
  Dmsg0(110, "Connection OK to FD.\n");

  if (tls_policy == TlsPolicy::kBnetTlsAuto) {
    std::string qualified_resource_name;
    if (!my_config->GetQualifiedResourceNameTypeConverter()->ResourceToString(
            jcr->Job, R_JOB, qualified_resource_name)) {
      goto bail_out;
    }

    if (!fd->DoTlsHandshake(TlsPolicy::kBnetTlsAuto, me, false,
                            qualified_resource_name.c_str(), jcr->sd_auth_key,
                            jcr)) {
      goto bail_out;
    }
  }

  fd->FillBSockWithConnectedDaemonInformation(*my_config, R_CLIENT);

  jcr->file_bsock = fd;
  fd->fsend("Hello Storage calling Start Job %s\n", jcr->Job);
  if (!AuthenticateWithFiledaemon(jcr)) {
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
    UpdateJobStatistics(jcr, now);
  }

  /*
   * Send OK to Director
   */
  return dir->fsend(OKpassive);

bail_out:
  dir->fsend(BADcmd, "passive client");
  return false;
}

static bool PluginoptionsCmd(JobControlRecord* jcr)
{
  BareosSocket* dir = jcr->dir_bsock;
  char plugin_options[2048];

  Dmsg1(100, "PluginOptionsCmd: %s", dir->msg);
  if (sscanf(dir->msg, pluginoptionscmd, plugin_options) != 1) {
    PmStrcpy(jcr->errmsg, dir->msg);
    Jmsg(jcr, M_FATAL, 0, _("Bad pluginoptionscmd command: %s"), jcr->errmsg);
    goto bail_out;
  }

  UnbashSpaces(plugin_options);
  if (!jcr->plugin_options) {
    jcr->plugin_options = new alist(10, owned_by_alist);
  }
  jcr->plugin_options->append(strdup(plugin_options));

  /*
   * Send OK to Director
   */
  return dir->fsend(OKpluginoptions);

bail_out:
  dir->fsend(BADcmd, "plugin options");
  return false;
}

} /* namespace storagedaemon */
