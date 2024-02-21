/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2003-2012 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2024 Bareos GmbH & Co. KG

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
// Kern Sibbald, May MMIII
/**
 * @file
 * Bareos File Daemon heartbeat routines
 * Listens for heartbeats coming from the SD
 * If configured, sends heartbeats to Dir
 */

#include "include/bareos.h"
#include "filed/filed.h"
#include "filed/filed_jcr_impl.h"
#include "filed/filed_globals.h"
#include "lib/bnet.h"
#include "lib/bsock.h"
#include "lib/watchdog.h"
#include "filed/heartbeat.h"

#include <functional>

namespace filedaemon {

#define WAIT_INTERVAL 5

extern bool no_signals;

heartbeat_dir::heartbeat_dir(BareosSocket* sock, time_t interval)
    : thread{std::mem_fn(&heartbeat_dir::send_heartbeat), this, sock, interval}
{
}

void heartbeat_dir::send_heartbeat(BareosSocket* sock, time_t interval)
{
  sock->suppress_error_msgs_ = true;
  time_t last_heartbeat = time(NULL);

  while (!sock->IsStop() && !stop_requested) {
    time_t now = time(NULL);
    time_t since_last = now - last_heartbeat;
    if (since_last >= interval) {
      sock->signal(BNET_HEARTBEAT);
      if (sock->IsStop() || stop_requested) { break; }
      last_heartbeat = now;
      Bmicrosleep(interval, 0);
    } else {
      Bmicrosleep(interval - since_last, 0);
    }
  }
  sock->close();

  delete sock;
}

heartbeat_dir::~heartbeat_dir()
{
  stop_requested = true;
  pthread_kill(thread.native_handle(), TIMEOUT_SIGNAL);
  thread.join();
}

heartbeat_sd_dir::heartbeat_sd_dir(BareosSocket* sd,
                                   BareosSocket* dir,
                                   time_t interval)
    : thread{std::mem_fn(&heartbeat_sd_dir::send_heartbeat), this, sd, dir,
             interval}
{
}

void heartbeat_sd_dir::send_heartbeat(BareosSocket* sd,
                                      BareosSocket* dir,
                                      time_t interval)
{
  dir->suppress_error_msgs_ = true;
  sd->suppress_error_msgs_ = true;

  time_t last_heartbeat = time(NULL);

  /* Hang reading the socket to the SD, and every time we get
   * a heartbeat or we get a wait timeout (5 seconds), we
   * check to see if we need to send a heartbeat to the
   * Director.
   */
  while (!sd->IsStop() && !stop_requested) {
    std::int32_t n = BnetWaitDataIntr(sd, WAIT_INTERVAL);
    if (n < 0 || sd->IsStop() || stop_requested) { break; }
    if (interval) {
      time_t now = time(NULL);
      if (now - last_heartbeat >= interval) {
        dir->signal(BNET_HEARTBEAT);
        if (dir->IsStop() || stop_requested) { break; }
        last_heartbeat = now;
      }
    }
    if (n == 1) { /* input waiting */
      sd->recv(); /* read it -- probably heartbeat from sd */
      if (sd->IsStop() || stop_requested) { break; }
      if (sd->message_length <= 0) {
        Dmsg1(100, "Got BNET_SIG %d from SD\n", sd->message_length);
      } else {
        Dmsg2(100, "Got %d bytes from SD. MSG=%s\n", sd->message_length,
              sd->msg);
      }
    }
    Dmsg2(200, "wait_intr=%d stop=%d\n", n, IsBnetStop(sd));
  }

  sd->close();
  dir->close();

  delete sd;
  delete dir;
}

heartbeat_sd_dir::~heartbeat_sd_dir()
{
  stop_requested = true;
  pthread_kill(thread.native_handle(), TIMEOUT_SIGNAL);
  thread.join();
}

std::optional<heartbeat_sd_dir> MakeHeartbeatMonitor(JobControlRecord* jcr)
{
  if (no_signals) { return std::nullopt; }

  // both the heartbeat thread and normal jmsgs will want to write to the
  // dir_bsock socket.  To prevent data races we have to enable locking on that
  // socket otherwise it will lead to bad problems.
  jcr->dir_bsock->SetLocking();

  return std::optional<heartbeat_sd_dir>{
      std::in_place, jcr->store_bsock->clone(), jcr->dir_bsock->clone(),
      me->heartbeat_interval};
}

std::optional<heartbeat_dir> MakeDirHeartbeat(JobControlRecord* jcr)
{
  auto interval = me->heartbeat_interval;
  if (no_signals || !interval) { return std::nullopt; }

  // both the heartbeat thread and normal jmsgs will want to write to the
  // dir_bsock socket.  To prevent data races we have to enable locking on that
  // socket otherwise it will lead to bad problems.
  jcr->dir_bsock->SetLocking();

  return std::optional<heartbeat_dir>{std::in_place, jcr->dir_bsock->clone(),
                                      interval};
}

} /* namespace filedaemon */
