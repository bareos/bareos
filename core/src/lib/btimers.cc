/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2004-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2017-2025 Bareos GmbH & Co. KG

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
 * Process and thread timer routines, built on top of watchdogs.
 *
 * Nic Bellamy <nic@bellamy.co.nz>, October 2004.
 *
 */

#include "include/bareos.h"
#include "include/jcr.h"
#include "lib/edit.h"
#include "lib/bsock.h"
#include "lib/btimers.h"
#include "lib/watchdog.h"

const int debuglevel = 900;

/* Forward referenced functions */
static void StopBtimer(btimer_t* wid);
static btimer_t* btimer_start_common();

/* Forward referenced callback functions */
static void CallbackChildTimer(watchdog_t* self);
static void CallbackThreadTimer(watchdog_t* self);

/*
 * Start a timer on a child process of pid, kill it after wait seconds.
 *
 *  Returns: btimer_t *(pointer to btimer_t struct) on success
 *           NULL on failure
 */
btimer_t* StartChildTimer(JobControlRecord* jcr, pid_t pid, uint32_t wait)
{
  btimer_t* wid;

  wid = btimer_start_common();
  if (wid == NULL) { return NULL; }
  wid->type = TYPE_CHILD;
  wid->pid = pid;
  wid->killed = false;
  wid->keepalive = false;
  wid->jcr = jcr;

  wid->wd->callback = CallbackChildTimer;
  wid->wd->one_shot = false;
  wid->wd->interval = wait;
  RegisterWatchdog(wid->wd);

  Dmsg3(debuglevel, "Start child timer %p, pid %" PRIiz " for %d secs.\n", wid,
        static_cast<ssize_t>(pid), wait);
  return wid;
}

// Stop child timer
void StopChildTimer(btimer_t* wid)
{
  if (wid == NULL) {
    Dmsg0(debuglevel, "StopChildTimer called with NULL btimer_id\n");
    return;
  }
  Dmsg2(debuglevel, "Stop child timer %p pid %" PRIiz "\n", wid,
        static_cast<ssize_t>(wid->pid));
  StopBtimer(wid);
}

/* Set the child operates properly flag on the timer.
 *
 * this signals the timeout callback to skip the kill once.
 * When repeatedly calling this, the child will not run
 * into a timeout.
 */
void TimerKeepalive(btimer_t& t) { t.keepalive = true; }

static void CallbackChildTimer(watchdog_t* self)
{
  btimer_t* wid = (btimer_t*)self->data;
  if (wid->keepalive) {
    // we got a keepalive for the child, so we don't kill it this time
    wid->keepalive = false;
  } else if (!wid->killed) {
    /* First kill attempt; try killing it softly (kill -SONG) first */
    wid->killed = true;

    Dmsg2(debuglevel, "watchdog %p term PID %" PRIiz "\n", self,
          static_cast<ssize_t>(wid->pid));

    /* Kill -TERM the specified PID, and reschedule a -KILL for 5 seconds
     * later. (Warning: this should let dvd-writepart enough time to term
     * and kill growisofs, which takes 3 seconds, so the interval must not
     * be less than 5 seconds)
     */
    kill(wid->pid, SIGTERM);
    self->interval = 5;
  } else {
    /* This is the second call - Terminate with prejudice. */
    Dmsg2(debuglevel, "watchdog %p kill PID %" PRIiz "\n", self,
          static_cast<ssize_t>(wid->pid));

    kill(wid->pid, SIGKILL);

    /* Setting one_shot to true before we leave ensures we don't get
     * rescheduled.
     */
    self->one_shot = true;
  }
}

/*
 * Start a timer on a thread. kill it after wait seconds.
 *
 *  Returns: btimer_t *(pointer to btimer_t struct) on success
 *           NULL on failure
 */
btimer_t* StartThreadTimer(JobControlRecord* jcr, pthread_t tid, uint32_t wait)
{
  char ed1[50];
  btimer_t* wid;

  wid = btimer_start_common();
  if (wid == NULL) {
    Dmsg1(debuglevel, "StartThreadTimer return NULL from common. wait=%d.\n",
          wait);
    return NULL;
  }

  wid->type = TYPE_PTHREAD;
  wid->tid = tid;
  wid->jcr = jcr;
  wid->wd->callback = CallbackThreadTimer;
  wid->wd->one_shot = true;
  wid->wd->interval = wait;
  RegisterWatchdog(wid->wd);

  Dmsg3(debuglevel, "Start thread timer %p tid %s for %d secs.\n", wid,
        edit_pthread(tid, ed1, sizeof(ed1)), wait);

  return wid;
}

/*
 * Start a timer on a BareosSocket. kill it after wait seconds.
 *
 *  Returns: btimer_t *(pointer to btimer_t struct) on success
 *           NULL on failure
 */
btimer_t* StartBsockTimer(BareosSocket* bsock, uint32_t wait)
{
  char ed1[50];
  btimer_t* wid;

  if (wait <= 0) { /* wait should be > 0 */
    return NULL;
  }

  wid = btimer_start_common();
  if (wid == NULL) { return NULL; }

  wid->type = TYPE_BSOCK;
  wid->tid = pthread_self();
  wid->bsock = bsock;
  wid->jcr = bsock->jcr();

  wid->wd->callback = CallbackThreadTimer;
  wid->wd->one_shot = true;
  wid->wd->interval = wait;
  RegisterWatchdog(wid->wd);

  Dmsg4(debuglevel, "Start bsock timer %p tid=%s for %d secs at %llu\n", wid,
        edit_pthread(wid->tid, ed1, sizeof(ed1)), wait,
        static_cast<long long unsigned>(time(NULL)));

  return wid;
}

// Stop bsock timer
void StopBsockTimer(btimer_t* wid)
{
  char ed1[50];

  if (wid == NULL) {
    Dmsg0(900, "StopBsockTimer called with NULL btimer_id\n");
    return;
  }

  Dmsg3(debuglevel, "Stop bsock timer %p tid=%s at %llu.\n", wid,
        edit_pthread(wid->tid, ed1, sizeof(ed1)),
        static_cast<long long unsigned>(time(NULL)));
  StopBtimer(wid);
}


// Stop thread timer
void StopThreadTimer(btimer_t* wid)
{
  char ed1[50];

  if (wid == NULL) {
    Dmsg0(debuglevel, "StopThreadTimer called with NULL btimer_id\n");
    return;
  }

  Dmsg2(debuglevel, "Stop thread timer %p tid=%s.\n", wid,
        edit_pthread(wid->tid, ed1, sizeof(ed1)));
  StopBtimer(wid);
}

static void CallbackThreadTimer(watchdog_t* self)
{
  char ed1[50];
  btimer_t* wid = (btimer_t*)self->data;

  Dmsg4(debuglevel, "thread timer %p kill %s tid=%p at %llu.\n", self,
        wid->type == TYPE_BSOCK ? "bsock" : "thread",
        edit_pthread(wid->tid, ed1, sizeof(ed1)),
        static_cast<long long unsigned>(time(NULL)));
  if (wid->jcr) {
    Dmsg2(debuglevel, "killed JobId=%u Job=%s\n", wid->jcr->JobId,
          wid->jcr->Job);
  }

  if (wid->type == TYPE_BSOCK && wid->bsock) { wid->bsock->SetTimedOut(); }
  pthread_kill(wid->tid, kTimeoutSignal);
}

static btimer_t* btimer_start_common()
{
  btimer_t* wid = (btimer_t*)malloc(sizeof(btimer_t));

  wid->wd = NewWatchdog();
  if (wid->wd == NULL) {
    free(wid);
    return NULL;
  }
  wid->wd->data = wid;
  wid->killed = false;

  return wid;
}

static void StopBtimer(btimer_t* wid)
{
  if (wid == NULL) {
    Emsg0(M_INFO, 0, T_("StopBtimer called with NULL btimer_id\n"));
    return;
  }
  if (wid->wd) {
    UnregisterWatchdog(wid->wd);
    free(wid->wd);
  }
  free(wid);
}
