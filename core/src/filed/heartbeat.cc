/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2003-2012 Free Software Foundation Europe e.V.
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
 * Kern Sibbald, May MMIII
 */
/**
 * @file
 * Bareos File Daemon heartbeat routines
 * Listens for heartbeats coming from the SD
 * If configured, sends heartbeats to Dir
 */

#include "include/bareos.h"
#include "filed/filed.h"
#include "filed/filed_globals.h"
#include "lib/bnet.h"

namespace filedaemon {

#define WAIT_INTERVAL 5

extern "C" void *sd_heartbeat_thread(void *arg);
extern "C" void *dir_heartbeat_thread(void *arg);
extern bool no_signals;

/**
 * Listen on the SD socket for heartbeat signals.
 * Send heartbeats to the Director every HB_TIME
 *   seconds.
 */
extern "C" void *sd_heartbeat_thread(void *arg)
{
   int32_t n;
   JobControlRecord *jcr = (JobControlRecord *)arg;
   std::shared_ptr<BareosSocket> sd, dir;
   time_t last_heartbeat = time(NULL);
   time_t now;

   pthread_detach(pthread_self());

   /*
    * Get our own local copy
    */
   sd.reset(jcr->store_bsock->clone());
   dir.reset(jcr->dir_bsock->clone());

   jcr->hb_bsock = sd;
   jcr->hb_started = true;
   jcr->hb_dir_bsock = dir;
   dir->suppress_error_msgs_ = true;
   sd->suppress_error_msgs_ = true;

   /* Hang reading the socket to the SD, and every time we get
    * a heartbeat or we get a wait timeout (5 seconds), we
    * check to see if we need to send a heartbeat to the
    * Director.
    */
   while (!sd->IsStop()) {
      n = BnetWaitDataIntr(sd.get(), WAIT_INTERVAL);
      if (n < 0 || sd->IsStop()) {
         break;
      }
      if (me->heartbeat_interval) {
         now = time(NULL);
         if (now-last_heartbeat >= me->heartbeat_interval) {
            dir->signal(BNET_HEARTBEAT);
            if (dir->IsStop()) {
               break;
            }
            last_heartbeat = now;
         }
      }
      if (n == 1) {               /* input waiting */
         sd->recv();              /* read it -- probably heartbeat from sd */
         if (sd->IsStop()) {
            break;
         }
         if (sd->message_length <= 0) {
            Dmsg1(100, "Got BNET_SIG %d from SD\n", sd->message_length);
         } else {
            Dmsg2(100, "Got %d bytes from SD. MSG=%s\n", sd->message_length, sd->msg);
         }
      }
      Dmsg2(200, "wait_intr=%d stop=%d\n", n, IsBnetStop(sd.get()));
   }

   sd->close();
   dir->close();
   jcr->hb_bsock.reset();
   jcr->hb_started = false;
   jcr->hb_dir_bsock = NULL;

   return NULL;
}

/* Startup the heartbeat thread -- see above */
void StartHeartbeatMonitor(JobControlRecord *jcr)
{
   /*
    * If no signals are set, do not start the heartbeat because
    * it gives a constant stream of TIMEOUT_SIGNAL signals that
    * make debugging impossible.
    */
   if (!no_signals) {
      jcr->hb_bsock = NULL;
      jcr->hb_started = false;
      jcr->hb_dir_bsock = NULL;
      pthread_create(&jcr->heartbeat_id, NULL, sd_heartbeat_thread, (void *)jcr);
   }
}

/* Terminate the heartbeat thread. Used for both SD and DIR */
void StopHeartbeatMonitor(JobControlRecord *jcr)
{
   int cnt = 0;
   if (no_signals) {
      return;
   }
   /* Wait max 10 secs for heartbeat thread to start */
   while (!jcr->hb_started && cnt++ < 200) {
      Bmicrosleep(0, 50000);         /* wait for start */
   }

   if (jcr->hb_started) {
      jcr->hb_bsock->SetTimedOut();       /* set timed_out to Terminate read */
      jcr->hb_bsock->SetTerminated();      /* set to Terminate read */
   }

   if (jcr->hb_dir_bsock) {
      jcr->hb_dir_bsock->SetTimedOut();     /* set timed_out to Terminate read */
      jcr->hb_dir_bsock->SetTerminated();    /* set to Terminate read */
   }

   if (jcr->hb_started) {
      Dmsg0(100, "Send kill to heartbeat id\n");
      pthread_kill(jcr->heartbeat_id, TIMEOUT_SIGNAL);  /* make heartbeat thread go away */
      Bmicrosleep(0, 50000);
   }
   cnt = 0;

   /*
    * Wait max 100 secs for heartbeat thread to stop
    */
   while (jcr->hb_started && cnt++ < 200) {
      pthread_kill(jcr->heartbeat_id, TIMEOUT_SIGNAL);  /* make heartbeat thread go away */
      Bmicrosleep(0, 500000);
   }

   if (jcr->hb_bsock) {
      // delete jcr->hb_bsock;
      jcr->hb_bsock.reset();
   }

   if (jcr->hb_dir_bsock) {
      // delete jcr->hb_dir_bsock;
      jcr->hb_dir_bsock.reset();
   }
}

/**
 * Thread for sending heartbeats to the Director when there
 *   is no SD monitoring needed -- e.g. restore and verify Vol
 *   both do their own read() on the SD socket.
 */
extern "C" void *dir_heartbeat_thread(void *arg)
{
   JobControlRecord *jcr = (JobControlRecord *)arg;
   BareosSocket *dir;
   time_t last_heartbeat = time(NULL);

   pthread_detach(pthread_self());

   /*
    * Get our own local copy
    */
   dir = jcr->dir_bsock->clone();

   jcr->hb_bsock.reset(dir);
   jcr->hb_started = true;
   dir->suppress_error_msgs_ = true;

   while (!dir->IsStop()) {
      time_t now, next;

      now = time(NULL);
      next = now - last_heartbeat;
      if (next >= me->heartbeat_interval) {
         dir->signal(BNET_HEARTBEAT);
         if (dir->IsStop()) {
            break;
         }
         last_heartbeat = now;
      }
      Bmicrosleep(next, 0);
   }
   dir->close();
   jcr->hb_bsock.reset();
   jcr->hb_started = false;
   return NULL;
}

/**
 * Same as above but we don't listen to the SD
 */
void StartDirHeartbeat(JobControlRecord *jcr)
{
   if (me->heartbeat_interval) {
      jcr->dir_bsock->SetLocking();
      pthread_create(&jcr->heartbeat_id, NULL, dir_heartbeat_thread, (void *)jcr);
   }
}

void StopDirHeartbeat(JobControlRecord *jcr)
{
   if (me->heartbeat_interval) {
      StopHeartbeatMonitor(jcr);
   }
}
} /* namespace filedaemon */
