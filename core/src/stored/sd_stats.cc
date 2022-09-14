/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2013-2022 Bareos GmbH & Co. KG

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
 * Storage Daemon statistics gatherer.
 *
 * Written by Marco van Wieringen and Philipp Storz, November 2013
 */

#include "include/bareos.h"
#include "stored/stored.h"
#include "stored/stored_globals.h"
#include "stored/device_control_record.h"
#include "stored/jcr_private.h"
#include "lib/util.h"
#include "include/jcr.h"
#include "lib/parse_conf.h"
#include "lib/bsock.h"
#include "lib/berrno.h"

namespace storagedaemon {

static char OKstats[] = "2000 OK statistics\n";
static char DevStats[]
    = "Devicestats [%lld]: Device=%s Read=%llu, Write=%llu, SpoolSize=%llu, "
      "NumWaiting=%ld, NumWriters=%ld, "
      "ReadTime=%lld, WriteTime=%lld, MediaId=%ld, VolBytes=%llu, "
      "VolFiles=%llu, "
      "VolBlocks=%llu\n";
static char TapeAlerts[] = "Tapealerts [%lld]: Device=%s TapeAlert=%llu\n";
static char JobStats[]
    = "Jobstats [%lld]: JobId=%ld, JobFiles=%lu, JobBytes=%llu, DevName=%s\n";

/* Static globals */
static bool quit = false;
static bool statistics_initialized = false;
static pthread_t statistics_tid;
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t wait_for_next_run = PTHREAD_COND_INITIALIZER;

struct device_statistic_item {
  dlink<device_statistic_item> link;
  bool collected{false};
  utime_t timestamp{0};
  btime_t DevReadTime{0};
  btime_t DevWriteTime{0};
  uint64_t DevWriteBytes{0};
  uint64_t DevReadBytes{0};
  uint64_t spool_size{0};
  int num_waiting{0};
  int num_writers{0};
  DBId_t MediaId{0};
  uint64_t VolCatBytes{0};
  uint64_t VolCatFiles{0};
  uint64_t VolCatBlocks{0};
};

struct device_tapealert_item {
  dlink<device_tapealert_item> link;
  utime_t timestamp{0};
  uint64_t flags{0};
};

struct device_statistics_t {
  dlink<device_statistics_t> link;
  char DevName[MAX_NAME_LENGTH]{};
  struct device_statistic_item* cached{nullptr};
  dlist<device_statistic_item>* device_statistics{nullptr};
  dlist<device_tapealert_item>* device_tapealerts{nullptr};
};

struct jobstatistic_item {
  dlink<jobstatistic_item> link;
  bool collected{false};
  utime_t timestamp{0};
  uint32_t JobFiles{0};
  uint64_t JobBytes{0};
  char* DevName{nullptr};
};

struct job_statistics_t {
  dlink<job_statistics_t> link;
  uint32_t JobId{0};
  struct jobstatistic_item* cached{nullptr};
  dlist<jobstatistic_item>* job_statistics{nullptr};
};

static dlist<device_statistics_t>* device_statistics = NULL;
static dlist<job_statistics_t>* job_statistics = NULL;

static inline void setup_statistics()
{
  device_statistics = new dlist<device_statistics_t>();
  job_statistics = new dlist<job_statistics_t>();
}

void UpdateDeviceTapealert(const char* devname, uint64_t flags, utime_t now)
{
  bool found = false;
  device_statistics_t* dev_stats = NULL;
  struct device_tapealert_item* tape_alert = NULL;

  if (!me || !me->collect_dev_stats || !device_statistics) { return; }

  foreach_dlist (dev_stats, device_statistics) {
    if (bstrcmp(dev_stats->DevName, devname)) {
      found = true;
      break;
    }
  }

  if (!found) {
    dev_stats = (device_statistics_t*)malloc(sizeof(device_statistics_t));
    device_statistics_t empty_device_statistics;
    *dev_stats = empty_device_statistics;

    bstrncpy(dev_stats->DevName, devname, sizeof(dev_stats->DevName));
    lock_mutex(mutex);
    device_statistics->append(dev_stats);
    unlock_mutex(mutex);
  }

  // Add a new tapealert message.
  tape_alert = (struct device_tapealert_item*)malloc(
      sizeof(struct device_tapealert_item));
  struct device_tapealert_item empty_device_tapealert;
  *tape_alert = empty_device_tapealert;

  tape_alert->timestamp = now;
  tape_alert->flags = flags;

  if (!dev_stats->device_tapealerts) {
    dev_stats->device_tapealerts = new dlist<device_tapealert_item>();
  }

  lock_mutex(mutex);
  dev_stats->device_tapealerts->append(tape_alert);
  unlock_mutex(mutex);

  Dmsg3(200, "New stats [%lld]: Device %s TapeAlert %llu\n",
        tape_alert->timestamp, dev_stats->DevName, tape_alert->flags);
}

static inline void UpdateDeviceStatistics(const char* devname,
                                          Device* dev,
                                          utime_t now)
{
  bool found = false;
  device_statistics_t* dev_stats = NULL;
  struct device_statistic_item* dev_stat = NULL;

  if (!me || !me->collect_dev_stats || !device_statistics) { return; }

  // See if we already have statistics for this device.
  foreach_dlist (dev_stats, device_statistics) {
    if (bstrcmp(dev_stats->DevName, devname)) {
      found = true;
      break;
    }
  }

  /*
   * If we have statistics and the cached entry is filled it points
   * to the latest sampled statistics so we compare them with the current
   * statistics and if nothing changed we just return.
   */
  if (found && dev_stats->cached) {
    dev_stat = dev_stats->cached;

    if (dev_stat->DevReadBytes == dev->DevReadBytes
        && dev_stat->DevWriteBytes == dev->DevWriteBytes
        && dev_stat->spool_size == dev->spool_size) {
      return;
    }
  } else if (!found) {
    dev_stats = (device_statistics_t*)malloc(sizeof(device_statistics_t));
    device_statistics_t empty_device_statistics;
    *dev_stats = empty_device_statistics;

    bstrncpy(dev_stats->DevName, devname, sizeof(dev_stats->DevName));
    lock_mutex(mutex);
    device_statistics->append(dev_stats);
    unlock_mutex(mutex);
  }

  // Add a new set of statistics.
  dev_stat = (struct device_statistic_item*)malloc(
      sizeof(struct device_statistic_item));

  struct device_statistic_item empty_device_statistic;
  *dev_stat = empty_device_statistic;

  dev_stat->timestamp = now;
  dev_stat->DevReadTime = dev->DevReadTime;
  dev_stat->DevWriteTime = dev->DevWriteTime;
  dev_stat->DevWriteBytes = dev->DevWriteBytes;
  dev_stat->DevReadBytes = dev->DevReadBytes;
  dev_stat->spool_size = dev->spool_size;
  dev_stat->num_waiting = dev->num_waiting;
  dev_stat->num_writers = dev->num_writers;
  dev_stat->MediaId = dev->VolCatInfo.VolMediaId;
  dev_stat->VolCatBytes = dev->VolCatInfo.VolCatBytes;
  dev_stat->VolCatFiles = dev->VolCatInfo.VolCatFiles;
  dev_stat->VolCatBlocks = dev->VolCatInfo.VolCatBlocks;


  if (!dev_stats->device_statistics) {
    dev_stats->device_statistics = new dlist<device_statistic_item>();
  }

  lock_mutex(mutex);
  dev_stats->cached = dev_stat;
  dev_stats->device_statistics->append(dev_stat);
  unlock_mutex(mutex);

  Dmsg5(200,
        "New stats [%lld]: Device %s Read %llu, Write %llu, Spoolsize %llu,\n",
        dev_stat->timestamp, dev_stats->DevName, dev_stat->DevReadBytes,
        dev_stat->DevWriteBytes, dev_stat->spool_size);
  Dmsg4(200, "NumWaiting %ld, NumWriters %ld, ReadTime=%lld, WriteTime=%lld,\n",
        dev_stat->num_waiting, dev_stat->num_writers, dev_stat->DevReadTime,
        dev_stat->DevWriteTime);
  Dmsg4(200, "MediaId=%ld VolBytes=%llu, VolFiles=%llu, VolBlocks=%llu\n",
        dev_stat->MediaId, dev_stat->VolCatBytes, dev_stat->VolCatFiles,
        dev_stat->VolCatBlocks);
}

void UpdateJobStatistics(JobControlRecord* jcr, utime_t now)
{
  bool found = false;
  job_statistics_t* job_stats = NULL;
  jobstatistic_item* job_stat = NULL;

  if (!me || !me->collect_job_stats || !job_statistics) { return; }

  // Skip job 0 info
  if (!jcr->JobId) { return; }

  // See if we already have statistics for this job.
  foreach_dlist (job_stats, job_statistics) {
    if (job_stats->JobId == jcr->JobId) {
      found = true;
      break;
    }
  }

  /*
   * If we have statistics and the cached entry is filled it points
   * to the latest sampled statistics so we compare them with the current
   * statistics and if nothing changed we just return.
   */
  if (found && job_stats->cached) {
    job_stat = job_stats->cached;

    if (job_stat->JobFiles == jcr->JobFiles
        && job_stat->JobBytes == jcr->JobBytes) {
      return;
    }
  } else if (!found) {
    job_stats = (job_statistics_t*)malloc(sizeof(job_statistics_t));
    job_statistics_t empty_job_statistics;
    *job_stats = empty_job_statistics;

    job_stats->JobId = jcr->JobId;
    lock_mutex(mutex);
    job_statistics->append(job_stats);
    unlock_mutex(mutex);
  }

  // Add a new set of statistics.
  job_stat
      = (struct jobstatistic_item*)malloc(sizeof(struct jobstatistic_item));
  struct jobstatistic_item empty_job_statistic;
  *job_stat = empty_job_statistic;

  job_stat->timestamp = now;
  job_stat->JobFiles = jcr->JobFiles;
  job_stat->JobBytes = jcr->JobBytes;
  if (jcr->impl->dcr && jcr->impl->dcr->device_resource) {
    job_stat->DevName = strdup(jcr->impl->dcr->device_resource->resource_name_);
  } else {
    job_stat->DevName = strdup("unknown");
  }

  if (!job_stats->job_statistics) {
    job_stats->job_statistics = new dlist<jobstatistic_item>();
  }

  lock_mutex(mutex);
  job_stats->cached = job_stat;
  job_stats->job_statistics->append(job_stat);
  unlock_mutex(mutex);

  Dmsg5(
      200,
      "New stats [%lld]: JobId %ld, JobFiles %lu, JobBytes %llu, DevName %s\n",
      job_stat->timestamp, job_stats->JobId, job_stat->JobFiles,
      job_stat->JobBytes, job_stat->DevName);
}

static inline void cleanup_cached_statistics()
{
  device_statistics_t* dev_stats;
  job_statistics_t* job_stats;

  lock_mutex(mutex);
  if (device_statistics) {
    foreach_dlist (dev_stats, device_statistics) {
      dev_stats->device_statistics->destroy();
      dev_stats->device_statistics = NULL;
    }

    device_statistics->destroy();
    device_statistics = NULL;
  }

  if (job_statistics) {
    foreach_dlist (job_stats, job_statistics) {
      job_stats->job_statistics->destroy();
      job_stats->job_statistics = NULL;
    }

    job_statistics->destroy();
    job_statistics = NULL;
  }
  unlock_mutex(mutex);
}

// Entry point for a separate statistics thread.
extern "C" void* statistics_thread_runner([[maybe_unused]] void* arg)
{
  utime_t now;
  struct timeval tv;
  struct timezone tz;
  struct timespec timeout;
  DeviceResource* device_resource = nullptr;
  JobControlRecord* jcr;

  setup_statistics();

  // Do our work as long as we are not signaled to quit.
  while (!quit) {
    now = (utime_t)time(NULL);

    if (me->collect_dev_stats) {
      // Loop over all defined devices.
      foreach_res (device_resource, R_DEVICE) {
        if (device_resource->collectstats) {
          Device* dev;

          dev = device_resource->dev;
          if (dev && dev->initiated) {
            UpdateDeviceStatistics(device_resource->resource_name_, dev, now);
          }
        }
      }
    }

    if (me->collect_job_stats) {
      // Loop over all running Jobs in the Storage Daemon.
      foreach_jcr (jcr) {
        UpdateJobStatistics(jcr, now);
      }
      endeach_jcr(jcr);
    }

    /*
     * Wait for a next run. Normally this waits exactly
     * me->stats_collect_interval seconds. It can be interrupted when signaled
     * by the StopStatisticsThread() function.
     */
    gettimeofday(&tv, &tz);
    timeout.tv_nsec = tv.tv_usec * 1000;
    timeout.tv_sec = tv.tv_sec + me->stats_collect_interval;

    lock_mutex(mutex);
    pthread_cond_timedwait(&wait_for_next_run, &mutex, &timeout);
    unlock_mutex(mutex);
  }

  // Cleanup the cached statistics.
  cleanup_cached_statistics();

  return NULL;
}

bool StartStatisticsThread(void)
{
  int status;

  // First see if device and job stats collection is enabled.
  if (!me->stats_collect_interval
      || (!me->collect_dev_stats && !me->collect_job_stats)) {
    return false;
  }

  /*
   * See if only device stats collection is enabled that there is a least
   * one device of which stats are collected.
   */
  if (me->collect_dev_stats && !me->collect_job_stats) {
    DeviceResource* device_resource = nullptr;
    int cnt = 0;

    foreach_res (device_resource, R_DEVICE) {
      if (device_resource->collectstats) { cnt++; }
    }

    if (cnt == 0) { return false; }
  }

  if ((status
       = pthread_create(&statistics_tid, NULL, statistics_thread_runner, NULL))
      != 0) {
    BErrNo be;
    Emsg1(M_ERROR_TERM, 0,
          _("Director Statistics Thread could not be started. ERR=%s\n"),
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

bool StatsCmd(JobControlRecord* jcr)
{
  BareosSocket* dir = jcr->dir_bsock;
  PoolMem msg(PM_MESSAGE);
  PoolMem dev_tmp(PM_MESSAGE);

  if (device_statistics) {
    device_statistics_t* dev_stats;

    foreach_dlist (dev_stats, device_statistics) {
      if (dev_stats->device_statistics) {
        struct device_statistic_item *dev_stat, *next_dev_stat;

        dev_stat = dev_stats->device_statistics->first();
        while (dev_stat) {
          next_dev_stat = dev_stats->device_statistics->next(dev_stat);

          // If the entry was already collected no need to do it again.
          if (!dev_stat->collected) {
            PmStrcpy(dev_tmp, dev_stats->DevName);
            BashSpaces(dev_tmp);
            Mmsg(msg, DevStats, dev_stat->timestamp, dev_tmp.c_str(),
                 dev_stat->DevReadBytes, dev_stat->DevWriteBytes,
                 dev_stat->spool_size, dev_stat->num_waiting,
                 dev_stat->num_writers, dev_stat->DevReadTime,
                 dev_stat->DevWriteTime, dev_stat->MediaId,
                 dev_stat->VolCatBytes, dev_stat->VolCatFiles,
                 dev_stat->VolCatBlocks);
            Dmsg1(100, ">dird: %s", msg.c_str());
            dir->fsend(msg.c_str());
          }

          lock_mutex(mutex);
          // If this is the last one on the list leave it for comparison.
          if (!next_dev_stat) {
            dev_stat->collected = true;
          } else {
            dev_stats->device_statistics->remove(dev_stat);

            if (dev_stats->cached == dev_stat) { dev_stats->cached = NULL; }
          }
          unlock_mutex(mutex);
          dev_stat = next_dev_stat;
        }
      }

      if (dev_stats->device_tapealerts) {
        struct device_tapealert_item *tape_alert, *next_tape_alert;

        tape_alert = dev_stats->device_tapealerts->first();
        while (tape_alert) {
          PmStrcpy(dev_tmp, dev_stats->DevName);
          BashSpaces(dev_tmp);
          Mmsg(msg, TapeAlerts, tape_alert->timestamp, dev_tmp.c_str(),
               tape_alert->flags);
          Dmsg1(100, ">dird: %s", msg.c_str());
          dir->fsend(msg.c_str());

          next_tape_alert = dev_stats->device_tapealerts->next(tape_alert);
          lock_mutex(mutex);
          dev_stats->device_tapealerts->remove(tape_alert);
          unlock_mutex(mutex);
          tape_alert = next_tape_alert;
        }
      }
    }
  }

  if (job_statistics) {
    bool found;
    JobControlRecord* jcr;
    job_statistics_t *job_stats, *next_job_stats;

    job_stats = (job_statistics_t*)job_statistics->first();
    while (job_stats) {
      if (job_stats->job_statistics) {
        struct jobstatistic_item *job_stat, *next_job_stat;

        job_stat = job_stats->job_statistics->first();
        while (job_stat) {
          next_job_stat = job_stats->job_statistics->next(job_stat);

          // If the entry was already collected no need to do it again.
          if (!job_stat->collected) {
            PmStrcpy(dev_tmp, job_stat->DevName);
            BashSpaces(dev_tmp);
            Mmsg(msg, JobStats, job_stat->timestamp, job_stats->JobId,
                 job_stat->JobFiles, job_stat->JobBytes, dev_tmp.c_str());
            Dmsg1(100, ">dird: %s", msg.c_str());
            dir->fsend(msg.c_str());
          }

          lock_mutex(mutex);
          // If this is the last one on the list leave it for comparison.
          if (!next_job_stat) {
            job_stat->collected = true;
          } else {
            job_stats->job_statistics->remove(job_stat);
            if (job_stats->cached == job_stat) { job_stats->cached = NULL; }
          }
          unlock_mutex(mutex);
          job_stat = next_job_stat;
        }
      }

      // If the Job doesn't exist anymore remove it from the job_statistics.
      next_job_stats = job_statistics->next(job_stats);

      found = false;
      foreach_jcr (jcr) {
        if (jcr->JobId == job_stats->JobId) {
          found = true;
          break;
        }
      }
      endeach_jcr(jcr);

      if (!found) {
        lock_mutex(mutex);
        Dmsg1(200, "Removing jobid %d from job_statistics\n", job_stats->JobId);
        job_statistics->remove(job_stats);
        unlock_mutex(mutex);
      }

      job_stats = next_job_stats;
    }
  }
  dir->fsend(OKstats);

  return false;
}

} /* namespace storagedaemon */
