/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2013-2026 Bareos GmbH & Co. KG

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
// Marco van Wieringen, March 2013
/**
 * @file
 * This file handles accepting Storage Daemon Commands
 */

#include "include/bareos.h"
#include "filed/filed.h"
#include "filed/filed_globals.h"
#include "filed/filed_jcr_impl.h"
#include "filed/authenticate.h"
#include "lib/bnet.h"
#include "lib/bsock.h"
#include "include/version_hex.h"

namespace filedaemon {

static int debuglevel = 100;

void* handle_stored_connection(BareosSocket* sd, JobControlRecord* jcr)
{
  jcr->store_bsock = sd;
  jcr->store_bsock->SetJcr(jcr);

  if (!jcr->max_bandwidth) {
    if (jcr->fd_impl->director->max_bandwidth_per_job) {
      jcr->max_bandwidth = jcr->fd_impl->director->max_bandwidth_per_job;
    } else if (me->max_bandwidth_per_job) {
      jcr->max_bandwidth = me->max_bandwidth_per_job;
    }
  }

  sd->SetBwlimit(jcr->max_bandwidth);
  if (me->allow_bw_bursting) { sd->SetBwlimitBursting(); }
  Dmsg0(debuglevel, "setting bandwidth to %" PRId64 "%s\n", jcr->max_bandwidth,
        me->allow_bw_bursting ? " (with bursting)" : "");

  FreeJcr(jcr);

  return NULL;
}
} /* namespace filedaemon */
