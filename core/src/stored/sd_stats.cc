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
/**
 * @file
 * Periodic Storage Daemon runtime snapshot sender.
 */

#include "include/bareos.h"
#include "stored/stored.h"
#include "stored/stored_globals.h"
#include "stored/sd_stats.h"
#include "stored/status.h"

#include "lib/berrno.h"
#include "lib/bsock.h"
#include "lib/edit.h"
#include "lib/util.h"

namespace {
constexpr uint32_t kDefaultStatisticsCollectInterval = 60;
inline constexpr const char RuntimeSnapshot[] = "RunTime Job=%s %s\n";
}  // namespace

namespace storagedaemon {

static bool quit = false;
static bool statistics_initialized = false;
static pthread_t statistics_tid;
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t wait_for_next_run = PTHREAD_COND_INITIALIZER;

static uint32_t GetStatisticsCollectInterval()
{
  if (!me || me->stats_collect_interval == 0) {
    return kDefaultStatisticsCollectInterval;
  }
  return me->stats_collect_interval;
}

static bool ShouldSendRuntimeSnapshot(JobControlRecord* jcr)
{
  return jcr && jcr->JobId != 0 && jcr->dir_bsock;
}

static void SendRuntimeSnapshot(JobControlRecord* jcr)
{
  if (!ShouldSendRuntimeSnapshot(jcr)) { return; }

  PoolMem runtime_record(PM_MESSAGE);
  FormatRunningJobRuntimeRecord(runtime_record, jcr);

  PoolMem job_name(PM_NAME);
  PmStrcpy(job_name, jcr->Job);
  BashSpaces(job_name);

  jcr->dir_bsock->fsend(RuntimeSnapshot, job_name.c_str(),
                        runtime_record.c_str());
}

extern "C" void* statistics_thread_runner(void*)
{
  struct timeval tv;
  struct timespec timeout;
  JobControlRecord* jcr;

  while (!quit) {
    foreach_jcr (jcr) { SendRuntimeSnapshot(jcr); }
    endeach_jcr(jcr);

    gettimeofday(&tv, NULL);
    timeout.tv_nsec = tv.tv_usec * 1000;
    timeout.tv_sec = tv.tv_sec + GetStatisticsCollectInterval();

    lock_mutex(mutex);
    pthread_cond_timedwait(&wait_for_next_run, &mutex, &timeout);
    unlock_mutex(mutex);
  }

  return NULL;
}

bool StartStatisticsThread(void)
{
  int status;

  if (!me) { return false; }

  quit = false;

  if ((status
       = pthread_create(&statistics_tid, NULL, statistics_thread_runner, NULL))
      != 0) {
    BErrNo be;
    Emsg1(M_ERROR_TERM, 0,
          T_("Storage runtime snapshot thread could not be started. ERR=%s\n"),
          be.bstrerror());
  }

  statistics_initialized = true;

  return true;
}

void StopStatisticsThread()
{
  if (!statistics_initialized) { return; }

  quit = true;
  pthread_cond_broadcast(&wait_for_next_run);
  if (!pthread_equal(statistics_tid, pthread_self())) {
    pthread_join(statistics_tid, NULL);
  }
}

} /* namespace storagedaemon */
