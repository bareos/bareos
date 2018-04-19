/*
   BAREOS® - Backup Archiving REcovery Open Sourced

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
/**
 * @file
 * Storage Daemon statistics gatherer.
 *
 * Written by Marco van Wieringen and Philipp Storz, November 2013
 */

#include "include/bareos.h"
#include "stored.h"
#include "lib/util.h"
#include "include/jcr.h"

static char OKstats[] =
   "2000 OK statistics\n";
static char DevStats[] =
   "Devicestats [%lld]: Device=%s Read=%llu, Write=%llu, SpoolSize=%llu, NumWaiting=%ld, NumWriters=%ld, "
   "ReadTime=%lld, WriteTime=%lld, MediaId=%ld, VolBytes=%llu, VolFiles=%llu, VolBlocks=%llu\n";
static char TapeAlerts[] =
   "Tapealerts [%lld]: Device=%s TapeAlert=%llu\n";
static char JobStats[] =
   "Jobstats [%lld]: JobId=%ld, JobFiles=%lu, JobBytes=%llu, DevName=%s\n";

/* Static globals */
static bool quit = false;
static bool statistics_initialized = false;
static pthread_t statistics_tid;
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t wait_for_next_run = PTHREAD_COND_INITIALIZER;

struct device_statistic {
   dlink link;
   bool collected;
   utime_t timestamp;
   btime_t DevReadTime;
   btime_t DevWriteTime;
   uint64_t DevWriteBytes;
   uint64_t DevReadBytes;
   uint64_t spool_size;
   int num_waiting;
   int num_writers;
   DBId_t MediaId;
   uint64_t VolCatBytes;
   uint64_t VolCatFiles;
   uint64_t VolCatBlocks;
};

struct device_tapealert {
   dlink link;
   utime_t timestamp;
   uint64_t flags;
};

struct device_statistics {
   dlink link;
   char DevName[MAX_NAME_LENGTH];
   struct device_statistic *cached;
   dlist *statistics;
   dlist *tapealerts;
};

struct job_statistic {
   dlink link;
   bool collected;
   utime_t timestamp;
   uint32_t JobFiles;
   uint64_t JobBytes;
   char *DevName;
};

struct job_statistics {
   dlink link;
   uint32_t JobId;
   struct job_statistic *cached;
   dlist *statistics;
};

static dlist *device_statistics = NULL;
static dlist *job_statistics = NULL;

static inline void setup_statistics()
{
   struct device_statistics *dev_stats = NULL;
   struct job_statistics *job_stats = NULL;

   device_statistics = New(dlist(dev_stats, &dev_stats->link));
   job_statistics = New(dlist(job_stats, &job_stats->link));
}

void update_device_tapealert(const char *devname, uint64_t flags, utime_t now)
{
   bool found = false;
   struct device_statistics *dev_stats = NULL;
   struct device_tapealert *tape_alert = NULL;

   if (!me || !me->collect_dev_stats || !device_statistics) {
      return;
   }

   foreach_dlist(dev_stats, device_statistics) {
      if (bstrcmp(dev_stats->DevName, devname)) {
         found = true;
         break;
      }
   }

   if (!found) {
      dev_stats = (struct device_statistics *)malloc(sizeof(struct device_statistics));
      memset(dev_stats, 0, sizeof(struct device_statistics));

      bstrncpy(dev_stats->DevName, devname, sizeof(dev_stats->DevName));
      P(mutex);
      device_statistics->append(dev_stats);
      V(mutex);
   }

   /*
    * Add a new tapealert message.
    */
   tape_alert = (struct device_tapealert *)malloc(sizeof(struct device_tapealert));
   memset(tape_alert, 0, sizeof(struct device_tapealert));

   tape_alert->timestamp = now;
   tape_alert->flags = flags;

   if (!dev_stats->tapealerts) {
      dev_stats->tapealerts = New(dlist(tape_alert, &tape_alert->link));
   }

   P(mutex);
   dev_stats->tapealerts->append(tape_alert);
   V(mutex);

   Dmsg3(200, "New stats [%lld]: Device %s TapeAlert %llu\n",
         tape_alert->timestamp, dev_stats->DevName, tape_alert->flags);
}

static inline void update_device_statistics(const char *devname, Device *dev, utime_t now)
{
   bool found = false;
   struct device_statistics *dev_stats = NULL;
   struct device_statistic *dev_stat = NULL;

   if (!me || !me->collect_dev_stats || !device_statistics) {
      return;
   }

   /*
    * See if we already have statistics for this device.
    */
   foreach_dlist(dev_stats, device_statistics) {
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

      if (dev_stat->DevReadBytes == dev->DevReadBytes &&
          dev_stat->DevWriteBytes == dev->DevWriteBytes &&
          dev_stat->spool_size == dev->spool_size) {
         return;
      }
   } else if (!found) {
      dev_stats = (struct device_statistics *)malloc(sizeof(struct device_statistics));
      memset(dev_stats, 0, sizeof(struct device_statistics));

      bstrncpy(dev_stats->DevName, devname, sizeof(dev_stats->DevName));
      P(mutex);
      device_statistics->append(dev_stats);
      V(mutex);
   }

   /*
    * Add a new set of statistics.
    */
   dev_stat = (struct device_statistic *)malloc(sizeof(struct device_statistic));
   memset(dev_stat, 0, sizeof(struct device_statistic));

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


   if (!dev_stats->statistics) {
      dev_stats->statistics = New(dlist(dev_stat, &dev_stat->link));
   }

   P(mutex);
   dev_stats->cached = dev_stat;
   dev_stats->statistics->append(dev_stat);
   V(mutex);

   Dmsg5(200, "New stats [%lld]: Device %s Read %llu, Write %llu, Spoolsize %llu,\n",
         dev_stat->timestamp, dev_stats->DevName, dev_stat->DevReadBytes,
         dev_stat->DevWriteBytes, dev_stat->spool_size);
   Dmsg4(200, "NumWaiting %ld, NumWriters %ld, ReadTime=%lld, WriteTime=%lld,\n",
         dev_stat->num_waiting, dev_stat->num_writers, dev_stat->DevReadTime,
         dev_stat->DevWriteTime);
   Dmsg4(200, "MediaId=%ld VolBytes=%llu, VolFiles=%llu, VolBlocks=%llu\n",
         dev_stat->MediaId, dev_stat->VolCatBytes, dev_stat->VolCatFiles,
         dev_stat->VolCatBlocks);
}

void update_job_statistics(JobControlRecord *jcr, utime_t now)
{
   bool found = false;
   struct job_statistics *job_stats = NULL;
   struct job_statistic *job_stat = NULL;

   if (!me || !me->collect_job_stats || !job_statistics) {
      return;
   }

   /*
    * Skip job 0 info
    */
   if (!jcr->JobId) {
      return;
   }

   /*
    * See if we already have statistics for this job.
    */
   foreach_dlist(job_stats, job_statistics) {
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

      if (job_stat->JobFiles == jcr->JobFiles &&
          job_stat->JobBytes == jcr->JobBytes) {
         return;
      }
   } else if (!found) {
      job_stats = (struct job_statistics *)malloc(sizeof(struct job_statistics));
      memset(job_stats, 0, sizeof(struct job_statistics));

      job_stats->JobId = jcr->JobId;
      P(mutex);
      job_statistics->append(job_stats);
      V(mutex);
   }

   /*
    * Add a new set of statistics.
    */
   job_stat = (struct job_statistic *)malloc(sizeof(struct job_statistic));
   memset(job_stat, 0, sizeof(struct job_statistic));

   job_stat->timestamp = now;
   job_stat->JobFiles = jcr->JobFiles;
   job_stat->JobBytes = jcr->JobBytes;
   if (jcr->dcr) {
      job_stat->DevName = bstrdup(jcr->dcr->device->name());
   } else {
      job_stat->DevName = bstrdup("unknown");
   }

   if (!job_stats->statistics) {
      job_stats->statistics = New(dlist(job_stat, &job_stat->link));
   }

   P(mutex);
   job_stats->cached = job_stat;
   job_stats->statistics->append(job_stat);
   V(mutex);

   Dmsg5(200, "New stats [%lld]: JobId %ld, JobFiles %lu, JobBytes %llu, DevName %s\n",
         job_stat->timestamp, job_stats->JobId, job_stat->JobFiles,
         job_stat->JobBytes, job_stat->DevName);
}

static inline void cleanup_cached_statistics()
{
   struct device_statistics *dev_stats;
   struct job_statistics *job_stats;

   P(mutex);
   if (device_statistics) {
      foreach_dlist(dev_stats, device_statistics) {
         dev_stats->statistics->destroy();
         dev_stats->statistics = NULL;
      }

      device_statistics->destroy();
      device_statistics = NULL;
   }

   if (job_statistics) {
      foreach_dlist(job_stats, job_statistics) {
         job_stats->statistics->destroy();
         job_stats->statistics = NULL;
      }

      job_statistics->destroy();
      job_statistics = NULL;
   }
   V(mutex);
}

/**
 * Entry point for a separate statistics thread.
 */
extern "C"
void *statistics_thread_runner(void *arg)
{
   utime_t now;
   struct timeval tv;
   struct timezone tz;
   struct timespec timeout;
   DeviceResource *device;
   JobControlRecord *jcr;

   setup_statistics();

   /*
    * Do our work as long as we are not signaled to quit.
    */
   while (!quit) {
      now = (utime_t)time(NULL);

      if (me->collect_dev_stats) {
         /*
          * Loop over all defined devices.
          */
         foreach_res(device, R_DEVICE) {
            if (device->collectstats) {
               Device *dev;

               dev = device->dev;
               if (dev && dev->initiated) {
                  update_device_statistics(device->name(), dev, now);
               }
            }
         }
      }

      if (me->collect_job_stats) {
         /*
          * Loop over all running Jobs in the Storage Daemon.
          */
         foreach_jcr(jcr) {
            update_job_statistics(jcr, now);
         }
         endeach_jcr(jcr);
      }

      /*
       * Wait for a next run. Normally this waits exactly me->stats_collect_interval seconds.
       * It can be interrupted when signaled by the stop_statistics_thread() function.
       */
      gettimeofday(&tv, &tz);
      timeout.tv_nsec = tv.tv_usec * 1000;
      timeout.tv_sec = tv.tv_sec + me->stats_collect_interval;

      P(mutex);
      pthread_cond_timedwait(&wait_for_next_run, &mutex, &timeout);
      V(mutex);
   }

   /*
    * Cleanup the cached statistics.
    */
   cleanup_cached_statistics();

   return NULL;
}

int start_statistics_thread(void)
{
   int status;

   /*
    * First see if device and job stats collection is enabled.
    */
   if (!me->stats_collect_interval || (!me->collect_dev_stats && !me->collect_job_stats)) {
      return 0;
   }

   /*
    * See if only device stats collection is enabled that there is a least
    * one device of which stats are collected.
    */
   if (me->collect_dev_stats && !me->collect_job_stats) {
      DeviceResource *device;
      int cnt = 0;

      foreach_res(device, R_DEVICE) {
         if (device->collectstats) {
            cnt++;
         }
      }

      if (cnt == 0) {
         return 0;
      }
   }

   if ((status = pthread_create(&statistics_tid, NULL, statistics_thread_runner, NULL)) != 0) {
      return status;
   }

   statistics_initialized = true;

   return 0;
}

void stop_statistics_thread()
{
   if (!statistics_initialized) {
      return;
   }

   quit = true;
   pthread_cond_broadcast(&wait_for_next_run);
   if (!pthread_equal(statistics_tid, pthread_self())) {
      pthread_join(statistics_tid, NULL);
   }
}

bool stats_cmd(JobControlRecord *jcr)
{
   BareosSocket *dir = jcr->dir_bsock;
   PoolMem msg(PM_MESSAGE);
   PoolMem dev_tmp(PM_MESSAGE);

   if (device_statistics) {
      struct device_statistics *dev_stats;

      foreach_dlist(dev_stats, device_statistics) {
         if (dev_stats->statistics) {
            struct device_statistic *dev_stat, *next_dev_stat;

            dev_stat = (struct device_statistic *)dev_stats->statistics->first();
            while (dev_stat) {
               next_dev_stat = (struct device_statistic *)dev_stats->statistics->next(dev_stat);

               /*
                * If the entry was already collected no need to do it again.
                */
               if (!dev_stat->collected) {
                  pm_strcpy(dev_tmp, dev_stats->DevName);
                  bash_spaces(dev_tmp);
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

               P(mutex);
               /*
                * If this is the last one on the list leave it for comparison.
                */
               if (!next_dev_stat) {
                  dev_stat->collected = true;
               } else {
                  dev_stats->statistics->remove(dev_stat);

                  if (dev_stats->cached == dev_stat) {
                     dev_stats->cached = NULL;
                  }
               }
               V(mutex);
               dev_stat = next_dev_stat;
            }
         }

         if (dev_stats->tapealerts) {
            struct device_tapealert *tape_alert, *next_tape_alert;

            tape_alert = (struct device_tapealert *)dev_stats->tapealerts->first();
            while (tape_alert) {
               pm_strcpy(dev_tmp, dev_stats->DevName);
               bash_spaces(dev_tmp);
               Mmsg(msg, TapeAlerts, tape_alert->timestamp, dev_tmp.c_str(), tape_alert->flags);
               Dmsg1(100, ">dird: %s", msg.c_str());
               dir->fsend(msg.c_str());

               next_tape_alert = (struct device_tapealert *)dev_stats->tapealerts->next(tape_alert);
               P(mutex);
               dev_stats->tapealerts->remove(tape_alert);
               V(mutex);
               tape_alert = next_tape_alert;
            }
         }
      }
   }

   if (job_statistics) {
      bool found;
      JobControlRecord *jcr;
      struct job_statistics *job_stats, *next_job_stats;

      job_stats = (struct job_statistics *)job_statistics->first();
      while (job_stats) {
         if (job_stats->statistics) {
            struct job_statistic *job_stat, *next_job_stat;

            job_stat = (struct job_statistic *)job_stats->statistics->first();
            while (job_stat) {
               next_job_stat = (struct job_statistic *)job_stats->statistics->next(job_stat);

               /*
                * If the entry was already collected no need to do it again.
                */
               if (!job_stat->collected) {
                  pm_strcpy(dev_tmp, job_stat->DevName);
                  bash_spaces(dev_tmp);
                  Mmsg(msg, JobStats, job_stat->timestamp, job_stats->JobId,
                       job_stat->JobFiles, job_stat->JobBytes, dev_tmp.c_str());
                  Dmsg1(100, ">dird: %s", msg.c_str());
                  dir->fsend(msg.c_str());
               }

               P(mutex);
               /*
                * If this is the last one on the list leave it for comparison.
                */
               if (!next_job_stat) {
                  job_stat->collected = true;
               } else {
                  job_stats->statistics->remove(job_stat);
                  if (job_stats->cached == job_stat) {
                     job_stats->cached = NULL;
                  }
               }
               V(mutex);
               job_stat = next_job_stat;
            }
         }

         /*
          * If the Job doesn't exist anymore remove it from the job_statistics.
          */
         next_job_stats = (struct job_statistics *)job_statistics->next(job_stats);

         found = false;
         foreach_jcr(jcr) {
            if (jcr->JobId == job_stats->JobId) {
               found = true;
               break;
            }
         }
         endeach_jcr(jcr);

         if (!found) {
            P(mutex);
            Dmsg1(200, "Removing jobid %d from job_statistics\n", job_stats->JobId);
            job_statistics->remove(job_stats);
            V(mutex);
         }

         job_stats = next_job_stats;
      }
   }
   dir->fsend(OKstats);

   return false;
}
