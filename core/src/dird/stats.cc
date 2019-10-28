/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2016-2016 Planets Communications B.V.
   Copyright (C) 2014-2019 Bareos GmbH & Co. KG

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
 * Written by Marco van Wieringen and Philipp Storz, April 2014
 */
/**
 * @file
 * Statistics collector thread.
 */

#include "include/bareos.h"
#include "dird.h"
#include "dird/dird_globals.h"
#include "cats/sql_pooling.h"
#include "dird/sd_cmds.h"
#include "dird/ua_server.h"
#include "lib/bnet.h"
#include "lib/parse_conf.h"
#include "lib/util.h"

namespace directordaemon {

/*
 * Commands received from storage daemon that need scanning
 */
static char DevStats[] =
    "Devicestats [%lld]: Device=%s Read=%llu, Write=%llu, SpoolSize=%llu, "
    "NumWaiting=%lu, NumWriters=%lu, "
    "ReadTime=%lld, WriteTime=%lld, MediaId=%ld, VolBytes=%llu, VolFiles=%llu, "
    "VolBlocks=%llu";
static char TapeAlerts[] = "Tapealerts [%lld]: Device=%s TapeAlert=%llu";
static char JobStats[] =
    "Jobstats [%lld]: JobId=%ld, JobFiles=%lu, JobBytes=%llu, DevName=%s";

static bool quit = false;
static bool statistics_initialized = false;
static bool need_flush = true;
static pthread_t statistics_tid;
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t wait_for_next_run_cond = PTHREAD_COND_INITIALIZER;

/*
 * Cache the last lookup of a DeviceId.
 */
static struct cached_device {
  char device_name[MAX_NAME_LENGTH];
  DBId_t StorageId;
  DBId_t DeviceId;
} cached_device;

static inline bool LookupDevice(JobControlRecord* jcr,
                                const char* device_name,
                                DBId_t StorageId,
                                DBId_t* DeviceId)
{
  DeviceDbRecord dr;

  if (cached_device.StorageId == StorageId &&
      bstrcmp(cached_device.device_name, device_name)) {
    *DeviceId = cached_device.DeviceId;
    return true;
  }

  dr.StorageId = StorageId;
  bstrncpy(dr.Name, device_name, sizeof(dr.Name));
  if (!jcr->db->CreateDeviceRecord(jcr, &dr)) {
    Dmsg0(100, "Failed to create new Device record\n");
    goto bail_out;
  }

  Dmsg3(200, "Deviceid of \"%s\" on StorageId %d is %d\n", dr.Name,
        dr.StorageId, dr.DeviceId);

  bstrncpy(cached_device.device_name, device_name,
           sizeof(cached_device.device_name));
  cached_device.StorageId = StorageId;
  cached_device.DeviceId = dr.DeviceId;

  *DeviceId = dr.DeviceId;
  return true;

bail_out:
  return false;
}

static inline void wait_for_next_run()
{
  struct timeval tv;
  struct timezone tz;
  struct timespec timeout;

  gettimeofday(&tv, &tz);
  timeout.tv_nsec = tv.tv_usec * 1000;
  timeout.tv_sec = tv.tv_sec + me->stats_collect_interval;

  P(mutex);
  pthread_cond_timedwait(&wait_for_next_run_cond, &mutex, &timeout);
  V(mutex);
}

extern "C" void* statistics_thread(void* arg)
{
  JobControlRecord* jcr;
  utime_t now;
  PoolMem current_store(PM_NAME);

  Dmsg0(200, "Starting statistics thread\n");

  memset(&cached_device, 0, sizeof(struct cached_device));
  PmStrcpy(current_store, "");

  jcr = new_control_jcr("*StatisticsCollector*", JT_SYSTEM);

  jcr->res.catalog = (CatalogResource*)my_config->GetNextRes(R_CATALOG, NULL);
  jcr->db = DbSqlGetPooledConnection(
      jcr, jcr->res.catalog->db_driver, jcr->res.catalog->db_name,
      jcr->res.catalog->db_user, jcr->res.catalog->db_password.value,
      jcr->res.catalog->db_address, jcr->res.catalog->db_port,
      jcr->res.catalog->db_socket, jcr->res.catalog->mult_db_connections,
      jcr->res.catalog->disable_batch_insert, jcr->res.catalog->try_reconnect,
      jcr->res.catalog->exit_on_fatal);
  if (jcr->db == NULL) {
    Jmsg(jcr, M_FATAL, 0, _("Could not open database \"%s\".\n"),
         jcr->res.catalog->db_name);
    goto bail_out;
  }

  while (!quit) {
    now = (utime_t)time(NULL);

    Dmsg1(200, "statistics_thread: Doing work at %ld\n", now);

    if (JobCount() == 0) {
      if (!need_flush) {
        Dmsg0(200, "statistics_thread: do nothing as no jobs are running\n");
        wait_for_next_run();
        continue;
      } else {
        Dmsg0(200, "statistics_thread_runner: flushing pending statistics\n");
        need_flush = false;
      }
    } else {
      need_flush = true;
    }

    while (1) {
      BareosSocket* sd;
      StorageResource* store;
      int64_t StorageId;

      LockRes(my_config);
      if ((current_store.c_str())[0]) {
        store = (StorageResource*)my_config->GetResWithName(
            R_STORAGE, current_store.c_str());
      } else {
        store = NULL;
      }

      store = (StorageResource*)my_config->GetNextRes(R_STORAGE,
                                                      (BareosResource*)store);
      if (!store) {
        PmStrcpy(current_store, "");
        UnlockRes(my_config);
        break;
      }

      PmStrcpy(current_store, store->resource_name_);
      if (!store->collectstats) {
        UnlockRes(my_config);
        continue;
      }

      switch (store->Protocol) {
        case APT_NATIVE:
          break;
        default:
          UnlockRes(my_config);
          continue;
      }

      jcr->res.read_storage = store;
      if (!ConnectToStorageDaemon(jcr, 2, 1, false)) {
        UnlockRes(my_config);
        continue;
      }

      StorageId = store->StorageId;
      sd = jcr->store_bsock;

      UnlockRes(my_config);

      /*
       * Do our work retrieving the statistics from the remote SD.
       */
      if (sd->fsend("stats")) {
        while (BnetRecv(sd) >= 0) {
          Dmsg1(200, "<stored: %s", sd->msg);
          if (bstrncmp(sd->msg, "Devicestats", 10)) {
            PoolMem DevName(PM_NAME);
            DeviceStatisticsDbRecord dsr;

            if (sscanf(sd->msg, DevStats, &dsr.SampleTime, DevName.c_str(),
                       &dsr.ReadBytes, &dsr.WriteBytes, &dsr.SpoolSize,
                       &dsr.NumWaiting, &dsr.NumWriters, &dsr.ReadTime,
                       &dsr.WriteTime, &dsr.MediaId, &dsr.VolCatBytes,
                       &dsr.VolCatFiles, &dsr.VolCatBlocks) == 13) {
              Dmsg5(200,
                    "New Devstats [%lld]: Device=%s Read=%llu, Write=%llu, "
                    "SpoolSize=%llu,\n",
                    dsr.SampleTime, DevName.c_str(), dsr.ReadBytes,
                    dsr.WriteBytes, dsr.SpoolSize);
              Dmsg4(200,
                    "NumWaiting=%lu, NumWriters=%lu, ReadTime=%lld, "
                    "WriteTime=%lld,\n",
                    dsr.NumWaiting, dsr.NumWriters, dsr.ReadTime,
                    dsr.WriteTime);
              Dmsg4(
                  200,
                  "MediaId=%ld, VolBytes=%llu, VolFiles=%llu, VolBlocks=%llu\n",
                  dsr.MediaId, dsr.VolCatBytes, dsr.VolCatFiles,
                  dsr.VolCatBlocks);

              if (!LookupDevice(jcr, DevName.c_str(), StorageId,
                                &dsr.DeviceId)) {
                continue;
              }

              if (!jcr->db->CreateDeviceStatistics(jcr, &dsr)) { continue; }
            } else {
              Jmsg1(jcr, M_ERROR, 0, _("Malformed message: %s\n"), sd->msg);
            }
          } else if (bstrncmp(sd->msg, "Tapealerts", 10)) {
            PoolMem DevName(PM_NAME);
            TapealertStatsDbRecord tsr;

            if (sscanf(sd->msg, TapeAlerts, &tsr.SampleTime, DevName.c_str(),
                       &tsr.AlertFlags) == 3) {
              UnbashSpaces(DevName);

              Dmsg3(200, "New stats [%lld]: Device %s TapeAlert %llu\n",
                    tsr.SampleTime, DevName.c_str(), tsr.AlertFlags);

              if (!LookupDevice(jcr, DevName.c_str(), StorageId,
                                &tsr.DeviceId)) {
                continue;
              }

              if (!jcr->db->CreateTapealertStatistics(jcr, &tsr)) { continue; }
            } else {
              Jmsg1(jcr, M_ERROR, 0, _("Malformed message: %s\n"), sd->msg);
            }
          } else if (bstrncmp(sd->msg, "Jobstats", 8)) {
            PoolMem DevName(PM_NAME);
            JobStatisticsDbRecord jsr;

            if (sscanf(sd->msg, JobStats, &jsr.SampleTime, &jsr.JobId,
                       &jsr.JobFiles, &jsr.JobBytes, DevName.c_str()) == 5) {
              UnbashSpaces(DevName);

              Dmsg5(200,
                    "New Jobstats [%lld]: JobId %ld, JobFiles %lu, JobBytes "
                    "%llu, DevName %s\n",
                    jsr.SampleTime, jsr.JobId, jsr.JobFiles, jsr.JobBytes,
                    DevName.c_str());

              if (!LookupDevice(jcr, DevName.c_str(), StorageId,
                                &jsr.DeviceId)) {
                continue;
              }

              if (!jcr->db->CreateJobStatistics(jcr, &jsr)) { continue; }
            } else {
              Jmsg1(jcr, M_ERROR, 0, _("Malformed message: %s\n"), sd->msg);
            }
          }
        }
      }

      jcr->store_bsock->close();
      delete jcr->store_bsock;
      jcr->store_bsock = NULL;

    }  // while (1)

    wait_for_next_run();

  }  // while(!quit)

bail_out:
  FreeJcr(jcr);

  Dmsg0(200, "Finished statistics thread\n");

  return NULL;
}

int StartStatisticsThread(void)
{
  int status;

  if (!me->stats_collect_interval) { return 0; }

  quit = false;

  if ((status = pthread_create(&statistics_tid, NULL, statistics_thread,
                               NULL)) != 0) {
    return status;
  }

  statistics_initialized = true;

  return 0;
}

void StopStatisticsThread()
{
  if (!statistics_initialized) { return; }

  quit = true;
  pthread_cond_broadcast(&wait_for_next_run_cond);
  if (!pthread_equal(statistics_tid, pthread_self())) {
    pthread_join(statistics_tid, NULL);
  }
}

void stats_job_started()
{
  if (statistics_initialized) { need_flush = true; }
}
} /* namespace directordaemon */
