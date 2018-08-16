/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2016-2016 Bareos GmbH & Co. KG

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
 * Kern Sibbald, March 2005
 */
/**
 * @file
 * Subroutines to handle waiting for operator intervention
 * or waiting for a Device to be released
 *
 * Code for WaitForSysop() pulled from askdir.c
 */

#include "include/bareos.h"                   /* pull in global headers */
#include "stored/stored.h"                   /* pull in Storage Daemon headers */
#include "lib/edit.h"
#include "include/jcr.h"

namespace storagedaemon {

const int debuglevel = 400;

static pthread_mutex_t device_release_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t wait_device_release = PTHREAD_COND_INITIALIZER;

/**
 * Wait for SysOp to mount a tape on a specific device
 *
 *   Returns: W_ERROR, W_TIMEOUT, W_POLL, W_MOUNT, or W_WAKE
 */
int WaitForSysop(DeviceControlRecord *dcr)
{
   struct timeval tv;
   struct timezone tz;
   struct timespec timeout;
   time_t last_heartbeat = 0;
   time_t first_start = time(NULL);
   int status = 0;
   int add_wait;
   bool unmounted;
   Device *dev = dcr->dev;
   JobControlRecord *jcr = dcr->jcr;

   dev->Lock();
   Dmsg1(debuglevel, "Enter blocked=%s\n", dev->print_blocked());

   /*
    * Since we want to mount a tape, make sure current one is
    *  not marked as using this drive.
    */
   VolumeUnused(dcr);

   unmounted = dev->IsDeviceUnmounted();
   dev->poll = false;
   /*
    * Wait requested time (dev->rem_wait_sec).  However, we also wake up every
    *    HB_TIME seconds and send a heartbeat to the FD and the Director
    *    to keep stateful firewalls from closing them down while waiting
    *    for the operator.
    */
   add_wait = dev->rem_wait_sec;
   if (me->heartbeat_interval && add_wait > me->heartbeat_interval) {
      add_wait = me->heartbeat_interval;
   }
   /* If the user did not unmount the tape and we are polling, ensure
    *  that we poll at the correct interval.
    */
   if (!unmounted && dev->vol_poll_interval && add_wait > dev->vol_poll_interval) {
      add_wait = dev->vol_poll_interval;
   }

   if (!unmounted) {
      Dmsg1(debuglevel, "blocked=%s\n", dev->print_blocked());
      dev->dev_prev_blocked = dev->blocked();
      dev->SetBlocked(BST_WAITING_FOR_SYSOP); /* indicate waiting for mount */
   }

   while (!JobCanceled(jcr)) {
      time_t now, start, total_waited;

      gettimeofday(&tv, &tz);
      timeout.tv_nsec = tv.tv_usec * 1000;
      timeout.tv_sec = tv.tv_sec + add_wait;

      Dmsg4(debuglevel, "I'm going to sleep on device %s. HB=%d rem_wait=%d add_wait=%d\n",
         dev->print_name(), (int)me->heartbeat_interval, dev->rem_wait_sec, add_wait);
      start = time(NULL);

      /* Wait required time */
      status = pthread_cond_timedwait(&dev->wait_next_vol, &dev->mutex_, &timeout);

      Dmsg2(debuglevel, "Wokeup from sleep on device status=%d blocked=%s\n", status,
         dev->print_blocked());
      now = time(NULL);
      total_waited = now - first_start;
      dev->rem_wait_sec -= (now - start);

      /* Note, this always triggers the first time. We want that. */
      if (me->heartbeat_interval) {
         if (now - last_heartbeat >= me->heartbeat_interval) {
            /* send heartbeats */
            if (jcr->file_bsock) {
               jcr->file_bsock->signal(BNET_HEARTBEAT);
               Dmsg0(debuglevel, "Send heartbeat to FD.\n");
            }
            if (jcr->dir_bsock) {
               jcr->dir_bsock->signal(BNET_HEARTBEAT);
            }
            last_heartbeat = now;
         }
      }

      if (status == EINVAL) {
         BErrNo be;
         Jmsg1(jcr, M_FATAL, 0, _("pthread timedwait error. ERR=%s\n"), be.bstrerror(status));
         status = W_ERROR;             /* error */
         break;
      }

      /*
       * Continue waiting if operator is labeling volumes
       */
      if (dev->blocked() == BST_WRITING_LABEL) {
         continue;
      }

      if (dev->rem_wait_sec <= 0) {  /* on exceeding wait time return */
         Dmsg0(debuglevel, "Exceed wait time.\n");
         status = W_TIMEOUT;
         break;
      }

      /*
       * Check if user unmounted the device while we were waiting
       */
      unmounted = dev->IsDeviceUnmounted();

      if (!unmounted && dev->vol_poll_interval &&
          (total_waited >= dev->vol_poll_interval)) {
         Dmsg1(debuglevel, "poll return in wait blocked=%s\n", dev->print_blocked());
         dev->poll = true;            /* returning a poll event */
         status = W_POLL;
         break;
      }
      /*
       * Check if user mounted the device while we were waiting
       */
      if (dev->blocked() == BST_MOUNT) {   /* mount request ? */
         Dmsg0(debuglevel, "Mounted return.\n");
         status = W_MOUNT;
         break;
      }

      /*
       * If we did not timeout, then some event happened, so
       *   return to check if state changed.
       */
      if (status != ETIMEDOUT) {
         BErrNo be;
         Dmsg2(debuglevel, "Wake return. status=%d. ERR=%s\n", status, be.bstrerror(status));
         status = W_WAKE;          /* someone woke us */
         break;
      }

      /*
       * At this point, we know we woke up because of a timeout,
       *   that was due to a heartbeat, because any other reason would
       *   have caused us to return, so update the wait counters and continue.
       */
      add_wait = dev->rem_wait_sec;
      if (me->heartbeat_interval && add_wait > me->heartbeat_interval) {
         add_wait = me->heartbeat_interval;
      }
      /* If the user did not unmount the tape and we are polling, ensure
       *  that we poll at the correct interval.
       */
      if (!unmounted && dev->vol_poll_interval &&
           add_wait > dev->vol_poll_interval - total_waited) {
         add_wait = dev->vol_poll_interval - total_waited;
      }
      if (add_wait < 0) {
         add_wait = 0;
      }
   }

   if (!unmounted) {
      dev->SetBlocked(dev->dev_prev_blocked);    /* restore entry state */
      Dmsg1(debuglevel, "set %s\n", dev->print_blocked());
   }
   Dmsg1(debuglevel, "Exit blocked=%s\n", dev->print_blocked());
   dev->Unlock();
   return status;
}


/**
 * Wait for any device to be released, then we return, so
 * higher level code can rescan possible devices.  Since there
 * could be a job waiting for a drive to free up, we wait a maximum
 * of 1 minute then retry just in case a broadcast was lost, and
 * we return to rescan the devices.
 *
 * Returns: true  if a device has changed state
 *          false if the total wait time has expired.
 */
bool WaitForDevice(JobControlRecord *jcr, int &retries)
{
   struct timeval tv;
   struct timezone tz;
   struct timespec timeout;
   int status = 0;
   bool ok = true;
   const int max_wait_time = 1 * 60;       /* wait 1 minute */
   char ed1[50];

   Dmsg0(debuglevel, "Enter WaitForDevice\n");
   P(device_release_mutex);

   if (++retries % 5 == 0) {
      /* Print message every 5 minutes */
      Jmsg(jcr, M_MOUNT, 0, _("JobId=%s, Job %s waiting to reserve a device.\n"),
         edit_uint64(jcr->JobId, ed1), jcr->Job);
   }

   gettimeofday(&tv, &tz);
   timeout.tv_nsec = tv.tv_usec * 1000;
   timeout.tv_sec = tv.tv_sec + max_wait_time;

   Dmsg0(debuglevel, "Going to wait for a device.\n");

   /* Wait required time */
   status = pthread_cond_timedwait(&wait_device_release, &device_release_mutex, &timeout);
   Dmsg1(debuglevel, "Wokeup from sleep on device status=%d\n", status);

   V(device_release_mutex);
   Dmsg1(debuglevel, "Return from wait_device ok=%d\n", ok);
   return ok;
}

/**
 * Signal the above WaitForDevice function.
 */
void ReleaseDeviceCond()
{
   pthread_cond_broadcast(&wait_device_release);
}

#ifdef xxx
/**
 * The jcr timers are used for waiting on any device *
 * Returns: true if time doubled
 *          false if max time expired
 */
static bool DoubleJcrWaitTime(JobControlRecord *jcr)
{
   jcr->wait_sec *= 2;               /* double wait time */
   if (jcr->wait_sec > jcr->max_wait) {   /* but not longer than maxtime */
      jcr->wait_sec = jcr->max_wait;
   }
   jcr->num_wait++;
   jcr->rem_wait_sec = jcr->wait_sec;
   if (jcr->num_wait >= jcr->max_num_wait) {
      return false;
   }
   return true;
}
#endif

} /* namespace storagedaemon */
