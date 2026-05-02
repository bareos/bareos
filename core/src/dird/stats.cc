/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2014-2026 Bareos GmbH & Co. KG

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

#include "include/bareos.h"
#include "dird.h"

#include "dird/dird_globals.h"
#include "dird/director_jcr_impl.h"
#include "dird/get_database_connection.h"
#include "dird/statistics_time_series.h"
#include "dird/stats.h"
#include "dird/ua_server.h"

namespace directordaemon {

namespace {

bool quit = false;
bool statistics_thread_started = false;
pthread_t statistics_tid;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t wait_for_next_run_cond = PTHREAD_COND_INITIALIZER;

void WaitForNextRun()
{
  struct timeval tv;
  struct timespec timeout;

  gettimeofday(&tv, nullptr);
  timeout.tv_nsec = tv.tv_usec * 1000;
  timeout.tv_sec
      = tv.tv_sec
        + GetStatisticsTimeSeriesStepSeconds(me->stats_collect_interval);

  lock_mutex(mutex);
  pthread_cond_timedwait(&wait_for_next_run_cond, &mutex, &timeout);
  unlock_mutex(mutex);
}

extern "C" void* statistics_thread_runner(void*)
{
  JobControlRecord* jcr = new_control_jcr("*StatisticsTimeSeries*", JT_SYSTEM);

  {
    ResLocker _{my_config};
    jcr->dir_impl->res.catalog
        = (CatalogResource*)my_config->GetNextRes(R_CATALOG, nullptr);
  }

  if (!jcr->dir_impl->res.catalog) {
    FlushStatisticsTimeSeries(nullptr);
    FreeJcr(jcr);
    return nullptr;
  }

  jcr->db = GetDatabaseConnection(jcr);
  if (!jcr->db) {
    Jmsg(jcr, M_FATAL, 0, T_("Could not open database \"%s\".\n"),
         jcr->dir_impl->res.catalog->db_name);
    FlushStatisticsTimeSeries(jcr);
    FreeJcr(jcr);
    return nullptr;
  }

  while (!quit) {
    SampleCatalogStatisticsTimeSeries(jcr);
    WaitForNextRun();
  }

  FlushStatisticsTimeSeries(jcr);
  FreeJcr(jcr);
  return nullptr;
}

}  // namespace

bool StartStatisticsThread(void)
{
  if (statistics_thread_started || !me || !me->statistics_time_series) {
    return false;
  }

  quit = false;
  int status = pthread_create(&statistics_tid, nullptr,
                              statistics_thread_runner, nullptr);
  if (status != 0) { return false; }

  statistics_thread_started = true;
  return true;
}

void StopStatisticsThread()
{
  if (!statistics_thread_started) {
    FlushStatisticsTimeSeries(nullptr);
    return;
  }

  lock_mutex(mutex);
  quit = true;
  pthread_cond_signal(&wait_for_next_run_cond);
  unlock_mutex(mutex);

  pthread_join(statistics_tid, nullptr);
  statistics_thread_started = false;
}

void stats_job_started() {}

} /* namespace directordaemon */
