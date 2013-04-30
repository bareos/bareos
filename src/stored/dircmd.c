/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2001-2012 Free Software Foundation Europe e.V.

   The main author of Bacula is Kern Sibbald, with contributions from
   many others, a complete list can be found in the file AUTHORS.
   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation and included
   in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   General Public License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.

   Bacula® is a registered trademark of Kern Sibbald.
   The licensor of Bacula is the Free Software Foundation Europe
   (FSFE), Fiduciary Program, Sumatrastrasse 25, 8006 Zürich,
   Switzerland, email:ftf@fsfeurope.org.
*/
/*
 *  This file handles accepting Director Commands
 *
 *    Most Director commands are handled here, with the
 *    exception of the Job command command and subsequent
 *    subcommands that are handled
 *    in job.c.
 *
 *    N.B. in this file, in general we must use P(dev->mutex) rather
 *      than dev->r_lock() so that we can examine the blocked
 *      state rather than blocking ourselves because a Job
 *      thread has the device blocked. In some "safe" cases,
 *      we can do things to a blocked device. CAREFUL!!!!
 *
 *    File daemon commands are handled in fdcmd.c
 *
 *     Kern Sibbald, May MMI
 *
 */

#include "bacula.h"
#include "stored.h"

/* Exported variables */

/* Imported variables */
extern BSOCK *filed_chan;
extern struct s_last_job last_job;
extern bool init_done;

/* Static variables */
static char derrmsg[]     = "3900 Invalid command:";
static char OKsetdebug[]  = "3000 OK setdebug=%d\n";
static char invalid_cmd[] = "3997 Invalid command for a Director with Monitor directive enabled.\n";
static char OK_bootstrap[]    = "3000 OK bootstrap\n";
static char ERROR_bootstrap[] = "3904 Error bootstrap\n";

/* Imported functions */
extern void terminate_child();
extern bool job_cmd(JCR *jcr);
extern bool use_cmd(JCR *jcr);
extern bool run_cmd(JCR *jcr);
extern bool status_cmd(JCR *sjcr);
extern bool qstatus_cmd(JCR *jcr);
//extern bool query_cmd(JCR *jcr);

/* Forward referenced functions */
static bool label_cmd(JCR *jcr);
static bool die_cmd(JCR *jcr);
static bool relabel_cmd(JCR *jcr);
static bool readlabel_cmd(JCR *jcr);
static bool release_cmd(JCR *jcr);
static bool setdebug_cmd(JCR *jcr);
static bool cancel_cmd(JCR *cjcr);
static bool mount_cmd(JCR *jcr);
static bool unmount_cmd(JCR *jcr);
//static bool action_on_purge_cmd(JCR *jcr);
static bool bootstrap_cmd(JCR *jcr);
static bool changer_cmd(JCR *sjcr);
static bool do_label(JCR *jcr, int relabel);
static DCR *find_device(JCR *jcr, POOL_MEM &dev_name, int drive);
static void read_volume_label(JCR *jcr, DCR *dcr, DEVICE *dev, int Slot);
static void label_volume_if_ok(DCR *dcr, char *oldname,
                               char *newname, char *poolname,
                               int Slot, int relabel);
static bool try_autoload_device(JCR *jcr, DCR *dcr, int slot, const char *VolName);
static void send_dir_busy_message(BSOCK *dir, DEVICE *dev);

struct s_cmds {
   const char *cmd;
   bool (*func)(JCR *jcr);
   bool monitoraccess;                      /* set if monitors can access this cmd */
};

/*
 * The following are the recognized commands from the Director.
 */
static struct s_cmds cmds[] = {
   {"JobId=",      job_cmd,         0},     /* start Job */
   {"autochanger", changer_cmd,     0},
   {"bootstrap",   bootstrap_cmd,   0},
   {"cancel",      cancel_cmd,      0},
   {".die",        die_cmd,         0},
   {"label",       label_cmd,       0},     /* label a tape */
   {"mount",       mount_cmd,       0},
   {"readlabel",   readlabel_cmd,   0},
   {"release",     release_cmd,     0},
   {"relabel",     relabel_cmd,     0},     /* relabel a tape */
   {"setdebug=",   setdebug_cmd,    0},     /* set debug level */
   {"status",      status_cmd,      1},
   {".status",     qstatus_cmd,     1},
   {"unmount",     unmount_cmd,     0},
//   {"action_on_purge",  action_on_purge_cmd,    0},
   {"use storage=", use_cmd,        0},
   {"run",         run_cmd,         0},
// {"query",       query_cmd,       0},
   {NULL,        NULL}                      /* list terminator */
};


/*
 * Connection request. We accept connections either from the
 *  Director or a Client (File daemon).
 *
 * Note, we are running as a seperate thread of the Storage daemon.
 *  and it is because a Director has made a connection with
 *  us on the "Message" channel.
 *
 * Basic tasks done here:
 *  - Create a JCR record
 *  - If it was from the FD, call handle_filed_connection()
 *  - Authenticate the Director
 *  - We wait for a command
 *  - We execute the command
 *  - We continue or exit depending on the return status
 */
void *handle_connection_request(void *arg)
{
   BSOCK *bs = (BSOCK *)arg;
   JCR *jcr;
   int i;
   bool found, quit;
   int bnet_stat = 0;
   char name[500];
   char tbuf[100];

   if (bs->recv() <= 0) {
      Emsg1(M_ERROR, 0, _("Connection request from %s failed.\n"), bs->who());
      bmicrosleep(5, 0);   /* make user wait 5 seconds */
      bs->close();
      return NULL;
   }

   /*
    * Do a sanity check on the message received
    */
   if (bs->msglen < 25 || bs->msglen > (int)sizeof(name)) {
      Dmsg1(000, "<filed: %s", bs->msg);
      Emsg2(M_ERROR, 0, _("Invalid connection from %s. Len=%d\n"), bs->who(), bs->msglen);
      bmicrosleep(5, 0);   /* make user wait 5 seconds */
      bs->close();
      return NULL;
   }
   /*
    * See if this is a File daemon connection. If so
    *   call FD handler.
    */
   Dmsg1(110, "Conn: %s", bs->msg);
   if (debug_level == 3) {
      Dmsg1(000, "<filed: %s", bs->msg);
   }
   if (sscanf(bs->msg, "Hello Start Job %127s", name) == 1) {
      Dmsg1(110, "Got a FD connection at %s\n", bstrftimes(tbuf, sizeof(tbuf), 
            (utime_t)time(NULL)));
      Dmsg1(50, "%s", bs->msg);
      handle_filed_connection(bs, name);
      return NULL;
   }

   /* 
    * This is a connection from the Director, so setup a JCR 
    */
   Dmsg1(110, "Got a DIR connection at %s\n", bstrftimes(tbuf, sizeof(tbuf), 
         (utime_t)time(NULL)));
   jcr = new_jcr(sizeof(JCR), stored_free_jcr); /* create Job Control Record */
   jcr->dir_bsock = bs;               /* save Director bsock */
   jcr->dir_bsock->set_jcr(jcr);
   jcr->dcrs = New(alist(10, not_owned_by_alist));
   /* Initialize FD start condition variable */
   int errstat = pthread_cond_init(&jcr->job_start_wait, NULL);
   if (errstat != 0) {
      berrno be;
      Jmsg1(jcr, M_FATAL, 0, _("Unable to init job cond variable: ERR=%s\n"), be.bstrerror(errstat));
      goto bail_out;
   }

   Dmsg0(1000, "stored in start_job\n");

   /*
    * Authenticate the Director
    */
   if (!authenticate_director(jcr)) {
      Jmsg(jcr, M_FATAL, 0, _("Unable to authenticate Director\n"));
      goto bail_out;
   }
   Dmsg0(90, "Message channel init completed.\n");

   for (quit=false; !quit;) {
      /* Read command */
      if ((bnet_stat = bs->recv()) <= 0) {
         break;               /* connection terminated */
      }
      Dmsg1(199, "<dird: %s\n", bs->msg);
      /* Ensure that device initialization is complete */
      while (!init_done) {
         bmicrosleep(1, 0);
      }
      found = false;
      for (i=0; cmds[i].cmd; i++) {
        if (strncmp(cmds[i].cmd, bs->msg, strlen(cmds[i].cmd)) == 0) {
           if ((!cmds[i].monitoraccess) && (jcr->director->monitor)) {
              Dmsg1(100, "Command \"%s\" is invalid.\n", cmds[i].cmd);
              bs->fsend(invalid_cmd);
              bs->signal(BNET_EOD);
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
        POOL_MEM err_msg;
        Mmsg(err_msg, "%s %s\n", derrmsg, bs->msg);
        bs->fsend(err_msg.c_str());
        break;
      }
   }
bail_out:
   generate_daemon_event(jcr, "JobEnd");
   generate_plugin_event(jcr, bsdEventJobEnd);
   dequeue_messages(jcr);             /* send any queued messages */
   bs->signal(BNET_TERMINATE);
   free_plugins(jcr);                 /* release instantiated plugins */
   free_jcr(jcr);
   return NULL;
}


/*
 * Force SD to die, and hopefully dump itself.  Turned on only
 *  in development version.
 */
static bool die_cmd(JCR *jcr)
{
#ifdef DEVELOPER
   JCR *djcr = NULL;
   int a;
   BSOCK *dir = jcr->dir_bsock;
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
   return 0;
}

     

/*
 * Set debug level as requested by the Director
 *
 */
static bool setdebug_cmd(JCR *jcr)
{
   BSOCK *dir = jcr->dir_bsock;
   int32_t level, trace_flag;

   Dmsg1(10, "setdebug_cmd: %s", dir->msg);
   if (sscanf(dir->msg, "setdebug=%d trace=%d", &level, &trace_flag) != 2 || level < 0) {
      dir->fsend(_("3991 Bad setdebug command: %s\n"), dir->msg);
      return 0;
   }
   debug_level = level;
   set_trace(trace_flag);
   return dir->fsend(OKsetdebug, level);
}


/*
 * Cancel a Job
 *   Be careful, we switch to using the job's JCR! So, using
 *   BSOCKs on that jcr can have two threads in the same code.
 */
static bool cancel_cmd(JCR *cjcr)
{
   BSOCK *dir = cjcr->dir_bsock;
   int oldStatus;
   char Job[MAX_NAME_LENGTH];
   JCR *jcr;
   int status;
   const char *reason;

   if (sscanf(dir->msg, "cancel Job=%127s", Job) == 1) {
      status = JS_Canceled;
      reason = "canceled";
   } else {
      dir->fsend(_("3903 Error scanning cancel command.\n"));
      goto bail_out;
   }
   if (!(jcr=get_jcr_by_full_name(Job))) {
      dir->fsend(_("3904 Job %s not found.\n"), Job);
   } else {
      oldStatus = jcr->JobStatus;
      jcr->setJobStatus(status);
      Dmsg2(800, "Cancel JobId=%d %p\n", jcr->JobId, jcr);
      if (!jcr->authenticated && oldStatus == JS_WaitFD) {
         pthread_cond_signal(&jcr->job_start_wait); /* wake waiting thread */
      }
      if (jcr->file_bsock) {
         jcr->file_bsock->set_terminated();
         jcr->file_bsock->set_timed_out();
         Dmsg2(800, "Term bsock jid=%d %p\n", jcr->JobId, jcr);
      } else {
         /* Still waiting for FD to connect, release it */
         pthread_cond_signal(&jcr->job_start_wait); /* wake waiting job */
         Dmsg2(800, "Signal FD connect jid=%d %p\n", jcr->JobId, jcr);
      }
      /* If thread waiting on mount, wake him */
      if (jcr->dcr && jcr->dcr->dev && jcr->dcr->dev->waiting_for_mount()) {
         pthread_cond_broadcast(&jcr->dcr->dev->wait_next_vol);
         Dmsg1(100, "JobId=%u broadcast wait_device_release\n", (uint32_t)jcr->JobId);
         pthread_cond_broadcast(&wait_device_release);
      }
      if (jcr->read_dcr && jcr->read_dcr->dev && jcr->read_dcr->dev->waiting_for_mount()) {
         pthread_cond_broadcast(&jcr->read_dcr->dev->wait_next_vol);
         Dmsg1(100, "JobId=%u broadcast wait_device_release\n", (uint32_t)jcr->JobId);
         pthread_cond_broadcast(&wait_device_release);
      }
      dir->fsend(_("3000 JobId=%ld Job=\"%s\" marked to be %s.\n"), 
         jcr->JobId, jcr->Job, reason);
      free_jcr(jcr);
   }

bail_out:
   dir->signal(BNET_EOD);
   return 1;
}

/*
 * Label a Volume
 *
 */
static bool label_cmd(JCR *jcr)
{
   return do_label(jcr, 0);
}

static bool relabel_cmd(JCR *jcr)
{
   return do_label(jcr, 1);
}

static bool do_label(JCR *jcr, int relabel)
{
   POOLMEM *newname, *oldname, *poolname, *mtype;
   POOL_MEM dev_name;
   BSOCK *dir = jcr->dir_bsock;
   DCR *dcr;
   DEVICE *dev;
   bool ok = false;
   int32_t slot, drive;

   newname = get_memory(dir->msglen+1);
   oldname = get_memory(dir->msglen+1);
   poolname = get_memory(dir->msglen+1);
   mtype = get_memory(dir->msglen+1);
   if (relabel) {
      if (sscanf(dir->msg, "relabel %127s OldName=%127s NewName=%127s PoolName=%127s "
                 "MediaType=%127s Slot=%d drive=%d",
                  dev_name.c_str(), oldname, newname, poolname, mtype, 
                  &slot, &drive) == 7) {
         ok = true;
      }
   } else {
      *oldname = 0;
      if (sscanf(dir->msg, "label %127s VolumeName=%127s PoolName=%127s "
                 "MediaType=%127s Slot=%d drive=%d", 
          dev_name.c_str(), newname, poolname, mtype, &slot, &drive) == 6) {
         ok = true;
      }
   }
   if (ok) {
      unbash_spaces(newname);
      unbash_spaces(oldname);
      unbash_spaces(poolname);
      unbash_spaces(mtype);
      dcr = find_device(jcr, dev_name, drive);
      if (dcr) {
         dev = dcr->dev;
         dev->Lock();                 /* Use P to avoid indefinite block */
         if (!dev->is_open() && !dev->is_busy()) {
            Dmsg1(400, "Can %slabel. Device is not open\n", relabel?"re":"");
            label_volume_if_ok(dcr, oldname, newname, poolname, slot, relabel);
            dev->close();
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
   free_memory(mtype);
   dir->signal(BNET_EOD);
   return true;
}

/*
 * Read the tape label and determine if we can safely
 * label the tape (not a Bacula volume), then label it.
 *
 *  Enter with the mutex set
 */
static void label_volume_if_ok(DCR *dcr, char *oldname,
                               char *newname, char *poolname,
                               int slot, int relabel)
{
   BSOCK *dir = dcr->jcr->dir_bsock;
   bsteal_lock_t hold;
   DEVICE *dev = dcr->dev;
   int label_status;
   int mode;
   const char *volname = (relabel == 1) ? oldname : newname;
   char ed1[50];

   steal_device_lock(dev, &hold, BST_WRITING_LABEL);
   Dmsg1(100, "Stole device %s lock, writing label.\n", dev->print_name());


   Dmsg0(90, "try_autoload_device - looking for volume_info\n");
   if (!try_autoload_device(dcr->jcr, dcr, slot, volname)) {
      goto bail_out;                  /* error */
   }

   /* Ensure that the device is open -- autoload_device() closes it */
   if (dev->is_tape()) {
      mode = OPEN_READ_WRITE;
   } else {
      mode = CREATE_READ_WRITE;
   }

   if (relabel) {
      dev->truncating = true;         /* let open() know we will truncate it */
   }
   /* Set old volume name for open if relabeling */
   dcr->setVolCatName(volname);
   if (!dev->open(dcr, mode)) {
      dir->fsend(_("3910 Unable to open device \"%s\": ERR=%s\n"),
         dev->print_name(), dev->bstrerror());
      goto bail_out;      
   }

   /* See what we have for a Volume */
   label_status = read_dev_volume_label(dcr);
   
   /* Set new volume name */
   dcr->setVolCatName(newname);
   switch(label_status) {
   case VOL_NAME_ERROR:
   case VOL_VERSION_ERROR:
   case VOL_LABEL_ERROR:
   case VOL_OK:
      if (!relabel) {
         dir->fsend(_(
            "3920 Cannot label Volume because it is already labeled: \"%s\"\n"),
             dev->VolHdr.VolumeName);
         break;
      }

      /* Relabel request. If oldname matches, continue */
      if (strcmp(oldname, dev->VolHdr.VolumeName) != 0) {
         dir->fsend(_("3921 Wrong volume mounted.\n"));
         break;
      }
      if (dev->label_type != B_BACULA_LABEL) {
         dir->fsend(_("3922 Cannot relabel an ANSI/IBM labeled Volume.\n"));
         break;
      }
      /* Fall through wanted! */
   case VOL_IO_ERROR:
   case VOL_NO_LABEL:
      if (!write_new_volume_label_to_dev(dcr, newname, poolname, 
              relabel, true /* write dvd now */)) {
         dir->fsend(_("3912 Failed to label Volume: ERR=%s\n"), dev->bstrerror());
         break;
      }
      bstrncpy(dcr->VolumeName, newname, sizeof(dcr->VolumeName));
      /* The following 3000 OK label. string is scanned in ua_label.c */
      dir->fsend("3000 OK label. VolBytes=%s DVD=%d Volume=\"%s\" Device=%s\n",
                 edit_uint64(dev->VolCatInfo.VolCatBytes, ed1),
                 dev->is_dvd()?1:0, newname, dev->print_name());
      break;
   case VOL_NO_MEDIA:
      dir->fsend(_("3914 Failed to label Volume (no media): ERR=%s\n"), dev->bstrerror());
      break;
   default:
      dir->fsend(_("3913 Cannot label Volume. "
"Unknown status %d from read_volume_label()\n"), label_status);
      break;
   }

bail_out:
   if (dev->is_open() && !dev->has_cap(CAP_ALWAYSOPEN)) {
      dev->close();
   }
   if (!dev->is_open()) {
      dev->clear_volhdr();
   }
   volume_unused(dcr);                   /* no longer using volume */
   give_back_device_lock(dev, &hold);
   return;
}


/*
 * Read the tape label
 *
 *  Enter with the mutex set
 */
static bool read_label(DCR *dcr)
{
   int ok;
   JCR *jcr = dcr->jcr;
   BSOCK *dir = jcr->dir_bsock;
   bsteal_lock_t hold;
   DEVICE *dev = dcr->dev;

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

/* 
 * Searches for device by name, and if found, creates a dcr and
 *  returns it.
 */
static DCR *find_device(JCR *jcr, POOL_MEM &devname, int drive)
{
   DEVRES *device;
   AUTOCHANGER *changer;
   bool found = false;
   DCR *dcr = NULL;

   unbash_spaces(devname);
   foreach_res(device, R_DEVICE) {
      /* Find resource, and make sure we were able to open it */
      if (strcmp(device->hdr.name, devname.c_str()) == 0) {
         if (!device->dev) {
            device->dev = init_dev(jcr, device);
         }
         if (!device->dev) {
            Jmsg(jcr, M_WARNING, 0, _("\n"
               "     Device \"%s\" requested by DIR could not be opened or does not exist.\n"),
                 devname.c_str());
            continue;
         }
         Dmsg1(20, "Found device %s\n", device->hdr.name);
         found = true;
         break;
      }
   }
   if (!found) {
      foreach_res(changer, R_AUTOCHANGER) {
         /* Find resource, and make sure we were able to open it */
         if (strcmp(devname.c_str(), changer->hdr.name) == 0) {
            /* Try each device in this AutoChanger */
            foreach_alist(device, changer->device) {
               Dmsg1(100, "Try changer device %s\n", device->hdr.name);
               if (!device->dev) {
                  device->dev = init_dev(jcr, device);
               }
               if (!device->dev) {
                  Dmsg1(100, "Device %s could not be opened. Skipped\n", devname.c_str());
                  Jmsg(jcr, M_WARNING, 0, _("\n"
                     "     Device \"%s\" in changer \"%s\" requested by DIR could not be opened or does not exist.\n"),
                       device->hdr.name, devname.c_str());
                  continue;
               }
               if (!device->dev->autoselect) {
                  Dmsg1(100, "Device %s not autoselect skipped.\n", devname.c_str());
                  continue;              /* device is not available */
               }
               if (drive < 0 || drive == (int)device->dev->drive_index) {
                  Dmsg1(20, "Found changer device %s\n", device->hdr.name);
                  found = true;
                  break;
               }
               Dmsg3(100, "Device %s drive wrong: want=%d got=%d skipping\n",
                  devname.c_str(), drive, (int)device->dev->drive_index);
            }
            break;                    /* we found it but could not open a device */
         }
      }
   }

   if (found) {
      Dmsg1(100, "Found device %s\n", device->hdr.name);
      dcr = new_dcr(jcr, NULL, device->dev);
      dcr->device = device;
   }
   return dcr;
}


/*
 * Mount command from Director
 */
static bool mount_cmd(JCR *jcr)
{
   POOL_MEM devname;
   BSOCK *dir = jcr->dir_bsock;
   DEVICE *dev;
   DCR *dcr;
   int32_t drive;
   int32_t slot = 0;
   bool ok;

   ok = sscanf(dir->msg, "mount %127s drive=%d slot=%d", devname.c_str(), 
               &drive, &slot) == 3;
   if (!ok) {
      ok = sscanf(dir->msg, "mount %127s drive=%d", devname.c_str(), &drive) == 2;
   }
   Dmsg3(100, "ok=%d drive=%d slot=%d\n", ok, drive, slot);
   if (ok) {
      dcr = find_device(jcr, devname, drive);
      if (dcr) {
         dev = dcr->dev;
         dev->Lock();                 /* Use P to avoid indefinite block */
         Dmsg2(100, "mount cmd blocked=%d must_unload=%d\n", dev->blocked(), 
            dev->must_unload());
         switch (dev->blocked()) {         /* device blocked? */
         case BST_WAITING_FOR_SYSOP:
            /* Someone is waiting, wake him */
            Dmsg0(100, "Waiting for mount. Attempting to wake thread\n");
            dev->set_blocked(BST_MOUNT);
            dir->fsend("3001 OK mount requested. %sDevice=%s\n", 
                       slot>0?_("Specified slot ignored. "):"",
                       dev->print_name());
            pthread_cond_broadcast(&dev->wait_next_vol);
            Dmsg1(100, "JobId=%u broadcast wait_device_release\n", (uint32_t)dcr->jcr->JobId);
            pthread_cond_broadcast(&wait_device_release);
            break;

         /* In both of these two cases, we (the user) unmounted the Volume */
         case BST_UNMOUNTED_WAITING_FOR_SYSOP:
         case BST_UNMOUNTED:
            Dmsg2(100, "Unmounted changer=%d slot=%d\n", dev->is_autochanger(), slot);
            if (dev->is_autochanger() && slot > 0) {
               try_autoload_device(jcr, dcr, slot, "");
            }
            /* We freed the device, so reopen it and wake any waiting threads */
            if (!dev->open(dcr, OPEN_READ_ONLY)) {
               dir->fsend(_("3901 Unable to open device \"%s\": ERR=%s\n"),
                  dev->print_name(), dev->bstrerror());
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
               dir->fsend(_("3001 Device \"%s\" is mounted with Volume \"%s\"\n"),
                  dev->print_name(), dev->VolHdr.VolumeName);
            } else {
               dir->fsend(_("3905 Device \"%s\" open but no Bacula volume is mounted.\n"
                                 "If this is not a blank tape, try unmounting and remounting the Volume.\n"),
                          dev->print_name());
            }
            pthread_cond_broadcast(&dev->wait_next_vol);
            Dmsg1(100, "JobId=%u broadcast wait_device_release\n", (uint32_t)dcr->jcr->JobId);
            pthread_cond_broadcast(&wait_device_release);
            break;

         case BST_DOING_ACQUIRE:
            dir->fsend(_("3001 Device \"%s\" is doing acquire.\n"),
                       dev->print_name());
            break;

         case BST_WRITING_LABEL:
            dir->fsend(_("3903 Device \"%s\" is being labeled.\n"), 
               dev->print_name());
            break;

         case BST_NOT_BLOCKED:
            Dmsg2(100, "Not blocked changer=%d slot=%d\n", dev->is_autochanger(), slot);
            if (dev->is_autochanger() && slot > 0) {
               try_autoload_device(jcr, dcr, slot, "");
            }
            if (dev->is_open()) {
               if (dev->is_labeled()) {
                  dir->fsend(_("3001 Device \"%s\" is mounted with Volume \"%s\"\n"),
                     dev->print_name(), dev->VolHdr.VolumeName);
               } else {
                  dir->fsend(_("3905 Device \"%s\" open but no Bacula volume is mounted.\n"
                                 "If this is not a blank tape, try unmounting and remounting the Volume.\n"),
                             dev->print_name());
               }
            } else if (dev->is_tape()) {
               if (!dev->open(dcr, OPEN_READ_ONLY)) {
                  dir->fsend(_("3901 Unable to open device \"%s\": ERR=%s\n"),
                     dev->print_name(), dev->bstrerror());
                  break;
               }
               read_label(dcr);
               if (dev->is_labeled()) {
                  dir->fsend(_("3001 Device \"%s\" is already mounted with Volume \"%s\"\n"),
                     dev->print_name(), dev->VolHdr.VolumeName);
               } else {
                  dir->fsend(_("3905 Device \"%s\" open but no Bacula volume is mounted.\n"
                                    "If this is not a blank tape, try unmounting and remounting the Volume.\n"),
                             dev->print_name());
               }
               if (dev->is_open() && !dev->has_cap(CAP_ALWAYSOPEN)) {
                  dev->close();
               }
            } else if (dev->is_unmountable()) {
               if (dev->mount(1)) {
                  dir->fsend(_("3002 Device \"%s\" is mounted.\n"), dev->print_name());
               } else {
                  dir->fsend(_("3907 %s"), dev->bstrerror());
               } 
            } else { /* must be file */
               dir->fsend(_("3906 File device \"%s\" is always mounted.\n"),
                  dev->print_name());
               pthread_cond_broadcast(&dev->wait_next_vol);
               Dmsg1(100, "JobId=%u broadcast wait_device_release\n", (uint32_t)dcr->jcr->JobId);
               pthread_cond_broadcast(&wait_device_release);
            }
            break;

         case BST_RELEASING:
            dir->fsend(_("3930 Device \"%s\" is being released.\n"), dev->print_name());
            break;

         default:
            dir->fsend(_("3905 Unknown wait state %d\n"), dev->blocked());
            break;
         }
         dev->Unlock();
         free_dcr(dcr);
      } else {
         dir->fsend(_("3999 Device \"%s\" not found or could not be opened.\n"), devname.c_str());
      }
   } else {
      pm_strcpy(jcr->errmsg, dir->msg);
      dir->fsend(_("3909 Error scanning mount command: %s\n"), jcr->errmsg);
   }
   dir->signal(BNET_EOD);
   return true;
}

/*
 * unmount command from Director
 */
static bool unmount_cmd(JCR *jcr)
{
   POOL_MEM devname;
   BSOCK *dir = jcr->dir_bsock;
   DEVICE *dev;
   DCR *dcr;
   int32_t drive;

   if (sscanf(dir->msg, "unmount %127s drive=%d", devname.c_str(), &drive) == 2) {
      dcr = find_device(jcr, devname, drive);
      if (dcr) {
         dev = dcr->dev;
         dev->Lock();                 /* Use P to avoid indefinite block */
         if (!dev->is_open()) {
            if (!dev->is_busy()) {
               unload_autochanger(dcr, -1);          
            }
            if (dev->is_unmountable()) {
               if (dev->unmount(0)) {
                  dir->fsend(_("3002 Device \"%s\" unmounted.\n"), 
                     dev->print_name());
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
               dev->close();
               free_volume(dev);
            }
            if (dev->is_unmountable() && !dev->unmount(0)) {
               dir->fsend(_("3907 %s"), dev->bstrerror());
            } else {
               dev->set_blocked(BST_UNMOUNTED_WAITING_FOR_SYSOP);
               dir->fsend(_("3001 Device \"%s\" unmounted.\n"), 
                  dev->print_name());
            }

         } else if (dev->blocked() == BST_DOING_ACQUIRE) {
            dir->fsend(_("3902 Device \"%s\" is busy in acquire.\n"), 
               dev->print_name());

         } else if (dev->blocked() == BST_WRITING_LABEL) {
            dir->fsend(_("3903 Device \"%s\" is being labeled.\n"), 
               dev->print_name());

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
               dev->close();
               free_volume(dev);
            }
            if (dev->is_unmountable() && !dev->unmount(0)) {
               dir->fsend(_("3907 %s"), dev->bstrerror());
            } else {
               dir->fsend(_("3002 Device \"%s\" unmounted.\n"), 
                  dev->print_name());
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
/*
 * The truncate command will recycle a volume. The director can call this
 * after purging a volume so that disk space will not be wasted. Only useful
 * for File Storage, of course.
 *
 *
 * It is currently disabled
 */
static bool action_on_purge_cmd(JCR *jcr)
{
   BSOCK *dir = jcr->dir_bsock;

   char devname[MAX_NAME_LENGTH];
   char volumename[MAX_NAME_LENGTH];
   int32_t action;

   /* TODO: Need to find a free device and ask for slot to the director */
   if (sscanf(dir->msg, 
              "action_on_purge %127s vol=%127s action=%d",
              devname, volumename, &action)!= 5) 
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

/*
 * Release command from Director. This rewinds the device and if
 *   configured does a offline and ensures that Bacula will
 *   re-read the label of the tape before continuing. This gives
 *   the operator the chance to change the tape anytime before the
 *   next job starts.
 */
static bool release_cmd(JCR *jcr)
{
   POOL_MEM devname;
   BSOCK *dir = jcr->dir_bsock;
   DEVICE *dev;
   DCR *dcr;
   int32_t drive;

   if (sscanf(dir->msg, "release %127s drive=%d", devname.c_str(), &drive) == 2) {
      dcr = find_device(jcr, devname, drive);
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

static bool get_bootstrap_file(JCR *jcr, BSOCK *sock)
{
   POOLMEM *fname = get_pool_memory(PM_FNAME);
   FILE *bs;
   bool ok = false;

   if (jcr->RestoreBootstrap) {
      unlink(jcr->RestoreBootstrap);
      free_pool_memory(jcr->RestoreBootstrap);
   }
   P(bsr_mutex);
   bsr_uniq++;
   Mmsg(fname, "%s/%s.%s.%d.bootstrap", me->working_directory, me->hdr.name,
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
   unlink(jcr->RestoreBootstrap);
   free_pool_memory(jcr->RestoreBootstrap);
   jcr->RestoreBootstrap = NULL;
   if (!ok) {
      sock->fsend(ERROR_bootstrap);
      return false;
   }
   return sock->fsend(OK_bootstrap);
}

static bool bootstrap_cmd(JCR *jcr)
{
   return get_bootstrap_file(jcr, jcr->dir_bsock);
}

/*
 * Autochanger command from Director
 */
static bool changer_cmd(JCR *jcr)
{
   POOL_MEM devname;
   BSOCK *dir = jcr->dir_bsock;
   DEVICE *dev;
   DCR *dcr;
   const char *cmd = NULL;
   bool ok = false;
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
   }
   if (ok) {
      dcr = find_device(jcr, devname, -1);
      if (dcr) {
         dev = dcr->dev;
         dev->Lock();                 /* Use P to avoid indefinite block */
         if (!dev->device->changer_res) {
            dir->fsend(_("3998 Device \"%s\" is not an autochanger.\n"), 
               dev->print_name());
         /* Under certain "safe" conditions, we can steal the lock */
         } else if (safe_cmd || !dev->is_open() || dev->can_steal_lock()) {
            autochanger_cmd(dcr, dir, cmd);
         } else if (dev->is_busy() || dev->is_blocked()) {
            send_dir_busy_message(dir, dev);
         } else {                     /* device not being used */
            autochanger_cmd(dcr, dir, cmd);
         }
         dev->Unlock();
         free_dcr(dcr);
      } else {
         dir->fsend(_("3999 Device \"%s\" not found or could not be opened.\n"), devname.c_str());
      }
   } else {  /* error on scanf */
      pm_strcpy(jcr->errmsg, dir->msg);
      dir->fsend(_("3908 Error scanning autochanger drives/list/slots command: %s\n"),
         jcr->errmsg);
   }
   dir->signal(BNET_EOD);
   return true;
}

/*
 * Read and return the Volume label
 */
static bool readlabel_cmd(JCR *jcr)
{
   POOL_MEM devname;
   BSOCK *dir = jcr->dir_bsock;
   DEVICE *dev;
   DCR *dcr;
   int32_t Slot, drive;

   if (sscanf(dir->msg, "readlabel %127s Slot=%d drive=%d", devname.c_str(), 
       &Slot, &drive) == 3) {
      dcr = find_device(jcr, devname, drive);
      if (dcr) {
         dev = dcr->dev;
         dev->Lock();                 /* Use P to avoid indefinite block */
         if (!dev->is_open()) {
            read_volume_label(jcr, dcr, dev, Slot);
            dev->close();
         /* Under certain "safe" conditions, we can steal the lock */
         } else if (dev->can_steal_lock()) {
            read_volume_label(jcr, dcr, dev, Slot);
         } else if (dev->is_busy() || dev->is_blocked()) {
            send_dir_busy_message(dir, dev);
         } else {                     /* device not being used */
            read_volume_label(jcr, dcr, dev, Slot);
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


/*
 * Read the tape label
 *
 *  Enter with the mutex set
 */
static void read_volume_label(JCR *jcr, DCR *dcr, DEVICE *dev, int Slot)
{
   BSOCK *dir = jcr->dir_bsock;
   bsteal_lock_t hold;

   dcr->set_dev(dev);
   steal_device_lock(dev, &hold, BST_WRITING_LABEL);

   if (!try_autoload_device(jcr, dcr, Slot, "")) {
      goto bail_out;                  /* error */
   }

   dev->clear_labeled();              /* force read of label */
   switch (read_dev_volume_label(dcr)) {
   case VOL_OK:
      /* DO NOT add quotes around the Volume name. It is scanned in the DIR */
      dir->fsend(_("3001 Volume=%s Slot=%d\n"), dev->VolHdr.VolumeName, Slot);
      Dmsg1(100, "Volume: %s\n", dev->VolHdr.VolumeName);
      break;
   default:
      dir->fsend(_("3902 Cannot mount Volume on Storage Device \"%s\" because:\n%s"),
                 dev->print_name(), jcr->errmsg);
      break;
   }

bail_out:
   give_back_device_lock(dev, &hold);
   return;
}

static bool try_autoload_device(JCR *jcr, DCR *dcr, int slot, const char *VolName)
{
   BSOCK *dir = jcr->dir_bsock;

   bstrncpy(dcr->VolumeName, VolName, sizeof(dcr->VolumeName));
   dcr->VolCatInfo.Slot = slot;
   dcr->VolCatInfo.InChanger = slot > 0;
   if (autoload_device(dcr, 0, dir) < 0) {    /* autoload if possible */
      return false;
   }
   return true;
}

static void send_dir_busy_message(BSOCK *dir, DEVICE *dev)
{
   if (dev->is_blocked()) {
      switch (dev->blocked()) {
      case BST_UNMOUNTED:
         dir->fsend(_("3931 Device \"%s\" is BLOCKED. user unmounted.\n"),
            dev->print_name());
         break;
      case BST_UNMOUNTED_WAITING_FOR_SYSOP:
         dir->fsend(_("3932 Device \"%s\" is BLOCKED. user unmounted during wait for media/mount.\n"),
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
   } else if (dev->can_read()) {
       dir->fsend(_("3936 Device \"%s\" is busy reading.\n"),
                   dev->print_name());;
   } else {
       dir->fsend(_("3937 Device \"%s\" is busy with writers=%d reserved=%d.\n"),
          dev->print_name(), dev->num_writers, dev->num_reserved());
   }
}
